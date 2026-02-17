// HTTP Client TLS/HTTPS Test for Glaze Library
// Tests HTTPS requests and SSL configuration

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <future>
#include <iostream>
#include <memory>
#include <optional>
#include <thread>

#include "glaze/glaze.hpp"
#include "glaze/net/http_client.hpp"
#include "glaze/net/http_server.hpp"
#include "ut/ut.hpp"

// OpenSSL includes for certificate generation
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#ifdef DELETE
#undef DELETE
#endif

#include "glaze/ext/glaze_asio.hpp"

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
   struct env_var_guard
   {
      std::string name;
      std::optional<std::string> original;

      explicit env_var_guard(std::string var_name) : name(std::move(var_name))
      {
         if (const char* value = std::getenv(name.c_str()); value) {
            original = value;
         }
      }

      ~env_var_guard() { restore(); }

      void set(std::string_view value)
      {
#ifdef _WIN32
         _putenv_s(name.c_str(), std::string(value).c_str());
#else
         setenv(name.c_str(), std::string(value).c_str(), 1);
#endif
      }

      void unset()
      {
#ifdef _WIN32
         _putenv_s(name.c_str(), "");
#else
         unsetenv(name.c_str());
#endif
      }

      void restore()
      {
         if (original) {
            set(*original);
         }
         else {
            unset();
         }
      }
   };
}

// Certificate generation class
class CertificateGenerator
{
  private:
   static void cleanup_openssl_errors()
   {
      while (ERR_get_error() != 0) {
      }
   }

   static EVP_PKEY* generate_rsa_key(int bits = 2048)
   {
      EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
      if (!ctx) return nullptr;

      if (EVP_PKEY_keygen_init(ctx) <= 0) {
         EVP_PKEY_CTX_free(ctx);
         return nullptr;
      }

      if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, bits) <= 0) {
         EVP_PKEY_CTX_free(ctx);
         return nullptr;
      }

      EVP_PKEY* pkey = nullptr;
      if (EVP_PKEY_keygen(ctx, &pkey) <= 0) {
         EVP_PKEY_CTX_free(ctx);
         return nullptr;
      }

      EVP_PKEY_CTX_free(ctx);
      return pkey;
   }

   static X509* create_certificate(EVP_PKEY* pkey, const std::string& subject, int days = 365)
   {
      X509* x509 = X509_new();
      if (!x509) return nullptr;

      X509_set_version(x509, 2);
      ASN1_INTEGER_set(X509_get_serialNumber(x509), 1);
      X509_gmtime_adj(X509_get_notBefore(x509), 0);
      X509_gmtime_adj(X509_get_notAfter(x509), static_cast<long>(days) * 24 * 60 * 60);
      X509_set_pubkey(x509, pkey);

      X509_NAME* name = X509_get_subject_name(x509);
      X509_NAME_add_entry_by_txt(name, "C", MBSTRING_ASC, reinterpret_cast<const unsigned char*>("US"), -1, -1, 0);
      X509_NAME_add_entry_by_txt(name, "ST", MBSTRING_ASC, reinterpret_cast<const unsigned char*>("Test"), -1, -1, 0);
      X509_NAME_add_entry_by_txt(name, "L", MBSTRING_ASC, reinterpret_cast<const unsigned char*>("Test"), -1, -1, 0);
      X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC, reinterpret_cast<const unsigned char*>("Test"), -1, -1, 0);
      X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, reinterpret_cast<const unsigned char*>(subject.c_str()), -1,
                                 -1, 0);

      X509_set_issuer_name(x509, name);

      if (subject == "localhost") {
         X509V3_CTX ctx;
         X509V3_set_ctx_nodb(&ctx);
         X509V3_set_ctx(&ctx, x509, x509, nullptr, nullptr, 0);

         X509_EXTENSION* ext =
            X509V3_EXT_conf_nid(nullptr, &ctx, NID_subject_alt_name,
                                const_cast<char*>("DNS:localhost,DNS:*.localhost"));
         if (ext) {
            X509_add_ext(x509, ext, -1);
            X509_EXTENSION_free(ext);
         }

         ext = X509V3_EXT_conf_nid(nullptr, &ctx, NID_basic_constraints, const_cast<char*>("CA:FALSE"));
         if (ext) {
            X509_add_ext(x509, ext, -1);
            X509_EXTENSION_free(ext);
         }

         ext = X509V3_EXT_conf_nid(nullptr, &ctx, NID_key_usage, const_cast<char*>("keyEncipherment,digitalSignature"));
         if (ext) {
            X509_add_ext(x509, ext, -1);
            X509_EXTENSION_free(ext);
         }
      }

      if (!X509_sign(x509, pkey, EVP_sha256())) {
         X509_free(x509);
         return nullptr;
      }

      return x509;
   }

   static bool write_pem_file(const std::string& filename, std::function<int(BIO*)> write_func)
   {
      BIO* bio = BIO_new_file(filename.c_str(), "w");
      if (!bio) return false;

      int result = write_func(bio);
      BIO_free(bio);
      return result > 0;
   }

  public:
   static bool generate_certificates(const std::string& prefix = "client_test")
   {
      cleanup_openssl_errors();

      std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> pkey(generate_rsa_key(2048), EVP_PKEY_free);
      if (!pkey) {
         return false;
      }

      std::unique_ptr<X509, decltype(&X509_free)> cert(create_certificate(pkey.get(), "localhost", 365), X509_free);
      if (!cert) {
         return false;
      }

      std::string cert_file = prefix + "_cert.pem";
      std::string key_file = prefix + "_key.pem";

      bool cert_written = write_pem_file(cert_file, [&](BIO* bio) { return PEM_write_bio_X509(bio, cert.get()); });

      bool key_written = write_pem_file(
         key_file, [&](BIO* bio) { return PEM_write_bio_PrivateKey(bio, pkey.get(), nullptr, nullptr, 0, nullptr, nullptr); });

#ifndef _WIN32
      chmod(key_file.c_str(), 0600);
      chmod(cert_file.c_str(), 0644);
#endif

      return cert_written && key_written;
   }

   static bool certificates_exist(const std::string& prefix = "client_test")
   {
      std::ifstream cert(prefix + "_cert.pem");
      std::ifstream key(prefix + "_key.pem");
      return cert.is_open() && key.is_open();
   }
};

