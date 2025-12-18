// Glaze Library
// For the license information refer to glaze.hpp

#include <latch>
#include <span>
#include <thread>

#include "glaze/glaze.hpp"
#include "glaze/rpc/registry.hpp"
#include "glaze/rpc/repe/buffer.hpp"
#include "glaze/thread/async_string.hpp"
#include "ut/ut.hpp"

using namespace ut;

namespace repe = glz::repe;

// ============================================================
// Test helpers for span-based API
// ============================================================
namespace test_helpers
{
   namespace detail
   {
      // thread_local buffers are safe in synchronous test code (no coroutines).
      // These avoid allocations by reusing buffer capacity across calls.
      inline std::string& request_buffer()
      {
         thread_local std::string buf;
         return buf;
      }

      inline std::string& response_buffer()
      {
         thread_local std::string buf;
         return buf;
      }

      inline repe::message& request_message()
      {
         thread_local repe::message msg;
         // Clear strings but preserve capacity for reuse
         msg.query.clear();
         msg.body.clear();
         return msg;
      }
   }

   // Convert message to wire format, call registry with span-based API, parse response
   template <class Registry>
   inline repe::message call(Registry& registry, repe::message& request)
   {
      auto& request_buffer = detail::request_buffer();
      auto& response_buffer = detail::response_buffer();

      repe::to_buffer(request, request_buffer);
      registry.call(std::span<const char>{request_buffer}, response_buffer);

      repe::message response{};
      repe::from_buffer(response_buffer, response);
      return response;
   }

   // JSON request without body
   template <class Registry>
   inline repe::message call_json(Registry& registry, const repe::user_header& hdr)
   {
      auto& request = detail::request_message();
      repe::request_json(hdr, request);
      return call(registry, request);
   }

   // JSON request with body
   template <class Registry, class Value>
   inline repe::message call_json(Registry& registry, const repe::user_header& hdr, Value&& value)
   {
      auto& request = detail::request_message();
      repe::request_json(hdr, request, std::forward<Value>(value));
      return call(registry, request);
   }

   // BEVE request without body
   template <class Registry>
   inline repe::message call_beve(Registry& registry, const repe::user_header& hdr)
   {
      auto& request = detail::request_message();
      repe::request_beve(hdr, request);
      return call(registry, request);
   }

   // BEVE request with body
   template <class Registry, class Value>
   inline repe::message call_beve(Registry& registry, const repe::user_header& hdr, Value&& value)
   {
      auto& request = detail::request_message();
      repe::request_beve(hdr, request, std::forward<Value>(value));
      return call(registry, request);
   }
}

struct my_functions_t
{
   int i{};
   std::function<std::string_view()> hello = []() -> std::string_view { return "Hello"; };
   std::function<std::string_view()> world = []() -> std::string_view { return "World"; };
   std::function<int()> get_number = [] { return 42; };
   std::function<void()> void_func = [] {};
   std::function<double(std::vector<double>& vec)> max = [](std::vector<double>& vec) {
      return (std::ranges::max)(vec);
   };
};

struct meta_functions_t
{
   std::function<std::string_view()> hello = []() -> std::string_view { return "Hello"; };
   std::function<std::string_view()> world = []() -> std::string_view { return "World"; };
   std::function<int()> get_number = [] { return 42; };

   struct glaze
   {
      using T = meta_functions_t;
      static constexpr auto value = glz::object(&T::hello, &T::world, &T::get_number);
   };
};

struct my_nested_functions_t
{
   my_functions_t my_functions{};
   meta_functions_t meta_functions{};
   std::function<std::string(const std::string&)> append_awesome = [](const std::string& in) {
      return in + " awesome!";
   };
   std::string my_string{};
};

struct example_functions_t
{
   std::string name{};
   std::string get_name() { return name; }
   void set_name(const std::string& new_name) { name = new_name; }

   struct glaze
   {
      using T = example_functions_t;
      static constexpr auto value =
         glz::object(&T::name, &T::get_name, &T::set_name, "custom_name", glz::custom<&T::set_name, &T::get_name>);
   };
};

