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

inline void init_server(glz::repe::server<>& server)
{
   server.on("sum", [](std::vector<int>& vec, int& sum) { sum = std::reduce(vec.begin(), vec.end()); });
   server.on("max", [](std::vector<double>& vec, double& max) { max = std::ranges::max(vec); });
}

void run_server()
{
   std::cerr << "Server active...\n";

   try {
      glz::asio_server<glz::repe::server<>> server{};
      server.init = init_server;
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
