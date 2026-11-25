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

#include "glaze/ext/asio_include.hpp"

import std;

import glaze.net.http_client;
import glaze.net.http_router;

import ut;

using std::size_t;
using std::uint16_t;

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

// Two Content-Length fields that disagree. Framing by either one leaves the
// remainder of the segment on the socket for the next read to pick up as a
// response the server never sent - here a 200 carrying "INJECTED". RFC 9112 §6.3
// leaves no correct choice between them, so the message has to be rejected.
static const std::string conflicting_content_length_response =
   "HTTP/1.1 200 OK\r\n"
   "Content-Length: 3\r\n"
   "Content-Length: 47\r\n"
   "Connection: keep-alive\r\n"
   "\r\n"
   "ABCHTTP/1.1 200 OK\r\nContent-Length: 8\r\n\r\nINJECTED";

// Repeated but identical, which resolves to one unambiguous length. RFC 9112 §6.3
// permits collapsing these, and the server does the same for a request, so the
// client must not reject the response.
static const std::string agreeing_content_length_response =
   "HTTP/1.1 200 OK\r\n"
   "Content-Length: 3\r\n"
   "Content-Length: 3\r\n"
   "Connection: keep-alive\r\n"
   "\r\n"
   "ABC";

// A Content-Length that is not a bare decimal resolves to no length at all.
// Defaulting it to zero would leave the body on the socket to be read as the head
// of the next response.
static const std::string malformed_content_length_response =
   "HTTP/1.1 200 OK\r\n"
   "Content-Length: 3, 3\r\n"
   "Connection: keep-alive\r\n"
   "\r\n"
   "ABC";

// Chunked framing plus a Content-Length that would be rejected on its own. RFC 9112
// 6.3 has Transfer-Encoding override Content-Length, so the body comes from the chunk
// sizes and the unusable field is simply never read.
static const std::string chunked_with_malformed_content_length_response =
   "HTTP/1.1 200 OK\r\n"
   "Transfer-Encoding: chunked\r\n"
   "Content-Length: abc\r\n"
   "Connection: keep-alive\r\n"
   "\r\n"
   "3\r\nABC\r\n0\r\n\r\n";

// RFC 9112 5 admits OWS on both sides of a field value, and RFC 9110 5.5 requires a
// recipient to strip it. These fields are conformant, so a client that frames the
// body by a strict parse of Content-Length has to trim before parsing rather than
// reject the response.
static const std::string padded_content_length_response =
   "HTTP/1.1 200 OK\r\n"
   "Content-Length: 3 \r\n"
   "X-Padded: \tspaced out\t \r\n"
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

// A response the client cannot frame to a single body length must fail the request
// outright. Accepting one of two disagreeing lengths is the client half of the
// smuggling desync the server already rejects on a request (CL.CL, 400 + close).
suite conflicting_content_length = [] {
   "async_conflicting_content_length_is_error"_test = [] {
      uint16_t port = 0;
      auto server = start_raw_server(port, conflicting_content_length_response);

      glz::http_client client;
      auto result = client.get_async("http://127.0.0.1:" + std::to_string(port) + "/", {}).get();

      server.join();

      expect(!result.has_value()) << "async: disagreeing Content-Length fields must fail the request";
      if (!result.has_value()) {
         expect(result.error() == glz::make_error_code(glz::http_client_error::unframed_response));
      }
      else {
         expect(result->response_body.find("INJECTED") == std::string::npos)
            << "a smuggled response must never surface as body data";
      }
   };

   "sync_conflicting_content_length_is_error"_test = [] {
      uint16_t port = 0;
      auto server = start_raw_server(port, conflicting_content_length_response);

      glz::http_client client;
      auto result = client.get("http://127.0.0.1:" + std::to_string(port) + "/");

      server.join();

      expect(!result.has_value()) << "sync: disagreeing Content-Length fields must fail the request";
      if (!result.has_value()) {
         expect(result.error() == glz::make_error_code(glz::http_client_error::unframed_response));
      }
      else {
         expect(result->response_body.find("INJECTED") == std::string::npos)
            << "a smuggled response must never surface as body data";
      }
   };

   "async_repeated_agreeing_content_length_is_accepted"_test = [] {
      uint16_t port = 0;
      auto server = start_raw_server(port, agreeing_content_length_response);

      glz::http_client client;
      auto result = client.get_async("http://127.0.0.1:" + std::to_string(port) + "/", {}).get();

      server.join();

      expect(result.has_value()) << "async: identical repeats resolve to one length and must be accepted";
      if (result) {
         expect(result->response_body == "ABC");
      }
   };

   "sync_repeated_agreeing_content_length_is_accepted"_test = [] {
      uint16_t port = 0;
      auto server = start_raw_server(port, agreeing_content_length_response);

      glz::http_client client;
      auto result = client.get("http://127.0.0.1:" + std::to_string(port) + "/");

      server.join();

      expect(result.has_value()) << "sync: identical repeats resolve to one length and must be accepted";
      if (result) {
         expect(result->response_body == "ABC");
      }
   };

   "sync_malformed_content_length_is_error"_test = [] {
      uint16_t port = 0;
      auto server = start_raw_server(port, malformed_content_length_response);

      glz::http_client client;
      auto result = client.get("http://127.0.0.1:" + std::to_string(port) + "/");

      server.join();

      expect(!result.has_value()) << "sync: a Content-Length that is not a bare decimal must fail the request";
      if (!result.has_value()) {
         expect(result.error() == glz::make_error_code(glz::http_client_error::unframed_response));
      }
   };

   "async_malformed_content_length_is_error"_test = [] {
      uint16_t port = 0;
      auto server = start_raw_server(port, malformed_content_length_response);

      glz::http_client client;
      auto result = client.get_async("http://127.0.0.1:" + std::to_string(port) + "/", {}).get();

      server.join();

      expect(!result.has_value()) << "async: a Content-Length that is not a bare decimal must fail the request";
      if (!result.has_value()) {
         expect(result.error() == glz::make_error_code(glz::http_client_error::unframed_response));
      }
   };
};

