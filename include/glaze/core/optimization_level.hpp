// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <cstdint>

namespace glz
{
   // ============================================================================
   // Optimization Level
   // ============================================================================
   //
   // Controls the trade-off between binary size and runtime performance.
   //
   // - Embedded systems: Use `size` to minimize binary footprint
   // - General applications: Use `normal` for maximum performance (default)
   //
   // ============================================================================

   enum struct optimization_level : uint8_t {
      // -------------------------------------------------------------------------
      // SIZE (Level 0)
      // -------------------------------------------------------------------------
      // Priority: Minimize binary size for embedded systems
      //
      // Current behavior:
      //   - Integer serialization: glz::to_chars (400B lookup tables)
      //   - Float serialization: glz::to_chars<T, OptSize=true> (zmij w/ recomputed
      //     pow-10, ~1.4 KB of __const)
      //   - Float parsing: glz::simple_float::from_chars (no fast_float tables)
      //   - Key matching: linear_search=true by default (no hash tables)
      //
      // Binary size savings (approximate):
      //   - ~39KB from using smaller integer tables (400B vs 40KB)
      //   - ~15.8KB from dropping zmij's pow-10 tables (OptSize=true)
      //   - Variable savings from hash table elimination
      //
      size = 0,

      // -------------------------------------------------------------------------
      // NORMAL (Level 1) - DEFAULT
      // -------------------------------------------------------------------------
      // Priority: Maximum performance (default)
      //
      // Current behavior:
      //   - Integer serialization: glz::to_chars_40kb (40KB digit_quads table)
      //   - Float serialization: glz::to_chars<T, OptSize=false> (zmij w/ full
      //     ~17KB pow-10 tables for peak throughput)
      //   - Key matching: hash-based lookup
      //
      // This is the default level optimized for speed.
      //
      normal = 1
   };

   // ============================================================================
   // Build-wide default optimization level
   // ============================================================================
   //
   // `default_optimization_level` is the level used whenever an options type does
   // not carry an explicit `optimization_level` member (e.g. a plain `glz::opts{}`).
   // It is normally `normal` (maximum performance).
   //
   // Define `GLZ_DEFAULT_OPTIMIZATION_SIZE` (most easily via the
   // `glaze_DEFAULT_OPTIMIZATION_SIZE` CMake option) to make `size` the build-wide
   // default. Every call site that does not specify a level then drops the 40 KB
   // integer table, the ~16 KB float pow-10 tables, and per-struct hash tables,
   // without having to remember to pass `glz::opts_size{}` everywhere. A single
   // forgotten `glz::write(obj, buf)` can no longer silently relink the large tables.
   //
   // For full control, `GLZ_DEFAULT_OPTIMIZATION_LEVEL` may instead be defined
   // directly to any `glz::optimization_level` value.
   //
   // This is a build-wide setting and must be defined uniformly across every
   // translation unit in a build (the CMake option injects it as an INTERFACE
   // compile definition for exactly this reason). Per-call options always win:
   // an explicit `optimization_level::normal` keeps the fast tables even here.
   //
   // The selection is value-based (a single exported constant feeding the existing
   // `if constexpr (is_size_optimized(Opts))` machinery), never a preprocessor
   // branch inside serialization code, so it composes cleanly with C++20 modules:
   // the value is baked once when the module is built and consumed everywhere by name.
   // ============================================================================

#ifndef GLZ_DEFAULT_OPTIMIZATION_LEVEL
#if defined(GLZ_DEFAULT_OPTIMIZATION_SIZE)
#define GLZ_DEFAULT_OPTIMIZATION_LEVEL ::glz::optimization_level::size
#else
#define GLZ_DEFAULT_OPTIMIZATION_LEVEL ::glz::optimization_level::normal
#endif
#endif

   inline constexpr optimization_level default_optimization_level = GLZ_DEFAULT_OPTIMIZATION_LEVEL;

} // namespace glz
