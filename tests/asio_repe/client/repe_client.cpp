// Glaze Library
// For the license information refer to glaze.hpp

#ifndef BOOST_UT_DISABLE_MODULE
#define BOOST_UT_DISABLE_MODULE
#endif

#include "boost/ut.hpp"

using namespace boost::ut;

#include "glaze/asio/glaze_asio.hpp"
#include "glaze/glaze.hpp"
#include "glaze/rpc/repe.hpp"

void asio_client_test()
{
   try
   {
      glz::asio_client client{};
      
      std::vector<int> data{};
      for (int i = 1; i < 100; ++i) {
         data.emplace_back(i);
         int sum{};
         const auto ec = client.call({"sum"}, data, sum);
         if (ec) {
            std::cerr << ec.message << '\n';
         }
         std::cerr << sum << '\n';
      }
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
