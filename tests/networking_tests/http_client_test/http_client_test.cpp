#include "glaze/net/http_client.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <future>
#include <initializer_list>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <string_view>
#include <system_error>
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
   struct json_payload
   {
      int value{};
      std::string message{};
   };
}

namespace glz
{
   template <>
   struct meta<test_http_client::json_payload>
   {
      using T = test_http_client::json_payload;
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
         server_.bind("127.0.0.1", 0);
         port_ = server_.port();

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
         if (running_ && ec != make_error_code(asio::error::eof) &&
             ec != make_error_code(asio::error::operation_aborted)) {
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
            response_body.append(it->value);
         }
         res.status(200).content_type("text/plain").body(response_body);
      });

      server_.put("/json", [](const request& req, response& res) {
         auto content_type = req.headers.first_value("content-type");
         if (!content_type) {
            res.status(415).body("missing content-type");
            return;
         }

         std::string response_body = "CT=";
         response_body.append(*content_type);
         response_body.append(";CT_COUNT=");
         response_body.append(std::to_string(req.headers.count("content-type")));
         response_body.append(";BODY=");
         response_body.append(req.body);
         res.status(200).content_type("text/plain").body(response_body);
      });

      server_.patch("/patch", [](const request& req, response& res) {
         std::string response_body = "PATCH:";
         response_body.append(req.body);
         if (auto it = req.headers.find("x-test-header"); it != req.headers.end()) {
            response_body.append(":");
            response_body.append(it->value);
         }
         res.status(200).content_type("text/plain").body(response_body);
      });

      server_.patch("/json", [](const request& req, response& res) {
         auto content_type = req.headers.first_value("content-type");
         if (!content_type) {
            res.status(415).body("missing content-type");
            return;
         }

         std::string response_body = "CT=";
         response_body.append(*content_type);
         response_body.append(";CT_COUNT=");
         response_body.append(std::to_string(req.headers.count("content-type")));
         response_body.append(";BODY=");
         response_body.append(req.body);
         res.status(200).content_type("text/plain").body(response_body);
      });

      server_.del("/items/:id", [](const request& req, response& res) {
         std::string response_body = "DELETE:";
         response_body.append(req.params.at("id"));
         if (auto it = req.headers.find("x-test-header"); it != req.headers.end()) {
            response_body.append(":");
            response_body.append(it->value);
         }
         res.status(200).content_type("text/plain").body(response_body);
      });

      // The server frames a HEAD reply and a 304 with the Content-Length of the body built
      // here but sends no body, which the client must not wait for.
      server_.route(http_method::HEAD, "/head",
                    [](const request&, response& res) { res.status(200).content_type("text/plain").body("hello"); });

      server_.get("/not-modified", [](const request&, response& res) { res.status(304).body("cached body"); });

      server_.get("/slow", [](const request&, response& res) {
         std::this_thread::sleep_for(std::chrono::milliseconds(50));
         res.status(200).body("Slow response");
      });

      server_.get("/files/*path", [](const request& req, response& res) {
         auto path_it = req.params.find("path");
         std::string captured = path_it != req.params.end() ? path_it->second : "";
         res.status(200).content_type("text/plain").body(captured);
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
      // Uses enable_shared_from_this to avoid circular reference memory leaks
      server_.stream_get("/slow-stream", [](request&, streaming_response& res) {
         res.start_stream(200, {{"Content-Type", "text/plain"}});

         struct slow_stream_handler : std::enable_shared_from_this<slow_stream_handler>
         {
            std::shared_ptr<streaming_connection_interface> conn;
            std::shared_ptr<asio::steady_timer> timer;
            int counter = 0;

            explicit slow_stream_handler(std::shared_ptr<streaming_connection_interface> c)
               : conn(std::move(c)), timer(std::make_shared<asio::steady_timer>(conn->get_executor()))
            {}

            void start()
            {
               timer->async_wait([self = shared_from_this()](const std::error_code& ec) { self->on_timer(ec); });
            }

            void on_timer(const std::error_code& ec)
            {
               if (ec || !conn->is_open() || counter >= 10) {
                  if (conn->is_open()) conn->close();
                  return;
               }

               conn->send_chunk("chunk" + std::to_string(counter++) + ";",
                                [self = shared_from_this()](std::error_code write_ec) { self->on_write(write_ec); });
            }

            void on_write(std::error_code write_ec)
            {
               if (write_ec || !conn->is_open()) {
                  if (conn->is_open()) conn->close();
                  return;
               }
               timer->expires_after(std::chrono::milliseconds(50));
               timer->async_wait([self = shared_from_this()](const std::error_code& ec) { self->on_timer(ec); });
            }
         };

         auto handler = std::make_shared<slow_stream_handler>(res.stream);
         handler->start();
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

         asio::error_code ec;
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

   std::expected<response, std::error_code> options(const std::string& url, const glz::http_headers& extra_headers)
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

   std::expected<response, std::error_code> perform_request(const std::string& method, const url_parts& url,
                                                            const std::string& body,
                                                            glz::http_headers extra_headers = {})
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

               resp.response_headers.add(std::move(name), std::move(value));
            }

            // Read body
            asio::error_code ec;
            asio::read(socket, response_buffer, asio::transfer_all(), ec);

            std::string response_body{std::istreambuf_iterator<char>(&response_buffer),
                                      std::istreambuf_iterator<char>()};

            resp.status_code = parsed_status->status_code;
            resp.response_body = response_body;

            promise.set_value(std::move(resp));
         }
         catch (const std::exception&) {
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
      glz::http_headers headers = {
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
      glz::http_headers headers = {
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
            expect(origin_header->value == "http://app.allowed.local")
               << "Origin should be echoed for allowed pattern\n";
         }
      }

      headers.set("Origin", "http://special.local");
      auto allowed_callback = client.options(server.base_url() + "/hello", headers);
      expect(allowed_callback.has_value()) << "Dynamic callback origin should succeed\n";
      if (allowed_callback.has_value()) {
         expect(allowed_callback->status_code == 204);
      }

      headers.set("Origin", "http://denied.local");
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
            expect(methods_it->value == "GET, HEAD, POST, PUT, DELETE, PATCH");
         }

         auto headers_it = result->response_headers.find("access-control-allow-headers");
         expect(headers_it != result->response_headers.end());
         if (headers_it != result->response_headers.end()) {
            expect(headers_it->value == "X-Test-Header");
         }

         auto max_age_it = result->response_headers.find("access-control-max-age");
         expect(max_age_it != result->response_headers.end());
         if (max_age_it != result->response_headers.end()) {
            expect(max_age_it->value == "123");
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
            expect(headers_it->value == "*") << "Expected * but got " << headers_it->value << "\n";
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
            expect(allow_it->value.find("GET") != std::string::npos)
               << "Allow header should list the implemented method\n";
         }
      }

      server.stop();
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
   };

   "cors_wildcard_with_credentials_rejects_origin"_test = [] {
      // A wildcard origin list with credentials enabled must not reflect the request
      // origin. Echoing the origin next to Access-Control-Allow-Credentials: true would
      // let any site read authenticated cross-origin responses, so an unlisted origin is
      // rejected and no Allow-Origin/Allow-Credentials headers are emitted.
      glz::cors_config config;
      config.allowed_origins = {"*"};
      config.allow_credentials = true;

      working_test_server server;
      server.set_cors_config(config);
      expect(server.start()) << "Server should start\n";

      simple_test_client client;
      auto result = client.options(server.base_url() + "/hello",
                                   {{"Origin", "http://auth.local"}, {"Access-Control-Request-Method", "GET"}});

      expect(result.has_value());
      if (result.has_value()) {
         expect(result->status_code == 403)
            << "Wildcard + credentials should reject an unlisted origin (got " << result->status_code << ")\n";

         expect(result->response_headers.find("access-control-allow-origin") == result->response_headers.end())
            << "Allow-Origin must not be echoed for a wildcard + credentials config\n";

         expect(result->response_headers.find("access-control-allow-credentials") == result->response_headers.end())
            << "Allow-Credentials must not be sent for a rejected origin\n";
      }

      server.stop();
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
   };

   "cors_preflight_then_actual_request"_test = [] {
      working_test_server server;
      expect(server.start()) << "Server should start\n";

      simple_test_client client;

      // Step 1: preflight for the subsequent POST.
      auto preflight =
         client.options(server.base_url() + "/echo", {{"Origin", "http://app.local"},
                                                      {"Access-Control-Request-Method", "POST"},
                                                      {"Access-Control-Request-Headers", "Content-Type"}});

      expect(preflight.has_value()) << "Preflight should succeed\n";
      if (preflight.has_value()) {
         expect(preflight->status_code == 204) << "Preflight should return 204\n";
      }

      // Step 2: the actual request should still work after preflight (no shared state corruption).
      std::string body = "follow-up payload";
      auto actual = client.post(server.base_url() + "/echo", body);

      expect(actual.has_value()) << "Follow-up POST should succeed\n";
      if (actual.has_value()) {
         expect(actual->status_code == 200) << "POST should return 200\n";
         expect(actual->response_body == body) << "POST should echo body\n";
      }

      server.stop();
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
   };

   "cors_preflight_for_put_delete_patch"_test = [] {
      working_test_server server;
      expect(server.start()) << "Server should start\n";

      simple_test_client client;

      // PUT is registered on /update.
      auto put_result = client.options(server.base_url() + "/update",
                                       {{"Origin", "http://app.local"}, {"Access-Control-Request-Method", "PUT"}});
      expect(put_result.has_value()) << "PUT preflight should succeed\n";
      if (put_result.has_value()) {
         expect(put_result->status_code == 204) << "PUT preflight should return 204\n";
         auto allow_it = put_result->response_headers.find("allow");
         expect(allow_it != put_result->response_headers.end()) << "Allow header should be present\n";
         if (allow_it != put_result->response_headers.end()) {
            expect(allow_it->value.find("PUT") != std::string::npos) << "Allow should advertise PUT\n";
         }
      }

      // /update has no DELETE handler, so a DELETE preflight should return 405.
      auto delete_result = client.options(
         server.base_url() + "/update", {{"Origin", "http://app.local"}, {"Access-Control-Request-Method", "DELETE"}});
      expect(delete_result.has_value()) << "DELETE preflight should yield a response\n";
      if (delete_result.has_value()) {
         expect(delete_result->status_code == 405)
            << "DELETE preflight should return 405 when DELETE is not implemented\n";
      }

      // PATCH on /update is also not implemented; same expectation.
      auto patch_result = client.options(server.base_url() + "/update",
                                         {{"Origin", "http://app.local"}, {"Access-Control-Request-Method", "PATCH"}});
      expect(patch_result.has_value()) << "PATCH preflight should yield a response\n";
      if (patch_result.has_value()) {
         expect(patch_result->status_code == 405)
            << "PATCH preflight should return 405 when PATCH is not implemented\n";
      }

      server.stop();
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
   };

   // RFC 9110 15.5.6: a known path requested with a method it has no route for answers
   // 405 with an Allow header listing the methods it does have; an unknown path stays 404.
   "wrong_method_returns_405_with_allow"_test = [] {
      working_test_server server;
      expect(server.start()) << "Server should start\n";

      simple_test_client client;

      auto expect_405 = [](const auto& result, std::string_view expected_allow) {
         expect(result.has_value()) << "Request should yield a response\n";
         if (result.has_value()) {
            expect(result->status_code == 405) << "Expected 405, got " << result->status_code << "\n";
            auto allow_it = result->response_headers.find("allow");
            expect(allow_it != result->response_headers.end()) << "Allow header should be present\n";
            if (allow_it != result->response_headers.end()) {
               expect(allow_it->value == expected_allow) << "Allow was: " << allow_it->value << "\n";
            }
         }
      };

      // A path with a GET route answers HEAD too (RFC 9110 9.3.2), so Allow lists both.
      expect_405(client.post(server.base_url() + "/hello", "body"), "GET, HEAD, OPTIONS");
      expect_405(client.get(server.base_url() + "/update?x=1"), "PUT, OPTIONS");
      expect_405(client.post(server.base_url() + "/files/a/b.txt", "body"), "GET, HEAD, OPTIONS");

      auto missing = client.get(server.base_url() + "/no-such-path");
      expect(missing.has_value()) << "Request should yield a response\n";
      if (missing.has_value()) {
         expect(missing->status_code == 404) << "Unknown path should stay 404\n";
         expect(missing->response_headers.find("allow") == missing->response_headers.end())
            << "404 should not carry an Allow header\n";
      }

      server.stop();
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
   };

   "cors_full_config_combination"_test = [] {
      // Non-wildcard origin list + credentials + custom max_age + restricted methods.
      // Existing tests cover these settings individually; this one exercises them together
      // and verifies the non-wildcard branch of the allow_credentials code path.
      glz::cors_config config;
      config.allowed_origins = {"https://app.local", "https://admin.local"};
      config.allowed_methods = {"GET", "POST"};
      config.allowed_headers = {"Content-Type", "Authorization"};
      config.allow_credentials = true;
      config.max_age = 7200;

      working_test_server server;
      server.set_cors_config(config);
      expect(server.start()) << "Server should start\n";

      simple_test_client client;
      // Use /echo which has POST registered. Hitting /hello (GET-only) with
      // Access-Control-Request-Method: POST would let the server's preflight
      // intelligence return 405 (method not implemented on this path) and
      // mask the CORS-layer behavior we want to verify here.
      auto result = client.options(server.base_url() + "/echo",
                                   {{"Origin", "https://app.local"},
                                    {"Access-Control-Request-Method", "POST"},
                                    {"Access-Control-Request-Headers", "Content-Type, Authorization"}});

      expect(result.has_value()) << "Preflight should succeed\n";
      if (result.has_value()) {
         expect(result->status_code == 204) << "Preflight should return 204\n";

         auto origin_it = result->response_headers.find("access-control-allow-origin");
         expect(origin_it != result->response_headers.end()) << "Allow-Origin header missing\n";
         if (origin_it != result->response_headers.end()) {
            expect(origin_it->value == "https://app.local")
               << "Allow-Origin should echo the specific origin, not '*', when credentials are allowed\n";
         }

         auto credentials_it = result->response_headers.find("access-control-allow-credentials");
         expect(credentials_it != result->response_headers.end()) << "Allow-Credentials should be present\n";
         if (credentials_it != result->response_headers.end()) {
            expect(credentials_it->value == "true");
         }

         auto methods_it = result->response_headers.find("access-control-allow-methods");
         expect(methods_it != result->response_headers.end()) << "Allow-Methods should be present\n";
         if (methods_it != result->response_headers.end()) {
            expect(methods_it->value == "GET, POST");
         }

         auto max_age_it = result->response_headers.find("access-control-max-age");
         expect(max_age_it != result->response_headers.end()) << "Max-Age should be present\n";
         if (max_age_it != result->response_headers.end()) {
            expect(max_age_it->value == "7200");
         }
      }

      // A disallowed origin with the same config should be rejected.
      auto denied = client.options(server.base_url() + "/echo",
                                   {{"Origin", "https://evil.local"}, {"Access-Control-Request-Method", "POST"}});
      expect(denied.has_value()) << "Denied preflight should still yield a response\n";
      if (denied.has_value()) {
         expect(denied->status_code == 403) << "Disallowed origin should return 403\n";
      }

      server.stop();
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
   };

   "cors_concurrent_preflight_requests"_test = [] {
      working_test_server server;
      expect(server.start()) << "Server should start\n";

      constexpr int num_threads = 5;
      std::vector<std::thread> threads;
      std::atomic<int> success_count{0};

      for (int i = 0; i < num_threads; ++i) {
         threads.emplace_back([&server, &success_count, i]() {
            simple_test_client client;
            auto result = client.options(
               server.base_url() + "/hello",
               {{"Origin", "http://client" + std::to_string(i) + ".local"}, {"Access-Control-Request-Method", "GET"}});
            if (result.has_value() && result->status_code == 204) {
               ++success_count;
            }
         });
      }

      for (auto& t : threads) t.join();

      expect(success_count.load() == num_threads) << "All concurrent preflight requests should return 204\n";

      server.stop();
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
   };

   "cors_preflight_on_wildcard_route"_test = [] {
      working_test_server server;
      expect(server.start()) << "Server should start\n";

      simple_test_client client;
      auto result = client.options(server.base_url() + "/files/documents/report.pdf",
                                   {{"Origin", "http://app.local"}, {"Access-Control-Request-Method", "GET"}});

      expect(result.has_value()) << "Wildcard-route preflight should succeed\n";
      if (result.has_value()) {
         expect(result->status_code == 204) << "Wildcard preflight should return 204\n";

         auto origin_it = result->response_headers.find("access-control-allow-origin");
         expect(origin_it != result->response_headers.end()) << "Allow-Origin should be set for wildcard paths\n";
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

      glz::http_headers headers{{"x-test-header", "header-value"}};
      auto result = client.put(server.base_url() + "/update", "payload", headers);

      expect(result.has_value()) << "PUT request should succeed";
      if (result.has_value()) {
         expect(result->status_code == 200) << "PUT status should be 200";
         expect(result->response_body == "PUT:payload:header-value") << "Response body should echo payload and header";
      }

      server.stop();
   };

   "synchronous_patch_request"_test = [] {
      working_test_server server;
      expect(server.start());

      glz::http_client client;

      glz::http_headers headers{{"x-test-header", "header-value"}};
      auto result = client.patch(server.base_url() + "/patch", "payload", headers);

      expect(result.has_value()) << "PATCH request should succeed";
      if (result.has_value()) {
         expect(result->status_code == 200) << "PATCH status should be 200";
         expect(result->response_body == "PATCH:payload:header-value")
            << "Response body should echo payload and header";
      }

      server.stop();
   };

   "synchronous_head_request"_test = [] {
      working_test_server server;
      expect(server.start());

      glz::http_client client;

      auto result = client.head(server.base_url() + "/head");

      expect(result.has_value()) << "HEAD request should succeed";
      if (result.has_value()) {
         expect(result->status_code == 200) << "HEAD status should be 200";
         expect(result->response_body.empty()) << "HEAD response should have no body";
         const auto content_length = result->response_headers.first_value("content-length");
         expect(content_length.has_value() && *content_length == "5") << "HEAD should report the GET body length";
      }

      // The pooled connection must be clean for the next request.
      auto follow_up = client.get(server.base_url() + "/hello");
      expect(follow_up.has_value() && follow_up->response_body == "Hello, World!") << "Follow-up GET should succeed";

      server.stop();
   };

   // RFC 9110 9.3.2: a server that answers GET answers HEAD, so a HEAD on a GET-only route
   // runs the GET route and reports its Content-Length without the body.
   "synchronous_head_on_get_route"_test = [] {
      working_test_server server;
      expect(server.start());

      glz::http_client client;

      auto result = client.head(server.base_url() + "/hello");

      expect(result.has_value()) << "HEAD request should succeed";
      if (result.has_value()) {
         expect(result->status_code == 200) << "HEAD on a GET route should be 200, got " << result->status_code;
         expect(result->response_body.empty()) << "HEAD response should have no body";
         const auto content_length = result->response_headers.first_value("content-length");
         expect(content_length.has_value() && *content_length == "13") << "HEAD should report the GET body length";
      }

      auto follow_up = client.get(server.base_url() + "/hello");
      expect(follow_up.has_value() && follow_up->response_body == "Hello, World!") << "Follow-up GET should succeed";

      server.stop();
   };

   "async_head_request"_test = [] {
      working_test_server server;
      expect(server.start());

      glz::http_client client;

      // Both an explicit HEAD route and a GET route the HEAD falls back to.
      for (const std::string_view path : {"/head", "/hello"}) {
         auto future = client.head_async(server.base_url() + std::string(path));
         expect(future.wait_for(std::chrono::seconds(5)) == std::future_status::ready)
            << "Async HEAD should complete without waiting for a body";
         if (future.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
            continue;
         }
         auto result = future.get();
         expect(result.has_value()) << "Async HEAD request should succeed";
         if (result.has_value()) {
            expect(result->status_code == 200) << "HEAD status should be 200";
            expect(result->response_body.empty()) << "HEAD response should have no body";
            expect(result->response_headers.first_value("content-length").has_value())
               << "HEAD should report the GET body length";
         }
      }

      // The pooled connection must be clean for the next request.
      auto follow_up = client.get_async(server.base_url() + "/hello");
      expect(follow_up.wait_for(std::chrono::seconds(5)) == std::future_status::ready)
         << "Follow-up async GET should complete";
      if (follow_up.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
         auto result = follow_up.get();
         expect(result.has_value() && result->response_body == "Hello, World!") << "Follow-up GET should succeed";
      }

      server.stop();
   };

   "synchronous_304_with_content_length"_test = [] {
      working_test_server server;
      expect(server.start());

      glz::http_client client;

      auto result = client.get(server.base_url() + "/not-modified");

      expect(result.has_value()) << "304 request should succeed";
      if (result.has_value()) {
         expect(result->status_code == 304) << "Status should be 304";
         expect(result->response_body.empty()) << "304 response should have no body";
      }

      auto follow_up = client.get(server.base_url() + "/hello");
      expect(follow_up.has_value() && follow_up->response_body == "Hello, World!") << "Follow-up GET should succeed";

      server.stop();
   };

   "async_304_with_content_length"_test = [] {
      working_test_server server;
      expect(server.start());

      glz::http_client client;

      auto future = client.get_async(server.base_url() + "/not-modified");
      expect(future.wait_for(std::chrono::seconds(5)) == std::future_status::ready)
         << "Async 304 should complete without waiting for a body";
      if (future.valid() && future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
         auto result = future.get();
         expect(result.has_value()) << "Async 304 request should succeed";
         if (result.has_value()) {
            expect(result->status_code == 304) << "Status should be 304";
            expect(result->response_body.empty()) << "304 response should have no body";
         }
      }

      auto follow_up = client.get_async(server.base_url() + "/hello");
      expect(follow_up.wait_for(std::chrono::seconds(5)) == std::future_status::ready)
         << "Follow-up async GET should complete";

      server.stop();
   };

   "synchronous_delete_request"_test = [] {
      working_test_server server;
      expect(server.start());

      glz::http_client client;

      glz::http_headers headers{{"x-test-header", "header-value"}};
      auto result = client.del(server.base_url() + "/items/7", headers);

      expect(result.has_value()) << "DELETE request should succeed";
      if (result.has_value()) {
         expect(result->status_code == 200) << "DELETE status should be 200";
         expect(result->response_body == "DELETE:7:header-value") << "Response body should echo id and header";
      }

      server.stop();
   };

   "synchronous_options_request"_test = [] {
      working_test_server server;
      expect(server.start());

      glz::http_client client;

      auto result = client.options(server.base_url() + "/items/7",
                                   {{"Origin", "http://app.local"}, {"Access-Control-Request-Method", "DELETE"}});

      expect(result.has_value()) << "OPTIONS request should succeed";
      if (result.has_value()) {
         expect(result->status_code == 204) << "Preflight status should be 204";
         const auto allow = result->response_headers.first_value("allow");
         expect(allow.has_value() && *allow == "DELETE, OPTIONS") << "Allow should list DELETE and OPTIONS";
      }

      // The bodyless 204 must leave the pooled connection clean for the next request.
      auto follow_up = client.del(server.base_url() + "/items/8");
      expect(follow_up.has_value() && follow_up->response_body == "DELETE:8") << "Follow-up DELETE should succeed";

      server.stop();
   };

   "put_json_sets_content_type"_test = [] {
      working_test_server server;
      expect(server.start());

      glz::http_client client;

      test_http_client::json_payload payload{.value = 42, .message = "update"};

      std::string expected_json;
      auto ec = glz::write_json(payload, expected_json);
      expect(!ec) << "Serializing payload should succeed";

      glz::http_headers extra_headers{{"x-extra", "value"}};
      auto result = client.put_json(server.base_url() + "/json", payload, extra_headers);

      expect(result.has_value()) << "PUT JSON request should succeed";
      if (result.has_value()) {
         expect(result->status_code == 200) << "PUT JSON status should be 200";
         expect(result->response_body.find("CT=application/json") != std::string::npos)
            << "Content-Type header should be forwarded";
         expect(result->response_body.find("CT_COUNT=1") != std::string::npos)
            << "Content-Type should reach the wire exactly once";
         expect(result->response_body.find("BODY=" + expected_json) != std::string::npos)
            << "JSON body should be forwarded";
      }

      server.stop();
   };

   "put_json_keeps_caller_content_type"_test = [] {
      working_test_server server;
      expect(server.start());

      glz::http_client client;

      test_http_client::json_payload payload{.value = 7, .message = "vendor"};

      glz::http_headers vendor_headers{{"Content-Type", "application/vnd.api+json"}};
      auto result = client.put_json(server.base_url() + "/json", payload, vendor_headers);

      expect(result.has_value()) << "PUT JSON request should succeed";
      if (result.has_value()) {
         expect(result->response_body.find("CT=application/vnd.api+json") != std::string::npos)
            << "A caller-supplied Content-Type must not be replaced by application/json";
         expect(result->response_body.find("CT_COUNT=1") != std::string::npos)
            << "The default must not be appended alongside the caller's field";
      }

      server.stop();
   };

   "put_json_matches_lowercase_content_type"_test = [] {
      working_test_server server;
      expect(server.start());

      glz::http_client client;

      test_http_client::json_payload payload{.value = 9, .message = "charset"};

      glz::http_headers lowercase_headers{{"content-type", "application/json; charset=utf-8"}};
      auto result = client.put_json(server.base_url() + "/json", payload, lowercase_headers);

      expect(result.has_value()) << "PUT JSON request should succeed";
      if (result.has_value()) {
         expect(result->response_body.find("CT=application/json; charset=utf-8") != std::string::npos)
            << "The charset parameter must survive";
         expect(result->response_body.find("CT_COUNT=1") != std::string::npos)
            << "A lowercase field name must be recognized as already present";
      }

      server.stop();
   };

   "patch_json_sets_content_type"_test = [] {
      working_test_server server;
      expect(server.start());

      glz::http_client client;

      test_http_client::json_payload payload{.value = 42, .message = "patch"};

      std::string expected_json;
      auto ec = glz::write_json(payload, expected_json);
      expect(!ec) << "Serializing payload should succeed";

      auto result = client.patch_json(server.base_url() + "/json", payload);

      expect(result.has_value()) << "PATCH JSON request should succeed";
      if (result.has_value()) {
         expect(result->status_code == 200) << "PATCH JSON status should be 200";
         expect(result->response_body.find("CT=application/json") != std::string::npos)
            << "Content-Type header should be forwarded";
         expect(result->response_body.find("CT_COUNT=1") != std::string::npos)
            << "Content-Type should reach the wire exactly once";
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
         if (ec && ec != make_error_code(asio::error::eof) && ec != make_error_code(asio::error::operation_aborted)) {
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
         if (ec && ec != make_error_code(asio::error::eof) && ec != make_error_code(asio::error::operation_aborted)) {
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

// Raw TCP server that sends HTTP responses without Content-Length or chunked encoding
// (EOF-delimited body, per RFC 7230 §3.3.3)
class eof_delimited_server
{
  public:
   eof_delimited_server() = default;
   ~eof_delimited_server() { stop(); }

   bool start(const std::string& response_body)
   {
      try {
         acceptor_ = std::make_unique<asio::ip::tcp::acceptor>(
            io_, asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), 0));
         port_ = acceptor_->local_endpoint().port();

         server_thread_ = std::thread([this, response_body]() {
            try {
               asio::ip::tcp::socket socket(io_);
               acceptor_->accept(socket);

               // Read the request (drain it)
               asio::streambuf request_buf;
               asio::read_until(socket, request_buf, "\r\n\r\n");

               // Send response with Connection: close but no Content-Length or Transfer-Encoding
               std::string http_response =
                  "HTTP/1.1 200 OK\r\n"
                  "Content-Type: application/json\r\n"
                  "Connection: close\r\n"
                  "\r\n" +
                  response_body;

               asio::write(socket, asio::buffer(http_response));
               socket.shutdown(asio::ip::tcp::socket::shutdown_both);
               socket.close();
            }
            catch (...) {
            }
         });

         return true;
      }
      catch (...) {
         return false;
      }
   }

   bool start_with_reset()
   {
      try {
         acceptor_ = std::make_unique<asio::ip::tcp::acceptor>(
            io_, asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), 0));
         port_ = acceptor_->local_endpoint().port();

         server_thread_ = std::thread([this]() {
            try {
               asio::ip::tcp::socket socket(io_);
               acceptor_->accept(socket);

               asio::streambuf request_buf;
               asio::read_until(socket, request_buf, "\r\n\r\n");

               // Send headers and partial body, then reset the connection
               std::string http_response =
                  "HTTP/1.1 200 OK\r\n"
                  "Content-Type: application/json\r\n"
                  "Connection: close\r\n"
                  "\r\n"
                  "partial";

               asio::write(socket, asio::buffer(http_response));

               // Force a RST by setting linger to 0 and closing
               asio::socket_base::linger option(true, 0);
               socket.set_option(option);
               socket.close();
            }
            catch (...) {
            }
         });

         return true;
      }
      catch (...) {
         return false;
      }
   }

   void stop()
   {
      if (acceptor_) acceptor_->close();
      if (server_thread_.joinable()) server_thread_.join();
   }

   uint16_t port() const { return port_; }
   std::string base_url() const { return "http://127.0.0.1:" + std::to_string(port_); }

  private:
   asio::io_context io_;
   std::unique_ptr<asio::ip::tcp::acceptor> acceptor_;
   std::thread server_thread_;
   uint16_t port_{0};
};

class host_header_server
{
  public:
   host_header_server() = default;
   ~host_header_server() { stop(); }

   bool start()
   {
      try {
         acceptor_ = std::make_unique<asio::ip::tcp::acceptor>(
            io_, asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), 0));
         port_ = acceptor_->local_endpoint().port();

         server_thread_ = std::thread([this]() {
            try {
               asio::ip::tcp::socket socket(io_);
               acceptor_->accept(socket);

               asio::streambuf request_buf;
               asio::read_until(socket, request_buf, "\r\n\r\n");

               std::istream request_stream(&request_buf);
               std::string line;
               std::getline(request_stream, line);

               while (std::getline(request_stream, line) && line != "\r") {
                  if (!line.empty() && line.back() == '\r') line.pop_back();
                  if (line.rfind("Host: ", 0) == 0) {
                     host_header_ = line.substr(6);
                     break;
                  }
               }

               const std::string http_response =
                  "HTTP/1.1 200 OK\r\n"
                  "Content-Length: 0\r\n"
                  "Connection: close\r\n"
                  "\r\n";

               asio::write(socket, asio::buffer(http_response));
               socket.shutdown(asio::ip::tcp::socket::shutdown_both);
               socket.close();
            }
            catch (...) {
            }
         });

         return true;
      }
      catch (...) {
         return false;
      }
   }

   void stop()
   {
      if (acceptor_) acceptor_->close();
      if (server_thread_.joinable()) server_thread_.join();
   }

   uint16_t port() const { return port_; }
   std::string base_url() const { return "http://127.0.0.1:" + std::to_string(port_); }
   const std::string& host_header() const { return host_header_; }

  private:
   asio::io_context io_;
   std::unique_ptr<asio::ip::tcp::acceptor> acceptor_;
   std::thread server_thread_;
   uint16_t port_{0};
   std::string host_header_;
};

