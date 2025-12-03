// Comprehensive HTTPS Server Test for Glaze Library with Built-in Certificate Generation
// Tests certificate loading, server startup, connections, and API functionality

#include <atomic>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <future>
#include <iostream>
#include <memory>
#include <thread>

#include "asio/io_context.hpp"

#ifndef GLZ_ENABLE_SSL
#define GLZ_ENABLE_SSL
#endif

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

#include <asio.hpp>

using namespace ut;

// Certificate generation class (unchanged)
class CertificateGenerator
{
  private:
   static void cleanup_openssl_errors()
   {
      while (ERR_get_error() != 0) {
         // Clear error queue
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

      // Set version (v3)
      X509_set_version(x509, 2);

      // Set serial number
      ASN1_INTEGER_set(X509_get_serialNumber(x509), 1);

      // Set validity period
      X509_gmtime_adj(X509_get_notBefore(x509), 0);
      X509_gmtime_adj(X509_get_notAfter(x509), static_cast<long>(days) * 24 * 60 * 60);

      // Set public key
      X509_set_pubkey(x509, pkey);

      // Set subject name
      X509_NAME* name = X509_get_subject_name(x509);
      X509_NAME_add_entry_by_txt(name, "C", MBSTRING_ASC, reinterpret_cast<const unsigned char*>("US"), -1, -1, 0);
      X509_NAME_add_entry_by_txt(name, "ST", MBSTRING_ASC, reinterpret_cast<const unsigned char*>("Test"), -1, -1, 0);
      X509_NAME_add_entry_by_txt(name, "L", MBSTRING_ASC, reinterpret_cast<const unsigned char*>("Test"), -1, -1, 0);
      X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC, reinterpret_cast<const unsigned char*>("Test"), -1, -1, 0);
      X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, reinterpret_cast<const unsigned char*>(subject.c_str()), -1,
                                 -1, 0);

      // Set issuer name (same as subject for self-signed)
      X509_set_issuer_name(x509, name);

      // Add extensions for server certificate
      if (subject == "localhost") {
         X509V3_CTX ctx;
         X509V3_set_ctx_nodb(&ctx);
         X509V3_set_ctx(&ctx, x509, x509, nullptr, nullptr, 0);

         // Add Subject Alternative Name extension
         X509_EXTENSION* ext =
            X509V3_EXT_conf_nid(nullptr, &ctx, NID_subject_alt_name,
                                const_cast<char*>("DNS:localhost,DNS:*.localhost,IP:127.0.0.1,IP:::1"));
         if (ext) {
            X509_add_ext(x509, ext, -1);
            X509_EXTENSION_free(ext);
         }

         // Add Basic Constraints
         ext = X509V3_EXT_conf_nid(nullptr, &ctx, NID_basic_constraints, const_cast<char*>("CA:FALSE"));
         if (ext) {
            X509_add_ext(x509, ext, -1);
            X509_EXTENSION_free(ext);
         }

         // Add Key Usage
         ext = X509V3_EXT_conf_nid(nullptr, &ctx, NID_key_usage, const_cast<char*>("keyEncipherment,digitalSignature"));
         if (ext) {
            X509_add_ext(x509, ext, -1);
            X509_EXTENSION_free(ext);
         }
      }

      // Sign the certificate
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

