// String write benchmark: old (SWAR-only) vs new (NEON/SSE2/AVX2 direct comparison) approach
// Validates correctness across edge cases and measures performance.

#include "bencher/bencher.hpp"
#include "bencher/diagnostics.hpp"

#include "glaze/json.hpp"

#include <bit>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>

// ============================================================================
// Old string serialization (SWAR 8-byte only, no 16-byte SIMD)
// This is the baseline before NEON/SSE2 and before AVX2 was changed to
// direct comparisons. We intentionally disable all SIMD paths to isolate
// the SWAR code that was previously the only path on ARM.
// ============================================================================

// Both old and new use glz::write_json for fair comparison.
// The old baseline uses raw_string mode (memcpy, no escaping/SIMD) to measure
// overhead, then we add SWAR-only processing separately.
// The new path is the library's current code (with NEON/SSE2/AVX2 optimizations).

namespace old_impl
{
   // Replicates the old SWAR-only inner loop with identical buffer management
   // to write_json (ensure_space, quote wrapping, etc.) for fair comparison.
   inline constexpr std::array<uint16_t, 256> char_escape_table = [] {
      auto combine = [](const char chars[2]) -> uint16_t {
         if constexpr (std::endian::native == std::endian::big) {
            return (uint16_t(uint8_t(chars[0])) << 8) | uint16_t(uint8_t(chars[1]));
         }
         else {
            return uint16_t(uint8_t(chars[0])) | (uint16_t(uint8_t(chars[1])) << 8);
         }
      };
      std::array<uint16_t, 256> t{};
      t['\b'] = combine(R"(\b)");
      t['\t'] = combine(R"(\t)");
      t['\n'] = combine(R"(\n)");
      t['\f'] = combine(R"(\f)");
      t['\r'] = combine(R"(\r)");
      t['\"'] = combine(R"(\")");
      t['\\'] = combine(R"(\\)");
      return t;
   }();

   consteval uint64_t repeat_byte8(const uint8_t repeat) { return 0x0101010101010101ull * repeat; }

   inline auto countr_zero(const uint64_t x) noexcept { return std::countr_zero(x); }

   // Matches glz::write_json buffer management exactly, but uses SWAR-only inner loop
   __attribute__((noinline)) void write_string_old(const std::string& str, std::string& out)
   {
      out.clear();
      const auto n = str.size();

      // Match write_json's ensure_space: ix + 10 + 2*n, resize to 2x if needed
      const size_t required = 10 + 2 * n;
      if (required > out.size()) {
         out.resize(2 * required);
      }

      size_t ix = 0;
      std::memcpy(&out[ix], "\"", 1);
      ++ix;

      const auto* c = str.data();
      const auto* const e = c + n;
      const auto start = &out[ix];
      auto data = start;

      // SWAR 8-byte path only (the old code path used on ARM before NEON)
      if (n > 7) {
         for (const auto end_m7 = e - 7; c < end_m7;) {
            std::memcpy(data, c, 8);
            uint64_t swar;
            std::memcpy(&swar, c, 8);
            if constexpr (std::endian::native == std::endian::big) {
               swar = std::byteswap(swar);
            }

            constexpr uint64_t lo7_mask = repeat_byte8(0b01111111);
            const uint64_t lo7 = swar & lo7_mask;
            const uint64_t quote = (lo7 ^ repeat_byte8('"')) + lo7_mask;
            const uint64_t backslash = (lo7 ^ repeat_byte8('\\')) + lo7_mask;
            const uint64_t less_32 = (swar & repeat_byte8(0b01100000)) + lo7_mask;
            uint64_t next = ~((quote & backslash & less_32) | swar);

            next &= repeat_byte8(0b10000000);
            if (next == 0) {
               data += 8;
               c += 8;
               continue;
            }

            const auto length = (countr_zero(next) >> 3);
            c += length;
            data += length;

            std::memcpy(data, &char_escape_table[uint8_t(*c)], 2);
            data += 2;
            ++c;
         }
      }

      // Scalar tail
      for (; c < e; ++c) {
         if (const auto escaped = char_escape_table[uint8_t(*c)]; escaped) {
            std::memcpy(data, &escaped, 2);
            data += 2;
         }
         else {
            std::memcpy(data, c, 1);
            ++data;
         }
      }

      ix += size_t(data - start);
      std::memcpy(&out[ix], "\"", 1);
      ++ix;
      out.resize(ix);
   }
} // namespace old_impl