// Answers every request with `reply`, which may be empty, then sends nothing, holding the
// connection open until the client closes it or `stall` passes.
class stalling_server
{
  public:
   stalling_server(std::string reply, std::chrono::milliseconds stall) : reply_(std::move(reply)), stall_(stall) {}
   ~stalling_server() { stop(); }

   bool start()
   {
      try {
         acceptor_ = std::make_unique<asio::ip::tcp::acceptor>(
            io_, asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), 0));
         port_ = acceptor_->local_endpoint().port();
         accept();
         server_thread_ = std::thread([this]() { io_.run(); });
         return true;
      }
      catch (...) {
         return false;
      }
   }

   void stop()
   {
      io_.stop();
      if (server_thread_.joinable()) server_thread_.join();
   }

   std::string base_url() const { return "http://127.0.0.1:" + std::to_string(port_); }

  private:
   void accept()
   {
      acceptor_->async_accept([this](asio::error_code ec, asio::ip::tcp::socket socket) {
         if (ec) return;
         serve(std::make_shared<asio::ip::tcp::socket>(std::move(socket)));
         accept();
      });
   }

   void serve(std::shared_ptr<asio::ip::tcp::socket> socket)
   {
      auto request = std::make_shared<asio::streambuf>();
      asio::async_read_until(*socket, *request, "\r\n\r\n", [this, socket, request](asio::error_code ec, std::size_t) {
         if (ec) return;
         asio::async_write(*socket, asio::buffer(reply_), [this, socket](asio::error_code ec, std::size_t) {
            if (ec) return;
            // The client's close or the end of the stall, whichever comes first, closes the socket.
            auto timer = std::make_shared<asio::steady_timer>(io_, stall_);
            timer->async_wait([socket, timer](asio::error_code) {
               asio::error_code ignored;
               socket->close(ignored);
            });
            auto sink = std::make_shared<std::array<char, 64>>();
            socket->async_read_some(asio::buffer(*sink),
                                    [timer, sink](asio::error_code, std::size_t) { timer->cancel(); });
         });
      });
   }

   std::string reply_;
   std::chrono::milliseconds stall_;
   asio::io_context io_;
   std::unique_ptr<asio::ip::tcp::acceptor> acceptor_;
   std::thread server_thread_;
   uint16_t port_{0};
};

