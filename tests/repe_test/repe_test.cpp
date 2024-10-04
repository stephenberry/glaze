// Glaze Library
// For the license information refer to glaze.hpp

#define UT_RUN_TIME_ONLY

#include "glaze/rpc/repe.hpp"

#include <latch>
#include <thread>

#include "glaze/ext/cli_menu.hpp"
#include "glaze/glaze.hpp"
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
   std::function<double(std::vector<double>& vec)> max = [](std::vector<double>& vec) { return std::ranges::max(vec); };
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
      repe::registry server{};

      my_functions_t obj{};

      server.on(obj);

      obj.i = 55;

      repe::message request{};
      repe::message response{};

      request = repe::request_json({"/i"});
      server.call(request, response);
      expect(response.body == R"(55)") << response.body;

      request = repe::request_json({.query = "/i"}, 42);
      server.call(request, response);
      expect(response.body == R"(null)") << response.body;

      request = repe::request_json({"/hello"});
      server.call(request, response);
      expect(response.body == R"("Hello")");

      request = repe::request_json({"/get_number"});
      server.call(request, response);
      expect(response.body == R"(42)");
   };

   "nested_structs_of_functions"_test = [] {
      repe::registry server{};

      my_nested_functions_t obj{};

      server.on(obj);

      repe::message request{};
      repe::message response{};

      request = repe::request_json({"/my_functions/void_func"});
      server.call(request, response);
      expect(response.body == R"(null)") << response.body;

      request = repe::request_json({"/my_functions/hello"});
      server.call(request, response);
      expect(response.body == R"("Hello")");

      request = repe::request_json({"/meta_functions/hello"});
      server.call(request, response);
      expect(response.body == R"("Hello")");

      request = repe::request_json({"/append_awesome"}, "you are");
      server.call(request, response);
      expect(response.body == R"("you are awesome!")");

      request = repe::request_json({"/my_string"}, "Howdy!");
      server.call(request, response);
      expect(response.body == R"(null)");

      request = repe::request_json({"/my_string"});
      server.call(request, response);
      expect(response.body == R"("Howdy!")") << response.body;

      obj.my_string.clear();

      request = repe::request_json({"/my_string"});
      server.call(request, response);
      // we expect an empty string returned because we cleared it
      expect(response.body == R"("")");

      request = repe::request_json({"/my_functions/max"}, std::vector<double>{1.1, 3.3, 2.25});
      server.call(request, response);
      expect(response.body == R"(3.3)") << response.body;

      request = repe::request_json({"/my_functions"});
      server.call(request, response);
      expect(
         response.body ==
         R"({"i":0,"hello":"std::function<std::string_view()>","world":"std::function<std::string_view()>","get_number":"std::function<int32_t()>","void_func":"std::function<void()>","max":"std::function<double(std::vector<double>&)>"})")
         << response.body;

      request = repe::request_json({""});
      server.call(request, response);
      expect(
         response.body ==
         R"({"my_functions":{"i":0,"hello":"std::function<std::string_view()>","world":"std::function<std::string_view()>","get_number":"std::function<int32_t()>","void_func":"std::function<void()>","max":"std::function<double(std::vector<double>&)>"},"meta_functions":{"hello":"std::function<std::string_view()>","world":"std::function<std::string_view()>","get_number":"std::function<int32_t()>"},"append_awesome":"std::function<std::string(const std::string&)>","my_string":""})")
         << response.body;
   };

   "example_functions"_test = [] {
      repe::registry server{};

      example_functions_t obj{};

      server.on(obj);

      repe::message request{};
      repe::message response{};

      request = repe::request_json({"/name"}, "Susan");
      server.call(request, response);
      expect(response.body == R"(null)") << response.body;

      request = repe::request_json({"/get_name"});
      server.call(request, response);
      expect(response.body == R"("Susan")") << response.body;

      request = repe::request_json({"/get_name"}, "Bob");
      server.call(request, response);
      expect(obj.name == "Susan"); // we expect the name to not have changed because this function take no inputs
      expect(response.body == R"("Susan")") << response.body;

      request = repe::request_json({"/set_name"}, "Bob");
      server.call(request, response);
      expect(obj.name == "Bob");
      expect(response.body == R"(null)") << response.body;

      request = repe::request_json({"/custom_name"}, "Alice");
      server.call(request, response);
      expect(obj.name == "Alice");
      expect(response.body == R"(null)") << response.body;
   };
};