struct Exhibit
{
   std::string name{};
   int year{};

   struct glaze
   {
      using T = Exhibit;
      static constexpr auto value = glz::object(&T::name, &T::year);
   };
};

struct Museum
{
   std::string name{};
   Exhibit main_exhibit{};

   Exhibit get_main_exhibit() { return main_exhibit; }
   void set_main_exhibit(const Exhibit& exhibit) { main_exhibit = exhibit; }

   struct glaze
   {
      using T = Museum;
      static constexpr auto value = glz::object(&T::name, &T::main_exhibit, &T::get_main_exhibit, &T::set_main_exhibit);
   };
};

struct VolatileData
{
   volatile int i = 10;
   volatile double d = 3.14;

   int get_i() volatile { return i; }
   void inc_i() volatile { i = i + 1; }

   struct glaze
   {
      using T = VolatileData;
      static constexpr auto value = glz::object(&T::i, &T::d, &T::get_i, &T::inc_i);
   };
};

suite structs_of_functions = [] {
   using namespace test_helpers;

   "structs_of_functions"_test = [] {
      glz::registry server{};

      my_functions_t obj{};

      server.on(obj);

      obj.i = 55;

      auto response = call_json(server, {"/i"});
      expect(response.body == R"(55)") << response.body;

      response = call_json(server, {.query = "/i"}, 42);
      expect(response.body == "null") << response.body;

      response = call_json(server, {"/hello"});
      expect(response.body == R"("Hello")");

      response = call_json(server, {"/get_number"});
      expect(response.body == R"(42)");

      response = call_json(server, {""});
      expect(
         response.body ==
         R"({"i":42,"hello":"std::function<std::string_view()>","world":"std::function<std::string_view()>","get_number":"std::function<int32_t()>","void_func":"std::function<void()>","max":"std::function<double(std::vector<double>&)>"})")
         << response.body;
   };

   "nested_structs_of_functions"_test = [] {
      glz::registry server{};

      my_nested_functions_t obj{};

      server.on(obj);

      auto response = call_json(server, {"/my_functions/void_func"});
      expect(response.body == "null") << response.body;

      response = call_json(server, {"/my_functions/hello"});
      expect(response.body == R"("Hello")");

      response = call_json(server, {"/meta_functions/hello"});
      expect(response.body == R"("Hello")");

      response = call_json(server, {"/append_awesome"}, "you are");
      expect(response.body == R"("you are awesome!")");

      response = call_json(server, {"/my_string"}, "Howdy!");
      expect(response.body == "null");

      response = call_json(server, {"/my_string"});
      expect(response.body == R"("Howdy!")") << response.body;

      obj.my_string.clear();

      response = call_json(server, {"/my_string"});
      // we expect an empty string returned because we cleared it
      expect(response.body == R"("")");

      response = call_json(server, {"/my_functions/max"}, std::vector<double>{1.1, 3.3, 2.25});
      expect(response.body == R"(3.3)") << response.body;

      response = call_json(server, {"/my_functions"});
      expect(
         response.body ==
         R"({"i":0,"hello":"std::function<std::string_view()>","world":"std::function<std::string_view()>","get_number":"std::function<int32_t()>","void_func":"std::function<void()>","max":"std::function<double(std::vector<double>&)>"})")
         << response.body;

      response = call_json(server, {""});
      expect(
         response.body ==
         R"({"my_functions":{"i":0,"hello":"std::function<std::string_view()>","world":"std::function<std::string_view()>","get_number":"std::function<int32_t()>","void_func":"std::function<void()>","max":"std::function<double(std::vector<double>&)>"},"meta_functions":{"hello":"std::function<std::string_view()>","world":"std::function<std::string_view()>","get_number":"std::function<int32_t()>"},"append_awesome":"std::function<std::string(const std::string&)>","my_string":""})")
         << response.body;
   };

   "example_functions"_test = [] {
      glz::registry server{};

      example_functions_t obj{};

      server.on(obj);

      auto response = call_json(server, {"/name"}, "Susan");
      expect(response.body == "null") << response.body;

      response = call_json(server, {"/get_name"});
      expect(response.body == R"("Susan")") << response.body;

      response = call_json(server, {"/get_name"}, "Bob");
      expect(obj.name == "Susan"); // we expect the name to not have changed because this function take no inputs
      expect(response.body == R"("Susan")") << response.body;

      response = call_json(server, {"/set_name"}, "Bob");
      expect(obj.name == "Bob");
      expect(response.body == "null") << response.body;

      response = call_json(server, {"/custom_name"}, "Alice");
      expect(obj.name == "Alice");
      expect(response.body == "null") << response.body;

      response = call_json(server, {""});
      expect(response.body == R"({"name":"Alice","custom_name":"Alice"})") << response.body;
   };

   "volatile_member_functions_test"_test = [] {
      struct opts_with_write_member_functions : glz::opts
      {
         bool write_member_functions = true;
      };

      glz::registry<opts_with_write_member_functions{}> server{};
      VolatileData obj{};

      server.on(obj);

      // Test reading volatile data member
      auto response = call_json(server, {"/i"});
      expect(response.body == "10");

      // Test calling volatile member function
      response = call_json(server, {"/get_i"});
      expect(response.body == "10");

      // Test calling volatile void member function (modifier)
      response = call_json(server, {"/inc_i"});
      expect(response.body == "null");

      // Verify change
      response = call_json(server, {"/i"});
      expect(response.body == "11");

      // Test empty query with write_member_functions=true
      response = call_json(server, {""});
      expect(
         response.body ==
         R"=({"i":11,"d":3.14,"get_i":"int (VolatileData::*)() volatile","inc_i":"void (VolatileData::*)() volatile"})=")
         << response.body;
   };
};