// Records how a stream ends, for tests that count its error and disconnect reports.
struct stream_end_log
{
   std::mutex mutex;
   std::condition_variable ended;
   std::vector<std::error_code> errors;
   int disconnects{};
   bool error_after_disconnect{};

   glz::http_error_handler on_error()
   {
      return [this](std::error_code ec) {
         std::lock_guard lock(mutex);
         errors.push_back(ec);
         error_after_disconnect |= disconnects > 0;
      };
   }

   glz::http_disconnect_handler on_disconnect()
   {
      return [this] {
         {
            std::lock_guard lock(mutex);
            ++disconnects;
         }
         ended.notify_all();
      };
   }

   bool wait_for_end(std::chrono::steady_clock::duration timeout)
   {
      std::unique_lock lock(mutex);
      return ended.wait_for(lock, timeout, [&] { return disconnects > 0; });
   }

   // One error, one of `allowed`, reported before the stream's one on_disconnect.
   void expect_one_end(std::initializer_list<std::error_code> allowed)
   {
      std::lock_guard lock(mutex);
      expect(disconnects == 1) << "on_disconnect ran " << disconnects << " times";
      expect(errors.size() == 1) << "on_error ran " << errors.size() << " times";
      for (const auto& ec : errors) {
         expect(std::find(allowed.begin(), allowed.end(), ec) != allowed.end()) << "unexpected error: " << ec.message();
      }
      expect(!error_after_disconnect) << "on_error ran after on_disconnect";
   }
};

