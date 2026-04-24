// Glaze Library
// For the license information refer to glaze.hpp
//
// Header-only port of Victor Zverovich's zmij library for float serialization.
//  https://github.com/vitaut/zmij
// Distributed under the MIT license (see LICENSE-zmij) or alternatively
// the Boost Software License, Version 1.0.
// Copyright (c) 2025 - present, Victor Zverovich
//
// Adaptation notes:
//  - Ported from zmij.h + zmij.cc into a single header wrapped in glz::zmij.
//  - The upstream ZMIJ_OPTIMIZE_SIZE compile-time macro is replaced by a
//    `bool OptSize` template parameter so the two variants (full tables vs
//    recompute-on-the-fly) can coexist in one translation unit. Glaze's
//    `is_size_optimized(Opts)` flag drives it at the call site.
//  - Exposes glz::to_chars, a JSON-compliant wrapper that writes "null" for
//    NaN/Inf, uppercase 'E' for the exponent, and omits the '+' on positive
//    exponents (matching Glaze's established output format).
//  - Internal helpers live in glz::zmij::detail_impl (formerly an anonymous
//    namespace in zmij.cc); namespace-level constants that previously collided
//    with struct members (neg100/neg10/neg10k/zeros) are suffixed `_v`.

#pragma once

#include <concepts>

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <bit>
#include <cstring>
#include <limits>
#include <type_traits>

#ifndef ZMIJ_USE_SIMD
#  define ZMIJ_USE_SIMD 1
#endif

// Non-finite output format for zmij::detail::write. 0 (default) emits "null"
// to stay JSON-compliant; =1 restores upstream zmij behavior ("inf" / "nan").
#ifndef GLZ_ZMIJ_EMIT_INF_NAN
#  define GLZ_ZMIJ_EMIT_INF_NAN 0
#endif

#ifdef ZMIJ_USE_NEON
#elif defined(__ARM_NEON) || defined(_M_ARM64)
#  define ZMIJ_USE_NEON ZMIJ_USE_SIMD
#else
#  define ZMIJ_USE_NEON 0
#endif
#if ZMIJ_USE_NEON
#  include <arm_neon.h>
#endif

#ifdef ZMIJ_USE_SSE
#elif defined(__SSE2__)
#  define ZMIJ_USE_SSE ZMIJ_USE_SIMD
#elif defined(_M_AMD64) || (defined(_M_IX86_FP) && _M_IX86_FP == 2)
#  define ZMIJ_USE_SSE ZMIJ_USE_SIMD
#else
#  define ZMIJ_USE_SSE 0
#endif
#if ZMIJ_USE_SSE
#  include <immintrin.h>
#endif

#ifdef ZMIJ_USE_SSE4_1
static_assert(!ZMIJ_USE_SSE4_1 || ZMIJ_USE_SSE);
#elif defined(__SSE4_1__) || defined(__AVX__)
#  define ZMIJ_USE_SSE4_1 ZMIJ_USE_SSE
#else
#  define ZMIJ_USE_SSE4_1 0
#endif

#ifdef __aarch64__
#  define ZMIJ_AARCH64 1
#else
#  define ZMIJ_AARCH64 0
#endif

#ifdef __x86_64__
#  define ZMIJ_X86_64 1
#else
#  define ZMIJ_X86_64 0
#endif

#ifdef __clang__
#  define ZMIJ_CLANG 1
#else
#  define ZMIJ_CLANG 0
#endif

#ifdef _MSC_VER
#  define ZMIJ_MSC_VER _MSC_VER
#  include <intrin.h>
#else
#  define ZMIJ_MSC_VER 0
#endif

#if defined(__has_builtin) && !defined(ZMIJ_NO_BUILTINS)
#  define ZMIJ_HAS_BUILTIN(x) __has_builtin(x)
#else
#  define ZMIJ_HAS_BUILTIN(x) 0
#endif
#ifdef __has_attribute
#  define ZMIJ_HAS_ATTRIBUTE(x) __has_attribute(x)
#else
#  define ZMIJ_HAS_ATTRIBUTE(x) 0
#endif
#ifdef __has_cpp_attribute
#  define ZMIJ_HAS_CPP_ATTRIBUTE(x) __has_cpp_attribute(x)
#else
#  define ZMIJ_HAS_CPP_ATTRIBUTE(x) 0
#endif

#if ZMIJ_HAS_CPP_ATTRIBUTE(likely) && ZMIJ_HAS_CPP_ATTRIBUTE(unlikely)
#  define ZMIJ_LIKELY likely
#  define ZMIJ_UNLIKELY unlikely
#else
#  define ZMIJ_LIKELY
#  define ZMIJ_UNLIKELY
#endif

#if ZMIJ_HAS_CPP_ATTRIBUTE(maybe_unused)
#  define ZMIJ_MAYBE_UNUSED maybe_unused
#else
#  define ZMIJ_MAYBE_UNUSED
#endif

// Size-vs-speed is controlled per call via the `OptSize` template parameter
// on glz::zmij::to_chars / detail::write / to_decimal — no global macro.

#if ZMIJ_HAS_ATTRIBUTE(always_inline)
#  define ZMIJ_INLINE __attribute__((always_inline)) inline
#elif ZMIJ_MSC_VER
#  define ZMIJ_INLINE __forceinline
#else
#  define ZMIJ_INLINE inline
#endif

#ifdef __GNUC__
#  define ZMIJ_ASM(x) asm x
#else
#  define ZMIJ_ASM(x)
#endif

#ifdef ZMIJ_CONST_DECL
#elif ZMIJ_AARCH64
#  define ZMIJ_CONST_DECL
#else
#  define ZMIJ_CONST_DECL static constexpr
#endif

#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wsign-compare"
#  pragma clang diagnostic ignored "-Wunused-parameter"
#  pragma clang diagnostic ignored "-Wpedantic"
#elif defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wsign-compare"
#  pragma GCC diagnostic ignored "-Wunused-parameter"
#  pragma GCC diagnostic ignored "-Wpedantic"
#endif

namespace glz::zmij
{
   struct dec_fp
   {
      long long sig;
      int exp;
      bool negative;
   };

   enum { non_finite_exp = int(~0u >> 1) };
   enum { float_buffer_size = 17, double_buffer_size = 34 };

   namespace detail_impl
   {

      inline constexpr bool is_big_endian = std::endian::native == std::endian::big;