suite structs_of_functions_beve = [] {
   using namespace test_helpers;

   "structs_of_functions"_test = [] {
      glz::registry<glz::opts{.format = glz::BEVE}> server{};

      my_functions_t obj{};

      server.on(obj);

      obj.i = 55;

      std::string res{};

      auto response = call_beve(server, {"/i"});
      expect(!glz::beve_to_json(response.body, res));
      expect(res == R"(55)") << res;

      response = call_beve(server, {.query = "/i"}, 42);
      expect(!glz::beve_to_json(response.body, res));
      expect(res == "null") << res;

      response = call_beve(server, {"/hello"});
      expect(!glz::beve_to_json(response.body, res));
      expect(res == R"("Hello")");

      response = call_beve(server, {"/get_number"});
      expect(!glz::beve_to_json(response.body, res));
      expect(res == R"(42)");
   };

   "nested_structs_of_functions"_test = [] {
      glz::registry<glz::opts{.format = glz::BEVE}> server{};

      my_nested_functions_t obj{};

      server.on(obj);

      std::string res{};

      auto response = call_beve(server, {"/my_functions/void_func"});
      expect(!glz::beve_to_json(response.body, res));
      expect(res == "null") << res;

      response = call_beve(server, {"/my_functions/hello"});
      expect(!glz::beve_to_json(response.body, res));
      expect(res == R"("Hello")");

      response = call_beve(server, {"/meta_functions/hello"});
      expect(!glz::beve_to_json(response.body, res));
      expect(res == R"("Hello")");

      response = call_beve(server, {"/append_awesome"}, "you are");
      expect(!glz::beve_to_json(response.body, res));
      expect(res == R"("you are awesome!")");

      response = call_beve(server, {"/my_string"}, "Howdy!");
      expect(!glz::beve_to_json(response.body, res));
      expect(res == "null");

      response = call_beve(server, {"/my_string"});
      expect(!glz::beve_to_json(response.body, res));
      expect(res == R"("Howdy!")") << res;

      obj.my_string.clear();

      response = call_beve(server, {"/my_string"});
      expect(!glz::beve_to_json(response.body, res));
      // we expect an empty string returned because we cleared it
      expect(res == R"("")");

      response = call_beve(server, {"/my_functions/max"}, std::vector<double>{1.1, 3.3, 2.25});
      expect(!glz::beve_to_json(response.body, res));
      expect(res == R"(3.3)") << res;

      response = call_beve(server, {"/my_functions"});
      expect(!glz::beve_to_json(response.body, res));
      expect(
         res ==
         R"({"i":0,"hello":"std::function<std::string_view()>","world":"std::function<std::string_view()>","get_number":"std::function<int32_t()>","void_func":"std::function<void()>","max":"std::function<double(std::vector<double>&)>"})")
         << res;

      response = call_beve(server, {""});
      expect(!glz::beve_to_json(response.body, res));
      expect(
         res ==
         R"({"my_functions":{"i":0,"hello":"std::function<std::string_view()>","world":"std::function<std::string_view()>","get_number":"std::function<int32_t()>","void_func":"std::function<void()>","max":"std::function<double(std::vector<double>&)>"},"meta_functions":{"hello":"std::function<std::string_view()>","world":"std::function<std::string_view()>","get_number":"std::function<int32_t()>"},"append_awesome":"std::function<std::string(const std::string&)>","my_string":""})")
         << res;
   };

   "example_functions"_test = [] {
      glz::registry<glz::opts{.format = glz::BEVE}> server{};

      example_functions_t obj{};

      server.on(obj);

      std::string res{};

      auto response = call_beve(server, {"/name"}, "Susan");
      expect(!glz::beve_to_json(response.body, res));
      expect(res == "null") << res;

      response = call_beve(server, {"/get_name"});
      expect(!glz::beve_to_json(response.body, res));
      expect(res == R"("Susan")") << res;

      response = call_beve(server, {"/get_name"}, "Bob");
      expect(!glz::beve_to_json(response.body, res));
      expect(obj.name == "Susan"); // we expect the name to not have changed because this function take no inputs
      expect(res == R"("Susan")") << res;

      response = call_beve(server, {"/set_name"}, "Bob");
      expect(!glz::beve_to_json(response.body, res));
      expect(obj.name == "Bob");
      expect(res == "null") << res;

      response = call_beve(server, {"/custom_name"}, "Alice");
      expect(!glz::beve_to_json(response.body, res));
      expect(obj.name == "Alice");
      expect(res == "null") << res;
   };
};