// Trailing OWS is legal and must be stripped before the value is evaluated, not
// treated as part of it. The server already does this; the client parsers must
// agree, or a conformant response fails the strict Content-Length parse.
suite optional_whitespace_around_field_values = [] {
   "sync_trailing_whitespace_is_stripped"_test = [] {
      uint16_t port = 0;
      auto server = start_raw_server(port, padded_content_length_response);

      glz::http_client client;
      auto result = client.get("http://127.0.0.1:" + std::to_string(port) + "/");

      server.join();

      expect(result.has_value()) << "sync: 'Content-Length: 3 ' is conformant and must not fail the request";
      if (result) {
         expect(result->response_body == "ABC");
         expect(result->response_headers.first_value("Content-Length") == "3") << "trailing OWS must not be stored";
         expect(result->response_headers.first_value("X-Padded") == "spaced out")
            << "OWS must be stripped from both ends of every field, not just Content-Length";
      }
   };

   "async_trailing_whitespace_is_stripped"_test = [] {
      uint16_t port = 0;
      auto server = start_raw_server(port, padded_content_length_response);

      glz::http_client client;
      auto result = client.get_async("http://127.0.0.1:" + std::to_string(port) + "/", {}).get();

      server.join();

      expect(result.has_value()) << "async: 'Content-Length: 3 ' is conformant and must not fail the request";
      if (result) {
         expect(result->response_body == "ABC");
         expect(result->response_headers.first_value("Content-Length") == "3") << "trailing OWS must not be stored";
         expect(result->response_headers.first_value("X-Padded") == "spaced out")
            << "OWS must be stripped from both ends of every field, not just Content-Length";
      }
   };
};

// A chunked response is framed by its chunk sizes, so a Content-Length the client
// never consults must not decide whether the request succeeds.
suite chunked_overrides_content_length = [] {
   "sync_chunked_ignores_an_unusable_content_length"_test = [] {
      uint16_t port = 0;
      auto server = start_raw_server(port, chunked_with_malformed_content_length_response);

      glz::http_client client;
      auto result = client.get("http://127.0.0.1:" + std::to_string(port) + "/");

      server.join();

      expect(result.has_value()) << "sync: chunked framing must win over an unusable Content-Length";
      if (result) {
         expect(result->response_body == "ABC") << "body must come from the chunk sizes";
      }
   };

   "async_chunked_ignores_an_unusable_content_length"_test = [] {
      uint16_t port = 0;
      auto server = start_raw_server(port, chunked_with_malformed_content_length_response);

      glz::http_client client;
      auto result = client.get_async("http://127.0.0.1:" + std::to_string(port) + "/", {}).get();

      server.join();

      expect(result.has_value()) << "async: chunked framing must win over an unusable Content-Length";
      if (result) {
         expect(result->response_body == "ABC") << "body must come from the chunk sizes";
      }
   };
};

