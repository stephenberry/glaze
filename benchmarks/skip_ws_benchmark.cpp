// Benchmark: scalar skip_ws vs skip_matching_ws + skip_ws (combined, as used in the library)
//
// In the library, skip_matching_ws is called BEFORE skip_ws:
//   skip_matching_ws(ws_start, it, ws_size);  // try to fast-skip
//   skip_ws(it);                               // handle remainder or do full skip
//
// This benchmark compares:
//   Option A: just skip_ws (scalar table loop)
//   Option B: skip_matching_ws + skip_ws (the current library approach)
//
// Both must produce the same result. Correctness is verified.

#include "bencher/bencher.hpp"
#include "bencher/diagnostics.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// BENCH_ALWAYS_INLINE is provided by bencher/config.hpp

// ============================================================================
// Scalar skip_ws (lookup table, 1 byte at a time)
// ============================================================================
namespace scalar
{
   inline constexpr std::array<bool, 256> whitespace_table = [] {
      std::array<bool, 256> t{};
      t['\n'] = true;
      t['\t'] = true;
      t['\r'] = true;
      t[' '] = true;
      return t;
   }();

   BENCH_ALWAYS_INLINE void skip_ws(const char*& it) noexcept
   {
      while (whitespace_table[uint8_t(*it)]) {
         ++it;
      }
   }
}

// ============================================================================
// Current skip_matching_ws (manual 8/4/2-byte cascade) — copied from parse.hpp
// ============================================================================
namespace matching
{
   BENCH_ALWAYS_INLINE void skip_matching_ws(const char* ws, const char*& it, uint64_t length) noexcept
   {
      if (length > 7) {
         uint64_t v[2];
         while (length > 8) {
            std::memcpy(v, ws, 8);
            std::memcpy(v + 1, it, 8);
            if (v[0] != v[1]) {
               return;
            }
            length -= 8;
            ws += 8;
            it += 8;
         }

         const auto shift = 8 - length;
         ws -= shift;
         it -= shift;

         std::memcpy(v, ws, 8);
         std::memcpy(v + 1, it, 8);
         if (v[0] != v[1]) {
            return;
         }
         it += 8;
         return;
      }
      {
         constexpr uint64_t n{sizeof(uint32_t)};
         if (length >= n) {
            uint32_t v[2];
            std::memcpy(v, ws, n);
            std::memcpy(v + 1, it, n);
            if (v[0] != v[1]) {
               return;
            }
            length -= n;
            ws += n;
            it += n;
         }
      }
      {
         constexpr uint64_t n{sizeof(uint16_t)};
         if (length >= n) {
            uint16_t v[2];
            std::memcpy(v, ws, n);
            std::memcpy(v + 1, it, n);
            if (v[0] != v[1]) {
               return;
            }
            it += n;
         }
      }
   }
}

// ============================================================================
// Combined path (as used in the library): skip_matching_ws then skip_ws
// ============================================================================
namespace combined
{
   BENCH_ALWAYS_INLINE void skip(const char* ws_pattern, const char*& it, size_t ws_size) noexcept
   {
      matching::skip_matching_ws(ws_pattern, it, ws_size);
      scalar::skip_ws(it);
   }
}

// ============================================================================
// Test helpers
// ============================================================================

// Build a buffer of repeated [ws + delimiter] tokens
static std::string build_repeated(const std::string& ws, char delim, size_t count)
{
   std::string buf;
   const size_t token_size = ws.size() + 1;
   buf.reserve(token_size * count);
   for (size_t i = 0; i < count; ++i) {
      buf += ws;
      buf += delim;
   }
   return buf;
}