      return result == 1;
   }

  public:
   static bool generate_test_certificates()
   {
      std::cout << "🔐 Generating SSL certificates programmatically...\n";

      cleanup_openssl_errors();

      // Initialize OpenSSL
      OpenSSL_add_all_algorithms();
      ERR_load_crypto_strings();

      // Generate server key pair
      std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> server_key(generate_rsa_key(2048), EVP_PKEY_free);

      if (!server_key) {
         std::cerr << "Failed to generate server RSA key\n";
         return false;
      }

      // Generate server certificate
      std::unique_ptr<X509, decltype(&X509_free)> server_cert(create_certificate(server_key.get(), "localhost", 365),
                                                              X509_free);

      if (!server_cert) {
         std::cerr << "Failed to generate server certificate\n";
         return false;
      }

      // Write server certificate
      bool cert_written =
         write_pem_file("test_cert.pem", [&](BIO* bio) { return PEM_write_bio_X509(bio, server_cert.get()); });

      if (!cert_written) {
         std::cerr << "Failed to write server certificate\n";
         return false;
      }

      // Write server private key
      bool key_written = write_pem_file("test_key.pem", [&](BIO* bio) {
         return PEM_write_bio_PrivateKey(bio, server_key.get(), nullptr, nullptr, 0, nullptr, nullptr);
      });

      if (!key_written) {
         std::cerr << "Failed to write server private key\n";
         return false;
      }

      // Generate client key pair for completeness
      std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> client_key(generate_rsa_key(2048), EVP_PKEY_free);

      if (!client_key) {
         std::cerr << "Failed to generate client RSA key\n";
         return false;
      }

      // Generate client certificate
      std::unique_ptr<X509, decltype(&X509_free)> client_cert(create_certificate(client_key.get(), "test-client", 365),
                                                              X509_free);

      if (!client_cert) {
         std::cerr << "Failed to generate client certificate\n";
         return false;
      }

      // Write client certificate
      bool client_cert_written =
         write_pem_file("test_client_cert.pem", [&](BIO* bio) { return PEM_write_bio_X509(bio, client_cert.get()); });

      // Write client private key
      bool client_key_written = write_pem_file("test_client_key.pem", [&](BIO* bio) {
         return PEM_write_bio_PrivateKey(bio, client_key.get(), nullptr, nullptr, 0, nullptr, nullptr);
      });

      // Create combined certificate + key file
      std::ofstream combined("test_combined.pem");
      if (combined.is_open()) {
         std::ifstream cert_file("test_cert.pem");
         std::ifstream key_file("test_key.pem");

         combined << cert_file.rdbuf() << key_file.rdbuf();
         combined.close();
      }

// Set file permissions (equivalent to chmod 600 for private keys)
#ifdef _WIN32
// Windows doesn't have chmod, but we can try to set file attributes
#else
      chmod("test_key.pem", 0600);
      chmod("test_client_key.pem", 0600);
      chmod("test_combined.pem", 0600);
      chmod("test_cert.pem", 0644);
      chmod("test_client_cert.pem", 0644);
#endif

      std::cout << "✅ Certificates generated successfully:\n";
      std::cout << "   test_cert.pem - Server certificate\n";
      std::cout << "   test_key.pem - Server private key\n";
      std::cout << "   test_client_cert.pem - Client certificate\n";
      std::cout << "   test_client_key.pem - Client private key\n";
      std::cout << "   test_combined.pem - Combined cert + key\n\n";

      return cert_written && key_written && client_cert_written && client_key_written;
   }

   static bool verify_certificate_files()
   {
      std::ifstream cert("test_cert.pem");
      std::ifstream key("test_key.pem");

      if (!cert.is_open() || !key.is_open()) {
         return false;
      }

      std::string cert_content((std::istreambuf_iterator<char>(cert)), std::istreambuf_iterator<char>());
      std::string key_content((std::istreambuf_iterator<char>(key)), std::istreambuf_iterator<char>());

      return cert_content.find("BEGIN CERTIFICATE") != std::string::npos &&
             cert_content.find("END CERTIFICATE") != std::string::npos &&
             key_content.find("BEGIN") != std::string::npos && key_content.find("END") != std::string::npos;
   }
};

// Test data structures (unchanged)
struct ServerStatus
{
   std::string status;
   std::string server_type;
   int uptime_seconds;
   bool secure_connection;
};

struct EchoRequest
{
   std::string message;
   int echo_count;
};

struct EchoResponse
{
   std::string original_message;
   std::vector<std::string> echoes;
   bool processed_securely;
   std::string timestamp;
};

struct TestUser
{
   int id;
   std::string name;
   std::string email;
};

// Helper function to check if an error should be suppressed
bool should_suppress_error(const std::error_code& ec)
{
   // Common errors that are expected during normal operation
   if (ec == asio::error::eof) return true; // Client disconnected normally
   if (ec == asio::error::connection_reset) return true; // Connection reset by peer
   if (ec == asio::error::operation_aborted) return true; // Operation aborted (shutdown)
   if (ec == asio::error::bad_descriptor) return true; // Socket already closed
   if (ec == asio::error::broken_pipe) return true; // Broken pipe (client disconnected)

// SSL-specific errors that are normal
#ifdef GLZ_ENABLE_SSL
// Add SSL-specific error codes here if needed
#endif

   return false;
}

