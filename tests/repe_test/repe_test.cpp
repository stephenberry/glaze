// Glaze Library
// For the license information refer to glaze.hpp

#ifndef BOOST_UT_DISABLE_MODULE
#define BOOST_UT_DISABLE_MODULE
#endif

#include "glaze/rpc/repe.hpp"

#include "boost/ut.hpp"
#include "glaze/ext/cli_menu.hpp"
#include "glaze/glaze.hpp"

using namespace boost::ut;

namespace repe = glz::repe;

struct my_struct
{
   std::string hello{};
   std::string world{};
};

struct thing
{
   std::string a{};
   int b{};
   float c{};
   my_struct d{};
};

suite repe_tests = [] {
   "references"_test = [] {
      repe::registry server{};

      my_struct params{};
      std::string result{};

      server.on("concat", params, result, [](auto& params, auto& result) {
         params.hello = "Aha";
         result = params.hello + " " + params.world;
      });

      {
         my_struct client_params{"Hello", "World"};

         auto request = repe::request_json({"concat", 5ul}, client_params);

         server.call(request);
      }

      expect(server.response == R"([[0,0,0,"concat",5],"Aha World"])") << server.response;
   };

   "concrete internal storage"_test = [] {
      repe::registry server{};

      server.on("concat", [](my_struct& params, std::string& result) {
         params.hello = "Aha";
         result = params.hello + " " + params.world;
      });

      {
         my_struct client_params{"Hello", "World"};

         auto request = repe::request_json({"concat", 5ul}, client_params);

         server.call(request);
      }

      expect(server.response == R"([[0,0,0,"concat",5],"Aha World"])") << server.response;
   };

   "notification"_test = [] {
      repe::registry server{};

      my_struct params{};
      std::string result{};

      server.on("concat", params, result, [](auto& params, auto& result) {
         params.hello = "Aha";
         // computing result, but we'll ignore it and not write it to the output buffer
         result = params.hello + " " + params.world;
      });

      {
         my_struct client_params{"Hello", "World"};

         auto request = repe::request_json({.method = "concat", .id = 5ul, .action = repe::notify}, client_params);

         server.call(request);
      }

      expect(server.response == "") << server.response;
   };

   "error support"_test = [] {
      repe::registry server{};

      server.on("concat", [&](my_struct& params, std::string& result) {
         params.hello = "Aha";
         result = params.hello + " " + params.world;
         server.error = {repe::error_e::internal, "my custom error"};
      });

      {
         my_struct client_params{"Hello", "World"};

         auto request = repe::request_json({"concat", 5ul}, client_params);

         server.call(request);
      }

      expect(server.response == R"([[0,1,0,"",null],[-32603,"my custom error"]])") << server.response;
   };

   "thing"_test = [] {
      repe::registry server{};

      server.on("modify", [&](thing& params, my_struct& result) {
         result.hello =
            params.a + ", " + std::to_string(params.b) + ", " + std::to_string(params.c) + ", " + params.d.world;
         result.world = "this is a neat place";
      });

      {
         thing params{"a", 5, 3.14f, {"H", "W"}};

         auto request = repe::request_json({"modify"}, params);

         server.call(request);
      }

      expect(server.response ==
             R"([[0,0,0,"modify",null],{"hello":"a, 5, 3.140000, W","world":"this is a neat place"}])")
         << server.response;
   };
};

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
      static constexpr auto value = glz::object(&T::name, &T::get_name, &T::set_name);
   };
};

