// Minimal diagnostic test for MinGW + SSL heap corruption (issue #2411)
//
// Isolates each layer to find where heap corruption originates:
//   Phase 1-3: Basic lifecycle (OpenSSL, ASIO SSL, http_client)
//   Phase 4-7: HTTP operations (single, repeated, connection reuse, concurrent)
//   Phase 8-11: Stress tests targeting race conditions in create/destroy
//               with active requests, rapid cycling, and concurrent clients

#ifndef GLZ_ENABLE_SSL
#define GLZ_ENABLE_SSL
#endif

#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

#include <openssl/err.h>
#include <openssl/ssl.h>

#include "glaze/net/http_client.hpp"
#include "glaze/net/http_server.hpp"
#include "ut/ut.hpp"

#ifdef DELETE
#undef DELETE
#endif

using namespace ut;

// Simple server for HTTP tests
class diag_server
{
  public:
   bool start()
   {
      server_.get("/ping", [](const glz::request&, glz::response& res) { res.status(200).body("pong"); });

      // Slow endpoint to keep requests in-flight during destruction
      server_.get("/slow", [](const glz::request&, glz::response& res) {
         std::this_thread::sleep_for(std::chrono::milliseconds(200));
         res.status(200).body("slow");
      });

      // Streaming endpoint
      server_.stream_get("/stream", [](glz::request&, glz::streaming_response& res) {
         res.start_stream(200, {{"Content-Type", "text/plain"}});
         res.send("chunk1;");
         res.send("chunk2;");
         res.send("chunk3;");
         res.close();
      });

      for (uint16_t p = 19200; p < 19300; ++p) {
         try {
            server_.bind("127.0.0.1", p);
            port_ = p;
            break;
         }
         catch (...) {
            continue;
         }
      }
      if (port_ == 0) return false;

      thread_ = std::thread([this]() {
         try {
            server_.start(1);
         }
         catch (...) {
         }
      });

      // Wait for server to accept connections
      for (int i = 0; i < 50; ++i) {
         std::this_thread::sleep_for(std::chrono::milliseconds(100));
         try {
            asio::io_context io;
            asio::ip::tcp::socket sock(io);
            sock.connect({asio::ip::make_address("127.0.0.1"), port_});
            sock.close();
            return true;
         }
         catch (...) {
         }
      }
      return false;
   }

   void stop()
   {
      server_.stop();
      if (thread_.joinable()) thread_.join();
   }

   ~diag_server() { stop(); }

   std::string url() const { return "http://127.0.0.1:" + std::to_string(port_); }

  private:
   glz::http_server<> server_;
   std::thread thread_;
   uint16_t port_{0};
};