// read_content_length is the single decision point both client paths share, so pin
// its behavior directly rather than only through the socket harness.
suite read_content_length_unit = [] {
   "absent_when_no_field"_test = [] {
      const auto parsed = glz::detail::read_content_length(glz::http_headers{{"Connection", "keep-alive"}});
      expect(parsed.state == glz::detail::content_length_state::absent);
   };

   "present_for_a_single_decimal"_test = [] {
      const auto parsed = glz::detail::read_content_length(glz::http_headers{{"Content-Length", "42"}});
      expect(parsed.state == glz::detail::content_length_state::present);
      expect(parsed.value == 42u);
   };

   "name_case_is_ignored"_test = [] {
      const auto parsed = glz::detail::read_content_length(glz::http_headers{{"content-length", "7"}});
      expect(parsed.state == glz::detail::content_length_state::present);
      expect(parsed.value == 7u);
   };

   "identical_repeats_collapse"_test = [] {
      const auto parsed =
         glz::detail::read_content_length(glz::http_headers{{"Content-Length", "3"}, {"Content-Length", "3"}});
      expect(parsed.state == glz::detail::content_length_state::present);
      expect(parsed.value == 3u);
   };

   "disagreeing_repeats_are_unframed"_test = [] {
      const auto parsed =
         glz::detail::read_content_length(glz::http_headers{{"Content-Length", "3"}, {"Content-Length", "47"}});
      expect(parsed.state == glz::detail::content_length_state::unframed);
   };

   "a_trailing_list_is_unframed"_test = [] {
      const auto parsed = glz::detail::read_content_length(glz::http_headers{{"Content-Length", "3, 3"}});
      expect(parsed.state == glz::detail::content_length_state::unframed);
   };

   "non_digits_are_unframed"_test = [] {
      expect(glz::detail::read_content_length(glz::http_headers{{"Content-Length", "abc"}}).state ==
             glz::detail::content_length_state::unframed);
      expect(glz::detail::read_content_length(glz::http_headers{{"Content-Length", ""}}).state ==
             glz::detail::content_length_state::unframed);
      expect(glz::detail::read_content_length(glz::http_headers{{"Content-Length", "-1"}}).state ==
             glz::detail::content_length_state::unframed);
      expect(glz::detail::read_content_length(glz::http_headers{{"Content-Length", "+1"}}).state ==
             glz::detail::content_length_state::unframed);
   };

   // Transfer-Encoding overrides Content-Length (RFC 9112 6.3), so a chunked
   // response reports no Content-Length framing to apply and a field that would
   // otherwise be rejected cannot fail the request. Without this the outcome would
   // hinge on whether a value we never read happens to parse.
   "chunked_ignores_content_length_entirely"_test = [] {
      const auto well_formed =
         glz::detail::read_content_length(glz::http_headers{{"Transfer-Encoding", "chunked"}, {"Content-Length", "5"}});
      expect(well_formed.state == glz::detail::content_length_state::absent);

      const auto malformed = glz::detail::read_content_length(
         glz::http_headers{{"Transfer-Encoding", "chunked"}, {"Content-Length", "abc"}});
      expect(malformed.state == glz::detail::content_length_state::absent)
         << "a Content-Length that is never consulted must not fail the request";

      const auto conflicting = glz::detail::read_content_length(
         glz::http_headers{{"Transfer-Encoding", "chunked"}, {"Content-Length", "3"}, {"Content-Length", "47"}});
      expect(conflicting.state == glz::detail::content_length_state::absent)
         << "chunked framing resolves the ambiguity the two lengths would otherwise create";

      // The override only applies to chunked. Another transfer coding leaves the
      // Content-Length as the body framing.
      const auto gzip_only =
         glz::detail::read_content_length(glz::http_headers{{"Transfer-Encoding", "gzip"}, {"Content-Length", "3"}});
      expect(gzip_only.state == glz::detail::content_length_state::present);
      expect(gzip_only.value == 3u);
   };

   "an_overflowing_length_is_unframed"_test = [] {
      // Silently truncating this to a small number would frame the body short and
      // leave the remainder to be read as the next response.
      const auto parsed =
         glz::detail::read_content_length(glz::http_headers{{"Content-Length", "99999999999999999999999"}});
      expect(parsed.state == glz::detail::content_length_state::unframed);
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
