// Glaze Library
// For the license information refer to glaze.hpp

// SIMD UTF-8 validation using the Lemire & Keiser algorithm, as popularized by simdjson.
//
// A UTF-8 error is always determined by a byte and its predecessor, so the whole check reduces to
// three 16 entry table lookups (on the previous byte's high nibble, its low nibble, and the current
// byte's high nibble) whose AND is non-zero exactly when the pair is illegal. A second check
// confirms that bytes which must be the 2nd or 3rd continuation of a long sequence actually are.
// Both run branchlessly over a whole register, so throughput barely depends on the content.
//
// One backend is selected at compile time, widest first: AVX-512BW, AVX2, SSSE3, AArch64 NEON, or
// WebAssembly SIMD128. Targets without a byte-granular shuffle (plain SSE2, 32 bit NEON) fall back
// to the scalar validator in parse.hpp, which is always correct, just slower.

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>

#include "glaze/simd/simd.hpp"
#include "glaze/util/inline.hpp"

// GLZ_UTF8_GENERIC_WIDTH selects the portable backend below, which needs no target support, so it
// enables the vector path on its own. Without it here the whole file would compile away on a host
// with no usable backend, and the width-generic tests would silently degrade to testing the scalar
// validator against itself. It still defers to GLZ_DISABLE_SIMD: the GLZ_USE_* macros are already
// suppressed by it, and a build asking for no vector path should get none regardless of which macro
// would have selected one.
#if defined(GLZ_USE_AVX512BW) || defined(GLZ_USE_AVX2) || defined(GLZ_USE_SSSE3) || defined(GLZ_USE_NEON64) || \
   defined(GLZ_USE_WASM_SIMD128) || (defined(GLZ_UTF8_GENERIC_WIDTH) && !defined(GLZ_DISABLE_SIMD))
#define GLZ_UTF8_SIMD
#endif

#if defined(GLZ_UTF8_SIMD)

namespace glz::detail::utf8_simd
{
   // Error classes. A lookup hit means "this pair is wrong for this reason".
   inline constexpr uint8_t too_short = 1 << 0; // lead followed by a non-continuation
   inline constexpr uint8_t too_long = 1 << 1; // continuation not preceded by a lead
   inline constexpr uint8_t overlong_3 = 1 << 2; // E0 80..9F
   inline constexpr uint8_t too_large = 1 << 3; // > U+10FFFF
   inline constexpr uint8_t surrogate = 1 << 4; // ED A0..BF
   inline constexpr uint8_t overlong_2 = 1 << 5; // C0..C1
   inline constexpr uint8_t too_large_1000 = 1 << 6;
   inline constexpr uint8_t overlong_4 = 1 << 6; // F0 80..8F
   inline constexpr uint8_t two_conts = 1 << 7;
   inline constexpr uint8_t carry = too_short | too_long | two_conts;

   // Indexed by the high nibble of the previous byte.
   alignas(16) inline constexpr uint8_t byte_1_high[16]{
      too_long,
      too_long,
      too_long,
      too_long, // 0..3 ASCII
      too_long,
      too_long,
      too_long,
      too_long, // 4..7 ASCII
      two_conts,
      two_conts,
      two_conts,
      two_conts, // 8..B continuation
      too_short | overlong_2, // C
      too_short, // D
      too_short | overlong_3 | surrogate, // E
      too_short | too_large | too_large_1000 | overlong_4 // F
   };

   // Indexed by the low nibble of the previous byte.
   alignas(16) inline constexpr uint8_t byte_1_low[16]{
      carry | overlong_3 | overlong_2 | overlong_4, // 0
      carry | overlong_2, // 1
      carry, // 2
      carry, // 3
      carry | too_large, // 4
      carry | too_large | too_large_1000, // 5
      carry | too_large | too_large_1000, // 6
      carry | too_large | too_large_1000, // 7
      carry | too_large | too_large_1000, // 8
      carry | too_large | too_large_1000, // 9
      carry | too_large | too_large_1000, // A
      carry | too_large | too_large_1000, // B
      carry | too_large | too_large_1000, // C
      carry | too_large | too_large_1000 | surrogate, // D
      carry | too_large | too_large_1000, // E
      carry | too_large | too_large_1000 // F
   };

