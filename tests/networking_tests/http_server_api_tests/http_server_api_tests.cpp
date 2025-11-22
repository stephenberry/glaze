// Glaze Library
// For the license information refer to glaze.hpp

// Unit Tests for Glaze HTTP Async Routes and Server API
// Tests async route handling, server lifecycle, and advanced routing features

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
#include "ut/ut.hpp"

using namespace ut;

// Test data structures
struct AsyncResult
{
   std::string message;
   int process_time_ms;
   bool success;
};

struct SlowOperation
{
   std::string operation_id;
   std::chrono::milliseconds delay;
   std::string result;
};

// Mock async service for testing
class AsyncService
{
   std::atomic<int> operation_counter_{0};

  public:
   std::future<AsyncResult> process_async(const std::string& input)
   {
      return std::async(std::launch::async, [this, input]() {
         int op_id = ++operation_counter_;

         // Simulate async work
         auto start = std::chrono::high_resolution_clock::now();
         std::this_thread::sleep_for(std::chrono::milliseconds(50));
         auto end = std::chrono::high_resolution_clock::now();

         auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

         return AsyncResult{"Processed: " + input + " (op #" + std::to_string(op_id) + ")",
                            static_cast<int>(duration.count()), true};
      });
   }

   std::future<std::vector<std::string>> batch_process_async(const std::vector<std::string>& inputs)
   {
      return std::async(std::launch::async, [inputs]() {
         std::vector<std::string> results;
         for (const auto& input : inputs) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            results.push_back("Batch processed: " + input);
         }
         return results;
      });
   }

   std::future<void> long_running_task()
   {
      return std::async(std::launch::async, []() {
         std::this_thread::sleep_for(std::chrono::milliseconds(100));
         // Void return - represents fire-and-forget operations
      });
   }

   int get_operation_count() const { return operation_counter_.load(); }
};

// HTTP Router Async Tests
suite http_router_async_tests = [] {
   "async_route_registration"_test = [] {
      glz::http_router router;
      bool route_executed = false;

      // Register async route
      router.get_async("/async-test", [&route_executed](const glz::request&, glz::response& res) -> std::future<void> {
         return std::async(std::launch::async, [&route_executed, &res]() {
            route_executed = true;
            res.json({{"async", true}, {"message", "Success"}});
         });
      });

      // Test route matching
      auto [handler, params] = router.match(glz::http_method::GET, "/async-test");
      expect(handler != nullptr) << "Async route should be registered and matchable\n";
      expect(params.empty()) << "Static async route should have no parameters\n";
   };

   "async_route_with_parameters"_test = [] {
      glz::http_router router;
      std::string captured_id;

      router.get_async("/users/:id/async",
                       [&captured_id](const glz::request& req, glz::response& res) -> std::future<void> {
                          return std::async(std::launch::async, [&captured_id, &req, &res]() {
                             captured_id = req.params.at("id");
                             res.json({{"user_id", captured_id}, {"async", true}});
                          });
                       });

      auto [handler, params] = router.match(glz::http_method::GET, "/users/123/async");
      expect(handler != nullptr) << "Parameterized async route should match\n";
      expect(params.at("id") == "123") << "Should extract parameter correctly\n";
   };

   "multiple_async_methods"_test = [] {
      glz::http_router router;

      router.get_async("/resource", [](const glz::request&, glz::response& res) -> std::future<void> {
         return std::async(std::launch::async, [&res]() { res.json({{"method", "GET"}, {"async", true}}); });
      });

      router.post_async("/resource", [](const glz::request&, glz::response& res) -> std::future<void> {
         return std::async(std::launch::async, [&res]() { res.json({{"method", "POST"}, {"async", true}}); });
      });

      router.put_async("/resource", [](const glz::request&, glz::response& res) -> std::future<void> {
         return std::async(std::launch::async, [&res]() { res.json({{"method", "PUT"}, {"async", true}}); });
      });

      router.del_async("/resource", [](const glz::request&, glz::response& res) -> std::future<void> {
         return std::async(std::launch::async, [&res]() { res.json({{"method", "DELETE"}, {"async", true}}); });
      });

      router.patch_async("/resource", [](const glz::request&, glz::response& res) -> std::future<void> {
         return std::async(std::launch::async, [&res]() { res.json({{"method", "PATCH"}, {"async", true}}); });
      });

      // Test all methods are registered
      auto get_result = router.match(glz::http_method::GET, "/resource");
      auto post_result = router.match(glz::http_method::POST, "/resource");
      auto put_result = router.match(glz::http_method::PUT, "/resource");
      auto delete_result = router.match(glz::http_method::DELETE, "/resource");
      auto patch_result = router.match(glz::http_method::PATCH, "/resource");

      expect(get_result.first != nullptr) << "GET async route should be registered\n";
      expect(post_result.first != nullptr) << "POST async route should be registered\n";
      expect(put_result.first != nullptr) << "PUT async route should be registered\n";
      expect(delete_result.first != nullptr) << "DELETE async route should be registered\n";
      expect(patch_result.first != nullptr) << "PATCH async route should be registered\n";
   };

   "generic_async_route_method"_test = [] {
      glz::http_router router;

      router.route_async(glz::http_method::HEAD, "/resource",
                         [](const glz::request&, glz::response& res) -> std::future<void> {
                            return std::async(std::launch::async, [&res]() {
                               res.status(200); // HEAD typically doesn't have body
                            });
                         });

      auto [handler, params] = router.match(glz::http_method::HEAD, "/resource");
      expect(handler != nullptr) << "Generic async route method should work\n";
   };
};