suite structs_of_functions_binary = [] {
   "structs_of_functions"_test = [] {
      repe::registry<glz::opts{.format = glz::BEVE}> server{};

      my_functions_t obj{};

      server.on(obj);

      obj.i = 55;

      repe::message request{};
      repe::message response{};

      request = repe::request_binary({"/i"});
      server.call(request, response);
      std::string res{};
      expect(!glz::beve_to_json(response.body, res));
      expect(res == R"(55)") << res;

      request = repe::request_binary({.query = "/i"}, 42);
      server.call(request, response);
      expect(!glz::beve_to_json(response.body, res));
      expect(res == R"(null)") << res;

      request = repe::request_binary({"/hello"});
      server.call(request, response);
      expect(!glz::beve_to_json(response.body, res));
      expect(res == R"("Hello")");

      request = repe::request_binary({"/get_number"});
      server.call(request, response);
      expect(!glz::beve_to_json(response.body, res));
      expect(res == R"(42)");
   };

   "nested_structs_of_functions"_test = [] {
      repe::registry<glz::opts{.format = glz::BEVE}> server{};

      my_nested_functions_t obj{};

      server.on(obj);

      repe::message request{};
      repe::message response{};

      request = repe::request_binary({"/my_functions/void_func"});
      server.call(request, response);

      std::string res{};
      expect(!glz::beve_to_json(response.body, res));
      expect(res == R"(null)") << res;

      request = repe::request_binary({"/my_functions/hello"});
      server.call(request, response);
      expect(!glz::beve_to_json(response.body, res));
      expect(res == R"("Hello")");

      request = repe::request_binary({"/meta_functions/hello"});
      server.call(request, response);
      expect(!glz::beve_to_json(response.body, res));
      expect(res == R"("Hello")");

      request = repe::request_binary({"/append_awesome"}, "you are");
      server.call(request, response);
      expect(!glz::beve_to_json(response.body, res));
      expect(res == R"("you are awesome!")");

      request = repe::request_binary({"/my_string"}, "Howdy!");
      server.call(request, response);
      expect(!glz::beve_to_json(response.body, res));
      expect(res == R"(null)");

      request = repe::request_binary({"/my_string"});
      server.call(request, response);
      expect(!glz::beve_to_json(response.body, res));
      expect(res == R"("Howdy!")") << res;

      obj.my_string.clear();

      request = repe::request_binary({"/my_string"});
      server.call(request, response);
      expect(!glz::beve_to_json(response.body, res));
      // we expect an empty string returned because we cleared it
      expect(res == R"("")");

      request = repe::request_binary({"/my_functions/max"}, std::vector<double>{1.1, 3.3, 2.25});
      server.call(request, response);
      expect(!glz::beve_to_json(response.body, res));
      expect(res == R"(3.3)") << res;

      request = repe::request_binary({"/my_functions"});
      server.call(request, response);
      expect(!glz::beve_to_json(response.body, res));
      expect(
         res ==
         R"({"i":0,"hello":"std::function<std::string_view()>","world":"std::function<std::string_view()>","get_number":"std::function<int32_t()>","void_func":"std::function<void()>","max":"std::function<double(std::vector<double>&)>"})")
         << res;

      request = repe::request_binary({""});
      server.call(request, response);
      expect(!glz::beve_to_json(response.body, res));
      expect(
             res ==
         R"({"my_functions":{"i":0,"hello":"std::function<std::string_view()>","world":"std::function<std::string_view()>","get_number":"std::function<int32_t()>","void_func":"std::function<void()>","max":"std::function<double(std::vector<double>&)>"},"meta_functions":{"hello":"std::function<std::string_view()>","world":"std::function<std::string_view()>","get_number":"std::function<int32_t()>"},"append_awesome":"std::function<std::string(const std::string&)>","my_string":""})")
         << res;
   };

   "example_functions"_test = [] {
      repe::registry<glz::opts{.format = glz::BEVE}> server{};

      example_functions_t obj{};

      server.on(obj);

      repe::message request{};
      repe::message response{};

      request = repe::request_binary({"/name"}, "Susan");
      server.call(request, response);

      std::string res{};
      expect(!glz::beve_to_json(response.body, res));
      expect(res == R"(null)") << res;

      request = repe::request_binary({"/get_name"});
      server.call(request, response);

      expect(!glz::beve_to_json(response.body, res));
      expect(res == R"("Susan")") << res;

      request = repe::request_binary({"/get_name"}, "Bob");
      server.call(request, response);

      expect(!glz::beve_to_json(response.body, res));
      expect(obj.name == "Susan"); // we expect the name to not have changed because this function take no inputs
      expect(res == R"("Susan")") << res;

      request = repe::request_binary({"/set_name"}, "Bob");
      server.call(request, response);

      expect(!glz::beve_to_json(response.body, res));
      expect(obj.name == "Bob");
      expect(res == R"(null)") << res;

      request = repe::request_binary({"/custom_name"}, "Alice");
      server.call(request, response);

      expect(!glz::beve_to_json(response.body, res));
      expect(obj.name == "Alice");
      expect(res == R"(null)") << res;
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
      repe::registry server{};

      my_nested_functions_t instance{};
      wrapper_t<my_nested_functions_t> obj{&instance};

      server.on(obj);

      repe::message request{};
      repe::message response{};

      request = repe::request_json({"/sub/my_functions/void_func"});
      server.call(request, response);
      expect(response.body == R"(null)") << response.body;

      request = repe::request_json({"/sub/my_functions/hello"});
      server.call(request, response);
      expect(response.body == R"("Hello")");
   };
};

