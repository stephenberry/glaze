// Glaze Library
// For the license information refer to glaze.hpp

#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

#include "glaze/rpc/repe/plugin.h"
#include "glaze/rpc/repe/plugin_helper.hpp"
#include "ut/ut.hpp"

using namespace ut;

namespace repe = glz::repe;

// =============================================================================
// C Interface Tests (plugin.h)
// =============================================================================

suite c_interface_tests = [] {
   "interface_version"_test = [] { expect(REPE_PLUGIN_INTERFACE_VERSION == 2); };

   "repe_buffer_layout"_test = [] {
      repe_buffer buf{};
      buf.data = "test";
      buf.size = 4;
      expect(buf.data != nullptr);
      expect(buf.size == 4);
   };

   "repe_plugin_data_layout"_test = [] {
      repe_plugin_data data{};
      data.name = "test_plugin";
      data.version = "1.0.0";
      data.root_path = "/test";
      expect(data.name != nullptr);
      expect(data.version != nullptr);
      expect(data.root_path != nullptr);
      expect(std::string_view(data.name) == "test_plugin");
      expect(std::string_view(data.version) == "1.0.0");
      expect(std::string_view(data.root_path) == "/test");
   };

   "repe_result_values"_test = [] {
      expect(REPE_OK == 0);
      expect(REPE_ERROR_INIT_FAILED == 1);
      expect(REPE_ERROR_INVALID_CONFIG == 2);
      expect(REPE_ERROR_ALREADY_INITIALIZED == 3);
   };
};

// =============================================================================
// plugin_error_response Tests
// =============================================================================

suite plugin_error_response_tests = [] {
   "creates_valid_repe_message"_test = [] {
      repe::plugin_response_buffer.clear();

      repe::plugin_error_response(glz::error_code::method_not_found, "test error", 42);

      expect(repe::plugin_response_buffer.size() > sizeof(repe::header));

      // Deserialize and verify
      repe::message msg{};
      auto ec = repe::from_buffer(repe::plugin_response_buffer, msg);
      expect(ec == glz::error_code::none);
      expect(msg.header.spec == repe::repe_magic);
      expect(msg.header.version == 1);
   };

   "sets_error_code"_test = [] {
      repe::plugin_response_buffer.clear();

      repe::plugin_error_response(glz::error_code::parse_error, "parse failed", 0);

      repe::message msg{};
      auto ec = repe::from_buffer(repe::plugin_response_buffer, msg);
      expect(ec == glz::error_code::none);
      expect(msg.header.ec == glz::error_code::parse_error);
   };

   "preserves_message_id"_test = [] {
      repe::plugin_response_buffer.clear();

      repe::plugin_error_response(glz::error_code::invalid_call, "error", 12345);

      repe::message msg{};
      auto ec = repe::from_buffer(repe::plugin_response_buffer, msg);
      expect(ec == glz::error_code::none);
      expect(msg.header.id == 12345);
   };

   "default_id_is_zero"_test = [] {
      repe::plugin_response_buffer.clear();

      repe::plugin_error_response(glz::error_code::invalid_call, "error");

      repe::message msg{};
      auto ec = repe::from_buffer(repe::plugin_response_buffer, msg);
      expect(ec == glz::error_code::none);
      expect(msg.header.id == 0);
   };

   "sets_error_message_in_body"_test = [] {
      repe::plugin_response_buffer.clear();

      repe::plugin_error_response(glz::error_code::method_not_found, "custom error message", 0);

      repe::message msg{};
      auto ec = repe::from_buffer(repe::plugin_response_buffer, msg);
      expect(ec == glz::error_code::none);
      expect(msg.body == "custom error message");
   };
};

// =============================================================================
// Test API for plugin_call tests
// =============================================================================

struct test_api
{
   int value = 100;
   std::string name = "test";

   int get_value() { return value; }
   void set_value(int v) { value = v; }
   std::string get_name() { return name; }
   void set_name(const std::string& n) { name = n; }

