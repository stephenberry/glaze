// Glaze Library
// For the license information refer to glaze.ixx

// Separated from json_reflection_test.cpp to work around GCC compiler bug
// with large translation units and DISABLE_ALWAYS_INLINE

import std;
import glaze;
import ut;

using namespace ut;

struct front_32_t
{
   int aaaa{};
   int aaab{};
   int aaba{};
   int bbbb{};
   int aabb{};
};

struct front_64_t
{
   int aaaaaaaa{};
   int aaaaaaaz{};
   int aaaaaaza{};
   int zzzzzzzz{};
   int aaaaaazz{};
};

struct three_element_unique_t
{
   int aaaaaaaa{};
   int aaaaaaab{};
   int aaaaaabc{};
};

suite front_hash_tests = [] {
   "front_32"_test = [] {
      front_32_t obj{};
      std::string_view buffer = R"({"aaaa":1,"aaab":2,"aaba":3})";
      auto ec = glz::read_json(obj, buffer);
      expect(not ec) << glz::format_error(ec, buffer);
      expect(obj.aaaa == 1);
      expect(obj.aaab == 2);
      expect(obj.aaba == 3);
   };

   "front_64"_test = [] {
      front_64_t obj{};
      std::string_view buffer = R"({"aaaaaaaa":1,"aaaaaaaz":2,"aaaaaaza":3})";
      auto ec = glz::read_json(obj, buffer);
      expect(not ec) << glz::format_error(ec, buffer);
      expect(obj.aaaaaaaa == 1);
      expect(obj.aaaaaaaz == 2);
      expect(obj.aaaaaaza == 3);
   };

   "three_element_unique"_test = [] {
      three_element_unique_t obj{};
      std::string_view buffer = R"({"aaaaaaaa":1,"aaaaaaab":2,"aaaaaabc":3})";
      auto ec = glz::read_json(obj, buffer);
      expect(not ec) << glz::format_error(ec, buffer);
      expect(obj.aaaaaaaa == 1);
      expect(obj.aaaaaaab == 2);
      expect(obj.aaaaaabc == 3);
   };
};

int main() { return 0; }
