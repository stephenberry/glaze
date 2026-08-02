// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <string_view>

#include "glaze/simd/simd.hpp"
#include "glaze/simd/utf8_validation.hpp"
#include "glaze/util/zmij.hpp"

namespace glz
{
   // Which SIMD path each of Glaze's accelerated subsystems compiled to.
   //
   // Selection happens entirely in the preprocessor -- Glaze has no runtime dispatch -- so these
   // describe the translation unit, not the host. A build reporting "AVX2" runs AVX2 on a machine
   // that also supports AVX-512, and crashes on one that supports neither. Compiling different
   // files with different -march flags gives each of them its own values; see simd_info below for
   // what that means when they are linked together.
   //
   // Four fields rather than one name because they genuinely disagree; docs/optimizing-performance.md
   // has the cases and why they arise.
   struct simd_backends
   {
      // Widest instruction set the detection in simd.hpp enabled. One of "AVX512BW", "AVX2",
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

   namespace detail
   {
      // Mirrors the macros defined in simd.hpp, widest first.
      consteval std::string_view detected_simd() noexcept
      {
#if defined(GLZ_USE_AVX512BW)
         return "AVX512BW";
#elif defined(GLZ_USE_AVX2)
         return "AVX2";
#elif defined(GLZ_USE_SSSE3)
         return "SSSE3";
#elif defined(GLZ_USE_SSE2)
         return "SSE2";
#elif defined(GLZ_USE_NEON64)
         return "NEON64";
#elif defined(GLZ_USE_NEON)
         return "NEON";
#elif defined(GLZ_USE_WASM_SIMD128)
         return "WASM_SIMD128";
#else
         // GLZ_DISABLE_SIMD, or a target the detection does not cover. Glaze still uses SWAR
         // everywhere, which needs no intrinsics, so this does not mean "no acceleration".
         return "scalar";
#endif
      }

      // Mirrors the escape cascade in json/write.hpp, whose branches static_assert against the
      // result. Those asserts key off the same macros as this chain, so what they catch is an edit
      // here that the cascade did not make: rename a branch and every translation unit that writes
      // JSON stops compiling.
      //
      // They cannot catch the reverse. A helper added to the cascade under a macro this chain does
      // not test -- an AVX-512 or WASM escape helper, the two Glaze lacks -- compiles cleanly and
      // leaves this reporting a name that is too narrow. Adding one means adding a branch here in
      // the same commit; the cascade says so at the point of change.
      consteval std::string_view string_escape_simd() noexcept
      {
#if defined(GLZ_USE_AVX2)
         return "AVX2";
#elif defined(GLZ_USE_SSE2)
         return "SSE2";
#elif defined(GLZ_USE_NEON)
         return "NEON";
#else
         return "SWAR";
#endif
      }

      // Mirrors zmij's own ZMIJ_USE_* selection. These are defined to 0 or 1 rather than
      // defined/undefined, so they are tested by value.
      consteval std::string_view float_write_simd() noexcept
      {
#if ZMIJ_USE_NEON
         return "NEON";
#elif ZMIJ_USE_SSE4_1
         return "SSE4.1";
#elif ZMIJ_USE_SSE
         return "SSE2";
#else
         return "scalar";
#endif
      }
   }

   // Report what this build compiled. Reflectable, so a benchmark harness can emit it directly:
   //
   //    std::string s;
   //    std::ignore = glz::write_json(glz::simd_info, s);
   //    // {"detected":"AVX512BW","utf8_validation":"AVX512BW",
   //    //  "string_escape":"AVX2","float_write":"SSE4.1"}
   //
   // Deliberately not `inline`. The values come from the preprocessor, so every translation unit
   // must keep its own answer, which internal linkage gives it. Under `inline` the copies merge
   // and a project compiling some files with -mavx2 and others without reports the survivor's
   // answer for all of them, including through the write_json call above.
   //
   // These spellings are public API. Renaming one breaks every comparison against it, and inserting
   // a wider entry changes what an already-working build reports, so treat both as deliberate.
   constexpr simd_backends simd_info{
      .detected = detail::detected_simd(),
      .utf8_validation = detail::utf8_simd::backend,
      .string_escape = detail::string_escape_simd(),
      .float_write = detail::float_write_simd(),
   };
}
