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

   constexpr int test_port = 8899;
   constexpr char test_host[] = "127.0.0.1";

   void wait_for_server_ready(int port, int max_tries = 50)
   {
      for (int tries = 0; tries < max_tries; ++tries) {
         try {
            asio::io_context io_ctx;
            asio::ip::tcp::socket sock(io_ctx);
            asio::ip::tcp::endpoint endpoint(asio::ip::make_address(test_host), uint16_t(port));
            asio::error_code ec;
            sock.connect(endpoint, ec);
            if (ec != asio::error::connection_refused) {
               return;
            }
         }
         catch (...) {
         }
         std::this_thread::sleep_for(std::chrono::milliseconds(40));
      }
      throw std::runtime_error("Server did not start in time");
   }

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

   std::string send_raw(const std::string& request)
   {
      wait_for_server_ready(test_port);
      asio::io_context io_ctx;
      asio::ip::tcp::socket socket(io_ctx);
      asio::ip::tcp::endpoint endpoint(asio::ip::make_address(test_host), uint16_t(test_port));
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

   std::thread server_thr([&] {
      server.post("/front", [&](const glz::request&, glz::response& res) {
         res.status(200);
         res.body("front");
      });
      server.get("/smuggled", [&](const glz::request&, glz::response& res) {
         smuggled_hits.fetch_add(1, std::memory_order_relaxed);
         res.status(200);
         res.body("smuggled");
      });

      server.bind("0.0.0.0", test_port);
      server.start(0);
      io_ctx->run();
   });

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

      std::future<std::string> f = std::async(std::launch::async, [&] { return send_raw(payload); });

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

      std::future<std::string> f = std::async(std::launch::async, [&] { return send_raw(payload); });

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

      std::future<std::string> f = std::async(std::launch::async, [&] { return send_raw(payload); });

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

      std::future<std::string> f = std::async(std::launch::async, [&] { return send_raw(payload); });

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

   server.stop();
   io_ctx->stop();
   if (server_thr.joinable()) server_thr.join();
};

int main() { return 0; }
