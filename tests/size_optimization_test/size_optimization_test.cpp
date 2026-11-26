// Glaze Library
// For the license information refer to glaze.hpp

// Verifies the build-wide default optimization level seam.
//
// CMake compiles this file twice: once normally (default optimization level = normal)
// and once with GLZ_DEFAULT_OPTIMIZATION_SIZE defined (default = size). The static_asserts
// below check the correct wiring for whichever mode this translation unit is built in, so a
// regression in the consteval fallbacks (check_optimization_level / check_linear_search /
// is_size_optimized) fails the build instead of silently re-bloating embedded binaries.

#include <bit>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "glaze/glaze.hpp"

namespace
{
   using glz::check_linear_search;
   using glz::check_optimization_level;
   using glz::default_optimization_level;
   using glz::is_size_optimized;
   using glz::optimization_level;
   using glz::opts;
   using glz::opts_size;

   // The exported default constant must agree with the configured build mode.
#ifdef GLZ_DEFAULT_OPTIMIZATION_SIZE
   static_assert(default_optimization_level == optimization_level::size);
#else
   static_assert(default_optimization_level == optimization_level::normal);
#endif

   // A bare opts{} (no optimization_level member) inherits the build-wide default for
   // integers/floats (is_size_optimized) AND hash-table elimination (check_linear_search).
#ifdef GLZ_DEFAULT_OPTIMIZATION_SIZE
   static_assert(is_size_optimized(opts{}));
   static_assert(check_linear_search(opts{}));
   static_assert(check_optimization_level(opts{}) == optimization_level::size);
#else
   static_assert(not is_size_optimized(opts{}));
   static_assert(not check_linear_search(opts{}));
   static_assert(check_optimization_level(opts{}) == optimization_level::normal);
#endif

   // opts_size{} is always size-optimized regardless of the build default.
   static_assert(is_size_optimized(opts_size{}));
   static_assert(check_linear_search(opts_size{}));

   // Escape hatch: an explicit optimization_level::normal always wins, even when the build
   // default is size. This keeps `normal` honest and greppable.
   struct force_normal_opts : opts
   {
      // Type fully qualified (matches glz::opts_size) so GCC's -Wchanges-meaning does not
      // fire on a member named the same as its unqualified type.
      glz::optimization_level optimization_level = glz::optimization_level::normal;
   };
   static_assert(not is_size_optimized(force_normal_opts{}));
   static_assert(check_optimization_level(force_normal_opts{}) == optimization_level::normal);

   // An explicit linear_search member still wins over the optimization-level-derived default.
   struct force_hash_opts : opts
   {
      bool linear_search = false;
      glz::optimization_level optimization_level = glz::optimization_level::size;
   };
   static_assert(not check_linear_search(force_hash_opts{}));

   // The size-optimized reader parses floats with simple_float::from_chars, while the normal
   // reader uses fast_float. Two parsers can drift apart silently: opts_size once accepted
   // 1e309 as inf while the normal reader rejected it. These inputs sit where parsers tend to
   // disagree (range limits, rounding halfway points, truncated digits, malformed syntax), and
   // both readers must give the same error code and, on success, the same bits.
   constexpr std::string_view float_parity_inputs[] = {
      // zero, including exponents far outside the range
      "0",
      "-0",
      "0.0",
      "0e999999",
      "-0e-999999",
      // the top of double: max, rounding down to max, rounding up to inf, overflow
      "1.7976931348623157e308",
      "1.7976931348623158e308",
      "1.7976931348623159e308",
      "1e309",
      "-1e309",
      "1e999999",
      // the bottom of double: smallest normal, subnormals, either side of the halfway point to zero
      "2.2250738585072014e-308",
      "2.2250738585072011e-308",
      "4.9406564584124654e-324",
      "5e-324",
      "2.4703282292062328e-324",
      "2.4703282292062327e-324",
      "2e-324",
      "1e-324",
      "-1e-324",
      "1e-999999",
      // the edges of float, which differ from double only for the float reads
      "3.4028235e38",
      "3.4028236e38",
      "1e39",
      "1.4e-45",
      "7.1e-46",
      "7e-46",
      "1e-46",
      // more significant digits than a 64-bit mantissa holds
      "9007199254740993",
      "0.1000000000000000055511151231257827021181583404541015625",
      "1000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000e300",
      "0.000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001e-300",
      // malformed: both must reject
      "",
      "-",
      "+1",
      "1.",
      ".1",
      "01",
      "1e",
      "1e+",
      "--1",
      "1.0.0",
      "inf",
      "nan",
      "0x1p3",
   };

   template <class T>
   bool same_read_result(const std::string_view input)
   {
      using bits_type = std::conditional_t<sizeof(T) == 4, uint32_t, uint64_t>;
      T normal{};
      T size{};
      const auto normal_ec = glz::read<force_normal_opts{}>(normal, input);
      const auto size_ec = glz::read<opts_size{}>(size, input);
      if (normal_ec.ec != size_ec.ec ||
          (not normal_ec && std::bit_cast<bits_type>(normal) != std::bit_cast<bits_type>(size))) {
         std::printf("%s '%.*s': normal reader ec=%d value=%.17g, opts_size ec=%d value=%.17g\n",
                     sizeof(T) == 4 ? "float" : "double", int(input.size()), input.data(), int(normal_ec.ec),
                     double(normal), int(size_ec.ec), double(size));
         return false;
      }
      return true;
   }
}

int main()
{
   // Runtime round-trip sanity in whichever mode this TU is compiled: exercise the integer
   // and float write/read paths that the optimization level switches between.
   int rc = 0;

   {
      std::vector<int64_t> v{0, 1, -42, 1234567890123LL};
      std::string buffer{};
      if (glz::write_json(v, buffer)) {
         rc = 1;
      }
      std::vector<int64_t> round{};
      if (glz::read_json(round, buffer) || round != v) {
         rc = 1;
      }
   }

   {
      double d = 3.141592653589793;
      std::string buffer{};
      if (glz::write_json(d, buffer)) {
         rc = 1;
      }
      double round{};
      if (glz::read_json(round, buffer) || round != d) {
         rc = 1;
      }
   }

   // Exercise the error-formatting path (previously pulled in the 40 KB integer table).
   {
      int value{};
      const std::string_view bad = "not-a-number";
      auto ec = glz::read_json(value, bad);
      if (ec) {
         const std::string msg = glz::format_error(ec, bad);
         if (msg.empty()) {
            rc = 1;
         }
      }
      else {
         rc = 1; // expected a parse error
      }
   }

   {
      // Outside the range of double: an error from every reader, never a saturated value.
      for (const std::string_view input : {"1e309", "-1e309", "1e-324"}) {
         double d{};
         if (not glz::read_json(d, input)) {
            std::printf("%.*s was accepted as %g\n", int(input.size()), input.data(), d);
            rc = 1;
         }
      }
   }

   for (const std::string_view input : float_parity_inputs) {
      // Both checks run so a failure report lists every mismatch, not just the first per input.
      const bool double_matches = same_read_result<double>(input);
      const bool float_matches = same_read_result<float>(input);
      if (not double_matches || not float_matches) {
         rc = 1;
      }
   }

   if (rc == 0) {
      std::printf("size_optimization_test: all checks passed\n");
   }
   return rc;
}