// ============================================================================
// New string serialization: uses the library's current write_json
// (with NEON/SSE2/AVX2 optimizations)
// ============================================================================

namespace new_impl
{
   __attribute__((noinline)) void write_string_new(const std::string& str, std::string& out)
   {
      out.clear();
      (void)glz::write_json(str, out);
   }
} // namespace new_impl

// ============================================================================
// String generators
// ============================================================================

// Pure ASCII lowercase, no escaping needed
static std::string gen_ascii(size_t len, uint64_t seed = 42)
{
   std::mt19937 rng(seed);
   std::uniform_int_distribution<int> dist('a', 'z');
   std::string s;
   s.reserve(len);
   for (size_t i = 0; i < len; ++i) {
      s += static_cast<char>(dist(rng));
   }
   return s;
}

// ASCII with ~15% characters that need escaping
static std::string gen_escaped(size_t len, uint64_t seed = 42)
{
   std::mt19937 rng(seed);
   std::uniform_int_distribution<int> dist(0, 99);
   std::string s;
   s.reserve(len);
   for (size_t i = 0; i < len; ++i) {
      int r = dist(rng);
      if (r < 5)
         s += '"';
      else if (r < 10)
         s += '\\';
      else if (r < 13)
         s += '\n';
      else if (r < 15)
         s += '\t';
      else
         s += static_cast<char>('a' + (r % 26));
   }
   return s;
}

// UTF-8 multibyte strings (2-4 byte sequences, no escaping needed)
static std::string gen_utf8(size_t approx_len, uint64_t seed = 42)
{
   std::mt19937 rng(seed);
   std::uniform_int_distribution<int> dist(0, 3);
   std::string s;
   s.reserve(approx_len * 2);
   while (s.size() < approx_len) {
      switch (dist(rng)) {
      case 0: // ASCII
         s += static_cast<char>('a' + (rng() % 26));
         break;
      case 1: // 2-byte UTF-8 (U+0080 - U+07FF)
         s += static_cast<char>(0xC0 | (rng() % 32));
         s += static_cast<char>(0x80 | (rng() % 64));
         break;
      case 2: // 3-byte UTF-8 (U+0800 - U+FFFF, avoiding surrogates)
         s += static_cast<char>(0xE0 | (rng() % 16));
         s += static_cast<char>(0x80 | (rng() % 64));
         s += static_cast<char>(0x80 | (rng() % 64));
         break;
      case 3: // 4-byte UTF-8 (U+10000 - U+10FFFF)
         s += static_cast<char>(0xF0 | (rng() % 8));
         s += static_cast<char>(0x80 | (rng() % 64));
         s += static_cast<char>(0x80 | (rng() % 64));
         s += static_cast<char>(0x80 | (rng() % 64));
         break;
      }
   }
   return s;
}

// String with all control characters (0x00-0x1F)
static std::string gen_all_control_chars()
{
   std::string s;
   for (int i = 0; i < 32; ++i) {
      s += static_cast<char>(i);
   }
   // Repeat to make it longer
   std::string result;
   for (int i = 0; i < 16; ++i) {
      result += s;
   }
   return result;
}

// Edge case: string with byte 0x1F (unit separator) — boundary of control char range
static std::string gen_boundary_0x1f(size_t len)
{
   std::string s(len, 'x');
   // Place 0x1F at various positions
   for (size_t i = 0; i < len; i += 7) {
      s[i] = 0x1F;
   }
   return s;
}

// Edge case: string with bytes at 0x20 boundary (0x1F and 0x20 alternating)
static std::string gen_boundary_alternating(size_t len)
{
   std::string s;
   s.reserve(len);
   for (size_t i = 0; i < len; ++i) {
      s += (i % 2 == 0) ? char(0x1F) : char(0x20);
   }
   return s;
}

// High bytes (0x80-0xFF) — should NOT be escaped
static std::string gen_high_bytes(size_t len, uint64_t seed = 42)
{
   std::mt19937 rng(seed);
   std::uniform_int_distribution<int> dist(0x80, 0xFF);
   std::string s;
   s.reserve(len);
   for (size_t i = 0; i < len; ++i) {
      s += static_cast<char>(dist(rng));
   }
   return s;
}

// ============================================================================
// Correctness verification
// ============================================================================

static int verify_count = 0;
static int verify_pass = 0;

