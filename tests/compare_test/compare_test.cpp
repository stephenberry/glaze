// Glaze Library
// For the license information refer to glaze.hpp

#ifndef BOOST_UT_DISABLE_MODULE
#define BOOST_UT_DISABLE_MODULE
#endif

#include "boost/ut.hpp"
#include "glaze/glaze.hpp"

using namespace boost::ut;

struct float_compare_t
{
   float x{};
   double y{};
   std::string str{};
};

template <>
struct glz::meta<float_compare_t>
{
   using T = float_compare_t;
   static constexpr auto value = object("x", &T::x, "y", &T::y);
   
   static constexpr auto compare_epsilon = 0.1;
};

suite comparison = [] {
   "float comparison"_test = [] {
      float_compare_t obj0{ 3.14, 5.5 };
      float_compare_t obj1{ 3.15, 5.55 };
      
      expect(glz::approx_equal(obj0, obj1));
   };
};

int main()
{
   const auto result = boost::ut::cfg<>.run({.report_errors = true});
   return result;
}
