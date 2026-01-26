// Benchmark for skip_ws optimization (PR #2269)
// Compares the scalar baseline against the library's skip_ws implementation.
// When the library adds SIMD skip_ws, this benchmark shows the speedup.

#include "bencher/bencher.hpp"
#include "bencher/diagnostics.hpp"

#include "glaze/json.hpp"

#include <random>
#include <string>
#include <vector>

// Structs must be at namespace scope for glaze reflection
struct TestObj
{
   int64_t id{};
   double value{};
   std::string name{};
   bool active{};
   std::vector<int> scores{};
};

struct Root
{
   std::vector<TestObj> data{};
};

// ============================================================================
// Scalar baseline: the pre-SIMD skip_ws implementation (lookup table, 1 byte at a time)
// This is the reference implementation to compare against.
// ============================================================================
namespace scalar_baseline
{
   inline constexpr std::array<bool, 256> whitespace_table = [] {
      std::array<bool, 256> t{};
      t['\n'] = true;
      t['\t'] = true;
      t['\r'] = true;
      t[' '] = true;
      return t;
   }();

   // Null-terminated version (matches the library's null_terminated path)
   GLZ_ALWAYS_INLINE void skip_ws(const char*& it) noexcept
   {
      while (whitespace_table[uint8_t(*it)]) {
         ++it;
      }
   }

   // Bounded version (matches the library's non-null-terminated path)
   GLZ_ALWAYS_INLINE bool skip_ws_bounded(const char*& it, const char* end) noexcept
   {
      while (it < end && whitespace_table[uint8_t(*it)]) {
         ++it;
      }
      return it == end;
   }
}

// ============================================================================
// skip_matching_ws alternatives
// ============================================================================