template <class T>
struct wrapper_t
{
   T* sub{};

   // TODO: support meta wrappers and not only reflectable wrappers (I don't know why this isn't working)
   // struct glaze {
   //   static constexpr auto value = glz::object(&wrapper_t::sub);
   //};
};

suite wrapper_tests = [] {
   using namespace test_helpers;

   "wrapper"_test = [] {
      glz::registry server{};

      my_nested_functions_t instance{};
      wrapper_t<my_nested_functions_t> obj{&instance};

      server.on(obj);

      auto response = call_json(server, {"/sub/my_functions/void_func"});
      expect(response.body == "null") << response.body;

      response = call_json(server, {"/sub/my_functions/hello"});
      expect(response.body == R"("Hello")");
   };
};

suite root_tests = [] {
   using namespace test_helpers;

   "root /sub"_test = [] {
      glz::registry server{};

      my_nested_functions_t obj{};

      server.on<glz::root<"/sub">>(obj);

      auto response = call_json(server, {"/sub/my_functions/void_func"});
      expect(response.body == "null") << response.body;

      response = call_json(server, {"/sub/my_functions/hello"});
      expect(response.body == R"("Hello")");
   };
};

suite wrapper_tests_beve = [] {
   using namespace test_helpers;

   "wrapper"_test = [] {
      glz::registry<glz::opts{.format = glz::BEVE}> server{};

      my_nested_functions_t instance{};
      wrapper_t<my_nested_functions_t> obj{&instance};

      server.on(obj);

      std::string res{};

      auto response = call_beve(server, {"/sub/my_functions/void_func"});
      expect(!glz::beve_to_json(response.body, res));
      expect(res == "null") << res;

      response = call_beve(server, {"/sub/my_functions/hello"});
      expect(!glz::beve_to_json(response.body, res));
      expect(res == R"("Hello")");
   };
};

