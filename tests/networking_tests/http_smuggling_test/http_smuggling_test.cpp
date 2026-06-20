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

   server.stop();
   io_ctx->stop();
   if (server_thr.joinable()) server_thr.join();
};

int main() { return 0; }
