// Verifies that glz does not emit a header field whose name or value contains
// CR or LF. A handler that reflects request-derived data into a response header
// value used to write it verbatim, so a value carrying CR/LF terminated the
// field on the wire and injected attacker-controlled headers (CWE-113, HTTP
// response splitting). The field is now dropped.
//
// Coverage spans all three guarded serializers plus the set-time guard:
//   - send_response_with_conn (the keep-alive response writer) via GET /echo,
//   - streaming_connection::send_headers via the streaming route /stream,
//   - detail::build_http_request_bytes (the client request writer) as a unit,
// and the GET /cl case proves the set-time guard in response::header() keeps a
// dropped Content-Length from suppressing the auto-generated one (which would
// otherwise leave the response unframed).
//
// The Server-Sent Events cases cover the same break one layer down. An event stream
// is framed by line breaks in the body, so streaming_connection::send_event has to
// keep a CR or LF in its data, event type or id from ending a field or an event.
#include <atomic>
#include <charconv>
#include <chrono>
#include <future>
#include <string>
#include <string_view>
#include <thread>
#include <ut/ut.hpp>
#include <vector>

#include "glaze/net/http.hpp"
#include "glaze/net/http_client.hpp"
#include "glaze/net/http_server.hpp"

#if defined(GLZ_USING_BOOST_ASIO)
namespace asio
{
   using namespace boost::asio;
   using error_code = boost::system::error_code;
}
#endif

namespace
{
   using namespace ut;

   constexpr char test_host[] = "127.0.0.1";

   std::string read_response(asio::ip::tcp::socket& socket)
   {
      std::string resp;
      std::array<char, 4096> buf{};
      asio::error_code ec;
      for (;;) {
         std::size_t n = socket.read_some(asio::buffer(buf), ec);
         if (n > 0) resp.append(buf.data(), n);
         if (n == 0 || ec) break;
      }
      return resp;
   }

   std::string send_raw(uint16_t port, const std::string& request)
   {
      asio::io_context io_ctx;
      asio::ip::tcp::socket socket(io_ctx);
      asio::ip::tcp::endpoint endpoint(asio::ip::make_address(test_host), port);
      asio::error_code ec;
      for (int tries = 0; tries < 50; ++tries) {
         socket.connect(endpoint, ec);
         if (!ec) break;
         socket.close(ec);
         std::this_thread::sleep_for(std::chrono::milliseconds(20));
      }
      if (ec) {
         return "";
      }
      asio::write(socket, asio::buffer(request.data(), request.size()), ec);
      return read_response(socket);
   }

   std::string send_raw_timed(uint16_t port, const std::string& request)
   {
      std::future<std::string> f = std::async(std::launch::async, [&] { return send_raw(port, request); });
      if (f.wait_for(std::chrono::seconds(5)) == std::future_status::ready) {
         return f.get();
      }
      return "TIMEOUT";
   }

   // Counts whole field lines rather than substrings: a plain find("Host:") would
   // also count an "X-Forwarded-Host:" that a later test happens to add.
   size_t count_header_fields(std::string_view message, std::string_view name)
   {
      const std::string field_start = "\r\n" + std::string{name} + ": ";
      size_t count = 0;
      for (size_t at = message.find(field_start); at != std::string_view::npos;
           at = message.find(field_start, at + 1)) {
         ++count;
      }
      return count;
   }

   glz::url_parts test_url()
   {
      glz::url_parts url;
      url.protocol = "http";
      url.host = "example.com";
      url.port = 80;
      url.path = "/";
      return url;
   }

   // The content of a chunked response, with the transfer coding removed.
   std::string chunked_content(std::string_view response)
   {
      std::string content;
      const size_t header_end = response.find("\r\n\r\n");
      if (header_end == std::string_view::npos) return content;
      response.remove_prefix(header_end + 4);

      for (;;) {
         const size_t size_end = response.find("\r\n");
         if (size_end == std::string_view::npos) break;
         size_t size = 0;
         if (std::from_chars(response.data(), response.data() + size_end, size, 16).ec != std::errc{}) break;
         if (size == 0 || response.size() < size_end + 2 + size + 2) break;
         content.append(response.substr(size_end + 2, size));
         response.remove_prefix(size_end + 2 + size + 2);
      }
      return content;
   }