struct tester
{
   std::atomic<int> integer{};
   std::atomic<double> number{};
   glz::async_string str{};
};

suite multi_threading_tests = [] {
   // TODO: Why is this randomly failing with Linux with Clang???
   /*"multi-threading"_test = [] {
      glz::registry registry{};
      tester obj{};

      registry.on(obj);

      static constexpr size_t N = 10'000;

      repe::message read_msg{};
      repe::request_json({"/str"}, read_msg);

      std::thread reader_str([&] {
         size_t response_counter{};
         for (size_t i = 0; i < N; ++i) {
            repe::message response{};
            registry.call(read_msg, response);
            response_counter += response.body.size();
         }
         std::cout << "read str response_counter: " << response_counter << '\n';
      });

      repe::message read_integer{};
      repe::request_json({"/integer"}, read_integer);

      std::thread reader_integer([&] {
         size_t response_counter{};
         for (size_t i = 0; i < N; ++i) {
            repe::message response{};
            registry.call(read_integer, response);
            response_counter += response.body.size();
         }
         std::cout << "read integer response_counter: " << response_counter << '\n';
      });

      repe::message read_full{};
      repe::request_json({""}, read_full);

      std::thread reader_full([&] {
         size_t response_counter{};
         for (size_t i = 0; i < N; ++i) {
            repe::message response{};
            registry.call(read_full, response);
            response_counter += response.body.size();
         }
         std::cout << "read full response_counter: " << response_counter << '\n';
      });

      std::latch latch{1};

      std::thread writer_str([&] {
         size_t response_counter{};
         std::string message;
         for (size_t i = 0; i < N; ++i) {
            message.append("x");
            repe::message write_msg{};
            repe::request_json({"/str"}, write_msg, message);
            repe::message response{};
            registry.call(write_msg, response);
            response_counter += response.body.size();

            if (i == 50) {
               latch.count_down();
            }
         }
         std::cout << "write str response_counter: " << response_counter << '\n';
      });

      std::thread writer_integer([&] {
         size_t response_counter{};
         for (size_t i = 0; i < N; ++i) {
            repe::message write_msg{};
            repe::request_json({"/integer"}, write_msg, i);
            repe::message response{};
            registry.call(write_msg, response);
            response_counter += response.body.size();
         }
         std::cout << "write integer response_counter: " << response_counter << '\n';
      });

      {
         latch.wait();
         bool valid = true;
         auto read_proxy = obj.str.read();
         for (char c : *read_proxy) {
            if (c != 'x') {
               valid = false;
               break;
            }
         }
         expect(valid);
      }

      reader_str.join();
      reader_integer.join();
      reader_full.join();
      writer_str.join();
      writer_integer.join();
   };*/
};

struct glaze_types
{
   glz::file_include include{};
};

suite glaze_types_test = [] {
   "glaze_types"_test = [] {
      glaze_types obj{};

      glz::registry<> registry{};
      registry.on(obj);
   };
};