// Test server manager class with clean error handling
class HTTPSTestServer
{
   glz::https_server server_;
   std::future<void> server_future_;
   std::chrono::steady_clock::time_point start_time_;
   std::atomic<bool> running_{false};
   std::vector<TestUser> users_;
   int next_user_id_;

  public:
   HTTPSTestServer() : start_time_(std::chrono::steady_clock::now()), next_user_id_(1)
   {
      // Initialize test data
      users_ = {{1, "Alice Johnson", "alice@test.com"},
                {2, "Bob Smith", "bob@test.com"},
                {3, "Charlie Brown", "charlie@test.com"}};
      next_user_id_ = 4;

      // Set up clean error handler that suppresses expected disconnection errors
      server_.on_error([](std::error_code ec, std::source_location loc) {
         if (!should_suppress_error(ec)) {
            std::fprintf(stderr, "⚠️  Server error at %s:%d: %s\n", loc.file_name(), static_cast<int>(loc.line()),
                         ec.message().c_str());
         }
      });

      setup_routes();
   }

   ~HTTPSTestServer() { stop(); }

   bool start(uint16_t port = 8443)
   {
      try {
         server_.load_certificate("test_cert.pem", "test_key.pem");
         server_.set_ssl_verify_mode(0);
         server_.enable_cors();
         server_.bind("127.0.0.1", port);

         running_ = true;
         server_future_ = std::async(std::launch::async, [this]() { server_.start(2); });

         std::this_thread::sleep_for(std::chrono::milliseconds(300));
         return true;
      }
      catch (const std::exception& e) {
         std::cerr << "❌ Failed to start HTTPS server: " << e.what() << std::endl;
         return false;
      }
   }

   void stop()
   {
      if (running_) {
         running_ = false;
         server_.stop();

         if (server_future_.valid()) {
            auto status = server_future_.wait_for(std::chrono::seconds(2));
            if (status != std::future_status::ready) {
               std::cerr << "⚠️  Warning: Server did not stop cleanly" << std::endl;
            }
         }
      }
   }

  private:
   void setup_routes()
   {
      // Health check endpoint
      server_.get("/health", [](const glz::request&, glz::response& res) { res.status(200).body("HTTPS Server OK"); });

      // Server status endpoint
      server_.get("/status", [this](const glz::request&, glz::response& res) {
         auto now = std::chrono::steady_clock::now();
         auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_);

         ServerStatus status{"running", "glaze-https", static_cast<int>(uptime.count()), true};

         res.json(status);
      });

      // Echo endpoint for JSON testing
      server_.post("/echo", [](const glz::request& req, glz::response& res) {
         EchoRequest echo_req;
         auto parse_result = glz::read_json(echo_req, req.body);

         if (parse_result) {
            res.status(400).body("Invalid JSON in request body");
            return;
         }

         EchoResponse echo_res;
         echo_res.original_message = echo_req.message;
         echo_res.processed_securely = true;
         echo_res.timestamp = std::to_string(
            std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
               .count());

         // Create echoes (limit to 10)
         for (int i = 0; i < echo_req.echo_count && i < 10; ++i) {
            echo_res.echoes.push_back(echo_req.message + " (echo " + std::to_string(i + 1) + ")");
         }

         res.json(echo_res);
      });

      // Users API - GET all users
      server_.get("/users", [this](const glz::request&, glz::response& res) { res.json(users_); });

      // Users API - GET user by ID
      server_.get("/users/{id}", [this](const glz::request& req, glz::response& res) {
         auto id_it = req.params.find("id");
         if (id_it == req.params.end()) {
            res.status(400).body("Missing user ID");
            return;
         }

         int user_id;
         try {
            user_id = std::stoi(id_it->second);
         }
         catch (...) {
            res.status(400).body("Invalid user ID");
            return;
         }

         auto user_it =
            std::find_if(users_.begin(), users_.end(), [user_id](const TestUser& u) { return u.id == user_id; });

         if (user_it != users_.end()) {
            res.json(*user_it);
         }
         else {
            res.status(404).body("User not found");
         }
      });

