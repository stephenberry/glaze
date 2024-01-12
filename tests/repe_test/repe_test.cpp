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
      
      expect(server.response ==
R"([0,0,0,0,"concat",5]
"Aha World")");
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
      
      expect(server.response ==
R"([0,0,0,0,"concat",5]
"Aha World")");
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
      
      expect(server.response ==
R"([0,1,0,0,"",null]
[-32603,"my custom error"])") << server.response;
   };
};

int main()
{
   const auto result = boost::ut::cfg<>.run({.report_errors = true});
   return result;
}