      inline auto bswap64(uint64_t x) noexcept -> uint64_t
      {
#if ZMIJ_HAS_BUILTIN(__builtin_bswap64)
         return __builtin_bswap64(x);
#elif ZMIJ_MSC_VER
         return _byteswap_uint64(x);
#else
         return ((x & 0xff00000000000000) >> 56) | ((x & 0x00ff000000000000) >> 40) |
                ((x & 0x0000ff0000000000) >> 24) | ((x & 0x000000ff00000000) >> +8) |
                ((x & 0x00000000ff000000) << +8) | ((x & 0x0000000000ff0000) << 24) |
                ((x & 0x000000000000ff00) << 40) | ((x & 0x00000000000000ff) << 56);
#endif
      }

      inline auto clz(uint64_t x) noexcept -> int
      {
         assert(x != 0);
#if ZMIJ_HAS_BUILTIN(__builtin_clzll)
         return __builtin_clzll(x);
#elif defined(_M_AMD64) && defined(__AVX2__)
         return __lzcnt64(x);
#elif defined(_M_AMD64) || defined(_M_ARM64)
         unsigned long idx;
         _BitScanReverse64(&idx, x);
         return 63 - idx;
#elif ZMIJ_MSC_VER
         unsigned long idx;
         if (_BitScanReverse(&idx, uint32_t(x >> 32))) return 31 - idx;
         _BitScanReverse(&idx, uint32_t(x));
         return 63 - idx;
#else
         int n = 64;
         for (; x > 0; x >>= 1) --n;
         return n;
#endif
      }

      inline auto ctz(uint64_t x) noexcept -> int
      {
         assert(x != 0);
#if ZMIJ_HAS_BUILTIN(__builtin_ctzll)
         return __builtin_ctzll(x);
#elif defined(_M_AMD64) || defined(_M_ARM64)
         unsigned long idx;
         _BitScanForward64(&idx, x);
         return idx;
#elif ZMIJ_MSC_VER
         unsigned long idx;
         if (_BitScanForward(&idx, uint32_t(x))) return idx;
         _BitScanForward(&idx, uint32_t(x >> 32));
         return idx + 32;
#else
         int n = 0;
         for (; (x & 1) == 0; x >>= 1) ++n;
         return n;
#endif
      }

      struct uint128
      {
         uint64_t hi;
         uint64_t lo;

         [[ZMIJ_MAYBE_UNUSED]] explicit constexpr operator uint64_t() const noexcept { return lo; }

         [[ZMIJ_MAYBE_UNUSED]] constexpr auto operator>>(int shift) const noexcept -> uint128
         {
            if (shift == 32) return {hi >> 32, (hi << 32) | (lo >> 32)};
            assert(shift >= 64 && shift < 128);
            return {0, hi >> (shift - 64)};
         }
      };

#ifdef ZMIJ_USE_INT128
#elif defined(__SIZEOF_INT128__)
#  define ZMIJ_USE_INT128 1
#else
#  define ZMIJ_USE_INT128 0
#endif

#if ZMIJ_USE_INT128
      using uint128_t = unsigned __int128;
#else
      using uint128_t = uint128;
#endif

#if ZMIJ_USE_INT128 && defined(__APPLE__)
      inline constexpr bool use_umul128_hi64 = true;
#else
      inline constexpr bool use_umul128_hi64 = false;
#endif

      constexpr auto umul128(uint64_t x, uint64_t y) noexcept -> uint128_t
      {
#if ZMIJ_USE_INT128
         return uint128_t(x) * y;
#else
#  if defined(_M_AMD64)
         if !consteval {
            uint64_t hi = 0;
            uint64_t lo = _umul128(x, y, &hi);
            return {hi, lo};
         }
#  elif defined(_M_ARM64)
         if !consteval { return {__umulh(x, y), x * y}; }
#  endif
         uint64_t a = x >> 32;
         uint64_t b = uint32_t(x);
         uint64_t c = y >> 32;
         uint64_t d = uint32_t(y);

         uint64_t ac = a * c;
         uint64_t bc = b * c;
         uint64_t ad = a * d;
         uint64_t bd = b * d;

         uint64_t cs = (bd >> 32) + uint32_t(ad) + uint32_t(bc);
         return {ac + (ad >> 32) + (bc >> 32) + (cs >> 32), (cs << 32) + uint32_t(bd)};
#endif
      }

      constexpr auto umul128_hi64(uint64_t x, uint64_t y) noexcept -> uint64_t { return uint64_t(umul128(x, y) >> 64); }

      inline auto umul128_add_hi64(uint64_t x, uint64_t y, uint64_t c) noexcept -> uint64_t
      {
#if ZMIJ_USE_INT128
         return uint64_t((uint128_t(x) * y + c) >> 64);
#else
         auto p = umul128(x, y);
         return p.hi + (p.lo + c < p.lo);
#endif
      }

      inline auto umul192_hi128(uint64_t x_hi, uint64_t x_lo, uint64_t y) noexcept -> uint128
      {
         uint128_t p = umul128(x_hi, y);
         uint64_t lo = uint64_t(p) + uint64_t(umul128(x_lo, y) >> 64);
         return {uint64_t(p >> 64) + (lo < uint64_t(p)), lo};
      }

      inline auto umulhi_inexact_to_odd(uint64_t x_hi, uint64_t x_lo, uint64_t y) noexcept -> uint64_t
      {
         uint128 p = umul192_hi128(x_hi, x_lo, y);
         return p.hi | ((p.lo >> 1) != 0);
      }
      inline auto umulhi_inexact_to_odd(uint64_t x_hi, uint64_t, uint32_t y) noexcept -> uint32_t
      {
         uint64_t p = uint64_t(umul128(x_hi, y) >> 32);
         return uint32_t(p >> 32) | ((uint32_t(p) >> 1) != 0);
      }

      ZMIJ_INLINE auto div10(uint64_t x) noexcept -> uint64_t
      {
         assert(x <= (1ull << 62));
         constexpr uint64_t div10_sig64 = (1ull << 63) / 5 + 1;
         return ZMIJ_USE_INT128 ? umul128_hi64(x, div10_sig64) : x / 10;
      }

      constexpr auto compute_dec_exp(int bin_exp, bool regular = true) noexcept -> int
      {
         assert(bin_exp >= -1334 && bin_exp <= 2620);
         constexpr int log10_3_over_4_sig = 131'072;
         constexpr int log10_2_sig = 315'653, log10_2_exp = 20;
         return (bin_exp * log10_2_sig - !regular * log10_3_over_4_sig) >> log10_2_exp;
      }

      template <typename Float>
      struct float_traits : std::numeric_limits<Float>
      {
         static_assert(float_traits::is_iec559, "IEEE 754 required");