   struct sse_event
   {
      std::string type;
      std::string data;
   };

   // What a recipient makes of an event stream.
   struct sse_stream
   {
      std::vector<sse_event> events;
      std::string last_event_id;
      std::string retry;
   };

   // Interprets an event stream the way an EventSource does (WHATWG HTML, "Interpreting
   // an event stream"), so the tests below assert on the events a recipient dispatches
   // rather than on wire text alone.
   sse_stream interpret_event_stream(std::string_view stream)
   {
      sse_stream result;
      std::string type;
      std::string data;
      bool has_data = false;

      while (!stream.empty()) {
         // CRLF, a lone CR and a lone LF each end a line.
         const size_t line_end = stream.find_first_of("\r\n");
         const std::string_view line = stream.substr(0, line_end);
         if (line_end == std::string_view::npos) {
            stream = {};
         }
         else {
            const bool is_crlf =
               stream[line_end] == '\r' && line_end + 1 < stream.size() && stream[line_end + 1] == '\n';
            stream.remove_prefix(line_end + (is_crlf ? 2 : 1));
         }

         if (line.empty()) {
            // An empty line dispatches the event, unless it carried no data.
            if (has_data) {
               data.pop_back(); // the LF the last data field appended
               result.events.push_back({type.empty() ? std::string{"message"} : type, data});
            }
            type.clear();
            data.clear();
            has_data = false;
            continue;
         }
         if (line.front() == ':') continue; // comment

         std::string_view name = line;
         std::string_view value;
         if (const size_t colon = line.find(':'); colon != std::string_view::npos) {
            name = line.substr(0, colon);
            value = line.substr(colon + 1);
            if (value.starts_with(' ')) value.remove_prefix(1);
         }

         if (name == "event") {
            type = value;
         }
         else if (name == "data") {
            data.append(value);
            data.push_back('\n');
            has_data = true;
         }
         else if (name == "id") {
            result.last_event_id = value;
         }
         else if (name == "retry") {
            result.retry = value;
         }
      }
      return result;
   }

} // namespace

static void error_handler(std::error_code, std::source_location) {}

suite header_field_crlf_helper = [] {
   "header_field_has_crlf flags CR or LF in name or value"_test = [] {
      expect(not glz::header_field_has_crlf("X-Echo", "plain value"));
      expect(not glz::header_field_has_crlf("", "")); // empty name and value are well-formed
      expect(glz::header_field_has_crlf("X-Echo", "ok\r\nSet-Cookie: sid=evil")); // CRLF in value
      expect(glz::header_field_has_crlf("X-Echo", "trailing\n")); // lone LF in value
      expect(glz::header_field_has_crlf("X-Echo", "lone\rcarriage")); // lone CR in value
      expect(glz::header_field_has_crlf("bad\rname", "value")); // lone CR in name
      expect(glz::header_field_has_crlf("bad\nname", "value")); // lone LF in name
   };

   // valid_header_name / valid_header_value are the shared RFC 7230 predicates the
   // WebSocket handshake also validates against (websocket_client delegates to them).
   "valid_header_name enforces non-empty tchar names"_test = [] {
      expect(glz::valid_header_name("X-Echo"));
      expect(glz::valid_header_name("Content-Type"));
      expect(not glz::valid_header_name("")); // empty
      expect(not glz::valid_header_name("X Echo")); // space is not a tchar
      expect(not glz::valid_header_name("X:Echo")); // colon is not a tchar
      expect(not glz::valid_header_name("bad\r\nname")); // CR/LF
   };

   "valid_header_value rejects control chars and DEL but allows VCHAR/SP/HTAB"_test = [] {
      expect(glz::valid_header_value("plain value, with punctuation: ok"));
      expect(glz::valid_header_value("tab\tseparated")); // HTAB is allowed
      expect(not glz::valid_header_value("ok\r\nInjected: 1")); // CRLF
      expect(not glz::valid_header_value(std::string_view("nul\0byte", 8))); // NUL
      expect(not glz::valid_header_value("del\x7f")); // DEL
   };
};

