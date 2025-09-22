// Glaze Library
// For the license information refer to glaze.hpp

// Unit Tests for CORS Preflight Handling
// Tests automatic OPTIONS route generation and CORS preflight request handling

#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "glaze/glaze.hpp"
#include "glaze/net/http_client.hpp"
#include "glaze/net/http_router.hpp"
#include "glaze/net/http_server.hpp"
#include "glaze/net/cors.hpp"
#include "ut/ut.hpp"

using namespace ut;

// Test data structures
struct UserData
{
   std::string name;
   int age;
   std::string email;
};

struct ApiResponse
{
   bool success;
   std::string message;
   std::optional<UserData> user;
};

// CORS Preflight Tests
suite cors_preflight_tests = [] {
   "http_router_options_method_support"_test = [] {
      glz::http_router router;
      bool options_executed = false;
      
      // Register OPTIONS route
      router.options("/api/test", [&options_executed](const glz::request&, glz::response& res) {
         options_executed = true;
         res.status(204)
            .header("Access-Control-Allow-Origin", "*")
            .header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
      });
      
      // Test route matching
      auto [handler, params] = router.match(glz::http_method::OPTIONS, "/api/test");
      expect(handler != nullptr) << "OPTIONS route should be registered and matchable";
      
      // Execute handler
      if (handler) {
         glz::request req{};
         req.method = glz::http_method::OPTIONS;
         req.target = "/api/test";
         
         glz::response res{};
         handler(req, res);
         
         expect(options_executed) << "OPTIONS handler should be executed";
         expect(res.status_code == 204) << "OPTIONS should return 204 No Content";
         expect(res.response_headers.count("access-control-allow-origin") > 0) << "CORS headers should be set";
      }
   };

   "http_router_options_async_support"_test = [] {
      glz::http_router router;
      std::atomic<bool> async_executed{false};
      
      // Register async OPTIONS route
      router.options_async("/api/async-test", 
         [&async_executed](const glz::request&, glz::response& res) -> std::future<void> {
            return std::async(std::launch::async, [&async_executed, &res]() {
               async_executed = true;
               res.status(204)
                  .header("Access-Control-Allow-Origin", "https://example.com")
                  .header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
            });
         });
      
      // Test route matching
      auto [handler, params] = router.match(glz::http_method::OPTIONS, "/api/async-test");
      expect(handler != nullptr) << "Async OPTIONS route should be registered";
      
      // Execute handler
      if (handler) {
         glz::request req{};
         req.method = glz::http_method::OPTIONS;
         req.target = "/api/async-test";
         
         glz::response res{};
         handler(req, res);
         
         // Wait for async completion
         std::this_thread::sleep_for(std::chrono::milliseconds(10));
         
         expect(async_executed.load()) << "Async OPTIONS handler should be executed";
         expect(res.status_code == 204) << "Async OPTIONS should return 204";
      }
   };

   "auto_options_route_generation_on_cors_enable"_test = [] {
      glz::http_server<> server;
      
      // Register routes before enabling CORS
      server.get("/api/users", [](const glz::request&, glz::response& res) {
         res.json(std::vector<UserData>{
            {"Alice", 30, "alice@example.com"},
            {"Bob", 25, "bob@example.com"}
         });
      });
      
      server.post("/api/users", [](const glz::request& req, glz::response& res) {
         UserData user{};
         auto ec = glz::read_json(user, req.body);
         if (ec) {
            res.status(400).json({{"error", "Invalid JSON"}});
         } else {
            res.status(201).json(ApiResponse{true, "User created", user});
         }
      });
      
      // Enable CORS - should auto-generate OPTIONS routes
      server.enable_cors();
      
      // Test that OPTIONS routes were created
      auto [get_options_handler, params1] = server.get_router().match(glz::http_method::OPTIONS, "/api/users");
      expect(get_options_handler != nullptr) << "OPTIONS route should be auto-generated for /api/users";
      
      // Execute OPTIONS handler
      if (get_options_handler) {
         glz::request req{};
         req.method = glz::http_method::OPTIONS;
         req.target = "/api/users";
         req.headers["origin"] = "https://example.com";
         req.headers["access-control-request-method"] = "POST";
         
         glz::response res{};
         get_options_handler(req, res);
         
         expect(res.status_code == 204) << "Auto-generated OPTIONS should return 204";
         expect(res.response_headers.count("access-control-allow-origin") > 0) 
            << "Auto-generated OPTIONS should set CORS headers";
      }
   };

   "custom_cors_config_with_auto_options"_test = [] {
      glz::http_server<> server;
      
      // Register routes
      server.put("/api/resource/:id", [](const glz::request& req, glz::response& res) {
         res.json({{"updated", req.params.at("id")}});
      });
      
      server.del("/api/resource/:id", [](const glz::request& req, glz::response& res) {
         res.status(204);
      });
      
      // Enable CORS with custom config
      glz::cors_config config;
      config.allowed_origins = {"https://app.example.com", "https://admin.example.com"};
      config.allowed_methods = {"GET", "POST", "PUT", "DELETE", "OPTIONS"};
      config.allowed_headers = {"Content-Type", "Authorization", "X-API-Key"};
      config.allow_credentials = true;
      config.max_age = 3600;
      
      server.enable_cors(config);
      
      // Test OPTIONS route for parameterized path
      auto [options_handler, params] = server.get_router().match(glz::http_method::OPTIONS, "/api/resource/123");
      expect(options_handler != nullptr) << "OPTIONS should work with parameterized routes";
      
      if (options_handler) {
         glz::request req{};
         req.method = glz::http_method::OPTIONS;
         req.target = "/api/resource/123";
         req.headers["origin"] = "https://app.example.com";
         req.headers["access-control-request-method"] = "PUT";
         req.headers["access-control-request-headers"] = "Content-Type, Authorization";
         
         glz::response res{};
         options_handler(req, res);
         
         expect(res.status_code == 204) << "Preflight with custom config should return 204";
         // Note: Detailed header validation would depend on CORS middleware implementation
      }
   };

   "override_specific_options_handler"_test = [] {
      glz::http_server<> server;
      
      // Register normal routes
      server.get("/api/public", [](const glz::request&, glz::response& res) {
         res.json({{"data", "public"}});
      });
      
      server.get("/api/special", [](const glz::request&, glz::response& res) {
         res.json({{"data", "special"}});
      });
      
      // Add custom OPTIONS handler for /api/special BEFORE enabling CORS
      bool custom_options_called = false;
      server.options("/api/special", [&custom_options_called](const glz::request&, glz::response& res) {
         custom_options_called = true;
         res.status(204)
            .header("Access-Control-Allow-Origin", "https://special.example.com")
            .header("Access-Control-Allow-Methods", "GET, OPTIONS")
            .header("X-Custom-Header", "special-value");
      });
      
      // Enable CORS - should not override existing OPTIONS handler
      server.enable_cors();
      
      // Test custom OPTIONS handler
      auto [special_handler, params1] = server.get_router().match(glz::http_method::OPTIONS, "/api/special");
      expect(special_handler != nullptr) << "Custom OPTIONS handler should be registered";
      
      if (special_handler) {
         glz::request req{};
         req.method = glz::http_method::OPTIONS;
         req.target = "/api/special";
         
         glz::response res{};
         special_handler(req, res);
         
         expect(custom_options_called) << "Custom OPTIONS handler should be called";
         expect(res.response_headers.count("x-custom-header") > 0) << "Custom header should be present";
      }
      
      // Test that /api/public still uses auto-generated OPTIONS
      auto [public_handler, params2] = server.get_router().match(glz::http_method::OPTIONS, "/api/public");
      expect(public_handler != nullptr) << "Auto-generated OPTIONS should exist for /api/public";
   };

   "routes_added_after_cors_enabled"_test = [] {
      glz::http_server<> server;
      
      // Enable CORS first
      server.enable_cors();
      
      // Add routes after CORS is enabled
      server.get("/api/late-route", [](const glz::request&, glz::response& res) {
         res.json({{"message", "Added after CORS"}});
      });
      
      server.post("/api/late-route", [](const glz::request&, glz::response& res) {
         res.status(201).json({{"created", true}});
      });
      
      // Test that OPTIONS route was auto-created for late-added routes
      auto [handler, params] = server.get_router().match(glz::http_method::OPTIONS, "/api/late-route");
      expect(handler != nullptr) << "OPTIONS should be auto-created for routes added after CORS is enabled";
      
      if (handler) {
         glz::request req{};
         req.method = glz::http_method::OPTIONS;
         req.target = "/api/late-route";
         req.headers["origin"] = "https://example.com";
         
         glz::response res{};
         handler(req, res);
         
         expect(res.status_code == 204) << "Late-added OPTIONS route should return 204";
      }
   };

   "wildcard_routes_with_options"_test = [] {
      glz::http_server<> server;
      
      // Enable CORS
      server.enable_cors();
      
      // Register wildcard route
      server.get("/api/files/*path", [](const glz::request& req, glz::response& res) {
         auto path_it = req.params.find("path");
         std::string path = path_it != req.params.end() ? path_it->second : "";
         res.json({{"file", path}});
      });
      
      // Test OPTIONS for wildcard route
      auto [handler, params] = server.get_router().match(glz::http_method::OPTIONS, "/api/files/documents/report.pdf");
      expect(handler != nullptr) << "OPTIONS should work with wildcard routes";
      
      if (handler) {
         glz::request req{};
         req.method = glz::http_method::OPTIONS;
         req.target = "/api/files/documents/report.pdf";
         
         glz::response res{};
         handler(req, res);
         
         expect(res.status_code == 204) << "Wildcard OPTIONS should return 204";
      }
   };

   "no_duplicate_options_routes"_test = [] {
      glz::http_server<> server;
      
      // Manually add OPTIONS route
      bool manual_options_called = false;
      server.options("/api/manual", [&manual_options_called](const glz::request&, glz::response& res) {
         manual_options_called = true;
         res.status(204).header("X-Manual", "true");
      });
      
      // Add regular route
      server.get("/api/manual", [](const glz::request&, glz::response& res) {
         res.json({{"data", "manual"}});
      });
      
      // Enable CORS - should not override existing OPTIONS route
      server.enable_cors();
      
      // Test that manual OPTIONS handler is still used
      auto [handler, params] = server.get_router().match(glz::http_method::OPTIONS, "/api/manual");
      expect(handler != nullptr) << "OPTIONS route should exist";
      
      if (handler) {
         glz::request req{};
         req.method = glz::http_method::OPTIONS;
         req.target = "/api/manual";
         
         glz::response res{};
         handler(req, res);
         
         expect(manual_options_called) << "Manual OPTIONS handler should be preserved";
         expect(res.response_headers.count("x-manual") > 0) << "Manual handler headers should be present";
      }
   };

   "cors_with_restrictive_origins"_test = [] {
      glz::http_server<> server;
      
      // Register routes
      server.get("/api/restricted", [](const glz::request&, glz::response& res) {
         res.json({{"data", "restricted"}});
      });
      
      // Enable CORS with specific origins
      std::vector<std::string> allowed_origins = {
         "https://app.example.com",
         "https://admin.example.com"
      };
      server.enable_cors(allowed_origins, true); // allow credentials
      
      // Test OPTIONS route
      auto [handler, params] = server.get_router().match(glz::http_method::OPTIONS, "/api/restricted");
      expect(handler != nullptr) << "OPTIONS route should be created with restrictive CORS";
      
      if (handler) {
         glz::request req{};
         req.method = glz::http_method::OPTIONS;
         req.target = "/api/restricted";
         req.headers["origin"] = "https://app.example.com";
         req.headers["access-control-request-method"] = "GET";
         
         glz::response res{};
         handler(req, res);
         
         expect(res.status_code == 204) << "Restrictive CORS OPTIONS should return 204";
         // Note: Actual origin validation would be in the CORS middleware
      }
   };
};

