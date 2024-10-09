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

      glz::repe::shared_buffer response{};

      {
         auto request = repe::request_json({"/i"});
         response = server.call(request.value());
      }

      expect(response->value() == R"([[0,0,0,"/i",null],55])") << response->value();

      {
         auto request = repe::request_json({.method = "/i"}, 42);
         response = server.call(request.value());
      }

      expect(response->value() == R"([[0,0,2,"/i",null],null])") << response->value();

      {
         auto request = repe::request_json({"/hello"});
         response = server.call(request.value());
      }

      expect(response->value() == R"([[0,0,0,"/hello",null],"Hello"])");

      {
         auto request = repe::request_json({"/get_number"});
         response = server.call(request.value());
      }

      expect(response->value() == R"([[0,0,0,"/get_number",null],42])");
   };

   "nested_structs_of_functions"_test = [] {
      repe::registry server{};

      my_nested_functions_t obj{};

      server.on(obj);

      glz::repe::shared_buffer response{};

      {
         auto request = repe::request_json({"/my_functions/void_func"});
         response = server.call(request.value());
      }

      expect(response->value() == R"([[0,0,2,"/my_functions/void_func",null],null])") << response->value();

      {
         auto request = repe::request_json({"/my_functions/hello"});
         response = server.call(request.value());
      }

      expect(response->value() == R"([[0,0,0,"/my_functions/hello",null],"Hello"])");

      {
         auto request = repe::request_json({"/meta_functions/hello"});
         response = server.call(request.value());
      }

      expect(response->value() == R"([[0,0,0,"/meta_functions/hello",null],"Hello"])");

      {
         auto request = repe::request_json({"/append_awesome"}, "you are");
         response = server.call(request.value());
      }

      expect(response->value() == R"([[0,0,0,"/append_awesome",null],"you are awesome!"])");

      {
         auto request = repe::request_json({"/my_string"}, "Howdy!");
         response = server.call(request.value());
      }

      expect(response->value() == R"([[0,0,2,"/my_string",null],null])");

      {
         auto request = repe::request_json({"/my_string"});
         response = server.call(request.value());
      }

      expect(response->value() == R"([[0,0,0,"/my_string",null],"Howdy!"])") << response->value();

      obj.my_string.clear();

      {
         auto request = repe::request_json({"/my_string"});
         response = server.call(request.value());
      }

      // we expect an empty string returned because we cleared it
      expect(response->value() == R"([[0,0,0,"/my_string",null],""])");

      {
         auto request = repe::request_json({"/my_functions/max"}, std::vector<double>{1.1, 3.3, 2.25});
         response = server.call(request.value());
      }

      expect(response->value() == R"([[0,0,0,"/my_functions/max",null],3.3])") << response->value();

      {
         auto request = repe::request_json({"/my_functions"});
         response = server.call(request.value());
      }

      expect(
         response->value() ==
         R"([[0,0,0,"/my_functions",null],{"i":0,"hello":"std::function<std::string_view()>","world":"std::function<std::string_view()>","get_number":"std::function<int32_t()>","void_func":"std::function<void()>","max":"std::function<double(std::vector<double>&)>"}])")
         << response->value();

      {
         auto request = repe::request_json({""});
         response = server.call(request.value());
      }

      expect(
         response->value() ==
         R"([[0,0,0,"",null],{"my_functions":{"i":0,"hello":"std::function<std::string_view()>","world":"std::function<std::string_view()>","get_number":"std::function<int32_t()>","void_func":"std::function<void()>","max":"std::function<double(std::vector<double>&)>"},"meta_functions":{"hello":"std::function<std::string_view()>","world":"std::function<std::string_view()>","get_number":"std::function<int32_t()>"},"append_awesome":"std::function<std::string(const std::string&)>","my_string":""}])")
         << response->value();
   };

   "example_functions"_test = [] {
      repe::registry server{};

      example_functions_t obj{};

      server.on(obj);

      glz::repe::shared_buffer response{};

      {
         auto request = repe::request_json({"/name"}, "Susan");
         response = server.call(request.value());
      }

      expect(response->value() == R"([[0,0,2,"/name",null],null])") << response->value();

      {
         auto request = repe::request_json({"/get_name"});
         response = server.call(request.value());
      }

      expect(response->value() == R"([[0,0,0,"/get_name",null],"Susan"])") << response->value();

      {
         auto request = repe::request_json({"/get_name"}, "Bob");
         response = server.call(request.value());
      }

      expect(obj.name == "Susan"); // we expect the name to not have changed because this function take no inputs
      expect(response->value() == R"([[0,0,0,"/get_name",null],"Susan"])") << response->value();

      {
         auto request = repe::request_json({"/set_name"}, "Bob");
         response = server.call(request.value());
      }

      expect(obj.name == "Bob");
      expect(response->value() == R"([[0,0,2,"/set_name",null],null])") << response->value();

      {
         auto request = repe::request_json({"/custom_name"}, "Alice");
         response = server.call(request.value());
      }

      expect(obj.name == "Alice");
      expect(response->value() == R"([[0,0,2,"/custom_name",null],null])") << response->value();
   };
};

