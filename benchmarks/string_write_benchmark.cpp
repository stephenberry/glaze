// String write benchmark: SWAR-only (GLZ_DISABLE_SIMD) vs SIMD+SWAR
// Both paths use glz::write_json for a fair comparison — the only difference
// is whether SIMD intrinsics (NEON/SSE2/AVX2) are compiled in.
// Validates correctness across edge cases and measures performance.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>

#include "bencher/bencher.hpp"
#include "bencher/diagnostics.hpp"
#include "glaze/json.hpp"
#include "string_write_no_simd.hpp"

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

   std::string swar_result;
   no_simd::write_json_string(input, swar_result);

   std::string simd_result;
   simd_result.clear();
   (void)glz::write_json(input, simd_result);

   if (swar_result != simd_result) {
      std::fprintf(stderr, "MISMATCH in '%s' (input len %zu):\n", name, input.size());
      std::fprintf(stderr, "  SWAR (%zu bytes): %.200s%s\n", swar_result.size(), swar_result.c_str(),
                   swar_result.size() > 200 ? "..." : "");
      std::fprintf(stderr, "  SIMD (%zu bytes): %.200s%s\n", simd_result.size(), simd_result.c_str(),
                   simd_result.size() > 200 ? "..." : "");

      // Find first difference
      for (size_t i = 0; i < std::min(swar_result.size(), simd_result.size()); ++i) {
         if (swar_result[i] != simd_result[i]) {
            std::fprintf(stderr, "  First diff at byte %zu: SWAR=0x%02X SIMD=0x%02X\n", i,
                         (unsigned char)swar_result[i], (unsigned char)simd_result[i]);
            break;
         }
      }
      if (swar_result.size() != simd_result.size()) {
         std::fprintf(stderr, "  Length difference: SWAR=%zu SIMD=%zu\n", swar_result.size(), simd_result.size());
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
   std::string swar_buf, simd_buf;
   no_simd::write_json_string(input, swar_buf);
   (void)glz::write_json(input, simd_buf);

   std::printf("%s — SWAR: %zu bytes, SIMD: %zu bytes\n", name, swar_buf.size(), simd_buf.size());

   bencher::stage stage;
   stage.name = name;

   stage.run("SWAR-only", [&] {
      std::string buf;
      no_simd::write_json_string(input, buf);
      bencher::do_not_optimize(buf.data());
      return buf.size();
   });

   stage.run("SIMD+SWAR", [&] {
      std::string buf;
      (void)glz::write_json(input, buf);
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
