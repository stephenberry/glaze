// Tests for how glz::http_client frames and finalizes a non-chunked, Content-Length
// response:
//   1. The body is framed strictly by Content-Length. A server may deliver more than
//      Content-Length octets in the same segment (a pipelined next response, or
//      trailing junk); those bytes must not leak into response_body. Covers both the
//      synchronous and asynchronous client paths.
//   2. A body truncated by the peer closing the connection before the declared
//      Content-Length arrives is an incomplete message (RFC 7230 §3.3.3) and is reported
//      as an error, not a short body. Covers both client paths.
//   3. A connection whose body read ends with the peer closing the socket (EOF) is not
//      returned to the pool. Pooling an EOF'd (half-closed) socket would hand the next
//      request a dead connection.

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

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

// A response that declares 10 body octets but delivers only 3 before the peer closes
// the connection. Per RFC 7230 §3.3.3 this is an incomplete message and must surface as
// an error rather than a silently truncated body.
static const std::string truncated_response =
   "HTTP/1.1 200 OK\r\n"
   "Content-Length: 10\r\n"
   "Connection: keep-alive\r\n"
   "\r\n"
   "ABC";

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

   // A Content-Length response truncated by connection close is an incomplete message and
   // must be reported as an error, not a short-but-successful body. Both client paths agree.
   "async_truncated_body_is_error"_test = [] {
      uint16_t port = 0;
      auto server = start_raw_server(port, truncated_response);

      glz::http_client client;
      auto result = client.get_async("http://127.0.0.1:" + std::to_string(port) + "/", {}).get();

      server.join();

      expect(!result.has_value()) << "async: a truncated Content-Length response must be an error, not a short body";
   };

   "sync_truncated_body_is_error"_test = [] {
      uint16_t port = 0;
      auto server = start_raw_server(port, truncated_response);

      glz::http_client client;
      auto result = client.get("http://127.0.0.1:" + std::to_string(port) + "/");

      server.join();

      expect(!result.has_value()) << "sync: a truncated Content-Length response must be an error, not a short body";
   };
};

// Serve a fixed sequence of raw responses, one per accepted connection, closing each
// connection after writing so the client observes EOF at the end of the body. The
// acceptor polls with a deadline instead of blocking, so the server never hangs waiting
// for a connection that a buggy client won't make: if the EOF'd connection is wrongly
// pooled, the follow-up request reuses that dead socket and connection 2 never arrives.
static std::thread serve_sequence(std::shared_ptr<asio::io_context> listener,
                                  std::shared_ptr<asio::ip::tcp::acceptor> acceptor, std::vector<std::string> responses,
                                  std::shared_ptr<std::atomic<int>> accept_count)
{
   return std::thread([listener, acceptor, responses = std::move(responses), accept_count] {
      asio::error_code ec;
      acceptor->non_blocking(true, ec);
      const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
      while (accept_count->load() < static_cast<int>(responses.size()) && std::chrono::steady_clock::now() < deadline) {
         asio::ip::tcp::socket socket(*listener);
         acceptor->accept(socket, ec);
         if (ec == asio::error::would_block || ec == asio::error::try_again) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
         }
         if (ec) break;
         const size_t n = static_cast<size_t>(accept_count->fetch_add(1));
         socket.non_blocking(false, ec); // the accepted socket may inherit non-blocking mode
         asio::streambuf req_buf;
         asio::read_until(socket, req_buf, "\r\n\r\n", ec);
         asio::write(socket, asio::buffer(responses[n]), ec);
         socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
         socket.close(ec);
      }
   });
}

suite content_length_connection_reuse = [] {
   // A Content-Length response whose body is truncated by the peer closing the connection
   // (declares 10 octets, delivers 3 then EOF) must not leave the socket in the pool. The
   // follow-up request has to open a fresh connection rather than reuse a dead one. POST is
   // used so the transparent-retry path (idempotent methods only) can't mask a wrongly
   // pooled socket, and the acquire-time liveness peek is disabled so a pooled dead socket
   // would actually be handed back out.
   "eof_truncated_response_not_pooled"_test = [] {
      auto listener = std::make_shared<asio::io_context>();
      auto acceptor = std::make_shared<asio::ip::tcp::acceptor>(*listener);
      asio::ip::tcp::endpoint ep(asio::ip::make_address("127.0.0.1"), 0);
      acceptor->open(ep.protocol());
      acceptor->set_option(asio::socket_base::reuse_address(true));
      acceptor->bind(ep);
      acceptor->listen(2);
      const uint16_t port = acceptor->local_endpoint().port();

      auto accept_count = std::make_shared<std::atomic<int>>(0);
      std::vector<std::string> responses = {
         "HTTP/1.1 200 OK\r\nContent-Length: 10\r\nConnection: keep-alive\r\n\r\nABC",
         "HTTP/1.1 200 OK\r\nContent-Length: 5\r\nConnection: keep-alive\r\n\r\nHELLO",
      };
      auto server = serve_sequence(listener, acceptor, responses, accept_count);

      glz::http_client client;
      // Force a pooled dead socket to be reused rather than culled by the acquire-time peek.
      client.set_pool_active_liveness_check(false);

      const std::string base = "http://127.0.0.1:" + std::to_string(port) + "/";
      client.post_async(base, "one").get(); // truncated response; peer closes connection 1
      auto second = client.post_async(base, "two").get(); // must land on a fresh connection 2

      server.join();

      expect(second.has_value()) << "after an EOF-truncated response the connection must not be pooled; the follow-up "
                                    "request must open a fresh connection instead of reusing a dead socket";
      if (second) {
         expect(second->status_code == 200);
         expect(second->response_body == "HELLO");
      }
      expect(accept_count->load() == 2) << "expected two server connections (dead socket not reused), got: "
                                        << accept_count->load();
   };
};

int main() { return 0; }