suite client_request_serializer_crlf = [] {
   // The client request writer is one of the three guarded serializers but is not
   // reachable through the server harness, so exercise it directly. A header whose
   // value carries CRLF must be dropped from the request bytes, never written
   // verbatim where it would smuggle a second header onto the wire.
   "build_http_request_bytes drops a CRLF-bearing header"_test = [] {
      glz::url_parts url;
      url.protocol = "http";
      url.host = "example.com";
      url.port = 80;
      url.path = "/";

      const glz::http_headers headers{
         {"X-Evil", "a\r\nSmuggled-Header: 1"},
         {"X-Safe", "kept"},
      };

      const std::string request = glz::detail::build_http_request_bytes("GET", url, false, "", headers);
      expect(request.find("Smuggled-Header") == std::string::npos) << "CRLF-bearing header must be dropped";
      expect(request.find("X-Safe: kept") != std::string::npos) << "Benign header must still be written";
   };

   // The request writer computes Content-Length from the body it is about to append,
   // so a caller-supplied one can only contradict it. glz::http_headers keeps
   // repeats, which makes a conflicting pair expressible in a single call - two
   // lengths on the wire let a proxy and the origin split the stream at different
   // offsets (RFC 9112 6.3, request smuggling).
   "build_http_request_bytes drops caller-supplied body framing headers"_test = [] {
      glz::url_parts url;
      url.protocol = "http";
      url.host = "example.com";
      url.port = 80;
      url.path = "/";

      const glz::http_headers headers{
         {"Content-Length", "0"},
         {"Content-Length", "999"},
         {"Transfer-Encoding", "chunked"},
         {"X-Safe", "kept"},
      };

      const std::string request = glz::detail::build_http_request_bytes("POST", url, false, "BODY", headers);

      expect(request.find("Transfer-Encoding") == std::string::npos)
         << "A caller Transfer-Encoding must not frame a body the writer does not chunk";
      expect(request.find("Content-Length: 999") == std::string::npos) << "A contradicting length must be dropped";
      expect(request.find("Content-Length: 0\r\n") == std::string::npos) << "A contradicting length must be dropped";
      expect(request.find("Content-Length: 4\r\n") != std::string::npos)
         << "The writer's own length, matching the body it appends, must survive";
      expect(request.find("X-Safe: kept") != std::string::npos) << "Unrelated headers must still be written";

      // Exactly one Content-Length reaches the wire.
      size_t count = 0;
      for (size_t at = request.find("Content-Length"); at != std::string::npos;
           at = request.find("Content-Length", at + 1)) {
         ++count;
      }
      expect(count == 1) << "Exactly one Content-Length must be written, found " << count;

      expect(request.ends_with("\r\n\r\nBODY")) << "The body must follow the header block unchanged";
   };

   // Dropping the caller's Content-Length has to be lossless. A method that
   // anticipates content carries a Content-Length even when the body is empty
   // (RFC 9110 8.6), or a caller who used to send "Content-Length: 0" by hand for
   // an empty POST would be left with a request many origin servers answer 411.
   "build_http_request_bytes frames an empty body for methods that anticipate content"_test = [] {
      glz::url_parts url;
      url.protocol = "http";
      url.host = "example.com";
      url.port = 80;
      url.path = "/";

      for (const auto* method : {"POST", "PUT", "PATCH"}) {
         const std::string request = glz::detail::build_http_request_bytes(method, url, false, "", {});
         expect(request.find("Content-Length: 0\r\n") != std::string::npos)
            << method << " with an empty body must still frame it";
      }

      // A caller-supplied length is still dropped in favor of the writer's own.
      const glz::http_headers headers{{"Content-Length", "999"}};
      const std::string overridden = glz::detail::build_http_request_bytes("POST", url, false, "", headers);
      expect(overridden.find("Content-Length: 0\r\n") != std::string::npos);
      expect(overridden.find("999") == std::string::npos) << "The caller's contradicting length must not survive";
   };

   // A method that anticipates no content frames an empty body by carrying no field
   // at all; inventing one would invite a body where none belongs.
   "build_http_request_bytes omits Content-Length for a bodyless GET"_test = [] {
      glz::url_parts url;
      url.protocol = "http";
      url.host = "example.com";
      url.port = 80;
      url.path = "/";

      for (const auto* method : {"GET", "HEAD", "DELETE", "OPTIONS"}) {
         const std::string request = glz::detail::build_http_request_bytes(method, url, false, "", {});
         expect(request.find("Content-Length") == std::string::npos)
            << method << " with no body must not carry a Content-Length";
      }
   };

   // The writer owns one Host and one Connection field. A caller who supplies
   // either replaces the writer's default rather than adding a second field to
   // the wire.
   "build_http_request_bytes prefers caller Host and Connection"_test = [] {
      const glz::url_parts url = test_url();

      const glz::http_headers headers{
         {"Host", "custom.example"},
         {"Connection", "close"},
         {"X-Safe", "kept"},
      };

      const std::string request = glz::detail::build_http_request_bytes("GET", url, false, "", headers);

      expect(count_header_fields(request, "Host") == 1) << "Exactly one Host must reach the wire";
      expect(count_header_fields(request, "Connection") == 1) << "Exactly one Connection must reach the wire";
      expect(request.find("Host: custom.example\r\n") != std::string::npos) << "The caller's Host must be written";
      expect(request.find("example.com") == std::string::npos) << "The derived Host must not also be written";
      expect(request.find("Connection: close\r\n") != std::string::npos) << "The caller's Connection must be written";
      expect(request.find("keep-alive") == std::string::npos) << "The default Connection must not also be written";
      expect(request.find("X-Safe: kept") != std::string::npos) << "Unrelated headers must still be written";
   };

   // Field names are case-insensitive (RFC 9110 5.1), so a lowercase caller field
   // has to suppress the default just as an exactly-cased one does.
   "build_http_request_bytes matches caller Host and Connection case-insensitively"_test = [] {
      const glz::url_parts url = test_url();

      const glz::http_headers headers{
         {"host", "custom.example"},
         {"CONNECTION", "close"},
      };

      const std::string request = glz::detail::build_http_request_bytes("GET", url, false, "", headers);

      expect(count_header_fields(request, "Host") == 1);
      expect(count_header_fields(request, "Connection") == 1);
      expect(request.find("example.com") == std::string::npos) << "The derived Host must be suppressed";
      expect(request.find("keep-alive") == std::string::npos) << "The default Connection must be suppressed";
   };

   // RFC 9112 3.2 admits exactly one Host. glz::http_headers keeps repeats, so a
   // caller can express two - and two authorities on one request let an
   // intermediary and the origin resolve it differently (request smuggling).
   // glaze's own server answers a repeated Host with 400, so the writer must not
   // be able to generate one.
   "build_http_request_bytes writes one Host when the caller repeats it"_test = [] {
      const glz::url_parts url = test_url();

      const glz::http_headers headers{
         {"Host", "first.example"},
         {"Host", "second.example"},
      };

      const std::string request = glz::detail::build_http_request_bytes("GET", url, false, "", headers);

      expect(count_header_fields(request, "Host") == 1) << "A repeated caller Host must not reach the wire twice";
      expect(request.find("Host: first.example\r\n") != std::string::npos) << "The first caller Host is the one kept";
      expect(request.find("second.example") == std::string::npos) << "The repeat must be dropped";
   };

   // A "close" and a "keep-alive" on one message leave each hop free to pick a
   // different connection lifetime, so Connection is reduced to a single field
   // the same way Host is.
   "build_http_request_bytes writes one Connection when the caller repeats it"_test = [] {
      const glz::url_parts url = test_url();

      const glz::http_headers headers{
         {"Connection", "close"},
         {"Connection", "keep-alive"},
      };

      const std::string request = glz::detail::build_http_request_bytes("GET", url, false, "", headers);

      expect(count_header_fields(request, "Connection") == 1)
         << "A repeated caller Connection must not reach the wire twice";
      expect(request.find("Connection: close\r\n") != std::string::npos)
         << "The first caller Connection is the one kept";
      expect(request.find("keep-alive") == std::string::npos) << "The contradicting repeat must be dropped";
   };

   // The CRLF guard drops a caller field carrying CR or LF, so such a field must
   // not count as having supplied a Host either: suppressing the derived one
   // would put an HTTP/1.1 request with no Host at all on the wire (RFC 9112 3.2),
   // which glaze's own server answers with 400.
   "build_http_request_bytes keeps its Host when the caller's carries CRLF"_test = [] {
      const glz::url_parts url = test_url();

      const glz::http_headers headers{
         {"Host", "evil.example\r\nX-Injected: 1"},
         {"Connection", "close\r\nX-Injected: 2"},
      };

      const std::string request = glz::detail::build_http_request_bytes("GET", url, false, "", headers);

      expect(request.find("X-Injected") == std::string::npos) << "A CRLF-bearing field must be dropped";
      expect(count_header_fields(request, "Host") == 1) << "The request must still carry exactly one Host";
      expect(request.find("Host: example.com\r\n") != std::string::npos) << "The derived Host must survive";
      expect(count_header_fields(request, "Connection") == 1) << "The request must still carry one Connection";
      expect(request.find("Connection: keep-alive\r\n") != std::string::npos) << "The default Connection must survive";
   };

   // RFC 9112 5: a user agent SHOULD generate Host as the first field after the
   // request-line. That holds whether the value is derived or caller-supplied,
   // so a caller's Host is written in the Host slot rather than wherever it sat
   // among their headers.
   "build_http_request_bytes writes Host first"_test = [] {
      const glz::url_parts url = test_url();

      const glz::http_headers headers{
         {"X-First", "1"},
         {"Host", "custom.example"},
      };

      const std::string derived = glz::detail::build_http_request_bytes("GET", url, false, "", {});
      expect(derived.starts_with("GET / HTTP/1.1\r\nHost: example.com\r\n")) << "A derived Host leads the field block";

      const std::string overridden = glz::detail::build_http_request_bytes("POST", url, false, "B", headers);
      expect(overridden.starts_with("POST / HTTP/1.1\r\nHost: custom.example\r\n"))
         << "A caller Host leads the field block too";
      expect(overridden.find("X-First: 1") != std::string::npos) << "The caller's other headers must still be written";
   };

   // The derived Host still carries the port whenever it is not the scheme
   // default, and only then.
   "build_http_request_bytes derives Host from the url when the caller supplies none"_test = [] {
      glz::url_parts url = test_url();
      url.port = 8080;

      const std::string request = glz::detail::build_http_request_bytes("GET", url, false, "", {});
      expect(request.find("Host: example.com:8080\r\n") != std::string::npos)
         << "A non-default port belongs in the Host";

      url.port = 80;
      const std::string default_port = glz::detail::build_http_request_bytes("GET", url, false, "", {});
      expect(default_port.find("Host: example.com\r\n") != std::string::npos) << "The scheme's default port is omitted";

      url.port = 443;
      const std::string https = glz::detail::build_http_request_bytes("GET", url, true, "", {});
      expect(https.find("Host: example.com\r\n") != std::string::npos) << "443 is the default port for https";
   };
};