         static constexpr int num_bits = float_traits::digits == 53 ? 64 : 32;
         static constexpr int num_sig_bits = float_traits::digits - 1;
         static constexpr int num_exp_bits = num_bits - num_sig_bits - 1;
         static constexpr int exp_mask = (1 << num_exp_bits) - 1;
         static constexpr int exp_bias = (1 << (num_exp_bits - 1)) - 1;
         static constexpr int exp_offset = exp_bias + num_sig_bits;
         static constexpr int min_fixed_dec_exp = -4;
         static constexpr int max_fixed_dec_exp = compute_dec_exp(float_traits::digits + 1) - 1;

         using sig_type = std::conditional_t<num_bits == 64, uint64_t, uint32_t>;
         static constexpr sig_type implicit_bit = sig_type(1) << num_sig_bits;

         static auto to_bits(Float value) noexcept -> sig_type
         {
            sig_type bits;
            memcpy(&bits, &value, sizeof(value));
            return bits;
         }

         static auto is_negative(sig_type bits) noexcept -> bool { return bits >> (num_bits - 1); }
         static auto get_sig(sig_type bits) noexcept -> sig_type { return bits & (implicit_bit - 1); }
         static auto get_exp(sig_type bits) noexcept -> int64_t { return int64_t((bits << 1) >> (num_sig_bits + 1)); }
      };

      inline constexpr uint64_t pow10_minor[] = {
         0x8000000000000000, 0xa000000000000000, 0xc800000000000000, 0xfa00000000000000, 0x9c40000000000000,
         0xc350000000000000, 0xf424000000000000, 0x9896800000000000, 0xbebc200000000000, 0xee6b280000000000,
         0x9502f90000000000, 0xba43b74000000000, 0xe8d4a51000000000, 0x9184e72a00000000, 0xb5e620f480000000,
         0xe35fa931a0000000, 0x8e1bc9bf04000000, 0xb1a2bc2ec5000000, 0xde0b6b3a76400000, 0x8ac7230489e80000,
         0xad78ebc5ac620000, 0xd8d726b7177a8000, 0x878678326eac9000, 0xa968163f0a57b400, 0xd3c21bcecceda100,
         0x84595161401484a0, 0xa56fa5b99019a5c8, 0xcecb8f27f4200f3a,
      };
      inline constexpr uint128 pow10_major[] = {
         {0xaf8e5410288e1b6f, 0x07ecf0ae5ee44dda}, {0xb1442798f49ffb4a, 0x99cd11cfdf41779d},
         {0xb2fe3f0b8599ef07, 0x861fa7e6dcb4aa15}, {0xb4bca50b065abe63, 0x0fed077a756b53aa},
         {0xb67f6455292cbf08, 0x1a3bc84c17b1d543}, {0xb84687c269ef3bfb, 0x3d5d514f40eea742},
         {0xba121a4650e4ddeb, 0x92f34d62616ce413}, {0xbbe226efb628afea, 0x890489f70a55368c},
         {0xbdb6b8e905cb600f, 0x5400e987bbc1c921}, {0xbf8fdb78849a5f96, 0xde98520472bdd034},
         {0xc16d9a0095928a27, 0x75b7053c0f178294}, {0xc350000000000000, 0x0000000000000000},
         {0xc5371912364ce305, 0x6c28000000000000}, {0xc722f0ef9d80aad6, 0x424d3ad2b7b97ef6},
         {0xc913936dd571c84c, 0x03bc3a19cd1e38ea}, {0xcb090c8001ab551c, 0x5cadf5bfd3072cc6},
         {0xcd036837130890a1, 0x36dba887c37a8c10}, {0xcf02b2c21207ef2e, 0x94f967e45e03f4bc},
         {0xd106f86e69d785c7, 0xe13336d701beba52}, {0xd31045a8341ca07c, 0x1ede48111209a051},
         {0xd51ea6fa85785631, 0x552a74227f3ea566}, {0xd732290fbacaf133, 0xa97c177947ad4096},
         {0xd94ad8b1c7380874, 0x18375281ae7822bc},
      };
      inline constexpr uint32_t pow10_fixups[] = {0x0a4e363f, 0x00001840, 0x00006400, 0x24200040, 0x00000000,
                                           0x0c000000, 0x82c81380, 0x5e4ce01f, 0xd730f60f, 0x0000001b,
                                           0x00000000, 0xcdf7fffc, 0x6e8201d8, 0x40cd3fd1, 0xdb642501,
                                           0x00000d0d, 0x14042400, 0x53713840, 0x11781db4, 0x00000000};

      template <bool OptSize>
      struct pow10_significand_table
      {
         static constexpr bool compress = OptSize;
         static constexpr bool split_tables = !compress && ZMIJ_AARCH64 != 0;
         static constexpr int num_pow10s = 618;
         uint64_t data[compress ? 1 : num_pow10s * 2] = {};

         static constexpr auto compute(unsigned i) noexcept -> uint128
         {
            constexpr int stride = sizeof(pow10_minor) / sizeof(*pow10_minor);
            auto m = pow10_minor[(i + 10) % stride];
            auto h = pow10_major[(i + 10) / stride];

            uint64_t h1 = umul128_hi64(h.lo, m);

            uint64_t c0 = h.lo * m;
            uint64_t c1 = h1 + h.hi * m;
            uint64_t c2 = (c1 < h1) + umul128_hi64(h.hi, m);

            uint128 result =
               (c2 >> 63) != 0 ? uint128{c2, c1} : uint128{c2 << 1 | c1 >> 63, c1 << 1 | c0 >> 63};
            result.lo -= (pow10_fixups[i >> 5] >> (i & 31)) & 1;
            return result;
         }

         constexpr pow10_significand_table()
         {
            for (int i = 0; i < num_pow10s && !compress; ++i) {
               uint128 result = compute(i);
               if (split_tables) {
                  data[num_pow10s - i - 1] = result.hi;
                  data[num_pow10s * 2 - i - 1] = result.lo;
               }
               else {
                  data[i * 2] = result.hi;
                  data[i * 2 + 1] = result.lo;
               }
            }
         }

         constexpr auto operator[](int dec_exp) const noexcept -> uint128
         {
            constexpr int dec_exp_min = -293;
            int i = dec_exp - dec_exp_min;
            if (compress) return compute(i);
            if (!split_tables) return {data[i * 2], data[i * 2 + 1]};

            const uint64_t* hi = data + num_pow10s + dec_exp_min - 1;
            const uint64_t* lo = hi + num_pow10s;

            if !consteval { ZMIJ_ASM(volatile("" : "+r"(hi), "+r"(lo))); }
            return {hi[-dec_exp], lo[-dec_exp]};
         }
      };

