// Glaze Library
// For the license information refer to glaze.hpp

#include <array>
#include <cstdint>
#include <string>
#include <ut/ut.hpp>

#include "glaze/base64/base64.hpp"
#include "glaze/util/bit_array.hpp"
#include "glaze/util/progress_bar.hpp"

using namespace ut;

suite base64_read_tests = [] {
   "hello world"_test = [] {
      std::string_view b64 = "aGVsbG8gd29ybGQ=";
      const auto decoded = glz::read_base64(b64);
      expect(decoded == "hello world");
   };

   "{\"key\":42}"_test = [] {
      std::string_view b64 = "eyJrZXkiOjQyfQ==";
      const auto decoded = glz::read_base64(b64);
      expect(decoded == "{\"key\":42}");
   };
};

suite base64_roundtrip_tests = [] {
   "hello world"_test = [] {
      std::string_view str = "Hello World";
      const auto b64 = glz::write_base64(str);
      const auto decoded = glz::read_base64(b64);
      expect(decoded == str);
   };

   "{\"key\":42}"_test = [] {
      std::string_view str = "{\"key\":42}";
      const auto b64 = glz::write_base64(str);
      const auto decoded = glz::read_base64(b64);
      expect(decoded == str);
   };
};

suite progress_bar_tests = [] {
   "progress bar 30%"_test = [] {
      glz::progress_bar bar{.width = 12, .completed = 3, .total = 10, .time_taken = 30.0};
      expect(bar.string() == "[===-------] 30% | ETA: 1m 10s | 3/10") << bar.string();
   };

   "progress bar 100%"_test = [] {
      glz::progress_bar bar{.width = 12, .completed = 10, .total = 10, .time_taken = 30.0};
      expect(bar.string() == "[==========] 100% | ETA: 0m 0s | 10/10") << bar.string();
   };

   "progress bar width 0 (no bar drawn)"_test = [] {
      glz::progress_bar bar{.width = 0, .completed = 3, .total = 10, .time_taken = 30.0};
      expect(bar.string() == " 30% | ETA: 1m 10s | 3/10") << bar.string();
   };

   "progress bar total 0 (clamp-to-1)"_test = [] {
      glz::progress_bar bar{.width = 12, .completed = 0, .total = 0, .time_taken = 30.0};
      expect(bar.string() == "[----------] 0% | ETA: 0m 30s | 0/1") << bar.string();
   };
};

// Naive reference for glz::bit_array: one bool per bit, scanned one bit at a time
template <size_t N>
struct naive_bits
{
   std::array<bool, N> bits{};

   int popcount() const
   {
      int n{};
      for (const bool b : bits) {
         n += int(b);
      }
      return n;
   }

   int countr_zero() const
   {
      for (size_t i = 0; i < N; ++i) {
         if (bits[i]) return int(i);
      }
      return int(N);
   }

   int countl_zero() const
   {
      for (size_t i = N; i-- > 0;) {
         if (bits[i]) return int(N - 1 - i);
      }
      return int(N);
   }
};

template <size_t N, class Chunk>
std::string bit_array_label(const std::string& what)
{
   return "bit_array<" + std::to_string(N) + ", " + std::to_string(sizeof(Chunk) * 8) + "-bit chunk> " + what;
}

// Returns true when every observable property of `b` matches the reference, reporting the first mismatch
template <size_t N, class Chunk>
bool matches(const glz::bit_array<N, Chunk>& b, const naive_bits<N>& ref, const std::string& what)
{
   for (size_t i = 0; i < N; ++i) {
      if (b[i] != ref.bits[i]) {
         expect(false) << bit_array_label<N, Chunk>(what) + ": bit " + std::to_string(i) + " differs";
         return false;
      }
   }
   const auto check = [&](const char* op, int got, int want) {
      expect(got == want) << bit_array_label<N, Chunk>(what) + ": " + op + " = " + std::to_string(got) + ", expected " +
                                std::to_string(want);
      return got == want;
   };
   return check("popcount", b.popcount(), ref.popcount()) && check("countr_zero", b.countr_zero(), ref.countr_zero()) &&
          check("countl_zero", b.countl_zero(), ref.countl_zero()) &&
          check("has_single_bit", int(b.has_single_bit()), int(ref.popcount() == 1));
}

