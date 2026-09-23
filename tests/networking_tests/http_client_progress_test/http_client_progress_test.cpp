// Tests for the streaming progress callback and the body framing it rests on:
//   1. on_progress reports the running body byte count against the total the response
//      framed itself with. The total is known before any body arrives, so a caller can
//      show a determinate progress display from the first callback.
//   2. A body with no knowable total - chunked, or framed by connection close - reports
//      a total of 0 while still counting what has arrived.
//   3. Returning false from on_progress cancels the transfer: no further data is
//      delivered, on_disconnect follows, and the half-read socket is not pooled.
//   4. A Content-Length framed stream ends at its last body byte rather than at the
//      peer's close, which is what makes the total meaningful in the first place and
//      what leaves the socket reusable. Responses that carry no body at all - a HEAD
//      reply, a 304 - end at once however they are framed.

#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <string>
#include <thread>
#include <utility>
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

namespace
{
   struct listening_socket
   {
      std::shared_ptr<asio::io_context> ctx{std::make_shared<asio::io_context>()};
      std::shared_ptr<asio::ip::tcp::acceptor> acceptor{std::make_shared<asio::ip::tcp::acceptor>(*ctx)};
      uint16_t port{};

      explicit listening_socket(int backlog = 2)
      {
         asio::ip::tcp::endpoint ep(asio::ip::make_address("127.0.0.1"), 0);
         acceptor->open(ep.protocol());
         acceptor->set_option(asio::socket_base::reuse_address(true));
         acceptor->bind(ep);
         acceptor->listen(backlog);
         port = acceptor->local_endpoint().port();
      }

      std::string base_url() const { return "http://127.0.0.1:" + std::to_string(port) + "/"; }
   };

   // Reads one request and writes the given segments back to back, so a test can decide
   // exactly how a response is split across the wire. The connection is held open briefly
   // afterwards so that keep-alive framing, rather than EOF, is what ends the body.
   std::thread serve_segments(const listening_socket& listener, std::vector<std::string> segments,
                              std::chrono::milliseconds linger = std::chrono::milliseconds(250))
   {
      return std::thread([ctx = listener.ctx, acceptor = listener.acceptor, segments = std::move(segments), linger] {
         asio::ip::tcp::socket socket(*ctx);
         asio::error_code ec;
         acceptor->accept(socket, ec);
         if (ec) return;

         asio::streambuf request;
         asio::read_until(socket, request, "\r\n\r\n", ec);

         for (const auto& segment : segments) {
            asio::write(socket, asio::buffer(segment), ec);
            if (ec) break;
         }

         std::this_thread::sleep_for(linger);
         socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
         socket.close(ec);
      });
   }

   // Everything a test might want to assert about one stream, collected on the I/O thread
   // and read back on the test thread once the stream has ended.
   struct stream_outcome
   {
      std::string body;
      std::vector<std::pair<size_t, size_t>> reports;
      std::error_code error;
      int error_calls{};
      int data_calls{};
      int disconnects{};
      bool finished{}; // on_disconnect arrived before the deadline
      bool reusable{}; // the socket went back to the pool rather than being closed
   };

   struct stream_options
   {
      std::string method{"GET"};
      glz::stream_read_strategy strategy{glz::stream_read_strategy::bulk_transfer};
      // Consulted after every report; returning false cancels, as it does for any caller.
      std::function<bool(size_t, size_t)> keep_going{};
      // A caller may legitimately supply no error handler. Omitting it here is how the
      // tests reach the paths that must not then throw out of an asio handler.
      bool with_error_handler{true};
   };