      constexpr ZMIJ_INLINE auto compute_exp_shift(int bin_exp, int dec_exp) noexcept -> unsigned char
      {
         assert(dec_exp >= -350 && dec_exp <= 350);
         constexpr int log2_pow10_sig = 217'707, log2_pow10_exp = 16;
         int pow10_bin_exp = -dec_exp * log2_pow10_sig >> log2_pow10_exp;
         return bin_exp + pow10_bin_exp + 1;
      }

      template <bool OptSize>
      struct exp_shift_table
      {
         static constexpr bool enable = !OptSize;
         static constexpr int extra_shift = 6;
         unsigned char data[enable ? float_traits<double>::exp_mask + 1 : 1] = {};

         constexpr exp_shift_table()
         {
            for (int raw_exp = 0; raw_exp < sizeof(data) && enable; ++raw_exp) {
               int bin_exp = raw_exp - float_traits<double>::exp_offset;
               if (raw_exp == 0) ++bin_exp;
               int dec_exp = compute_dec_exp(bin_exp);
               data[raw_exp] = compute_exp_shift(bin_exp, dec_exp + 1) + extra_shift;
            }
         }
      };
      template <bool OptSize>
      inline constexpr exp_shift_table<OptSize> exp_shifts_v;

      template <bool OptSize>
      struct exp_string_table
      {
         static constexpr bool enable = !OptSize;
         using traits = float_traits<double>;
         static constexpr int min_dec_exp = traits::min_exponent10 - traits::max_digits10;
         static constexpr int offset = -min_dec_exp;
         uint64_t data[enable ? traits::max_exponent10 - min_dec_exp + 1 : 1] = {};

         constexpr exp_string_table()
         {
            for (int e = min_dec_exp; e <= traits::max_exponent10 && enable; ++e) {
               uint64_t abs_e = e >= 0 ? e : -e;
               uint64_t bc = abs_e % 100;
               uint64_t val = ((bc % 10 + '0') << 8) | (bc / 10 + '0');
               if (uint64_t a = abs_e / 100) val = (val << 8) | (a + '0');
               // Glaze emits uppercase 'E' and omits a '+' on positive exponents
               // to match the pre-zmij dragonbox output.
               if (e >= 0) {
                  // "E12" (3 bytes) or "E123" (4 bytes)
                  uint64_t len = 3 + (abs_e >= 100);
                  data[e + offset] = (len << 48) | (val << 8) | 'E';
               }
               else {
                  // "E-12" (4 bytes) or "E-123" (5 bytes)
                  uint64_t len = 4 + (abs_e >= 100);
                  data[e + offset] = (len << 48) | (val << 16) | (uint64_t('-') << 8) | 'E';
               }
            }
         }
      };
      template <bool OptSize>
      inline constexpr exp_string_table<OptSize> exp_strings_v;

      struct dec_exp_format_table
      {
         using traits = float_traits<double>;
         static constexpr int num_entries = traits::max_fixed_dec_exp - traits::min_fixed_dec_exp + 2;

         struct entry
         {
            unsigned char start_pos;
            unsigned char point_pos;
            unsigned char shift_pos;
            unsigned char exp_pos[traits::max_digits10];
         };

         entry data[num_entries] = {};

         constexpr dec_exp_format_table()
         {
            for (int dec_exp = traits::min_fixed_dec_exp; dec_exp <= traits::max_fixed_dec_exp + 1; ++dec_exp) {
               auto& e = data[dec_exp - traits::min_fixed_dec_exp];
               bool neg_fixed = dec_exp >= traits::min_fixed_dec_exp && dec_exp <= -1;
               bool pos_fixed = dec_exp >= 0 && dec_exp <= traits::max_fixed_dec_exp;

               e.start_pos = neg_fixed ? 1 - dec_exp : 0;
               e.point_pos = pos_fixed ? 1 + dec_exp : 1;
               e.shift_pos = e.point_pos + (dec_exp >= 0 || dec_exp < traits::min_fixed_dec_exp);

               for (int s = 1; s <= traits::max_digits10; ++s) {
                  if (neg_fixed)
                     e.exp_pos[s - 1] = s;
                  else if (pos_fixed)
                     e.exp_pos[s - 1] = s > dec_exp + 1 ? s + 1 : dec_exp + 1;
                  else
                     e.exp_pos[s - 1] = s + 1 - (s == 1);
               }
            }
         }

         template <typename Traits>
         constexpr auto get(int dec_exp) const noexcept -> const entry&
         {
            constexpr auto min = traits::min_fixed_dec_exp, max = Traits::max_fixed_dec_exp;
            unsigned i = unsigned(dec_exp - min);
            return data[i <= unsigned(max - min) ? i : num_entries - 1];
         }
      };
      inline constexpr dec_exp_format_table dec_exp_formats;

      inline auto count_trailing_nonzeros(uint64_t x) noexcept -> int
      {
         if (is_big_endian) x = bswap64(x);
         return (size_t(70) - clz((x << 1) | 1)) / 8;
      }

      inline auto digits2(size_t value) noexcept -> const char*
      {
         alignas(2) static const char data[] =
            "0001020304050607080910111213141516171819"
            "2021222324252627282930313233343536373839"
            "4041424344454647484950515253545556575859"
            "6061626364656667686970717273747576777879"
            "8081828384858687888990919293949596979899";
         return &data[value * 2];
      }

      inline constexpr int div10k_exp = 40;
      inline constexpr uint32_t div10k_sig = uint32_t((1ull << div10k_exp) / 10000 + 1);
      inline constexpr uint32_t neg10k_v = uint32_t((1ull << 32) - 10000);

      inline constexpr int div100_exp = 19;
      inline constexpr uint32_t div100_sig = (1 << div100_exp) / 100 + 1;
      inline constexpr uint32_t neg100_v = (1 << 16) - 100;

      inline constexpr int div10_exp = 10;
      inline constexpr uint32_t div10_sig = (1 << div10_exp) / 10 + 1;
      inline constexpr uint32_t neg10_v = (1 << 8) - 10;

      inline constexpr uint64_t zeros_v = 0x0101010101010101u * '0';

      inline auto write_if(char* buffer, uint32_t digit, bool condition) noexcept -> char*
      {
         *buffer = char('0' + digit);
         return buffer + condition;
      }