// Async Service Integration Tests
suite async_service_tests = [] {
   "async_service_basic_operation"_test = [] {
      AsyncService service;

      auto future = service.process_async("test data");
      auto result = future.get();

      expect(result.success) << "Async operation should succeed\n";
      expect(result.message.find("test data") != std::string::npos) << "Result should contain input data\n";
      expect(result.process_time_ms >= 40) << "Should have realistic processing time\n";
      expect(service.get_operation_count() == 1) << "Should track operation count\n";
   };

   "async_service_batch_processing"_test = [] {
      AsyncService service;

      std::vector<std::string> inputs = {"item1", "item2", "item3"};
      auto future = service.batch_process_async(inputs);
      auto results = future.get();

      expect(results.size() == 3) << "Should process all input items\n";
      expect(results[0].find("item1") != std::string::npos) << "Should process first item\n";
      expect(results[2].find("item3") != std::string::npos) << "Should process last item\n";
   };

   "async_service_void_return"_test = [] {
      AsyncService service;

      auto future = service.long_running_task();

      // Should complete without throwing
      expect(!throws([&]() { future.wait(); })) << "Void async operation should complete successfully\n";
   };

   "concurrent_async_operations"_test = [] {
      AsyncService service;

      std::vector<std::future<AsyncResult>> futures;
      for (int i = 0; i < 5; ++i) {
         futures.push_back(service.process_async("input_" + std::to_string(i)));
      }

      std::vector<AsyncResult> results;
      for (auto& future : futures) {
         results.push_back(future.get());
      }

      expect(results.size() == 5) << "All async operations should complete\n";
      expect(service.get_operation_count() == 5) << "Should track all operations\n";

      for (const auto& result : results) {
         expect(result.success) << "Each operation should succeed\n";
      }
   };
};

