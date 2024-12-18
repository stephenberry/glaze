// Glaze Library
// For the license information refer to glaze.hpp

#include <ut/ut.hpp>

#include "glaze/core/convert_struct.hpp"

using namespace ut;

struct test_type
{
   int32_t int1{};
   int64_t int2{};
};

suite reflect_test_type = [] {
   static_assert(glz::reflect<test_type>::size == 2);
   static_assert(glz::reflect<test_type>::keys[0] == "int1");

   "for_each_field"_test = [] {
      test_type var{42, 43};

      glz::for_each_field(var, [](auto& field) { field += 1; });

      expect(var.int1 == 43);
      expect(var.int2 == 44);
   };
};

struct test_type_meta
{
   int32_t int1{};
   int64_t int2{};
};

template <>
struct glz::meta<test_type_meta>
{
   using T = test_type_meta;
   static constexpr auto value = object(&T::int1, &T::int2);
};

suite meta_reflect_test_type = [] {
   static_assert(glz::reflect<test_type_meta>::size == 2);
   static_assert(glz::reflect<test_type_meta>::keys[0] == "int1");

   "for_each_field"_test = [] {
      test_type_meta var{42, 43};

      glz::for_each_field(var, [](auto& field) { field += 1; });

      expect(var.int1 == 43);
      expect(var.int2 == 44);
   };
};

struct a_type
{
   float fluff = 1.1f;
   int goo = 1;
   std::string stub = "a";
};

struct b_type
{
   float fluff = 2.2f;
   int goo = 2;
   std::string stub = "b";
};

struct c_type
{
   std::optional<float> fluff = 3.3f;
   std::optional<int> goo = 3;
   std::optional<std::string> stub = "c";
};

suite convert_tests = [] {
   "convert a to b"_test = [] {
      a_type in{};
      b_type out{};

      glz::convert_struct(in, out);

      expect(out.fluff == 1.1f);
      expect(out.goo == 1);
      expect(out.stub == "a");
   };

   "convert a to c"_test = [] {
      a_type in{};
      c_type out{};

      glz::convert_struct(in, out);

      expect(out.fluff.value() == 1.1f);
      expect(out.goo.value() == 1);
      expect(out.stub.value() == "a");
   };

   "convert c to a"_test = [] {
      c_type in{};
      a_type out{};

      glz::convert_struct(in, out);

      expect(out.fluff == 3.3f);
      expect(out.goo == 3);
      expect(out.stub == "c");
   };
};

int main() { return 0; }
