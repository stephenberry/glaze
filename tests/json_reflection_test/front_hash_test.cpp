// Separated from json_reflection_test.cpp to work around GCC compiler bug
// with large translation units and DISABLE_ALWAYS_INLINE

#include "glaze/glaze.hpp"
#include "ut/ut.hpp"

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
      glz::keys_info_t info{.min_length = 8, .max_length = 8};
      [[maybe_unused]] const auto valid = glz::front_bytes_hash_info<uint64_t>(glz::reflect<front_64_t>::keys, info);

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
