// Glaze Library
// For the license information refer to glaze.hpp

#ifndef BOOST_UT_DISABLE_MODULE
#define BOOST_UT_DISABLE_MODULE
#endif

#include "boost/ut.hpp"

using namespace boost::ut;

#include "glaze/ext/glaze_asio.hpp"
#include "glaze/glaze.hpp"
#include "glaze/rpc/repe.hpp"

struct api
{
   std::function<int(std::vector<int>& vec)> sum = [](std::vector<int>& vec) { return std::reduce(vec.begin(), vec.end()); };
   std::function<double(std::vector<double>& vec)> max = [](std::vector<double>& vec) { return std::ranges::max(vec); };
};

void run_server()
{
   std::cerr << "Server active...\n";

   try {
      glz::asio_server<glz::repe::server<>> server{};
      api methods{};
      server.init = [&](glz::repe::server<>& server) {
         server.on(methods);
      };
      server.run();
   }
   catch (std::exception& e) {
      std::cerr << "Exception: " << e.what();
   }

   std::cerr << "Server closed...\n";
}

int main()
{
   run_server();

   const auto result = boost::ut::cfg<>.run({.report_errors = true});
   return result;
}