   stream_outcome run_stream(glz::http_client& client, const std::string& url, stream_options options = {})
   {
      stream_outcome outcome;
      std::promise<void> done;
      auto finished = done.get_future();

      glz::stream_request_params_v2 params{};
      params.method = options.method;
      params.url = url;
      params.strategy = options.strategy;
      params.on_data = [&](std::string_view data) {
         ++outcome.data_calls;
         outcome.body.append(data);
      };
      if (options.with_error_handler) {
         params.on_error = [&](std::error_code ec) {
            ++outcome.error_calls;
            outcome.error = ec;
         };
      }
      params.on_progress = [&](size_t transferred, size_t total) {
         outcome.reports.emplace_back(transferred, total);
         return options.keep_going ? options.keep_going(transferred, total) : true;
      };
      params.on_disconnect = [&] {
         // set_value() would throw on a second call and the worker loop would swallow it,
         // so the count is what proves on_disconnect arrives exactly once.
         if (++outcome.disconnects == 1) done.set_value();
      };

      auto conn = client.stream_request_v2(params);
      expect(conn != nullptr) << "stream request should start";
      if (!conn) return outcome;

      outcome.finished = finished.wait_for(std::chrono::seconds(5)) == std::future_status::ready;
      outcome.reusable = conn->response_complete.load();
      return outcome;
   }
}

suite progress_reporting = [] {
   // The point of the callback: a determinate total, available before the first body
   // byte, and a count that walks up to it.
   "content_length_progress_reaches_the_declared_total"_test = [] {
      listening_socket listener;
      const std::string body(4096, 'x');
      auto server = serve_segments(listener, {"HTTP/1.1 200 OK\r\nContent-Length: " + std::to_string(body.size()) +
                                                 "\r\nConnection: keep-alive\r\n\r\n",
                                              body.substr(0, 1000), body.substr(1000)});

      glz::http_client client;
      const auto outcome = run_stream(client, listener.base_url());
      server.join();

      expect(outcome.finished)
         << "a Content-Length framed stream must end at its last body byte, not wait for the peer to close";
      expect(outcome.body == body) << "body should arrive intact, got " << outcome.body.size() << " of " << body.size();
      expect(outcome.reports.size() >= 2) << "expected a report at the headers and at least one for the body";
      if (outcome.reports.size() >= 2) {
         expect(outcome.reports.front() == std::pair<size_t, size_t>{0, body.size()})
            << "the total must be known before any body arrives";
         expect(outcome.reports.back() == std::pair<size_t, size_t>{body.size(), body.size()})
            << "the final report must account for the whole body";
      }

      size_t previous = 0;
      for (const auto& [transferred, total] : outcome.reports) {
         expect(total == body.size()) << "every report should carry the same declared total";
         expect(transferred >= previous) << "the byte count must never go backwards";
         previous = transferred;
      }
   };

   // A chunked body has no total to report, and saying so with 0 is what lets a caller
   // choose an indeterminate progress display instead of a wrong determinate one.
   "chunked_progress_reports_no_total"_test = [] {
      listening_socket listener;
      auto server = serve_segments(listener, {"HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n", "5\r\nHELLO\r\n",
                                              "5\r\nWORLD\r\n", "0\r\n\r\n"});

      glz::http_client client;
      const auto outcome = run_stream(client, listener.base_url());
      server.join();

      expect(outcome.finished) << "chunked stream should end";
      expect(outcome.body == "HELLOWORLD") << "body should arrive intact, got: " << outcome.body;
      expect(!outcome.reports.empty()) << "progress should be reported for a chunked body too";
      for (const auto& [transferred, total] : outcome.reports) {
         expect(total == 0u) << "a chunked body has no knowable total";
         (void)transferred;
      }
      if (!outcome.reports.empty()) {
         expect(outcome.reports.back().first == 10u) << "the count should still add up to the body delivered";
      }
   };

   // An empty body is complete the moment the headers are parsed, and the socket behind
   // it is immediately reusable.
   "empty_body_completes_without_waiting_for_close"_test = [] {
      listening_socket listener;
      auto server = serve_segments(listener, {"HTTP/1.1 204 No Content\r\nContent-Length: 0\r\n\r\n"});

      glz::http_client client;
      const auto outcome = run_stream(client, listener.base_url());
      server.join();

      expect(outcome.finished) << "a zero-length body should end the stream at once";
      expect(outcome.data_calls == 0) << "an empty body should not produce a data callback";
      expect(outcome.error_calls == 0) << "an empty body is not an error";
      expect(outcome.reports.size() == 1u) << "exactly the headers-time report is expected";
      if (outcome.reports.size() == 1u) {
         expect(outcome.reports.front() == std::pair<size_t, size_t>{0, 0});
      }
      expect(outcome.reusable) << "a complete response leaves a reusable socket";
   };
};