// Verify both approaches produce identical results on a buffer
static bool verify_correctness(const std::string& buf, const char* ws_pattern, size_t ws_size, const char* test_name)
{
   // Scalar-only
   const char* it_a = buf.data();
   const char* end = buf.data() + buf.size();
   size_t skips_a = 0;
   while (it_a < end) {
      scalar::skip_ws(it_a);
      if (it_a < end) ++it_a;
      ++skips_a;
   }

   // Combined (matching + scalar)
   const char* it_b = buf.data();
   size_t skips_b = 0;
   while (it_b < end) {
      combined::skip(ws_pattern, it_b, ws_size);
      if (it_b < end) ++it_b;
      ++skips_b;
   }

   if (it_a != it_b || skips_a != skips_b) {
      std::fprintf(stderr, "CORRECTNESS FAILURE in '%s': scalar ended at offset %td (%zu skips), combined at %td (%zu skips)\n",
                   test_name, it_a - buf.data(), skips_a, it_b - buf.data(), skips_b);
      return false;
   }
   return true;
}

// ============================================================================
// Benchmark runner
// ============================================================================

static void bench_fixed_pattern(const char* name, const std::string& ws, size_t count)
{
   const size_t ws_size = ws.size();
   const std::string buf = build_repeated(ws, '{', count);

   // Verify correctness first
   if (!verify_correctness(buf, ws.data(), ws_size, name)) {
      std::abort();
   }

   bencher::stage stage;
   stage.name = name;

   stage.run("scalar skip_ws only", [&] {
      const char* it = buf.data();
      const char* end = buf.data() + buf.size();
      while (it < end) {
         scalar::skip_ws(it);
         if (it < end) ++it;
      }
      bencher::do_not_optimize(it);
      return ws_size * count;
   });

   stage.run("matching + skip_ws", [&] {
      const char* it = buf.data();
      const char* end = buf.data() + buf.size();
      const char* ws_ptr = ws.data();
      while (it < end) {
         combined::skip(ws_ptr, it, ws_size);
         if (it < end) ++it;
      }
      bencher::do_not_optimize(it);
      return ws_size * count;
   });

   bencher::print_results(stage);
}

