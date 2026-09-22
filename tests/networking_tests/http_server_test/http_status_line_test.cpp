// The server's response writers take their reason phrases from the shared
// glz::detail::http_status_reason_phrase table, so every registered status code
// carries its phrase and an unregistered one carries none (RFC 9112 4).
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

static void error_handler(std::error_code, std::source_location) {}

suite http_status_line_suite = [] {
   auto io_ctx = std::make_shared<asio::io_context>();
   glz::http_server<> server(io_ctx, error_handler);
   server.get("/found", [](const glz::request&, glz::response& res) {
      res.status(302).header("Location", "/elsewhere");
   });
   server.get("/unprocessable", [](const glz::request&, glz::response& res) { res.status(422); });
   server.get("/unregistered", [](const glz::request&, glz::response& res) { res.status(299); });
   server.stream_get("/stream-conflict", [](glz::request&, glz::streaming_response& res) {
      res.start_stream(409);
      res.close();
   });

   server.bind(test_host, 0);
   const uint16_t test_port = server.port();
   server.start(0);

   std::thread server_thr([&] { io_ctx->run(); });

   const auto get = [&](std::string_view path) {
      const std::string payload = "GET " + std::string(path) +
                                  " HTTP/1.1\r\n"
                                  "Host: localhost\r\n"
                                  "Connection: close\r\n"
                                  "\r\n";

      std::future<std::string> f = std::async(std::launch::async, [&] { return send_raw(test_port, payload); });

      std::string response;
      if (f.wait_for(5s) == std::future_status::ready) {
         response = f.get();
      }
      return response;
   };

   "a redirect carries its reason phrase"_test = [&] {
      const auto response = get("/found");
      expect(response.starts_with("HTTP/1.1 302 Found\r\n")) << "got: " << response;
   };

   "a status outside the fast path carries its reason phrase"_test = [&] {
      const auto response = get("/unprocessable");
      expect(response.starts_with("HTTP/1.1 422 Unprocessable Content\r\n")) << "got: " << response;
   };

   "an unregistered status carries an empty reason phrase"_test = [&] {
      const auto response = get("/unregistered");
      expect(response.starts_with("HTTP/1.1 299 \r\n")) << "got: " << response;
   };

   "a streamed response carries its reason phrase"_test = [&] {
      const auto response = get("/stream-conflict");
      expect(response.starts_with("HTTP/1.1 409 Conflict\r\n")) << "got: " << response;
   };

   server.stop();
   io_ctx->stop();
   if (server_thr.joinable()) server_thr.join();
};

int main() { return 0; }