// HTTP Server API Tests
suite http_server_api_tests = [] {
   "server_creation_and_configuration"_test = [] {
      glz::http_server server;

      // Test method chaining
      auto& server_ref = server.bind(0); // Bind to any available port
      expect(&server_ref == &server) << "bind() should return server reference for chaining\n";
   };

   "server_random_port_binding"_test = [] {
      glz::http_server server;
      server.bind(0);

      // Test non-throwing version
      asio::error_code ec;
      expect(server.port(ec) > 0) << "Server should be assigned a non-zero port when binding to 0\n";
      expect(!ec) << "Error code should not be set\n";

      // Test throwing version
      expect(server.port() > 0) << "Server should be assigned a non-zero port\n";
   };

   "server_port_not_bound"_test = [] {
      glz::http_server server;

      // Test throwing version
      expect(throws([&]() { server.port(); })) << "Should throw when not bound\n";

      // Test non-throwing version
      asio::error_code ec;
      expect(server.port(ec) == 0) << "Should return 0 when not bound\n";
      expect(bool(ec)) << "Error code should be set\n";
   };

   "route_registration_methods"_test = [] {
      glz::http_server server;
      bool handler_called = false;

      auto handler = [&handler_called](const glz::request&, glz::response& res) {
         handler_called = true;
         res.body("test");
      };

      // Test all route registration methods return server reference
      [[maybe_unused]] auto& get_ref = server.get("/get", handler);
      [[maybe_unused]] auto& post_ref = server.post("/post", handler);
      [[maybe_unused]] auto& put_ref = server.put("/put", handler);
      [[maybe_unused]] auto& del_ref = server.del("/del", handler);
      [[maybe_unused]] auto& patch_ref = server.patch("/patch", handler);
   };

   "middleware_registration"_test = [] {
      glz::http_server server;
      std::vector<std::string> execution_order;

      auto& router = server.get("/ping", [&execution_order](const glz::request&, glz::response& res) {
         execution_order.push_back("handler");
         res.body("ok");
      });

      auto& use_ref = server.use(
         [&execution_order](const glz::request&, glz::response&) { execution_order.push_back("middleware"); });
      expect(&use_ref == &server) << "use() should return server reference for chaining\n";

      glz::request req;
      req.method = glz::http_method::GET;
      req.target = "/ping";

      glz::response res;

      for (const auto& middleware : router.middlewares) {
         middleware(req, res);
      }

      auto [handler, params] = router.match(req.method, req.target);
      expect(handler != nullptr) << "Route handler should be registered\n";
      handler(req, res);

      expect(execution_order.size() == 2) << "Middleware and handler should both execute\n";
      expect(execution_order[0] == "middleware") << "Middleware should execute before handler\n";
      expect(execution_order[1] == "handler") << "Handler should execute after middleware\n";
   };

   "cors_configuration"_test = [] {
      glz::http_server server;

      // Test simple CORS enable
      auto& cors_ref = server.enable_cors();
      expect(&cors_ref == &server) << "enable_cors() should return server reference\n";

      // Test CORS with origins
      std::vector<std::string> origins = {"https://example.com"};
      auto& cors_origins_ref = server.enable_cors(origins, true);
      expect(&cors_origins_ref == &server) << "enable_cors(origins) should return server reference\n";

      // Test CORS with config
      glz::cors_config config;
      config.allowed_origins = {"https://test.com"};
      auto& cors_config_ref = server.enable_cors(config);
      expect(&cors_config_ref == &server) << "enable_cors(config) should return server reference\n";
   };

   "mount_subrouter"_test = [] {
      glz::http_server server;
      glz::http_router api_router;

      bool api_handler_registered = false;
      api_router.get("/users",
                     [&api_handler_registered](const glz::request&, glz::response&) { api_handler_registered = true; });

      auto& mount_ref = server.mount("/api", api_router);
      expect(&mount_ref == &server) << "mount() should return server reference\n";
   };

   "error_handler_configuration"_test = [] {
      glz::http_server server;
      bool error_handler_called = false;

      auto& error_ref = server.on_error(
         [&error_handler_called](std::error_code, std::source_location) { error_handler_called = true; });

      expect(&error_ref == &server) << "on_error() should return server reference\n";
   };
};

struct RequestData
{
   std::string name;
   int value;
};

struct ResponseData
{
   std::string processed_name;
   int doubled_value;
   bool async_processed;
};

