#include <cstdint>
#include <cstring>
#include <random>
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

suite utf8_stream_validation_tests = [] {
   "utf8_stream_split_sequences"_test = [] {
      glz::utf8_stream_validator validator;

      const char part1[] = "\xF0\x9F";
      expect(validator.consume(part1, 2));
      expect(!validator.complete());

      const char part2[] = "\x92\xA9";
      expect(validator.consume(part2, 2));
      expect(validator.complete());

      validator.reset();
      const char euro_part1[] = "\xE2\x82";
      const char euro_part2[] =
         "\xAC"
         " hello";
      expect(validator.consume(euro_part1, 2));
      expect(!validator.complete());
      expect(validator.consume(euro_part2, 7));
      expect(validator.complete());
   };

   "utf8_stream_invalid_across_chunks"_test = [] {
      glz::utf8_stream_validator validator;

      const char lead[] = "\xC2";
      expect(validator.consume(lead, 1));
      expect(!validator.complete());
      expect(!validator.consume(" ", 1));

      validator.reset();
      const char overlong_part1[] = "\xF0";
      const char overlong_part2[] = "\x8F\xBF\xBF";
      expect(validator.consume(overlong_part1, 1));
      expect(!validator.consume(overlong_part2, 3));

      validator.reset();
      const char truncated[] = "\xE2\x82";
      expect(validator.consume(truncated, 2));
      expect(!validator.complete());
   };

   "utf8_stream_valid_boundaries_and_all_splits"_test = [] {
      auto expect_valid_for_all_splits = [](const char* bytes, size_t size) {
         for (size_t split = 0; split <= size; ++split) {
            glz::utf8_stream_validator validator;

            expect(validator.consume(bytes, split));

            if (split > 0 && split < size) {
               expect(!validator.complete());
            }

            expect(validator.consume(bytes + split, size - split));
            expect(validator.complete());
         }
      };

      expect_valid_for_all_splits("\x7F", 1);
      expect_valid_for_all_splits("\xC2\x80", 2);
      expect_valid_for_all_splits("\xDF\xBF", 2);
      expect_valid_for_all_splits("\xE0\xA0\x80", 3);
      expect_valid_for_all_splits("\xED\x9F\xBF", 3);
      expect_valid_for_all_splits("\xEE\x80\x80", 3);
      expect_valid_for_all_splits("\xEF\xBF\xBF", 3);
      expect_valid_for_all_splits("\xF0\x90\x80\x80", 4);
      expect_valid_for_all_splits("\xF4\x8F\xBF\xBF", 4);
   };

   "utf8_stream_invalid_boundaries"_test = [] {
      auto expect_invalid = [](const char* bytes, size_t size) {
         glz::utf8_stream_validator validator;
         expect(!validator.consume(bytes, size));
      };

      expect_invalid("\x80", 1);
      expect_invalid("\xBF", 1);

      expect_invalid("\xC0\x80", 2);
      expect_invalid("\xC1\xBF", 2);

      expect_invalid("\xE0\x80\x80", 3);
      expect_invalid("\xE0\x9F\xBF", 3);

      expect_invalid("\xED\xA0\x80", 3);
      expect_invalid("\xED\xBF\xBF", 3);

      expect_invalid("\xF0\x80\x80\x80", 4);
      expect_invalid("\xF0\x8F\xBF\xBF", 4);

      expect_invalid("\xF4\x90\x80\x80", 4);
      expect_invalid("\xF4\xBF\xBF\xBF", 4);

      expect_invalid("\xF5\x80\x80\x80", 4);
      expect_invalid("\xFE", 1);
      expect_invalid("\xFF", 1);
   };

   "utf8_stream_invalid_continuation_positions"_test = [] {
      auto expect_invalid = [](const char* bytes, size_t size) {
         glz::utf8_stream_validator validator;
         expect(!validator.consume(bytes, size));
      };

      expect_invalid("\xC2\x20", 2);

      expect_invalid("\xE2\x20\x80", 3);
      expect_invalid("\xE2\x82\x20", 3);

      expect_invalid("\xF0\x90\x20\x80", 4);
      expect_invalid("\xF0\x90\x80\x20", 4);
   };

   "utf8_stream_invalid_boundary_rules_across_chunks"_test = [] {
      auto expect_invalid_across_chunks = [](const char* first, size_t first_size, const char* second,
                                             size_t second_size) {
         glz::utf8_stream_validator validator;

         expect(validator.consume(first, first_size));
         expect(!validator.complete());
         expect(!validator.consume(second, second_size));
      };

      expect_invalid_across_chunks("\xC2", 1, "\x20", 1);

      expect_invalid_across_chunks("\xE0", 1, "\x9F\x80", 2);
      expect_invalid_across_chunks("\xED", 1, "\xA0\x80", 2);

      expect_invalid_across_chunks("\xF0", 1, "\x8F\xBF\xBF", 3);
      expect_invalid_across_chunks("\xF4", 1, "\x90\x80\x80", 3);

      expect_invalid_across_chunks("\xE2\x82", 2, "\x20", 1);
      expect_invalid_across_chunks("\xF0\x90\x80", 3, "\x20", 1);
   };

   "utf8_stream_truncated_sequences"_test = [] {
      auto expect_incomplete = [](const char* bytes, size_t size) {
         glz::utf8_stream_validator validator;

         expect(validator.consume(bytes, size));
         expect(!validator.complete());
      };

      expect_incomplete("\xC2", 1);

      expect_incomplete("\xE0", 1);
      expect_incomplete("\xE0\xA0", 2);

      expect_incomplete("\xF0", 1);
      expect_incomplete("\xF0\x90", 2);
      expect_incomplete("\xF0\x90\x80", 3);
   };

   "utf8_stream_empty_ascii_and_reset"_test = [] {
      glz::utf8_stream_validator validator;

      expect(validator.consume("", 0));
      expect(validator.complete());

      expect(validator.consume("hello", 5));
      expect(validator.complete());

      expect(validator.consume("\xE2", 1));
      expect(!validator.complete());

      validator.reset();
      expect(validator.complete());

      expect(validator.consume("ok", 2));
      expect(validator.complete());
   };
};