   // Function that throws
   int throw_error() { throw std::runtime_error("intentional error"); }
};

template <>
struct glz::meta<test_api>
{
   using T = test_api;
   static constexpr auto value =
      object(&T::value, &T::name, &T::get_value, &T::set_value, &T::get_name, &T::set_name, &T::throw_error);
};

// =============================================================================
// plugin_call Tests
// =============================================================================

suite plugin_call_tests = [] {
   "successful_call"_test = [] {
      test_api api{};
      api.value = 42;

      glz::registry<> registry{};
      registry.on<glz::root<"/api">>(api);

      // Create request for /api/get_value
      repe::message request{};
      repe::request_json({"/api/get_value"}, request);

      std::string request_buffer;
      repe::to_buffer(request, request_buffer);

      auto result = repe::plugin_call(registry, request_buffer.data(), request_buffer.size());

      expect(result.data != nullptr);
      expect(result.size > 0);

      // Verify response
      repe::message response{};
      auto ec = repe::from_buffer(result.data, result.size, response);
      expect(ec == glz::error_code::none);
      expect(response.header.ec == glz::error_code::none);
      expect(response.body == "42");
   };

   "call_with_parameter"_test = [] {
      test_api api{};
      api.value = 0;

      glz::registry<> registry{};
      registry.on<glz::root<"/api">>(api);

      // Create request for /api/set_value with parameter
      repe::message request{};
      repe::request_json({"/api/set_value"}, request, 999);

      std::string request_buffer;
      repe::to_buffer(request, request_buffer);

      auto result = repe::plugin_call(registry, request_buffer.data(), request_buffer.size());

      repe::message response{};
      auto ec = repe::from_buffer(result.data, result.size, response);
      expect(ec == glz::error_code::none);
      expect(response.header.ec == glz::error_code::none);

      // Verify value was set
      expect(api.value == 999);
   };

   "deserialization_error"_test = [] {
      test_api api{};
      glz::registry<> registry{};
      registry.on<glz::root<"/api">>(api);

      // Send invalid REPE data
      std::string invalid_data = "not a valid REPE message";

      auto result = repe::plugin_call(registry, invalid_data.data(), invalid_data.size());

      repe::message response{};
      auto ec = repe::from_buffer(result.data, result.size, response);
      expect(ec == glz::error_code::none);
      // Invalid data too small for header returns invalid_header
      expect(response.header.ec == glz::error_code::invalid_header);
   };

   "registry_exception_handling"_test = [] {
      test_api api{};
      glz::registry<> registry{};
      registry.on<glz::root<"/api">>(api);

      // Call function that throws
      repe::message request{};
      repe::request_json({"/api/throw_error"}, request);
      request.header.id = 777;

      std::string request_buffer;
      repe::to_buffer(request, request_buffer);

      auto result = repe::plugin_call(registry, request_buffer.data(), request_buffer.size());

      repe::message response{};
      auto ec = repe::from_buffer(result.data, result.size, response);
      expect(ec == glz::error_code::none);
      // Registry catches and handles the exception
      expect(response.header.id == 777) << "ID should be preserved";
   };

   "method_not_found"_test = [] {
      test_api api{};
      glz::registry<> registry{};
      registry.on<glz::root<"/api">>(api);

      // Call non-existent method
      repe::message request{};
      repe::request_json({"/api/nonexistent"}, request);
      request.header.id = 123;

      std::string request_buffer;
      repe::to_buffer(request, request_buffer);

      auto result = repe::plugin_call(registry, request_buffer.data(), request_buffer.size());

      repe::message response{};
      auto ec = repe::from_buffer(result.data, result.size, response);
      expect(ec == glz::error_code::none);
      expect(response.header.ec == glz::error_code::method_not_found);
      expect(response.header.id == 123) << "ID should be preserved";
   };

   "preserves_request_id"_test = [] {
      test_api api{};
      glz::registry<> registry{};
      registry.on<glz::root<"/api">>(api);

      repe::message request{};
      repe::request_json({"/api/get_value"}, request);
      request.header.id = 99999;

      std::string request_buffer;
      repe::to_buffer(request, request_buffer);

      auto result = repe::plugin_call(registry, request_buffer.data(), request_buffer.size());

      repe::message response{};
      auto ec = repe::from_buffer(result.data, result.size, response);
      expect(ec == glz::error_code::none);
      expect(response.header.id == 99999);
   };
};