suite structs_of_functions_binary = [] {
   "structs_of_functions"_test = [] {
      repe::registry<glz::opts{.format = glz::BEVE}> server{};

      my_functions_t obj{};

      server.on(obj);

      obj.i = 55;

      glz::repe::shared_buffer response{};

      {
         auto request = repe::request_binary({"/i"});
         response = server.call(request.value());
      }

      std::string res{};
      expect(!glz::beve_to_json(response->value(), res));
      expect(res == R"([[0,0,0,"/i",null],55])") << res;

      {
         auto request = repe::request_binary({.method = "/i"}, 42);
         response = server.call(request.value());
      }

      expect(!glz::beve_to_json(response->value(), res));
      expect(res == R"([[0,0,2,"/i",null],null])") << res;

      {
         auto request = repe::request_binary({"/hello"});
         response = server.call(request.value());
      }

      expect(!glz::beve_to_json(response->value(), res));
      expect(res == R"([[0,0,0,"/hello",null],"Hello"])");

      {
         auto request = repe::request_binary({"/get_number"});
         response = server.call(request.value());
      }

      expect(!glz::beve_to_json(response->value(), res));
      expect(res == R"([[0,0,0,"/get_number",null],42])");
   };

   "nested_structs_of_functions"_test = [] {
      repe::registry<glz::opts{.format = glz::BEVE}> server{};

      my_nested_functions_t obj{};

      server.on(obj);

      glz::repe::shared_buffer response{};

      {
         auto request = repe::request_binary({"/my_functions/void_func"});
         response = server.call(request.value());
      }

      std::string res{};
      expect(!glz::beve_to_json(response->value(), res));
      expect(res == R"([[0,0,2,"/my_functions/void_func",null],null])") << response;

      {
         auto request = repe::request_binary({"/my_functions/hello"});
         response = server.call(request.value());
      }

      expect(!glz::beve_to_json(response->value(), res));
      expect(res == R"([[0,0,0,"/my_functions/hello",null],"Hello"])");

      {
         auto request = repe::request_binary({"/meta_functions/hello"});
         response = server.call(request.value());
      }

      expect(!glz::beve_to_json(response->value(), res));
      expect(res == R"([[0,0,0,"/meta_functions/hello",null],"Hello"])");

      {
         auto request = repe::request_binary({"/append_awesome"}, "you are");
         response = server.call(request.value());
      }

      expect(!glz::beve_to_json(response->value(), res));
      expect(res == R"([[0,0,0,"/append_awesome",null],"you are awesome!"])");

      {
         auto request = repe::request_binary({"/my_string"}, "Howdy!");
         response = server.call(request.value());
      }

      expect(!glz::beve_to_json(response->value(), res));
      expect(res == R"([[0,0,2,"/my_string",null],null])");

      {
         auto request = repe::request_binary({"/my_string"});
         response = server.call(request.value());
      }

      expect(!glz::beve_to_json(response->value(), res));
      expect(res == R"([[0,0,0,"/my_string",null],"Howdy!"])") << response;

      obj.my_string.clear();

      {
         auto request = repe::request_binary({"/my_string"});
         response = server.call(request.value());
      }

      expect(!glz::beve_to_json(response->value(), res));
      // we expect an empty string returned because we cleared it
      expect(res == R"([[0,0,0,"/my_string",null],""])");

      {
         auto request = repe::request_binary({"/my_functions/max"}, std::vector<double>{1.1, 3.3, 2.25});
         response = server.call(request.value());
      }

      expect(!glz::beve_to_json(response->value(), res));
      expect(res == R"([[0,0,0,"/my_functions/max",null],3.3])") << response;

      {
         auto request = repe::request_binary({"/my_functions"});
         response = server.call(request.value());
      }

      expect(!glz::beve_to_json(response->value(), res));
      expect(
         res ==
         R"([[0,0,0,"/my_functions",null],{"i":0,"hello":"std::function<std::string_view()>","world":"std::function<std::string_view()>","get_number":"std::function<int32_t()>","void_func":"std::function<void()>","max":"std::function<double(std::vector<double>&)>"}])")
         << res;

      {
         auto request = repe::request_binary({""});
         response = server.call(request.value());
      }

      expect(!glz::beve_to_json(response->value(), res));
      expect(
         res ==
         R"([[0,0,0,"",null],{"my_functions":{"i":0,"hello":"std::function<std::string_view()>","world":"std::function<std::string_view()>","get_number":"std::function<int32_t()>","void_func":"std::function<void()>","max":"std::function<double(std::vector<double>&)>"},"meta_functions":{"hello":"std::function<std::string_view()>","world":"std::function<std::string_view()>","get_number":"std::function<int32_t()>"},"append_awesome":"std::function<std::string(const std::string&)>","my_string":""}])")
         << res;
   };

   "example_functions"_test = [] {
      repe::registry<glz::opts{.format = glz::BEVE}> server{};

      example_functions_t obj{};

      server.on(obj);

      glz::repe::shared_buffer response{};

      {
         auto request = repe::request_binary({"/name"}, "Susan");
         response = server.call(request.value());
      }

      std::string res{};
      expect(!glz::beve_to_json(response->value(), res));
      expect(res == R"([[0,0,2,"/name",null],null])") << response;

      {
         auto request = repe::request_binary({"/get_name"});
         response = server.call(request.value());
      }

      expect(!glz::beve_to_json(response->value(), res));
      expect(res == R"([[0,0,0,"/get_name",null],"Susan"])") << response;

      {
         auto request = repe::request_binary({"/get_name"}, "Bob");
         response = server.call(request.value());
      }

      expect(!glz::beve_to_json(response->value(), res));
      expect(obj.name == "Susan"); // we expect the name to not have changed because this function take no inputs
      expect(res == R"([[0,0,0,"/get_name",null],"Susan"])") << res;

      {
         auto request = repe::request_binary({"/set_name"}, "Bob");
         response = server.call(request.value());
      }

      expect(!glz::beve_to_json(response->value(), res));
      expect(obj.name == "Bob");
      expect(res == R"([[0,0,2,"/set_name",null],null])") << response;

      {
         auto request = repe::request_binary({"/custom_name"}, "Alice");
         response = server.call(request.value());
      }

      expect(!glz::beve_to_json(response->value(), res));
      expect(obj.name == "Alice");
      expect(res == R"([[0,0,2,"/custom_name",null],null])") << response;
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

      glz::repe::shared_buffer response{};

      {
         auto request = repe::request_json({"/sub/my_functions/void_func"});
         response = server.call(request.value());
      }

      expect(response->value() == R"([[0,0,2,"/sub/my_functions/void_func",null],null])") << response->value();

      {
         auto request = repe::request_json({"/sub/my_functions/hello"});
         response = server.call(request.value());
      }

      expect(response->value() == R"([[0,0,0,"/sub/my_functions/hello",null],"Hello"])");
   };
};

