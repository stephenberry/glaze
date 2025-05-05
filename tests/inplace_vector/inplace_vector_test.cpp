#include <beman/inplace_vector/inplace_vector.hpp>
#include <compare>

#include "glaze/glaze.hpp"
#include "ut/ut.hpp"

using namespace ut;
using namespace beman;

struct my_struct
{
   struct entry_t
   {
      int a;
      int b;
      int c;
      auto operator<=>(const entry_t&) const = default;
   };
   inplace_vector<entry_t, 3> vec;

   auto operator<=>(const my_struct&) const
   {
      return std::lexicographical_compare_three_way(vec.begin(), vec.end(), vec.begin(), vec.end());
   }
};

suite json_test = [] {
   "int_vec"_test = [] {
      const std::string_view json{R"([1,2,3,4,5])"};
      inplace_vector<int, 10> vec;
      std::string buffer{};

      expect(!glz::read<glz::opts{}>(vec, json));
      expect(vec.size() == 5);
      expect(vec == inplace_vector<int, 10>{1, 2, 3, 4, 5});

      expect(!glz::write<glz::opts{}>(vec, buffer));
      expect(buffer == json);
   };

   "int_vec_overflow"_test = [] {
      inplace_vector<int, 10> vec;
      expect(glz::read<glz::opts{}>(vec, "[1,2,3,4,5,6,7,8,9,10]") == glz::error_code::none);
      expect(vec.size() == 10);

      expect(glz::read<glz::opts{}>(vec, "[1,2,3,4,5,6,7,8,9,10,11]") == glz::error_code::exceeded_static_array_size);
      expect(vec.size() == 10);

      expect(glz::read<glz::opts{}>(vec, "[1]") == glz::error_code::none);
      expect(vec.size() == 1);

      expect(glz::read<glz::opts{}>(vec, "[1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16]") ==
             glz::error_code::exceeded_static_array_size);
      expect(vec.size() == 10);

      expect(glz::read<glz::opts{}>(vec, "[]") == glz::error_code::none);
      expect(vec.size() == 0);
   };

   "struct_vec"_test = [] {
      const std::string_view json{R"({"vec":[{"a":1,"b":2,"c":3},{"a":4,"b":5,"c":6},{"a":7,"b":8,"c":9}]})"};
      std::string buffer{};
      my_struct s;

      expect(!glz::read<glz::opts{}>(s, json));
      expect(s.vec.size() == 3);
      expect(s.vec == inplace_vector<my_struct::entry_t, 3>{{1, 2, 3}, {4, 5, 6}, {7, 8, 9}});

      expect(!glz::write<glz::opts{}>(s, buffer));
      expect(buffer == json);
   };

   "pair_vec"_test = [] {
      inplace_vector<std::pair<int, int>, 2> vec;
      expect(!glz::read_json(vec, R"({"1":2,"3":4})"));
      expect(vec.size() == 2);
      const auto s = glz::write_json(vec).value_or("error");
      expect(s == R"({"1":2,"3":4})") << s;

      expect(glz::read_json(vec, R"({"1":2,"3":4,"5":6})") == glz::error_code::exceeded_static_array_size);
      expect(glz::read_json(vec, R"({})") == glz::error_code::none);
      expect(vec.size() == 0);
   };
};

int main() { return 0; }