// =============================================================================
// Thread Safety Tests
// =============================================================================

suite thread_safety_tests = [] {
   "thread_local_buffer_isolation"_test = [] {
      std::atomic<bool> thread1_done{false};
      std::atomic<bool> thread2_done{false};
      std::string thread1_error_msg;
      std::string thread2_error_msg;

      std::thread t1([&]() {
         repe::plugin_error_response(glz::error_code::method_not_found, "thread1_message", 1);
         thread1_error_msg = repe::plugin_response_buffer;
         thread1_done = true;

         // Wait for thread2 to finish
         while (!thread2_done) {
            std::this_thread::yield();
         }

         // Verify our buffer wasn't modified by thread2
         expect(repe::plugin_response_buffer == thread1_error_msg);
      });

      std::thread t2([&]() {
         // Wait for thread1 to set its buffer
         while (!thread1_done) {
            std::this_thread::yield();
         }

         repe::plugin_error_response(glz::error_code::parse_error, "thread2_message", 2);
         thread2_error_msg = repe::plugin_response_buffer;
         thread2_done = true;
      });

      t1.join();
      t2.join();

      // Verify buffers are different
      expect(thread1_error_msg != thread2_error_msg);
      expect(thread1_error_msg.find("thread1_message") != std::string::npos);
      expect(thread2_error_msg.find("thread2_message") != std::string::npos);
   };

   "concurrent_plugin_calls"_test = [] {
      test_api api{};
      glz::registry<> registry{};
      registry.on<glz::root<"/api">>(api);

      constexpr int num_threads = 4;
      constexpr int calls_per_thread = 100;

      std::atomic<int> success_count{0};
      std::vector<std::thread> threads;

      for (int t = 0; t < num_threads; ++t) {
         threads.emplace_back([&, t]() {
            for (int i = 0; i < calls_per_thread; ++i) {
               repe::message request{};
               repe::request_json({"/api/get_value"}, request);
               request.header.id = t * calls_per_thread + i;

               std::string request_buffer;
               repe::to_buffer(request, request_buffer);

               auto result = repe::plugin_call(registry, request_buffer.data(), request_buffer.size());

               repe::message response{};
               auto ec = repe::from_buffer(result.data, result.size, response);

               if (ec == glz::error_code::none && response.header.ec == glz::error_code::none) {
                  ++success_count;
               }
            }
         });
      }

      for (auto& t : threads) {
         t.join();
      }

      expect(success_count == num_threads * calls_per_thread);
   };
};

// =============================================================================
// Integration Tests - Full Plugin Workflow
// =============================================================================

namespace test_plugin
{
   struct calculator_api
   {
      double value = 0.0;
      double get_value() { return value; }
      void set_value(double v) { value = v; }
      double increment() { return ++value; }
   };
}

template <>
struct glz::meta<test_plugin::calculator_api>
{
   using T = test_plugin::calculator_api;
   static constexpr auto value = object(&T::value, &T::get_value, &T::set_value, &T::increment);
};

namespace test_plugin
{
   calculator_api api_instance;
   glz::registry<> internal_registry;
   std::once_flag init_flag;

   void ensure_initialized()
   {
      std::call_once(init_flag, []() { internal_registry.on<glz::root<"/calculator">>(api_instance); });
   }

   // File-scope static plugin metadata (initialized at load time)
   static const repe_plugin_data plugin_info_data = {
      "calculator", // name
      "1.0.0", // version
      "/calculator" // root_path
   };