   // Indexed by the high nibble of the current byte.
   alignas(16) inline constexpr uint8_t byte_2_high[16]{
      too_short,
      too_short,
      too_short,
      too_short, // 0..3
      too_short,
      too_short,
      too_short,
      too_short, // 4..7
      too_long | overlong_2 | two_conts | overlong_3 | too_large_1000 | overlong_4, // 8
      too_long | overlong_2 | two_conts | overlong_3 | too_large, // 9
      too_long | overlong_2 | two_conts | surrogate | too_large, // A
      too_long | overlong_2 | two_conts | surrogate | too_large, // B
      too_short,
      too_short,
      too_short,
      too_short // C..F
   };

   // Saturating-subtract thresholds leaving a non-zero byte when the last 1..3 bytes of a register
   // begin a sequence running past its end. Backends load the trailing `width` bytes of this array,
   // so the three thresholds always land in the final three lanes regardless of register size.
   alignas(64) inline constexpr uint8_t incomplete_max[64]{
      0xFF,     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
      0xFF,     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
      0xFF,     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
      0xFF,     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
      0xF0 - 1, // a 4 byte lead here needs 3 more bytes
      0xE0 - 1, // a 3 byte lead here needs 2 more
      0xC0 - 1 // any lead here needs at least 1 more
   };

   // ---------------------------------------------------------------------------------------------
   // Backend primitives. Each provides: vec, width, load, zero, set1, and_, or_, xor_, shr4, subs,
   // lookup16 (16 entry byte table, replicated per 128 bit lane), is_zero, prev<N>, load_table,
   // load_incomplete_max.
   // ---------------------------------------------------------------------------------------------

#if defined(GLZ_UTF8_GENERIC_WIDTH)
   // Testing hook. Defining GLZ_UTF8_GENERIC_WIDTH to 16, 32, or 64 selects a portable backend that
   // implements each primitive in plain C++ per its documented semantics. It exercises the
   // width-generic algorithm at register sizes the host may not be able to execute, so the driver
   // loop, the incomplete_max offset arithmetic, and the checker can be differentially tested at
   // every width. Never selected by a normal build.
   struct vec
   {
      uint8_t b[GLZ_UTF8_GENERIC_WIDTH]{};
   };
   inline constexpr size_t width = GLZ_UTF8_GENERIC_WIDTH;
   static_assert(width == 16 || width == 32 || width == 64, "generic width must be 16, 32, or 64");
   // Named apart from the native backends of the same width, so a build that meant to exercise the
   // portable path cannot mistake a native one for it. Width alone is ambiguous: AVX2 and generic32
   // both report 32.
   inline constexpr std::string_view backend = width == 64 ? "generic64" : (width == 32 ? "generic32" : "generic16");

   GLZ_ALWAYS_INLINE vec load(const uint8_t* p) noexcept
   {
      vec r;
      std::memcpy(r.b, p, width);
      return r;
   }
   GLZ_ALWAYS_INLINE vec zero() noexcept { return vec{}; }
   GLZ_ALWAYS_INLINE vec set1(uint8_t v) noexcept
   {
      vec r;
      std::memset(r.b, v, width);
      return r;
   }
#define GLZ_UTF8_GENERIC_BINOP(name, op)                            \
   GLZ_ALWAYS_INLINE vec name(vec a, vec c) noexcept                \
   {                                                                \
      vec r;                                                        \
      for (size_t i = 0; i < width; ++i) r.b[i] = a.b[i] op c.b[i]; \
      return r;                                                     \
   }
   GLZ_UTF8_GENERIC_BINOP(and_, &)
   GLZ_UTF8_GENERIC_BINOP(or_, |)
   GLZ_UTF8_GENERIC_BINOP(xor_, ^)
#undef GLZ_UTF8_GENERIC_BINOP
   GLZ_ALWAYS_INLINE vec shr4(vec a) noexcept
   {
      vec r;
      for (size_t i = 0; i < width; ++i) r.b[i] = uint8_t(a.b[i] >> 4);
      return r;
   }
   GLZ_ALWAYS_INLINE vec subs(vec a, vec c) noexcept
   {
      vec r;
      for (size_t i = 0; i < width; ++i) r.b[i] = a.b[i] > c.b[i] ? uint8_t(a.b[i] - c.b[i]) : uint8_t(0);
      return r;
   }
   // Mirrors pshufb / vqtbl1q / swizzle: index >= 16 (high bit set) yields 0. The table repeats
   // every 128 bit lane, so indexing is within lane.
   GLZ_ALWAYS_INLINE vec lookup16(vec table, vec idx) noexcept
   {
      vec r;
      for (size_t i = 0; i < width; ++i) {
         const uint8_t x = idx.b[i];
         r.b[i] = (x & 0x80) ? 0 : table.b[(i & ~size_t(15)) + (x & 0x0F)];
      }
      return r;
   }
   GLZ_ALWAYS_INLINE bool is_zero(vec a) noexcept
   {
      for (size_t i = 0; i < width; ++i)
         if (a.b[i]) return false;
      return true;
   }
   GLZ_ALWAYS_INLINE vec load_table(const uint8_t* p) noexcept
   {
      vec r;
      for (size_t i = 0; i < width; ++i) r.b[i] = p[i & 15];
      return r;
   }
   GLZ_ALWAYS_INLINE vec load_incomplete_max() noexcept { return load(incomplete_max + (64 - width)); }
   // The byte stream shifted right by N: the last N bytes of prv followed by the first width-N of cur.
   template <int N>
   GLZ_ALWAYS_INLINE vec prev(vec cur, vec prv) noexcept
   {
      vec r;
      for (size_t i = 0; i < width; ++i) {
         r.b[i] = (i < size_t(N)) ? prv.b[width - N + i] : cur.b[i - N];
      }
      return r;
   }

#elif defined(GLZ_USE_AVX512BW)
   using vec = __m512i;
   inline constexpr size_t width = 64;
   inline constexpr std::string_view backend = "AVX512BW";

