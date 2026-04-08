// Minimal diagnostic test for MinGW + SSL heap corruption (issue #2411)
//
// Isolates each layer to find where heap corruption originates:
//   1. Raw OpenSSL context create/destroy
//   2. ASIO SSL context create/destroy
//   3. http_client create/destroy (no requests)
//   4. http_client with a single HTTP GET (non-SSL, but SSL compiled in)
//   5. Multiple http_client lifecycles

#ifndef GLZ_ENABLE_SSL
#define GLZ_ENABLE_SSL
#endif

#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

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
         // Don't fail on verify path errors - some CI environments lack system CA certs
         ctx.set_verify_mode(asio::ssl::verify_peer);
      }

      std::cout << "[phase2] OK - asio SSL context create/destroy" << std::endl;
   };

   // Phase 3: http_client lifecycle without any requests
   "phase3_http_client_lifecycle_no_request"_test = [] {
      std::cout << "[phase3] Creating/destroying http_client (no requests)..." << std::endl;

      for (int i = 0; i < 5; ++i) {
         glz::http_client client;
         // Just create and destroy - no requests
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

   // Phase 7: Concurrent requests
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

         // Wait for all to complete
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
};

int main()
{
   std::cout << "MinGW + SSL Diagnostic Test" << std::endl;
   std::cout << "===========================" << std::endl;
   std::cout << "OpenSSL version: " << OpenSSL_version(OPENSSL_VERSION) << std::endl;
   std::cout << "Compiler: " << __VERSION__ << std::endl;
   std::cout << std::endl;
   return 0;
}