suite mingw_ssl_diag = [] {
   // Phase 1: Raw OpenSSL - does SSL_CTX create/destroy corrupt the heap?
   "phase1_raw_openssl_ctx"_test = [] {
      std::cout << "[phase1] Creating raw OpenSSL SSL_CTX..." << std::endl;

      for (int i = 0; i < 5; ++i) {
         SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
         expect(ctx != nullptr) << "SSL_CTX_new failed on iteration " << i;
         if (ctx) {
            SSL_CTX_free(ctx);
         }
      }

      std::cout << "[phase1] OK - raw OpenSSL context create/destroy" << std::endl;
   };

   // Phase 2: ASIO SSL context - does asio::ssl::context corrupt the heap?
   "phase2_asio_ssl_ctx"_test = [] {
      std::cout << "[phase2] Creating asio::ssl::context..." << std::endl;

      for (int i = 0; i < 5; ++i) {
         asio::ssl::context ctx(asio::ssl::context::tls_client);
         asio::error_code ec;
         ctx.set_default_verify_paths(ec);
         ctx.set_verify_mode(asio::ssl::verify_peer);
      }

      std::cout << "[phase2] OK - asio SSL context create/destroy" << std::endl;
   };

   // Phase 3: http_client lifecycle without any requests
   "phase3_http_client_lifecycle_no_request"_test = [] {
      std::cout << "[phase3] Creating/destroying http_client (no requests)..." << std::endl;

      for (int i = 0; i < 5; ++i) {
         glz::http_client client;
      }

      std::cout << "[phase3] OK - http_client lifecycle without requests" << std::endl;
   };

   // Phase 4: http_client with a single plain HTTP GET
   "phase4_http_client_single_get"_test = [] {
      std::cout << "[phase4] http_client single HTTP GET..." << std::endl;

      diag_server server;
      expect(server.start()) << "Server failed to start";

      {
         glz::http_client client;
         auto result = client.get(server.url() + "/ping");
         expect(result.has_value()) << "GET /ping failed";
         if (result) {
            expect(result->status_code == 200) << "Expected 200, got " << result->status_code;
         }
      }

      server.stop();
      std::cout << "[phase4] OK - single HTTP GET" << std::endl;
   };

   // Phase 5: Multiple http_client instances doing HTTP GETs
   "phase5_http_client_repeated"_test = [] {
      std::cout << "[phase5] Multiple http_client instances with HTTP GETs..." << std::endl;

      diag_server server;
      expect(server.start()) << "Server failed to start";

      for (int i = 0; i < 10; ++i) {
         glz::http_client client;
         auto result = client.get(server.url() + "/ping");
         expect(result.has_value()) << "GET failed on iteration " << i;
      }

      server.stop();
      std::cout << "[phase5] OK - repeated http_client lifecycle" << std::endl;
   };

   // Phase 6: Single http_client, multiple requests (connection reuse)
   "phase6_http_client_multiple_requests"_test = [] {
      std::cout << "[phase6] Single http_client, multiple requests..." << std::endl;

      diag_server server;
      expect(server.start()) << "Server failed to start";

      {
         glz::http_client client;
         for (int i = 0; i < 20; ++i) {
            auto result = client.get(server.url() + "/ping");
            expect(result.has_value()) << "GET failed on iteration " << i;
         }
      }

      server.stop();
      std::cout << "[phase6] OK - multiple requests on single client" << std::endl;
   };

   // Phase 7: Concurrent async requests
   "phase7_http_client_concurrent"_test = [] {
      std::cout << "[phase7] Concurrent async requests..." << std::endl;

      diag_server server;
      expect(server.start()) << "Server failed to start";

      {
         glz::http_client client;
         std::atomic<int> completed{0};
         std::atomic<int> errors{0};
         constexpr int N = 20;

         for (int i = 0; i < N; ++i) {
            client.get_async(server.url() + "/ping", {},
                             [&](glz::expected<glz::response, std::error_code> result) {
                                if (result.has_value() && result->status_code == 200) {
                                   completed++;
                                }
                                else {
                                   errors++;
                                }
                             });
         }

         for (int i = 0; i < 100 && completed + errors < N; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
         }

         expect(completed + errors == N) << "Not all requests completed: " << completed.load() << " ok, "
                                         << errors.load() << " errors";
         expect(errors == 0) << "Some requests failed";
      }

      server.stop();
      std::cout << "[phase7] OK - concurrent requests" << std::endl;
   };

   // Phase 8: Destroy client while async requests are in-flight
   // This is the most likely race condition: worker threads processing
   // completions while the destructor tears down the io_context.
   "phase8_destroy_with_inflight_requests"_test = [] {
      std::cout << "[phase8] Destroying client with in-flight requests..." << std::endl;

      diag_server server;
      expect(server.start()) << "Server failed to start";

      for (int round = 0; round < 20; ++round) {
         glz::http_client client;

         // Fire async requests against the slow endpoint
         for (int i = 0; i < 5; ++i) {
            client.get_async(server.url() + "/slow", {}, [](auto) {});
         }

         // Don't wait — destroy immediately while requests are pending.
         // The destructor calls stop_workers() which stops io_context and
         // joins threads. If there's a race, this is where it crashes.
      }

      server.stop();
      std::cout << "[phase8] OK - destroy with in-flight requests" << std::endl;
   };

   // Phase 9: Rapid create/destroy cycling with immediate requests
   // Stresses the worker thread start/stop path.
   "phase9_rapid_lifecycle_with_requests"_test = [] {
      std::cout << "[phase9] Rapid client lifecycle cycling..." << std::endl;

      diag_server server;
      expect(server.start()) << "Server failed to start";

      for (int round = 0; round < 50; ++round) {
         glz::http_client client;
         // Fire one async request then immediately destroy
         client.get_async(server.url() + "/ping", {}, [](auto) {});
      }

      server.stop();
      std::cout << "[phase9] OK - rapid lifecycle cycling" << std::endl;
   };

   // Phase 10: Multiple clients sharing work concurrently
   // Each client has its own io_context and worker threads.
   "phase10_concurrent_clients"_test = [] {
      std::cout << "[phase10] Multiple concurrent clients..." << std::endl;

      diag_server server;
      expect(server.start()) << "Server failed to start";

      {
         constexpr int NUM_CLIENTS = 5;
         constexpr int REQUESTS_PER_CLIENT = 10;
         std::vector<std::thread> threads;
         std::atomic<int> total_success{0};
         std::atomic<int> total_errors{0};

         for (int c = 0; c < NUM_CLIENTS; ++c) {
            threads.emplace_back([&]() {
               glz::http_client client;
               for (int i = 0; i < REQUESTS_PER_CLIENT; ++i) {
                  auto result = client.get(server.url() + "/ping");
                  if (result.has_value() && result->status_code == 200) {
                     total_success++;
                  }
                  else {
                     total_errors++;
                  }
               }
            });
         }

         for (auto& t : threads) {
            t.join();
         }

         expect(total_errors == 0) << "Concurrent client errors: " << total_errors.load();
         expect(total_success == NUM_CLIENTS * REQUESTS_PER_CLIENT)
            << "Expected " << NUM_CLIENTS * REQUESTS_PER_CLIENT << " successes, got " << total_success.load();
      }

      server.stop();
      std::cout << "[phase10] OK - concurrent clients" << std::endl;
   };

   // Phase 11: Streaming request — the code path in http_client_test
   // that we suspect triggers the heap corruption.
   "phase11_streaming_request"_test = [] {
      std::cout << "[phase11] Streaming request..." << std::endl;

      diag_server server;
      expect(server.start()) << "Server failed to start";

      {
         glz::http_client client;

         std::string received;
         std::mutex mtx;
         std::promise<void> done;
         auto future = done.get_future();

         auto conn = client.stream_request_v2({
            .method = "GET",
            .url = server.url() + "/stream",
            .on_connect = [](const glz::response&) {},
            .on_disconnect = [&]() { done.set_value(); },
            .on_data = [&](std::string_view data) {
               std::lock_guard lock(mtx);
               received.append(data);
            },
            .on_error = [](std::error_code) {},
         });

         auto status = future.wait_for(std::chrono::seconds(5));
         expect(status == std::future_status::ready) << "Stream did not complete";
      }

      server.stop();
      std::cout << "[phase11] OK - streaming request" << std::endl;
   };

   // Phase 12: Repeated streaming with client destruction
   "phase12_streaming_destroy_cycle"_test = [] {
      std::cout << "[phase12] Streaming with client destruction cycling..." << std::endl;

      diag_server server;
      expect(server.start()) << "Server failed to start";

      for (int round = 0; round < 20; ++round) {
         glz::http_client client;

         std::promise<void> done;
         auto future = done.get_future();

         auto conn = client.stream_request_v2({
            .method = "GET",
            .url = server.url() + "/stream",
            .on_connect = [](const glz::response&) {},
            .on_disconnect = [&]() { done.set_value(); },
            .on_data = [](std::string_view) {},
            .on_error = [](std::error_code) {},
         });

         auto status = future.wait_for(std::chrono::seconds(5));
         (void)status;
         // Destroy client — may have pending handlers
      }

      server.stop();
      std::cout << "[phase12] OK - streaming destroy cycle" << std::endl;
   };

   // Phase 13: Mixed sync + async with destruction under load
   "phase13_stress_mixed"_test = [] {
      std::cout << "[phase13] Mixed stress test..." << std::endl;

      diag_server server;
      expect(server.start()) << "Server failed to start";

      for (int round = 0; round < 10; ++round) {
         glz::http_client client;

         auto result = client.get(server.url() + "/ping");
         (void)result;

         for (int i = 0; i < 3; ++i) {
            client.get_async(server.url() + "/ping", {}, [](auto) {});
            client.get_async(server.url() + "/slow", {}, [](auto) {});
         }

         if (round % 3 == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
         }
      }

      server.stop();
      std::cout << "[phase13] OK - mixed stress test" << std::endl;
   };
};

int main()
{
   std::cout << "MinGW + SSL Diagnostic Test" << std::endl;
   std::cout << "===========================" << std::endl;
   std::cout << "OpenSSL version: " << OpenSSL_version(OPENSSL_VERSION) << std::endl;
#ifdef __VERSION__
   std::cout << "Compiler: " << __VERSION__ << std::endl;
#elif defined(_MSC_VER)
   std::cout << "Compiler: MSVC " << _MSC_VER << std::endl;
#endif
   std::cout << std::endl;
   return 0;
}