suite validation_tests = [] {
   using namespace test_helpers;

   "version_validation"_test = [] {
      glz::registry server{};
      my_functions_t obj{};
      server.on(obj);

      // Create a request with invalid version
      auto request = repe::request_json({"/hello"});
      request.header.version = 2; // Invalid version

      auto response = call(server, request);

      expect(response.header.ec == glz::error_code::version_mismatch);
      expect(response.body.find("version mismatch") != std::string::npos);
   };

   "length_validation"_test = [] {
      glz::registry server{};
      my_functions_t obj{};
      server.on(obj);

      // Create a request with invalid length
      auto request = repe::request_json({"/hello"});
      request.header.length = 100; // Wrong length

      auto response = call(server, request);

      expect(response.header.ec == glz::error_code::invalid_header);
      expect(response.body.find("length mismatch") != std::string::npos);
   };

   "magic_number_validation"_test = [] {
      glz::registry server{};
      my_functions_t obj{};
      server.on(obj);

      // Create a request with invalid magic number
      auto request = repe::request_json({"/hello"});
      request.header.spec = 0x1234; // Wrong magic number

      auto response = call(server, request);

      expect(response.header.ec == glz::error_code::invalid_header);
      expect(response.body.find("magic number mismatch") != std::string::npos);
   };

   "valid_message_passes"_test = [] {
      glz::registry server{};
      my_functions_t obj{};
      server.on(obj);

      // Create a valid request
      auto response = call_json(server, {"/hello"});

      // Should succeed and not have validation errors
      expect(response.header.ec == glz::error_code::none);
      expect(response.body == R"("Hello")");
   };
};

// Define throwing_functions_t at namespace scope for proper linkage
struct throwing_functions_t
{
   std::function<int()> throw_func = []() -> int { throw std::runtime_error("Test exception"); };
};

suite id_preservation_tests = [] {
   using namespace test_helpers;

   "method_not_found_preserves_id"_test = [] {
      glz::registry server{};

      my_functions_t obj{};
      server.on(obj);

      // Create a request with a specific ID for a non-existent endpoint
      auto request = repe::request_json({"/non_existent_endpoint"});
      request.header.id = 12345; // Set a specific ID

      auto response = call(server, request);

      // Verify error is set and ID is preserved
      expect(response.header.ec == glz::error_code::method_not_found);
      expect(response.header.id == 12345) << "ID should be preserved in method_not_found error";
      expect(response.body.find("invalid_query") != std::string::npos);
   };

   "exception_error_preserves_id"_test = [] {
      glz::registry server{};

      throwing_functions_t obj{};
      server.on(obj);

      // Create a request with a specific ID
      auto request = repe::request_json({"/throw_func"});
      request.header.id = 67890; // Set a specific ID

      auto response = call(server, request);

      // Verify error is set and ID is preserved
      expect(response.header.ec == glz::error_code::parse_error);
      expect(response.header.id == 67890) << "ID should be preserved in exception error";
      expect(response.body.find("Test exception") != std::string::npos);
   };

   "header_validation_errors_preserve_id"_test = [] {
      glz::registry server{};

      my_functions_t obj{};
      server.on(obj);

      // Test version mismatch
      auto request = repe::request_json({"/hello"});
      request.header.id = 11111;
      request.header.version = 2; // Invalid version

      auto response = call(server, request);

      expect(response.header.ec == glz::error_code::version_mismatch);
      expect(response.header.id == 11111) << "ID should be preserved in version_mismatch error";

      // Test invalid length
      request = repe::request_json({"/hello"});
      request.header.id = 22222;
      request.header.length = 100; // Wrong length

      response = call(server, request);

      expect(response.header.ec == glz::error_code::invalid_header);
      expect(response.header.id == 22222) << "ID should be preserved in length mismatch error";

      // Test invalid magic number
      request = repe::request_json({"/hello"});
      request.header.id = 33333;
      request.header.spec = 0x1234; // Wrong magic number

      response = call(server, request);

      expect(response.header.ec == glz::error_code::invalid_header);
      expect(response.header.id == 33333) << "ID should be preserved in magic number error";
   };

   "successful_request_preserves_id"_test = [] {
      glz::registry server{};

      my_functions_t obj{};
      server.on(obj);

      // Create a valid request with a specific ID
      auto request = repe::request_json({"/get_number"});
      request.header.id = 99999;

      auto response = call(server, request);

      // Verify success and ID is preserved
      expect(response.header.ec == glz::error_code::none);
      expect(response.header.id == 99999) << "ID should be preserved in successful response";
      expect(response.body == R"(42)");
   };
};

