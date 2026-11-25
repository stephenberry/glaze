// Guards that reading a deeply nested BEVE variant stays linear in depth. Exits nonzero if not.
//
// The variant reader's key scan must stop once a single candidate remains. Without that it skips
// every nested subtree, making the read quadratic in depth. The result is still correct and, under
// max_recursive_depth_limit (256), only milliseconds slow, so no correctness test notices. This
// asserts on the shape of the growth rather than absolute time, which varies far too much across
// machines.
//
// Depth is capped at 200 because the reader rejects anything past max_recursive_depth_limit, one
// level per nested object. Each buffer is read many times so the ratio does not rest on a single
// sub-millisecond sample.
//
// How many times depends on the clock, whose tick is platform-defined and can outlast a whole
// batch: FreeBSD's CLOCKS_PER_SEC is 128, a 7.8 ms tick, while an optimized build reads the shallow
// buffer 200 times in under 1 ms. That batch measured zero and the growth read as infinite. So the
// tick is measured, and the batch grows until the shallow read spans at least ten of them, which
// bounds the rounding at a tenth of the smaller sample.
//
// The clock is process CPU time rather than wall time, because a loaded machine deschedules the
// loops. Wall time counts that waiting and the ratio stops describing the code: a run alongside two
// other test binaries on a macOS CI runner read 12.0x on a commit that touched only write paths,
// which is indistinguishable from the ~13x of a genuinely quadratic reader. CPU time excludes the
// waiting, and holds 3.1-4.9x against the quadratic reader's 10.5-13.9x even when the machine is
// oversubscribed six to one.
//
// The growth is then the median of several rounds rather than one sample of each depth. Both
// depths are timed back-to-back within a round so that whatever interference remains lands on
// numerator and denominator together and largely divides out; the median then discards whichever
// rounds it skewed anyway. The median and not the minimum: noise inflates a denominator as readily
// as a numerator, and a low outlier is how a real regression would slip through. On wall time the
// quadratic reader's cheapest loaded round already read 8.0x.

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <memory>
#include <string>
#include <variant>

#include "glaze/beve.hpp"

struct deep_leaf
{
   int v{};
};
struct deep_node;
using deep_v = std::variant<deep_leaf, std::unique_ptr<deep_node>>;
struct deep_node
{
   deep_v child{};
   int n{};
};

static std::string build(int depth)
{
   deep_v v{deep_leaf{1}};
   for (int i = 0; i < depth; ++i) {
      auto node = std::make_unique<deep_node>();
      node->child = std::move(v);
      node->n = i;
      v = std::move(node);
   }
   return glz::write_beve(v).value();
}

static double bench_cpu_ms(const std::string& buf, int n)
{
   const auto c0 = std::clock();
   for (int i = 0; i < n; ++i) {
      deep_v out{};
      (void)glz::read_beve(out, buf);
   }
   return 1000.0 * double(std::clock() - c0) / double(CLOCKS_PER_SEC);
}

static double clock_tick_ms()
{
   // Spin to a tick boundary first, so the second spin spans exactly one tick.
   const auto start = std::clock();
   auto edge = start;
   while ((edge = std::clock()) == start) {
   }
   auto next = edge;
   while ((next = std::clock()) == edge) {
   }
   return 1000.0 * double(next - edge) / double(CLOCKS_PER_SEC);
}

int main()
{
   constexpr int rounds = 5; // odd, so the median is the middle element
   // Measured 3.9-4.1x linear against 12.5-12.8x quadratic, so 8x separates them with margin.
   constexpr double max_growth = 8.0;

   const auto shallow = build(50);
   const auto deep = build(200); // 4x the depth
   {
      deep_v out{};
      if (const auto ec = glz::read_beve(out, deep)) {
         std::fprintf(stderr, "reading the deep buffer failed: %s\n", glz::format_error(ec).c_str());
         return EXIT_FAILURE;
      }
   }

   const double tick_ms = clock_tick_ms();
   const double min_sample_ms = std::max(5.0, 10.0 * tick_ms);
   int reps = 64;
   while (bench_cpu_ms(shallow, reps) < min_sample_ms) { // also warms the shallow path
      reps *= 2;
   }
   bench_cpu_ms(deep, reps / 4); // warm the deep path before timing

   std::array<double, rounds> growth{};
   for (auto& g : growth) {
      const auto t_shallow = bench_cpu_ms(shallow, reps);
      const auto t_deep = bench_cpu_ms(deep, reps);
      g = t_deep / t_shallow;
   }
   std::ranges::sort(growth);
   const auto median_growth = growth[rounds / 2];

   std::printf("clock tick: %.4f ms, reads per sample: %d\n", tick_ms, reps);
   std::printf("read time growth for 4x the depth (sorted):");
   for (const auto g : growth) {
      std::printf(" %.2fx", g);
   }
   std::printf("\nmedian: %.2fx (limit %.1fx)\n", median_growth, max_growth);

   if (!(median_growth < max_growth)) {
      std::fprintf(stderr, "FAILED: deeply nested variant reads grew super-linearly with depth\n");
      return EXIT_FAILURE;
   }
   return EXIT_SUCCESS;
}
