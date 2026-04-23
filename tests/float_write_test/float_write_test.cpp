// Float serialization integration test.
//
// Exercises both glz::to_chars variants (OptSize=false default, OptSize=true)
// for both float and double, and verifies on every input:
//   - Round-trip: v -> glz::to_chars -> glz::from_chars (fast_float) -> v
//                exact bit equality, including sign-of-zero preservation.
//   - Cross-variant byte-identical output (OptSize=false == OptSize=true).
//   - JSON-envelope round-trip: v -> glz::write -> glz::read_json -> v,
//                tested for the default opts and for opts_size.
//   - Non-finite inputs (NaN, +/-Inf) emit "null" on both variants.

#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <random>
#include <string>

#include "glaze/glaze.hpp"
#include "glaze/util/zmij.hpp"

static int failures = 0;
constexpr int max_logged_failures = 10;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

template <std::floating_point T, bool OptSize>
static std::string to_chars_str(T v)
{
   char buf[64];
   char* end = glz::to_chars<T, OptSize>(buf, v);
   return std::string(buf, size_t(end - buf));
}

template <std::floating_point T>
static bool bit_equal(T a, T b) noexcept
{
   using Raw = std::conditional_t<std::is_same_v<T, float>, uint32_t, uint64_t>;
   return std::bit_cast<Raw>(a) == std::bit_cast<Raw>(b);
}

// ---------------------------------------------------------------------------
// Per-value checks
// ---------------------------------------------------------------------------

template <std::floating_point T>
static void expect_null(T v, const char* label)
{
   const auto s_fast = to_chars_str<T, false>(v);
   const auto s_opt = to_chars_str<T, true>(v);
   if (s_fast != "null" || s_opt != "null") {
      std::printf("FAIL null %s: fast='%s', size='%s'\n", label, s_fast.c_str(), s_opt.c_str());
      ++failures;
   }
}

// Core round-trip check for finite values. Runs:
//   1. to_chars<T, false> and <T, true>: outputs must match byte-for-byte.
//   2. Parse the output with fast_float: result must bit-equal the input.
template <std::floating_point T>
static void expect_roundtrip(T v, const char* label)
{
   const auto s_fast = to_chars_str<T, false>(v);
   const auto s_opt = to_chars_str<T, true>(v);

   if (s_fast != s_opt) {
      if (failures < max_logged_failures) {
         std::printf("FAIL cross-variant %s (%.17g): fast='%s', size='%s'\n", label, double(v), s_fast.c_str(),
                     s_opt.c_str());
      }
      ++failures;
      return;
   }

   T parsed{};
   auto [ptr, ec] = glz::from_chars<false>(s_fast.data(), s_fast.data() + s_fast.size(), parsed);
   if (ec != std::errc{}) {
      if (failures < max_logged_failures) {
         std::printf("FAIL parse %s (%.17g): '%s'\n", label, double(v), s_fast.c_str());
      }
      ++failures;
      return;
   }

   if (!bit_equal(parsed, v)) {
      if (failures < max_logged_failures) {
         std::printf("FAIL roundtrip %s: in=%.17g out=%.17g via '%s'\n", label, double(v), double(parsed),
                     s_fast.c_str());
      }
      ++failures;
   }
}