struct inner_t
{
   int val = 10;
   int get_val() { return val; }

   struct glaze
   {
      using T = inner_t;
      static constexpr auto value = glz::object(&T::val, &T::get_val);
   };
};

struct middle_t
{
   inner_t inner{};
   std::string name = "mid";
   std::string_view get_name() { return name; }

   struct glaze
   {
      using T = middle_t;
      static constexpr auto value = glz::object(&T::inner, &T::name, &T::get_name);
   };
};

struct outer_t
{
   middle_t middle{};
   double score = 3.14;
   void set_score(double s) { score = s; }

   struct glaze
   {
      using T = outer_t;
      static constexpr auto value = glz::object(&T::middle, &T::score, &T::set_score);
   };
};

suite deeply_nested_tests = [] {
   using namespace test_helpers;

   "nested_mix_test"_test = [] {
      glz::registry server{};
      outer_t obj{};
      server.on(obj);

      // Modify some values to ensure we get current state
      obj.middle.inner.val = 99;
      obj.middle.name = "modified_mid";
      obj.score = 1.23;

      auto response = call_json(server, {""});

      expect(response.body == R"({"middle":{"inner":{"val":99},"name":"modified_mid"},"score":1.23})") << response.body;
   };

   "nested_mix_write_member_functions_test"_test = [] {
      struct opts_with_write_member_functions : glz::opts
      {
         bool write_member_functions = true;
      };

      glz::registry<opts_with_write_member_functions{}> server{};
      outer_t obj{};
      server.on(obj);

      // Modify some values
      obj.middle.inner.val = 99;
      obj.middle.name = "modified_mid";
      obj.score = 1.23;

      auto response = call_json(server, {""});

      expect(
         response.body ==
         R"e({"middle":{"inner":{"val":99,"get_val":"int32_t (inner_t::*)()"},"name":"modified_mid","get_name":"std::string_view (middle_t::*)()"},"score":1.23,"set_score":"void (outer_t::*)(double)"})e")
         << response.body;
   };

   "museum_member_functions_test"_test = [] {
      struct opts_with_write_member_functions : glz::opts
      {
         bool write_member_functions = true;
      };

      glz::registry<opts_with_write_member_functions{}> server{};
      Museum museum{};
      museum.name = "The Louvre";
      museum.main_exhibit = {"Mona Lisa", 1503};

      server.on(museum);

      // Test reading data member
      auto response = call_json(server, {"/name"});
      expect(response.body == R"("The Louvre")");

      // Test calling member function that returns a struct
      response = call_json(server, {"/get_main_exhibit"});
      expect(response.body == R"({"name":"Mona Lisa","year":1503})") << response.body;

      // Test writing via member function
      response = call_json(server, {.query = "/set_main_exhibit"}, Exhibit{"The Raft of the Medusa", 1819});
      expect(response.body == "null");

      // Verify change
      response = call_json(server, {"/main_exhibit"});
      expect(response.body == R"({"name":"The Raft of the Medusa","year":1819})");

      response = call_json(server, {""});
      expect(
         response.body ==
         R"=({"name":"The Louvre","main_exhibit":{"name":"The Raft of the Medusa","year":1819},"get_main_exhibit":"Exhibit (Museum::*)()","set_main_exhibit":"void (Museum::*)(const Exhibit&)"})=")
         << response.body;
   };
};

// Test types for merge functionality
struct first_object_t
{
   int value1 = 42;
   std::string name1 = "first";
   int get_value1() { return value1; }

   struct glaze
   {
      using T = first_object_t;
      static constexpr auto value = glz::object(&T::value1, &T::name1, &T::get_value1);
   };
};

struct second_object_t
{
   double value2 = 3.14;
   std::string name2 = "second";
   double get_value2() { return value2; }

   struct glaze
   {
      using T = second_object_t;
      static constexpr auto value = glz::object(&T::value2, &T::name2, &T::get_value2);
   };
};