suite structs_of_functions = [] {
   "structs_of_functions"_test = [] {
      repe::registry server{};

      my_functions_t obj{};

      server.on(obj);

      obj.i = 55;

      {
         auto request = repe::request_json({"/i"});
         server.call(request);
      }

      expect(server.response == R"([[0,0,0,"/i",null],55])") << server.response;

      {
         auto request = repe::request_json({.method = "/i"}, 42);
         server.call(request);
      }

      expect(server.response == R"([[0,0,2,"/i",null],null])") << server.response;

      {
         auto request = repe::request_json({"/hello"});
         server.call(request);
      }

      expect(server.response == R"([[0,0,0,"/hello",null],"Hello"])");

      {
         auto request = repe::request_json({"/get_number"});
         server.call(request);
      }

      expect(server.response == R"([[0,0,0,"/get_number",null],42])");
   };

   "nested_structs_of_functions"_test = [] {
      repe::registry server{};

      my_nested_functions_t obj{};

      server.on(obj);

      {
         auto request = repe::request_json({"/my_functions/void_func"});
         server.call(request);
      }

      expect(server.response == R"([[0,0,2,"/my_functions/void_func",null],null])") << server.response;

      {
         auto request = repe::request_json({"/my_functions/hello"});
         server.call(request);
      }

      expect(server.response == R"([[0,0,0,"/my_functions/hello",null],"Hello"])");

      {
         auto request = repe::request_json({"/meta_functions/hello"});
         server.call(request);
      }

      expect(server.response == R"([[0,0,0,"/meta_functions/hello",null],"Hello"])");

      {
         auto request = repe::request_json({"/append_awesome"}, "you are");
         server.call(request);
      }

      expect(server.response == R"([[0,0,0,"/append_awesome",null],"you are awesome!"])");

      {
         auto request = repe::request_json({"/my_string"}, "Howdy!");
         server.call(request);
      }

      expect(server.response == R"([[0,0,2,"/my_string",null],null])");

      {
         auto request = repe::request_json({"/my_string"});
         server.call(request);
      }

      expect(server.response == R"([[0,0,0,"/my_string",null],"Howdy!"])") << server.response;

      obj.my_string.clear();

      {
         auto request = repe::request_json({"/my_string"});
         server.call(request);
      }

      // we expect an empty string returned because we cleared it
      expect(server.response == R"([[0,0,0,"/my_string",null],""])");

      {
         auto request = repe::request_json({"/my_functions/max"}, std::vector<double>{1.1, 3.3, 2.25});
         server.call(request);
      }

      expect(server.response == R"([[0,0,0,"/my_functions/max",null],3.3])") << server.response;

      {
         auto request = repe::request_json({"/my_functions"});
         server.call(request);
      }

      expect(
         server.response ==
         R"([[0,0,0,"/my_functions",null],{"i":0,"hello":"std::function<std::string_view()>","world":"std::function<std::string_view()>","get_number":"std::function<int32_t()>","void_func":"std::function<void()>","max":"std::function<double(std::vector<double>&)>"}])")
         << server.response;

      {
         auto request = repe::request_json({""});
         server.call(request);
      }

      expect(
         server.response ==
         R"([[0,0,0,"",null],{"my_functions":{"i":0,"hello":"std::function<std::string_view()>","world":"std::function<std::string_view()>","get_number":"std::function<int32_t()>","void_func":"std::function<void()>","max":"std::function<double(std::vector<double>&)>"},"meta_functions":{"hello":"std::function<std::string_view()>","world":"std::function<std::string_view()>","get_number":"std::function<int32_t()>"},"append_awesome":"std::function<std::string(const std::string&)>","my_string":""}])")
         << server.response;
   };

   "example_functions"_test = [] {
      repe::registry server{};

      example_functions_t obj{};

      server.on(obj);

      {
         auto request = repe::request_json({"/name"}, "Susan");
         server.call(request);
      }

      expect(server.response == R"([[0,0,2,"/name",null],null])") << server.response;

      {
         auto request = repe::request_json({"/get_name"});
         server.call(request);
      }

      expect(server.response == R"([[0,0,0,"/get_name",null],"Susan"])") << server.response;

      {
         auto request = repe::request_json({"/get_name"}, "Bob");
         server.call(request);
      }

      expect(obj.name == "Susan"); // we expect the name to not have changed because this function take no inputs
      expect(server.response == R"([[0,0,2,"/get_name",null],null])") << server.response;

      {
         auto request = repe::request_json({"/set_name"}, "Bob");
         server.call(request);
      }

      expect(obj.name == "Bob");
      expect(server.response == R"([[0,0,2,"/set_name",null],null])") << server.response;
   };
};

template <class T>
struct wrapper_t
{
   T* sub{};

   // TODO: support meta wrappers and not only reflectable wrappers (I don't know why this isn't working)
   /*struct glaze {
      static constexpr auto value = glz::object(&wrapper_t::sub);
   };*/
};

suite wrapper_tests = [] {
   "wrapper"_test = [] {
      repe::registry server{};

      my_nested_functions_t instance{};
      wrapper_t<my_nested_functions_t> obj{&instance};

      server.on(obj);

      {
         auto request = repe::request_json({"/sub/my_functions/void_func"});
         server.call(request);
      }

      expect(server.response == R"([[0,0,2,"/sub/my_functions/void_func",null],null])") << server.response;

      {
         auto request = repe::request_json({"/sub/my_functions/hello"});
         server.call(request);
      }

      expect(server.response == R"([[0,0,0,"/sub/my_functions/hello",null],"Hello"])");
   };
};

int main()
{
   const auto result = boost::ut::cfg<>.run({.report_errors = true});
   return result;
}