   GLZ_ALWAYS_INLINE vec load(const uint8_t* p) noexcept { return _mm512_loadu_si512(p); }
   GLZ_ALWAYS_INLINE vec zero() noexcept { return _mm512_setzero_si512(); }
   GLZ_ALWAYS_INLINE vec set1(uint8_t v) noexcept { return _mm512_set1_epi8(static_cast<char>(v)); }
   GLZ_ALWAYS_INLINE vec and_(vec a, vec b) noexcept { return _mm512_and_si512(a, b); }
   GLZ_ALWAYS_INLINE vec or_(vec a, vec b) noexcept { return _mm512_or_si512(a, b); }
   GLZ_ALWAYS_INLINE vec xor_(vec a, vec b) noexcept { return _mm512_xor_si512(a, b); }
   GLZ_ALWAYS_INLINE vec shr4(vec a) noexcept
   {
      return _mm512_and_si512(_mm512_srli_epi16(a, 4), _mm512_set1_epi8(0x0F));
   }
   GLZ_ALWAYS_INLINE vec subs(vec a, vec b) noexcept { return _mm512_subs_epu8(a, b); }
   GLZ_ALWAYS_INLINE vec lookup16(vec table, vec idx) noexcept { return _mm512_shuffle_epi8(table, idx); }
   GLZ_ALWAYS_INLINE bool is_zero(vec a) noexcept { return _mm512_test_epi8_mask(a, a) == 0; }
   GLZ_ALWAYS_INLINE vec load_table(const uint8_t* p) noexcept
   {
      return _mm512_broadcast_i32x4(_mm_loadu_si128(reinterpret_cast<const __m128i*>(p)));
   }
   GLZ_ALWAYS_INLINE vec load_incomplete_max() noexcept { return load(incomplete_max + (64 - width)); }
   // _mm512_alignr_epi8 concatenates within each 128 bit lane, so first gather, for every lane, the
   // 128 bits that precede it in the byte stream: prev's top lane followed by cur's lower three.
   template <int N>
   GLZ_ALWAYS_INLINE vec prev(vec cur, vec prv) noexcept
   {
      const __m512i idx = _mm512_set_epi64(13, 12, 11, 10, 9, 8, 7, 6);
      return _mm512_alignr_epi8(cur, _mm512_permutex2var_epi64(prv, idx, cur), 16 - N);
   }

#elif defined(GLZ_USE_AVX2)
   using vec = __m256i;
   inline constexpr size_t width = 32;
   inline constexpr std::string_view backend = "AVX2";

