// Glaze Library
// For the license information refer to glaze.hpp

#ifndef BOOST_UT_DISABLE_MODULE
#define BOOST_UT_DISABLE_MODULE
#endif

#include "glaze/glaze.hpp"
#include "glaze/rpc/repe.hpp"

#include "boost/ut.hpp"

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
      repe::server server{};
      
      my_struct params{};
      std::string result{};
      
      server.on("concat", params, result, [](auto& params, auto& result){
         params.hello = "Aha";
         result = params.hello + " " + params.world;
      });
      
      {
         my_struct params{"Hello", "World"};
         
         auto request = repe::request_json({"concat", 5ul}, params);
         
         server.call(request);
      }
      
      expect(server.response == R"([[0,0,0,"concat",5],"Aha World"])") << server.response;
   };
   
   "concrete internal storage"_test = [] {
      repe::server server{};
      
      server.on("concat", [](my_struct& params, std::string& result){
         params.hello = "Aha";
         result = params.hello + " " + params.world;
      });
      
      {
         my_struct params{"Hello", "World"};
         
         auto request = repe::request_json({"concat", 5ul}, params);
         
         server.call(request);
      }
      
      expect(server.response == R"([[0,0,0,"concat",5],"Aha World"])") << server.response;
   };
   
   "notification"_test = [] {
      repe::server server{};
      
      my_struct params{};
      std::string result{};
      
      server.on("concat", params, result, [](auto& params, auto& result){
         params.hello = "Aha";
         // computing result, but we'll ignore it and not write it to the output buffer
         result = params.hello + " " + params.world;
      });
      
      {
         my_struct params{"Hello", "World"};
         
         auto request = repe::request_json({.method = "concat", .id = 5ul, .notification = true}, params);
         
         server.call(request);
      }
      
      expect(server.response == "") << server.response;
   };
   
   "error support"_test = [] {
      repe::server server{};
      
      server.on("concat", [&](my_struct& params, std::string& result){
         params.hello = "Aha";
         result = params.hello + " " + params.world;
         server.error = {repe::error_e::internal, "my custom error"};
      });
      
      {
         my_struct params{"Hello", "World"};
         
         auto request = repe::request_json({"concat", 5ul}, params);
         
         server.call(request);
      }
      
      expect(server.response == R"([[0,1,0,"",null],[-32603,"my custom error"]])") << server.response;
   };
   
   "thing"_test = [] {
      repe::server server{};
      
      server.on("modify", [&](thing& params, my_struct& result){
         result.hello = params.a + ", " + std::to_string(params.b) + ", " + std::to_string(params.c) + ", " + params.d.world;
         result.world = "this is a neat place";
      });
      
      {
         thing params{"a", 5, 3.14f, { "H", "W" }};
         
         auto request = repe::request_json({"modify"}, params);
         
         server.call(request);
      }
      
      expect(server.response == R"([[0,0,0,"modify",null],{"hello":"a, 5, 3.140000, W","world":"this is a neat place"}])") << server.response;
   };
};

int main()
{
   const auto result = boost::ut::cfg<>.run({.report_errors = true});
   return result;
}