suite stream_disconnect_tests = [] {
   // disconnect() called from a thread other than the client's workers, a few microseconds after
   // the first chunk is delivered, while a worker moves on to the next read. The server then
   // stalls, so a stop the client loses holds the stream until the server gives up, while one it
   // honours ends the stream at once. A stop that races the client's own close of the socket
   // crashes under AddressSanitizer when the two are not ordered.
   "disconnect_from_another_thread_ends_a_stalled_chunked_stream"_test = [] {
      constexpr auto stall = std::chrono::milliseconds(1000);
      stalling_server server{
         "HTTP/1.1 200 OK\r\n"
         "Transfer-Encoding: chunked\r\n"
         "\r\n"
         "5\r\nhello\r\n",
         stall};
      expect(server.start()) << "Stalling server should start";

      glz::http_client client;
      std::mt19937 random{3021};
      std::uniform_int_distribution<int> spin_us{0, 20};
      const auto spin_for = [](std::chrono::steady_clock::duration span, const std::atomic<bool>* until_set) {
         const auto deadline = std::chrono::steady_clock::now() + span;
         while (std::chrono::steady_clock::now() < deadline) {
            if (until_set && until_set->load()) return true;
         }
         return until_set == nullptr;
      };

      // Each lost stop costs a stall, so a failing run stops counting at a handful.
      constexpr int rounds = 200;
      constexpr int enough_lost = 5;
      int ran = 0;
      int lost = 0;
      for (; ran < rounds && lost < enough_lost; ++ran) {
         std::atomic<bool> delivered = false;
         std::atomic<bool> released = false;
         std::promise<void> disconnect_promise;
         auto disconnect_future = disconnect_promise.get_future();

         auto conn = client.stream_request_v2({.url = server.base_url() + "/stall",
                                               .body = {},
                                               .headers = {},
                                               .on_data =
                                                  [&](std::string_view) {
                                                     // Hold the worker until the test thread is running, so
                                                     // the stop below lands as the worker takes its next step.
                                                     delivered = true;
                                                     spin_for(std::chrono::seconds(5), &released);
                                                  },
                                               .on_error = [](std::error_code) {},
                                               .on_progress = {},
                                               .on_connect = {},
                                               .on_disconnect = [&] { disconnect_promise.set_value(); }});
         expect(conn != nullptr);
         if (!conn || !spin_for(std::chrono::seconds(5), &delivered)) {
            expect(false) << "The first chunk did not arrive";
            break;
         }

         released = true;
         spin_for(std::chrono::microseconds(spin_us(random)), nullptr);
         conn->disconnect();

         if (disconnect_future.wait_for(stall / 2) != std::future_status::ready) {
            ++lost;
         }
         // A lost stop still ends when the server closes, so the next round starts on a clean slate.
         if (disconnect_future.wait_for(std::chrono::seconds(5)) != std::future_status::ready) {
            expect(false) << "The stream never ended";
            break;
         }
      }

      expect(ran == rounds && lost == 0) << lost << " stops were lost in " << ran << " rounds";
      server.stop();
   };

   // The timeout cancels the pending read of the head. The stream must still end once: one
   // timed_out, then one on_disconnect, and no operation_aborted from the cancelled read.
   "timeout_before_head_ends_the_stream_once"_test = [] {
      stalling_server server{"", std::chrono::milliseconds(3000)};
      expect(server.start()) << "Stalling server should start";

      stream_end_log log; // outlives the client, whose workers run its handlers
      glz::http_client client;
      auto conn = client.stream_request_v2({.url = server.base_url() + "/silent",
                                            .timeout = std::chrono::seconds(1),
                                            .on_data = [](std::string_view) {},
                                            .on_error = log.on_error(),
                                            .on_disconnect = log.on_disconnect()});
      expect(conn != nullptr);

      expect(log.wait_for_end(std::chrono::seconds(5))) << "The stream never ended";
      std::this_thread::sleep_for(std::chrono::milliseconds(300)); // room for a second report
      log.expect_one_end({std::make_error_code(std::errc::timed_out)});
      server.stop();
   };

   // A stream that fails before its head arrives ends there, and its timeout must not report it
   // a second time once the timer runs out.
   "refused_connect_is_not_reported_again_by_the_timeout"_test = [] {
      // A bound socket that never listens refuses connections and keeps the port from being reused.
      asio::io_context io;
      asio::ip::tcp::socket closed{io};
      closed.open(asio::ip::tcp::v4());
      closed.bind(asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), 0));
      const uint16_t closed_port = closed.local_endpoint().port();

      stream_end_log log; // outlives the client, whose workers run its handlers
      glz::http_client client;
      auto conn = client.stream_request_v2({.url = "http://127.0.0.1:" + std::to_string(closed_port) + "/",
                                            .timeout = std::chrono::seconds(1),
                                            .on_data = [](std::string_view) {},
                                            .on_error = log.on_error(),
                                            .on_disconnect = log.on_disconnect()});
      expect(conn != nullptr);

      expect(log.wait_for_end(std::chrono::seconds(5))) << "The stream never ended";
      std::this_thread::sleep_for(std::chrono::milliseconds(1500)); // past the timeout
      // Windows retries a refused loopback connect for about two seconds, so there the timeout
      // may end the stream first. Either way it ends once.
      log.expect_one_end(
         {make_error_code(asio::error::connection_refused), std::make_error_code(std::errc::timed_out)});
   };
};

