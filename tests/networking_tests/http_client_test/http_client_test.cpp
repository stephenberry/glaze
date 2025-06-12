#include "glaze/net/http_client.hpp"

#include <atomic>
#include <chrono>
#include <future>
#include <thread>

#include "glaze/net/http_server.hpp"
#include "ut/ut.hpp"

using namespace ut;
using namespace glz;

// Simplified test server that actually works
class working_test_server
{
  public:
   working_test_server() : port_(0), running_(false) {}

   ~working_test_server() { stop(); }

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

      server_.get("/hello", [](const request& req, response& res) {
         res.status(200).content_type("text/plain").body("Hello, World!");
      });

      server_.post(
         "/echo", [](const request& req, response& res) { res.status(200).content_type("text/plain").body(req.body); });

      server_.get("/json", [](const request& req, response& res) {
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

      server_.get("/slow", [](const request& req, response& res) {
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

         std::function<void(const std::error_code&)> send_data;
         send_data = [conn, timer, counter, send_data](const std::error_code& ec) {
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
                                timer->async_wait(send_data);
                             });
         };
         timer->async_wait(send_data);
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

  private:
   asio::io_context io_context_;
   std::thread worker_thread_;

   std::expected<response, std::error_code> perform_request(const std::string& method, const url_parts& url,
                                                            const std::string& body)
   {
      std::promise<std::expected<response, std::error_code>> promise;
      auto future = promise.get_future();

      asio::post(io_context_, [this, method, url, body, &promise]() {
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

            // Skip headers for simplicity
            std::string header_line;
            while (std::getline(response_stream, header_line) && header_line != "\r") {
               // Skip headers
            }

            // Read body
            std::error_code ec;
            asio::read(socket, response_buffer, asio::transfer_all(), ec);

            std::string response_body{std::istreambuf_iterator<char>(&response_buffer),
                                      std::istreambuf_iterator<char>()};

            response resp;
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

      auto conn =
         client.get_stream(server.base_url() + "/stream-test", on_data, on_error, {}, on_connect, on_disconnect);
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

      auto conn =
         client.get_stream(server.base_url() + "/slow-stream", on_data, on_error, {}, on_connect, on_disconnect);
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
};

int main()
{
   std::cout << "Running HTTP Client Tests...\n";
   std::cout << "============================\n";
   return 0;
}
