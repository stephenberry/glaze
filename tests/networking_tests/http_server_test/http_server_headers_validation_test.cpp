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

using namespace ut;
using namespace std::chrono_literals;

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
      std::this_thread::sleep_for(40ms);
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

static void error_handler(std::error_code, std::source_location) {}

suite http_server_headers_validation_suite = [] {
   auto io_ctx = std::make_shared<asio::io_context>();
   glz::http_server<> server(io_ctx, error_handler);

   std::thread server_thr([&] {
      server.bind("0.0.0.0", test_port);
      server.start(0);
      io_ctx->run();
   });

   "request with missing Host header is rejected with 400 bad request"_test = [&] {
      std::string payload =
         "GET /front HTTP/1.1\r\n"
         "Connection: close\r\n"
         "\r\n";

      std::future<std::string> f = std::async(std::launch::async, [&] { return send_raw(payload); });

      std::string response;
      if (f.wait_for(5s) == std::future_status::ready) {
         response = f.get();
      }

      expect(response.contains("HTTP/1.1 400")) << "Expected 400 for missing Host header";
   };

   "request with multiple Host headers is rejected with 400 bad request"_test = [&] {
      std::string payload =
         "GET /front HTTP/1.1\r\n"
         "Host: first\r\n"
         "Host: second\r\n"
         "Connection: close\r\n"
         "\r\n";

      std::future<std::string> f = std::async(std::launch::async, [&] { return send_raw(payload); });

      std::string response;
      if (f.wait_for(5s) == std::future_status::ready) {
         response = f.get();
      }

      expect(response.contains("HTTP/1.1 400")) << "Expected 400 for multiple Host headers";
   };

   "request with case-insensitive multiple Host headers is rejected with 400 bad request"_test = [&] {
      std::string payload =
         "GET /front HTTP/1.1\r\n"
         "Host: first\r\n"
         "hoST: second\r\n"
         "Connection: close\r\n"
         "\r\n";

      std::future<std::string> f = std::async(std::launch::async, [&] { return send_raw(payload); });

      std::string response;
      if (f.wait_for(5s) == std::future_status::ready) {
         response = f.get();
      }

      expect(response.contains("HTTP/1.1 400")) << "Expected 400 for multiple Host headers";
   };

   server.stop();
   io_ctx->stop();
   if (server_thr.joinable()) server_thr.join();
};

int main() { return 0; }