suite progress_cancellation = [] {
   // Cancelling from the first body report has to stop the transfer there: nothing more
   // is delivered, the stream ends through its normal disconnect path, and the socket is
   // left mid-response and so must not go back to the pool.
   "returning_false_cancels_the_transfer"_test = [] {
      listening_socket listener;

      // Announce far more than will ever be sent, then drip segments until the client goes
      // away. The write failing is the server's signal that the cancel took effect.
      std::thread server([ctx = listener.ctx, acceptor = listener.acceptor] {
         asio::ip::tcp::socket socket(*ctx);
         asio::error_code ec;
         acceptor->accept(socket, ec);
         if (ec) return;

         asio::streambuf request;
         asio::read_until(socket, request, "\r\n\r\n", ec);
         asio::write(socket, asio::buffer(std::string("HTTP/1.1 200 OK\r\nContent-Length: 1048576\r\n\r\n")), ec);

         const std::string segment(4096, 'y');
         for (int i = 0; i < 256 && !ec; ++i) {
            asio::write(socket, asio::buffer(segment), ec);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
         }

         socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
         socket.close(ec);
      });

      glz::http_client client;
      // Keep going while the body is empty (the headers-time report), then cancel.
      const auto outcome = run_stream(client, listener.base_url(),
                                      {.keep_going = [](size_t transferred, size_t) { return transferred == 0; }});
      server.join();

      expect(outcome.finished) << "a cancelled stream must disconnect promptly rather than run to the declared length";
      expect(outcome.disconnects == 1) << "on_disconnect should arrive exactly once, got " << outcome.disconnects;
      expect(outcome.reports.size() == 2u)
         << "no report should follow the one that cancelled, got " << outcome.reports.size();
      expect(outcome.body.size() > 0u) << "the first body span should still have been delivered";
      expect(outcome.body.size() < 1048576u)
         << "cancelling must stop well short of the declared length, got " << outcome.body.size();
      expect(!outcome.reusable) << "a socket abandoned part way through a body must not be pooled";
      for (const auto& [transferred, total] : outcome.reports) {
         expect(total == 1048576u) << "the declared total should be reported";
         (void)transferred;
      }
   };

   // The same rule on the chunked path, which reaches the cancel through its own reader.
   "cancelling_a_chunked_stream_stops_it"_test = [] {
      listening_socket listener;
      auto server = serve_segments(listener, {"HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n", "5\r\nHELLO\r\n",
                                              "5\r\nWORLD\r\n", "0\r\n\r\n"});

      glz::http_client client;
      const auto outcome = run_stream(client, listener.base_url(),
                                      {.keep_going = [](size_t transferred, size_t) { return transferred == 0; }});
      server.join();

      expect(outcome.finished) << "a cancelled chunked stream should disconnect";
      expect(outcome.disconnects == 1) << "on_disconnect should arrive exactly once";
      expect(outcome.body == "HELLO") << "only the chunk that was in flight should arrive, got: " << outcome.body;
      expect(!outcome.reusable) << "a chunked stream abandoned before its terminal chunk must not be pooled";
   };

   // Cancelling from the headers-time report stops the transfer before it costs anything.
   "cancelling_at_the_headers_delivers_no_body"_test = [] {
      listening_socket listener;
      const std::string body(8192, 'z');
      auto server = serve_segments(
         listener, {"HTTP/1.1 200 OK\r\nContent-Length: " + std::to_string(body.size()) + "\r\n\r\n", body});

      glz::http_client client;
      const auto outcome =
         run_stream(client, listener.base_url(), {.keep_going = [](size_t, size_t) { return false; }});
      server.join();

      expect(outcome.finished) << "cancelling at the headers should end the stream";
      expect(outcome.reports.size() == 1u) << "only the headers-time report should run";
      expect(outcome.data_calls == 0) << "no body should be delivered after cancelling at the headers";
   };
};