template <size_t N, class Chunk = uint64_t>
void check_bit_array()
{
   using bits_t = glz::bit_array<N, Chunk>;

   const bits_t empty{};
   const naive_bits<N> empty_ref{};
   expect(matches(empty, empty_ref, "empty"));

   bits_t full = bits_t{}.flip();
   naive_bits<N> full_ref{};
   full_ref.bits.fill(true);
   expect(matches(full, full_ref, "flipped empty"));
   expect(bits_t{full}.flip() == empty) << bit_array_label<N, Chunk>("double flip");

   // Every single bit: alone, with every other bit set, and cleared again
   bool single_ok = true;
   for (size_t i = 0; i < N && single_ok; ++i) {
      bits_t b{};
      b[i] = true;
      naive_bits<N> ref{};
      ref.bits[i] = true;
      const auto label = "bit " + std::to_string(i);
      single_ok = matches(b, ref, label);

      auto inverted = b;
      inverted.flip();
      auto inverted_ref = full_ref;
      inverted_ref.bits[i] = false;
      single_ok = single_ok && matches(inverted, inverted_ref, "all but " + label);
      single_ok = single_ok && (inverted | b) == full && (inverted & b) == empty;

      b[i] = false;
      single_ok = single_ok && matches(b, empty_ref, label + " cleared");
   }
   expect(single_ok) << bit_array_label<N, Chunk>("single bits");

   // Every pair of bits, combined through each of the set operations
   bool pairs_ok = true;
   for (size_t i = 0; i < N && pairs_ok; ++i) {
      for (size_t j = 0; j < N && pairs_ok; ++j) {
         bits_t a{}, b{};
         a[i] = true;
         b[j] = true;
         naive_bits<N> or_ref{}, and_ref{};
         or_ref.bits[i] = or_ref.bits[j] = true;
         and_ref.bits[i] = (i == j);
         const auto label = "bits " + std::to_string(i) + ", " + std::to_string(j);
         pairs_ok =
            matches(a | b, or_ref, label + " (|)") && matches(a & b, and_ref, label + " (&)") && (a == b) == (i == j);
      }
   }
   expect(pairs_ok) << bit_array_label<N, Chunk>("pairs");
}

// The bit scans are usable in constant expressions, and the top chunk's padding never leaks into them
static_assert(glz::bit_array<65>{}.flip().popcount() == 65);
static_assert(glz::bit_array<65>{}.flip().countl_zero() == 0);
static_assert(glz::bit_array<200>{}.countr_zero() == 200);
static_assert(glz::bit_array<200>{}.countl_zero() == 200);
static_assert([] {
   glz::bit_array<128> b{};
   b[1] = true;
   return b.countr_zero() == 1 && b.countl_zero() == 126;
}());

suite bit_array_tests = [] {
   "bit_array 64-bit chunks"_test = [] {
      check_bit_array<1>();
      check_bit_array<63>();
      check_bit_array<64>();
      check_bit_array<65>();
      check_bit_array<127>();
      check_bit_array<128>();
      check_bit_array<129>();
      check_bit_array<200>();
   };

   "bit_array narrow chunks"_test = [] {
      check_bit_array<1, uint8_t>();
      check_bit_array<7, uint8_t>();
      check_bit_array<8, uint8_t>();
      check_bit_array<9, uint8_t>();
      check_bit_array<17, uint8_t>();
      check_bit_array<24, uint8_t>();
      check_bit_array<33, uint32_t>();
      check_bit_array<64, uint32_t>();
      check_bit_array<65, uint32_t>();
   };

   "bit_array zero size"_test = [] {
      glz::bit_array<0> b{};
      expect(b.popcount() == 0);
      expect(b.countr_zero() == 0);
      expect(b.countl_zero() == 0);
      expect(not b.has_single_bit());
      expect(b.flip() == glz::bit_array<0>{});
   };
};

int main() { return 0; }