suite root_tests = [] {
   "root /sub"_test = [] {
      repe::registry server{};

      my_nested_functions_t obj{};

      server.on<glz::root<"/sub">>(obj);

      repe::message request{};
      repe::message response{};

      request = repe::request_json({"/sub/my_functions/void_func"});
      server.call(request, response);
      expect(response.body == R"(null)") << response.body;

      request = repe::request_json({"/sub/my_functions/hello"});
      server.call(request, response);
      expect(response.body == R"("Hello")");
   };
};

suite wrapper_tests_binary = [] {
   "wrapper"_test = [] {
      repe::registry<glz::opts{.format = glz::BEVE}> server{};

      my_nested_functions_t instance{};
      wrapper_t<my_nested_functions_t> obj{&instance};

      server.on(obj);

      repe::message request{};
      repe::message response{};

      request = repe::request_binary({"/sub/my_functions/void_func"});
      server.call(request, response);

      std::string res{};
      expect(!glz::beve_to_json(response.body, res));
      expect(res == R"(null)") << res;

      request = repe::request_binary({"/sub/my_functions/hello"});
      server.call(request, response);

      expect(!glz::beve_to_json(response.body, res));
      expect(res == R"("Hello")");
   };
};

/*struct tester
{
   int integer{};
   double number{};
   std::string str{};
};

suite multi_threading_tests = [] {
   "multi-threading"_test = [] {
      repe::registry registry{};
      tester obj{};

      registry.on(obj);

      static constexpr size_t N = 10'000;

      auto read_msg = repe::request_json({"/str"});

      std::thread reader_str([&] {
         size_t response_counter{};
         for (size_t i = 0; i < N; ++i) {
            const auto response = registry.call(read_msg);
            response_counter += response.body.size();
         }
         std::cout << "read str response_counter: " << response_counter << '\n';
      });

      auto read_integer = repe::request_json({"/integer"});

      std::thread reader_integer([&] {
         size_t response_counter{};
         for (size_t i = 0; i < N; ++i) {
            const auto response = registry.call(read_integer);
            response_counter += response.body.size();
         }
         std::cout << "read integer response_counter: " << response_counter << '\n';
      });

      auto read_full = repe::request_json({""});

      std::thread reader_full([&] {
         size_t response_counter{};
         for (size_t i = 0; i < N; ++i) {
            const auto response = registry.call(read_full);
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
            auto write_msg = repe::request_json({"/str"}, message);
            const auto response = registry.call(write_msg);
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
            auto write_msg = repe::request_json({"/integer"}, i);
            const auto response = registry.call(write_msg);
            response_counter += response.body.size();
         }
         std::cout << "write integer response_counter: " << response_counter << '\n';
      });

      {
         latch.wait();
         auto lock = registry.read_only_lock<"/str">();
         expect(lock);
         bool valid = true;
         for (char c : obj.str) {
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
   };
};

struct glaze_types
{
   glz::file_include include{};
};

suite glaze_types_test = [] {
   "glaze_types"_test = [] {
      glaze_types obj{};

      glz::repe::registry<> registry{};
      registry.on(obj);
   };
};*/

int main() { return 0; }
