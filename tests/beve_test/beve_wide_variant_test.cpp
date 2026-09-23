// Glaze Library
// For the license information refer to glaze.hpp

// Variants with more than 64 alternatives. Key deduction narrows a candidate bit set that spans several
// words, and the lone survivor must be read back as exactly the alternative that was written.

#include <map>
#include <string>

#include "glaze/glaze.hpp"
#include "ut/ut.hpp"
#include "wide_variant_types.hpp"

using namespace ut;

namespace
{
   constexpr glz::opts beve{.format = glz::BEVE};
   constexpr glz::opts tolerant{.format = glz::BEVE, .error_on_unknown_keys = false};

   // Reads `buffer` into a variant that starts out holding a different alternative, and checks that
   // alternative `index` holding `value` comes out
   template <auto Opts, class V>
   bool reads_as(const std::string& buffer, const size_t index, const int value)
   {
      constexpr auto N = std::variant_size_v<V>;
      auto out = wide_variant::make<V>((index + 1) % N, 0);
      const auto ec = glz::read<Opts>(out, buffer);
      const bool ok = !ec && out.index() == index && wide_variant::value_of(out) == value;
      expect(ok) << "variant of " + std::to_string(N) + " alternatives, expected index " + std::to_string(index) +
                       ", got index " + std::to_string(out.index()) + " (" + glz::format_error(ec) + ")";
      return ok;
   }

   // Every alternative written by the BEVE writer reads back as itself
   template <class V>
   void roundtrip_every_alternative()
   {
      bool ok = true;
      for (size_t i = 0; ok && i < std::variant_size_v<V>; ++i) {
         std::string buffer{};
         ok = !glz::write_beve(wide_variant::make<V>(i, int(i) + 1), buffer) &&
              reads_as<beve, V>(buffer, i, int(i) + 1);
      }
      expect(ok);
   }
}

suite beve_wide_variant_tests = [] {
   using namespace wide_variant;

   "untagged 65 alternatives"_test = [] { roundtrip_every_alternative<variant<65>>(); };

   "untagged 129 alternatives"_test = [] { roundtrip_every_alternative<variant<129>>(); };

   "tagged 65 alternatives"_test = [] { roundtrip_every_alternative<tagged_variant<65>>(); };

   "untagged 65 alternatives with an unknown key"_test = [] {
      // The foreign key matches no alternative, so deduction rests on the one key that does
      bool ok = true;
      for (size_t i = 0; ok && i < 65; ++i) {
         const std::map<std::string, int> object{{"k" + std::to_string(i), int(i) + 7}, {"zz", 0}};
         std::string buffer{};
         ok = !glz::write_beve(object, buffer) && reads_as<tolerant, variant<65>>(buffer, i, int(i) + 7);
      }
      expect(ok);
   };

   "untagged 65 alternatives with only an unknown key"_test = [] {
      const std::map<std::string, int> object{{"zz", 0}};
      std::string buffer{};
      expect(not glz::write_beve(object, buffer));
      variant<65> v{};
      expect(bool(glz::read_beve(v, buffer)));
   };
};

int main() { return 0; }