static void verify(const char* name, const std::string& input)
{
   ++verify_count;

   std::string old_result;
   old_impl::write_string_old(input, old_result);

   std::string new_result;
   new_impl::write_string_new(input, new_result);

   if (old_result != new_result) {
      std::fprintf(stderr, "MISMATCH in '%s' (input len %zu):\n", name, input.size());
      std::fprintf(stderr, "  Old (%zu bytes): %.200s%s\n", old_result.size(), old_result.c_str(),
                   old_result.size() > 200 ? "..." : "");
      std::fprintf(stderr, "  New (%zu bytes): %.200s%s\n", new_result.size(), new_result.c_str(),
                   new_result.size() > 200 ? "..." : "");

      // Find first difference
      for (size_t i = 0; i < std::min(old_result.size(), new_result.size()); ++i) {
         if (old_result[i] != new_result[i]) {
            std::fprintf(stderr, "  First diff at byte %zu: old=0x%02X new=0x%02X\n", i,
                         (unsigned char)old_result[i], (unsigned char)new_result[i]);
            break;
         }
      }
      if (old_result.size() != new_result.size()) {
         std::fprintf(stderr, "  Length difference: old=%zu new=%zu\n", old_result.size(), new_result.size());
      }
   }
   else {
      ++verify_pass;
   }
}

static void run_correctness_checks()
{
   std::printf("=== Correctness Verification ===\n\n");

   // Empty string
   verify("empty", "");

   // Single characters
   verify("single 'a'", "a");
   verify("single quote", "\"");
   verify("single backslash", "\\");
   verify("single newline", "\n");
   verify("single tab", "\t");
   verify("single null", std::string(1, '\0'));
   verify("single 0x1F", std::string(1, '\x1F'));
   verify("single 0x20 (space)", " ");
   verify("single 0x7F (DEL)", std::string(1, '\x7F'));
   verify("single 0x80", std::string(1, '\x80'));
   verify("single 0xFF", std::string(1, '\xFF'));

   // Pure ASCII, various sizes (tests SWAR, SSE2/NEON, AVX2 paths)
   for (size_t len : {1, 2, 3, 7, 8, 9, 15, 16, 17, 31, 32, 33, 63, 64, 100, 255, 256, 1000, 4096, 16384}) {
      char label[64];
      std::snprintf(label, sizeof(label), "ASCII %zu", len);
      verify(label, gen_ascii(len));
   }

   // Escaped strings, various sizes
   for (size_t len : {1, 7, 8, 15, 16, 31, 32, 64, 256, 1000, 4096}) {
      char label[64];
      std::snprintf(label, sizeof(label), "escaped %zu", len);
      verify(label, gen_escaped(len));
   }

   // UTF-8 multibyte (bytes >= 0x80 must NOT be escaped)
   for (size_t len : {16, 64, 256, 1024, 4096}) {
      char label[64];
      std::snprintf(label, sizeof(label), "utf8 %zu", len);
      verify(label, gen_utf8(len));
   }

   // All control characters
   verify("all_control_chars", gen_all_control_chars());

   // Boundary 0x1F at various positions
   for (size_t len : {7, 8, 15, 16, 31, 32, 64, 256}) {
      char label[64];
      std::snprintf(label, sizeof(label), "boundary_0x1F %zu", len);
      verify(label, gen_boundary_0x1f(len));
   }

   // Alternating 0x1F/0x20
   for (size_t len : {8, 16, 32, 64, 256}) {
      char label[64];
      std::snprintf(label, sizeof(label), "alternating_0x1F_0x20 %zu", len);
      verify(label, gen_boundary_alternating(len));
   }

   // High bytes (0x80-0xFF) — must NOT be escaped
   for (size_t len : {8, 16, 32, 64, 256, 1024}) {
      char label[64];
      std::snprintf(label, sizeof(label), "high_bytes %zu", len);
      verify(label, gen_high_bytes(len));
   }

   // String with escapes at exact SIMD boundary positions
   for (size_t boundary : {8, 16, 32}) {
      for (int offset = -1; offset <= 1; ++offset) {
         size_t pos = boundary + offset;
         if (pos == 0) continue;
         std::string s(pos + 10, 'a');
         s[pos] = '"'; // Place escapable char at boundary
         char label[64];
         std::snprintf(label, sizeof(label), "escape_at_%zu", pos);
         verify(label, s);
      }
   }

   // String of all identical escapable chars
   verify("all_quotes_32", std::string(32, '"'));
   verify("all_backslash_32", std::string(32, '\\'));
   verify("all_newline_32", std::string(32, '\n'));

   // All printable ASCII
   {
      std::string printable;
      for (int i = 0x20; i < 0x7F; ++i) {
         printable += static_cast<char>(i);
      }
      verify("all_printable_ascii", printable);
   }

   // Full byte range 0x00-0xFF
   {
      std::string full_range;
      for (int i = 0; i < 256; ++i) {
         full_range += static_cast<char>(i);
      }
      verify("full_byte_range", full_range);
   }

   std::printf("  %d / %d tests passed\n\n", verify_pass, verify_count);
   if (verify_pass != verify_count) {
      std::fprintf(stderr, "CORRECTNESS FAILURE: %d tests failed! Aborting.\n", verify_count - verify_pass);
      std::abort();
   }
}

