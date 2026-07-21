// Glaze Library
// For the license information refer to glaze.hpp

#include "glaze/compare/compare.hpp"

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

int main() { return 0; }