suite length_framed_streaming = [] {
   // The framing rule the totals rest on. A stream that stops at its last body byte
   // leaves the socket at the start of the next response, which is the one state that
   // makes it reusable; a stream that ran to the peer's close never could be.
   "length_framed_stream_pools_its_socket"_test = [] {
      listening_socket listener;
      auto accepts = std::make_shared<std::atomic<int>>(0);

      // One connection, two responses. If the stream hands its socket back, the follow-up
      // request is answered on this same connection and no second accept ever happens.
      std::thread server([ctx = listener.ctx, acceptor = listener.acceptor, accepts] {
         asio::ip::tcp::socket socket(*ctx);
         asio::error_code ec;
         acceptor->accept(socket, ec);
         if (ec) return;
         accepts->fetch_add(1);

         asio::streambuf request;
         asio::read_until(socket, request, "\r\n\r\n", ec);
         asio::write(socket,
                     asio::buffer(std::string("HTTP/1.1 200 OK\r\nContent-Length: 8\r\nConnection: keep-alive\r\n\r\n"
                                              "STREAMED")),
                     ec);

         // The second request only arrives if the streaming socket went back to the pool.
         // Polled against a deadline rather than blocking: were a regression to stop the
         // socket being reused, an unbounded read would hang this thread, and with it the
         // join() below and the whole suite.
         request.consume(request.size());
         socket.non_blocking(true, ec);
         const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
         bool second_request_arrived = false;
         while (std::chrono::steady_clock::now() < deadline) {
            ec = {};
            asio::read_until(socket, request, "\r\n\r\n", ec);
            if (!ec) {
               second_request_arrived = true;
               break;
            }
            if (ec != asio::error::would_block && ec != asio::error::try_again) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
         }
         socket.non_blocking(false, ec);
         if (second_request_arrived) {
            asio::write(socket, asio::buffer(std::string("HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nHELLO")), ec);
         }

         std::this_thread::sleep_for(std::chrono::milliseconds(100));
         socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
         socket.close(ec);
      });

      glz::http_client client;
      // The acquire-time peek would cull a socket the test wants exercised.
      client.set_pool_active_liveness_check(false);

      const auto outcome = run_stream(client, listener.base_url());
      expect(outcome.finished) << "stream should end at the declared length";
      expect(outcome.body == "STREAMED") << "body should arrive intact, got: " << outcome.body;

      // POST, not GET: a GET would be reopened by the transparent-retry path and hide a
      // socket that was never pooled.
      auto second = client.post_async(listener.base_url(), "two");
      const bool answered = second.wait_for(std::chrono::seconds(10)) == std::future_status::ready;
      expect(answered) << "follow-up request never completed";
      server.join();

      if (answered) {
         auto result = second.get();
         expect(result.has_value()) << "the pooled socket should sit at the start of the next response";
         if (result) {
            expect(result->response_body == "HELLO");
         }
      }
      expect(accepts->load() == 1) << "a length-framed stream should leave a reusable socket; got " << accepts->load()
                                   << " connections";
   };

   // Bytes past the declared length belong to whatever the peer sends next and must never
   // reach on_data, whichever read strategy is in play.
   "body_never_runs_past_the_declared_length"_test = [] {
      for (auto strategy : {glz::stream_read_strategy::bulk_transfer, glz::stream_read_strategy::immediate_delivery}) {
         listening_socket listener;
         auto server =
            serve_segments(listener, {"HTTP/1.1 200 OK\r\nContent-Length: 3\r\nConnection: keep-alive\r\n\r\n"
                                      "ABCHTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nHELLO"});

         glz::http_client client;
         const auto outcome = run_stream(client, listener.base_url(), {.strategy = strategy});
         server.join();

         expect(outcome.finished) << "stream should end";
         expect(outcome.body == "ABC") << "only the declared body should be delivered, got: " << outcome.body;
         expect(!outcome.reusable) << "a peer that sent more than it framed leaves the socket out of step";
      }
   };

   // A peer that closes before the last declared byte has sent an incomplete message
   // (RFC 9112 8). Ending the stream quietly there is what lets a half-finished download
   // pass for a complete file, so it has to surface as an error - as it already does on
   // the buffered paths.
   "truncated_length_framed_stream_is_an_error"_test = [] {
      for (auto strategy : {glz::stream_read_strategy::bulk_transfer, glz::stream_read_strategy::immediate_delivery}) {
         listening_socket listener;
         // Ten octets declared, three delivered, then the peer hangs up.
         auto server = serve_segments(listener, {"HTTP/1.1 200 OK\r\nContent-Length: 10\r\n\r\nABC"},
                                      std::chrono::milliseconds(50));

         glz::http_client client;
         const auto outcome = run_stream(client, listener.base_url(), {.strategy = strategy});
         server.join();

         expect(outcome.finished) << "stream should end";
         expect(outcome.body == "ABC") << "what did arrive should still be delivered, got: " << outcome.body;
         expect(outcome.error_calls == 1) << "a body cut short of its declared length must be reported as an error";
         expect(!outcome.reports.empty());
         if (!outcome.reports.empty()) {
            expect(outcome.reports.back() == std::pair<size_t, size_t>{3, 10})
               << "the final report should show the shortfall against the declared total";
         }
      }
   };

   // The error paths above are reachable for a caller that supplies only on_data, and
   // on_error is not required. Calling it unguarded would throw std::bad_function_call out
   // of an asio completion handler, which the worker loop swallows - taking the disconnect
   // with it, so the socket is neither closed nor pooled and the caller waits forever.
   "a_stream_without_an_error_handler_still_disconnects"_test = [] {
      listening_socket listener;
      auto server =
         serve_segments(listener, {"HTTP/1.1 200 OK\r\nContent-Length: 10\r\n\r\nABC"}, std::chrono::milliseconds(50));

      glz::http_client client;
      const auto outcome = run_stream(client, listener.base_url(), {.with_error_handler = false});
      server.join();

      expect(outcome.finished) << "on_disconnect must arrive even when the caller supplied no on_error";
      expect(outcome.body == "ABC") << "what did arrive should still be delivered, got: " << outcome.body;
   };

   // RFC 9112 6.3: Content-Length fields that disagree frame no body at all. Framing the
   // body by one of the two lengths would leave the remainder on the socket for the next
   // reader to take for a status line.
   "conflicting_content_length_is_refused"_test = [] {
      listening_socket listener;
      auto server =
         serve_segments(listener, {"HTTP/1.1 200 OK\r\nContent-Length: 3\r\nContent-Length: 5\r\n\r\nABCDE"});

      glz::http_client client;
      const auto outcome = run_stream(client, listener.base_url());
      server.join();

      expect(outcome.finished) << "stream should end";
      expect(outcome.error == glz::make_error_code(glz::http_client_error::unframed_response))
         << "a response with disagreeing Content-Length fields must be refused, got: " << outcome.error.message();
      expect(outcome.data_calls == 0) << "no body should be delivered from an unframed response";
      expect(!outcome.reusable) << "an unframed response leaves a socket that cannot be trusted";
   };

   // A response that asked for a close is single-use however cleanly its body ended.
   "connection_close_leaves_no_reusable_socket"_test = [] {
      listening_socket listener;
      auto server = serve_segments(listener, {"HTTP/1.1 200 OK\r\nContent-Length: 3\r\nConnection: close\r\n\r\nABC"});

      glz::http_client client;
      const auto outcome = run_stream(client, listener.base_url());
      server.join();

      expect(outcome.finished) << "stream should end at the declared length";
      expect(outcome.body == "ABC");
      expect(!outcome.reusable) << "a response that asked to close must not leave a pooled socket";
   };

   // RFC 9112 9.3: before HTTP/1.1 the connection closed after each response unless the
   // reply opted back in, so an HTTP/1.0 response with no Connection header is the same
   // single-use case as an explicit close.
   "http_1_0_leaves_no_reusable_socket"_test = [] {
      listening_socket listener;
      auto server = serve_segments(listener, {"HTTP/1.0 200 OK\r\nContent-Length: 3\r\n\r\nABC"});

      glz::http_client client;
      const auto outcome = run_stream(client, listener.base_url());
      server.join();

      expect(outcome.finished) << "stream should end at the declared length";
      expect(outcome.body == "ABC");
      expect(!outcome.reusable) << "HTTP/1.0 defaults to closing, so the socket must not be pooled";
   };
};