   GLZ_ALWAYS_INLINE vec load(const uint8_t* p) noexcept
   {
      return _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p));
   }
   GLZ_ALWAYS_INLINE vec zero() noexcept { return _mm256_setzero_si256(); }
   GLZ_ALWAYS_INLINE vec set1(uint8_t v) noexcept { return _mm256_set1_epi8(static_cast<char>(v)); }
   GLZ_ALWAYS_INLINE vec and_(vec a, vec b) noexcept { return _mm256_and_si256(a, b); }
   GLZ_ALWAYS_INLINE vec or_(vec a, vec b) noexcept { return _mm256_or_si256(a, b); }
   GLZ_ALWAYS_INLINE vec xor_(vec a, vec b) noexcept { return _mm256_xor_si256(a, b); }
   GLZ_ALWAYS_INLINE vec shr4(vec a) noexcept
   {
      return _mm256_and_si256(_mm256_srli_epi16(a, 4), _mm256_set1_epi8(0x0F));
   }
   GLZ_ALWAYS_INLINE vec subs(vec a, vec b) noexcept { return _mm256_subs_epu8(a, b); }
   GLZ_ALWAYS_INLINE vec lookup16(vec table, vec idx) noexcept { return _mm256_shuffle_epi8(table, idx); }
   GLZ_ALWAYS_INLINE bool is_zero(vec a) noexcept { return _mm256_testz_si256(a, a) != 0; }
   GLZ_ALWAYS_INLINE vec load_table(const uint8_t* p) noexcept
   {
      return _mm256_broadcastsi128_si256(_mm_loadu_si128(reinterpret_cast<const __m128i*>(p)));
   }
   GLZ_ALWAYS_INLINE vec load_incomplete_max() noexcept { return load(incomplete_max + (64 - width)); }
   // permute2x128 with 0x21 builds [prev.hi, cur.lo], the 128 bit lane preceding each of cur's.
   template <int N>
   GLZ_ALWAYS_INLINE vec prev(vec cur, vec prv) noexcept
   {
      return _mm256_alignr_epi8(cur, _mm256_permute2x128_si256(prv, cur, 0x21), 16 - N);
   }

#elif defined(GLZ_USE_SSSE3)
   using vec = __m128i;
   inline constexpr size_t width = 16;
   inline constexpr std::string_view backend = "SSSE3";

   GLZ_ALWAYS_INLINE vec load(const uint8_t* p) noexcept
   {
      return _mm_loadu_si128(reinterpret_cast<const __m128i*>(p));
   }
   GLZ_ALWAYS_INLINE vec zero() noexcept { return _mm_setzero_si128(); }
   GLZ_ALWAYS_INLINE vec set1(uint8_t v) noexcept { return _mm_set1_epi8(static_cast<char>(v)); }
   GLZ_ALWAYS_INLINE vec and_(vec a, vec b) noexcept { return _mm_and_si128(a, b); }
   GLZ_ALWAYS_INLINE vec or_(vec a, vec b) noexcept { return _mm_or_si128(a, b); }
   GLZ_ALWAYS_INLINE vec xor_(vec a, vec b) noexcept { return _mm_xor_si128(a, b); }
   GLZ_ALWAYS_INLINE vec shr4(vec a) noexcept { return _mm_and_si128(_mm_srli_epi16(a, 4), _mm_set1_epi8(0x0F)); }
   GLZ_ALWAYS_INLINE vec subs(vec a, vec b) noexcept { return _mm_subs_epu8(a, b); }
   GLZ_ALWAYS_INLINE vec lookup16(vec table, vec idx) noexcept { return _mm_shuffle_epi8(table, idx); }
   GLZ_ALWAYS_INLINE bool is_zero(vec a) noexcept
   {
      return _mm_movemask_epi8(_mm_cmpeq_epi8(a, _mm_setzero_si128())) == 0xFFFF;
   }
   GLZ_ALWAYS_INLINE vec load_table(const uint8_t* p) noexcept { return load(p); }
   GLZ_ALWAYS_INLINE vec load_incomplete_max() noexcept { return load(incomplete_max + (64 - width)); }
   template <int N>
   GLZ_ALWAYS_INLINE vec prev(vec cur, vec prv) noexcept
   {
      return _mm_alignr_epi8(cur, prv, 16 - N);
   }

