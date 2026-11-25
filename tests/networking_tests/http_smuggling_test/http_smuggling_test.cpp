#include "glaze/ext/asio_include.hpp"

import std;

import glaze.net.http_router;
import glaze.net.http_server;
import glaze.util.compare;

import ut;

using std::uint16_t;

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
         if (n == 0 || ec) break;
         resp.append(buf.data(), n);
      }
      return resp;
   }

   std::string send_raw(uint16_t port, const std::string& request)
   {
      asio::io_context io_ctx;
      asio::ip::tcp::socket socket(io_ctx);
      asio::ip::tcp::endpoint endpoint(asio::ip::make_address(test_host), port);
      asio::error_code ec;
      socket.connect(endpoint, ec);
      if (ec) {
         return "";
      }
      asio::write(socket, asio::buffer(request.data(), request.size()));
      return read_response(socket);
   }

} // namespace

static void error_handler(std::error_code, std::source_location) {}

suite http_smuggling_suite = [] {
   auto io_ctx = std::make_shared<asio::io_context>();
   glz::http_server<false> server(io_ctx, error_handler);

   std::atomic<int> smuggled_hits{0};

   server.post("/front", [&](const glz::request&, glz::response& res) {
      res.status(200);
      res.body("front");
   });
   server.get("/smuggled", [&](const glz::request&, glz::response& res) {
      smuggled_hits.fetch_add(1, std::memory_order_relaxed);
      res.status(200);
      res.body("smuggled");
   });
   // A handler that announces a transfer coding the buffered response writer never
   // applies. The body below is deliberately shaped like a valid chunked stream, so
   // a recipient that believed the Transfer-Encoding would decode "smuggled!" as a
   // chunk while a recipient that believed the Content-Length would not.
   server.get("/handler-sets-te", [&](const glz::request&, glz::response& res) {
      res.header("Transfer-Encoding", "chunked");
      res.status(200);
      res.body("9\r\nsmuggled!\r\n0\r\n\r\n");
   });
   server.bind(test_host, 0);
   const uint16_t test_port = server.port();
   server.start(0);

   std::thread server_thr([&] { io_ctx->run(); });

   "Transfer-Encoding request is refused, not desynced"_test = [&] {
      // The bytes after the blank line are a complete second request. With a
      // Transfer-Encoding header and no Content-Length the server used to treat
      // the body as empty and leave those bytes in the keep-alive buffer, where
      // they were parsed as a smuggled GET /smuggled.
      std::string payload =
         "POST /front HTTP/1.1\r\n"
         "Host: localhost\r\n"
         "Transfer-Encoding: chunked\r\n"
         "Connection: keep-alive\r\n"
         "\r\n"
         "GET /smuggled HTTP/1.1\r\n"
         "Host: localhost\r\n"
         "Connection: close\r\n"
         "\r\n";

      std::future<std::string> f = std::async(std::launch::async, [&] { return send_raw(test_port, payload); });

      auto future_timeout = std::chrono::system_clock::now() + std::chrono::seconds(5);
      std::string response;
      if (std::future_status::ready == f.wait_until(future_timeout)) {
         response = f.get();
      }

      expect(response.find("501") != std::string::npos) << "Expected 501 for unsupported Transfer-Encoding";
      // Give the io_context a moment to surface any (incorrectly) queued request.
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      expect(smuggled_hits.load(std::memory_order_relaxed) == 0) << "Smuggled request reached a route handler";
   };

   "Conflicting Content-Length headers are refused, not desynced"_test = [&] {
      // Two Content-Length values disagree (53 vs 7). The header map keeps the last value (7),
      // so the server used to frame only "BLOCKED" as the body and re-parse the trailing bytes
      // as a smuggled GET /smuggled. A proxy framing by the first value (53) would have passed
      // the whole region through as one body, so the split is a CL.CL desync.
      std::string payload =
         "POST /front HTTP/1.1\r\n"
         "Host: localhost\r\n"
         "Content-Length: 53\r\n"
         "Content-Length: 7\r\n"
         "Connection: keep-alive\r\n"
         "\r\n"
         "BLOCKED"
         "GET /smuggled HTTP/1.1\r\n"
         "Host: localhost\r\n"
         "Connection: close\r\n"
         "\r\n";

      std::future<std::string> f = std::async(std::launch::async, [&] { return send_raw(test_port, payload); });

      auto future_timeout = std::chrono::system_clock::now() + std::chrono::seconds(5);
      std::string response;
      if (std::future_status::ready == f.wait_until(future_timeout)) {
         response = f.get();
      }

      // Match the status line, not a bare "400" substring, so an unrelated rejection
      // (or a "400" appearing in a body) cannot make this pass for the wrong reason.
      expect(response.find("HTTP/1.1 400") != std::string::npos) << "Expected 400 for conflicting Content-Length";
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      expect(smuggled_hits.load(std::memory_order_relaxed) == 0) << "Smuggled request reached a route handler";
   };

   "Identical duplicate Content-Length headers are tolerated"_test = [&] {
      // RFC 7230 3.3.2 permits collapsing repeated Content-Length fields that carry the same
      // value, so a lenient-but-honest client that sends the header twice must still be served.
      // This pins the "identical repeats are tolerated" branch of the fix: a regression that
      // rejected all duplicate Content-Length headers would break such clients, and the only
      // thing catching it would be this test.
      std::string payload =
         "POST /front HTTP/1.1\r\n"
         "Host: localhost\r\n"
         "Content-Length: 5\r\n"
         "Content-Length: 5\r\n"
         "Connection: close\r\n"
         "\r\n"
         "HELLO";

      std::future<std::string> f = std::async(std::launch::async, [&] { return send_raw(test_port, payload); });

      auto future_timeout = std::chrono::system_clock::now() + std::chrono::seconds(5);
      std::string response;
      if (std::future_status::ready == f.wait_until(future_timeout)) {
         response = f.get();
      }

      expect(response.find("HTTP/1.1 200") != std::string::npos)
         << "Identical duplicate Content-Length should be accepted";
   };

   "Case-varied duplicate Content-Length headers are refused, not desynced"_test = [&] {
      // The duplicate check lowercases the field-name before the map lookup, so varying the
      // case ("Content-Length" vs "content-length") must not land two distinct keys that would
      // let a conflicting value slip past conflict detection. Same CL.CL desync as above, only
      // obfuscated by case.
      std::string payload =
         "POST /front HTTP/1.1\r\n"
         "Host: localhost\r\n"
         "Content-Length: 53\r\n"
         "content-length: 7\r\n"
         "Connection: keep-alive\r\n"
         "\r\n"
         "BLOCKED"
         "GET /smuggled HTTP/1.1\r\n"
         "Host: localhost\r\n"
         "Connection: close\r\n"
         "\r\n";

      std::future<std::string> f = std::async(std::launch::async, [&] { return send_raw(test_port, payload); });

      auto future_timeout = std::chrono::system_clock::now() + std::chrono::seconds(5);
      std::string response;
      if (std::future_status::ready == f.wait_until(future_timeout)) {
         response = f.get();
      }

      expect(response.find("HTTP/1.1 400") != std::string::npos)
         << "Expected 400 for case-varied conflicting Content-Length";
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      expect(smuggled_hits.load(std::memory_order_relaxed) == 0) << "Smuggled request reached a route handler";
   };

   // The buffered response writer always frames with Content-Length, so it must not
   // let a handler advertise a transfer coding it does not apply. Emitting both
   // fields is the response-side twin of the CL.TE request desync the cases above
   // cover: two recipients read two different body boundaries from one response.
   "Handler-set Transfer-Encoding is dropped, not emitted alongside Content-Length"_test = [&] {
      const std::string payload =
         "GET /handler-sets-te HTTP/1.1\r\n"
         "Host: localhost\r\n"
         "Connection: close\r\n"
         "\r\n";

      std::future<std::string> f = std::async(std::launch::async, [&] { return send_raw(test_port, payload); });

      auto future_timeout = std::chrono::system_clock::now() + std::chrono::seconds(5);
      std::string response;
      if (std::future_status::ready == f.wait_until(future_timeout)) {
         response = f.get();
      }

      // Field names are matched case-insensitively: response::header() lowercases
      // today but is not required to, and a case-sensitive search would silently
      // stop testing anything the day that changes.
      const auto count_fields = [](std::string_view haystack, std::string_view field) {
         size_t found = 0;
         for (size_t i = 0; i + field.size() <= haystack.size(); ++i) {
            if (glz::striequal(haystack.substr(i, field.size()), field)) {
               ++found;
            }
         }
         return found;
      };

      expect(response.find("HTTP/1.1 200") != std::string::npos) << "The response itself should still be served";
      expect(count_fields(response, "Transfer-Encoding") == 0)
         << "A transfer coding this writer never applies must not reach the wire, got: " << response;

      // Exactly one framing field, and it must describe the bytes actually sent.
      const size_t content_length_fields = count_fields(response, "Content-Length");
      expect(content_length_fields == 1) << "Expected exactly one Content-Length, found " << content_length_fields;
      expect(count_fields(response, "Content-Length: 19") == 1)
         << "Content-Length must match the unencoded body length, got: " << response;
   };

   server.stop();
   io_ctx->stop();
   if (server_thr.joinable()) server_thr.join();
};

int main() { return 0; }
