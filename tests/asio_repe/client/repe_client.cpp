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

void asio_client_test()
{
   try {
      glz::asio_client client{};

      std::vector<int> data{};
      for (int i = 1; i < 10; ++i) {
         data.emplace_back(i);
         int sum{};
         if (auto ec = client.call({"/sum"}, data, sum); ec) {
            std::cerr << ec.message << '\n';
         }
         std::cerr << sum << '\n';
      }
      
      auto sum = client.callable<int, const std::vector<int>&>({"/sum"});
      
      std::cerr << "callable result: " << sum(std::vector<int>{1, 2, 3}) << '\n';
   }
   catch (const std::exception& e) {
      std::cerr << e.what() << '\n';
   }
}

int main()
{
   asio_client_test();

   const auto result = boost::ut::cfg<>.run({.report_errors = true});
   return result;
}
