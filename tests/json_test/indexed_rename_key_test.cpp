#include <iostream>

#include "glaze/glaze.hpp"

namespace mylib
{
   enum struct MyEnum { First, Second };
   enum struct MyFlag { Yes, No };
} // namespace mylib

namespace glz
{
   template <>
   struct meta<mylib::MyEnum>
   {
      using enum mylib::MyEnum;
      static constexpr auto value = enumerate(First, Second);
   };

   template <>
   struct meta<mylib::MyFlag>
   {
      using enum mylib::MyFlag;
      static constexpr auto value = enumerate(Yes, No);
   };
} // namespace glz

// Test struct using automatic reflection
namespace test1
{
   struct AppContext
   {
      int num{};
      mylib::MyEnum e{};
      mylib::MyFlag f{};
   };
} // namespace test1

// Use indexed rename_key to automatically use enum type names as keys
template <>
struct glz::meta<test1::AppContext>
{
   template <size_t Index>
   static constexpr auto rename_key()
   {
      // Get the member type at this index
      using MemberType = glz::member_type_t<test1::AppContext, Index>;

      // If it's an enum, use the type name; otherwise use the member name
      if constexpr (std::is_enum_v<MemberType>) {
         return glz::name_v<MemberType>;
      }
      else {
         return glz::member_nameof<Index, test1::AppContext>;
      }
   }
};

// Test with custom short names (strip namespace)
namespace test2
{
   struct AppContext
   {
      int num{};
      mylib::MyEnum e{};
      mylib::MyFlag f{};
   };
} // namespace test2

template <>
struct glz::meta<test2::AppContext>
{
   template <size_t Index>
   static constexpr auto rename_key()
   {
      using MemberType = glz::member_type_t<test2::AppContext, Index>;

      if constexpr (std::is_enum_v<MemberType>) {
         // Strip namespace from enum type name
         constexpr auto full_name = glz::name_v<MemberType>;
         constexpr size_t pos = full_name.rfind("::");
         return (pos == std::string_view::npos) ? full_name : full_name.substr(pos + 2);
      }
      else {
         return glz::member_nameof<Index, test2::AppContext>;
      }
   }
};

// Test that it works with non-enum types too
namespace test3
{
   struct Point
   {
      double x{};
      double y{};
   };
} // namespace test3

// Transform all keys to uppercase
template <>
struct glz::meta<test3::Point>
{
   template <size_t Index>
   static constexpr auto rename_key()
   {
      constexpr auto name = glz::member_nameof<Index, test3::Point>;
      if constexpr (name == "x")
         return "X";
      else if constexpr (name == "y")
         return "Y";
      else
         return name;
   }
};

int main()
{
   std::cout << "=== Test 1: Enum type names (full) ===\n";
   test1::AppContext obj1{42, mylib::MyEnum::Second, mylib::MyFlag::Yes};
   std::string json1 = glz::write_json(obj1).value_or("error");
   std::cout << json1 << '\n';

   bool test1_pass = (json1 == R"({"num":42,"mylib::MyEnum":"Second","mylib::MyFlag":"Yes"})");
   std::cout << (test1_pass ? "✓ PASS" : "✗ FAIL") << "\n\n";

   std::cout << "=== Test 2: Enum type names (short) ===\n";
   test2::AppContext obj2{42, mylib::MyEnum::Second, mylib::MyFlag::Yes};
   std::string json2 = glz::write_json(obj2).value_or("error");
   std::cout << json2 << '\n';

   bool test2_pass = (json2 == R"({"num":42,"MyEnum":"Second","MyFlag":"Yes"})");
   std::cout << (test2_pass ? "✓ PASS" : "✗ FAIL") << "\n\n";

   std::cout << "=== Test 3: Generic key transformation ===\n";
   test3::Point obj3{3.14, 2.71};
   std::string json3 = glz::write_json(obj3).value_or("error");
   std::cout << json3 << '\n';

   bool test3_pass = (json3 == R"({"X":3.14,"Y":2.71})");
   std::cout << (test3_pass ? "✓ PASS" : "✗ FAIL") << "\n\n";

   if (test1_pass && test2_pass && test3_pass) {
      std::cout << "All tests passed!\n";
      return 0;
   }
   else {
      std::cout << "Some tests failed!\n";
      return 1;
   }
}