#elif defined(GLZ_USE_NEON64)
   using vec = uint8x16_t;
   inline constexpr size_t width = 16;
   inline constexpr std::string_view backend = "NEON64";

   GLZ_ALWAYS_INLINE vec load(const uint8_t* p) noexcept { return vld1q_u8(p); }
   GLZ_ALWAYS_INLINE vec zero() noexcept { return vdupq_n_u8(0); }
   GLZ_ALWAYS_INLINE vec set1(uint8_t v) noexcept { return vdupq_n_u8(v); }
   GLZ_ALWAYS_INLINE vec and_(vec a, vec b) noexcept { return vandq_u8(a, b); }
   GLZ_ALWAYS_INLINE vec or_(vec a, vec b) noexcept { return vorrq_u8(a, b); }
   GLZ_ALWAYS_INLINE vec xor_(vec a, vec b) noexcept { return veorq_u8(a, b); }
   GLZ_ALWAYS_INLINE vec shr4(vec a) noexcept { return vshrq_n_u8(a, 4); }
   GLZ_ALWAYS_INLINE vec subs(vec a, vec b) noexcept { return vqsubq_u8(a, b); }
   GLZ_ALWAYS_INLINE vec lookup16(vec table, vec idx) noexcept { return vqtbl1q_u8(table, idx); }
   GLZ_ALWAYS_INLINE bool is_zero(vec a) noexcept { return vmaxvq_u8(a) == 0; }
   GLZ_ALWAYS_INLINE vec load_table(const uint8_t* p) noexcept { return load(p); }
   GLZ_ALWAYS_INLINE vec load_incomplete_max() noexcept { return load(incomplete_max + (64 - width)); }
   template <int N>
   GLZ_ALWAYS_INLINE vec prev(vec cur, vec prv) noexcept
   {
      return vextq_u8(prv, cur, 16 - N);
   }

#elif defined(GLZ_USE_WASM_SIMD128)
   using vec = v128_t;
   inline constexpr size_t width = 16;
   inline constexpr std::string_view backend = "WASM_SIMD128";

   GLZ_ALWAYS_INLINE vec load(const uint8_t* p) noexcept { return wasm_v128_load(p); }
   GLZ_ALWAYS_INLINE vec zero() noexcept { return wasm_i8x16_splat(0); }
   GLZ_ALWAYS_INLINE vec set1(uint8_t v) noexcept { return wasm_i8x16_splat(static_cast<int8_t>(v)); }
   GLZ_ALWAYS_INLINE vec and_(vec a, vec b) noexcept { return wasm_v128_and(a, b); }
   GLZ_ALWAYS_INLINE vec or_(vec a, vec b) noexcept { return wasm_v128_or(a, b); }
   GLZ_ALWAYS_INLINE vec xor_(vec a, vec b) noexcept { return wasm_v128_xor(a, b); }
   GLZ_ALWAYS_INLINE vec shr4(vec a) noexcept { return wasm_u8x16_shr(a, 4); }
   GLZ_ALWAYS_INLINE vec subs(vec a, vec b) noexcept { return wasm_u8x16_sub_sat(a, b); }
   // swizzle yields 0 for any index >= 16, matching pshufb's handling of out of range indices.
   GLZ_ALWAYS_INLINE vec lookup16(vec table, vec idx) noexcept { return wasm_i8x16_swizzle(table, idx); }
   GLZ_ALWAYS_INLINE bool is_zero(vec a) noexcept { return !wasm_v128_any_true(a); }
   GLZ_ALWAYS_INLINE vec load_table(const uint8_t* p) noexcept { return load(p); }
   GLZ_ALWAYS_INLINE vec load_incomplete_max() noexcept { return load(incomplete_max + (64 - width)); }
   // No alignr; wasm_i8x16_shuffle selects lanes 0..15 from prv and 16..31 from cur.
   template <int N>
   GLZ_ALWAYS_INLINE vec prev(vec cur, vec prv) noexcept
   {
      return wasm_i8x16_shuffle(prv, cur, 16 - N, 17 - N, 18 - N, 19 - N, 20 - N, 21 - N, 22 - N, 23 - N, 24 - N,
                                25 - N, 26 - N, 27 - N, 28 - N, 29 - N, 30 - N, 31 - N);
   }
