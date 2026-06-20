// Verifies RFC 7230 3.2.4 header-field strictness in glz::http_server: a header
// line that uses obsolete line folding (obs-fold) or places whitespace between
// the field-name and the colon is rejected with 400 and the connection is closed.
//
// Both forms are request-smuggling vectors: a folding-aware or whitespace-lenient
// front-end proxy reads an obfuscated "Transfer-Encoding" and frames the body as
// chunked, while a server that stores the header under a mangled key
// ("transfer-encoding " / "\ttransfer-encoding") misses the lookup, treats the
// body as empty, and reparses the leftover bytes as a second, smuggled request.
// (The plain "Transfer-Encoding: chunked" case is handled separately in
// finish_request; this test guards the obfuscated variants at the parser.)
#include <atomic>
#include <chrono>
#include <future>
#include <string>
#include <thread>
#include <ut/ut.hpp>

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

   // Connect (retrying briefly while the acceptor warms up) and send raw bytes,
   // returning the full response read until the server closes the connection.
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

   // Run send_raw under a hard timeout so a desync regression that hangs the
   // connection fails the test instead of stalling it.
   std::string send_raw_timed(uint16_t port, const std::string& request)
   {
      std::future<std::string> f = std::async(std::launch::async, [&] { return send_raw(port, request); });
      if (f.wait_for(std::chrono::seconds(5)) == std::future_status::ready) {
         return f.get();
      }
      return "TIMEOUT";
   }

   // The bytes after the blank line are a complete second request. A desync would
   // reparse them as a smuggled GET /smuggled.
   const std::string smuggled_tail =
      "GET /smuggled HTTP/1.1\r\n"
      "Host: localhost\r\n"
      "Connection: close\r\n"
      "\r\n";

} // namespace

static void error_handler(std::error_code, std::source_location) {}

suite http_header_strictness_suite = [] {
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
   server.get("/ok", [&](const glz::request&, glz::response& res) {
      res.status(200);
      res.body("ok");
   });

   // Bind to an ephemeral port and read it back, so the test never collides with
   // another listener or a prior run stuck in TIME_WAIT.
   server.bind(test_host, 0);
   const uint16_t port = server.port();
   server.start(0);
   std::thread server_thr([&] { io_ctx->run(); });

   "space before colon is rejected, not desynced"_test = [&] {
      const std::string payload =
         "POST /front HTTP/1.1\r\n"
         "Host: localhost\r\n"
         "Transfer-Encoding : chunked\r\n"
         "Connection: keep-alive\r\n"
         "\r\n" +
         smuggled_tail;

      const std::string response = send_raw_timed(port, payload);
      expect(response.find("400") != std::string::npos) << "Expected 400 for whitespace before colon";
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      expect(smuggled_hits.load(std::memory_order_relaxed) == 0) << "Smuggled request reached a route handler";
   };

   "tab before colon is rejected, not desynced"_test = [&] {
      const std::string payload =
         "POST /front HTTP/1.1\r\n"
         "Host: localhost\r\n"
         "Transfer-Encoding\t: chunked\r\n"
         "Connection: keep-alive\r\n"
         "\r\n" +
         smuggled_tail;

      const std::string response = send_raw_timed(port, payload);
      expect(response.find("400") != std::string::npos) << "Expected 400 for tab before colon";
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      expect(smuggled_hits.load(std::memory_order_relaxed) == 0) << "Smuggled request reached a route handler";
   };

   "obs-fold header continuation is rejected, not desynced"_test = [&] {
      // Transfer-Encoding rides a folded continuation of X-Foo (line begins with a tab).
      const std::string payload =
         "POST /front HTTP/1.1\r\n"
         "Host: localhost\r\n"
         "X-Foo: bar\r\n"
         "\tTransfer-Encoding: chunked\r\n"
         "Connection: keep-alive\r\n"
         "\r\n" +
         smuggled_tail;

      const std::string response = send_raw_timed(port, payload);
      expect(response.find("400") != std::string::npos) << "Expected 400 for obs-fold continuation";
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      expect(smuggled_hits.load(std::memory_order_relaxed) == 0) << "Smuggled request reached a route handler";
   };

   "well-formed headers still parse and route"_test = [&] {
      // Positive control: ordinary header names, and values that contain spaces
      // and colons, must continue to parse normally (no over-rejection).
      const std::string payload =
         "GET /ok HTTP/1.1\r\n"
         "Host: localhost:8080\r\n"
         "User-Agent: glaze-test/1.0 (X11; check)\r\n"
         "Accept: text/plain, application/json\r\n"
         "Connection: close\r\n"
         "\r\n";

      const std::string response = send_raw_timed(port, payload);
      expect(response.find("200") != std::string::npos) << "Well-formed request should return 200";
      expect(response.find("ok") != std::string::npos) << "Handler body should be present";
   };

   server.stop();
   io_ctx->stop();
   if (server_thr.joinable()) server_thr.join();
};

int main() { return 0; }