// Independent scalar reference, intentionally unrelated to glz's implementations, used to
// cross-check glz::validate_utf8 (which dispatches to the SIMD range validator for buffers
// >= 512 bytes). Running this on CI verifies the NEON / AVX2 / SSSE3 / scalar paths agree.
namespace
{
   bool reference_validate_utf8(const uint8_t* it, const uint8_t* end) noexcept
   {
      while (it < end) {
         const uint8_t b = *it;
         if (b < 0x80) {
            ++it;
         }
         else if ((b & 0xE0) == 0xC0) {
            if (it + 2 > end || (it[1] & 0xC0) != 0x80 || b < 0xC2) return false;
            it += 2;
         }
         else if ((b & 0xF0) == 0xE0) {
            if (it + 3 > end || (it[1] & 0xC0) != 0x80 || (it[2] & 0xC0) != 0x80) return false;
            if (b == 0xE0 && it[1] < 0xA0) return false;
            if (b == 0xED && it[1] >= 0xA0) return false;
            it += 3;
         }
         else if ((b & 0xF8) == 0xF0) {
            if (it + 4 > end || (it[1] & 0xC0) != 0x80 || (it[2] & 0xC0) != 0x80 || (it[3] & 0xC0) != 0x80)
               return false;
            if (b == 0xF0 && it[1] < 0x90) return false;
            if (b == 0xF4 && it[1] >= 0x90) return false;
            if (b > 0xF4) return false;
            it += 4;
         }
         else {
            return false;
         }
      }
      return true;
   }

   void append_codepoint(std::vector<uint8_t>& v, uint32_t cp)
   {
      if (cp < 0x80) {
         v.push_back(uint8_t(cp));
      }
      else if (cp < 0x800) {
         v.push_back(uint8_t(0xC0 | (cp >> 6)));
         v.push_back(uint8_t(0x80 | (cp & 0x3F)));
      }
      else if (cp < 0x10000) {
         v.push_back(uint8_t(0xE0 | (cp >> 12)));
         v.push_back(uint8_t(0x80 | ((cp >> 6) & 0x3F)));
         v.push_back(uint8_t(0x80 | (cp & 0x3F)));
      }
      else {
         v.push_back(uint8_t(0xF0 | (cp >> 18)));
         v.push_back(uint8_t(0x80 | ((cp >> 12) & 0x3F)));
         v.push_back(uint8_t(0x80 | ((cp >> 6) & 0x3F)));
         v.push_back(uint8_t(0x80 | (cp & 0x3F)));
      }
   }
}

suite utf8_simd_differential_tests = [] {
   // Cross-check the production validate_utf8 (SIMD-dispatched for >= 512 bytes) against the
   // independent reference across many sizes, weighting the buffers around the 512-byte
   // threshold and exercising valid multibyte text, truncated sequences, and invalid bytes.
   "utf8_validate_matches_reference"_test = [] {
      std::mt19937_64 rng(0x9E3779B97F4A7C15ull);
      static constexpr uint8_t palette[] = {0x00, 0x41, 0x7F, 0x80, 0x9F, 0xA0, 0xBF, 0xC0, 0xC1, 0xC2, 0xDF,
                                            0xE0, 0xED, 0xEE, 0xEF, 0xF0, 0xF4, 0xF5, 0xF7, 0xF8, 0xFF};
      static constexpr uint32_t code_points[] = {0x24,   0xA2,   0x7F,    0x80,    0x7FF,   0x800,  0xD7FF,
                                                 0xE000, 0xFFFD, 0x10000, 0x10FFFF, 0xD800, 0xDFFF, 0x110000};

      std::vector<uint8_t> buf;
      std::uint64_t mismatches = 0;
      for (int i = 0; i < 300000; ++i) {
         buf.clear();
         // Sizes span the 512-byte SIMD threshold (mostly 0..1100, with some larger).
         const size_t target = (i % 4 == 0) ? (rng() % 2200) : (350 + rng() % 400);
         while (buf.size() < target) {
            const uint32_t r = rng() % 16;
            if (r < 2) {
               append_codepoint(buf, code_points[rng() % (sizeof(code_points) / sizeof(code_points[0]))]);
            }
            else if (r < 3) {
               buf.push_back(palette[rng() % sizeof(palette)]);
            }
            else {
               buf.push_back(uint8_t('a' + (rng() % 26)));
            }
         }
         buf.resize(target);
         // Occasionally truncate the tail to land inside a multibyte sequence.
         if ((rng() & 7) == 0 && !buf.empty()) buf.resize(rng() % buf.size());

         const bool expected = reference_validate_utf8(buf.data(), buf.data() + buf.size());
         const bool actual = glz::validate_utf8(buf.data(), buf.size());
         if (expected != actual) ++mismatches;
      }
      expect(mismatches == 0u);
   };
};

int main() {}
