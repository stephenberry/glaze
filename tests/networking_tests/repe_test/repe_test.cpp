// Glaze Library
// For the license information refer to glaze.hpp

#include <latch>
#include <thread>

#include "glaze/glaze.hpp"
#include "glaze/rpc/registry.hpp"
#include "glaze/thread/async_string.hpp"
#include "ut/ut.hpp"

using namespace ut;

namespace repe = glz::repe;

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

suite structs_of_functions = [] {
   "structs_of_functions"_test = [] {
      glz::registry server{};

      my_functions_t obj{};

      server.on(obj);

      obj.i = 55;

      repe::message response{};

      server.call(repe::request_json({"/i"}), response);
      expect(response.body == R"(55)") << response.body;

      server.call(repe::request_json({.query = "/i"}, 42), response);
      expect(response.body == "null") << response.body;

      server.call(repe::request_json({"/hello"}), response);
      expect(response.body == R"("Hello")");

      server.call(repe::request_json({"/get_number"}), response);
      expect(response.body == R"(42)");
   };

   "nested_structs_of_functions"_test = [] {
      glz::registry server{};

      my_nested_functions_t obj{};

      server.on(obj);

      repe::message request{};
      repe::message response{};

      repe::request_json({"/my_functions/void_func"}, request);
      server.call(request, response);
      expect(response.body == "null") << response.body;

      repe::request_json({"/my_functions/hello"}, request);
      server.call(request, response);
      expect(response.body == R"("Hello")");

      repe::request_json({"/meta_functions/hello"}, request);
      server.call(request, response);
      expect(response.body == R"("Hello")");

      repe::request_json({"/append_awesome"}, request, "you are");
      server.call(request, response);
      expect(response.body == R"("you are awesome!")");

      repe::request_json({"/my_string"}, request, "Howdy!");
      server.call(request, response);
      expect(response.body == "null");

      request = repe::request_json({"/my_string"});
      server.call(request, response);
      expect(response.body == R"("Howdy!")") << response.body;

      obj.my_string.clear();

      repe::request_json({"/my_string"}, request);
      server.call(request, response);
      // we expect an empty string returned because we cleared it
      expect(response.body == R"("")");

      repe::request_json({"/my_functions/max"}, request, std::vector<double>{1.1, 3.3, 2.25});
      server.call(request, response);
      expect(response.body == R"(3.3)") << response.body;

      request = repe::request_json({"/my_functions"});
      server.call(request, response);
      expect(
         response.body ==
         R"({"i":0,"hello":"std::function<std::string_view()>","world":"std::function<std::string_view()>","get_number":"std::function<int32_t()>","void_func":"std::function<void()>","max":"std::function<double(std::vector<double>&)>"})")
         << response.body;

      repe::request_json({""}, request);
      server.call(request, response);
      expect(
         response.body ==
         R"({"my_functions":{"i":0,"hello":"std::function<std::string_view()>","world":"std::function<std::string_view()>","get_number":"std::function<int32_t()>","void_func":"std::function<void()>","max":"std::function<double(std::vector<double>&)>"},"meta_functions":{"hello":"std::function<std::string_view()>","world":"std::function<std::string_view()>","get_number":"std::function<int32_t()>"},"append_awesome":"std::function<std::string(const std::string&)>","my_string":""})")
         << response.body;
   };

   "example_functions"_test = [] {
      glz::registry server{};

      example_functions_t obj{};

      server.on(obj);

      repe::message request{};
      repe::message response{};

      repe::request_json({"/name"}, request, "Susan");
      server.call(request, response);
      expect(response.body == "null") << response.body;

      repe::request_json({"/get_name"}, request);
      server.call(request, response);
      expect(response.body == R"("Susan")") << response.body;

      repe::request_json({"/get_name"}, request, "Bob");
      server.call(request, response);
      expect(obj.name == "Susan"); // we expect the name to not have changed because this function take no inputs
      expect(response.body == R"("Susan")") << response.body;

      repe::request_json({"/set_name"}, request, "Bob");
      server.call(request, response);
      expect(obj.name == "Bob");
      expect(response.body == "null") << response.body;

      repe::request_json({"/custom_name"}, request, "Alice");
      server.call(request, response);
      expect(obj.name == "Alice");
      expect(response.body == "null") << response.body;
   };
};