suite http_response_splitting_suite = [] {
   auto io_ctx = std::make_shared<asio::io_context>();
   glz::http_server<false> server(io_ctx, error_handler);

   // Reflects a url-decoded query parameter straight into a response header, the
   // shape that turns a header value into an injection sink: %0d%0a in the query
   // decodes to a real CRLF before it reaches res.header().
   server.get("/echo", [&](const glz::request& req, glz::response& res) {
      auto it = req.query.find("v");
      res.header("X-Echo", it != req.query.end() ? it->second : "");
      res.status(200);
      res.body("ok");
   });

   // Reflects a url-decoded query parameter into the Content-Length header. This
   // is the case where dropping the malformed field is not enough on its own:
   // response::header() records that the user set Content-Length, which suppresses
   // the auto-generated one, so a guard that only dropped at serialization would
   // leave the response with no Content-Length at all. The set-time guard keeps
   // the framing header intact.
   server.get("/cl", [&](const glz::request& req, glz::response& res) {
      auto it = req.query.find("v");
      res.header("Content-Length", it != req.query.end() ? it->second : "");
      res.status(200);
      res.body("hello world"); // 11 bytes
   });

   // Streaming responses go through a different serializer (send_headers) than the
   // keep-alive writer. Reflects a CRLF value into one header and keeps a benign
   // one alongside it as a positive control.
   server.stream_get("/stream", [&](glz::request&, glz::streaming_response& res) {
      res.start_stream(200, {{"X-Echo", "ok\r\nInjected-Header: pwned"}, {"X-Safe", "kept"}});
      res.send("hello");
      res.close();
   });

   // A CRLF-bearing transfer-encoding is dropped, but that drop must not suppress
   // the auto-generated chunked framing: otherwise the stream would be emitted
   // with no Transfer-Encoding and no framing at all.
   server.stream_get("/stream_te", [&](glz::request&, glz::streaming_response& res) {
      res.start_stream(200, {{"transfer-encoding", "chunked\r\nInjected-Header: pwned"}});
      res.send("hello");
      res.close();
   });

   // Server-Sent Events are framed by line breaks in the body rather than in the header
   // block. Relays url-decoded query parameters into each send_event argument, the shape
   // of a handler that pushes text one user wrote to the others subscribed to a topic.
   // The trailing "done" event shows the stream is still intact after the relayed one.
   server.stream_get("/events", [&](glz::request& req, glz::streaming_response& res) {
      const auto param = [&](const char* key) {
         auto it = req.query.find(key);
         return it != req.query.end() ? it->second : std::string{};
      };
      res.as_event_stream();
      res.send_event(param("type"), param("msg"), param("id"));
      res.send_event("done", "end");
      res.close();
   });

   // The connection-level send_event takes a callback, which is how a refused event is
   // reported. The stream is closed from the callback, so the response ends only once it
   // has run.
   std::atomic<int> refused_event_error{0};
   server.stream_get("/events-refused", [&](glz::request&, glz::streaming_response& res) {
      res.as_event_stream();
      res.stream->send_event("chat\nevent: admin", "x", {}, [&, stream = res.stream](std::error_code ec) {
         refused_event_error = ec.value();
         stream->close();
      });
   });

   server.bind(test_host, 0);
   const uint16_t port = server.port();
   server.start(0);
   std::thread server_thr([&] { io_ctx->run(); });

   "a CRLF-bearing reflected header value cannot inject a header"_test = [&] {
      // %0d%0a in the query decodes to CRLF, so the reflected value carries a
      // second header. Before the fix the server wrote it verbatim and
      // "Injected-Header: pwned" appeared as a real response header.
      const std::string payload =
         "GET /echo?v=ok%0d%0aInjected-Header:%20pwned HTTP/1.1\r\n"
         "Host: localhost\r\n"
         "Connection: close\r\n"
         "\r\n";

      const std::string response = send_raw_timed(port, payload);
      expect(response.find("200") != std::string::npos) << "Request should still be served";
      expect(response.find("Injected-Header") == std::string::npos) << "Reflected CRLF must not inject a header";
   };

   "well-formed reflected header value still round-trips"_test = [&] {
      // Positive control: a benign reflected value must still be emitted.
      const std::string payload =
         "GET /echo?v=harmless-value HTTP/1.1\r\n"
         "Host: localhost\r\n"
         "Connection: close\r\n"
         "\r\n";

      const std::string response = send_raw_timed(port, payload);
      expect(response.find("200") != std::string::npos) << "Request should be served";
      expect(response.find("X-Echo: harmless-value") != std::string::npos) << "Benign header should round-trip";
   };

   "dropping a reflected Content-Length still frames the response"_test = [&] {
      // The reflected value decodes to "11\r\nX-Injected: pwned". The malformed
      // Content-Length is dropped, and because the set-time guard never recorded
      // it the server still emits its own Content-Length for the 11-byte body.
      const std::string payload =
         "GET /cl?v=11%0d%0aX-Injected:%20pwned HTTP/1.1\r\n"
         "Host: localhost\r\n"
         "Connection: close\r\n"
         "\r\n";

      const std::string response = send_raw_timed(port, payload);
      expect(response.find("200") != std::string::npos) << "Request should still be served";
      expect(response.find("X-Injected") == std::string::npos) << "Reflected CRLF must not inject a header";
      expect(response.find("Content-Length: 11") != std::string::npos)
         << "Auto Content-Length must survive the dropped override so the message stays framed";
   };

   "a CRLF-bearing streaming header cannot inject a header"_test = [&] {
      // Streaming uses send_headers, a separate serializer from the keep-alive
      // writer. The malicious X-Echo is dropped while the benign X-Safe remains.
      const std::string payload =
         "GET /stream HTTP/1.1\r\n"
         "Host: localhost\r\n"
         "Connection: close\r\n"
         "\r\n";

      const std::string response = send_raw_timed(port, payload);
      expect(response.find("200") != std::string::npos) << "Stream should be served";
      expect(response.find("Injected-Header") == std::string::npos) << "Reflected CRLF must not inject a header";
      expect(response.find("X-Safe: kept") != std::string::npos) << "Benign streaming header should round-trip";
   };

   "a dropped streaming transfer-encoding still frames the stream"_test = [&] {
      // The malicious transfer-encoding is dropped; the server must fall back to
      // its auto Transfer-Encoding: chunked rather than leaving the stream
      // unframed, and the injected header must not appear.
      const std::string payload =
         "GET /stream_te HTTP/1.1\r\n"
         "Host: localhost\r\n"
         "Connection: close\r\n"
         "\r\n";

      const std::string response = send_raw_timed(port, payload);
      expect(response.find("200") != std::string::npos) << "Stream should be served";
      expect(response.find("Injected-Header") == std::string::npos) << "Reflected CRLF must not inject a header";
      expect(response.find("Transfer-Encoding: chunked") != std::string::npos)
         << "Auto chunked framing must survive the dropped transfer-encoding override";
   };

   const auto get_events = [&](const std::string& query) {
      const std::string payload = "GET /events?" + query +
                                  " HTTP/1.1\r\n"
                                  "Host: localhost\r\n"
                                  "Connection: close\r\n"
                                  "\r\n";
      return chunked_content(send_raw_timed(port, payload));
   };

   "a single-line SSE event is written as before"_test = [&] {
      const std::string content = get_events("type=chat&msg=hello&id=7");
      expect(content == "id: 7\nevent: chat\ndata: hello\n\nevent: done\ndata: end\n\n") << content;
   };

   "multi-line SSE data reaches the recipient whole"_test = [&] {
      // Written as one data field, "line2" was read as a field of its own and dropped,
      // so the recipient saw only "line1".
      const std::string content = get_events("type=chat&msg=line1%0Aline2");
      expect(content == "event: chat\ndata: line1\ndata: line2\n\nevent: done\ndata: end\n\n") << content;

      const sse_stream stream = interpret_event_stream(content);
      expect(stream.events.size() == 2) << content;
      if (stream.events.size() == 2) {
         expect(stream.events[0].type == "chat");
         expect(stream.events[0].data == "line1\nline2") << stream.events[0].data;
         expect(stream.events[1].type == "done");
      }
   };

   "a line break in SSE data cannot start another event"_test = [&] {
      // An empty line dispatches the event, so the text after it used to arrive as a
      // second event whose type and data the sender of the message chose.
      const std::string content = get_events("type=chat&msg=hi%0A%0Aevent:%20admin%0Adata:%20forged");

      const sse_stream stream = interpret_event_stream(content);
      expect(stream.events.size() == 2) << content;
      if (stream.events.size() == 2) {
         expect(stream.events[0].type == "chat");
         expect(stream.events[0].data == "hi\n\nevent: admin\ndata: forged") << stream.events[0].data;
         expect(stream.events[1].type == "done");
      }
   };

   "CR and CRLF in SSE data are line breaks too"_test = [&] {
      // A recipient ends a line at a lone CR as well, so splitting on LF alone would
      // leave the same hole open. Either form arrives as LF.
      const std::string content = get_events("type=chat&msg=a%0D%0Ab%0D%0Devent:%20admin%0Ddata:%20forged%0D%0A");
      expect(content.find('\r') == std::string::npos) << "No CR may reach the event stream";

      const sse_stream stream = interpret_event_stream(content);
      expect(stream.events.size() == 2) << content;
      if (stream.events.size() == 2) {
         expect(stream.events[0].type == "chat");
         expect(stream.events[0].data == "a\nb\n\nevent: admin\ndata: forged\n") << stream.events[0].data;
         expect(stream.events[1].type == "done");
      }
   };

   "SSE data cannot set the last event id or the reconnection time"_test = [&] {
      const std::string content = get_events("type=chat&msg=hi%0Aid:%20999%0Aretry:%2086400000");

      const sse_stream stream = interpret_event_stream(content);
      expect(stream.last_event_id.empty()) << stream.last_event_id;
      expect(stream.retry.empty()) << stream.retry;
      expect(stream.events.size() == 2) << content;
      if (stream.events.size() == 2) {
         expect(stream.events[0].data == "hi\nid: 999\nretry: 86400000") << stream.events[0].data;
      }
   };

   "an SSE event type holding a line break is refused"_test = [&] {
      // The event field is a single line, so there is no way to carry the break. The
      // event is not written at all, and the one after it still arrives.
      for (const auto* type :
           {"chat%0Adata:%20forged%0A%0Aevent:%20admin", "chat%0Ddata:%20forged%0D%0Devent:%20admin"}) {
         const std::string content = get_events(std::string{"type="} + type + "&msg=real");
         expect(content == "event: done\ndata: end\n\n") << content;
      }
   };

   "an SSE id holding a line break is refused"_test = [&] {
      for (const auto* id : {"7%0Aevent:%20admin%0Adata:%20forged%0A", "7%0D%0Aretry:%2086400000"}) {
         const std::string content = get_events(std::string{"type=chat&msg=real&id="} + id);
         expect(content == "event: done\ndata: end\n\n") << content;
      }
   };

   "a refused SSE event reports invalid_argument to its callback"_test = [&] {
      const std::string payload =
         "GET /events-refused HTTP/1.1\r\n"
         "Host: localhost\r\n"
         "Connection: close\r\n"
         "\r\n";

      const std::string response = send_raw_timed(port, payload);
      expect(response.find("200") != std::string::npos) << "Stream should be served";
      expect(chunked_content(response).empty()) << "A refused event must not reach the event stream";
      expect(refused_event_error.load() == static_cast<int>(std::errc::invalid_argument));
   };

   server.stop();
   io_ctx->stop();
   if (server_thr.joinable()) server_thr.join();
};

int main() { return 0; }
