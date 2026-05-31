// Tests covering glz::http_client connection-pool reuse, idle eviction, and
// transparent retry on stale keep-alive connections. The pool tests were added
// after diagnosing intermittent failures against FastAPI/uvicorn servers, where
// a kept-alive socket would be picked up from the pool after the server had
// already closed it, leading to a write-then-EOF failure on what looked from the
// caller's side like a healthy connection.
//
// We use a hand-rolled HTTP/1.1 mini-server here rather than glz::http_server so
// the tests can exercise specific keep-alive behaviors on demand: closing
// immediately after each response, closing on server-side idle, etc.

#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "glaze/ext/glaze_asio.hpp"
#include "glaze/net/http_client.hpp"
#include "ut/ut.hpp"

using namespace ut;
using namespace glz;

namespace
{
   // Minimal HTTP/1.1 server with controllable keep-alive behavior.
   // Counts accepts and requests so tests can verify pool reuse / re-connect patterns.
   class control_server
   {
     public:
      struct config
      {
         // If true, every response is sent with `Connection: close` and the server
         // closes the socket. Mimics a server that does not support keep-alive.
         bool close_after_each_response = false;
         // If true, the server sends a normal keep-alive response and then immediately
         // closes the socket WITHOUT a Connection: close header. The client has no way to
         // know the connection is now dead and will pool it. This is the deterministic
         // way to exercise the active-peek check and (with the peek disabled) the
         // transparent-retry path.
         bool silently_close_after_response = false;
         // If > 0, the server accepts and immediately closes the first N connections
         // before reading anything. Mimics listener overload, server crash between
         // accept and read, or rate-limiting close. This forces the client into a
         // fresh-socket-RST-during-write scenario whose retry coverage is the whole
         // point of the response_started gate.
         int drop_first_n_connections = 0;
         // If non-zero, the server closes any connection that is idle (between
         // requests) longer than this. Mimics uvicorn's keep-alive timeout.
         std::chrono::milliseconds idle_timeout{0};
         // If non-zero, the server delays this long before reading the request.
         // Mainly useful for forcing concurrency (multiple connections in flight).
         std::chrono::milliseconds response_delay{0};
      };

      control_server() = default;
      ~control_server() { stop(); }

      bool start() { return start(config{}); }

      bool start(config cfg)
      {
         cfg_ = cfg;

         asio::error_code ec;
         asio::ip::tcp::endpoint ep(asio::ip::address_v4::loopback(), 0);
         acceptor_.open(ep.protocol(), ec);
         if (ec) return false;
         acceptor_.set_option(asio::ip::tcp::acceptor::reuse_address(true), ec);
         acceptor_.bind(ep, ec);
         if (ec) return false;
         acceptor_.listen(asio::socket_base::max_listen_connections, ec);
         if (ec) return false;
         port_ = acceptor_.local_endpoint().port();

         work_.emplace(io_ctx_.get_executor());
         do_accept();

         thread_ = std::thread([this]() { io_ctx_.run(); });

         // listen() has already returned, so the socket is accepting connections by the
         // time start() returns. No sleep needed.
         return true;
      }

      void stop()
      {
         if (!work_) return;
         asio::post(io_ctx_, [this]() {
            asio::error_code ec;
            acceptor_.close(ec);
         });
         work_.reset();
         io_ctx_.stop();
         if (thread_.joinable()) thread_.join();
         io_ctx_.restart();
         work_.reset();
      }

      uint16_t port() const { return port_; }
      std::string base_url() const { return "http://127.0.0.1:" + std::to_string(port_); }

      int accept_count() const { return accept_count_.load(); }
      int request_count() const { return request_count_.load(); }

     private:
      asio::io_context io_ctx_{1};
      std::optional<asio::executor_work_guard<asio::io_context::executor_type>> work_;
      asio::ip::tcp::acceptor acceptor_{io_ctx_};
      std::thread thread_;
      uint16_t port_ = 0;
      config cfg_;
      std::atomic<int> accept_count_{0};
      std::atomic<int> request_count_{0};

      struct connection : std::enable_shared_from_this<connection>
      {
         asio::ip::tcp::socket socket;
         asio::streambuf buffer;
         asio::steady_timer idle_timer;
         control_server* server;

         connection(asio::ip::tcp::socket s, control_server* srv)
            : socket(std::move(s)), idle_timer(socket.get_executor()), server(srv)
         {}

         void start() { read_request_headers(); }

