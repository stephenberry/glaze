#include "glaze/net/http_client.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <future>
#include <optional>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "glaze/json/write.hpp"
#include "glaze/net/http_server.hpp"
#include "glaze/util/key_transformers.hpp"
#include "ut/ut.hpp"

using namespace ut;
using namespace glz;

namespace test_http_client
{
   struct put_payload
   {
      int value{};
      std::string message{};
   };
}

namespace glz
{
   template <>
   struct meta<test_http_client::put_payload>
   {
      using T = test_http_client::put_payload;
      static constexpr auto value = glz::object("value", &T::value, "message", &T::message);
   };
}

// Simplified test server that actually works
class working_test_server
{
  public:
   working_test_server() : port_(0), running_(false) {}

   ~working_test_server() { stop(); }

   void set_cors_config(glz::cors_config config) { cors_config_ = std::move(config); }

   bool start()
   {
      if (running_) return true;

      setup_routes();

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

  private:
   http_server<> server_;
   std::thread server_thread_;
   uint16_t port_;
   std::atomic<bool> running_;
   std::optional<glz::cors_config> cors_config_;

   void setup_routes()
   {
      // Set up custom error handler to suppress expected shutdown errors
      server_.on_error([this](std::error_code ec, std::source_location loc) {
         // Only log unexpected errors, not normal shutdown errors
         if (running_ && ec != asio::error::eof && ec != asio::error::operation_aborted) {
            std::fprintf(stderr, "Server error at %s:%d: %s\n", loc.file_name(), static_cast<int>(loc.line()),
                         ec.message().c_str());
         }
      });

      if (cors_config_) {
         server_.enable_cors(*cors_config_);
      }
      else {
         server_.enable_cors();
      }

      server_.get("/hello", [](const request&, response& res) {
         res.status(200).content_type("text/plain").body("Hello, World!");
      });

      server_.post(
         "/echo", [](const request& req, response& res) { res.status(200).content_type("text/plain").body(req.body); });

      server_.get("/json", [](const request&, response& res) {
         res.status(200).content_type("application/json").body(R"({"message": "test", "value": 42})");
      });

      server_.post("/json", [](const request& req, response& res) {
         if (req.body.find("\"value\"") != std::string::npos) {
            res.status(200).content_type("application/json").body(R"({"message": "received", "value": 84})");
         }
         else {
            res.status(400).body("Invalid JSON");
         }
      });

      server_.put("/update", [](const request& req, response& res) {
         std::string response_body = "PUT:";
         response_body.append(req.body);
         if (auto it = req.headers.find("x-test-header"); it != req.headers.end()) {
            response_body.append(":");
            response_body.append(it->second);
         }
         res.status(200).content_type("text/plain").body(response_body);
      });

      server_.put("/json", [](const request& req, response& res) {
         auto content_type = req.headers.find("content-type");
         if (content_type == req.headers.end()) {
            res.status(415).body("missing content-type");
            return;
         }

         std::string response_body = "CT=";
         response_body.append(content_type->second);
         response_body.append(";BODY=");
         response_body.append(req.body);
         res.status(200).content_type("text/plain").body(response_body);
      });

      server_.get("/slow", [](const request&, response& res) {
         std::this_thread::sleep_for(std::chrono::milliseconds(50));
         res.status(200).body("Slow response");
      });

      // Add a streaming endpoint for testing the client
      server_.stream_get("/stream-test", [](request&, streaming_response& res) {
         res.start_stream(200, {{"Content-Type", "text/plain"}});
         // The `send` calls are async, so they don't block.
         // They queue writes that the client will receive as a stream.
         res.send("Hello, ");
         res.send("Streaming ");
         res.send("World!");
         res.close();
      });

      // Endpoint that sends data periodically, for testing client disconnect
      server_.stream_get("/slow-stream", [](request&, streaming_response& res) {
         res.start_stream(200, {{"Content-Type", "text/plain"}});
         auto conn = res.stream;

         auto timer = std::make_shared<asio::steady_timer>(conn->socket_->get_executor());
         auto counter = std::make_shared<int>(0);

         // Use shared_ptr to safely handle recursive lambda calls and avoid compiler-specific segfaults
         auto send_data = std::make_shared<std::function<void(const std::error_code&)>>();
         *send_data = [conn, timer, counter, send_data](const std::error_code& ec) {
            if (ec || !conn->is_open() || *counter >= 10) {
               if (conn->is_open()) conn->close();
               return;
            }

            conn->send_chunk("chunk" + std::to_string((*counter)++) + ";",
                             [conn, timer, send_data](std::error_code write_ec) {
                                if (write_ec || !conn->is_open()) {
                                   if (conn->is_open()) conn->close();
                                   return;
                                }
                                timer->expires_after(std::chrono::milliseconds(50));
                                timer->async_wait(*send_data);
                             });
         };
         timer->async_wait(*send_data);
      });

      // Endpoint that immediately returns an error
      server_.stream_get("/stream-error", [](request&, streaming_response& res) {
         res.start_stream(403).close(); // e.g. Forbidden
      });

      // Endpoint that returns HTTP 200 but includes mixed success payloads (Typesense-style)
      server_.stream_get("/stream-typesense", [](request&, streaming_response& res) {
         res.start_stream(200, {{"Content-Type", "application/json"}});
         res.send(R"({"success":false}\n)");
         res.send(R"({"success":true}\n)");
         res.close();
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
         // Ignore connection errors during startup
      }
      return false;
   }
};

// Simplified HTTP client for testing - synchronous only
class simple_test_client
{
  public:
   simple_test_client() : io_context_(1)
   {
      // Start a single worker thread
      worker_thread_ = std::thread([this]() {
         asio::executor_work_guard<asio::io_context::executor_type> work_guard(io_context_.get_executor());
         io_context_.run();
      });
   }

   ~simple_test_client()
   {
      io_context_.stop();
      if (worker_thread_.joinable()) {
         worker_thread_.join();
      }
   }

   std::expected<response, std::error_code> get(const std::string& url)
   {
      auto url_parts = parse_url(url);
      if (!url_parts) {
         return std::unexpected(url_parts.error());
      }

      return perform_request("GET", *url_parts, "");
   }

   std::expected<response, std::error_code> post(const std::string& url, const std::string& body)
   {
      auto url_parts = parse_url(url);
      if (!url_parts) {
         return std::unexpected(url_parts.error());
      }

      return perform_request("POST", *url_parts, body);
   }

   std::expected<response, std::error_code> options(
      const std::string& url, const std::vector<std::pair<std::string, std::string>>& extra_headers)
   {
      auto url_parts = parse_url(url);
      if (!url_parts) {
         return std::unexpected(url_parts.error());
      }

      return perform_request("OPTIONS", *url_parts, "", extra_headers);
   }

  private:
   asio::io_context io_context_;
   std::thread worker_thread_;

   std::expected<response, std::error_code> perform_request(
      const std::string& method, const url_parts& url, const std::string& body,
      std::vector<std::pair<std::string, std::string>> extra_headers = {})
   {
      std::promise<std::expected<response, std::error_code>> promise;
      auto future = promise.get_future();

      asio::post(io_context_, [this, method, url, body, headers = std::move(extra_headers), &promise]() mutable {
         try {
            asio::ip::tcp::socket socket(io_context_);
            asio::ip::tcp::resolver resolver(io_context_);

            // Resolve and connect
            auto endpoints = resolver.resolve(url.host, std::to_string(url.port));
            asio::connect(socket, endpoints);

            // Build request
            std::string request = method + " " + url.path + " HTTP/1.1\r\n";
            request += "Host: " + url.host + "\r\n";
            request += "Connection: close\r\n";

            for (const auto& [name, value] : headers) {
               request += name + ": " + value + "\r\n";
            }

            if (!body.empty()) {
               request += "Content-Length: " + std::to_string(body.size()) + "\r\n";
            }

            request += "\r\n" + body;

            // Send request
            asio::write(socket, asio::buffer(request));

            // Read response
            asio::streambuf response_buffer;
            asio::read_until(socket, response_buffer, "\r\n\r\n");

            // Parse headers
            std::istream response_stream(&response_buffer);
            std::string status_line;
            std::getline(response_stream, status_line);

            if (!status_line.empty() && status_line.back() == '\r') {
               status_line.pop_back();
            }

            auto parsed_status = parse_http_status_line(status_line);
            if (!parsed_status) {
               promise.set_value(std::unexpected(parsed_status.error()));
               return;
            }

            response resp;

            std::string header_line;
            while (std::getline(response_stream, header_line) && header_line != "\r") {
               if (!header_line.empty() && header_line.back() == '\r') {
                  header_line.pop_back();
               }

               auto colon_pos = header_line.find(':');
               if (colon_pos == std::string::npos) continue;

               std::string name = header_line.substr(0, colon_pos);
               std::string value = header_line.substr(colon_pos + 1);

               value.erase(value.begin(), std::find_if(value.begin(), value.end(),
                                                       [](unsigned char ch) { return !std::isspace(ch); }));
               value.erase(
                  std::find_if(value.rbegin(), value.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(),
                  value.end());

               auto lower_name = glz::to_lower_case(name);
               resp.response_headers[lower_name] = value;
            }

            // Read body
            std::error_code ec;
            asio::read(socket, response_buffer, asio::transfer_all(), ec);

            std::string response_body{std::istreambuf_iterator<char>(&response_buffer),
                                      std::istreambuf_iterator<char>()};

            resp.status_code = parsed_status->status_code;
            resp.response_body = response_body;

            promise.set_value(std::move(resp));
         }
         catch (const std::exception& e) {
            promise.set_value(std::unexpected(std::make_error_code(std::errc::connection_refused)));
         }
      });

      return future.get();
   }
};

suite working_http_tests = [] {
   "url_parsing_basic"_test = [] {
      auto result = parse_url("http://example.com/test");
      expect(result.has_value()) << "Basic URL should parse correctly\n";
      expect(result->protocol == "http") << "Protocol should be http\n";
      expect(result->host == "example.com") << "Host should be example.com\n";
      expect(result->port == 80) << "Port should default to 80\n";
      expect(result->path == "/test") << "Path should be /test\n";
   };

   "simple_server_test"_test = [] {
      working_test_server server;

      expect(server.start()) << "Test server should start successfully\n";
      expect(server.port() > 0) << "Server should have valid port\n";

      // Give server a moment to fully initialize
      std::this_thread::sleep_for(std::chrono::milliseconds(100));

      server.stop();
   };

   "basic_get_request"_test = [] {
      working_test_server server;
      expect(server.start()) << "Server should start\n";

      simple_test_client client;
      auto result = client.get(server.base_url() + "/hello");

      expect(result.has_value()) << "GET request should succeed\n";
      if (result.has_value()) {
         expect(result->status_code == 200) << "Status should be 200\n";
         expect(result->response_body == "Hello, World!") << "Body should match\n";
      }

      server.stop();
      std::this_thread::sleep_for(std::chrono::milliseconds(50)); // Clean shutdown
   };

   "cors_preflight_generates_options_response"_test = [] {
      working_test_server server;
      expect(server.start()) << "Server should start\n";

      simple_test_client client;
      std::vector<std::pair<std::string, std::string>> headers = {
         {"Origin", "http://localhost"},
         {"Access-Control-Request-Method", "GET"},
         {"Access-Control-Request-Headers", "X-Test-Header"},
      };

      auto result = client.options(server.base_url() + "/hello", headers);

      expect(result.has_value()) << "OPTIONS preflight should succeed\n";
      if (result.has_value()) {
         expect(result->status_code == 204) << "Preflight should return HTTP 204\n";
      }

      server.stop();
      std::this_thread::sleep_for(std::chrono::milliseconds(50)); // Clean shutdown
   };

   "cors_dynamic_origin_validation"_test = [] {
      glz::cors_config config;
      config.allowed_origins.clear();
      config.allowed_origins_validator = [](std::string_view origin) {
         return (origin.starts_with("http://") && origin.ends_with(".allowed.local")) ||
                origin == "http://special.local";
      };

      working_test_server server;
      server.set_cors_config(config);
      expect(server.start()) << "Server should start\n";

      simple_test_client client;
      std::vector<std::pair<std::string, std::string>> headers = {
         {"Origin", "http://app.allowed.local"},
         {"Access-Control-Request-Method", "GET"},
      };
      auto allowed = client.options(server.base_url() + "/hello", headers);
      expect(allowed.has_value()) << "OPTIONS preflight should succeed\n";
      if (allowed.has_value()) {
         expect(allowed->status_code == 204) << "Default status should remain 204\n";
         auto origin_header = allowed->response_headers.find("access-control-allow-origin");
         expect(origin_header != allowed->response_headers.end()) << "Allow-Origin header should be present\n";
         if (origin_header != allowed->response_headers.end()) {
            expect(origin_header->second == "http://app.allowed.local")
               << "Origin should be echoed for allowed pattern\n";
         }
      }

      headers[0].second = "http://special.local";
      auto allowed_callback = client.options(server.base_url() + "/hello", headers);
      expect(allowed_callback.has_value()) << "Dynamic callback origin should succeed\n";
      if (allowed_callback.has_value()) {
         expect(allowed_callback->status_code == 204);
      }

      headers[0].second = "http://denied.local";
      auto denied = client.options(server.base_url() + "/hello", headers);
      expect(denied.has_value()) << "Request should return a response even when denied\n";
      if (denied.has_value()) {
         expect(denied->status_code == 403) << "Denied origin should return 403\n";
      }

      server.stop();
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
   };

   "cors_reflects_headers"_test = [] {
      glz::cors_config config;
      config.allowed_origins = {"http://client.local"};
      config.allowed_methods = {"GET", "HEAD", "POST", "PUT", "DELETE", "PATCH"};
      config.allowed_headers.clear();
      config.options_success_status = 200;
      config.max_age = 123;

      working_test_server server;
      server.set_cors_config(config);
      expect(server.start()) << "Server should start\n";

      simple_test_client client;
      auto result = client.options(server.base_url() + "/hello", {{"Origin", "http://client.local"},
                                                                  {"Access-Control-Request-Method", "GET"},
                                                                  {"Access-Control-Request-Headers", "X-Test-Header"}});

      expect(result.has_value()) << "OPTIONS preflight should succeed\n";
      if (result.has_value()) {
         expect(result->status_code == 200)
            << "Configured OPTIONS status should be used (got " << result->status_code << ")\n";

         auto methods_it = result->response_headers.find("access-control-allow-methods");
         expect(methods_it != result->response_headers.end()) << "Allow-Methods header missing\n";
         if (methods_it != result->response_headers.end()) {
            expect(methods_it->second == "GET, HEAD, POST, PUT, DELETE, PATCH");
         }

         auto headers_it = result->response_headers.find("access-control-allow-headers");
         expect(headers_it != result->response_headers.end());
         if (headers_it != result->response_headers.end()) {
            expect(headers_it->second == "X-Test-Header");
         }

         auto max_age_it = result->response_headers.find("access-control-max-age");
         expect(max_age_it != result->response_headers.end());
         if (max_age_it != result->response_headers.end()) {
            expect(max_age_it->second == "123");
         }
      }

      server.stop();
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
   };

   "cors_allow_all_headers_flag"_test = [] {
      glz::cors_config config;
      config.allowed_origins = {"http://client.local"};
      config.allowed_methods = {"*"};
      config.allowed_headers = {"*"};

      working_test_server server;
      server.set_cors_config(config);
      expect(server.start()) << "Server should start\n";

      simple_test_client client;
      auto result = client.options(server.base_url() + "/hello", {{"Origin", "http://client.local"},
                                                                  {"Access-Control-Request-Method", "DELETE"},
                                                                  {"Access-Control-Request-Headers", "X-One"}});

      expect(result.has_value());
      if (result.has_value()) {
         auto headers_it = result->response_headers.find("access-control-allow-headers");
         expect(headers_it != result->response_headers.end());
         if (headers_it != result->response_headers.end()) {
            expect(headers_it->second == "*") << "Expected * but got " << headers_it->second << "\n";
         }
      }

      server.stop();
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
   };

   "cors_preflight_rejects_missing_method"_test = [] {
      working_test_server server;
      expect(server.start()) << "Server should start\n";

      simple_test_client client;
      auto result = client.options(server.base_url() + "/hello",
                                   {{"Origin", "http://client.local"}, {"Access-Control-Request-Method", "POST"}});

      expect(result.has_value()) << "Preflight request should yield a response\n";
      if (result.has_value()) {
         expect(result->status_code == 405)
            << "Preflight should return 405 when requested method is not implemented (got " << result->status_code
            << ")\n";

         auto allow_it = result->response_headers.find("allow");
         expect(allow_it != result->response_headers.end()) << "Allow header must be present\n";
         if (allow_it != result->response_headers.end()) {
            expect(allow_it->second.find("GET") != std::string::npos)
               << "Allow header should list the implemented method\n";
         }
      }

      server.stop();
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
   };

   "cors_wildcard_with_credentials_echoes_origin"_test = [] {
      glz::cors_config config;
      config.allowed_origins = {"*"};
      config.allow_credentials = true;
      config.allowed_methods = {"GET", "HEAD", "POST", "PUT", "DELETE", "PATCH"};

      working_test_server server;
      server.set_cors_config(config);
      expect(server.start()) << "Server should start\n";

      simple_test_client client;
      auto result = client.options(server.base_url() + "/hello",
                                   {{"Origin", "http://auth.local"}, {"Access-Control-Request-Method", "POST"}});

      expect(result.has_value());
      if (result.has_value()) {
         auto origin_it = result->response_headers.find("access-control-allow-origin");
         expect(origin_it != result->response_headers.end());
         if (origin_it != result->response_headers.end()) {
            expect(origin_it->second == "http://auth.local");
         }

         auto credentials_it = result->response_headers.find("access-control-allow-credentials");
         expect(credentials_it != result->response_headers.end());
      }

      server.stop();
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
   };

   "basic_post_request"_test = [] {
      working_test_server server;
      expect(server.start()) << "Server should start\n";

      simple_test_client client;
      std::string test_body = "Test message";
      auto result = client.post(server.base_url() + "/echo", test_body);

      expect(result.has_value()) << "POST request should succeed\n";
      if (result.has_value()) {
         expect(result->status_code == 200) << "Status should be 200\n";
         expect(result->response_body == test_body) << "Body should echo input\n";
      }

      server.stop();
      std::this_thread::sleep_for(std::chrono::milliseconds(50)); // Clean shutdown
   };

   "multiple_requests"_test = [] {
      working_test_server server;
      expect(server.start()) << "Server should start\n";

      simple_test_client client;

      // Make multiple requests to test connection handling
      for (int i = 0; i < 3; ++i) {
         auto result = client.get(server.base_url() + "/hello");
         expect(result.has_value()) << "Request " << i << " should succeed\n";
         if (result.has_value()) {
            expect(result->status_code == 200) << "Status should be 200\n";
         }
      }

      server.stop();
      std::this_thread::sleep_for(std::chrono::milliseconds(50)); // Clean shutdown
   };

   "json_response"_test = [] {
      working_test_server server;
      expect(server.start()) << "Server should start\n";

      simple_test_client client;
      auto result = client.get(server.base_url() + "/json");

      expect(result.has_value()) << "JSON GET should succeed\n";
      if (result.has_value()) {
         expect(result->status_code == 200) << "Status should be 200\n";
         expect(result->response_body.find("\"message\"") != std::string::npos) << "Response should contain JSON\n";
      }

      server.stop();
      std::this_thread::sleep_for(std::chrono::milliseconds(50)); // Clean shutdown
   };

   "error_handling"_test = [] {
      simple_test_client client;

      // Test connection to non-existent server
      auto result = client.get("http://127.0.0.1:19999/test");
      expect(!result.has_value()) << "Connection to closed port should fail\n";
   };

   "http_status_error_category"_test = [] {
      auto ec = make_http_status_error(502);

      expect(ec.category() == http_status_category());
      expect(ec.value() == 502);
      expect(!ec.message().empty());
      expect(ec.message().find("502") != std::string::npos);
   };

   "concurrent_server_requests"_test = [] {
      working_test_server server;
      expect(server.start()) << "Server should start\n";

      const int num_threads = 3;
      std::vector<std::thread> threads;
      std::atomic<int> success_count{0};

      for (int i = 0; i < num_threads; ++i) {
         threads.emplace_back([&server, &success_count]() {
            simple_test_client client;
            auto result = client.get(server.base_url() + "/hello");
            if (result.has_value() && result->status_code == 200) {
               success_count++;
            }
         });
      }

      for (auto& t : threads) {
         t.join();
      }

      expect(success_count == num_threads) << "All concurrent requests should succeed\n";

      server.stop();
      std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Extra time for concurrent cleanup
   };
};

// Test suite for the main glz::http_client, including streaming
suite glz_http_client_tests = [] {
   "synchronous_put_request"_test = [] {
      working_test_server server;
      expect(server.start());

      glz::http_client client;

      std::unordered_map<std::string, std::string> headers{{"x-test-header", "header-value"}};
      auto result = client.put(server.base_url() + "/update", "payload", headers);

      expect(result.has_value()) << "PUT request should succeed";
      if (result.has_value()) {
         expect(result->status_code == 200) << "PUT status should be 200";
         expect(result->response_body == "PUT:payload:header-value") << "Response body should echo payload and header";
      }

      server.stop();
   };

   "put_json_sets_content_type"_test = [] {
      working_test_server server;
      expect(server.start());

      glz::http_client client;

      test_http_client::put_payload payload{.value = 42, .message = "update"};

      std::string expected_json;
      auto ec = glz::write_json(payload, expected_json);
      expect(!ec) << "Serializing payload should succeed";

      std::unordered_map<std::string, std::string> extra_headers{{"x-extra", "value"}};
      auto result = client.put_json(server.base_url() + "/json", payload, extra_headers);

      expect(result.has_value()) << "PUT JSON request should succeed";
      if (result.has_value()) {
         expect(result->status_code == 200) << "PUT JSON status should be 200";
         expect(result->response_body.find("CT=application/json") != std::string::npos)
            << "Content-Type header should be forwarded";
         expect(result->response_body.find("BODY=" + expected_json) != std::string::npos)
            << "JSON body should be forwarded";
      }

      server.stop();
   };

   "basic_streaming_get"_test = [] {
      working_test_server server;
      expect(server.start()) << "Server should start\n";

      glz::http_client client;

      std::string received_data;
      std::mutex data_mutex;
      std::atomic<bool> connected = false;
      std::atomic<int> error_count = 0;
      std::promise<void> disconnect_promise;
      auto disconnect_future = disconnect_promise.get_future();

      auto on_data = [&](std::string_view data) {
         std::lock_guard lock(data_mutex);
         received_data.append(data);
      };
      auto on_error = [&](std::error_code ec) {
         if (ec && ec != asio::error::eof && ec != asio::error::operation_aborted) {
            error_count++;
         }
      };
      auto on_connect = [&](const response& headers) {
         expect(headers.status_code == 200);
         connected = true;
      };
      auto on_disconnect = [&]() { disconnect_promise.set_value(); };

      auto conn = client.stream_request({.url = server.base_url() + "/stream-test",
                                         .on_data = on_data,
                                         .on_error = on_error,
                                         .method = "GET",
                                         .on_connect = on_connect,
                                         .on_disconnect = on_disconnect});
      expect(conn != nullptr) << "Connection handle should not be null\n";

      // Wait for the stream to complete
      auto status = disconnect_future.wait_for(std::chrono::seconds(2));
      expect(status == std::future_status::ready) << "Stream did not disconnect in time\n";

      expect(connected == true) << "on_connect was not called";
      expect(error_count == 0) << "on_error was called unexpectedly";

      std::string final_data;
      {
         std::lock_guard lock(data_mutex);
         final_data = received_data;
      }
      expect(final_data == "Hello, Streaming World!") << "Received data mismatch. Got: " << final_data;

      server.stop();
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
   };

   "client_disconnects_stream"_test = [] {
      working_test_server server;
      expect(server.start()) << "Server should start\n";

      glz::http_client client;

      std::string received_data;
      std::mutex data_mutex;
      std::atomic<bool> connected = false;
      std::atomic<int> error_count = 0;
      std::promise<void> disconnect_promise;
      auto disconnect_future = disconnect_promise.get_future();
      std::atomic<int> data_chunks_received = 0;

      auto on_data = [&](std::string_view data) {
         std::lock_guard lock(data_mutex);
         received_data.append(data);
         data_chunks_received++;
      };
      auto on_error = [&](std::error_code ec) {
         if (ec && ec != asio::error::eof && ec != asio::error::operation_aborted) {
            error_count++;
         }
      };
      auto on_connect = [&](const response& headers) {
         expect(headers.status_code == 200);
         connected = true;
      };
      auto on_disconnect = [&]() { disconnect_promise.set_value(); };

      auto conn = client.stream_request({.url = server.base_url() + "/slow-stream",
                                         .on_data = on_data,
                                         .on_error = on_error,
                                         .method = "GET",
                                         .on_connect = on_connect,
                                         .on_disconnect = on_disconnect});
      expect(conn != nullptr) << "Connection handle should not be null\n";

      // Wait until we have received at least one chunk to ensure the stream is active
      while (data_chunks_received.load() < 2) {
         std::this_thread::sleep_for(std::chrono::milliseconds(10));
         if (disconnect_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            break; // Exit if disconnected prematurely
         }
      }

      // Now, disconnect the stream from the client-side
      conn->disconnect();

      // Wait for the on_disconnect handler to be called
      auto status = disconnect_future.wait_for(std::chrono::seconds(2));
      expect(status == std::future_status::ready) << "Stream did not disconnect in time\n";

      expect(connected == true) << "on_connect was not called";
      expect(error_count == 0) << "on_error was called unexpectedly";

      // We should have received some data, but not all 10 chunks
      int chunks = data_chunks_received.load();
      expect(chunks > 0 && chunks < 10) << "Should receive some but not all data chunks. Received: " << chunks << "\n";

      server.stop();
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
   };

   "streaming_request_with_http_error"_test = [] {
      working_test_server server;
      expect(server.start()) << "Server should start\n";

      glz::http_client client;

      std::atomic<bool> connected = false;
      std::atomic<bool> data_received = false;
      std::atomic<bool> error_received = false;
      std::promise<void> disconnect_promise;
      auto disconnect_future = disconnect_promise.get_future();

      auto on_data = [&](std::string_view) { data_received = true; };
      auto on_error = [&](std::error_code ec) {
         expect(bool(ec)) << "Expected an error code for HTTP failure";

         auto status = http_status_from(ec);
         expect(status.has_value()) << "Error should expose HTTP status";
         if (status) {
            expect(*status == 403) << "Unexpected HTTP status propagated: " << ec.message();
         }

         error_received = true;
      };
      auto on_connect = [&](const response& headers) {
         // on_connect is called as soon as headers are parsed, even error ones.
         expect(headers.status_code == 403);
         connected = true;
      };
      auto on_disconnect = [&]() { disconnect_promise.set_value(); };

      auto conn = client.stream_request({.url = server.base_url() + "/stream-error",
                                         .on_data = on_data,
                                         .on_error = on_error,
                                         .method = "GET",
                                         .on_connect = on_connect,
                                         .on_disconnect = on_disconnect});
      expect(conn != nullptr);

      // Wait for the interaction to complete
      auto status = disconnect_future.wait_for(std::chrono::seconds(2));
      expect(status == std::future_status::ready) << "Disconnect was not called on error\n";

      expect(connected == true) << "on_connect should be called with error headers\n";
      expect(error_received == true) << "on_error was not called for HTTP error status\n";
      expect(data_received == false) << "on_data should not be called on error\n";

      server.stop();
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
   };

   "streaming_request_with_custom_status_predicate"_test = [] {
      working_test_server server;
      expect(server.start()) << "Server should start\n";

      glz::http_client client;

      std::atomic<bool> connected = false;
      std::atomic<bool> error_received = false;
      std::promise<void> disconnect_promise;
      auto disconnect_future = disconnect_promise.get_future();

      auto on_data = [&](std::string_view data) {
         // Capture any payload for later inspection
         (void)data;
      };
      auto on_error = [&](std::error_code) { error_received = true; };
      auto on_connect = [&](const response& headers) {
         expect(headers.status_code == 403);
         connected = true;
      };
      auto on_disconnect = [&]() { disconnect_promise.set_value(); };

      auto conn = client.stream_request({.url = server.base_url() + "/stream-error",
                                         .on_data = on_data,
                                         .on_error = on_error,
                                         .method = "GET",
                                         .on_connect = on_connect,
                                         .on_disconnect = on_disconnect,
                                         .status_is_error = [](int status) { return status >= 500; }});
      expect(conn != nullptr);

      auto status = disconnect_future.wait_for(std::chrono::seconds(2));
      expect(status == std::future_status::ready) << "Disconnect was not called\n";

      expect(connected == true) << "on_connect should run";
      expect(error_received == false) << "Custom predicate should suppress 4xx error";

      server.stop();
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
   };

   "streaming_request_custom_predicate_flags_success"_test = [] {
      working_test_server server;
      expect(server.start()) << "Server should start\n";

      glz::http_client client;

      std::atomic<bool> error_received = false;
      std::promise<void> disconnect_promise;
      auto disconnect_future = disconnect_promise.get_future();

      auto on_data = [&](std::string_view) { /* no-op */ };
      auto on_error = [&](std::error_code ec) {
         error_received = true;
         auto status = http_status_from(ec);
         expect(status.has_value());
         if (status) {
            expect(*status == 200);
         }
      };
      auto on_connect = [&](const response& headers) { expect(headers.status_code == 200); };
      auto on_disconnect = [&]() { disconnect_promise.set_value(); };

      auto conn = client.stream_request({.url = server.base_url() + "/stream-typesense",
                                         .on_data = on_data,
                                         .on_error = on_error,
                                         .method = "GET",
                                         .on_connect = on_connect,
                                         .on_disconnect = on_disconnect,
                                         .status_is_error = [](int status) { return status == 200; }});
      expect(conn != nullptr);

      auto status = disconnect_future.wait_for(std::chrono::seconds(2));
      expect(status == std::future_status::ready);
      expect(error_received == true);

      server.stop();
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
   };
};

int main()
{
   std::cout << "Running HTTP Client Tests...\n";
   std::cout << "============================\n";
   return 0;
}