// Integration test server for CORS testing
class cors_test_server
{
public:
   cors_test_server() : port_(0), running_(false) {}
   
   ~cors_test_server() { stop(); }
   
   bool start(bool enable_cors = true, const glz::cors_config* custom_config = nullptr)
   {
      if (running_) return true;
      
      setup_routes();
      
      if (enable_cors) {
         if (custom_config) {
            server_.enable_cors(*custom_config);
         } else {
            server_.enable_cors();
         }
      }
      
      try {
         // Try to find a free port
         for (uint16_t test_port = 18080; test_port < 18200; ++test_port) {
            try {
               server_.bind("127.0.0.1", test_port);
               port_ = test_port;
               break;
            }
            catch (...) {
               continue;
            }
         }
         
         if (port_ == 0) {
            std::cerr << "Could not find free port for test server\n";
            return false;
         }
         
         running_ = true;
         
         // Start server in background thread
         server_thread_ = std::thread([this]() {
            try {
               server_.start(1);
            }
            catch (const std::exception& e) {
               std::cerr << "Server error: " << e.what() << std::endl;
               running_ = false;
            }
         });
         
         // Wait for server to be ready
         for (int i = 0; i < 50; ++i) { // 5 second timeout
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (is_server_ready()) {
               return true;
            }
         }
         
         std::cerr << "Server failed to start within timeout\n";
         stop();
         return false;
      }
      catch (const std::exception& e) {
         std::cerr << "Failed to start server: " << e.what() << std::endl;
         return false;
      }
   }
   