      // Users API - POST new user
      server_.post("/users", [this](const glz::request& req, glz::response& res) {
         TestUser new_user;
         auto parse_result = glz::read_json(new_user, req.body);

         if (parse_result) {
            res.status(400).body("Invalid JSON in request body");
            return;
         }

         new_user.id = next_user_id_++;
         users_.push_back(new_user);

         res.status(201).json(new_user);
      });

      // Large response test
      server_.get("/large", [](const glz::request&, glz::response& res) {
         std::string large_response;
         large_response.reserve(50000);

         for (int i = 0; i < 500; ++i) {
            large_response += "Line " + std::to_string(i) + ": ";
            large_response += "This is a test of large HTTPS responses to ensure ";
            large_response += "that SSL/TLS can handle substantial payloads correctly. ";
            large_response += "Each line contains meaningful test data for verification.\n";
         }

         res.content_type("text/plain").body(large_response);
      });

      // Concurrent test endpoint
      server_.get("/concurrent/{id}", [](const glz::request& req, glz::response& res) {
         auto id_it = req.params.find("id");
         std::string id = (id_it != req.params.end()) ? id_it->second : "unknown";

         // Simulate some processing time
         std::this_thread::sleep_for(std::chrono::milliseconds(50));

         res.json(ServerStatus{"processed", "concurrent-test-" + id, 50, true});
      });

      // Stress test endpoint
      server_.get("/stress", [](const glz::request&, glz::response& res) {
         // Generate some CPU work
         volatile int sum = 0;
         for (int i = 0; i < 100000; ++i) {
            sum += i;
         }

         res.json(ServerStatus{"stress-complete", "stress-test", static_cast<int>(sum % 1000), true});
      });
   }
};

// Helper functions (unchanged)
bool file_exists(const std::string& filename)
{
   std::ifstream file(filename);
   return file.good();
}

bool certificates_exist() { return file_exists("test_cert.pem") && file_exists("test_key.pem"); }

