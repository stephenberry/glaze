#include <string>
#include <vector>

#include "glaze/util/parse.hpp"
#include "ut/ut.hpp"

using namespace ut;

// Helper to adapt string/string_view tests to pointer+size API
inline bool validate(std::string_view s) { return glz::validate_utf8(s.data(), s.size()); }

suite utf8_validation_tests = [] {
   "ascii_valid"_test = [] {
      expect(validate("Hello World"));
      expect(validate(""));
      expect(validate("1234567890")); // > 8 chars for SWAR
      std::string long_ascii(1000, 'a');
      expect(validate(long_ascii));
   };

   "ascii_invalid"_test = [] {
      // High bit set in otherwise ASCII-looking string
      std::string s = "Hello";
      s += static_cast<char>(0x80);
      s += "World";
      expect(!validate(s));
   };

   "utf8_2byte_valid"_test = [] {
      expect(validate("£")); // C2 A3
      expect(validate("a£b"));
      // Boundary condition for SWAR (8 bytes)
      // "aaaaaaa£" -> 7 'a' + 2 bytes = 9 bytes
      expect(validate("aaaaaaa£"));
   };

   "utf8_2byte_invalid"_test = [] {
      // C0 80 is overlong for U+0000 (NUL) -> Invalid
      const char overlong[] = "\xC0\x80";
      expect(!validate(std::string_view(overlong, 2)));

      // C1 BF is overlong for U+007F -> Invalid
      const char overlong2[] = "\xC1\xBF";
      expect(!validate(std::string_view(overlong2, 2)));

      // Missing continuation
      const char missing[] = "\xC2";
      expect(!validate(std::string_view(missing, 1)));

      // Bad continuation
      const char bad_cont[] = "\xC2\x20"; // space instead of continuation
      expect(!validate(std::string_view(bad_cont, 2)));
   };

   "utf8_3byte_valid"_test = [] {
      expect(validate("€")); // E2 82 AC
      expect(validate("한")); // ED 95 9C
   };

   "utf8_3byte_invalid"_test = [] {
      // Overlong (could be represented in 2 bytes)
      // E0 80 80 -> U+0000
      const char overlong[] = "\xE0\x80\x80";
      expect(!validate(std::string_view(overlong, 3)));

      // E0 9F BF -> U+07FF (Last code point for 2 bytes is U+07FF)
      const char overlong2[] = "\xE0\x9F\xBF";
      expect(!validate(std::string_view(overlong2, 3)));

      // Surrogate pairs (invalid in UTF-8)
      // ED A0 80 -> U+D800 (High surrogate start)
      const char surrogate_start[] = "\xED\xA0\x80";
      expect(!validate(std::string_view(surrogate_start, 3)));

      // ED BF BF -> U+DFFF (Low surrogate end)
      const char surrogate_end[] = "\xED\xBF\xBF";
      expect(!validate(std::string_view(surrogate_end, 3)));

      // Truncated
      const char truncated[] = "\xE2\x82";
      expect(!validate(std::string_view(truncated, 2)));
   };

   "utf8_4byte_valid"_test = [] {
      expect(validate("𐍈")); // F0 90 8D 88
      expect(validate("💩")); // F0 9F 92 A9
   };

   "utf8_4byte_invalid"_test = [] {
      // Overlong (could be 3 bytes)
      // F0 80 80 80
      const char overlong[] = "\xF0\x80\x80\x80";
      expect(!validate(std::string_view(overlong, 4)));

      // F0 8F BF BF -> U+FFFF
      const char overlong2[] = "\xF0\x8F\xBF\xBF";
      expect(!validate(std::string_view(overlong2, 4)));

      // Too large (> U+10FFFF)
      // F4 90 80 80 -> U+110000
      const char too_large[] = "\xF4\x90\x80\x80";
      expect(!validate(std::string_view(too_large, 4)));

      // F5 80 80 80
      const char too_large2[] = "\xF5\x80\x80\x80";
      expect(!validate(std::string_view(too_large2, 4)));

      // Truncated
      const char truncated[] = "\xF0\x9F\x92";
      expect(!validate(std::string_view(truncated, 3)));
   };

   "swar_boundary_tests"_test = [] {
      // Test SWAR logic transitions
      // 8 bytes ASCII -> Valid
      expect(validate("12345678"));

      // 9 bytes ASCII -> Valid (SWAR loop + 1 byte loop)
      expect(validate("123456789"));

      // 7 bytes ASCII -> Valid (Loop only)
      expect(validate("1234567"));

      // 8 bytes with last one invalid
      std::string s = "1234567";
      s += static_cast<char>(0xFF);
      expect(!validate(s));

      // 8 bytes with first one invalid
      s = "";
      s += static_cast<char>(0xFF);
      s += "1234567";
      expect(!validate(s));
   };
};

int main() {}