// Test data structures
struct TestData
{
   std::string message;
   int value;
};

struct TestResponse
{
   std::string result;
   bool success;
};

// Helper to suppress expected errors
bool should_suppress_error(const std::error_code& ec)
{
   if (ec == asio::error::eof) return true;
   if (ec == asio::error::connection_reset) return true;
   if (ec == asio::error::operation_aborted) return true;
   if (ec == asio::error::bad_descriptor) return true;
   if (ec == asio::error::broken_pipe) return true;
   return false;
}

// Test HTTPS server - constructed during global initialization
class TestHTTPSServer
{
   glz::https_server server_;
   std::future<void> server_future_;
   std::atomic<bool> running_{false};
   uint16_t port_;
   bool initialized_{false};

  public:
   TestHTTPSServer(uint16_t port = 9443) : port_(port)
   {
      // Always regenerate certificates so test expectations stay deterministic.
      if (!CertificateGenerator::generate_certificates()) {
         return;
      }

      server_.on_error([](std::error_code ec, std::source_location) {
         if (!should_suppress_error(ec)) {
            // Silent in tests
         }
      });

      setup_routes();

      try {
         server_.load_certificate("client_test_cert.pem", "client_test_key.pem");
         server_.set_ssl_verify_mode(0);
         server_.enable_cors();
         server_.bind("127.0.0.1", port_);

         running_ = true;
         server_future_ = std::async(std::launch::async, [this]() { server_.start(2); });

         std::this_thread::sleep_for(std::chrono::milliseconds(300));
         initialized_ = true;
      }
      catch (...) {
         // Initialization failed
      }
   }

   ~TestHTTPSServer()
   {
      if (running_) {
         running_ = false;
         server_.stop();

         if (server_future_.valid()) {
            server_future_.wait_for(std::chrono::seconds(2));
         }
      }
   }

   bool is_initialized() const { return initialized_; }
   uint16_t port() const { return port_; }
   std::string base_url() const { return "https://127.0.0.1:" + std::to_string(port_); }