   // Simulated plugin exports
   uint32_t interface_version() { return REPE_PLUGIN_INTERFACE_VERSION; }

   const repe_plugin_data* info() { return &plugin_info_data; }

   repe_result init(const char*, uint64_t)
   {
      try {
         ensure_initialized();
         return REPE_OK;
      }
      catch (...) {
         return REPE_ERROR_INIT_FAILED;
      }
   }

   void shutdown()
   {
      // Cleanup
   }

   repe_buffer call(const char* request, uint64_t request_size)
   {
      // Plugin is responsible for ensuring initialization before calling plugin_call
      ensure_initialized();
      return repe::plugin_call(internal_registry, request, request_size);
   }
}

suite integration_tests = [] {
   "full_plugin_workflow"_test = [] {
      // Verify plugin info
      expect(test_plugin::interface_version() == REPE_PLUGIN_INTERFACE_VERSION);

      const repe_plugin_data* info = test_plugin::info();
      expect(info != nullptr);
      expect(std::string_view(info->name) == "calculator");
      expect(std::string_view(info->version) == "1.0.0");
      expect(std::string_view(info->root_path) == "/calculator");

      // Initialize plugin
      expect(test_plugin::init(nullptr, 0) == REPE_OK);

      // Call get_value
      {
         repe::message request{};
         repe::request_json({"/calculator/get_value"}, request);
         request.header.id = 1;

         std::string request_buffer;
         repe::to_buffer(request, request_buffer);

         auto result = test_plugin::call(request_buffer.data(), request_buffer.size());

         repe::message response{};
         auto ec = repe::from_buffer(result.data, result.size, response);
         expect(ec == glz::error_code::none);
         expect(response.header.ec == glz::error_code::none);
         expect(response.header.id == 1);
         expect(response.body == "0") << response.body; // Initial value
      }

      // Call set_value
      {
         repe::message request{};
         repe::request_json({"/calculator/set_value"}, request, 42.5);
         request.header.id = 2;

         std::string request_buffer;
         repe::to_buffer(request, request_buffer);

         auto result = test_plugin::call(request_buffer.data(), request_buffer.size());

         repe::message response{};
         auto ec = repe::from_buffer(result.data, result.size, response);
         expect(ec == glz::error_code::none);
         expect(response.header.ec == glz::error_code::none);
         expect(response.header.id == 2);
      }

      // Verify value was set
      {
         repe::message request{};
         repe::request_json({"/calculator/get_value"}, request);
         request.header.id = 3;

         std::string request_buffer;
         repe::to_buffer(request, request_buffer);

         auto result = test_plugin::call(request_buffer.data(), request_buffer.size());

         repe::message response{};
         auto ec = repe::from_buffer(result.data, result.size, response);
         expect(ec == glz::error_code::none);
         expect(response.body == "42.5") << response.body;
      }

      // Call increment
      {
         repe::message request{};
         repe::request_json({"/calculator/increment"}, request);
         request.header.id = 4;

         std::string request_buffer;
         repe::to_buffer(request, request_buffer);

         auto result = test_plugin::call(request_buffer.data(), request_buffer.size());

         repe::message response{};
         auto ec = repe::from_buffer(result.data, result.size, response);
         expect(ec == glz::error_code::none);
         expect(response.body == "43.5") << response.body;
      }

      // Shutdown
      test_plugin::shutdown();
   };

   "plugin_reads_entire_object"_test = [] {
      test_plugin::api_instance.value = 99.9;

      repe::message request{};
      repe::request_json({"/calculator"}, request);

      std::string request_buffer;
      repe::to_buffer(request, request_buffer);

      auto result = test_plugin::call(request_buffer.data(), request_buffer.size());

      repe::message response{};
      auto ec = repe::from_buffer(result.data, result.size, response);
      expect(ec == glz::error_code::none);
      expect(response.header.ec == glz::error_code::none);
      expect(response.body.find("99.9") != std::string::npos) << response.body;
   };
};

int main() { return 0; }