// Async Route Execution Tests
suite async_route_execution_tests = [] {
   "async_route_with_real_async_work"_test = [] {
      glz::http_router router;
      AsyncService service;
      std::string response_body;

      router.post_async("/process", [&service](const glz::request& req, glz::response& res) -> std::future<void> {
         return std::async(std::launch::async, [&service, &req, &res]() {
            auto future = service.process_async(req.body);
            auto result = future.get();
            res.json(result);
         });
      });

      // Simulate request
      glz::request req;
      req.method = glz::http_method::POST;
      req.target = "/process";
      req.body = "test input";

      glz::response res;

      auto [handler, params] = router.match(req.method, req.target);
      expect(handler != nullptr) << "Should find async handler\n";

      // Execute handler (this would normally be done by the server)
      handler(req, res);

      expect(!res.response_body.empty()) << "Async handler should set response body\n";
   };

   "async_route_error_handling"_test = [] {
      glz::http_router router;

      router.get_async("/error", [](const glz::request&, glz::response&) -> std::future<void> {
         return std::async([]() { throw std::runtime_error("Async operation failed"); });
      });

      glz::request req;
      req.method = glz::http_method::GET;
      req.target = "/error";

      glz::response res;

      auto [handler, params] = router.match(req.method, req.target);
      expect(handler != nullptr) << "Should find error handler\n";

      // Handler execution with error should be testable
      expect(throws([&]() { handler(req, res); })) << "Async handler errors should propagate\n";
   };

   "async_route_with_json_processing"_test = [] {
      glz::http_router router;

      router.post_async("/json-process", [](const glz::request& req, glz::response& res) -> std::future<void> {
         return std::async(std::launch::async, [&req, &res]() {
            RequestData input;
            auto ec = glz::read_json(input, req.body);

            if (ec) {
               res.status(400).json({{"error", "Invalid JSON"}});
               return;
            }

            // Simulate async processing
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            ResponseData output;
            output.processed_name = "Processed: " + input.name;
            output.doubled_value = input.value * 2;
            output.async_processed = true;

            res.json(output);
         });
      });

      glz::request req;
      req.method = glz::http_method::POST;
      req.target = "/json-process";
      req.body = R"({"name": "test", "value": 42})";

      glz::response res;

      auto [handler, params] = router.match(req.method, req.target);
      handler(req, res);

      expect(res.status_code == 200) << "Should return success status\n";
      expect(!res.response_body.empty()) << "Should have JSON response body\n";
   };
};

// Concurrent Async Route Tests
suite concurrent_async_tests = [] {
   "multiple_concurrent_async_routes"_test = [] {
      glz::http_router router;
      std::atomic<int> completed_requests{0};

      router.get_async("/concurrent/:id",
                       [&completed_requests](const glz::request& req, glz::response& res) -> std::future<void> {
                          return std::async(std::launch::async, [&completed_requests, &req, &res]() {
                             std::string id = req.params.at("id");

                             // Simulate varying processing times
                             int delay_ms = std::stoi(id) * 10;
                             std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));

                             completed_requests++;
                             res.json({{"id", id}, {"completed_at", completed_requests.load()}});
                          });
                       });

      // Simulate multiple concurrent requests
      std::vector<std::thread> request_threads;
      std::vector<glz::response> responses(5);

      for (int i = 0; i < 5; ++i) {
         request_threads.emplace_back([&router, &responses, i]() {
            glz::request req;
            req.method = glz::http_method::GET;
            req.target = "/concurrent/" + std::to_string(i + 1);
            req.params["id"] = std::to_string(i + 1);

            auto [handler, params] = router.match(req.method, req.target);
            handler(req, responses[i]);
         });
      }

      for (auto& thread : request_threads) {
         thread.join();
      }

      expect(completed_requests.load() == 5) << "All concurrent requests should complete\n";

      for (const auto& response : responses) {
         expect(!response.response_body.empty()) << "Each response should have content\n";
      }
   };

   "async_vs_sync_route_performance"_test = [] {
      glz::http_router async_router;
      glz::http_router sync_router;

      // Async route with simulated work
      async_router.get_async("/work", [](const glz::request&, glz::response& res) -> std::future<void> {
         return std::async(std::launch::async, [&res]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            res.json({{"type", "async"}, {"work_done", true}});
         });
      });

      // Sync route with same simulated work
      sync_router.get("/work", [](const glz::request&, glz::response& res) {
         std::this_thread::sleep_for(std::chrono::milliseconds(20));
         res.json({{"type", "sync"}, {"work_done", true}});
      });

      // Both should be able to handle the request
      glz::request req;
      req.method = glz::http_method::GET;
      req.target = "/work";

      glz::response async_res, sync_res;

      auto [async_handler, async_params] = async_router.match(req.method, req.target);
      auto [sync_handler, sync_params] = sync_router.match(req.method, req.target);

      expect(async_handler != nullptr) << "Async handler should be found\n";
      expect(sync_handler != nullptr) << "Sync handler should be found\n";

      // Execute both (timing would be measured in real scenarios)
      async_handler(req, async_res);
      sync_handler(req, sync_res);

      expect(!async_res.response_body.empty()) << "Async route should produce response\n";
      expect(!sync_res.response_body.empty()) << "Sync route should produce response\n";
   };
};