// JSON envelope roundtrip: v -> glz::write -> glz::read_json -> v.
// Default opts (OptSize=false via write_chars) and opts_size (OptSize=true).
template <std::floating_point T>
static void expect_json_roundtrip(T v, const char* label)
{
   {
      std::string out;
      if (glz::write_json(v, out)) {
         if (failures < max_logged_failures) std::printf("FAIL glz::write_json %s (%.17g)\n", label, double(v));
         ++failures;
         return;
      }
      T parsed{};
      if (glz::read_json(parsed, out)) {
         if (failures < max_logged_failures)
            std::printf("FAIL glz::read_json (default) %s: '%s'\n", label, out.c_str());
         ++failures;
         return;
      }
      if (!bit_equal(parsed, v)) {
         if (failures < max_logged_failures)
            std::printf("FAIL json roundtrip (default) %s: in=%.17g out=%.17g via '%s'\n", label, double(v),
                        double(parsed), out.c_str());
         ++failures;
      }
   }
   {
      std::string out;
      if (glz::write<glz::opts_size{}>(v, out)) {
         if (failures < max_logged_failures)
            std::printf("FAIL glz::write<opts_size> %s (%.17g)\n", label, double(v));
         ++failures;
         return;
      }
      T parsed{};
      if (glz::read_json(parsed, out)) {
         if (failures < max_logged_failures)
            std::printf("FAIL glz::read_json (opts_size) %s: '%s'\n", label, out.c_str());
         ++failures;
         return;
      }
      if (!bit_equal(parsed, v)) {
         if (failures < max_logged_failures)
            std::printf("FAIL json roundtrip (opts_size) %s: in=%.17g out=%.17g via '%s'\n", label, double(v),
                        double(parsed), out.c_str());
         ++failures;
      }
   }
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

int main()
{
   using std::numeric_limits;

   // --- Non-finite --------------------------------------------------------
   expect_null(numeric_limits<double>::quiet_NaN(), "qnan d");
   expect_null(numeric_limits<double>::signaling_NaN(), "snan d");
   expect_null(numeric_limits<double>::infinity(), "+inf d");
   expect_null(-numeric_limits<double>::infinity(), "-inf d");
   expect_null(numeric_limits<float>::quiet_NaN(), "qnan f");
   expect_null(numeric_limits<float>::signaling_NaN(), "snan f");
   expect_null(numeric_limits<float>::infinity(), "+inf f");
   expect_null(-numeric_limits<float>::infinity(), "-inf f");

   // --- Double special values --------------------------------------------
   const double special_doubles[] = {
      0.0,
      -0.0,
      1.0,
      -1.0,
      0.5,
      1.5,
      100.0,
      -100.0,
      numeric_limits<double>::min(),
      -numeric_limits<double>::min(),
      numeric_limits<double>::max(),
      -numeric_limits<double>::max(),
      numeric_limits<double>::denorm_min(),
      -numeric_limits<double>::denorm_min(),
      std::nextafter(1.0, 2.0),
      std::nextafter(1.0, 0.0),
      // Values in the dragonbox-vs-zmij fixed/scientific boundary band
      1e16, 1e17, 1e18, 1e19, 1e20, 1e21,
      1e-7, 1e-8, 1e-9,
      1e100, 1e-100, 1e300, 1e-300,
      3.141592653589793,
      2.718281828459045,
      0.1,
      0.2,
      0.3,
   };
   for (double v : special_doubles) {
      char lbl[40];
      std::snprintf(lbl, sizeof(lbl), "special d=%.17g", v);
      expect_roundtrip(v, lbl);
      expect_json_roundtrip(v, lbl);
   }

   // --- Float special values ---------------------------------------------
   const float special_floats[] = {
      0.0f,
      -0.0f,
      1.0f,
      -1.0f,
      0.5f,
      1.5f,
      numeric_limits<float>::min(),
      -numeric_limits<float>::min(),
      numeric_limits<float>::max(),
      -numeric_limits<float>::max(),
      numeric_limits<float>::denorm_min(),
      -numeric_limits<float>::denorm_min(),
      std::nextafterf(1.0f, 2.0f),
      std::nextafterf(1.0f, 0.0f),
      1e-7f, 1e-8f, 1e-9f,
      1e10f, 1e20f, 1e30f,
      1e-10f, 1e-20f, 1e-30f,
      3.14159265f,
      0.1f,
      0.2f,
      0.3f,
   };
   for (float v : special_floats) {
      char lbl[40];
      std::snprintf(lbl, sizeof(lbl), "special f=%.9g", double(v));
      expect_roundtrip(v, lbl);
      expect_json_roundtrip(v, lbl);
   }

   // --- Random fuzz: doubles ---------------------------------------------
   std::mt19937_64 rng(0xc0ffee'5eedULL);
   constexpr int N_DOUBLE = 100000;
   int d_tested = 0;
   {
      std::uniform_int_distribution<uint64_t> dist;
      for (int i = 0; i < N_DOUBLE; ++i) {
         double v = std::bit_cast<double>(dist(rng));
         if (std::isnan(v) || std::isinf(v)) continue;
         expect_roundtrip(v, "rand(double)");
         ++d_tested;
         if (failures > max_logged_failures) {
            std::puts("too many failures; stopping");
            return 1;
         }
      }
   }
   std::printf("round-tripped %d random finite doubles (glz::to_chars)\n", d_tested);

   // --- Random fuzz: floats ----------------------------------------------
   constexpr int N_FLOAT = 100000;
   int f_tested = 0;
   {
      std::uniform_int_distribution<uint32_t> dist;
      for (int i = 0; i < N_FLOAT; ++i) {
         float v = std::bit_cast<float>(dist(rng));
         if (std::isnan(v) || std::isinf(v)) continue;
         expect_roundtrip(v, "rand(float)");
         ++f_tested;
         if (failures > max_logged_failures) {
            std::puts("too many failures; stopping");
            return 1;
         }
      }
   }
   std::printf("round-tripped %d random finite floats (glz::to_chars)\n", f_tested);

   // --- JSON-envelope fuzz (smaller sample — covers the full write+read) -
   {
      std::uniform_int_distribution<uint64_t> dist;
      for (int i = 0; i < 10000; ++i) {
         double v = std::bit_cast<double>(dist(rng));
         if (std::isnan(v) || std::isinf(v)) continue;
         expect_json_roundtrip(v, "rand(double,json)");
         if (failures > max_logged_failures) return 1;
      }
   }
   {
      std::uniform_int_distribution<uint32_t> dist;
      for (int i = 0; i < 10000; ++i) {
         float v = std::bit_cast<float>(dist(rng));
         if (std::isnan(v) || std::isinf(v)) continue;
         expect_json_roundtrip(v, "rand(float,json)");
         if (failures > max_logged_failures) return 1;
      }
   }

   if (failures == 0) {
      std::puts("float_write_test: OK");
      return 0;
   }
   std::printf("%d failure(s)\n", failures);
   return 1;
}