   void stop()
   {
      if (!running_) return;
      
      running_ = false;
      
      // Give any ongoing operations a moment to complete
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      
      server_.stop();
      
      if (server_thread_.joinable()) {
         server_thread_.join();
      }
   }
   
   uint16_t port() const { return port_; }
   std::string base_url() const { return "http://127.0.0.1:" + std::to_string(port_); }
   
   glz::http_server<>& get_server() { return server_; }
   
private:
   glz::http_server<> server_;
   std::thread server_thread_;
   uint16_t port_;
   std::atomic<bool> running_;
   
   void setup_routes()
   {
      // Set up error handler
      server_.on_error([this](std::error_code ec, std::source_location loc) {
         if (running_ && ec != asio::error::eof && ec != asio::error::operation_aborted) {
            std::fprintf(stderr, "Server error at %s:%d: %s\n", 
                        loc.file_name(), static_cast<int>(loc.line()),
                        ec.message().c_str());
         }
      });
      
      // Basic routes for testing
      server_.get("/api/data", [](const glz::request&, glz::response& res) {
         res.json({{"message", "GET successful"}, {"value", 42}});
      });
      
      server_.post("/api/data", [](const glz::request& req, glz::response& res) {
         glz::json_t data{};
         auto ec = glz::read_json(data, req.body);
         if (ec) {
            res.status(400).json({{"error", "Invalid JSON"}});
         } else {
            res.status(201).json({{"message", "POST successful"}, {"received", data}});
         }
      });
      
      server_.put("/api/data/:id", [](const glz::request& req, glz::response& res) {
         res.json({{"message", "PUT successful"}, {"id", req.params.at("id")}});
      });
      
      server_.del("/api/data/:id", [](const glz::request& req, glz::response& res) {
         res.status(204);
      });
      
      server_.patch("/api/data/:id", [](const glz::request& req, glz::response& res) {
         res.json({{"message", "PATCH successful"}, {"id", req.params.at("id")}});
      });
   }
   
