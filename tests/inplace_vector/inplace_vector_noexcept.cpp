#include <beman/inplace_vector/inplace_vector.hpp>

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
   };
   inplace_vector<entry_t, 3> vec;
};

suite json_test = [] {
   "int_vec"_test = [] {
      const std::string_view json{R"([1,2,3,4,5])"};
      inplace_vector<int, 10> vec;
      std::string buffer{};

      expect(!glz::read<glz::opts{}>(vec, json));
      expect(vec.size() == 5);
      // at the time of writing beman inplace_vector initializer list dose not work without exceptions
      expect(vec[0] == 1);
      expect(vec[1] == 2);
      expect(vec[2] == 3);
      expect(vec[3] == 4);
      expect(vec[4] == 5);

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
      // at the time of writing beman inplace_vector initializer list dose not work without exceptions
      expect(s.vec[0].a == 1);
      expect(s.vec[0].b == 2);
      expect(s.vec[0].c == 3);
      expect(s.vec[1].a == 4);
      expect(s.vec[1].b == 5);
      expect(s.vec[1].c == 6);
      expect(s.vec[2].a == 7);
      expect(s.vec[2].b == 8);
      expect(s.vec[2].c == 9);

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