  private:
   void setup_routes()
   {
      server_.get("/health", [](const glz::request&, glz::response& res) { res.status(200).body("OK"); });

      server_.post("/echo", [](const glz::request& req, glz::response& res) {
         res.status(200).header("Content-Type", "text/plain").body(req.body);
      });

      server_.put("/echo", [](const glz::request& req, glz::response& res) {
         res.status(200).header("Content-Type", "text/plain").body(req.body);
      });

      server_.get("/json", [](const glz::request&, glz::response& res) {
         TestResponse resp{"success", true};
         res.status(200).json(resp);
      });

      server_.post("/json", [](const glz::request& req, glz::response& res) {
         TestData data;
         auto err = glz::read_json(data, req.body);
         if (err) {
            res.status(400).body("Invalid JSON");
            return;
         }
         TestResponse resp{"Received: " + data.message, true};
         res.status(200).json(resp);
      });

      server_.get("/large", [](const glz::request&, glz::response& res) {
         std::string large_body(50000, 'X');
         res.status(200).body(large_body);
      });

      server_.get("/headers", [](const glz::request& req, glz::response& res) {
         std::string body;
         for (const auto& [key, value] : req.headers) {
            body += key + ": " + value + "\n";
         }
         res.status(200).body(body);
      });

      server_.get("/status/{code}", [](const glz::request& req, glz::response& res) {
         auto it = req.params.find("code");
         if (it != req.params.end()) {
            int code = std::stoi(it->second);
            res.status(code).body("Status " + std::to_string(code));
         }
         else {
            res.status(400).body("Missing code");
         }
      });
   }
};

// Global test server - initialized before tests run
static TestHTTPSServer g_server{9443};