int main()
{
   constexpr size_t N = 500000;

   // ========================================================================
   // Part 1: Fixed-size patterns (best case for skip_matching_ws — always matches)
   // Common JSON indentation: \n followed by spaces
   // ========================================================================
   std::printf("=== Part 1: Fixed indent (all matching — best case for skip_matching_ws) ===\n\n");

   bench_fixed_pattern("\\n + 1 space (2B)", "\n ", N);
   bench_fixed_pattern("\\n + 2 spaces (3B)", "\n  ", N);
   bench_fixed_pattern("\\n + 3 spaces (4B)", "\n   ", N);
   bench_fixed_pattern("\\n + 4 spaces (5B)", "\n    ", N);
   bench_fixed_pattern("\\n + 5 spaces (6B)", "\n     ", N);
   bench_fixed_pattern("\\n + 6 spaces (7B)", "\n      ", N);
   bench_fixed_pattern("\\n + 7 spaces (8B)", "\n       ", N);
   bench_fixed_pattern("\\n + 8 spaces (9B)", "\n        ", N);
   bench_fixed_pattern("\\n + 12 spaces (13B)", "\n            ", N);
   bench_fixed_pattern("\\n + 16 spaces (17B)", "\n                ", N);

   // Tab-based indentation
   std::printf("\n=== Part 1b: Tab indentation (all matching) ===\n\n");

   bench_fixed_pattern("\\n + 1 tab (2B)", "\n\t", N);
   bench_fixed_pattern("\\n + 2 tabs (3B)", "\n\t\t", N);
   bench_fixed_pattern("\\n + 3 tabs (4B)", "\n\t\t\t", N);
   bench_fixed_pattern("\\n + 4 tabs (5B)", "\n\t\t\t\t", N);

   // ========================================================================
   // Part 2: Alternating indent depths (always mismatching — worst case for skip_matching_ws)
   // Simulates entering/exiting nested objects: indent toggles between two depths.
   // skip_matching_ws will always fail, adding pure overhead.
   // ========================================================================
   std::printf("\n=== Part 2: Alternating depths (always mismatching — worst case) ===\n\n");

   {
      // Alternate between 2-space indent and 4-space indent
      const std::string ws_a = "\n  ";   // 3B
      const std::string ws_b = "\n    "; // 5B

      std::string buf;
      buf.reserve((ws_a.size() + 1 + ws_b.size() + 1) * (N / 2));
      for (size_t i = 0; i < N / 2; ++i) {
         buf += ws_a;
         buf += '{';
         buf += ws_b;
         buf += '{';
      }

      // Verify correctness (using ws_a as the "recorded" pattern — ws_b will always mismatch)
      if (!verify_correctness(buf, ws_a.data(), ws_a.size(), "alternating 2sp/4sp")) {
         std::abort();
      }

      bencher::stage stage;
      stage.name = "Alternating \\n+2sp / \\n+4sp (50% mismatch)";

      stage.run("scalar skip_ws only", [&] {
         const char* it = buf.data();
         const char* end = buf.data() + buf.size();
         while (it < end) {
            scalar::skip_ws(it);
            if (it < end) ++it;
         }
         bencher::do_not_optimize(it);
         return buf.size();
      });

      stage.run("matching + skip_ws", [&] {
         const char* it = buf.data();
         const char* end = buf.data() + buf.size();
         const char* ws_ptr = ws_a.data();
         size_t ws_size = ws_a.size();
         while (it < end) {
            combined::skip(ws_ptr, it, ws_size);
            if (it < end) ++it;
         }
         bencher::do_not_optimize(it);
         return buf.size();
      });

      bencher::print_results(stage);
   }

   {
      // Alternate between 4-space and 8-space indent
      const std::string ws_a = "\n    ";     // 5B
      const std::string ws_b = "\n        "; // 9B

      std::string buf;
      buf.reserve((ws_a.size() + 1 + ws_b.size() + 1) * (N / 2));
      for (size_t i = 0; i < N / 2; ++i) {
         buf += ws_a;
         buf += '{';
         buf += ws_b;
         buf += '{';
      }

      if (!verify_correctness(buf, ws_a.data(), ws_a.size(), "alternating 4sp/8sp")) {
         std::abort();
      }

      bencher::stage stage;
      stage.name = "Alternating \\n+4sp / \\n+8sp (50% mismatch)";

      stage.run("scalar skip_ws only", [&] {
         const char* it = buf.data();
         const char* end = buf.data() + buf.size();
         while (it < end) {
            scalar::skip_ws(it);
            if (it < end) ++it;
         }
         bencher::do_not_optimize(it);
         return buf.size();
      });

      stage.run("matching + skip_ws", [&] {
         const char* it = buf.data();
         const char* end = buf.data() + buf.size();
         const char* ws_ptr = ws_a.data();
         size_t ws_size = ws_a.size();
         while (it < end) {
            combined::skip(ws_ptr, it, ws_size);
            if (it < end) ++it;
         }
         bencher::do_not_optimize(it);
         return buf.size();
      });

      bencher::print_results(stage);
   }

   // ========================================================================
   // Part 3: Realistic JSON nesting (mostly matching with occasional depth changes)
   // Pattern: 80% at depth 2 (4 spaces), 10% at depth 3 (6 spaces), 10% at depth 1 (2 spaces)
   // This is typical of an array of objects with occasional nested fields.
   // ========================================================================
   std::printf("\n=== Part 3: Realistic nesting (80%% match, 20%% mismatch) ===\n\n");

   {
      const std::string ws_primary = "\n    ";   // 5B — depth 2 (most common)
      const std::string ws_deeper  = "\n      "; // 7B — depth 3
      const std::string ws_shallow = "\n  ";     // 3B — depth 1

      std::string buf;
      buf.reserve(N * 6); // rough estimate
      for (size_t i = 0; i < N; ++i) {
         size_t mod = i % 10;
         if (mod < 8) {
            buf += ws_primary;
         } else if (mod < 9) {
            buf += ws_deeper;
         } else {
            buf += ws_shallow;
         }
         buf += '{';
      }

      if (!verify_correctness(buf, ws_primary.data(), ws_primary.size(), "realistic 80/10/10")) {
         std::abort();
      }

      size_t total_ws = buf.size() - N; // total ws bytes (buf minus N delimiters)

      bencher::stage stage;
      stage.name = "Realistic nesting: 80% \\n+4sp, 10% \\n+6sp, 10% \\n+2sp";

      stage.run("scalar skip_ws only", [&] {
         const char* it = buf.data();
         const char* end = buf.data() + buf.size();
         while (it < end) {
            scalar::skip_ws(it);
            if (it < end) ++it;
         }
         bencher::do_not_optimize(it);
         return total_ws;
      });

      stage.run("matching + skip_ws", [&] {
         const char* it = buf.data();
         const char* end = buf.data() + buf.size();
         const char* ws_ptr = ws_primary.data();
         size_t ws_size = ws_primary.size();
         while (it < end) {
            combined::skip(ws_ptr, it, ws_size);
            if (it < end) ++it;
         }
         bencher::do_not_optimize(it);
         return total_ws;
      });

      bencher::print_results(stage);
   }

   // ========================================================================
   // Part 4: Real prettified JSON whitespace
   // Generate actual JSON, prettify it, extract all whitespace gaps,
   // then benchmark skipping through them.
   // ========================================================================
   std::printf("\n=== Part 4: Extracted from real prettified JSON ===\n\n");

   {
      // Build a realistic prettified JSON by hand (avoids needing glaze headers)
      // Represents: {"data":[{"id":1,"name":"x","tags":["a","b"],"meta":{"k":"v"}}, ...]}
      std::string json;
      json += "{\n";
      json += "  \"data\": [\n";
      for (int i = 0; i < 200; ++i) {
         if (i > 0) json += ",\n";
         json += "    {\n";
         json += "      \"id\": " + std::to_string(i) + ",\n";
         json += "      \"name\": \"user_" + std::to_string(i) + "\",\n";
         json += "      \"active\": true,\n";
         json += "      \"tags\": [\n";
         json += "        \"alpha\",\n";
         json += "        \"beta\"\n";
         json += "      ],\n";
         json += "      \"meta\": {\n";
         json += "        \"role\": \"admin\",\n";
         json += "        \"level\": 5\n";
         json += "      }\n";
         json += "    }";
      }
      json += "\n  ]\n";
      json += "}\n";

      // Extract whitespace gaps: sequences of ws chars between non-ws chars
      struct ws_gap {
         size_t offset;
         size_t length;
      };
      std::vector<ws_gap> gaps;
      size_t total_ws = 0;
      {
         size_t i = 0;
         while (i < json.size()) {
            if (scalar::whitespace_table[uint8_t(json[i])]) {
               size_t start = i;
               while (i < json.size() && scalar::whitespace_table[uint8_t(json[i])]) {
                  ++i;
               }
               gaps.push_back({start, i - start});
               total_ws += i - start;
            } else {
               ++i;
            }
         }
      }

      std::printf("JSON size: %zu bytes, %zu whitespace gaps, %zu total ws bytes\n",
                  json.size(), gaps.size(), total_ws);

      // Print gap size distribution
      std::array<size_t, 32> size_dist{};
      for (const auto& g : gaps) {
         size_t bucket = g.length < 32 ? g.length : 31;
         ++size_dist[bucket];
      }
      std::printf("Gap size distribution:\n");
      for (size_t s = 1; s < 32; ++s) {
         if (size_dist[s] > 0) {
            std::printf("  %2zu bytes: %zu gaps\n", s, size_dist[s]);
         }
      }
      std::printf("\n");

      // Find the most common gap pattern (for skip_matching_ws)
      // In prettified JSON, "\n      " (7 bytes) is very common at depth 2
      std::string most_common_ws;
      {
         // Count exact pattern frequencies
         struct pattern_count { std::string pat; size_t count; };
         std::vector<pattern_count> pats;
         for (const auto& g : gaps) {
            std::string p = json.substr(g.offset, g.length);
            bool found = false;
            for (auto& pc : pats) {
               if (pc.pat == p) { ++pc.count; found = true; break; }
            }
            if (!found) pats.push_back({p, 1});
         }
         // Find most frequent
         size_t best = 0;
         for (size_t i = 1; i < pats.size(); ++i) {
            if (pats[i].count > pats[best].count) best = i;
         }
         most_common_ws = pats[best].pat;
         std::printf("Most common ws pattern: %zu bytes (%zu/%zu = %.0f%% of gaps)\n\n",
                     most_common_ws.size(), pats[best].count, gaps.size(),
                     100.0 * pats[best].count / gaps.size());
      }

      // Benchmark: iterate through the actual JSON, skipping ws gaps
      // Repeat the JSON multiple times for stable timing
      constexpr size_t repeats = 100;
      std::string big_json;
      big_json.reserve(json.size() * repeats);
      for (size_t r = 0; r < repeats; ++r) {
         big_json += json;
      }

      bencher::stage stage;
      stage.name = "Real prettified JSON (" + std::to_string(big_json.size() / 1024) + " KB)";

      stage.run("scalar skip_ws only", [&] {
         const char* it = big_json.data();
         const char* end = big_json.data() + big_json.size();
         size_t ws_bytes = 0;
         while (it < end) {
            const char* before = it;
            scalar::skip_ws(it);
            ws_bytes += size_t(it - before);
            if (it < end) ++it; // advance past non-ws char
         }
         bencher::do_not_optimize(it);
         return total_ws * repeats;
      });

      stage.run("matching + skip_ws", [&] {
         const char* it = big_json.data();
         const char* end = big_json.data() + big_json.size();
         const char* ws_ptr = most_common_ws.data();
         size_t ws_size = most_common_ws.size();
         size_t ws_bytes = 0;
         while (it < end) {
            const char* before = it;
            if (ws_size && ws_size < size_t(end - it)) {
               combined::skip(ws_ptr, it, ws_size);
            } else {
               scalar::skip_ws(it);
            }
            ws_bytes += size_t(it - before);
            if (it < end) ++it;
         }
         bencher::do_not_optimize(it);
         return total_ws * repeats;
      });

      bencher::print_results(stage);
   }

   // ========================================================================
   // Part 5: Only whitespace (no non-ws chars between)
   // Pure throughput test: how fast can each approach skip through
   // a large contiguous block of whitespace?
   // ========================================================================
   std::printf("\n=== Part 5: Contiguous whitespace throughput ===\n\n");

   {
      size_t sizes[] = {16, 64, 256, 1024, 4096};
      for (size_t sz : sizes) {
         // All spaces followed by a terminator
         std::string ws(sz, ' ');
         ws += 'x';

         // For matching: record first 8 bytes (or fewer) as the pattern
         // In reality the pattern would be the first line's indent
         std::string pattern(std::min(sz, size_t(8)), ' ');

         char label[64];
         std::snprintf(label, sizeof(label), "Contiguous spaces (%zu B)", sz);

         bencher::stage stage;
         stage.name = label;

         stage.run("scalar skip_ws only", [&] {
            const char* it = ws.data();
            scalar::skip_ws(it);
            bencher::do_not_optimize(it);
            return sz;
         });

         // For contiguous ws, matching can only skip pattern-size bytes,
         // then skip_ws handles the rest. This shows the overhead.
         stage.run("matching + skip_ws", [&] {
            const char* it = ws.data();
            combined::skip(pattern.data(), it, pattern.size());
            bencher::do_not_optimize(it);
            return sz;
         });

         bencher::print_results(stage);
      }
   }

   std::printf("\n=== All correctness checks passed ===\n");

   return 0;
}