   bool is_server_ready()
   {
      // Simple check to see if server is responding
      try {
         asio::io_context io;
         asio::ip::tcp::socket socket(io);
         asio::ip::tcp::endpoint endpoint(asio::ip::make_address("127.0.0.1"), port_);
         
         std::error_code ec;
         socket.connect(endpoint, ec);
         if (!ec) {
            socket.close();
            return true;
         }
      }
      catch (...) {
         // Ignore exceptions during readiness check
      }
      return false;
   }
};

// Full integration tests with actual server
suite cors_preflight_integration_tests = [] {
   "basic_preflight_request"_test = [] {
      cors_test_server server;
      expect(server.start(true)) << "Server should start with CORS enabled";
      
      glz::http_client client;
      
      // Send OPTIONS preflight request
      std::unordered_map<std::string, std::string> headers;
      headers["Origin"] = "https://example.com";
      headers["Access-Control-Request-Method"] = "POST";
      headers["Access-Control-Request-Headers"] = "Content-Type";
      
      auto result = client.options(server.base_url() + "/api/data", headers);
      
      expect(result.has_value()) << "OPTIONS request should succeed";
      if (result.has_value()) {
         expect(result->status_code == 204) << "Preflight should return 204 No Content";
         
         // Check for CORS headers
         auto origin_it = result->response_headers.find("access-control-allow-origin");
         expect(origin_it != result->response_headers.end()) << "Should have Allow-Origin header";
         
         auto methods_it = result->response_headers.find("access-control-allow-methods");
         expect(methods_it != result->response_headers.end()) << "Should have Allow-Methods header";
      }
      
      server.stop();
   };
   
   "preflight_followed_by_actual_request"_test = [] {
      cors_test_server server;
      expect(server.start(true)) << "Server should start with CORS enabled";
      
      glz::http_client client;
      
      // Step 1: Send preflight
      std::unordered_map<std::string, std::string> preflight_headers;
      preflight_headers["Origin"] = "https://app.example.com";
      preflight_headers["Access-Control-Request-Method"] = "POST";
      preflight_headers["Access-Control-Request-Headers"] = "Content-Type";
      
      auto preflight = client.options(server.base_url() + "/api/data", preflight_headers);
      expect(preflight.has_value() && preflight->status_code == 204) << "Preflight should succeed";
      
      // Step 2: Send actual POST request
      std::unordered_map<std::string, std::string> post_headers;
      post_headers["Content-Type"] = "application/json";
      post_headers["Origin"] = "https://app.example.com";
      
      std::string json_body = R"({"test": "data", "value": 123})";
      auto actual = client.post(server.base_url() + "/api/data", json_body, post_headers);
      
      if (!actual.has_value()) {
         std::cerr << "POST failed with error: " << actual.error().message() << std::endl;
      }
      expect(actual.has_value()) << "POST request should succeed after preflight";
      if (actual.has_value()) {
         expect(actual->status_code == 201) << "POST should return 201 Created";
         expect(actual->response_body.find("POST successful") != std::string::npos) 
            << "Response should contain success message";
      }
      
      server.stop();
   };
   
   "preflight_for_different_methods"_test = [] {
      cors_test_server server;
      expect(server.start(true)) << "Server should start with CORS enabled";
      
      glz::http_client client;
      std::string base = server.base_url();
      
      // Test preflight for PUT
      {
         std::unordered_map<std::string, std::string> headers;
         headers["Origin"] = "https://example.com";
         headers["Access-Control-Request-Method"] = "PUT";
         
         auto result = client.options(base + "/api/data/123", headers);
         expect(result.has_value() && result->status_code == 204) << "PUT preflight should succeed";
      }
      
      // Test preflight for DELETE
      {
         std::unordered_map<std::string, std::string> headers;
         headers["Origin"] = "https://example.com";
         headers["Access-Control-Request-Method"] = "DELETE";
         
         auto result = client.options(base + "/api/data/456", headers);
         if (!result.has_value()) {
            std::cerr << "DELETE OPTIONS failed with error: " << result.error().message() << std::endl;
         } else if (result->status_code != 204) {
            std::cerr << "DELETE OPTIONS returned status: " << result->status_code << std::endl;
         }
         expect(result.has_value() && result->status_code == 204) << "DELETE preflight should succeed";
      }
      
      // Test preflight for PATCH
      {
         std::unordered_map<std::string, std::string> headers;
         headers["Origin"] = "https://example.com";
         headers["Access-Control-Request-Method"] = "PATCH";
         
         auto result = client.options(base + "/api/data/789", headers);
         expect(result.has_value() && result->status_code == 204) << "PATCH preflight should succeed";
      }
      
      server.stop();
   };
   
   "custom_cors_configuration"_test = [] {
      cors_test_server server;
      
      // Configure custom CORS
      glz::cors_config config;
      config.allowed_origins = {"https://trusted.example.com"};
      config.allowed_methods = {"GET", "POST", "OPTIONS"};
      config.allowed_headers = {"Content-Type", "Authorization"};
      config.allow_credentials = true;
      config.max_age = 7200;
      
      // Start server with custom config
      expect(server.start(false)) << "Server should start without default CORS";
      server.get_server().enable_cors(config);
      
      glz::http_client client;
      
      // Test with allowed origin
      {
         std::unordered_map<std::string, std::string> headers;
         headers["Origin"] = "https://trusted.example.com";
         headers["Access-Control-Request-Method"] = "POST";
         headers["Access-Control-Request-Headers"] = "Content-Type, Authorization";
         
         auto result = client.options(server.base_url() + "/api/data", headers);
         expect(result.has_value() && result->status_code == 204) 
            << "Preflight with allowed origin should succeed";
         
         if (result.has_value()) {
            auto creds_it = result->response_headers.find("access-control-allow-credentials");
            if (creds_it != result->response_headers.end()) {
               expect(creds_it->second == "true") << "Should allow credentials";
            }
         }
      }
      
      server.stop();
   };
   
   "concurrent_preflight_requests"_test = [] {
      cors_test_server server;
      expect(server.start(true)) << "Server should start with CORS enabled";
      
      const int num_threads = 5;
      std::vector<std::thread> threads;
      std::atomic<int> success_count{0};
      
      for (int i = 0; i < num_threads; ++i) {
         threads.emplace_back([&server, &success_count, i]() {
            glz::http_client client;
            
            std::unordered_map<std::string, std::string> headers;
            headers["Origin"] = "https://example" + std::to_string(i) + ".com";
            headers["Access-Control-Request-Method"] = "POST";
            
            auto result = client.options(server.base_url() + "/api/data", headers);
            if (result.has_value() && result->status_code == 204) {
               success_count++;
            }
         });
      }
      
      for (auto& t : threads) {
         t.join();
      }
      
      expect(success_count == num_threads) << "All concurrent preflight requests should succeed";
      
      server.stop();
   };
   
   "custom_headers_in_preflight"_test = [] {
      cors_test_server server;
      expect(server.start(true)) << "Server should start with CORS enabled";
      
      glz::http_client client;
      
      // Request with custom headers
      std::unordered_map<std::string, std::string> headers;
      headers["Origin"] = "https://example.com";
      headers["Access-Control-Request-Method"] = "POST";
      headers["Access-Control-Request-Headers"] = "Content-Type, X-Custom-Header, X-API-Key";
      
      auto result = client.options(server.base_url() + "/api/data", headers);
      
      expect(result.has_value() && result->status_code == 204) 
         << "Preflight with custom headers should succeed";
      
      if (result.has_value()) {
         auto headers_it = result->response_headers.find("access-control-allow-headers");
         expect(headers_it != result->response_headers.end()) 
            << "Should have Allow-Headers in response";
      }
      
      server.stop();
   };
   
   "wildcard_path_preflight"_test = [] {
      cors_test_server server;
      expect(server.start(false)) << "Server should start";
      
      // Add wildcard route
      server.get_server().get("/api/files/*path", [](const glz::request& req, glz::response& res) {
         auto path_it = req.params.find("path");
         std::string path = path_it != req.params.end() ? path_it->second : "";
         res.json({{"file", path}});
      });
      
      // Enable CORS after adding route
      server.get_server().enable_cors();
      
      glz::http_client client;
      
      // Test OPTIONS for wildcard path
      std::unordered_map<std::string, std::string> headers;
      headers["Origin"] = "https://example.com";
      headers["Access-Control-Request-Method"] = "GET";
      
      auto result = client.options(server.base_url() + "/api/files/documents/report.pdf", headers);
      
      expect(result.has_value() && result->status_code == 204) 
         << "Preflight for wildcard route should succeed";
      
      server.stop();
   };
   
   "override_auto_generated_options"_test = [] {
      cors_test_server server;
      expect(server.start(false)) << "Server should start";
      
      // Add normal route
      server.get_server().get("/api/special", [](const glz::request&, glz::response& res) {
         res.json({{"data", "special"}});
      });
      
      // Add custom OPTIONS handler
      bool custom_called = false;
      server.get_server().options("/api/special", 
         [&custom_called](const glz::request&, glz::response& res) {
            custom_called = true;
            res.status(204)
               .header("Access-Control-Allow-Origin", "https://special.com")
               .header("Access-Control-Allow-Methods", "GET")
               .header("X-Custom", "true");
         });
      
      // Enable CORS (should not override existing OPTIONS)
      server.get_server().enable_cors();
      
      glz::http_client client;
      
      std::unordered_map<std::string, std::string> headers;
      headers["Origin"] = "https://special.com";
      
      auto result = client.options(server.base_url() + "/api/special", headers);
      
      expect(result.has_value() && result->status_code == 204) << "Custom OPTIONS should work";
      expect(custom_called) << "Custom OPTIONS handler should be called";
      
      if (result.has_value()) {
         auto custom_it = result->response_headers.find("x-custom");
         expect(custom_it != result->response_headers.end()) << "Should have custom header";
      }
      
      server.stop();
   };
};

int main()
{
   // Run all tests
   return 0;
}