// ============================================================================
// Benchmarks
// ============================================================================

static void bench_string(const char* name, const std::string& input)
{
   // Warmup and verify sizes match
   std::string old_buf, new_buf;
   old_impl::write_string_old(input, old_buf);
   new_impl::write_string_new(input, new_buf);

   std::printf("%s — old: %zu bytes, new: %zu bytes\n", name, old_buf.size(), new_buf.size());

   bencher::stage stage;
   stage.name = name;

   stage.run("old (SWAR-only)", [&] {
      std::string buf;
      old_impl::write_string_old(input, buf);
      bencher::do_not_optimize(buf.data());
      return buf.size();
   });

   stage.run("new (SIMD+SWAR)", [&] {
      std::string buf;
      new_impl::write_string_new(input, buf);
      bencher::do_not_optimize(buf.data());
      return buf.size();
   });

   bencher::print_results(stage);
}

int main()
{
   // ========================================================================
   // Phase 1: Correctness — must pass before any performance measurements
   // ========================================================================
   run_correctness_checks();

   // ========================================================================
   // Phase 2: Performance
   // ========================================================================

   // --- Pure ASCII (no escaping) ---
   std::printf("=== Pure ASCII (no escaping needed) ===\n\n");
   for (size_t len : {16, 64, 256, 1024, 4096, 16384}) {
      char label[128];
      std::snprintf(label, sizeof(label), "ASCII %zu bytes", len);
      bench_string(label, gen_ascii(len));
   }

   // --- ASCII with ~15% escapable characters ---
   std::printf("\n=== ~15%% escapable characters ===\n\n");
   for (size_t len : {16, 64, 256, 1024, 4096, 16384}) {
      char label[128];
      std::snprintf(label, sizeof(label), "Escaped %zu bytes", len);
      bench_string(label, gen_escaped(len));
   }

   // --- UTF-8 multibyte (no escaping needed, but high bytes) ---
   std::printf("\n=== UTF-8 multibyte (no escaping, bytes >= 0x80) ===\n\n");
   for (size_t len : {64, 256, 1024, 4096}) {
      char label[128];
      std::snprintf(label, sizeof(label), "UTF-8 ~%zu bytes", len);
      bench_string(label, gen_utf8(len));
   }

   // --- High bytes only (0x80-0xFF, no escaping) ---
   std::printf("\n=== High bytes only (0x80-0xFF, no escaping) ===\n\n");
   for (size_t len : {64, 256, 1024, 4096}) {
      char label[128];
      std::snprintf(label, sizeof(label), "High bytes %zu", len);
      bench_string(label, gen_high_bytes(len));
   }

   // --- Control characters (all need escaping) ---
   std::printf("\n=== Control characters (all need escaping) ===\n\n");
   bench_string("All control chars (512 bytes)", gen_all_control_chars());

   // --- Boundary cases ---
   std::printf("\n=== Boundary 0x1F (every 7th byte) ===\n\n");
   for (size_t len : {64, 256, 1024}) {
      char label[128];
      std::snprintf(label, sizeof(label), "0x1F boundary %zu bytes", len);
      bench_string(label, gen_boundary_0x1f(len));
   }

   std::printf("\n=== Alternating 0x1F/0x20 ===\n\n");
   for (size_t len : {64, 256, 1024}) {
      char label[128];
      std::snprintf(label, sizeof(label), "Alt 0x1F/0x20 %zu bytes", len);
      bench_string(label, gen_boundary_alternating(len));
   }

   return 0;
}
