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
#include <atomic>
#include <chrono>
#include <future>
#include <string>
#include <thread>
#include <ut/ut.hpp>

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

      const std::unordered_map<std::string, std::string> headers{
         {"X-Evil", "a\r\nSmuggled-Header: 1"},
         {"X-Safe", "kept"},
      };

      const std::string request = glz::detail::build_http_request_bytes("GET", url, false, "", headers);
      expect(request.find("Smuggled-Header") == std::string::npos) << "CRLF-bearing header must be dropped";
      expect(request.find("X-Safe: kept") != std::string::npos) << "Benign header must still be written";
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
      // response::header() lowercases the field-name (RFC 7230 case-insensitive).
      expect(response.find("x-echo: harmless-value") != std::string::npos) << "Benign header should round-trip";
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

   server.stop();
   io_ctx->stop();
   if (server_thr.joinable()) server_thr.join();
};

int main() { return 0; }