suite eof_delimited_tests = [] {
   "sync_eof_delimited_body"_test = [] {
      const std::string expected_body = R"({"name":"Hue Bridge","modelid":"BSB002"})";
      eof_delimited_server server;
      expect(server.start(expected_body)) << "EOF-delimited server should start";

      glz::http_client client;
      auto result = client.get(server.base_url() + "/api/0/config");

      expect(result.has_value()) << "GET should succeed for EOF-delimited response";
      if (result.has_value()) {
         expect(result->status_code == 200) << "Status should be 200";
         expect(result->response_body == expected_body) << "Body should match: " << result->response_body;
      }

      server.stop();
   };

   "async_eof_delimited_body"_test = [] {
      const std::string expected_body = R"({"name":"Hue Bridge","modelid":"BSB002"})";
      eof_delimited_server server;
      expect(server.start(expected_body)) << "EOF-delimited server should start";

      glz::http_client client;

      std::promise<glz::expected<glz::response, std::error_code>> promise;
      auto future = promise.get_future();

      client.get_async(
         server.base_url() + "/api/0/config", {},
         [&](glz::expected<glz::response, std::error_code> result) { promise.set_value(std::move(result)); });

      auto status = future.wait_for(std::chrono::seconds(5));
      expect(status == std::future_status::ready) << "Async GET should complete";

      if (status == std::future_status::ready) {
         auto result = future.get();
         expect(result.has_value()) << "Async GET should succeed for EOF-delimited response";
         if (result.has_value()) {
            expect(result->status_code == 200) << "Status should be 200";
            expect(result->response_body == expected_body) << "Body should match: " << result->response_body;
         }
      }

      server.stop();
   };

   "sync_eof_delimited_max_body_size"_test = [] {
      // Body larger than the limit
      const std::string large_body(1024, 'X');
      eof_delimited_server server;
      expect(server.start(large_body)) << "EOF-delimited server should start";

      glz::http_client client;
      client.max_response_body_size(100); // limit well below body size
      auto result = client.get(server.base_url() + "/api/0/config");

      expect(!result.has_value()) << "GET should fail when body exceeds max size";

      server.stop();
   };

   "async_eof_delimited_max_body_size"_test = [] {
      const std::string large_body(1024, 'X');
      eof_delimited_server server;
      expect(server.start(large_body)) << "EOF-delimited server should start";

      glz::http_client client;
      client.max_response_body_size(100);

      std::promise<glz::expected<glz::response, std::error_code>> promise;
      auto future = promise.get_future();

      client.get_async(
         server.base_url() + "/api/0/config", {},
         [&](glz::expected<glz::response, std::error_code> result) { promise.set_value(std::move(result)); });

      auto status = future.wait_for(std::chrono::seconds(5));
      expect(status == std::future_status::ready) << "Async GET should complete";

      if (status == std::future_status::ready) {
         auto result = future.get();
         expect(!result.has_value()) << "Async GET should fail when body exceeds max size";
      }

      server.stop();
   };

   "sync_eof_delimited_connection_reset"_test = [] {
      // Server that resets the connection after sending partial data
      eof_delimited_server server;
      expect(server.start_with_reset()) << "Server should start";

      glz::http_client client;
      auto result = client.get(server.base_url() + "/api/0/config");

      // RST behavior is platform-dependent:
      // - On most platforms, the RST is received as a connection error (result is an error)
      // - On some platforms, data may arrive before the RST, resulting in a partial body with EOF
      if (result.has_value()) {
         expect(result->response_body == "partial") << "If RST is seen as EOF, partial body should be returned";
      }

      server.stop();
   };
};

