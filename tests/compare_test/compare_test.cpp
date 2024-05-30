// Glaze Library
// For the license information refer to glaze.hpp

#define UT_RUN_TIME_ONLY

#include "glaze/compare/compare.hpp"

#include "glaze/compare/approx.hpp"
#include "ut/ut.hpp"

using namespace ut;

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
      float_compare_t obj0{3.14f, 5.5, 0.0};
      float_compare_t obj1{3.15f, 5.55, 0.099};

      expect(glz::approx_equal_to{}(obj0, obj1));

      obj1.z = 1.0;

      expect(!glz::approx_equal_to{}(obj0, obj1));

      obj1.z = 0.1;

      expect(!glz::approx_equal_to{}(obj0, obj1));
   };
};

suite equality = [] {
   "float equality"_test = [] {
      float_compare_t obj0{3.14f, 5.5, 0.0};
      float_compare_t obj1{3.15f, 5.55, 0.099};

      expect(!glz::equal_to{}(obj0, obj1));

      obj1 = obj0;

      expect(glz::equal_to{}(obj0, obj1));
   };
};

int main() { return 0; }
