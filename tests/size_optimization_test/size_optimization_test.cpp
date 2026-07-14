// Glaze Library
// For the license information refer to glaze.hpp

// Verifies the build-wide default optimization level seam.
//
// CMake compiles this file twice: once normally (default optimization level = normal)
// and once with GLZ_DEFAULT_OPTIMIZATION_SIZE defined (default = size). The static_asserts
// below check the correct wiring for whichever mode this translation unit is built in, so a
// regression in the consteval fallbacks (check_optimization_level / check_linear_search /
// is_size_optimized) fails the build instead of silently re-bloating embedded binaries.

#include <cstdint>
#include <cstdio>
#include <string>
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

   if (rc == 0) {
      std::printf("size_optimization_test: all checks passed\n");
   }
   return rc;
}