         void arm_idle_timer()
         {
            if (server->cfg_.idle_timeout.count() <= 0) return;
            idle_timer.expires_after(server->cfg_.idle_timeout);
            auto self = shared_from_this();
            idle_timer.async_wait([self](asio::error_code ec) {
               if (!ec) {
                  asio::error_code close_ec;
                  self->socket.close(close_ec);
               }
            });
         }

         void cancel_idle_timer() { idle_timer.cancel(); }

         void read_request_headers()
         {
            arm_idle_timer();
            auto self = shared_from_this();
            asio::async_read_until(socket, buffer, "\r\n\r\n",
                                   [this, self](asio::error_code ec, std::size_t bytes_transferred) {
                                      cancel_idle_timer();
                                      if (ec) return;
                                      handle_headers(bytes_transferred);
                                   });
         }

         void handle_headers(std::size_t header_bytes)
         {
            std::string_view header_view{static_cast<const char*>(buffer.data().data()), header_bytes};

            std::size_t content_length = 0;
            // Case-insensitive search for Content-Length.
            for (std::size_t i = 0; i + 15 < header_view.size(); ++i) {
               static constexpr std::string_view target = "content-length:";
               bool match = true;
               for (std::size_t j = 0; j < target.size(); ++j) {
                  char c = header_view[i + j];
                  if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
                  if (c != target[j]) {
                     match = false;
                     break;
                  }
               }
               if (match) {
                  std::size_t v = i + target.size();
                  while (v < header_view.size() && (header_view[v] == ' ' || header_view[v] == '\t')) ++v;
                  std::size_t end = header_view.find('\r', v);
                  if (end != std::string_view::npos) {
                     try {
                        content_length =
                           static_cast<std::size_t>(std::stoul(std::string(header_view.substr(v, end - v))));
                     }
                     catch (...) {
                        // Malformed Content-Length: leave it at zero, the test will see
                        // an unexpected response rather than crashing the io_context.
                     }
                  }
                  break;
               }
            }

            buffer.consume(header_bytes);
            read_request_body(content_length);
         }

         void read_request_body(std::size_t content_length)
         {
            if (buffer.size() >= content_length) {
               buffer.consume(content_length);
               on_request_complete();
               return;
            }
            std::size_t need = content_length - buffer.size();
            auto self = shared_from_this();
            asio::async_read(socket, buffer, asio::transfer_exactly(need),
                             [this, self, content_length](asio::error_code ec, std::size_t) {
                                if (ec) return;
                                buffer.consume(content_length);
                                on_request_complete();
                             });
         }

         void on_request_complete()
         {
            ++server->request_count_;

            auto send_response = [self = shared_from_this(), this]() {
               auto body = std::make_shared<std::string>("ok");
               auto resp = std::make_shared<std::string>();
               resp->append("HTTP/1.1 200 OK\r\n");
               resp->append("Content-Type: text/plain\r\n");
               resp->append("Content-Length: ");
               resp->append(std::to_string(body->size()));
               resp->append("\r\n");
               if (server->cfg_.close_after_each_response) {
                  resp->append("Connection: close\r\n");
               }
               else {
                  resp->append("Connection: keep-alive\r\n");
               }
               resp->append("\r\n");
               resp->append(*body);

               asio::async_write(
                  socket, asio::buffer(*resp), [self, this, resp, body](asio::error_code ec, std::size_t) {
                     if (ec) return;
                     if (server->cfg_.close_after_each_response || server->cfg_.silently_close_after_response) {
                        asio::error_code close_ec;
                        socket.shutdown(asio::ip::tcp::socket::shutdown_send, close_ec);
                        socket.close(close_ec);
                     }
                     else {
                        read_request_headers();
                     }
                  });
            };

            if (server->cfg_.response_delay.count() > 0) {
               auto t = std::make_shared<asio::steady_timer>(socket.get_executor());
               t->expires_after(server->cfg_.response_delay);
               t->async_wait([t, send_response](asio::error_code) { send_response(); });
            }
            else {
               send_response();
            }
         }
      };

      void do_accept()
      {
         acceptor_.async_accept([this](asio::error_code ec, asio::ip::tcp::socket s) {
            if (ec) return;
            const int n = ++accept_count_;
            if (n <= cfg_.drop_first_n_connections) {
               // Close immediately without reading or replying; the client will see
               // its write succeed locally and then the read will fail with EOF/RST.
               asio::error_code close_ec;
               s.close(close_ec);
            }
            else {
               std::make_shared<connection>(std::move(s), this)->start();
            }
            do_accept();
         });
      }
   };
}

