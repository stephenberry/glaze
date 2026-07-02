// Tests that glz::http_client frames a non-chunked response body strictly by
// Content-Length. A server may deliver more than Content-Length octets in the
// same segment (a pipelined next response, or trailing junk); those bytes must
// not leak into response_body. Covers both the synchronous and asynchronous
// client paths.

#include <chrono>
#include <memory>
#include <string>
#include <thread>

#include "glaze/net/http_client.hpp"
#include "ut/ut.hpp"

#if defined(GLZ_USING_BOOST_ASIO)
namespace asio
{
   using namespace boost::asio;
   using error_code = boost::system::error_code;
}
#endif

using namespace ut;

// Start a raw TCP server that reads one request and replies with a hand-crafted
// response written in a single segment.
static std::thread start_raw_server(uint16_t& out_port, const std::string& raw_response)
{
   auto listener = std::make_shared<asio::io_context>();
   auto acceptor = std::make_shared<asio::ip::tcp::acceptor>(*listener);

   asio::ip::tcp::endpoint ep(asio::ip::make_address("127.0.0.1"), 0);
   acceptor->open(ep.protocol());
   acceptor->set_option(asio::socket_base::reuse_address(true));
   acceptor->bind(ep);
   acceptor->listen(1);

   out_port = acceptor->local_endpoint().port();

   return std::thread([listener, acceptor, raw_response] {
      asio::ip::tcp::socket socket(*listener);
      acceptor->accept(socket);

      asio::streambuf req_buf;
      asio::error_code ec;
      asio::read_until(socket, req_buf, "\r\n\r\n", ec);

      asio::write(socket, asio::buffer(raw_response), ec);

      // Hold the connection open briefly so keep-alive framing (not EOF) drives
      // the body length, then close.
      std::this_thread::sleep_for(std::chrono::milliseconds(150));
      socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
      socket.close(ec);
   });
}

// A response whose body is "ABC" (Content-Length: 3) followed by extra octets
// that a naive reader would append to the body.
static const std::string overrun_response =
   "HTTP/1.1 200 OK\r\n"
   "Content-Length: 3\r\n"
   "Connection: keep-alive\r\n"
   "\r\n"
   "ABCHTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nHELLO";

suite content_length_framing = [] {
   "async_body_clamped_to_content_length"_test = [] {
      uint16_t port = 0;
      auto server = start_raw_server(port, overrun_response);

      glz::http_client client;
      auto result = client.get_async("http://127.0.0.1:" + std::to_string(port) + "/", {}).get();

      server.join();

      expect(result.has_value());
      if (result) {
         expect(result->status_code == 200);
         expect(result->response_body == "ABC")
            << "async body must stop at Content-Length, got: " << result->response_body;
      }
   };

   "sync_body_clamped_to_content_length"_test = [] {
      uint16_t port = 0;
      auto server = start_raw_server(port, overrun_response);

      glz::http_client client;
      auto result = client.get("http://127.0.0.1:" + std::to_string(port) + "/");

      server.join();

      expect(result.has_value());
      if (result) {
         expect(result->status_code == 200);
         expect(result->response_body == "ABC")
            << "sync body must stop at Content-Length, got: " << result->response_body;
      }
   };
};

int main() { return 0; }
