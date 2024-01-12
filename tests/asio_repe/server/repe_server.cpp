// Glaze Library
// For the license information refer to glaze.hpp

#ifndef BOOST_UT_DISABLE_MODULE
#define BOOST_UT_DISABLE_MODULE
#endif

#include "boost/ut.hpp"

using namespace boost::ut;

#include "glaze/glaze.hpp"
#include "glaze/rpc/repe.hpp"

#include "glaze/asio/glaze_asio.hpp"

int main()
{
   const auto result = boost::ut::cfg<>.run({.report_errors = true});
   return result;
}
