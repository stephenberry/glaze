// Glaze Library
// For the license information refer to glaze.hpp

// Variants with more than 64 alternatives. Key deduction narrows a candidate bit set that spans several
// words, and the lone survivor must be read back as exactly the alternative that was written.

#include <string>

#include "glaze/glaze.hpp"
#include "ut/ut.hpp"
#include "wide_variant_types.hpp"

using namespace ut;

namespace
{
   constexpr glz::opts tolerant{.error_on_unknown_keys = false};

   std::string member_json(const size_t index, const int value)
   {
      return "\"k" + std::to_string(index) + "\":" + std::to_string(value);
   }

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
                       ", got index " + std::to_string(out.index()) + " (" + glz::format_error(ec, buffer) +
                       ") reading " + buffer;
      return ok;
   }

   // Every alternative written by the JSON writer reads back as itself
   template <class V, auto Opts = glz::opts{}>
   void roundtrip_every_alternative()
   {
      bool ok = true;
      for (size_t i = 0; ok && i < std::variant_size_v<V>; ++i) {
         std::string buffer{};
         ok =
            !glz::write_json(wide_variant::make<V>(i, int(i) + 1), buffer) && reads_as<Opts, V>(buffer, i, int(i) + 1);
      }
      expect(ok);
   }

   // Unknown keys around the deciding key are tolerated without disturbing the deduction
   template <class V>
   void unknown_keys_around_every_alternative()
   {
      bool ok = true;
      for (size_t i = 0; ok && i < std::variant_size_v<V>; ++i) {
         const auto member = member_json(i, int(i) + 7);
         ok = reads_as<tolerant, V>("{\"zz\":0," + member + "}", i, int(i) + 7) &&
              reads_as<tolerant, V>("{" + member + ",\"zz\":0}", i, int(i) + 7);
      }
      expect(ok);
   }
}

suite json_wide_variant_tests = [] {
   using namespace wide_variant;

   "untagged 65 alternatives"_test = [] {
      roundtrip_every_alternative<variant<65>>();
      roundtrip_every_alternative<variant<65>, tolerant>();
      unknown_keys_around_every_alternative<variant<65>>();
   };

   "untagged 129 alternatives"_test = [] { roundtrip_every_alternative<variant<129>>(); };

   "untagged unknown key is rejected"_test = [] {
      variant<65> v{};
      expect(glz::read_json(v, std::string{"{\"zz\":0}"}) == glz::error_code::unknown_key);
   };

   "tagged 65 alternatives"_test = [] { roundtrip_every_alternative<tagged_variant<65>>(); };

   "tagged 65 alternatives, discriminator last"_test = [] {
      // With the discriminator after the member, the reader deduces from the member key and then checks
      // the deduced alternative against the discriminator
      bool ok = true;
      for (size_t i = 0; ok && i < 65; ++i) {
         const auto doc = "{" + member_json(i, int(i) + 3) + ",\"type\":\"k" + std::to_string(i) + "\"}";
         ok = reads_as<glz::opts{}, tagged_variant<65>>(doc, i, int(i) + 3);
      }
      expect(ok);
   };

   "tagged 65 alternatives, discriminator contradicts the keys"_test = [] {
      tagged_variant<65> v{};
      expect(glz::read_json(v, std::string{"{\"k64\":1,\"type\":\"k1\"}"}) ==
             glz::error_code::no_matching_variant_type);
      expect(glz::read_json(v, std::string{"{\"k1\":1,\"type\":\"k64\"}"}) ==
             glz::error_code::no_matching_variant_type);
   };
};

int main() { return 0; }