      template <bool OptSize>
      struct constants
      {
         static constexpr auto splat64(uint64_t x) -> uint128 { return {x, x}; }
         static constexpr auto splat32(uint32_t x) -> uint128 { return splat64(uint64_t(x) << 32 | x); }
         static constexpr auto splat16(uint16_t x) -> uint128 { return splat32(uint32_t(x) << 16 | x); }
         static constexpr auto pack8(uint8_t a, uint8_t b, uint8_t c, uint8_t d, //
                                     uint8_t e, uint8_t f, uint8_t g, uint8_t h) -> uint64_t
         {
            using u64 = uint64_t;
            return u64(h) << 56 | u64(g) << 48 | u64(f) << 40 | u64(e) << 32 | u64(d) << 24 | u64(c) << 16 |
                   u64(b) << +8 | u64(a);
         }

         ZMIJ_CONST_DECL uint64_t threshold = 1e15;
         ZMIJ_CONST_DECL uint64_t biased_half = (uint64_t(1) << 63) + 6;

#if ZMIJ_USE_NEON
         static constexpr int32_t neg10k = -10000 + 0x10000;

         using int32x4 = std::conditional_t<ZMIJ_MSC_VER != 0, int32_t[4], int32x4_t>;
         using int16x8 = std::conditional_t<ZMIJ_MSC_VER != 0, int16_t[8], int16x8_t>;

         uint64_t mul_const = 0xabcc77118461cefd;
         uint64_t hundred_million = 100000000;
         int32x4 multipliers32 = {div10k_sig, neg10k, div100_sig << 12, neg100_v};
         int16x8 multipliers16 = {0xce0, neg10_v};
#elif ZMIJ_USE_SSE
         uint128 div100 = splat32(div100_sig);
         uint128 div10 = splat16((1 << 16) / 10 + 1);
#  if ZMIJ_USE_SSE4_1
         uint128 neg100 = splat32(neg100_v);
         uint128 neg10 = splat16((1 << 8) - 10);
         uint128 bswap = uint128{pack8(15, 14, 13, 12, 11, 10, 9, 8), pack8(7, 6, 5, 4, 3, 2, 1, 0)};
#  else
         uint128 hundred = splat32(100);
         uint128 moddiv10 = splat16(10 * (1 << 8) - 1);
#  endif
         uint128 div10k = splat64(div10k_sig);
         uint128 neg10k = splat64(neg10k_v);
         uint128 zeros = splat64(zeros_v);
#endif

         alignas(64) pow10_significand_table<OptSize> pow10_significands;
      };
      template <bool OptSize>
      alignas(64) inline constexpr constants<OptSize> consts_v;

#if ZMIJ_USE_NEON
      template <bool OptSize>
      ZMIJ_INLINE auto to_bcd_4x4(int32x4_t ddee_bbcc_hhii_ffgg, const constants<OptSize>& c) noexcept -> uint8x16_t
      {
         ZMIJ_ASM(("" : "+w"(ddee_bbcc_hhii_ffgg)));

         int32x4_t dd_bb_hh_ff = vqdmulhq_n_s32(ddee_bbcc_hhii_ffgg, c.multipliers32[2]);
         int16x8_t ee_dd_cc_bb_ii_hh_gg_ff =
            vreinterpretq_s16_s32(vmlaq_n_s32(ddee_bbcc_hhii_ffgg, dd_bb_hh_ff, c.multipliers32[3]));
         int16x8_t high_10s = vqdmulhq_n_s16(ee_dd_cc_bb_ii_hh_gg_ff, c.multipliers16[0]);
         return vreinterpretq_u8_s16(vmlaq_n_s16(ee_dd_cc_bb_ii_hh_gg_ff, high_10s, c.multipliers16[1]));
      }

      template <bool OptSize, bool reverse_hi_lo = false>
      ZMIJ_INLINE auto to_unshuffled_digits(uint64_t value, const constants<OptSize>& c) -> uint8x16_t
      {
         uint64_t hundred_million = c.hundred_million;

         ZMIJ_ASM(("" : "+r"(hundred_million)));

         uint64_t abbccddee = uint64_t(umul128(value, c.mul_const) >> 90);
         uint64_t ffgghhii = value - abbccddee * hundred_million;

         uint64x1_t ffgghhii_bbccddee_64 = {
            reverse_hi_lo ? (abbccddee << 32) | ffgghhii : (ffgghhii << 32) | abbccddee};
         int32x2_t bbccddee_ffgghhii = vreinterpret_s32_u64(ffgghhii_bbccddee_64);

         int32x2_t bbcc_ffgg = vreinterpret_s32_u32(vshr_n_u32(
            vreinterpret_u32_s32(vqdmulh_n_s32(bbccddee_ffgghhii, c.multipliers32[0])), 9));
         int32x2_t ddee_bbcc_hhii_ffgg_32 = vmla_n_s32(bbccddee_ffgghhii, bbcc_ffgg, c.multipliers32[1]);

         // AArch64 accepts `vshll_n_u16(x, 0)` as "widen without shift", but
         // ARMv7 VSHLL requires a shift in [1, 15]. vmovl_u16 is the portable
         // name for the same operation and codegens identically on AArch64.
         int32x4_t ddee_bbcc_hhii_ffgg =
            vreinterpretq_s32_u32(vmovl_u16(vreinterpret_u16_s32(ddee_bbcc_hhii_ffgg_32)));
         return to_bcd_4x4(ddee_bbcc_hhii_ffgg, c);
      }

#elif ZMIJ_USE_SSE

      using m128ptr = const __m128i*;

      template <bool OptSize>
      ZMIJ_INLINE auto to_bcd_4x4(__m128i y, const constants<OptSize>& c) noexcept -> __m128i
      {
         const __m128i div100 = _mm_load_si128(m128ptr(&c.div100));
         const __m128i div10 = _mm_load_si128(m128ptr(&c.div10));
#  if ZMIJ_USE_SSE4_1
         const __m128i neg100 = _mm_load_si128(m128ptr(&c.neg100));
         const __m128i neg10 = _mm_load_si128(m128ptr(&c.neg10));

         __m128i z = _mm_add_epi64(
            y, _mm_mullo_epi32(neg100, _mm_srli_epi32(_mm_mulhi_epu16(y, div100), 3)));
         return _mm_add_epi16(z, _mm_mullo_epi16(neg10, _mm_mulhi_epu16(z, div10)));
#  else
         const __m128i hundred = _mm_load_si128(m128ptr(&c.hundred));
         const __m128i moddiv10 = _mm_load_si128(m128ptr(&c.moddiv10));

         __m128i y_div_100 = _mm_srli_epi16(_mm_mulhi_epu16(y, div100), 3);
         __m128i y_mod_100 = _mm_sub_epi16(y, _mm_mullo_epi16(y_div_100, hundred));
         __m128i z = _mm_or_si128(_mm_slli_epi32(y_mod_100, 16), y_div_100);
         return _mm_sub_epi16(_mm_slli_epi16(z, 8),
                              _mm_mullo_epi16(moddiv10, _mm_mulhi_epu16(z, div10)));
#  endif
      }