suite bodiless_responses = [] {
   // RFC 9110 6.4.1: a Content-Length on a HEAD reply states the length the equivalent
   // GET would have had. Framing the body by it waits for bytes that are never coming,
   // and then reports that wait as a truncated download.
   "head_reply_carries_no_body"_test = [] {
      listening_socket listener;
      auto server = serve_segments(listener, {"HTTP/1.1 200 OK\r\nContent-Length: 4096\r\n\r\n"});

      glz::http_client client;
      const auto outcome = run_stream(client, listener.base_url(), {.method = "HEAD"});
      server.join();

      expect(outcome.finished) << "a HEAD reply should end as soon as its headers are read";
      expect(outcome.data_calls == 0) << "a HEAD reply carries no body";
      expect(outcome.error_calls == 0) << "a HEAD reply is not a truncated response, got: " << outcome.error.message();
      expect(outcome.reusable) << "a HEAD reply leaves the socket at the start of the next response";
   };

   // The same rule by status code rather than by method.
   "not_modified_carries_no_body"_test = [] {
      listening_socket listener;
      auto server = serve_segments(listener, {"HTTP/1.1 304 Not Modified\r\nContent-Length: 1234\r\n\r\n"});

      glz::http_client client;
      // 304 is not an error status here; the default predicate only fires at 400 and above.
      const auto outcome = run_stream(client, listener.base_url());
      server.join();

      expect(outcome.finished) << "a 304 should end as soon as its headers are read";
      expect(outcome.data_calls == 0) << "a 304 carries no body";
      expect(outcome.error_calls == 0) << "a 304 is not a truncated response, got: " << outcome.error.message();
      expect(outcome.reusable) << "a 304 leaves the socket at the start of the next response";
   };

   // Transfer-Encoding does not change the rule: there is no chunked body to read either.
   "head_reply_with_chunked_framing_carries_no_body"_test = [] {
      listening_socket listener;
      auto server = serve_segments(listener, {"HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n"});

      glz::http_client client;
      const auto outcome = run_stream(client, listener.base_url(), {.method = "HEAD"});
      server.join();

      expect(outcome.finished) << "a chunked-framed HEAD reply should still end at its headers";
      expect(outcome.data_calls == 0) << "a HEAD reply carries no body";
      expect(outcome.error_calls == 0) << "got: " << outcome.error.message();
      expect(outcome.reusable) << "a HEAD reply leaves the socket at the start of the next response";
   };
};

int main() { return 0; }