suite host_header_tests = [] {
   "http_request_host_header_includes_port"_test = [] {
      host_header_server server;
      expect(server.start()) << "Host header server should start";

      glz::http_client client;
      auto result = client.get(server.base_url() + "/host-header");

      server.stop();

      expect(result.has_value()) << "GET should succeed";
      if (result.has_value()) {
         expect(result->status_code == 200) << "Status should be 200";
      }

      const std::string expected_host = "127.0.0.1:" + std::to_string(server.port());
      expect(server.host_header() == expected_host) << "Host header should include authority port";
   };
};

suite with_json_content_type_tests = [] {
   "with_json_content_type_sets_default"_test = [] {
      auto headers = glz::detail::with_json_content_type(glz::http_headers{{"X-Extra", "value"}});

      expect(headers.count("Content-Type") == 1);
      expect(headers.first_value("Content-Type") == "application/json");
      expect(headers.first_value("X-Extra") == "value") << "Caller headers should be carried through";
   };

   "with_json_content_type_keeps_caller_value"_test = [] {
      auto headers =
         glz::detail::with_json_content_type(glz::http_headers{{"Content-Type", "application/vnd.api+json"}});

      expect(headers.count("Content-Type") == 1);
      expect(headers.first_value("Content-Type") == "application/vnd.api+json");
   };

   "with_json_content_type_ignores_name_case"_test = [] {
      auto headers = glz::detail::with_json_content_type(glz::http_headers{{"content-type", "text/plain"}});

      expect(headers.count("Content-Type") == 1) << "A lowercase name must not yield a second field";
      expect(headers.first_value("Content-Type") == "text/plain");
   };

   "with_json_content_type_copies_caller_headers"_test = [] {
      glz::http_headers original{{"X-Extra", "value"}};
      auto headers = glz::detail::with_json_content_type(original);

      expect(!original.contains("Content-Type")) << "The caller's container is taken by value";
      expect(headers.contains("Content-Type"));
   };
};