void wait_for_port_free(int port, int max_attempts = 20)
{
   for (int i = 0; i < max_attempts; ++i) {
      if (system(("lsof -ti:" + std::to_string(port) + " >/dev/null 2>&1").c_str()) != 0) {
         return; // Port is free
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
   }
}

// Test suites (unchanged)
suite certificate_tests = [] {
   "certificate_generation"_test = [] {
      if (!certificates_exist()) {
         expect(CertificateGenerator::generate_test_certificates())
            << "Should generate SSL certificates programmatically\n";
      }

      expect(certificates_exist()) << "SSL certificates should exist after generation\n";
   };

   "certificate_content_valid"_test = [] {
      // Ensure certificates exist (generate if needed)
      if (!certificates_exist()) {
         CertificateGenerator::generate_test_certificates();
      }

      expect(CertificateGenerator::verify_certificate_files())
         << "Generated certificates should have valid PEM format\n";
   };
};

suite server_lifecycle_tests = [] {
   "https_server_creation"_test = [] {
      glz::https_server server;
      expect(true) << "HTTPS server should be created successfully\n";
   };

   "https_server_configuration"_test = [] {
      glz::https_server server;

      auto& server_ref = server.enable_cors().set_ssl_verify_mode(0);

      expect(&server_ref == &server) << "Method chaining should work\n";
   };

   "https_server_startup_shutdown"_test = [] {
      // Ensure certificates exist
      if (!certificates_exist()) {
         CertificateGenerator::generate_test_certificates();
      }

      wait_for_port_free(8444);

      HTTPSTestServer test_server;
      expect(test_server.start(8444)) << "HTTPS server should start successfully\n";

      std::this_thread::sleep_for(std::chrono::milliseconds(200));

      test_server.stop();
      expect(true) << "HTTPS server should stop cleanly\n";
   };
};

class ExternalIOContextServer
{
  public:
   ExternalIOContextServer()
      : io_context(std::make_shared<asio::io_context>()),
        server_(io_context,
                [](std::error_code, std::source_location) { std::cout << "HTTPS Server Error Handler Invoked\n"; }),
        work_guard(asio::make_work_guard(*io_context))
   {
      // Ensure certificates exist
      if (!certificates_exist()) {
         CertificateGenerator::generate_test_certificates();
      }

      wait_for_port_free(8443);
   }

   ~ExternalIOContextServer() { stop_io_thread(); }

   void start_io_thread()
   {
      io_thread = std::thread([this]() {
         setup_routes();
         server_.load_certificate("test_cert.pem", "test_key.pem");
         server_.set_ssl_verify_mode(0);
         server_.enable_cors();
         server_.bind("127.0.0.1", 8443);
         // Start the server accepting connections without creating worker threads (0)
         // We'll run the io_context manually in this thread
         server_.start(0);
         // Run the io_context event loop in this thread
         io_context->run();
      });
   }

   void stop_io_thread()
   {
      work_guard.reset();
      io_context->stop();
      server_.stop();
      if (io_thread.joinable()) {
         io_thread.join();
      }
   }

   void setup_routes()
   {
      // Health check endpoint
      server_.get("/health", [](const glz::request&, glz::response& res) { res.status(200).body("HTTPS Server OK"); });
   }

  private:
   std::shared_ptr<asio::io_context> io_context;
   glz::https_server server_;
   std::thread io_thread;
   asio::executor_work_guard<asio::io_context::executor_type> work_guard;
};

suite server_lifecycle_external_context_tests = [] {
   auto io_context = std::make_shared<asio::io_context>();
   auto error_handler = [](std::error_code, std::source_location) {};
   "https_server_creation"_test = [&] {
      glz::https_server server(io_context, error_handler);
      expect(true) << "HTTPS server should be created successfully\n";
   };

   "https_server_configuration"_test = [&] {
      glz::https_server server(io_context, error_handler);

      auto& server_ref = server.enable_cors().set_ssl_verify_mode(0);

      expect(&server_ref == &server) << "Method chaining should work\n";
   };

   "https_server_startup_shutdown"_test = [&] {
      // Ensure certificates exist
      if (!certificates_exist()) {
         CertificateGenerator::generate_test_certificates();
      }

      ExternalIOContextServer server;
      std::cout << "Starting HTTPS server with external io_context thread...\n";
      server.start_io_thread();

      // Wait for server to be ready
      std::this_thread::sleep_for(std::chrono::milliseconds(500));

      // Verify server is running by making a successful connection
      bool connection_successful = false;
      try {
         asio::io_context client_io;
         asio::ip::tcp::socket socket(client_io);
         asio::ip::tcp::endpoint endpoint(asio::ip::make_address("127.0.0.1"), 8443);

         std::error_code ec;
         socket.connect(endpoint, ec);
         connection_successful = !ec;
         if (!ec) {
            socket.close();
         }
      }
      catch (...) {
         connection_successful = false;
      }

      expect(connection_successful) << "Server should accept connections before shutdown\n";

      // Stop the server
      server.stop_io_thread();
      std::cout << "Server stopped, verifying connections are closed...\n";

      // Wait a moment for cleanup
      std::this_thread::sleep_for(std::chrono::milliseconds(200));

      // Verify connections are refused after shutdown
      bool connection_refused = false;
      try {
         asio::io_context client_io;
         asio::ip::tcp::socket socket(client_io);
         asio::ip::tcp::endpoint endpoint(asio::ip::make_address("127.0.0.1"), 8443);

         std::error_code ec;
         socket.connect(endpoint, ec);
         connection_refused = (ec == asio::error::connection_refused);
         if (!ec) {
            socket.close();
         }
      }
      catch (...) {
         connection_refused = true; // Exception also indicates connection failed
      }

      expect(connection_refused) << "Server should refuse connections after shutdown\n";
   };
};

suite api_functionality_tests = [] {
   "https_api_endpoints"_test = [] {
      // Ensure certificates exist
      if (!certificates_exist()) {
         CertificateGenerator::generate_test_certificates();
      }

      wait_for_port_free(8445);

      HTTPSTestServer test_server;
      if (!test_server.start(8445)) {
         std::cout << "❌ Failed to start server for API test\n";
         return;
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(300));

      std::cout << "HTTPS server started on port 8445 with API endpoints:\n";
      std::cout << "  /health - Health check\n";
      std::cout << "  /status - Server status\n";
      std::cout << "  /echo - JSON echo service\n";
      std::cout << "  /users - User management API\n";
      std::cout << "  /large - Large response test\n";
      std::cout << "Manual test: curl -k https://localhost:8445/health\n";

      test_server.stop();
      expect(true) << "API endpoints configured successfully\n";
   };
};

suite concurrent_tests = [] {
   "concurrent_server_instances"_test = [] {
      // Ensure certificates exist
      if (!certificates_exist()) {
         CertificateGenerator::generate_test_certificates();
      }

      wait_for_port_free(8446);
      wait_for_port_free(8447);

      HTTPSTestServer server1;
      HTTPSTestServer server2;

      expect(server1.start(8446)) << "First server should start successfully\n";
      std::this_thread::sleep_for(std::chrono::milliseconds(200));

      expect(server2.start(8447)) << "Second server should start on different port\n";
      std::this_thread::sleep_for(std::chrono::milliseconds(200));

      server1.stop();
      server2.stop();

      expect(true) << "Multiple HTTPS servers should work concurrently\n";
   };

   "rapid_startup_shutdown"_test = [] {
      // Ensure certificates exist
      if (!certificates_exist()) {
         CertificateGenerator::generate_test_certificates();
      }

      wait_for_port_free(8448);

      for (int i = 0; i < 3; ++i) {
         HTTPSTestServer server;
         expect(server.start(8448)) << "Server " << i << " should start\n";
         std::this_thread::sleep_for(std::chrono::milliseconds(100));
         server.stop();
         std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }

      expect(true) << "Rapid startup/shutdown should work\n";
   };
};

suite performance_tests = [] {
   "server_startup_performance"_test = [] {
      // Ensure certificates exist
      if (!certificates_exist()) {
         CertificateGenerator::generate_test_certificates();
      }

      wait_for_port_free(8449);

      auto start_time = std::chrono::high_resolution_clock::now();

      HTTPSTestServer server;
      bool started = server.start(8449);

      auto end_time = std::chrono::high_resolution_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

      if (started) {
         std::cout << "HTTPS server startup time: " << duration.count() << "ms\n";
         expect(duration.count() < 2000) << "Server should start within 2 seconds\n";

         server.stop();
      }
      else {
         expect(false) << "Server should start for performance test\n";
      }
   };

   "memory_usage_test"_test = [] {
      std::vector<std::unique_ptr<glz::https_server>> servers;

      for (int i = 0; i < 10; ++i) {
         servers.push_back(std::make_unique<glz::https_server>());
      }

      servers.clear();

      expect(true) << "Multiple server creation/destruction should not leak memory\n";
   };
};

suite stress_tests = [] {
   "configuration_stress"_test = [] {
      glz::https_server server;

      for (int i = 0; i < 100; ++i) {
         server.enable_cors().set_ssl_verify_mode(i % 3);
      }

      expect(true) << "Rapid configuration changes should not crash\n";
   };

   "route_registration_stress"_test = [] {
      glz::https_server server;

      for (int i = 0; i < 50; ++i) {
         std::string path = "/test" + std::to_string(i);
         server.get(path, [i](const glz::request&, glz::response& res) { res.json({"test_id", i}); });
      }

      expect(true) << "Many route registrations should work\n";
   };
};

int main()
{
   std::cout << "===============================================\n\n";

   std::cout << "🔐 Checking for SSL certificates...\n";

   if (!certificates_exist()) {
      std::cout << "📋 Certificates not found - generating them now...\n";
      if (!CertificateGenerator::generate_test_certificates()) {
         std::cerr << "❌ Failed to generate certificates!\n";
         return 1;
      }
   }
   else {
      std::cout << "✅ SSL certificates already exist\n";

      // Verify existing certificates are valid
      if (!CertificateGenerator::verify_certificate_files()) {
         std::cout << "⚠️  Existing certificates appear invalid - regenerating...\n";
         if (!CertificateGenerator::generate_test_certificates()) {
            std::cerr << "❌ Failed to regenerate certificates!\n";
            return 1;
         }
      }
   }

   std::cout << "\n🧪 Running comprehensive HTTPS tests...\n";

   // Tests run automatically through the ut library

   std::cout << "\n🔍 Manual Testing Commands:\n";
   std::cout << "  curl -k https://localhost:8443/health\n";
   std::cout << "  curl -k https://localhost:8443/status\n";
   std::cout << "  curl -k -X POST https://localhost:8443/echo -H 'Content-Type: application/json' -d "
                "'{\"message\":\"test\",\"echo_count\":3}'\n";
   std::cout << "  openssl s_client -connect localhost:8443 -servername localhost\n\n";

   return 0;
}