// Test suite
suite https_client_tests = [] {
   "https_get_request"_test = [] {
      if (!g_server.is_initialized()) {
         expect(false) << "Server not initialized";
         return;
      }
      glz::http_client client;
      client.set_ssl_verify_mode(asio::ssl::verify_none);

      auto result = client.get(g_server.base_url() + "/health");
      expect(result.has_value()) << "HTTPS GET should succeed";
      if (result.has_value()) {
         expect(result->status_code == 200);
         expect(result->response_body == "OK");
      }
   };

   "https_post_request"_test = [] {
      if (!g_server.is_initialized()) return;
      glz::http_client client;
      client.set_ssl_verify_mode(asio::ssl::verify_none);

      auto result = client.post(g_server.base_url() + "/echo", "test body");
      expect(result.has_value()) << "HTTPS POST should succeed";
      if (result.has_value()) {
         expect(result->status_code == 200);
         expect(result->response_body == "test body");
      }
   };

   "https_put_request"_test = [] {
      if (!g_server.is_initialized()) return;
      glz::http_client client;
      client.set_ssl_verify_mode(asio::ssl::verify_none);

      auto result = client.put(g_server.base_url() + "/echo", "put body");
      expect(result.has_value()) << "HTTPS PUT should succeed";
      if (result.has_value()) {
         expect(result->status_code == 200);
      }
   };

   "https_post_json"_test = [] {
      if (!g_server.is_initialized()) return;
      glz::http_client client;
      client.set_ssl_verify_mode(asio::ssl::verify_none);

      TestData data{"Hello TLS", 42};
      auto result = client.post_json(g_server.base_url() + "/json", data);
      expect(result.has_value()) << "HTTPS POST JSON should succeed";
      if (result.has_value()) {
         expect(result->status_code == 200);
         TestResponse resp;
         auto err = glz::read_json(resp, result->response_body);
         expect(!err) << "Should parse response JSON";
         expect(resp.success == true);
      }
   };

   "https_large_response"_test = [] {
      if (!g_server.is_initialized()) return;
      glz::http_client client;
      client.set_ssl_verify_mode(asio::ssl::verify_none);

      auto result = client.get(g_server.base_url() + "/large");
      expect(result.has_value()) << "Should handle large HTTPS response";
      if (result.has_value()) {
         expect(result->status_code == 200);
         expect(result->response_body.size() == size_t(50000));
      }
   };

   "https_custom_headers"_test = [] {
      if (!g_server.is_initialized()) return;
      glz::http_client client;
      client.set_ssl_verify_mode(asio::ssl::verify_none);

      std::unordered_map<std::string, std::string> headers;
      headers["X-Custom-Header"] = "CustomValue";
      headers["Authorization"] = "Bearer test-token";

      auto result = client.get(g_server.base_url() + "/headers", headers);
      expect(result.has_value()) << "HTTPS with custom headers should succeed";
      if (result.has_value()) {
         expect(result->status_code == 200);
         expect(result->response_body.find("x-custom-header") != std::string::npos);
      }
   };

   "ssl_context_access"_test = [] {
      glz::http_client client;
      auto& ctx = client.ssl_context_unsafe();
      (void)ctx;
      expect(true) << "SSL context should be accessible";
   };

   "ssl_verify_mode_none"_test = [] {
      if (!g_server.is_initialized()) return;
      glz::http_client client;
      client.set_ssl_verify_mode(asio::ssl::verify_none);

      auto result = client.get(g_server.base_url() + "/health");
      expect(result.has_value()) << "verify_none should allow self-signed certs";
   };

   "https_connection_reuse"_test = [] {
      if (!g_server.is_initialized()) return;
      glz::http_client client;
      client.set_ssl_verify_mode(asio::ssl::verify_none);

      for (int i = 0; i < 5; ++i) {
         auto result = client.get(g_server.base_url() + "/health");
         expect(result.has_value()) << "Request " << i << " should succeed";
         if (result.has_value()) {
            expect(result->status_code == 200);
         }
      }
   };

   "https_status_codes"_test = [] {
      if (!g_server.is_initialized()) return;
      glz::http_client client;
      client.set_ssl_verify_mode(asio::ssl::verify_none);

      auto result_404 = client.get(g_server.base_url() + "/status/404");
      expect(result_404.has_value()) << "Should receive 404 response";
      if (result_404.has_value()) {
         expect(result_404->status_code == 404) << "Status code should be 404";
      }
   };

   "https_invalid_url"_test = [] {
      glz::http_client client;
      client.set_ssl_verify_mode(asio::ssl::verify_none);

      auto result = client.get("https://");
      expect(!result.has_value()) << "Invalid URL should fail";
   };

   "https_hostname_mismatch_fails_with_default_verify"_test = [] {
      if (!g_server.is_initialized()) return;

      glz::http_client client;
      client.configure_ssl_context([](asio::ssl::context& ctx) { ctx.load_verify_file("client_test_cert.pem"); });

      // Certificate SAN is DNS-only (localhost), so this IP host must fail hostname verification.
      auto result = client.get(g_server.base_url() + "/health");
      expect(!result.has_value()) << "Hostname mismatch should fail with default verify mode";
   };

   "protocol_detection"_test = [] {
      if (!g_server.is_initialized()) return;
      glz::http_client client;
      client.set_ssl_verify_mode(asio::ssl::verify_none);

      auto https_result = client.get(g_server.base_url() + "/health");
      expect(https_result.has_value()) << "HTTPS URL should work";
   };

   // =========================================================================
   // SSL Error Category Tests
   // =========================================================================

   "ssl_error_category_name"_test = [] {
      auto ec = glz::make_error_code(glz::ssl_error::sni_hostname_failed);
      expect(std::string(ec.category().name()) == "glaze.ssl") << "Error category name should be 'glaze.ssl'";
   };

   "ssl_error_messages"_test = [] {
      // Test that all error codes have meaningful messages
      auto ec_sni = glz::make_error_code(glz::ssl_error::sni_hostname_failed);
      expect(ec_sni.message().find("SNI") != std::string::npos) << "SNI error should mention SNI";

      auto ec_not_supported = glz::make_error_code(glz::ssl_error::ssl_not_supported);
      expect(ec_not_supported.message().find("not supported") != std::string::npos)
         << "Not supported error should mention 'not supported'";
   };

   "ssl_error_default_conditions"_test = [] {
      // SNI failure should map to protocol_error
      auto ec_sni = glz::make_error_code(glz::ssl_error::sni_hostname_failed);
      expect(ec_sni.default_error_condition() == std::errc::protocol_error)
         << "SNI failure should map to protocol_error";

      // SSL not supported should map to protocol_not_supported
      auto ec_not_supported = glz::make_error_code(glz::ssl_error::ssl_not_supported);
      expect(ec_not_supported.default_error_condition() == std::errc::protocol_not_supported)
         << "SSL not supported should map to protocol_not_supported";
   };

   "ssl_error_bool_conversion"_test = [] {
      // Success should be falsy
      auto ec_success = glz::make_error_code(glz::ssl_error::success);
      expect(!ec_success) << "Success error code should be falsy";

      // Errors should be truthy
      auto ec_error = glz::make_error_code(glz::ssl_error::sni_hostname_failed);
      expect(static_cast<bool>(ec_error)) << "Error codes should be truthy";
   };

   // =========================================================================
   // SSL Shutdown Configuration Tests
   // =========================================================================

   "graceful_ssl_shutdown_default"_test = [] {
      glz::http_client client;
      // Default should be enabled (graceful shutdown)
      expect(client.graceful_ssl_shutdown() == true) << "Graceful SSL shutdown should be enabled by default";
   };

   "graceful_ssl_shutdown_disable"_test = [] {
      glz::http_client client;
      client.set_graceful_ssl_shutdown(false);
      expect(client.graceful_ssl_shutdown() == false) << "Graceful SSL shutdown should be disabled";
   };

   "graceful_ssl_shutdown_enable"_test = [] {
      glz::http_client client;
      client.set_graceful_ssl_shutdown(false);
      client.set_graceful_ssl_shutdown(true);
      expect(client.graceful_ssl_shutdown() == true) << "Graceful SSL shutdown should be re-enabled";
   };

   "graceful_ssl_shutdown_requests_work"_test = [] {
      if (!g_server.is_initialized()) return;

      // Test with graceful shutdown enabled (default)
      {
         glz::http_client client;
         client.set_ssl_verify_mode(asio::ssl::verify_none);
         expect(client.graceful_ssl_shutdown() == true);

         auto result = client.get(g_server.base_url() + "/health");
         expect(result.has_value()) << "Request should succeed with graceful shutdown enabled";
         if (result.has_value()) {
            expect(result->status_code == 200);
         }
      }

      // Test with graceful shutdown disabled
      {
         glz::http_client client;
         client.set_ssl_verify_mode(asio::ssl::verify_none);
         client.set_graceful_ssl_shutdown(false);

         auto result = client.get(g_server.base_url() + "/health");
         expect(result.has_value()) << "Request should succeed with graceful shutdown disabled";
         if (result.has_value()) {
            expect(result->status_code == 200);
         }
      }
   };

   // =========================================================================
   // Thread-Safe SSL Configuration Tests
   // =========================================================================

   "configure_ssl_context_callable"_test = [] {
      glz::http_client client;

      // Use configure_ssl_context to safely modify the context
      bool callback_executed = false;
      client.configure_ssl_context([&callback_executed](asio::ssl::context& ctx) {
         ctx.set_verify_mode(asio::ssl::verify_none);
         callback_executed = true;
      });

      expect(callback_executed) << "configure_ssl_context callback should be executed";
   };

   "configure_ssl_context_with_requests"_test = [] {
      if (!g_server.is_initialized()) return;

      glz::http_client client;

      // Configure context using thread-safe method
      client.configure_ssl_context(
         [](asio::ssl::context& ctx) { ctx.set_verify_mode(asio::ssl::verify_none); });

      // Make request after configuration
      auto result = client.get(g_server.base_url() + "/health");
      expect(result.has_value()) << "Request should succeed after configure_ssl_context";
      if (result.has_value()) {
         expect(result->status_code == 200);
      }
   };

   "configure_system_ca_certificates_allows_verified_https"_test = [] {
      if (!g_server.is_initialized()) return;

      glz::http_client client;
      auto configured = client.configure_system_ca_certificates("client_test_cert.pem");
      expect(configured.has_value()) << "Explicit CA bundle configuration should succeed";

      auto result = client.get("https://localhost:" + std::to_string(g_server.port()) + "/health");
      expect(result.has_value()) << "Verified HTTPS request should succeed with configured CA bundle";
      if (result.has_value()) {
         expect(result->status_code == 200);
      }
   };

   "configure_system_ca_certificates_fallback_order_prefers_explicit"_test = [] {
      bool loaded_explicit = false;
      bool loaded_env = false;
      bool used_default = false;

      auto configured = glz::detail::configure_ssl_ca_fallback(
         "explicit.pem", "env.pem", "env_dir",
         [&](std::string_view path) -> std::error_code {
            if (path == "explicit.pem") {
               loaded_explicit = true;
               return {};
            }
            loaded_env = true;
            return std::make_error_code(std::errc::no_such_file_or_directory);
         },
         [&](std::string_view) -> std::error_code {
            return std::make_error_code(std::errc::permission_denied);
         },
         [&]() -> std::error_code {
            used_default = true;
            return {};
         });

      expect(configured.has_value());
      if (configured.has_value()) {
         expect(*configured == glz::detail::ssl_ca_source::explicit_file);
      }
      expect(loaded_explicit == true);
      expect(loaded_env == false);
      expect(used_default == false);
   };

   "configure_system_ca_certificates_fallback_order_uses_env_then_default"_test = [] {
      int file_attempts = 0;
      bool dir_attempted = false;
      bool default_used = false;

      auto configured = glz::detail::configure_ssl_ca_fallback(
         "missing.pem", "env.pem", "env_dir",
         [&](std::string_view path) -> std::error_code {
            ++file_attempts;
            if (path == "env.pem") {
               return {};
            }
            return std::make_error_code(std::errc::no_such_file_or_directory);
         },
         [&](std::string_view) -> std::error_code {
            dir_attempted = true;
            return std::make_error_code(std::errc::permission_denied);
         },
         [&]() -> std::error_code {
            default_used = true;
            return {};
         });

      expect(configured.has_value());
      if (configured.has_value()) {
         expect(*configured == glz::detail::ssl_ca_source::env_ssl_cert_file);
      }
      expect(file_attempts == 2);
      expect(dir_attempted == false);
      expect(default_used == false);
   };

   "configure_system_ca_certificates_fallback_order_returns_error_when_all_fail"_test = [] {
      auto configured = glz::detail::configure_ssl_ca_fallback(
         "missing.pem", std::nullopt, std::nullopt,
         [&](std::string_view) -> std::error_code {
            return std::make_error_code(std::errc::no_such_file_or_directory);
         },
         [&](std::string_view) -> std::error_code { return std::make_error_code(std::errc::permission_denied); },
         [&]() -> std::error_code { return std::make_error_code(std::errc::io_error); });

      expect(!configured.has_value());
      if (!configured.has_value()) {
         expect(configured.error() == std::make_error_code(std::errc::no_such_file_or_directory));
      }
   };

   "configure_system_ca_certificates_env_fallback_integration"_test = [] {
      if (!g_server.is_initialized()) return;

      env_var_guard cert_file{"SSL_CERT_FILE"};
      env_var_guard cert_dir{"SSL_CERT_DIR"};
      cert_file.set("client_test_cert.pem");
      cert_dir.unset();

      glz::http_client client;
      auto configured = client.configure_system_ca_certificates("definitely_missing_bundle.pem");
      expect(configured.has_value()) << "Configuration should fall back to SSL_CERT_FILE";

      auto result = client.get("https://localhost:" + std::to_string(g_server.port()) + "/health");
      expect(result.has_value()) << "Verified HTTPS request should succeed via env fallback";
      if (result.has_value()) {
         expect(result->status_code == 200);
      }
   };

   "concurrent_requests_with_ssl"_test = [] {
      if (!g_server.is_initialized()) return;

      glz::http_client client;
      client.set_ssl_verify_mode(asio::ssl::verify_none);

      // Launch multiple concurrent requests to test thread safety
      constexpr int num_threads = 4;
      constexpr int requests_per_thread = 5;
      std::vector<std::future<int>> futures;

      for (int t = 0; t < num_threads; ++t) {
         futures.push_back(std::async(std::launch::async, [&client]() {
            int success_count = 0;
            for (int r = 0; r < requests_per_thread; ++r) {
               auto result = client.get(g_server.base_url() + "/health");
               if (result.has_value() && result->status_code == 200) {
                  ++success_count;
               }
            }
            return success_count;
         }));
      }

      int total_success = 0;
      for (auto& f : futures) {
         total_success += f.get();
      }

      expect(total_success == num_threads * requests_per_thread)
         << "All concurrent requests should succeed";
   };

   // ==================== Default Port Tests ====================

   "https_default_port_443"_test = [] {
      // This test verifies that parse_url correctly defaults to port 443 for HTTPS
      auto url_result = glz::parse_url("https://example.com/path");

      expect(url_result.has_value()) << "URL parsing should succeed";
      if (url_result.has_value()) {
         expect(url_result->protocol == "https") << "Protocol should be https";
         expect(url_result->host == "example.com") << "Host should be example.com";
         expect(url_result->port == 443) << "Port should default to 443 for HTTPS";
         expect(url_result->path == "/path") << "Path should be /path";
      }
   };
};

int main()
{
   std::cout << "HTTP Client TLS/HTTPS Tests" << std::endl;
   std::cout << "============================" << std::endl;
   std::cout << "Server initialized: " << (g_server.is_initialized() ? "yes" : "no") << std::endl;
   std::cout << "Server URL: " << g_server.base_url() << std::endl << std::endl;

   return 0;
}