// The server's own answers to requests no route handles (404, 405, 426 and the automatic
// preflight) go through the same middleware and hooks as a route's response.
suite unrouted_request_tests = [] {
   struct unrouted_server
   {
      std::shared_ptr<asio::io_context> io_ctx = std::make_shared<asio::io_context>();
      glz::http_server<> server{io_ctx};
      std::thread thread;
      std::atomic<int> hooked_responses{0};
      std::atomic<int> last_hooked_status{0};

      unrouted_server()
      {
         // Headers set by a request hook belong to the response whatever answers it.
         server.on_request([](const request&, response& res) { res.header("X-Request-Id", "req-1"); });
         server.use([](const request&, response& res) { res.header("X-Middleware", "ran"); });
         server.on_response([this](const request&, const response& res) {
            hooked_responses.fetch_add(1);
            last_hooked_status.store(res.status_code);
         });
         server.enable_cors();

         server.get("/thing", [](const request&, response& res) { res.body("thing"); });
         server.route(http_method::OPTIONS, "/probe", [](const request&, response& res) { res.status(204); });
         server.websocket("/ws/:room", std::make_shared<glz::websocket_server>());

         server.bind("127.0.0.1", 0);
         server.start(0);
         thread = std::thread([this] { io_ctx->run(); });
      }

      ~unrouted_server()
      {
         server.stop();
         io_ctx->stop();
         if (thread.joinable()) thread.join();
      }

      std::string url(std::string_view path) const
      {
         return "http://127.0.0.1:" + std::to_string(server.port()) + std::string(path);
      }
   };

   const auto header_value = [](const auto& result, std::string_view name) {
      return result->response_headers.first_value(name).value_or("");
   };

   "not_found_runs_middleware_and_hooks"_test = [&] {
      unrouted_server s;
      glz::http_client client;

      auto result = client.get(s.url("/missing"), {{"Origin", "http://app.local"}});
      expect(result.has_value()) << "Request should yield a response";
      if (result) {
         expect(result->status_code == 404);
         expect(result->response_body == "Not Found");
         expect(header_value(result, "x-request-id") == "req-1") << "request hook headers are kept";
         expect(header_value(result, "x-middleware") == "ran") << "middleware runs on a 404";
         expect(header_value(result, "access-control-allow-origin") == "*") << "CORS headers reach a 404";
         expect(!result->response_headers.first_value("allow").has_value());
      }
      expect(s.hooked_responses.load() == 1) << "response hooks see the 404";
      expect(s.last_hooked_status.load() == 404);
   };

   "method_not_allowed_runs_middleware_and_hooks"_test = [&] {
      unrouted_server s;
      glz::http_client client;

      auto result = client.del(s.url("/thing"), {{"Origin", "http://app.local"}});
      expect(result.has_value()) << "Request should yield a response";
      if (result) {
         expect(result->status_code == 405);
         expect(result->response_body == "Method Not Allowed");
         expect(header_value(result, "allow") == "GET, HEAD, OPTIONS");
         expect(header_value(result, "x-request-id") == "req-1") << "request hook headers are kept";
         expect(header_value(result, "x-middleware") == "ran") << "middleware runs on a 405";
         expect(header_value(result, "access-control-allow-origin") == "*") << "CORS headers reach a 405";
      }
      expect(s.hooked_responses.load() == 1) << "response hooks see the 405";
      expect(s.last_hooked_status.load() == 405);
   };

   "preflight_with_unrouted_method_answers_405"_test = [&] {
      unrouted_server s;
      glz::http_client client;

      auto result =
         client.options(s.url("/thing"), {{"Origin", "http://app.local"}, {"Access-Control-Request-Method", "PUT"}});
      expect(result.has_value()) << "Request should yield a response";
      if (result) {
         expect(result->status_code == 405);
         expect(header_value(result, "allow") == "GET, HEAD, OPTIONS");
         expect(header_value(result, "x-request-id") == "req-1") << "request hook headers are kept";
      }
      expect(s.last_hooked_status.load() == 405);
   };

   // A path served only by an explicit OPTIONS route has a route, so other methods get 405.
   "options_only_path_answers_405"_test = [&] {
      unrouted_server s;
      glz::http_client client;

      auto result = client.get(s.url("/probe"));
      expect(result.has_value()) << "Request should yield a response";
      if (result) {
         expect(result->status_code == 405) << "got " << result->status_code;
         expect(header_value(result, "allow") == "OPTIONS");
      }

      auto options = client.options(s.url("/probe"));
      expect(options.has_value() && options->status_code == 204) << "the explicit OPTIONS route still answers";
   };

   // A WebSocket route only answers the upgrade: other methods get 405, and a GET that does
   // not ask to upgrade gets 426 naming the protocol (RFC 9110 15.5.22).
   "websocket_path_answers_405_or_426"_test = [&] {
      unrouted_server s;
      glz::http_client client;

      auto post = client.post(s.url("/ws/lobby"), "body");
      expect(post.has_value()) << "Request should yield a response";
      if (post) {
         expect(post->status_code == 405) << "got " << post->status_code;
         expect(header_value(post, "allow") == "GET, HEAD, OPTIONS");
      }

      auto get = client.get(s.url("/ws/lobby"));
      expect(get.has_value()) << "Request should yield a response";
      if (get) {
         expect(get->status_code == 426) << "got " << get->status_code;
         expect(header_value(get, "upgrade") == "websocket");
         expect(get->response_headers.contains_token("connection", "upgrade"))
            << "RFC 9110 7.8: Upgrade is listed in Connection";
         expect(get->response_headers.contains_token("connection", "keep-alive"))
            << "the keep-alive decision is still announced";
         expect(header_value(get, "x-middleware") == "ran");
      }

      // The 426 left the connection usable.
      auto thing = client.get(s.url("/thing"));
      expect(thing.has_value() && thing->response_body == "thing");

      auto other = client.get(s.url("/ws"));
      expect(other.has_value() && other->status_code == 404) << "the parameter is required";
   };
};

int main()
{
   std::cout << "Running HTTP Client Tests...\n";
   std::cout << "============================\n";
   return 0;
}