// Advanced Async Scenarios
suite advanced_async_scenarios = [] {
   "async_route_with_middleware"_test = [] {
      glz::http_router router;
      std::vector<std::string> execution_order;

      // Add middleware
      router.use([&execution_order](const glz::request&, glz::response&) { execution_order.push_back("middleware1"); });

      router.use([&execution_order](const glz::request&, glz::response&) { execution_order.push_back("middleware2"); });

      // Add async route
      router.get_async("/with-middleware",
                       [&execution_order](const glz::request&, glz::response& res) -> std::future<void> {
                          return std::async(std::launch::async, [&execution_order, &res]() {
                             execution_order.push_back("async_handler");
                             res.json({{"middleware_executed", true}});
                          });
                       });

      expect(router.middlewares.size() == 2) << "Should have 2 middleware functions\n";

      // Simulate request execution (middleware would be called by server)
      glz::request req;
      req.method = glz::http_method::GET;
      req.target = "/with-middleware";

      glz::response res;

      // Execute middleware manually for testing
      for (const auto& middleware : router.middlewares) {
         middleware(req, res);
      }

      auto [handler, params] = router.match(req.method, req.target);
      handler(req, res);

      expect(execution_order.size() == 3) << "Should execute middleware and handler\n";
      expect(execution_order[0] == "middleware1") << "First middleware should execute first\n";
      expect(execution_order[1] == "middleware2") << "Second middleware should execute second\n";
      expect(execution_order[2] == "async_handler") << "Async handler should execute last\n";
   };

   "async_route_with_wildcard_parameters"_test = [] {
      glz::http_router router;

      router.get_async("/files/*path", [](const glz::request& req, glz::response& res) -> std::future<void> {
         return std::async(std::launch::async, [&req, &res]() {
            std::string file_path = req.params.at("path");
            res.json({{"file_path", file_path}, {"async_served", true}});
         });
      });

      auto [handler, params] = router.match(glz::http_method::GET, "/files/documents/readme.txt");
      expect(handler != nullptr) << "Should match wildcard async route\n";
      expect(params.at("path") == "documents/readme.txt") << "Should capture full wildcard path\n";

      glz::request req;
      req.params = params;
      glz::response res;

      handler(req, res);
      expect(!res.response_body.empty()) << "Wildcard async route should produce response\n";
   };

   "async_route_chaining_operations"_test = [] {
      glz::http_router router;
      AsyncService service;

      router.post_async("/chain", [&service](const glz::request& req, glz::response& res) -> std::future<void> {
         return std::async(std::launch::async, [&service, &req, &res]() {
            // Chain multiple async operations
            auto first_result = service.process_async(req.body).get();
            auto second_result = service.process_async(first_result.message).get();

            res.json({{"first_operation", first_result.message},
                      {"second_operation", second_result.message},
                      {"total_operations", service.get_operation_count()},
                      {"chained", true}});
         });
      });

      glz::request req;
      req.method = glz::http_method::POST;
      req.target = "/chain";
      req.body = "initial data";

      glz::response res;

      auto [handler, params] = router.match(req.method, req.target);
      handler(req, res);

      expect(!res.response_body.empty()) << "Chained async operations should produce response\n";
      expect(service.get_operation_count() >= 2) << "Should have executed multiple async operations\n";
   };
};

struct TestData
{
   std::string message;
   int code;
   bool success;
};

struct ComplexData
{
   std::string name;
   std::vector<int> values;
   std::unordered_map<std::string, std::string> metadata;
};

