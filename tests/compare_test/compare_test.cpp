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
   double z{};
   std::string str{};
};

template <>
struct glz::meta<float_compare_t>
{
   using T = float_compare_t;
   static constexpr auto value = object("x", &T::x, "y", &T::y, "z", &T::z);
   
   static constexpr auto compare_epsilon = 0.1;
};

suite comparison = [] {
   "float comparison"_test = [] {
      float_compare_t obj0{ 3.14, 5.5, 0.0 };
      float_compare_t obj1{ 3.15, 5.55, 0.099 };
      
      expect(glz::approx_equal(obj0, obj1));
      
      obj1.z = 1.0;
      
      expect(!glz::approx_equal(obj0, obj1));
      
      obj1.z = 0.1;
      
      expect(!glz::approx_equal(obj0, obj1));
   };
};

int main()
{
   const auto result = boost::ut::cfg<>.run({.report_errors = true});
   return result;
}