#endif

   // Anchors the width each branch declares to the register type it picked alongside it. Everything
   // downstream trusts `width` -- regs_per_step, the tail handling, the incomplete_max offset -- so
   // a branch that names a width its vec does not have corrupts validation rather than failing to
   // build. The name/width pairing is checked separately, in utf8_validation_test.cpp.
   static_assert(sizeof(vec) == width, "a UTF-8 backend declared a width its register type does not have");

   // Number of registers consumed per 64 byte step of the main loop.
   inline constexpr size_t regs_per_step = 64 / width;

   struct checker
   {
      vec error_{zero()};
      vec prev_input_{zero()};
      vec prev_incomplete_{zero()};
      const vec tbl_1_high_{load_table(byte_1_high)};
      const vec tbl_1_low_{load_table(byte_1_low)};
      const vec tbl_2_high_{load_table(byte_2_high)};
      const vec incomplete_max_{load_incomplete_max()};
      const vec nibble_mask_{set1(0x0F)};

      // Three table lookups whose AND is non-zero exactly when (prev1, input) is an illegal pair.
      GLZ_ALWAYS_INLINE vec check_special_cases(vec input, vec prev1) const noexcept
      {
         return and_(and_(lookup16(tbl_1_high_, shr4(prev1)), lookup16(tbl_1_low_, and_(prev1, nibble_mask_))),
                     lookup16(tbl_2_high_, shr4(input)));
      }

      // A byte 2 or 3 positions after a 3 or 4 byte lead must be a continuation. Subtracting
      // 0xE0-0x80 / 0xF0-0x80 leaves the high bit set exactly where that obligation exists, so
      // XOR against the lookup result cancels the legitimate cases and leaves real errors.
      GLZ_ALWAYS_INLINE vec check_multibyte_lengths(vec input, vec prv, vec sc) const noexcept
      {
         const vec prev2 = prev<2>(input, prv);
         const vec prev3 = prev<3>(input, prv);
         const vec must23 = or_(subs(prev2, set1(0xE0 - 0x80)), subs(prev3, set1(0xF0 - 0x80)));
         return xor_(and_(must23, set1(0x80)), sc);
      }

      GLZ_ALWAYS_INLINE void check_block(vec input) noexcept
      {
         const vec prev1 = prev<1>(input, prev_input_);
         const vec sc = check_special_cases(input, prev1);
         error_ = or_(error_, check_multibyte_lengths(input, prev_input_, sc));
         prev_input_ = input;
         prev_incomplete_ = subs(input, incomplete_max_);
      }

      // An all-ASCII step cannot contain an error, but it must still close out any sequence the
      // previous step left open.
      GLZ_ALWAYS_INLINE void check_ascii_step(vec last) noexcept
      {
         error_ = or_(error_, prev_incomplete_);
         prev_input_ = last;
         prev_incomplete_ = zero();
      }

      [[nodiscard]] GLZ_ALWAYS_INLINE bool has_error() const noexcept
      {
         return !is_zero(or_(error_, prev_incomplete_));
      }
   };

   inline bool validate(const uint8_t* it, const uint8_t* end) noexcept
   {
      checker c{};
      const vec high_bits = set1(0x80);

      // 64 bytes per step, with one ASCII test covering every register in the step.
      while (static_cast<size_t>(end - it) >= 64) {
         vec b[regs_per_step];
         for (size_t i = 0; i < regs_per_step; ++i) {
            b[i] = load(it + i * width);
         }
         vec any = b[0];
         for (size_t i = 1; i < regs_per_step; ++i) {
            any = or_(any, b[i]);
         }

         if (is_zero(and_(any, high_bits))) {
            c.check_ascii_step(b[regs_per_step - 1]);
         }
         else {
            for (size_t i = 0; i < regs_per_step; ++i) {
               c.check_block(b[i]);
            }
         }
         it += 64;
      }

      while (static_cast<size_t>(end - it) >= width) {
         c.check_block(load(it));
         it += width;
      }

      if (it < end) {
         // Pad the final partial register with ASCII so a truncated sequence at the true end of the
         // input reports "too short" rather than being completed by whatever happens to follow.
         alignas(64) uint8_t tail[width];
         std::memset(tail, ' ', sizeof(tail));
         std::memcpy(tail, it, static_cast<size_t>(end - it));
         c.check_block(load(tail));
      }

      return !c.has_error();
   }
}

#else

namespace glz::detail::utf8_simd
{
   // No vector path on this target, so validation runs the scalar validator in parse.hpp. Declared
   // here so the tests and the simd-backends workflow can name the fallback the same way they name
   // a backend, without a second spelling for "there isn't one".
   inline constexpr std::string_view backend = "scalar";
}

#endif