suite structs_of_functions_beve = [] {
   "structs_of_functions"_test = [] {
      glz::registry<glz::opts{.format = glz::BEVE}> server{};

      my_functions_t obj{};

      server.on(obj);

      obj.i = 55;

      repe::message request{};
      repe::message response{};

      repe::request_beve({"/i"}, request);
      server.call(request, response);
      std::string res{};
      expect(!glz::beve_to_json(response.body, res));
      expect(res == R"(55)") << res;

      repe::request_beve({.query = "/i"}, request, 42);
      server.call(request, response);
      expect(!glz::beve_to_json(response.body, res));
      expect(res == "null") << res;

      repe::request_beve({"/hello"}, request);
      server.call(request, response);
      expect(!glz::beve_to_json(response.body, res));
      expect(res == R"("Hello")");

      repe::request_beve({"/get_number"}, request);
      server.call(request, response);
      expect(!glz::beve_to_json(response.body, res));
      expect(res == R"(42)");
   };

   "nested_structs_of_functions"_test = [] {
      glz::registry<glz::opts{.format = glz::BEVE}> server{};

      my_nested_functions_t obj{};

      server.on(obj);

      repe::message request{};
      repe::message response{};

      repe::request_beve({"/my_functions/void_func"}, request);
      server.call(request, response);

      std::string res{};
      expect(!glz::beve_to_json(response.body, res));
      expect(res == "null") << res;

      repe::request_beve({"/my_functions/hello"}, request);
      server.call(request, response);
      expect(!glz::beve_to_json(response.body, res));
      expect(res == R"("Hello")");

      repe::request_beve({"/meta_functions/hello"}, request);
      server.call(request, response);
      expect(!glz::beve_to_json(response.body, res));
      expect(res == R"("Hello")");

      repe::request_beve({"/append_awesome"}, request, "you are");
      server.call(request, response);
      expect(!glz::beve_to_json(response.body, res));
      expect(res == R"("you are awesome!")");

      repe::request_beve({"/my_string"}, request, "Howdy!");
      server.call(request, response);
      expect(!glz::beve_to_json(response.body, res));
      expect(res == "null");

      request = repe::request_beve({"/my_string"});
      server.call(request, response);
      expect(!glz::beve_to_json(response.body, res));
      expect(res == R"("Howdy!")") << res;

      obj.my_string.clear();

      request = repe::request_beve({"/my_string"});
      server.call(request, response);
      expect(!glz::beve_to_json(response.body, res));
      // we expect an empty string returned because we cleared it
      expect(res == R"("")");

      repe::request_beve({"/my_functions/max"}, request, std::vector<double>{1.1, 3.3, 2.25});
      server.call(request, response);
      expect(!glz::beve_to_json(response.body, res));
      expect(res == R"(3.3)") << res;

      request = repe::request_beve({"/my_functions"});
      server.call(request, response);
      expect(!glz::beve_to_json(response.body, res));
      expect(
         res ==
         R"({"i":0,"hello":"std::function<std::string_view()>","world":"std::function<std::string_view()>","get_number":"std::function<int32_t()>","void_func":"std::function<void()>","max":"std::function<double(std::vector<double>&)>"})")
         << res;

      repe::request_beve({""}, request);
      server.call(request, response);
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

      repe::message request{};
      repe::message response{};

      repe::request_beve({"/name"}, request, "Susan");
      server.call(request, response);

      std::string res{};
      expect(!glz::beve_to_json(response.body, res));
      expect(res == "null") << res;

      repe::request_beve({"/get_name"}, request);
      server.call(request, response);

      expect(!glz::beve_to_json(response.body, res));
      expect(res == R"("Susan")") << res;

      repe::request_beve({"/get_name"}, request, "Bob");
      server.call(request, response);

      expect(!glz::beve_to_json(response.body, res));
      expect(obj.name == "Susan"); // we expect the name to not have changed because this function take no inputs
      expect(res == R"("Susan")") << res;

      repe::request_beve({"/set_name"}, request, "Bob");
      server.call(request, response);

      expect(!glz::beve_to_json(response.body, res));
      expect(obj.name == "Bob");
      expect(res == "null") << res;

      repe::request_beve({"/custom_name"}, request, "Alice");
      server.call(request, response);

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
   "wrapper"_test = [] {
      glz::registry server{};

      my_nested_functions_t instance{};
      wrapper_t<my_nested_functions_t> obj{&instance};

      server.on(obj);

      repe::message request{};
      repe::message response{};

      repe::request_json({"/sub/my_functions/void_func"}, request);
      server.call(request, response);
      expect(response.body == "null") << response.body;

      repe::request_json({"/sub/my_functions/hello"}, request);
      server.call(request, response);
      expect(response.body == R"("Hello")");
   };
};

suite root_tests = [] {
   "root /sub"_test = [] {
      glz::registry server{};

      my_nested_functions_t obj{};

      server.on<glz::root<"/sub">>(obj);

      repe::message request{};
      repe::message response{};

      repe::request_json({"/sub/my_functions/void_func"}, request);
      server.call(request, response);
      expect(response.body == "null") << response.body;

      repe::request_json({"/sub/my_functions/hello"}, request);
      server.call(request, response);
      expect(response.body == R"("Hello")");
   };
};

suite wrapper_tests_beve = [] {
   "wrapper"_test = [] {
      glz::registry<glz::opts{.format = glz::BEVE}> server{};

      my_nested_functions_t instance{};
      wrapper_t<my_nested_functions_t> obj{&instance};

      server.on(obj);

      repe::message request{};
      repe::message response{};

      repe::request_beve({"/sub/my_functions/void_func"}, request);
      server.call(request, response);

      std::string res{};
      expect(!glz::beve_to_json(response.body, res));
      expect(res == "null") << res;

      repe::request_beve({"/sub/my_functions/hello"}, request);
      server.call(request, response);

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
   "version_validation"_test = [] {
      glz::registry server{};
      my_functions_t obj{};
      server.on(obj);

      repe::message request{};
      repe::message response{};

      // Create a request with invalid version
      repe::request_json({"/hello"}, request);
      request.header.version = 2; // Invalid version

      server.call(request, response);

      expect(response.header.ec == glz::error_code::version_mismatch);
      expect(response.body.find("version mismatch") != std::string::npos);
   };

   "length_validation"_test = [] {
      glz::registry server{};
      my_functions_t obj{};
      server.on(obj);

      repe::message request{};
      repe::message response{};

      // Create a request with invalid length
      repe::request_json({"/hello"}, request);
      request.header.length = 100; // Wrong length

      server.call(request, response);

      expect(response.header.ec == glz::error_code::invalid_header);
      expect(response.body.find("length mismatch") != std::string::npos);
   };

   "magic_number_validation"_test = [] {
      glz::registry server{};
      my_functions_t obj{};
      server.on(obj);

      repe::message request{};
      repe::message response{};

      // Create a request with invalid magic number
      repe::request_json({"/hello"}, request);
      request.header.spec = 0x1234; // Wrong magic number

      server.call(request, response);

      expect(response.header.ec == glz::error_code::invalid_header);
      expect(response.body.find("magic number mismatch") != std::string::npos);
   };

   "valid_message_passes"_test = [] {
      glz::registry server{};
      my_functions_t obj{};
      server.on(obj);

      repe::message request{};
      repe::message response{};

      // Create a valid request
      repe::request_json({"/hello"}, request);
      // All validation fields should be correct by default

      server.call(request, response);

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
   "method_not_found_preserves_id"_test = [] {
      glz::registry server{};

      my_functions_t obj{};
      server.on(obj);

      repe::message request{};
      repe::message response{};

      // Create a request with a specific ID for a non-existent endpoint
      repe::request_json({"/non_existent_endpoint"}, request);
      request.header.id = 12345; // Set a specific ID

      server.call(request, response);

      // Verify error is set and ID is preserved
      expect(response.header.ec == glz::error_code::method_not_found);
      expect(response.header.id == 12345) << "ID should be preserved in method_not_found error";
      expect(response.body.find("invalid_query") != std::string::npos);
   };

   "exception_error_preserves_id"_test = [] {
      glz::registry server{};

      throwing_functions_t obj{};
      server.on(obj);

      repe::message request{};
      repe::message response{};

      // Create a request with a specific ID
      repe::request_json({"/throw_func"}, request);
      request.header.id = 67890; // Set a specific ID

      server.call(request, response);

      // Verify error is set and ID is preserved
      expect(response.header.ec == glz::error_code::parse_error);
      expect(response.header.id == 67890) << "ID should be preserved in exception error";
      expect(response.body.find("Test exception") != std::string::npos);
   };

   "header_validation_errors_preserve_id"_test = [] {
      glz::registry server{};

      my_functions_t obj{};
      server.on(obj);

      repe::message request{};
      repe::message response{};

      // Test version mismatch
      repe::request_json({"/hello"}, request);
      request.header.id = 11111;
      request.header.version = 2; // Invalid version

      server.call(request, response);

      expect(response.header.ec == glz::error_code::version_mismatch);
      expect(response.header.id == 11111) << "ID should be preserved in version_mismatch error";

      // Test invalid length
      repe::request_json({"/hello"}, request);
      request.header.id = 22222;
      request.header.length = 100; // Wrong length

      server.call(request, response);

      expect(response.header.ec == glz::error_code::invalid_header);
      expect(response.header.id == 22222) << "ID should be preserved in length mismatch error";

      // Test invalid magic number
      repe::request_json({"/hello"}, request);
      request.header.id = 33333;
      request.header.spec = 0x1234; // Wrong magic number

      server.call(request, response);

      expect(response.header.ec == glz::error_code::invalid_header);
      expect(response.header.id == 33333) << "ID should be preserved in magic number error";
   };

   "successful_request_preserves_id"_test = [] {
      glz::registry server{};

      my_functions_t obj{};
      server.on(obj);

      repe::message request{};
      repe::message response{};

      // Create a valid request with a specific ID
      repe::request_json({"/get_number"}, request);
      request.header.id = 99999;

      server.call(request, response);

      // Verify success and ID is preserved
      expect(response.header.ec == glz::error_code::none);
      expect(response.header.id == 99999) << "ID should be preserved in successful response";
      expect(response.body == R"(42)");
   };
};

int main() { return 0; }