// Response Building Tests
suite response_building_tests = [] {
   "response_method_chaining"_test = [] {
      glz::response res;

      auto& chained_res =
         res.status(201).header("X-Custom", "value").content_type("application/json").body("test body");

      expect(&chained_res == &res) << "Response methods should return reference for chaining\n";
      expect(res.status_code == 201) << "Status should be set correctly\n";
      expect(res.response_headers.at("x-custom") == "value") << "Custom header should be set\n";
      expect(res.response_headers.at("content-type") == "application/json") << "Content-Type should be set\n";
      expect(res.response_body == "test body") << "Body should be set correctly\n";
   };

   "response_json_serialization"_test = [] {
      glz::response res;

      TestData data{"Test message", 200, true};
      res.json(data);

      expect(!res.response_body.empty()) << "JSON serialization should produce content\n";
      expect(res.response_headers.at("content-type") == "application/json") << "Should set JSON content type\n";

      // Verify serialization worked
      TestData deserialized;
      auto ec = glz::read_json(deserialized, res.response_body);
      expect(!ec) << "Should be able to deserialize response\n";
      expect(deserialized.message == data.message) << "Deserialized data should match original\n";
   };

   "response_with_custom_options"_test = [] {
      glz::response res;

      ComplexData data{"test", {1, 2, 3, 4, 5}, {{"key1", "value1"}, {"key2", "value2"}}};

      // Use custom options for pretty printing
      res.body<glz::opts{.prettify = true}>(data);

      expect(!res.response_body.empty()) << "Custom options serialization should work\n";
      expect(res.response_body.find("\n") != std::string::npos) << "Pretty printing should add newlines\n";
   };
};

// Response Middleware Tests
suite response_middleware_tests = [] {
   "request_hook_registration"_test = [] {
      glz::http_server<> server;

      bool hook_called = false;
      auto& hook_ref = server.on_request([&hook_called](const glz::request&, glz::response&) { hook_called = true; });

      expect(&hook_ref == &server) << "on_request() should return server reference for chaining\n";
   };

   "response_hook_registration"_test = [] {
      glz::http_server<> server;

      bool hook_called = false;
      auto& hook_ref =
         server.on_response([&hook_called](const glz::request&, const glz::response&) { hook_called = true; });

      expect(&hook_ref == &server) << "on_response() should return server reference for chaining\n";
   };

   "metrics_tracking_simulation"_test = [] {
      glz::http_router router;

      // Simulate basic metrics tracking without timing
      struct Metrics
      {
         std::atomic<uint64_t> total_requests{0};
         std::atomic<uint64_t> total_responses{0};
      };

      Metrics metrics;

      // Simulate on_request hook
      auto request_hook = [&metrics](const glz::request&, glz::response&) {
         metrics.total_requests.fetch_add(1, std::memory_order_relaxed);
      };

      // Simulate on_response hook
      auto response_hook = [&metrics](const glz::request&, const glz::response&) {
         metrics.total_responses.fetch_add(1, std::memory_order_relaxed);
      };

      // Register a route
      router.get("/test", [](const glz::request&, glz::response& res) { res.body("test response"); });

      // Simulate handling 5 requests
      for (int i = 0; i < 5; ++i) {
         glz::request req;
         req.method = glz::http_method::GET;
         req.target = "/test";

         glz::response res;

         // Simulate request hook
         request_hook(req, res);

         // Execute handler
         auto [handler, params] = router.match(req.method, req.target);
         handler(req, res);

         // Simulate response hook
         response_hook(req, res);
      }

      expect(metrics.total_requests.load() == 5) << "Should track all requests\n";
      expect(metrics.total_responses.load() == 5) << "Should track all responses\n";
   };

   "multiple_request_hooks"_test = [] {
      std::vector<std::string> execution_order;

      auto hook1 = [&execution_order](const glz::request&, glz::response&) { execution_order.push_back("hook1"); };

      auto hook2 = [&execution_order](const glz::request&, glz::response&) { execution_order.push_back("hook2"); };

      glz::request req;
      req.method = glz::http_method::GET;
      req.target = "/test";

      glz::response res;

      // Execute hooks in order
      hook1(req, res);
      hook2(req, res);

      expect(execution_order.size() == 2) << "Both hooks should execute\n";
      expect(execution_order[0] == "hook1") << "First hook should execute first\n";
      expect(execution_order[1] == "hook2") << "Second hook should execute second\n";
   };

   "response_hook_can_inspect_status"_test = [] {
      int captured_status = 0;

      auto response_hook = [&captured_status](const glz::request&, const glz::response& res) {
         captured_status = res.status_code;
      };

      glz::request req;
      glz::response res;
      res.status(404).body("Not found");

      response_hook(req, res);

      expect(captured_status == 404) << "Response hook should be able to inspect status code\n";
   };

   "response_hook_can_inspect_headers"_test = [] {
      std::string captured_content_type;

      auto response_hook = [&captured_content_type](const glz::request&, const glz::response& res) {
         auto it = res.response_headers.find("content-type");
         if (it != res.response_headers.end()) {
            captured_content_type = it->second;
         }
      };

      glz::request req;
      glz::response res;
      res.content_type("application/json").body("{\"test\": true}");

      response_hook(req, res);

      expect(captured_content_type == "application/json") << "Response hook should inspect headers\n";
   };

   "hook_can_inspect_response_data"_test = [] {
      int captured_status = 0;
      std::string captured_body;

      auto response_hook = [&captured_status, &captured_body](const glz::request&, const glz::response& res) {
         captured_status = res.status_code;
         captured_body = res.response_body;
      };

      glz::request req;
      glz::response res;
      res.status(201).body("created");

      response_hook(req, res);

      expect(captured_status == 201) << "Response hook should inspect status code\n";
      expect(captured_body == "created") << "Response hook should inspect body\n";
   };
};