suite merge_tests = [] {
   using namespace test_helpers;

   "merge_basic"_test = [] {
      glz::registry server{};

      first_object_t obj1{};
      second_object_t obj2{};

      auto merged = glz::merge{obj1, obj2};
      server.on(merged);

      // Test reading from first object
      auto response = call_json(server, {"/value1"});
      expect(response.body == "42") << response.body;

      response = call_json(server, {"/name1"});
      expect(response.body == R"("first")") << response.body;

      response = call_json(server, {"/get_value1"});
      expect(response.body == "42") << response.body;

      // Test reading from second object
      response = call_json(server, {"/value2"});
      expect(response.body == "3.14") << response.body;

      response = call_json(server, {"/name2"});
      expect(response.body == R"("second")") << response.body;

      response = call_json(server, {"/get_value2"});
      expect(response.body == "3.14") << response.body;

      // Test merged root endpoint returns combined view
      response = call_json(server, {""});
      expect(response.body == R"({"value1":42,"name1":"first","value2":3.14,"name2":"second"})") << response.body;
   };

   "merge_write_individual_fields"_test = [] {
      glz::registry server{};

      first_object_t obj1{};
      second_object_t obj2{};

      auto merged = glz::merge{obj1, obj2};
      server.on(merged);

      // Write to individual fields should work
      auto response = call_json(server, {.query = "/value1"}, 100);
      expect(response.body == "null") << response.body;

      response = call_json(server, {"/value1"});
      expect(response.body == "100") << response.body;

      response = call_json(server, {.query = "/name2"}, "modified");
      expect(response.body == "null") << response.body;

      response = call_json(server, {"/name2"});
      expect(response.body == R"("modified")") << response.body;
   };

   "merge_write_to_root_not_supported"_test = [] {
      glz::registry server{};

      first_object_t obj1{};
      second_object_t obj2{};

      auto merged = glz::merge{obj1, obj2};
      server.on(merged);

      // Writing to merged root should return error
      auto response = call_json(server, {.query = ""}, R"({"value1":999})");
      expect(response.header.ec == glz::error_code::invalid_body);
      expect(response.body.find("not supported") != std::string::npos) << response.body;
   };

   "merge_with_nested_objects"_test = [] {
      glz::registry server{};

      Museum museum{};
      museum.name = "Art Museum";
      museum.main_exhibit = {"Starry Night", 1889};

      first_object_t obj1{};
      obj1.value1 = 10;

      auto merged = glz::merge{museum, obj1};
      server.on(merged);

      // Access nested object from museum
      auto response = call_json(server, {"/main_exhibit"});
      expect(response.body == R"({"name":"Starry Night","year":1889})") << response.body;

      response = call_json(server, {"/main_exhibit/name"});
      expect(response.body == R"("Starry Night")") << response.body;

      // Access field from first_object_t
      response = call_json(server, {"/value1"});
      expect(response.body == "10") << response.body;

      // Root should return merged view
      response = call_json(server, {""});
      expect(response.body.find("\"name\":\"Art Museum\"") != std::string::npos) << response.body;
      expect(response.body.find("\"value1\":10") != std::string::npos) << response.body;
   };

   "merge_beve"_test = [] {
      glz::registry<glz::opts{.format = glz::BEVE}> server{};

      first_object_t obj1{};
      second_object_t obj2{};

      auto merged = glz::merge{obj1, obj2};
      server.on(merged);

      std::string res{};

      // Test reading from merged objects with BEVE
      auto response = call_beve(server, {"/value1"});
      expect(!glz::beve_to_json(response.body, res));
      expect(res == "42") << res;

      response = call_beve(server, {"/value2"});
      expect(!glz::beve_to_json(response.body, res));
      expect(res == "3.14") << res;

      // Test merged root with BEVE
      response = call_beve(server, {""});
      expect(!glz::beve_to_json(response.body, res));
      expect(res.find("\"value1\":42") != std::string::npos) << res;
      expect(res.find("\"value2\":3.14") != std::string::npos) << res;
   };
};

int main() { return 0; }
