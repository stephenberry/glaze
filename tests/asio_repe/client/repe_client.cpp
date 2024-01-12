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
   glz::asio_client client{};
   
   std::vector<int> data{};
   for (int i = 0; i < 100; ++i) {
      data.emplace_back(i);
      std::cerr << client.call({"sum", uint64_t(i)}, data) << '\n';
   }
}

int main()
{
   asio_client_test();

   const auto result = boost::ut::cfg<>.run({.report_errors = true});
   return result;
}