suite pool_tests = [] {
   "pool_reuse_single_accept"_test = [] {
      // Five sequential GETs against a server that supports keep-alive should
      // open exactly one TCP connection and reuse it for every request.
      control_server server;
      expect(server.start());

      glz::http_client client{};
      for (int i = 0; i < 5; ++i) {
         auto r = client.get(server.base_url() + "/hello");
         expect(r.has_value()) << "request " << i << " should succeed\n";
         if (r) expect(r->status_code == 200);
      }

      expect(server.request_count() == 5);
      expect(server.accept_count() == 1) << "all five requests should reuse one connection, got "
                                         << server.accept_count() << " accepts\n";
   };

   "server_closes_each_response"_test = [] {
      // When the server insists on closing after each response, every request
      // requires a fresh TCP connection. The client must handle this without
      // any retries needed (the close header tells us to drop the socket).
      control_server server;
      expect(server.start({.close_after_each_response = true}));

      glz::http_client client{};
      for (int i = 0; i < 4; ++i) {
         auto r = client.get(server.base_url() + "/hello");
         expect(r.has_value()) << "request " << i << " should succeed\n";
      }

      expect(server.request_count() == 4);
      expect(server.accept_count() == 4);
   };

   "stale_after_server_idle_close_get"_test = [] {
      // Server closes any kept-alive connection that's idle longer than 100ms.
      // The client's pool default idle timeout is longer, but the active peek
      // check in acquire() should catch the closed socket and we should open a
      // fresh one for the second request.
      control_server server;
      expect(server.start({.idle_timeout = std::chrono::milliseconds(100)}));

      glz::http_client client{};
      auto r1 = client.get(server.base_url() + "/hello");
      expect(r1.has_value());

      // Wait long enough for the server to time out the idle connection.
      std::this_thread::sleep_for(std::chrono::milliseconds(250));

      auto r2 = client.get(server.base_url() + "/hello");
      expect(r2.has_value()) << "GET after server-side idle close should still succeed\n";
      if (r2) expect(r2->status_code == 200);

      expect(server.request_count() == 2);
      expect(server.accept_count() == 2) << "second request should have used a new connection\n";
   };

   "stale_after_server_idle_close_post"_test = [] {
      // Same scenario but POST. POST is not retried after a write+read failure,
      // but the active peek check on acquire still rejects the dead socket
      // before any bytes are written, so this should still succeed.
      control_server server;
      expect(server.start({.idle_timeout = std::chrono::milliseconds(100)}));

      glz::http_client client{};
      auto r1 = client.post(server.base_url() + "/echo", "first");
      expect(r1.has_value());

      std::this_thread::sleep_for(std::chrono::milliseconds(250));

      auto r2 = client.post(server.base_url() + "/echo", "second");
      expect(r2.has_value()) << "POST after server-side idle close should succeed via peek\n";
      if (r2) expect(r2->status_code == 200);

      expect(server.request_count() == 2);
   };

   "client_idle_timeout_eviction"_test = [] {
      // Isolate timestamp eviction: server keep-alive is intact (so the connection is
      // genuinely alive at the TCP level) and the active peek is disabled (so the only
      // mechanism that can evict the pooled socket is the client-side timestamp check).
      // Without disabling peek, this test would also pass if peek ever started
      // incorrectly evicting alive sockets - it would not isolate the property the test
      // name claims.
      control_server server;
      expect(server.start());

      glz::http_client client{};
      client.set_pool_idle_timeout(std::chrono::milliseconds(50));
      client.set_pool_active_liveness_check(false);

      auto r1 = client.get(server.base_url() + "/hello");
      expect(r1.has_value());

      std::this_thread::sleep_for(std::chrono::milliseconds(150));

      auto r2 = client.get(server.base_url() + "/hello");
      expect(r2.has_value());

      expect(server.accept_count() == 2) << "client idle timeout should have evicted the first socket\n";
   };

   "concurrent_pool_requests_retention"_test = [] {
      // Burst of N concurrent requests must all succeed AND all N sockets must be
      // retained for reuse. The follow-up assertion uses a *concurrent* round of N
      // (not serial) because a serial round only needs one socket at a time and would
      // pass even with a tiny pool. Bump the per-host cap above the burst size so we
      // are testing pool retention, not the cap itself (covered separately).
      control_server server;
      control_server::config cfg;
      cfg.response_delay = std::chrono::milliseconds(10);
      expect(server.start(cfg));

      glz::http_client client{};
      constexpr int N = 20;
      client.set_pool_max_connections_per_host(N + 5);

      std::vector<std::future<std::expected<response, std::error_code>>> futs;
      futs.reserve(N);
      for (int i = 0; i < N; ++i) {
         futs.push_back(client.get_async(server.base_url() + "/hello"));
      }

      int success = 0;
      for (auto& f : futs) {
         auto r = f.get();
         if (r && r->status_code == 200) ++success;
      }
      expect(success == N) << "all " << N << " concurrent requests should succeed (got " << success << ")\n";
      expect(server.request_count() == N);

      const int accepts_after_burst = server.accept_count();
      expect(accepts_after_burst <= N);

      // Concurrent follow-up round: must be served entirely by pooled sockets.
      futs.clear();
      for (int i = 0; i < N; ++i) {
         futs.push_back(client.get_async(server.base_url() + "/hello"));
      }
      int success2 = 0;
      for (auto& f : futs) {
         auto r = f.get();
         if (r && r->status_code == 200) ++success2;
      }
      expect(success2 == N);
      expect(server.accept_count() == accepts_after_burst) << "concurrent follow-up round should add zero accepts, got "
                                                           << (server.accept_count() - accepts_after_burst) << "\n";
   };

   "pool_cap_drops_excess_on_return"_test = [] {
      // With a tight per-host cap, only `cap` sockets survive the return, so a
      // follow-up concurrent burst must open at least `N - cap` fresh connections.
      // This grounds the public-API note about the perf cost of exceeding the cap.
      control_server server;
      control_server::config cfg;
      cfg.response_delay = std::chrono::milliseconds(10);
      expect(server.start(cfg));

      glz::http_client client{};
      constexpr int cap = 2;
      constexpr int N = 5;
      client.set_pool_max_connections_per_host(cap);

      auto burst = [&]() {
         std::vector<std::future<std::expected<response, std::error_code>>> futs;
         futs.reserve(N);
         for (int i = 0; i < N; ++i) futs.push_back(client.get_async(server.base_url() + "/hello"));
         for (auto& f : futs) {
            auto r = f.get();
            expect(r.has_value());
         }
      };

      burst();
      const int after_first = server.accept_count();
      expect(after_first >= N) << "first burst must accept at least N=" << N << " connections\n";

      burst();
      const int after_second = server.accept_count();
      const int new_accepts = after_second - after_first;
      // First burst returned N sockets but pool retained only `cap`. Second burst
      // therefore opens at least N - cap new connections.
      expect(new_accepts >= N - cap) << "expected at least " << (N - cap)
                                     << " new accepts in the second burst (cap=" << cap << ", N=" << N << "); got "
                                     << new_accepts << "\n";
   };

   "async_retry_path_forced"_test = [] {
      // Deterministically exercise the async transparent-retry path by:
      //   - Server silently closes after each response (no Connection: close header,
      //     so the client pools the now-dead socket).
      //   - Client peek check is disabled, so acquire() will hand back the dead socket.
      //   - Client idle timeout is set huge, so timestamp eviction will not fire.
      // The first GET succeeds (fresh socket). The second GET MUST exercise the retry
      // path: write succeeds locally, read fails with EOF/RST, retry on a fresh socket
      // succeeds. We verify behavior end-to-end.
      control_server server;
      control_server::config cfg;
      cfg.silently_close_after_response = true;
      expect(server.start(cfg));

      glz::http_client client{};
      client.set_pool_active_liveness_check(false);
      client.set_pool_idle_timeout(std::chrono::hours(1));

      auto r1 = client.get_async(server.base_url() + "/x").get();
      expect(r1.has_value()) << "first GET should succeed\n";

      // Server has now closed; client has pooled the dead socket. Give the FIN time
      // to land at the client's TCP stack.
      std::this_thread::sleep_for(std::chrono::milliseconds(50));

      auto r2 = client.get_async(server.base_url() + "/x").get();
      expect(r2.has_value()) << "async GET should succeed via transparent retry\n";
      if (r2) expect(r2->status_code == 200);

      expect(server.request_count() == 2);
      // Two accepts: the fresh socket from request 1, and the retry from request 2.
      // The first attempt of request 2 used the dead pooled socket and failed before
      // reaching the server.
      expect(server.accept_count() == 2) << "expected 2 accepts (initial + retry), got " << server.accept_count()
                                         << "\n";
   };

   "post_not_retried_after_silent_close"_test = [] {
      // The retry invariant is: only idempotent methods are auto-retried. POST is not.
      // Setup: server silently closes after each response. Peek disabled, idle timeout
      // huge, so the second POST gets the dead socket. Write locally succeeds, read
      // fails. Because POST is not idempotent, the failure is surfaced to the caller.
      control_server server;
      control_server::config cfg;
      cfg.silently_close_after_response = true;
      expect(server.start(cfg));

      glz::http_client client{};
      client.set_pool_active_liveness_check(false);
      client.set_pool_idle_timeout(std::chrono::hours(1));

      auto r1 = client.post(server.base_url() + "/echo", "first");
      expect(r1.has_value()) << "first POST should succeed\n";

      std::this_thread::sleep_for(std::chrono::milliseconds(50));

      auto r2 = client.post(server.base_url() + "/echo", "second");
      expect(!r2.has_value()) << "second POST must surface error, not auto-retry\n";

      // Exactly one POST landed at the server; the client did not silently re-send.
      expect(server.request_count() == 1)
         << "POST must not be retried; got " << server.request_count() << " server-side requests\n";
   };

   "async_stale_get_retries_transparently"_test = [] {
      // End-to-end version of the GET stale scenario: peek + retry combined. With
      // peek enabled, this most often resolves at acquire(). Either path must yield
      // success.
      control_server server;
      control_server::config cfg;
      cfg.idle_timeout = std::chrono::milliseconds(100);
      expect(server.start(cfg));

      glz::http_client client{};
      auto r1 = client.get(server.base_url() + "/hello");
      expect(r1.has_value());

      std::this_thread::sleep_for(std::chrono::milliseconds(250));

      auto fut = client.get_async(server.base_url() + "/hello");
      auto r2 = fut.get();
      expect(r2.has_value()) << "async GET after server idle close should succeed\n";
      if (r2) expect(r2->status_code == 200);
   };

   "fresh_socket_dropped_then_retried_get"_test = [] {
      // The production case the response_started gate exists to cover: the first
      // connection is accepted by the server but immediately closed before any reply.
      // With from_pool as the gate (the prior implementation) this would NOT retry,
      // because the failed socket was a fresh one. With response_started=false on the
      // first attempt, the retry fires and the second connection succeeds.
      control_server server;
      control_server::config cfg;
      cfg.drop_first_n_connections = 1;
      expect(server.start(cfg));

      glz::http_client client{};
      auto r = client.get(server.base_url() + "/x");
      expect(r.has_value()) << "GET should succeed via transparent retry on listener-drop\n";
      if (r) expect(r->status_code == 200);

      expect(server.accept_count() == 2) << "expected exactly 2 accepts (first dropped, second served), got "
                                         << server.accept_count() << "\n";
      expect(server.request_count() == 1) << "exactly one request should reach the server\n";
   };

   "fresh_socket_dropped_post_not_retried"_test = [] {
      // Same scenario as above with POST. Non-idempotent: must NOT retry, even though
      // the client also has not received any response bytes. The user must see the
      // failure so they can decide what to do (the request may or may not have been
      // processed by the server in the general case, even though here it clearly was
      // not).
      control_server server;
      control_server::config cfg;
      cfg.drop_first_n_connections = 1;
      expect(server.start(cfg));

      glz::http_client client{};
      auto r = client.post(server.base_url() + "/x", "payload");
      expect(!r.has_value()) << "POST must surface the error, not auto-retry\n";

      expect(server.accept_count() == 1) << "no retry: only the dropped accept happened\n";
      expect(server.request_count() == 0);
   };

   "fresh_socket_dropped_async_get_retried"_test = [] {
      // Async path of the listener-drop scenario. The state_ptr threaded through the
      // chain must record response_started=false at the failure point, allowing the
      // retry handler to dispatch a fresh attempt.
      control_server server;
      control_server::config cfg;
      cfg.drop_first_n_connections = 1;
      expect(server.start(cfg));

      glz::http_client client{};
      auto r = client.get_async(server.base_url() + "/x").get();
      expect(r.has_value()) << "async GET should succeed via transparent retry\n";
      if (r) expect(r->status_code == 200);

      expect(server.accept_count() == 2);
   };

   "clear_connection_pool_drops_entries"_test = [] {
      // Calling clear_connection_pool() must close all pooled entries so that
      // the next request opens a fresh connection.
      control_server server;
      expect(server.start());

      glz::http_client client{};
      auto r1 = client.get(server.base_url() + "/hello");
      expect(r1.has_value());

      client.clear_connection_pool();

      auto r2 = client.get(server.base_url() + "/hello");
      expect(r2.has_value());

      expect(server.accept_count() == 2) << "clear_connection_pool should have forced a fresh connection\n";
   };
};

int main()
{
   std::printf("Running HTTP Client Pool Tests...\n");
   std::printf("=================================\n");
   return 0;
}