      template <bool OptSize>
      ZMIJ_INLINE auto to_unshuffled_digits(uint32_t bbccddee, uint32_t ffgghhii, const constants<OptSize>& c) noexcept
         -> __m128i
      {
         const __m128i div10k = _mm_load_si128(m128ptr(&c.div10k));
         const __m128i neg10k = _mm_load_si128(m128ptr(&c.neg10k));
         __m128i x = _mm_set_epi64x(bbccddee, ffgghhii);
         __m128i y = _mm_add_epi64(
            x, _mm_mul_epu32(neg10k, _mm_srli_epi64(_mm_mul_epu32(x, div10k), div10k_exp)));
         return to_bcd_4x4(y, c);
      }

#endif

      struct bcd_result
      {
         uint64_t bcd;
         int len;
      };

      template <bool OptSize>
      inline auto to_bcd8(uint64_t abcdefgh) noexcept -> bcd_result
      {
         if (!ZMIJ_USE_SSE && !ZMIJ_USE_NEON) {
            uint64_t abcd_efgh = abcdefgh + neg10k_v * ((abcdefgh * div10k_sig) >> div10k_exp);
            uint64_t ab_cd_ef_gh =
               abcd_efgh + neg100_v * (((abcd_efgh * div100_sig) >> div100_exp) & 0x7f0000007f);
            uint64_t a_b_c_d_e_f_g_h =
               ab_cd_ef_gh + neg10_v * (((ab_cd_ef_gh * div10_sig) >> div10_exp) & 0xf000f000f000f);
            uint64_t bcd = is_big_endian ? a_b_c_d_e_f_g_h : bswap64(a_b_c_d_e_f_g_h);
            return {bcd, count_trailing_nonzeros(bcd)};
         }

         const auto* c = &consts_v<OptSize>;
         ZMIJ_ASM(("" : "+r"(c)));

#if ZMIJ_USE_NEON
         uint64_t abcd_efgh_64 = abcdefgh + neg10k_v * ((abcdefgh * div10k_sig) >> div10k_exp);
         int32x4_t abcd_efgh =
            vcombine_s32(vreinterpret_s32_u64(vcreate_u64(abcd_efgh_64)), vdup_n_s32(0));
         uint8x16_t digits_128 = to_bcd_4x4(abcd_efgh, *c);
         uint8x8_t digits = vget_low_u8(digits_128);
         uint64_t bcd = vget_lane_u64(vreinterpret_u64_u8(vrev64_u8(digits)), 0);
         return {bcd, count_trailing_nonzeros(bcd)};
#elif ZMIJ_USE_SSE4_1
         uint64_t abcd_efgh = abcdefgh + neg10k_v * ((abcdefgh * div10k_sig) >> div10k_exp);
         uint64_t unshuffled_bcd = _mm_cvtsi128_si64(to_bcd_4x4(_mm_set_epi64x(0, abcd_efgh), *c));
         int len = unshuffled_bcd ? 8 - ctz(unshuffled_bcd) / 8 : 0;
         return {bswap64(unshuffled_bcd), len};
#elif ZMIJ_USE_SSE
         uint64_t abcd_efgh = (abcdefgh << 32) - uint64_t((10000ull << 32) - 1) *
                                                    ((abcdefgh * div10k_sig) >> div10k_exp);
         uint64_t bcd = _mm_cvtsi128_si64(to_bcd_4x4(_mm_set_epi64x(0, abcd_efgh), *c));
         return {bcd, count_trailing_nonzeros(bcd)};
#endif
      }

      template <int num_bits>
      struct dec_digits
      {
         uint64_t digits;
         int num_digits;
      };

      template <>
      struct dec_digits<64>
      {
#if ZMIJ_USE_NEON
         uint16x8_t digits;
#elif ZMIJ_USE_SSE
         __m128i digits;
#else
         uint128 digits;
#endif
         int num_digits;
      };

      template <int num_bits, bool OptSize>
      ZMIJ_INLINE auto to_digits(uint64_t value, [[maybe_unused]] bool extra_digit,
                                 [[maybe_unused]] const constants<OptSize>& c) noexcept -> dec_digits<num_bits>
      {
         if constexpr (num_bits == 32) {
            auto result = to_bcd8<OptSize>(value);
            return {result.bcd + zeros_v, result.len};
         }
         else {
#if !ZMIJ_USE_NEON && !ZMIJ_USE_SSE
         uint32_t bbccddee = uint32_t(value / 100'000'000);
         uint32_t ffgghhii = uint32_t(value % 100'000'000);
         auto hi = to_bcd8<OptSize>(bbccddee);
         if (ffgghhii == 0) return {{hi.bcd + zeros_v, zeros_v}, hi.len};
         auto lo = to_bcd8<OptSize>(ffgghhii);
         return {{hi.bcd + zeros_v, lo.bcd + zeros_v}, 8 + lo.len};
#elif ZMIJ_USE_NEON
         auto unshuffled_digits = to_unshuffled_digits(value, c);
         uint8x16_t digits = vrev64q_u8(unshuffled_digits);
         uint16x8_t str =
            vaddq_u16(vreinterpretq_u16_u8(digits), vreinterpretq_u16_s8(vdupq_n_s8('0')));

         // vcgtzq_s8 is AArch64-only; the portable spelling (ARMv7 NEON and
         // up) is "compare greater-than against a zero vector".
         uint16x8_t is_not_zero = vreinterpretq_u16_u8(
            vcgtq_s8(vreinterpretq_s8_u8(digits), vdupq_n_s8(0)));
         uint64_t zeroes = vget_lane_u64(vreinterpret_u64_u8(vshrn_n_u16(is_not_zero, 4)), 0);
         return {str, 16 - (clz(zeroes) >> 2)};
#else
         uint32_t abbccddee = uint32_t(value / 100'000'000);
         uint32_t ffgghhii = uint32_t(value % 100'000'000);

         const __m128i zeros = _mm_load_si128(m128ptr(&c.zeros));
         auto unshuffled_bcd = to_unshuffled_digits(abbccddee, ffgghhii, c);
#  if ZMIJ_USE_SSE4_1
         const __m128i bswap = _mm_load_si128(m128ptr(&c.bswap));
         auto bcd = _mm_shuffle_epi8(unshuffled_bcd, bswap);
#  else
         auto bcd = _mm_shuffle_epi32(unshuffled_bcd, _MM_SHUFFLE(0, 1, 2, 3));
#  endif

         __m128i mask128 = _mm_cmpgt_epi8(bcd, _mm_setzero_si128());
         uint64_t mask = _mm_movemask_epi8(mask128);
#  if defined(__LZCNT__) && !defined(ZMIJ_NO_BUILTINS)
         int len = 32 - _lzcnt_u32(mask);
#  else
         int len = 63 - clz((mask << 1) | 1);
#  endif
         return {_mm_or_si128(bcd, zeros), len};
#endif
         }
      }

