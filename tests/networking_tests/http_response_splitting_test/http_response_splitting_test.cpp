// Verifies that glz::http_server does not emit a header field whose name or
// value contains CR or LF. A handler that reflects request-derived data into a
// response header value used to write it verbatim, so a value carrying CR/LF
// terminated the field on the wire and injected attacker-controlled headers
// (CWE-113, HTTP response splitting). The serializer now drops such a field.
#include <atomic>
#include <chrono>
#include <future>
#include <string>
#include <thread>
#include <ut/ut.hpp>

#include "glaze/net/http.hpp"
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
      expect(glz::header_field_has_crlf("X-Echo", "ok\r\nSet-Cookie: sid=evil"));
      expect(glz::header_field_has_crlf("X-Echo", "trailing\n"));
      expect(glz::header_field_has_crlf("bad\rname", "value"));
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

   server.stop();
   io_ctx->stop();
   if (server_thr.joinable()) server_thr.join();
};

int main() { return 0; }