// Current library implementation (manual 8/4/2-byte cascade)
namespace current_matching
{
   GLZ_ALWAYS_INLINE void skip_matching_ws(const auto* ws, auto&& it, uint64_t length) noexcept
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

// Proposed: memcmp with a stack buffer
namespace memcmp_matching
{
   // Copy the expected ws pattern to a stack buffer once, then memcmp against it
   GLZ_ALWAYS_INLINE void skip_matching_ws(const char* ws_buf, auto&& it, size_t ws_size) noexcept
   {
      if (std::memcmp(ws_buf, it, ws_size) == 0) {
         it += ws_size;
      }
   }
}

// ============================================================================
// Generate whitespace buffers with varying patterns
// ============================================================================

// Pure whitespace (spaces, tabs, newlines) followed by a non-WS terminator
static std::string generate_whitespace(size_t length, uint64_t seed = 42)
{
   std::mt19937 rng(seed);
   std::uniform_int_distribution<int> dist(0, 3);
   const char ws_chars[] = {' ', '\t', '\n', '\r'};

   std::string result;
   result.reserve(length + 1);
   for (size_t i = 0; i < length; ++i) {
      result += ws_chars[dist(rng)];
   }
   result += 'x'; // non-whitespace terminator
   return result;
}

// Realistic: short whitespace gaps between JSON tokens
// e.g., "  \n   " (1-8 chars), simulating prettified JSON indentation
static std::vector<std::string> generate_short_gaps(size_t count, uint64_t seed = 42)
{
   std::mt19937 rng(seed);
   std::uniform_int_distribution<int> len_dist(1, 8);
   std::uniform_int_distribution<int> ws_dist(0, 3);
   const char ws_chars[] = {' ', '\t', '\n', '\r'};

   std::vector<std::string> gaps;
   gaps.reserve(count);
   for (size_t i = 0; i < count; ++i) {
      int len = len_dist(rng);
      std::string gap;
      gap.reserve(len + 1);
      for (int j = 0; j < len; ++j) {
         gap += ws_chars[ws_dist(rng)];
      }
      gap += '{'; // non-whitespace terminator
      gaps.push_back(std::move(gap));
   }
   return gaps;
}

int main()
{
   // ========================================================================
   // Benchmark 1: Long contiguous whitespace (where SIMD helps most)
   // ========================================================================
   {
      const auto ws_64 = generate_whitespace(64);
      const auto ws_256 = generate_whitespace(256);
      const auto ws_4k = generate_whitespace(4096);

      struct ws_test {
         const char* name;
         const std::string* data;
      };
      ws_test tests[] = {
         {"64 bytes", &ws_64},
         {"256 bytes", &ws_256},
         {"4 KB", &ws_4k},
      };

      for (const auto& [name, data] : tests) {
         bencher::stage stage;
         stage.name = std::string("Contiguous whitespace (") + name + ")";

         const size_t ws_len = data->size() - 1; // exclude terminator

         stage.run("scalar baseline", [&] {
            const char* it = data->data();
            scalar_baseline::skip_ws(it);
            bencher::do_not_optimize(it);
            return ws_len;
         });

         stage.run("library skip_ws", [&] {
            const char* it = data->data();
            const char* end = data->data() + data->size();
            glz::context ctx{};
            glz::skip_ws<glz::ws_opts{false, true, false}>(ctx, it, end);
            bencher::do_not_optimize(it);
            return ws_len;
         });

         bencher::print_results(stage);
      }
   }

   // ========================================================================
   // Benchmark 2: Many short whitespace gaps (realistic JSON indentation)
   // ========================================================================
   {
      constexpr size_t N = 100000;
      const auto gaps = generate_short_gaps(N);

      size_t total_ws_bytes = 0;
      for (const auto& gap : gaps) {
         total_ws_bytes += gap.size() - 1; // exclude terminator
      }

      bencher::stage stage;
      stage.name = "Short whitespace gaps (1-8 bytes each, 100K calls)";

      stage.run("scalar baseline", [&] {
         for (const auto& gap : gaps) {
            const char* it = gap.data();
            scalar_baseline::skip_ws(it);
            bencher::do_not_optimize(it);
         }
         return total_ws_bytes;
      });

      stage.run("library skip_ws", [&] {
         for (const auto& gap : gaps) {
            const char* it = gap.data();
            const char* end = gap.data() + gap.size();
            glz::context ctx{};
            glz::skip_ws<glz::ws_opts{false, true, false}>(ctx, it, end);
            bencher::do_not_optimize(it);
         }
         return total_ws_bytes;
      });

      bencher::print_results(stage);
   }

   // ========================================================================
   // Benchmark 3: skip_matching_ws — current cascade vs memcmp
   // Simulates prettified JSON: record ws pattern once, match it N times.
   // ========================================================================
   {
      // Typical prettified JSON indentation patterns
      struct ws_pattern {
         const char* name;
         std::string pattern;
      };
      ws_pattern patterns[] = {
         {"\\n + 2 spaces (3B)", std::string("\n  ") + "{"},
         {"\\n + 4 spaces (5B)", std::string("\n    ") + "{"},
         {"\\n + 6 spaces (7B)", std::string("\n      ") + "{"},
         {"\\n + 8 spaces (9B)", std::string("\n        ") + "{"},
         {"\\n + 3 tabs (4B)", std::string("\n\t\t\t") + "{"},
      };

      constexpr size_t N = 500000;

      for (auto& [name, data] : patterns) {
         const size_t ws_size = data.size() - 1; // exclude the '{' terminator

         // Build a buffer of N repeated occurrences: ws + '{' + ws + '{' + ...
         std::string repeated;
         repeated.reserve(data.size() * N);
         for (size_t i = 0; i < N; ++i) {
            repeated += data;
         }

         // Stack buffer for the expected pattern (what the memcmp approach would use)
         char ws_buf[16]{};
         std::memcpy(ws_buf, data.data(), ws_size);

         const char* ws_ptr = data.data(); // pointer into original pattern

         bencher::stage stage;
         stage.name = std::string("skip_matching_ws: ") + name;

         stage.run("current (8/4/2 cascade)", [&] {
            const char* it = repeated.data();
            const char* end = repeated.data() + repeated.size();
            while (it < end) {
               current_matching::skip_matching_ws(ws_ptr, it, ws_size);
               if (it < end) ++it; // skip the '{' delimiter
            }
            bencher::do_not_optimize(it);
            return ws_size * N;
         });

         stage.run("memcmp (stack buffer)", [&] {
            const char* it = repeated.data();
            const char* end = repeated.data() + repeated.size();
            while (it < end) {
               memcmp_matching::skip_matching_ws(ws_buf, it, ws_size);
               if (it < end) ++it; // skip the '{' delimiter
            }
            bencher::do_not_optimize(it);
            return ws_size * N;
         });

         stage.run("scalar skip_ws", [&] {
            const char* it = repeated.data();
            const char* end = repeated.data() + repeated.size();
            while (it < end) {
               scalar_baseline::skip_ws(it);
               if (it < end) ++it; // skip the '{' delimiter
            }
            bencher::do_not_optimize(it);
            return ws_size * N;
         });

         bencher::print_results(stage);
      }
   }

   // ========================================================================
   // Benchmark 4: End-to-end read_json (shows real-world impact)
   // ========================================================================
   {
      Root original;
      original.data.reserve(10000);
      std::mt19937 rng(42);
      std::uniform_int_distribution<int> id_dist(1, 1000000);
      std::uniform_real_distribution<double> val_dist(0.0, 1000.0);
      std::uniform_int_distribution<int> bool_dist(0, 1);
      std::uniform_int_distribution<int> score_dist(0, 100);
      std::uniform_int_distribution<int> score_len_dist(1, 8);

      for (size_t i = 0; i < 10000; ++i) {
         TestObj s;
         s.id = id_dist(rng);
         s.value = val_dist(rng);
         s.active = bool_dist(rng) != 0;
         s.name = "user_" + std::to_string(i);
         int scores_len = score_len_dist(rng);
         s.scores.reserve(scores_len);
         for (int j = 0; j < scores_len; ++j) {
            s.scores.push_back(score_dist(rng));
         }
         original.data.push_back(std::move(s));
      }

      std::string minified;
      (void)glz::write_json(original, minified);

      std::string prettified;
      (void)glz::write<glz::opts{.prettify = true}>(original, prettified);

      std::printf("Minified size:   %.2f MB\n", minified.size() / (1024.0 * 1024.0));
      std::printf("Prettified size: %.2f MB\n\n", prettified.size() / (1024.0 * 1024.0));

      bencher::stage stage;
      stage.name = "End-to-end read_json (10K objects)";

      stage.run("minified", [&] {
         Root dest;
         auto ec = glz::read_json(dest, minified);
         if (ec) std::abort();
         return minified.size();
      });

      stage.run("prettified", [&] {
         Root dest;
         auto ec = glz::read_json(dest, prettified);
         if (ec) std::abort();
         return prettified.size();
      });

      bencher::print_results(stage);
   }

   return 0;
}
