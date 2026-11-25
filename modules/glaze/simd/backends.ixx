// Glaze Library
// For the license information refer to glaze.ixx
// glz:header path="glaze/simd/backends.hpp"
// glz:header std=<string_view>

module;

#include "glaze/simd/simd_config.hpp"

export module glaze.simd.backends;

import std;

import glaze.util.zmij;

namespace glz
{
   namespace detail::simd
   {
#if defined(GLZ_USE_AVX512BW)
      inline constexpr std::string_view detected_backend = "AVX512BW";
#elif defined(GLZ_USE_AVX2)
      inline constexpr std::string_view detected_backend = "AVX2";
#elif defined(GLZ_USE_SSSE3)
      inline constexpr std::string_view detected_backend = "SSSE3";
#elif defined(GLZ_USE_SSE2)
      inline constexpr std::string_view detected_backend = "SSE2";
#elif defined(GLZ_USE_NEON64)
      inline constexpr std::string_view detected_backend = "NEON64";
#elif defined(GLZ_USE_NEON)
      inline constexpr std::string_view detected_backend = "NEON";
#elif defined(GLZ_USE_WASM_SIMD128)
      inline constexpr std::string_view detected_backend = "WASM_SIMD128";
#else
      inline constexpr std::string_view detected_backend = "scalar";
#endif

#if defined(GLZ_USE_AVX2)
      inline constexpr std::string_view string_escape_backend = "AVX2";
#elif defined(GLZ_USE_SSE2)
      inline constexpr std::string_view string_escape_backend = "SSE2";
#elif defined(GLZ_USE_NEON)
      inline constexpr std::string_view string_escape_backend = "NEON";
#else
      inline constexpr std::string_view string_escape_backend = "SWAR";
#endif

      // Mirror utf8_validation.hpp's selection without pulling its intrinsic implementation into
      // this module's BMI. The UTF-8 tests keep these public backend names aligned with execution.
#if defined(GLZ_UTF8_GENERIC_WIDTH) && !defined(GLZ_DISABLE_SIMD)
      static_assert(GLZ_UTF8_GENERIC_WIDTH == 16 || GLZ_UTF8_GENERIC_WIDTH == 32 || GLZ_UTF8_GENERIC_WIDTH == 64,
                    "generic UTF-8 width must be 16, 32, or 64");
      inline constexpr std::string_view utf8_validation_backend =
         GLZ_UTF8_GENERIC_WIDTH == 64 ? "generic64" : (GLZ_UTF8_GENERIC_WIDTH == 32 ? "generic32" : "generic16");
#elif defined(GLZ_USE_AVX512BW)
      inline constexpr std::string_view utf8_validation_backend = "AVX512BW";
#elif defined(GLZ_USE_AVX2)
      inline constexpr std::string_view utf8_validation_backend = "AVX2";
#elif defined(GLZ_USE_SSSE3)
      inline constexpr std::string_view utf8_validation_backend = "SSSE3";
#elif defined(GLZ_USE_NEON64)
      inline constexpr std::string_view utf8_validation_backend = "NEON64";
#elif defined(GLZ_USE_WASM_SIMD128)
      inline constexpr std::string_view utf8_validation_backend = "WASM_SIMD128";
#else
      inline constexpr std::string_view utf8_validation_backend = "scalar";
#endif
   }

   // Which SIMD path each of Glaze's accelerated subsystems compiled to.
   //
   // Selection happens entirely in the preprocessor -- Glaze has no runtime dispatch -- so these
   // describe the module BMI, not the host. A build reporting "AVX2" runs AVX2 on a machine that
   // also supports AVX-512, and crashes on one that supports neither. CMake supplies compatible
   // synthetic BMIs when consumer targets compile the module with different options.
   //
   // Four fields rather than one name because they genuinely disagree; docs/optimizing-performance.md
   // has the cases and why they arise.
   export struct simd_backends
   {
      // Widest instruction set the detection in simd_config.hpp enabled. One of "AVX512BW", "AVX2",
      // "SSSE3", "SSE2", "NEON64", "NEON", "WASM_SIMD128", or "scalar".
      //
      // An upper bound on string_escape, but not on the other two: float_write runs its own
      // detection, and GLZ_UTF8_GENERIC_WIDTH overrides utf8_validation outright.
      std::string_view detected{};

      // UTF-8 validation. Needs a byte-granular shuffle, which plain SSE2 and 32 bit NEON lack, so
      // those targets validate with the scalar validator in parse.hpp while the rest of Glaze stays
      // vectorized. "generic16" / "generic32" / "generic64" mean GLZ_UTF8_GENERIC_WIDTH selected the
      // portable width-generic validator, a testing hook no ordinary build uses.
      std::string_view utf8_validation{};

      // JSON string escaping. A cascade rather than a single path: the widest helper compiled takes
      // whole registers, narrower ones take the remainder, and SWAR finishes the tail. This names
      // the widest. Glaze has no AVX-512 or WASM escape helper, so an AVX-512 build reports "AVX2"
      // here and a wasm build reports "SWAR".
      std::string_view string_escape{};

      // Float writing, through the zmij port in util/zmij.hpp. It runs its own detection off
      // __SSE2__ / __ARM_NEON rather than Glaze's GLZ_USE_* macros, and only honours
      // GLZ_DISABLE_SIMD from Glaze. `detected` does not even bound it from above: a 32 bit x86
      // build with SSE2 reports detected == "scalar" and float_write == "SSE2", because Glaze's own
      // detection requires __x86_64__ and zmij's does not.
      //
      // Names zmij's path only. Setting opts::float_format routes floats through std::format
      // instead (core/write_chars.hpp), which this field does not describe.
      std::string_view float_write{};
   };

   // Report what this build compiled. Reflectable, so a benchmark harness can emit it directly:
   //
   //    std::string s;
   //    std::ignore = glz::write_json(glz::simd_info, s);
   //    // {"detected":"AVX512BW","utf8_validation":"AVX512BW",
   //    //  "string_escape":"AVX2","float_write":"SSE4.1"}
   //
   // The inline value comes from the compile options used for this module's BMI. CMake creates
   // compatible synthetic BMIs when consumers use different options, so each compatible consumer
   // graph gets the answer selected by its own flags without depending on the provider's object.
   //
   // These spellings are public API. Renaming one breaks every comparison against it, and inserting
   // a wider entry changes what an already-working build reports, so treat both as deliberate.
   export inline constexpr simd_backends simd_info{
      .detected = detail::simd::detected_backend,
      .utf8_validation = detail::simd::utf8_validation_backend,
      .string_escape = detail::simd::string_escape_backend,
      .float_write = zmij::detail::float_write_backend(),
   };
}