suite root_tests = [] {
   "root /sub"_test = [] {
      repe::registry server{};

      my_nested_functions_t obj{};

      server.on<glz::root<"/sub">>(obj);

      glz::repe::shared_buffer response{};

      {
         auto request = repe::request_json({"/sub/my_functions/void_func"});
         response = server.call(request.value());
      }

      expect(response->value() == R"([[0,0,2,"/sub/my_functions/void_func",null],null])") << response->value();

      {
         auto request = repe::request_json({"/sub/my_functions/hello"});
         response = server.call(request.value());
      }

      expect(response->value() == R"([[0,0,0,"/sub/my_functions/hello",null],"Hello"])");
   };
};

suite wrapper_tests_binary = [] {
   "wrapper"_test = [] {
      repe::registry<glz::opts{.format = glz::BEVE}> server{};

      my_nested_functions_t instance{};
      wrapper_t<my_nested_functions_t> obj{&instance};

      server.on(obj);

      glz::repe::shared_buffer response{};

      {
         auto request = repe::request_binary({"/sub/my_functions/void_func"});
         response = server.call(request.value());
      }

      std::string res{};
      expect(!glz::beve_to_json(response->value(), res));
      expect(res == R"([[0,0,2,"/sub/my_functions/void_func",null],null])") << response;

      {
         auto request = repe::request_binary({"/sub/my_functions/hello"});
         response = server.call(request.value());
      }

      expect(!glz::beve_to_json(response->value(), res));
      expect(res == R"([[0,0,0,"/sub/my_functions/hello",null],"Hello"])");
   };
};

struct tester
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

      auto read_str = repe::request_json({"/str"}).value();

      std::thread reader_str([&] {
         size_t response_counter{};
         for (size_t i = 0; i < N; ++i) {
            const auto response = registry.call(read_str);
            response_counter += response->value().size();
         }
         std::cout << "read str response_counter: " << response_counter << '\n';
      });

      auto read_integer = repe::request_json({"/integer"}).value();

      std::thread reader_integer([&] {
         size_t response_counter{};
         for (size_t i = 0; i < N; ++i) {
            const auto response = registry.call(read_integer);
            response_counter += response->value().size();
         }
         std::cout << "read integer response_counter: " << response_counter << '\n';
      });

      auto read_full = repe::request_json({""}).value();

      std::thread reader_full([&] {
         size_t response_counter{};
         for (size_t i = 0; i < N; ++i) {
            const auto response = registry.call(read_full);
            response_counter += response->value().size();
         }
         std::cout << "read full response_counter: " << response_counter << '\n';
      });

      std::latch latch{1};

      std::thread writer_str([&] {
         size_t response_counter{};
         std::string message;
         for (size_t i = 0; i < N; ++i) {
            message.append("x");
            auto write_str = repe::request_json({"/str"}, message).value();
            const auto response = registry.call(write_str);
            response_counter += response->value().size();

            if (i == 50) {
               latch.count_down();
            }
         }
         std::cout << "write str response_counter: " << response_counter << '\n';
      });

      std::thread writer_integer([&] {
         size_t response_counter{};
         for (size_t i = 0; i < N; ++i) {
            auto write_str = repe::request_json({"/integer"}, i).value();
            const auto response = registry.call(write_str);
            response_counter += response->value().size();
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
};

int main() { return 0; }