      struct to_decimal_result
      {
         long long sig;
         int exp;
         int last_digit = 0;
         bool has_last_digit = false;
      };

      template <typename Float, typename UInt, bool OptSize>
      ZMIJ_INLINE auto to_decimal(UInt bin_sig, int64_t raw_exp, bool regular, const constants<OptSize>& c) noexcept
         -> to_decimal_result
      {
         using traits = float_traits<Float>;
         int64_t bin_exp = raw_exp - traits::exp_offset;
         constexpr int num_bits = std::numeric_limits<UInt>::digits;

         constexpr uint64_t log10_2_sig = 78'913;
         constexpr int log10_2_exp = 18;
         int dec_exp = use_umul128_hi64 ? umul128_hi64(bin_exp, log10_2_sig << (64 - log10_2_exp))
                                         : compute_dec_exp(bin_exp);
         uint64_t even = 1 - (bin_sig & 1);
         constexpr int extra_shift = exp_shift_table<OptSize>::extra_shift;

         if (!regular) [[ZMIJ_UNLIKELY]] {
            int dec_exp = compute_dec_exp(bin_exp, false);
            unsigned char shift = compute_exp_shift(bin_exp, dec_exp + 1) + extra_shift;
            uint128 pow10 = c.pow10_significands[-dec_exp - 1];
            uint128 p = umul192_hi128(pow10.hi, pow10.lo, bin_sig << shift);

            long long integral = p.hi >> extra_shift;
            uint64_t fractional = p.hi << (64 - extra_shift) | p.lo >> extra_shift;

            uint64_t half_ulp = pow10.hi >> (extra_shift + 1 - shift);
            bool round_up = half_ulp > ~uint64_t(0) - fractional;
            bool round_down = (half_ulp >> 1) > fractional;
            integral += round_up;

            int digit = int(umul128_add_hi64(fractional, 10, (uint64_t(1) << 63) - 1));
            int lo = int(umul128_add_hi64(fractional - (half_ulp >> 1), 10, ~uint64_t(0)));
            if (digit < lo) digit = lo;
            return {integral, dec_exp, digit, (round_up + round_down) == 0};
         }

         if (num_bits == 32) {
            constexpr int extra_shift = 34;
            unsigned char shift = compute_exp_shift(bin_exp, dec_exp + 1) + extra_shift;
            uint64_t pow10_hi = c.pow10_significands[-dec_exp - 1].hi;
            uint64_t p = umul128_hi64(pow10_hi + 1, uint64_t(bin_sig) << shift);

            long long integral = p >> extra_shift;
            uint64_t fractional = p & ((1ull << extra_shift) - 1);

            uint64_t half_ulp = (pow10_hi >> (65 - shift)) + even;
            bool round_up = (fractional + half_ulp) >> extra_shift;
            bool round_down = half_ulp > fractional;
            integral += round_up;

            uint64_t prod = fractional * 10;
            int digit = int(prod >> extra_shift);
            uint64_t rem = prod & ((1ull << extra_shift) - 1);
            digit += rem > (1ull << (extra_shift - 1)) ||
                     (rem == (1ull << (extra_shift - 1)) && (digit & 1));
            return {integral, dec_exp, digit, (round_up + round_down) == 0};
         }

         unsigned char shift = exp_shift_table<OptSize>::enable
                                  ? exp_shifts_v<OptSize>.data[bin_exp + traits::exp_offset]
                                  : compute_exp_shift(bin_exp, dec_exp + 1) + extra_shift;
         ZMIJ_ASM(("" : "+r"(dec_exp)));
         uint128 pow10 = c.pow10_significands[-dec_exp - 1];
         uint128 p = umul192_hi128(pow10.hi, pow10.lo, bin_sig << shift);

         long long integral = p.hi >> extra_shift;
         uint64_t fractional = p.hi << (64 - extra_shift) | p.lo >> extra_shift;

         uint64_t half_ulp = (pow10.hi >> (extra_shift + 1 - shift)) + even;
         bool round_up = fractional + half_ulp < fractional;
         bool round_down = half_ulp > fractional;
         integral += round_up;

         int digit = int(umul128_add_hi64(fractional, 10, c.biased_half));
         if (fractional == (1ull << 62)) [[ZMIJ_UNLIKELY]]
            digit = 2;
         return {integral, dec_exp, digit, (round_up + round_down) == 0};
      }

   } // namespace detail_impl

   template <bool OptSize = false>
   inline auto to_decimal(double value) noexcept -> dec_fp
   {
      using namespace detail_impl;
      using traits = float_traits<double>;
      auto bits = traits::to_bits(value);
      auto bin_exp = traits::get_exp(bits);
      auto bin_sig = traits::get_sig(bits);
      auto negative = traits::is_negative(bits);
      if (bin_exp == 0 || bin_exp == traits::exp_mask) [[ZMIJ_UNLIKELY]] {
         if (bin_exp != 0) return {int64_t(bin_sig), int(~0u >> 1), negative};
         if (bin_sig == 0) return {0, 0, negative};
         bin_exp = 1;
         bin_sig |= traits::implicit_bit;
      }
      auto dec = detail_impl::to_decimal<double, typename traits::sig_type, OptSize>(
         bin_sig ^ traits::implicit_bit, bin_exp, bin_sig != 0, consts_v<OptSize>);
      auto last_digit = dec.has_last_digit ? dec.last_digit : 0;
      return {dec.sig * 10 + last_digit, dec.exp, negative};
   }

