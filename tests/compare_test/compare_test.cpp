// Glaze Library
// For the license information refer to glaze.hpp

#include "glaze/compare/compare.hpp"

#include <iterator>
#include <set>
#include <string>

#include "glaze/compare/approx.hpp"
#include "glaze/util/compare.hpp"
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

suite striequal_tests = [] {
   "striequal basics"_test = [] {
      expect(glz::striequal("", ""));
      expect(glz::striequal("a", "A"));
      expect(glz::striequal("Connection", "connection"));
      expect(glz::striequal("KEEP-ALIVE", "keep-alive"));
      expect(!glz::striequal("close", "closed"));
      expect(!glz::striequal("close", "clos"));
      expect(!glz::striequal("abc", "abd"));
   };

   "striequal block boundaries"_test = [] {
      expect(glz::striequal("12345678", "12345678"));
      expect(glz::striequal("AbCdEfGh", "aBcDeFgH"));
      expect(glz::striequal("Transfer-Encoding", "TRANSFER-ENCODING"));
      expect(!glz::striequal("Transfer-Encoding", "Transfer-Encodinh"));
      expect(!glz::striequal("AbCdEfGhX", "aBcDeFgHY"));
   };

   "striequal only letters fold"_test = [] {
      // '@' (0x40) vs '`' (0x60) and '[' (0x5B) vs '{' (0x7B) differ only in bit 0x20
      expect(!glz::striequal("@", "`"));
      expect(!glz::striequal("[", "{"));
      expect(!glz::striequal("0", "P"));
      expect(glz::striequal("a1!Z", "A1!z"));
   };

   "striequal non-ascii"_test = [] {
      expect(!glz::striequal("\xC1", "\xE1"));
      expect(glz::striequal("\xC1", "\xC1"));
      expect(glz::striequal("caf\xC3\xA9", "CAF\xC3\xA9"));
      expect(!glz::striequal("\xFF\x41", "\xFF\x42"));
      expect(glz::striequal("\xFF\x41", "\xFF\x61"));
   };

   "striequal constexpr"_test = [] {
      static_assert(glz::striequal("Host", "host"));
      static_assert(!glz::striequal("Host", "hose"));
   };
};

struct ordering_t
{
   int a{};
   int b{};
   std::string c{};
};

template <>
struct glz::meta<ordering_t>
{
   using T = ordering_t;
   static constexpr auto value = object("a", &T::a, "b", &T::b, "c", &T::c);
};

suite ordering = [] {
   "orderings compare members lexicographically"_test = [] {
      const ordering_t x{1, 2, "m"};
      const ordering_t later_member{1, 3, "a"};
      const ordering_t earlier_member{0, 9, "z"};

      expect(glz::less{}(x, later_member));
      expect(!glz::less{}(later_member, x));
      expect(glz::greater{}(later_member, x));
      expect(!glz::greater{}(x, later_member));
      expect(glz::less_equal{}(x, later_member));
      expect(!glz::less_equal{}(later_member, x));
      expect(glz::greater_equal{}(later_member, x));
      expect(!glz::greater_equal{}(x, later_member));

      expect(glz::less{}(earlier_member, x));
      expect(glz::greater{}(x, earlier_member));
   };

   "equal objects are neither less nor greater"_test = [] {
      const ordering_t x{1, 2, "m"};
      const ordering_t y{1, 2, "m"};

      expect(!glz::less{}(x, y));
      expect(!glz::greater{}(x, y));
      expect(glz::less_equal{}(x, y));
      expect(glz::greater_equal{}(x, y));
   };

   "incomparability is transitive"_test = [] {
      const ordering_t a{1, 1, ""};
      const ordering_t b{2, 0, ""};
      const ordering_t c{2, 1, ""};

      expect(glz::less{}(a, b));
      expect(glz::less{}(b, c));
      expect(glz::less{}(a, c));
   };

   "less works as a set comparator"_test = [] {
      std::set<ordering_t, glz::less> values;
      values.insert({1, 3, ""});
      values.insert({1, 2, ""});
      values.insert({0, 9, ""});
      values.insert({1, 2, ""});

      expect(values.size() == 3u);
      expect(values.begin()->a == 0);
      expect(std::next(values.begin())->b == 2);
      expect(std::prev(values.end())->b == 3);
   };
};

int main() { return 0; }