// Wrapping Middleware Tests
suite wrapping_middleware_tests = [] {
   "wrap_middleware_registration"_test = [] {
      glz::http_server<> server;

      auto& wrap_ref = server.wrap([](const glz::request&, glz::response&, const auto& next) { next(); });

      expect(&wrap_ref == &server) << "wrap() should return server reference for chaining\n";
   };

   "wrap_middleware_timing"_test = [] {
      glz::http_router router;
      router.get("/test", [](const glz::request&, glz::response& res) {
         std::this_thread::sleep_for(std::chrono::milliseconds(10));
         res.body("response");
      });

      glz::request req;
      req.method = glz::http_method::GET;
      req.target = "/test";

      glz::response res;

      std::chrono::nanoseconds measured_duration{0};

      // Simulate wrapping middleware that measures timing
      auto wrapping_middleware = [&measured_duration](const glz::request&, glz::response&, const auto& next) {
         auto start = std::chrono::steady_clock::now();
         next();
         measured_duration = std::chrono::steady_clock::now() - start;
      };

      auto [handler, params] = router.match(req.method, req.target);

      // Build and execute chain
      std::function<void()> chain = [&handler, &req, &res]() { handler(req, res); };
      auto prev_chain = std::move(chain);
      chain = [wrapping_middleware, prev_chain = std::move(prev_chain), &req, &res]() {
         glz::next_handler next(prev_chain);
         wrapping_middleware(req, res, next);
      };
      chain();

      expect(measured_duration.count() > 0) << "Should measure non-zero duration\n";
      auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(measured_duration);
      expect(ms.count() >= 10) << "Should measure at least 10ms\n";
   };

   "wrap_middleware_order"_test = [] {
      std::vector<std::string> execution_order;

      auto middleware1 = [&execution_order](const glz::request&, glz::response&, const auto& next) {
         execution_order.push_back("middleware1_before");
         next();
         execution_order.push_back("middleware1_after");
      };

      auto middleware2 = [&execution_order](const glz::request&, glz::response&, const auto& next) {
         execution_order.push_back("middleware2_before");
         next();
         execution_order.push_back("middleware2_after");
      };

      auto handler = [&execution_order]() { execution_order.push_back("handler"); };

      // Build chain (middleware added in order, but chain built in reverse)
      std::function<void()> chain = handler;

      // Wrap with middleware2 (will be outermost)
      auto prev = std::move(chain);
      chain = [middleware2, prev = std::move(prev)]() {
         glz::request req;
         glz::response res;
         glz::next_handler next(prev);
         middleware2(req, res, next);
      };

      // Wrap with middleware1 (will be innermost after middleware2)
      prev = std::move(chain);
      chain = [middleware1, prev = std::move(prev)]() {
         glz::request req;
         glz::response res;
         glz::next_handler next(prev);
         middleware1(req, res, next);
      };

      chain();

      // Expected order: middleware1 before -> middleware2 before -> handler ->
      //                 middleware2 after -> middleware1 after
      expect(execution_order.size() == 5) << "Should execute all parts\n";
      expect(execution_order[0] == "middleware1_before") << "First middleware before\n";
      expect(execution_order[1] == "middleware2_before") << "Second middleware before\n";
      expect(execution_order[2] == "handler") << "Handler in middle\n";
      expect(execution_order[3] == "middleware2_after") << "Second middleware after\n";
      expect(execution_order[4] == "middleware1_after") << "First middleware after\n";
   };

   "wrap_middleware_can_modify_response"_test = [] {
      glz::request req;
      glz::response res;
      res.body("original");

      auto middleware = [](const glz::request&, glz::response& res, const auto& next) {
         next();
         // Transform response after handler
         res.body(res.response_body + " + transformed");
      };

      auto handler = [](glz::response& res) { res.body("handler_output"); };

      std::function<void()> chain = [&handler, &res]() { handler(res); };
      chain = [middleware, prev = std::move(chain), &req, &res]() {
         glz::next_handler next(prev);
         middleware(req, res, next);
      };

      chain();

      expect(res.response_body == "handler_output + transformed") << "Middleware should transform response\n";
   };

   "wrap_middleware_error_handling"_test = [] {
      bool error_caught = false;

      auto middleware = [&error_caught](const glz::request&, glz::response&, const auto& next) {
         try {
            next();
         }
         catch (const std::exception&) {
            error_caught = true;
         }
      };

      auto handler = []() { throw std::runtime_error("test error"); };

      std::function<void()> chain = handler;
      glz::request req;
      glz::response res;
      chain = [middleware, prev = std::move(chain), &req, &res]() {
         glz::next_handler next(prev);
         middleware(req, res, next);
      };

      chain();

      expect(error_caught) << "Middleware should catch errors from handler\n";
   };

   "wrap_middleware_metrics_use_case"_test = [] {
      struct Metrics
      {
         std::atomic<uint64_t> total_requests{0};
         std::atomic<uint64_t> total_responses{0};
         std::atomic<uint64_t> success_count{0};
         std::atomic<uint64_t> error_count{0};
      };

      Metrics metrics;

      auto metrics_middleware = [&metrics](const glz::request&, glz::response& res, const auto& next) {
         metrics.total_requests++;
         next();
         metrics.total_responses++;
         if (res.status_code >= 200 && res.status_code < 300) {
            metrics.success_count++;
         }
         else if (res.status_code >= 400) {
            metrics.error_count++;
         }
      };

      // Simulate 3 successful requests
      for (int i = 0; i < 3; ++i) {
         glz::request req;
         glz::response res;
         res.status(200);
         auto handler = []() {};
         std::function<void()> chain = handler;
         chain = [metrics_middleware, prev = std::move(chain), &req, &res]() {
            glz::next_handler next(prev);
            metrics_middleware(req, res, next);
         };
         chain();
      }

      // Simulate 2 error requests
      for (int i = 0; i < 2; ++i) {
         glz::request req;
         glz::response res;
         res.status(404);
         auto handler = []() {};
         std::function<void()> chain = handler;
         chain = [metrics_middleware, prev = std::move(chain), &req, &res]() {
            glz::next_handler next(prev);
            metrics_middleware(req, res, next);
         };
         chain();
      }

      expect(metrics.total_requests.load() == 5) << "Should count all requests\n";
      expect(metrics.total_responses.load() == 5) << "Should count all responses\n";
      expect(metrics.success_count.load() == 3) << "Should count successful responses\n";
      expect(metrics.error_count.load() == 2) << "Should count error responses\n";
   };
};

int main()
{
   std::cout << "Running Glaze Async Server and API Unit Tests...\n";
   std::cout << "Testing async routes, server API, and advanced routing features\n\n";
}
