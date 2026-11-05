// Verifies that glz::http_server bounds what one connection can make it hold before
// a request's headers are complete:
//   1. A header section that does not end within max_request_header_size is answered
//      with 431 and the connection is closed, instead of the read buffer growing for
//      as long as the peer keeps sending header bytes.
//   2. The idle timeout covers a connection's first request too, and releases the
//      connection even when the peer ignores the server's FIN, so a request that
//      completes after the deadline never reaches a handler.
#include <array>
#include <atomic>
#include <chrono>
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
   constexpr size_t header_limit = 1024;

   // Connect, retrying briefly while the acceptor warms up.
   bool connect_to(asio::ip::tcp::socket& socket, uint16_t port)
   {
      asio::ip::tcp::endpoint endpoint(asio::ip::make_address(test_host), port);
      asio::error_code ec;
      for (int tries = 0; tries < 50; ++tries) {
         socket.connect(endpoint, ec);
         if (!ec) return true;
         socket.close(ec);
         std::this_thread::sleep_for(std::chrono::milliseconds(20));
      }
      return false;
   }

   // Read until the server closes the connection. A connection still open after five
   // seconds yields "TIMEOUT", so a server that never closes fails the test instead of
   // stalling it.
   std::string read_until_closed(asio::ip::tcp::socket& socket)
   {
      socket.non_blocking(true);
      std::string resp;
      std::array<char, 4096> buf{};
      const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
      while (std::chrono::steady_clock::now() < deadline) {
         asio::error_code ec;
         const std::size_t n = socket.read_some(asio::buffer(buf), ec);
         resp.append(buf.data(), n);
         if (ec == asio::error::would_block) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
         }
         else if (ec) {
            return resp;
         }
      }
      return "TIMEOUT";
   }

   std::string send_raw(uint16_t port, const std::string& request)
   {
      asio::io_context io_ctx;
      asio::ip::tcp::socket socket(io_ctx);
      if (!connect_to(socket, port)) {
         return "";
      }
      asio::error_code ec;
      asio::write(socket, asio::buffer(request.data(), request.size()), ec);
      return read_until_closed(socket);
   }

   // A GET whose request line and headers, final "\r\n\r\n" included, are `size` bytes.
   std::string header_section(size_t size)
   {
      std::string s = "GET /ok HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\nX-Pad: ";
      s.append(size - s.size() - 4, 'a');
      s += "\r\n\r\n";
      return s;
   }
} // namespace

static void error_handler(std::error_code, std::source_location) {}

suite http_request_limits_suite = [] {
   auto io_ctx = std::make_shared<asio::io_context>();
   glz::http_server<false> server(io_ctx, error_handler);

   std::atomic<int> ok_hits{0};

   server.get("/ok", [&](const glz::request&, glz::response& res) {
      ok_hits.fetch_add(1, std::memory_order_relaxed);
      res.status(200);
      res.body("ok");
   });

   server.max_request_header_size(header_limit);
   server.keep_alive_timeout(1);

   // Bind to an ephemeral port and read it back, so the test never collides with
   // another listener or a prior run stuck in TIME_WAIT.
   server.bind(test_host, 0);
   const uint16_t port = server.port();
   server.start(0);
   std::thread server_thr([&] { io_ctx->run(); });

   "header section that fills the limit is accepted"_test = [&] {
      const std::string response = send_raw(port, header_section(header_limit));
      expect(response.starts_with("HTTP/1.1 200")) << response;
   };

   "header section that does not end within the limit is rejected with 431"_test = [&] {
      // Every byte the limit allows arrives, but the final "\n" does not; the server
      // must answer instead of buffering whatever comes next. Sending no more than the
      // limit leaves nothing unread when the server closes, so no RST races the reply.
      const std::string payload = header_section(header_limit + 1).substr(0, header_limit);
      const std::string response = send_raw(port, payload);
      expect(response.starts_with("HTTP/1.1 431")) << response;
   };

   "pipelined header cannot bypass the limit"_test = [&] {
      const std::string first = "GET /ok HTTP/1.1\r\nHost: localhost\r\n\r\n";
      const std::string second = header_section(header_limit + 1).substr(0, header_limit);
      const std::string response = send_raw(port, first + second);
      expect(response.starts_with("HTTP/1.1 200")) << response;
      expect(response.find("HTTP/1.1 431") != std::string::npos) << response;
   };

   "first request that never completes is closed after the idle timeout"_test = [&] {
      const std::string response = send_raw(port, "GET /ok HTTP/1.1\r\nHost: localhost\r\n");
      expect(response.empty()) << response;
   };

   "request completed after the idle timeout does not reach its handler"_test = [&] {
      // The peer ignores the FIN and finishes its request late; by then the server
      // must have released the connection rather than still be reading from it.
      const int hits_before = ok_hits.load(std::memory_order_relaxed);
      asio::io_context client_ctx;
      asio::ip::tcp::socket socket(client_ctx);
      expect(connect_to(socket, port));
      asio::error_code ec;
      asio::write(socket, asio::buffer(std::string_view{"GET /ok HTTP/1.1\r\nHost: localhost\r\n"}), ec);
      expect(read_until_closed(socket).empty()) << "server did not close the idle connection";
      asio::write(socket, asio::buffer(std::string_view{"\r\n"}), ec);
      std::this_thread::sleep_for(std::chrono::milliseconds(300));
      expect(ok_hits.load(std::memory_order_relaxed) == hits_before) << "late request reached a route handler";
   };

   server.stop();
   io_ctx->stop();
   if (server_thr.joinable()) server_thr.join();
};

int main() { return 0; }
