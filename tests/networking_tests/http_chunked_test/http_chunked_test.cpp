// Tests for chunked transfer-encoding support in glz::http_client
// Covers synchronous, asynchronous, and streaming paths

#include <atomic>
#include <chrono>
#include <future>
#include <mutex>
#include <numeric>
#include <string>
#include <thread>

#include "glaze/net/http_client.hpp"
#include "glaze/net/http_server.hpp"
#include "ut/ut.hpp"

using namespace ut;

// --------------------------------------------------------------------------
// Reusable test server that exposes various chunked-response endpoints
// --------------------------------------------------------------------------
struct chunked_test_server
{
   chunked_test_server() : port_(0), running_(false) {}
   ~chunked_test_server() { stop(); }

   bool start()
   {
      if (running_) return true;

      setup_routes();

      try {
         for (uint16_t p = 19100; p < 19300; ++p) {
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

         running_ = true;
         server_thread_ = std::thread([this] {
            try {
               server_.start(1);
            }
            catch (...) {
               running_ = false;
            }
         });

         for (int i = 0; i < 50; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (is_ready()) return true;
         }
         stop();
         return false;
      }
      catch (...) {
         return false;
      }
   }

   void stop()
   {
      if (!running_) return;
      running_ = false;
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      server_.stop();
      if (server_thread_.joinable()) server_thread_.join();
   }

   uint16_t port() const { return port_; }
   std::string base_url() const { return "http://127.0.0.1:" + std::to_string(port_); }

   glz::http_server<> server_;
   std::thread server_thread_;
   uint16_t port_;
   std::atomic<bool> running_;

   void setup_routes()
   {
      server_.on_error([this](std::error_code ec, std::source_location) {
         if (running_ && ec != asio::error::eof && ec != asio::error::operation_aborted) {
            std::fprintf(stderr, "Server error: %s\n", ec.message().c_str());
         }
      });

      // --- Basic single-chunk endpoint ---
      server_.stream_get("/single-chunk", [](glz::request&, glz::streaming_response& res) {
         res.start_stream(200, {{"Content-Type", "text/plain"}});
         res.send("hello chunked world");
         res.close();
      });

      // --- Multi-chunk endpoint (3 chunks) ---
      server_.stream_get("/multi-chunk", [](glz::request&, glz::streaming_response& res) {
         res.start_stream(200, {{"Content-Type", "text/plain"}});
         res.send("chunk1");
         res.send("chunk2");
         res.send("chunk3");
         res.close();
      });

      // --- Empty body (only terminal chunk) ---
      server_.stream_get("/empty-chunked", [](glz::request&, glz::streaming_response& res) {
         res.start_stream(200, {{"Content-Type", "text/plain"}});
         res.close(); // immediate close => 0-length body
      });

      // --- Large body (many chunks) ---
      server_.stream_get("/large-chunked", [](glz::request&, glz::streaming_response& res) {
         res.start_stream(200, {{"Content-Type", "application/octet-stream"}});
         // Send 100 chunks of 1000 bytes each = 100 KB total
         std::string chunk(1000, 'X');
         for (int i = 0; i < 100; ++i) {
            res.send(chunk);
         }
         res.close();
      });

      // --- JSON chunked response ---
      server_.stream_get("/json-chunked", [](glz::request&, glz::streaming_response& res) {
         res.start_stream(200, {{"Content-Type", "application/json"}});
         res.send(R"({"status":"ok","value":42})");
         res.close();
      });

      // --- Binary data with null bytes ---
      server_.stream_get("/binary-chunked", [](glz::request&, glz::streaming_response& res) {
         res.start_stream(200, {{"Content-Type", "application/octet-stream"}});
         std::string binary_data;
         binary_data.push_back('\x00');
         binary_data.push_back('\x01');
         binary_data.push_back('\xFF');
         binary_data.push_back('\x00');
         binary_data.append("text after nulls");
         binary_data.push_back('\x00');
         res.send(binary_data);
         res.close();
      });

      // --- Chunked POST echo ---
      server_.stream_post("/echo-chunked", [](glz::request& req, glz::streaming_response& res) {
         res.start_stream(200, {{"Content-Type", "text/plain"}});
         res.send("echo:");
         res.send(req.body);
         res.close();
      });

      // --- Non-chunked (Content-Length) endpoint for comparison ---
      server_.get("/not-chunked", [](const glz::request&, glz::response& res) {
         res.status(200).content_type("text/plain").body("not chunked body");
      });

      // --- Chunked response with custom headers ---
      server_.stream_get("/chunked-with-headers", [](glz::request&, glz::streaming_response& res) {
         res.start_stream(200, {{"Content-Type", "text/plain"}, {"X-Custom-Header", "custom-value"}});
         res.send("header test body");
         res.close();
      });

      // --- Many small chunks (single byte each) ---
      server_.stream_get("/many-tiny-chunks", [](glz::request&, glz::streaming_response& res) {
         res.start_stream(200, {{"Content-Type", "text/plain"}});
         const std::string message = "Hello!";
         for (char c : message) {
            res.send(std::string_view(&c, 1));
         }
         res.close();
      });

      // --- Varying chunk sizes ---
      server_.stream_get("/varying-sizes", [](glz::request&, glz::streaming_response& res) {
         res.start_stream(200, {{"Content-Type", "text/plain"}});
         res.send("A"); // 1 byte
         res.send(std::string(10, 'B')); // 10 bytes
         res.send(std::string(100, 'C')); // 100 bytes
         res.send(std::string(1000, 'D')); // 1000 bytes
         res.send("E"); // 1 byte
         res.close();
      });
   }

   bool is_ready()
   {
      try {
         asio::io_context io;
         asio::ip::tcp::socket socket(io);
         asio::ip::tcp::endpoint ep(asio::ip::make_address("127.0.0.1"), port_);
         asio::error_code ec;
         socket.connect(ep, ec);
         if (!ec) {
            socket.close();
            return true;
         }
      }
      catch (...) {
      }
      return false;
   }
};

// ==========================================================================
// Test suites
// ==========================================================================

suite chunked_sync_tests = [] {
   "sync_single_chunk"_test = [] {
      chunked_test_server server;
      expect(server.start()) << "Server should start";

      glz::http_client client;
      auto result = client.get(server.base_url() + "/single-chunk");

      expect(result.has_value()) << "Request should succeed";
      if (result) {
         expect(result->status_code == 200) << "Status should be 200";
         expect(result->response_body == "hello chunked world") << "Body should match single chunk content";
      }

      server.stop();
   };

   "sync_multi_chunk"_test = [] {
      chunked_test_server server;
      expect(server.start()) << "Server should start";

      glz::http_client client;
      auto result = client.get(server.base_url() + "/multi-chunk");

      expect(result.has_value()) << "Request should succeed";
      if (result) {
         expect(result->status_code == 200) << "Status should be 200";
         expect(result->response_body == "chunk1chunk2chunk3") << "Body should concatenate all chunks";
      }

      server.stop();
   };

   "sync_empty_chunked_body"_test = [] {
      chunked_test_server server;
      expect(server.start()) << "Server should start";

      glz::http_client client;
      auto result = client.get(server.base_url() + "/empty-chunked");

      expect(result.has_value()) << "Request should succeed";
      if (result) {
         expect(result->status_code == 200) << "Status should be 200";
         expect(result->response_body.empty()) << "Body should be empty for zero-chunk response";
      }

      server.stop();
   };

   "sync_large_chunked_body"_test = [] {
      chunked_test_server server;
      expect(server.start()) << "Server should start";

      glz::http_client client;
      auto result = client.get(server.base_url() + "/large-chunked");

      expect(result.has_value()) << "Request should succeed";
      if (result) {
         expect(result->status_code == 200) << "Status should be 200";
         expect(result->response_body.size() == 100'000u) << "Body should be 100KB (100 * 1000)";
         // Verify content is all 'X'
         bool all_x =
            std::all_of(result->response_body.begin(), result->response_body.end(), [](char c) { return c == 'X'; });
         expect(all_x) << "All bytes should be 'X'";
      }

      server.stop();
   };

   "sync_binary_chunked"_test = [] {
      chunked_test_server server;
      expect(server.start()) << "Server should start";

      glz::http_client client;
      auto result = client.get(server.base_url() + "/binary-chunked");

      expect(result.has_value()) << "Request should succeed";
      if (result) {
         expect(result->status_code == 200) << "Status should be 200";

         // Build expected binary data
         std::string expected;
         expected.push_back('\x00');
         expected.push_back('\x01');
         expected.push_back('\xFF');
         expected.push_back('\x00');
         expected.append("text after nulls");
         expected.push_back('\x00');

         expect(result->response_body.size() == expected.size()) << "Binary body size should match";
         expect(result->response_body == expected) << "Binary body content should match exactly";
      }

      server.stop();
   };

   "sync_not_chunked_still_works"_test = [] {
      chunked_test_server server;
      expect(server.start()) << "Server should start";

      glz::http_client client;
      auto result = client.get(server.base_url() + "/not-chunked");

      expect(result.has_value()) << "Request should succeed";
      if (result) {
         expect(result->status_code == 200) << "Status should be 200";
         expect(result->response_body == "not chunked body") << "Non-chunked body should still work";
      }

      server.stop();
   };

   "sync_chunked_preserves_headers"_test = [] {
      chunked_test_server server;
      expect(server.start()) << "Server should start";

      glz::http_client client;
      auto result = client.get(server.base_url() + "/chunked-with-headers");

      expect(result.has_value()) << "Request should succeed";
      if (result) {
         expect(result->status_code == 200) << "Status should be 200";
         expect(result->response_body == "header test body") << "Body should match";

         auto custom = result->response_headers.find("x-custom-header");
         expect(custom != result->response_headers.end()) << "Custom header should be present";
         if (custom != result->response_headers.end()) {
            expect(custom->second == "custom-value") << "Custom header value should match";
         }

         auto te = result->response_headers.find("transfer-encoding");
         expect(te != result->response_headers.end()) << "Transfer-Encoding header should be present";
         if (te != result->response_headers.end()) {
            expect(te->second.find("chunked") != std::string::npos) << "Transfer-Encoding should be chunked";
         }
      }

      server.stop();
   };

   "sync_many_tiny_chunks"_test = [] {
      chunked_test_server server;
      expect(server.start()) << "Server should start";

      glz::http_client client;
      auto result = client.get(server.base_url() + "/many-tiny-chunks");

      expect(result.has_value()) << "Request should succeed";
      if (result) {
         expect(result->status_code == 200) << "Status should be 200";
         expect(result->response_body == "Hello!") << "Single-byte chunks should reassemble correctly";
      }

      server.stop();
   };

   "sync_varying_chunk_sizes"_test = [] {
      chunked_test_server server;
      expect(server.start()) << "Server should start";

      glz::http_client client;
      auto result = client.get(server.base_url() + "/varying-sizes");

      expect(result.has_value()) << "Request should succeed";
      if (result) {
         expect(result->status_code == 200) << "Status should be 200";

         std::string expected;
         expected += "A";
         expected += std::string(10, 'B');
         expected += std::string(100, 'C');
         expected += std::string(1000, 'D');
         expected += "E";
         expect(result->response_body.size() == expected.size()) << "Varying-size body length should match";
         expect(result->response_body == expected) << "Varying-size body content should match";
      }

      server.stop();
   };

   "sync_chunked_post_echo"_test = [] {
      chunked_test_server server;
      expect(server.start()) << "Server should start";

      glz::http_client client;
      std::string body = "request body data";
      auto result = client.post(server.base_url() + "/echo-chunked", body);

      expect(result.has_value()) << "POST request should succeed";
      if (result) {
         expect(result->status_code == 200) << "Status should be 200";
         expect(result->response_body == "echo:" + body) << "Echoed chunked POST response should match";
      }

      server.stop();
   };

   "sync_multiple_sequential_chunked_requests"_test = [] {
      chunked_test_server server;
      expect(server.start()) << "Server should start";

      // Use a fresh client per request because the streaming server closes
      // connections after each response, making pooled connections stale.
      for (int i = 0; i < 5; ++i) {
         glz::http_client client;
         auto result = client.get(server.base_url() + "/multi-chunk");
         expect(result.has_value()) << "Request " << i << " should succeed";
         if (result) {
            expect(result->status_code == 200);
            expect(result->response_body == "chunk1chunk2chunk3");
         }
      }

      server.stop();
   };

   "sync_interleaved_chunked_and_content_length"_test = [] {
      chunked_test_server server;
      expect(server.start()) << "Server should start";

      // Alternate between chunked and non-chunked responses.
      // Fresh client per request because streaming endpoints close connections.
      for (int i = 0; i < 4; ++i) {
         glz::http_client client;
         if (i % 2 == 0) {
            auto result = client.get(server.base_url() + "/multi-chunk");
            expect(result.has_value());
            if (result) {
               expect(result->response_body == "chunk1chunk2chunk3");
            }
         }
         else {
            auto result = client.get(server.base_url() + "/not-chunked");
            expect(result.has_value());
            if (result) {
               expect(result->response_body == "not chunked body");
            }
         }
      }

      server.stop();
   };

   "sync_concurrent_chunked_requests"_test = [] {
      chunked_test_server server;
      expect(server.start()) << "Server should start";

      const int num_threads = 4;
      std::vector<std::thread> threads;
      std::atomic<int> success_count{0};

      for (int i = 0; i < num_threads; ++i) {
         threads.emplace_back([&server, &success_count] {
            glz::http_client client;
            auto result = client.get(server.base_url() + "/multi-chunk");
            if (result.has_value() && result->status_code == 200 && result->response_body == "chunk1chunk2chunk3") {
               ++success_count;
            }
         });
      }

      for (auto& t : threads) t.join();

      expect(success_count == num_threads) << "All concurrent chunked requests should succeed";

      server.stop();
   };
};

// --------------------------------------------------------------------------
// Async tests (using get_async / post_async returning futures)
// --------------------------------------------------------------------------
suite chunked_async_tests = [] {
   "async_single_chunk"_test = [] {
      chunked_test_server server;
      expect(server.start()) << "Server should start";

      glz::http_client client;
      auto future = client.get_async(server.base_url() + "/single-chunk");

      auto result = future.get();
      expect(result.has_value()) << "Async request should succeed";
      if (result) {
         expect(result->status_code == 200);
         expect(result->response_body == "hello chunked world") << "Async chunked body should match";
      }

      server.stop();
   };

   "async_multi_chunk"_test = [] {
      chunked_test_server server;
      expect(server.start()) << "Server should start";

      glz::http_client client;
      auto future = client.get_async(server.base_url() + "/multi-chunk");

      auto result = future.get();
      expect(result.has_value()) << "Async request should succeed";
      if (result) {
         expect(result->status_code == 200);
         expect(result->response_body == "chunk1chunk2chunk3");
      }

      server.stop();
   };

   "async_empty_chunked"_test = [] {
      chunked_test_server server;
      expect(server.start()) << "Server should start";

      glz::http_client client;
      auto future = client.get_async(server.base_url() + "/empty-chunked");

      auto result = future.get();
      expect(result.has_value()) << "Async request should succeed";
      if (result) {
         expect(result->status_code == 200);
         expect(result->response_body.empty()) << "Async empty chunked body should be empty";
      }

      server.stop();
   };

   "async_large_chunked"_test = [] {
      chunked_test_server server;
      expect(server.start()) << "Server should start";

      glz::http_client client;
      auto future = client.get_async(server.base_url() + "/large-chunked");

      auto result = future.get();
      expect(result.has_value()) << "Async request should succeed";
      if (result) {
         expect(result->status_code == 200);
         expect(result->response_body.size() == 100'000u);
         bool all_x =
            std::all_of(result->response_body.begin(), result->response_body.end(), [](char c) { return c == 'X'; });
         expect(all_x) << "All bytes should be 'X'";
      }

      server.stop();
   };

   "async_binary_chunked"_test = [] {
      chunked_test_server server;
      expect(server.start()) << "Server should start";

      glz::http_client client;
      auto future = client.get_async(server.base_url() + "/binary-chunked");

      auto result = future.get();
      expect(result.has_value()) << "Async request should succeed";
      if (result) {
         std::string expected;
         expected.push_back('\x00');
         expected.push_back('\x01');
         expected.push_back('\xFF');
         expected.push_back('\x00');
         expected.append("text after nulls");
         expected.push_back('\x00');

         expect(result->response_body == expected) << "Async binary body should match";
      }

      server.stop();
   };

   "async_post_chunked_echo"_test = [] {
      chunked_test_server server;
      expect(server.start()) << "Server should start";

      glz::http_client client;
      std::string body = "async post data";
      auto future = client.post_async(server.base_url() + "/echo-chunked", body);

      auto result = future.get();
      expect(result.has_value()) << "Async POST should succeed";
      if (result) {
         expect(result->status_code == 200);
         expect(result->response_body == "echo:" + body);
      }

      server.stop();
   };

   "async_many_tiny_chunks"_test = [] {
      chunked_test_server server;
      expect(server.start()) << "Server should start";

      glz::http_client client;
      auto future = client.get_async(server.base_url() + "/many-tiny-chunks");

      auto result = future.get();
      expect(result.has_value()) << "Async request should succeed";
      if (result) {
         expect(result->response_body == "Hello!");
      }

      server.stop();
   };

   "async_varying_chunk_sizes"_test = [] {
      chunked_test_server server;
      expect(server.start()) << "Server should start";

      glz::http_client client;
      auto future = client.get_async(server.base_url() + "/varying-sizes");

      auto result = future.get();
      expect(result.has_value()) << "Async request should succeed";
      if (result) {
         std::string expected;
         expected += "A";
         expected += std::string(10, 'B');
         expected += std::string(100, 'C');
         expected += std::string(1000, 'D');
         expected += "E";
         expect(result->response_body == expected);
      }

      server.stop();
   };

   "async_multiple_concurrent_chunked"_test = [] {
      chunked_test_server server;
      expect(server.start()) << "Server should start";

      glz::http_client client;

      // Fire off multiple async requests concurrently
      auto f1 = client.get_async(server.base_url() + "/single-chunk");
      auto f2 = client.get_async(server.base_url() + "/multi-chunk");
      auto f3 = client.get_async(server.base_url() + "/json-chunked");
      auto f4 = client.get_async(server.base_url() + "/not-chunked");

      auto r1 = f1.get();
      auto r2 = f2.get();
      auto r3 = f3.get();
      auto r4 = f4.get();

      expect(r1.has_value());
      if (r1) expect(r1->response_body == "hello chunked world");

      expect(r2.has_value());
      if (r2) expect(r2->response_body == "chunk1chunk2chunk3");

      expect(r3.has_value());
      if (r3) expect(r3->response_body == R"({"status":"ok","value":42})");

      expect(r4.has_value());
      if (r4) expect(r4->response_body == "not chunked body");

      server.stop();
   };

   "async_callback_large_chunked"_test = [] {
      chunked_test_server server;
      expect(server.start()) << "Server should start";

      glz::http_client client;

      std::promise<std::expected<glz::response, std::error_code>> promise;
      auto future = promise.get_future();

      client.get_async(server.base_url() + "/large-chunked", {},
                       [&promise](std::expected<glz::response, std::error_code> result) mutable {
                          promise.set_value(std::move(result));
                       });

      auto status = future.wait_for(std::chrono::seconds(10));
      expect(status == std::future_status::ready) << "Callback should fire within timeout";

      if (status == std::future_status::ready) {
         auto result = future.get();
         expect(result.has_value());
         if (result) {
            expect(result->response_body.size() == 100'000u);
         }
      }

      server.stop();
   };
};

// --------------------------------------------------------------------------
// Streaming tests (using stream_request_v2 with on_data callbacks)
// --------------------------------------------------------------------------
suite chunked_streaming_tests = [] {
   "stream_single_chunk"_test = [] {
      chunked_test_server server;
      expect(server.start()) << "Server should start";

      glz::http_client client;

      std::string received_data;
      std::mutex data_mutex;
      std::atomic<int> chunk_count{0};
      std::promise<void> done_promise;
      auto done_future = done_promise.get_future();

      auto conn = client.stream_request_v2({
         .url = server.base_url() + "/single-chunk",
         .on_disconnect = [&] { done_promise.set_value(); },
         .on_data =
            [&](std::string_view data) {
               std::lock_guard lock(data_mutex);
               received_data.append(data);
               ++chunk_count;
            },
         .on_error = [](std::error_code) {},
      });

      auto status = done_future.wait_for(std::chrono::seconds(5));
      expect(status == std::future_status::ready) << "Stream should complete";

      std::lock_guard lock(data_mutex);
      expect(received_data == "hello chunked world") << "Streamed single chunk should match";

      server.stop();
   };

   "stream_multi_chunk_receives_all_data"_test = [] {
      chunked_test_server server;
      expect(server.start()) << "Server should start";

      glz::http_client client;

      std::string received_data;
      std::mutex data_mutex;
      std::atomic<int> chunk_count{0};
      std::promise<void> done_promise;
      auto done_future = done_promise.get_future();

      auto conn = client.stream_request_v2({
         .url = server.base_url() + "/multi-chunk",
         .on_disconnect = [&] { done_promise.set_value(); },
         .on_data =
            [&](std::string_view data) {
               std::lock_guard lock(data_mutex);
               received_data.append(data);
               ++chunk_count;
            },
         .on_error = [](std::error_code) {},
      });

      auto status = done_future.wait_for(std::chrono::seconds(5));
      expect(status == std::future_status::ready) << "Stream should complete";

      std::lock_guard lock(data_mutex);
      expect(received_data == "chunk1chunk2chunk3") << "Streamed multi-chunk should concatenate";

      server.stop();
   };

   "stream_large_chunked"_test = [] {
      chunked_test_server server;
      expect(server.start()) << "Server should start";

      glz::http_client client;

      std::string received_data;
      std::mutex data_mutex;
      std::promise<void> done_promise;
      auto done_future = done_promise.get_future();

      auto conn = client.stream_request_v2({
         .url = server.base_url() + "/large-chunked",
         .on_disconnect = [&] { done_promise.set_value(); },
         .on_data =
            [&](std::string_view data) {
               std::lock_guard lock(data_mutex);
               received_data.append(data);
            },
         .on_error = [](std::error_code) {},
      });

      auto status = done_future.wait_for(std::chrono::seconds(10));
      expect(status == std::future_status::ready) << "Stream should complete";

      std::lock_guard lock(data_mutex);
      expect(received_data.size() == 100'000u) << "Streamed large body should be 100KB";

      server.stop();
   };

   "stream_empty_chunked"_test = [] {
      chunked_test_server server;
      expect(server.start()) << "Server should start";

      glz::http_client client;

      std::string received_data;
      std::mutex data_mutex;
      std::promise<void> done_promise;
      auto done_future = done_promise.get_future();

      auto conn = client.stream_request_v2({
         .url = server.base_url() + "/empty-chunked",
         .on_disconnect = [&] { done_promise.set_value(); },
         .on_data =
            [&](std::string_view data) {
               std::lock_guard lock(data_mutex);
               received_data.append(data);
            },
         .on_error = [](std::error_code) {},
      });

      auto status = done_future.wait_for(std::chrono::seconds(5));
      expect(status == std::future_status::ready) << "Stream should complete";

      std::lock_guard lock(data_mutex);
      expect(received_data.empty()) << "Streamed empty chunked should produce no data";

      server.stop();
   };
};

// --------------------------------------------------------------------------
// Raw socket tests: send hand-crafted chunked HTTP responses to ensure
// the client parser handles edge cases correctly
// --------------------------------------------------------------------------
suite chunked_raw_socket_tests = [] {
   // Helper: start a raw TCP server that sends a pre-built HTTP response
   auto start_raw_server = [](uint16_t& out_port, const std::string& raw_response) -> std::thread {
      auto listener = std::make_shared<asio::io_context>();
      auto acceptor = std::make_shared<asio::ip::tcp::acceptor>(*listener);

      asio::ip::tcp::endpoint ep(asio::ip::make_address("127.0.0.1"), 0);
      acceptor->open(ep.protocol());
      acceptor->set_option(asio::socket_base::reuse_address(true));
      acceptor->bind(ep);
      acceptor->listen(1);

      out_port = acceptor->local_endpoint().port();

      return std::thread([listener, acceptor, raw_response] {
         asio::ip::tcp::socket socket(*listener);
         acceptor->accept(socket);

         // Read request (consume it fully so the socket doesn't hang)
         asio::streambuf req_buf;
         asio::error_code ec;
         asio::read_until(socket, req_buf, "\r\n\r\n", ec);

         // Send the raw response
         asio::write(socket, asio::buffer(raw_response), ec);

         // Shutdown gracefully
         socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
         socket.close(ec);
      });
   };

   "raw_basic_chunked"_test = [&] {
      uint16_t port = 0;
      std::string raw =
         "HTTP/1.1 200 OK\r\n"
         "Transfer-Encoding: chunked\r\n"
         "Content-Type: text/plain\r\n"
         "\r\n"
         "5\r\n"
         "Hello\r\n"
         "7\r\n"
         " World!\r\n"
         "0\r\n"
         "\r\n";

      auto server_thread = start_raw_server(port, raw);

      glz::http_client client;
      auto result = client.get("http://127.0.0.1:" + std::to_string(port) + "/test");

      server_thread.join();

      expect(result.has_value()) << "Raw chunked request should succeed";
      if (result) {
         expect(result->status_code == 200);
         expect(result->response_body == "Hello World!") << "Raw chunked body should be decoded";
      }
   };

   "raw_single_byte_chunks"_test = [&] {
      uint16_t port = 0;
      std::string raw =
         "HTTP/1.1 200 OK\r\n"
         "Transfer-Encoding: chunked\r\n"
         "\r\n"
         "1\r\n"
         "A\r\n"
         "1\r\n"
         "B\r\n"
         "1\r\n"
         "C\r\n"
         "0\r\n"
         "\r\n";

      auto server_thread = start_raw_server(port, raw);

      glz::http_client client;
      auto result = client.get("http://127.0.0.1:" + std::to_string(port) + "/test");

      server_thread.join();

      expect(result.has_value());
      if (result) {
         expect(result->response_body == "ABC");
      }
   };

   "raw_chunk_extension_ignored"_test = [&] {
      uint16_t port = 0;
      // Chunk extensions (;key=value) should be ignored per RFC 7230
      std::string raw =
         "HTTP/1.1 200 OK\r\n"
         "Transfer-Encoding: chunked\r\n"
         "\r\n"
         "5;ext=val\r\n"
         "Hello\r\n"
         "6;another-ext\r\n"
         " World\r\n"
         "0\r\n"
         "\r\n";

      auto server_thread = start_raw_server(port, raw);

      glz::http_client client;
      auto result = client.get("http://127.0.0.1:" + std::to_string(port) + "/test");

      server_thread.join();

      expect(result.has_value());
      if (result) {
         expect(result->response_body == "Hello World") << "Chunk extensions should be ignored";
      }
   };

   "raw_uppercase_hex_chunk_size"_test = [&] {
      uint16_t port = 0;
      // Use uppercase hex digits for chunk sizes
      std::string raw =
         "HTTP/1.1 200 OK\r\n"
         "Transfer-Encoding: chunked\r\n"
         "\r\n"
         "A\r\n"
         "0123456789\r\n"
         "a\r\n"
         "abcdefghij\r\n"
         "0\r\n"
         "\r\n";

      auto server_thread = start_raw_server(port, raw);

      glz::http_client client;
      auto result = client.get("http://127.0.0.1:" + std::to_string(port) + "/test");

      server_thread.join();

      expect(result.has_value());
      if (result) {
         expect(result->response_body == "0123456789abcdefghij")
            << "Hex chunk sizes (uppercase/lowercase) should parse";
      }
   };

   "raw_empty_chunked_body"_test = [&] {
      uint16_t port = 0;
      // Only the terminal chunk, no data chunks
      std::string raw =
         "HTTP/1.1 200 OK\r\n"
         "Transfer-Encoding: chunked\r\n"
         "\r\n"
         "0\r\n"
         "\r\n";

      auto server_thread = start_raw_server(port, raw);

      glz::http_client client;
      auto result = client.get("http://127.0.0.1:" + std::to_string(port) + "/test");

      server_thread.join();

      expect(result.has_value());
      if (result) {
         expect(result->response_body.empty()) << "Empty chunked response should have empty body";
      }
   };

   "raw_large_chunk_size_hex"_test = [&] {
      uint16_t port = 0;
      // A chunk with size > 255 to test multi-digit hex parsing
      std::string data(300, 'Z');
      // 300 decimal = 12c hex
      std::string raw =
         "HTTP/1.1 200 OK\r\n"
         "Transfer-Encoding: chunked\r\n"
         "\r\n"
         "12c\r\n" +
         data +
         "\r\n"
         "0\r\n"
         "\r\n";

      auto server_thread = start_raw_server(port, raw);

      glz::http_client client;
      auto result = client.get("http://127.0.0.1:" + std::to_string(port) + "/test");

      server_thread.join();

      expect(result.has_value());
      if (result) {
         expect(result->response_body.size() == 300u) << "300-byte chunk should decode correctly";
         expect(result->response_body == data);
      }
   };

   "raw_chunked_case_insensitive_header"_test = [&] {
      uint16_t port = 0;
      // Header name in mixed case
      std::string raw =
         "HTTP/1.1 200 OK\r\n"
         "transfer-ENCODING: chunked\r\n"
         "\r\n"
         "4\r\n"
         "test\r\n"
         "0\r\n"
         "\r\n";

      auto server_thread = start_raw_server(port, raw);

      glz::http_client client;
      auto result = client.get("http://127.0.0.1:" + std::to_string(port) + "/test");

      server_thread.join();

      expect(result.has_value());
      if (result) {
         expect(result->response_body == "test") << "Case-insensitive Transfer-Encoding should work";
      }
   };

   "raw_chunked_with_connection_close"_test = [&] {
      uint16_t port = 0;
      std::string raw =
         "HTTP/1.1 200 OK\r\n"
         "Transfer-Encoding: chunked\r\n"
         "Connection: close\r\n"
         "\r\n"
         "d\r\n"
         "close chunked\r\n"
         "0\r\n"
         "\r\n";

      auto server_thread = start_raw_server(port, raw);

      glz::http_client client;
      auto result = client.get("http://127.0.0.1:" + std::to_string(port) + "/test");

      server_thread.join();

      expect(result.has_value());
      if (result) {
         expect(result->response_body == "close chunked") << "Chunked + Connection: close should work";
      }
   };

   "raw_connection_reuse_after_chunked"_test = [&] {
      // Regression test: after a chunked response the connection must be clean
      // for reuse. A stale trailing CRLF left in the socket would corrupt the
      // next response parse.
      uint16_t port = 0;
      auto listener = std::make_shared<asio::io_context>();
      auto acceptor = std::make_shared<asio::ip::tcp::acceptor>(*listener);

      asio::ip::tcp::endpoint ep(asio::ip::make_address("127.0.0.1"), 0);
      acceptor->open(ep.protocol());
      acceptor->set_option(asio::socket_base::reuse_address(true));
      acceptor->bind(ep);
      acceptor->listen(1);
      port = acceptor->local_endpoint().port();

      // Server thread handles two requests on the same connection
      auto server_thread = std::thread([listener, acceptor] {
         asio::ip::tcp::socket socket(*listener);
         acceptor->accept(socket);
         asio::error_code ec;

         // --- First request: chunked response ---
         asio::streambuf req_buf;
         asio::read_until(socket, req_buf, "\r\n\r\n", ec);
         req_buf.consume(req_buf.size());

         std::string resp1 =
            "HTTP/1.1 200 OK\r\n"
            "Transfer-Encoding: chunked\r\n"
            "Connection: keep-alive\r\n"
            "\r\n"
            "5\r\n"
            "first\r\n"
            "0\r\n"
            "\r\n";
         asio::write(socket, asio::buffer(resp1), ec);

         // --- Second request: content-length response ---
         asio::read_until(socket, req_buf, "\r\n\r\n", ec);
         req_buf.consume(req_buf.size());

         std::string resp2 =
            "HTTP/1.1 200 OK\r\n"
            "Content-Length: 6\r\n"
            "Connection: close\r\n"
            "\r\n"
            "second";
         asio::write(socket, asio::buffer(resp2), ec);

         socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
         socket.close(ec);
      });

      std::string url = "http://127.0.0.1:" + std::to_string(port) + "/test";

      glz::http_client client;
      auto r1 = client.get(url);
      expect(r1.has_value()) << "First (chunked) request should succeed";
      if (r1) {
         expect(r1->response_body == "first");
      }

      auto r2 = client.get(url);
      expect(r2.has_value()) << "Second (reused connection) request should succeed";
      if (r2) {
         expect(r2->response_body == "second") << "Reused connection should parse cleanly";
      }

      server_thread.join();
   };

   "raw_malformed_chunk_size"_test = [&] {
      uint16_t port = 0;
      // Invalid hex in chunk size
      std::string raw =
         "HTTP/1.1 200 OK\r\n"
         "Transfer-Encoding: chunked\r\n"
         "\r\n"
         "ZZZ\r\n"
         "bad\r\n"
         "0\r\n"
         "\r\n";

      auto server_thread = start_raw_server(port, raw);

      glz::http_client client;
      auto result = client.get("http://127.0.0.1:" + std::to_string(port) + "/test");

      server_thread.join();

      expect(!result.has_value()) << "Malformed chunk size should return an error";
   };

   "raw_empty_chunk_size_line"_test = [&] {
      uint16_t port = 0;
      // Empty chunk size line (just CRLF where hex is expected)
      std::string raw =
         "HTTP/1.1 200 OK\r\n"
         "Transfer-Encoding: chunked\r\n"
         "\r\n"
         "\r\n";

      auto server_thread = start_raw_server(port, raw);

      glz::http_client client;
      auto result = client.get("http://127.0.0.1:" + std::to_string(port) + "/test");

      server_thread.join();

      expect(!result.has_value()) << "Empty chunk size line should return an error";
   };

   "raw_server_disconnect_mid_chunk"_test = [&] {
      // Server sends headers + partial chunk data then closes
      uint16_t port = 0;
      auto listener = std::make_shared<asio::io_context>();
      auto acceptor = std::make_shared<asio::ip::tcp::acceptor>(*listener);

      asio::ip::tcp::endpoint ep(asio::ip::make_address("127.0.0.1"), 0);
      acceptor->open(ep.protocol());
      acceptor->set_option(asio::socket_base::reuse_address(true));
      acceptor->bind(ep);
      acceptor->listen(1);
      port = acceptor->local_endpoint().port();

      auto server_thread = std::thread([listener, acceptor] {
         asio::ip::tcp::socket socket(*listener);
         acceptor->accept(socket);
         asio::error_code ec;

         asio::streambuf req_buf;
         asio::read_until(socket, req_buf, "\r\n\r\n", ec);

         // Promise 10 bytes but only send 3, then close
         std::string partial =
            "HTTP/1.1 200 OK\r\n"
            "Transfer-Encoding: chunked\r\n"
            "\r\n"
            "a\r\n"
            "onl";
         asio::write(socket, asio::buffer(partial), ec);

         // Close abruptly
         socket.close(ec);
      });

      glz::http_client client;
      auto result = client.get("http://127.0.0.1:" + std::to_string(port) + "/test");

      server_thread.join();

      expect(!result.has_value()) << "Server disconnect mid-chunk should return an error";
   };

   "raw_trailers_after_terminal_chunk"_test = [&] {
      uint16_t port = 0;
      // RFC 7230 allows trailer headers after the terminal chunk:
      // 0\r\n
      // Trailer-Header: value\r\n
      // \r\n
      std::string raw =
         "HTTP/1.1 200 OK\r\n"
         "Transfer-Encoding: chunked\r\n"
         "\r\n"
         "5\r\n"
         "Hello\r\n"
         "0\r\n"
         "Trailer-Header: trailer-value\r\n"
         "Another-Trailer: 123\r\n"
         "\r\n";

      auto server_thread = start_raw_server(port, raw);

      glz::http_client client;
      auto result = client.get("http://127.0.0.1:" + std::to_string(port) + "/test");

      server_thread.join();

      expect(result.has_value()) << "Chunked response with trailers should succeed";
      if (result) {
         expect(result->status_code == 200);
         expect(result->response_body == "Hello") << "Body should be decoded correctly despite trailers";
      }
   };

   "raw_trailers_connection_reuse"_test = [&] {
      // Verify trailers are fully consumed so connection reuse works
      uint16_t port = 0;
      auto listener = std::make_shared<asio::io_context>();
      auto acceptor = std::make_shared<asio::ip::tcp::acceptor>(*listener);

      asio::ip::tcp::endpoint ep(asio::ip::make_address("127.0.0.1"), 0);
      acceptor->open(ep.protocol());
      acceptor->set_option(asio::socket_base::reuse_address(true));
      acceptor->bind(ep);
      acceptor->listen(1);
      port = acceptor->local_endpoint().port();

      auto server_thread = std::thread([listener, acceptor] {
         asio::ip::tcp::socket socket(*listener);
         acceptor->accept(socket);
         asio::error_code ec;

         // First request: chunked with trailers
         asio::streambuf req_buf;
         asio::read_until(socket, req_buf, "\r\n\r\n", ec);
         req_buf.consume(req_buf.size());

         std::string resp1 =
            "HTTP/1.1 200 OK\r\n"
            "Transfer-Encoding: chunked\r\n"
            "Connection: keep-alive\r\n"
            "\r\n"
            "5\r\n"
            "first\r\n"
            "0\r\n"
            "X-Checksum: abc123\r\n"
            "\r\n";
         asio::write(socket, asio::buffer(resp1), ec);

         // Second request on same connection
         asio::read_until(socket, req_buf, "\r\n\r\n", ec);
         req_buf.consume(req_buf.size());

         std::string resp2 =
            "HTTP/1.1 200 OK\r\n"
            "Content-Length: 6\r\n"
            "Connection: close\r\n"
            "\r\n"
            "second";
         asio::write(socket, asio::buffer(resp2), ec);

         socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
         socket.close(ec);
      });

      std::string url = "http://127.0.0.1:" + std::to_string(port) + "/test";

      glz::http_client client;
      auto r1 = client.get(url);
      expect(r1.has_value()) << "First request (chunked + trailers) should succeed";
      if (r1) {
         expect(r1->response_body == "first");
      }

      auto r2 = client.get(url);
      expect(r2.has_value()) << "Second request (reused after trailers) should succeed";
      if (r2) {
         expect(r2->response_body == "second") << "Connection reuse after trailers should work";
      }

      server_thread.join();
   };

   "raw_leading_zeros_chunk_size"_test = [&] {
      uint16_t port = 0;
      // Leading zeros in chunk size should be valid
      std::string raw =
         "HTTP/1.1 200 OK\r\n"
         "Transfer-Encoding: chunked\r\n"
         "\r\n"
         "005\r\n"
         "Hello\r\n"
         "0007\r\n"
         " World!\r\n"
         "00000\r\n"
         "\r\n";

      auto server_thread = start_raw_server(port, raw);

      glz::http_client client;
      auto result = client.get("http://127.0.0.1:" + std::to_string(port) + "/test");

      server_thread.join();

      expect(result.has_value());
      if (result) {
         expect(result->response_body == "Hello World!") << "Leading zeros in chunk size should parse correctly";
      }
   };

   "raw_multiple_transfer_encodings"_test = [&] {
      uint16_t port = 0;
      // RFC 7230 allows multiple encodings: "gzip, chunked"
      // The implementation uses value.find("chunked"), which should match
      std::string raw =
         "HTTP/1.1 200 OK\r\n"
         "Transfer-Encoding: gzip, chunked\r\n"
         "\r\n"
         "4\r\n"
         "data\r\n"
         "0\r\n"
         "\r\n";

      auto server_thread = start_raw_server(port, raw);

      glz::http_client client;
      auto result = client.get("http://127.0.0.1:" + std::to_string(port) + "/test");

      server_thread.join();

      expect(result.has_value()) << "Multiple transfer encodings with chunked should be detected";
      if (result) {
         // Note: we don't decompress gzip, but chunked framing should still be decoded
         expect(result->response_body == "data");
      }
   };
};

int main() { return 0; }