   namespace detail
   {
      template <typename Float, bool OptSize>
      inline auto write(Float value, char* buffer) noexcept -> char*
      {
         using namespace detail_impl;
         using traits = float_traits<Float>;
         auto bits = traits::to_bits(value);
         auto bin_exp = traits::get_exp(bits);
         auto bin_sig = traits::get_sig(bits);

         *buffer = '-';
         buffer += traits::is_negative(bits);

         const auto* c = &consts_v<OptSize>;
         ZMIJ_ASM(("" : "+r"(c)));
         uint64_t threshold = traits::num_bits == 64 ? c->threshold : uint64_t(1e7);

         to_decimal_result dec;
         bool is_normal = unsigned(bin_exp - 1) < unsigned(traits::exp_mask - 1);
         if (!is_normal) [[ZMIJ_UNLIKELY]] {
            if (bin_exp != 0) {
#if GLZ_ZMIJ_EMIT_INF_NAN
               memcpy(buffer, bin_sig == 0 ? "inf" : "nan", 4);
               return buffer + 3;
#else
               // Undo the speculative '-' for sign-bit-set NaN/Inf: JSON "null"
               // never carries a sign.
               buffer -= traits::is_negative(bits);
               memcpy(buffer, "null", 4);
               return buffer + 4;
#endif
            }
            if (bin_sig == 0) {
               memcpy(buffer, "0", 2);
               return buffer + 1;
            }
            dec = detail_impl::to_decimal<Float, typename traits::sig_type, OptSize>(bin_sig, 1, true, *c);
            long long dec_sig = dec.sig * 10 + (dec.has_last_digit ? dec.last_digit : 0);
            int dec_exp = dec.exp;
            while (dec_sig < threshold) {
               dec_sig *= 10;
               --dec_exp;
            }
            long long d = detail_impl::div10(dec_sig);
            int last_digit = dec_sig - d * 10;
            dec = {d, dec_exp, last_digit, last_digit != 0};
         }
         else {
            dec = detail_impl::to_decimal<Float, typename traits::sig_type, OptSize>(
               bin_sig | traits::implicit_bit, bin_exp, bin_sig != 0, *c);
         }
         bool extra_digit = dec.sig >= threshold;
         int dec_exp = dec.exp + traits::max_digits10 - 2 + extra_digit;
         if (traits::num_bits == 32 && dec.sig < uint32_t(1e6)) [[ZMIJ_UNLIKELY]] {
            dec.sig = 10 * dec.sig + (dec.has_last_digit ? dec.last_digit : 0);
            dec.has_last_digit = false;
            --dec_exp;
         }

         char* start = buffer;
         auto dig = to_digits<traits::num_bits, OptSize>(dec.sig, extra_digit, *c);
         constexpr int bcd_size = traits::num_bits == 64 ? 16 : 8;
         if (dec_exp >= traits::min_fixed_dec_exp && dec_exp <= traits::max_fixed_dec_exp) {
            memcpy(start, &zeros_v, 8);
            const auto& fmt = dec_exp_formats.get<traits>(dec_exp);
            buffer += fmt.start_pos;
            memcpy(buffer, &dig.digits, bcd_size);
            memmove(buffer, buffer + !extra_digit, bcd_size);
            buffer[bcd_size + extra_digit - 1] = '0' + (dec.has_last_digit ? dec.last_digit : 0);
            memmove(start + fmt.shift_pos, start + fmt.point_pos, bcd_size);
            start[fmt.point_pos] = '.';
            int num_digits = dec.has_last_digit ? bcd_size : dig.num_digits - 1;
            return buffer + fmt.exp_pos[num_digits + extra_digit - 1];
         }
         buffer += extra_digit;
         memcpy(buffer, &dig.digits, bcd_size);
         buffer[bcd_size] = '0' + dec.last_digit;
         buffer += dec.has_last_digit ? bcd_size + 1 : dig.num_digits;
         start[0] = start[1];
         start[1] = '.';
         buffer -= (buffer - 1 == start + 1);

         if (exp_string_table<OptSize>::enable) {
            uint64_t exp_data = exp_strings_v<OptSize>.data[dec_exp + exp_string_table<OptSize>::offset];
            int len = int(exp_data >> 48);
            if (is_big_endian) exp_data = bswap64(exp_data);
            memcpy(buffer, &exp_data, traits::max_exponent10 >= 100 ? 8 : 4);
            return buffer + len;
         }
         // Glaze emits uppercase 'E' and omits '+' on positive exponents to
         // match the pre-zmij dragonbox output. Branchless: write "E-" always,
         // then only advance past the '-' when the exponent is negative.
         bool neg = dec_exp < 0;
         buffer[0] = 'E';
         buffer[1] = '-';
         buffer += 1 + unsigned(neg);
         dec_exp = neg ? -dec_exp : dec_exp;
         if (traits::max_exponent10 >= 100) {
            uint32_t digit = use_umul128_hi64
                                ? umul128_hi64(dec_exp, 0x290000000000000)
                                : (uint32_t(dec_exp) * div100_sig) >> div100_exp;
            *buffer = '0' + digit;
            buffer += dec_exp >= 100;
            dec_exp -= digit * 100;
         }
         memcpy(buffer, digits2(dec_exp), 2);
         return buffer + 2;
      }

   } // namespace detail

} // namespace glz::zmij

namespace glz
{
   // JSON-compliant float serializer. Caller must provide a buffer of at least
   // zmij::double_buffer_size (34) bytes. Non-finite inputs emit "null" (gated
   // by GLZ_ZMIJ_EMIT_INF_NAN).
   //
   // OptSize=false (default): uses the full ~17 KB pow-10 lookup tables for peak throughput.
   // OptSize=true:             drops the tables (recomputed on the fly) for size-constrained
   //                           builds — wired to Glaze's is_size_optimized(Opts) flag.
   // Both instantiations can coexist in a single binary.
   //
   // Returns a pointer past the last written character. Nothing is promised
   // about *end — zmij's branchless fixed-point shuffle may scribble one byte
   // past the returned pointer (a '.' that's excluded from the returned
   // length). Callers who need a null-terminated C-string must write '\0' at
   // *end themselves. Glaze's JSON writer doesn't care: buffer_traits::finalize
   // resizes the output down to the tracked length, discarding anything past it.
   template <std::floating_point T, bool OptSize = false>
   inline char* to_chars(char* buf, T val) noexcept
   {
      static_assert(std::is_same_v<T, float> || std::is_same_v<T, double>);
      return zmij::detail::write<T, OptSize>(val, buf);
   }
}

#if defined(__clang__)
#  pragma clang diagnostic pop
#elif defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif
