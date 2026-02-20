#include <algorithm>
#include <cstdint>
#include <random>
#include <vector>

#include "bencher/bencher.hpp"
#include "bencher/diagnostics.hpp"
#include "glaze/hash/sweethash.hpp"

// Branchless binary search on a sorted array of (hash, index) pairs.
// Returns pointer to the matching element, or end if not found.
inline const std::pair<uint32_t, uint32_t>* branchless_lower_bound(const std::pair<uint32_t, uint32_t>* data, size_t n,
                                                                   uint32_t target) noexcept
{
   const std::pair<uint32_t, uint32_t>* p = data;
   size_t len = n;
   while (len > 1) {
      size_t half = len / 2;
      p += (p[half - 1].first < target) * half; // compiles to cmov
      len -= half;
   }
   // Final element check: advance past it if it's less than target
   p += (len == 1 && p->first < target);
   return p;
}

struct bench_data
{
   std::vector<std::pair<uint32_t, uint32_t>> index; // sorted (hash, original_index) pairs
   std::vector<uint32_t> lookup_hashes; // hashes to look up (all exist in index)
};

bench_data generate_data(size_t n, size_t num_lookups = 10000)
{
   bench_data bd;
   bd.index.reserve(n);

   // Generate realistic keys like "key_0", "key_1", ... and hash them
   for (uint32_t i = 0; i < static_cast<uint32_t>(n); ++i) {
      std::string key = "key_" + std::to_string(i);
      uint32_t h = glz::sweethash::sweet32(key);
      bd.index.emplace_back(h, i);
   }

   // Sort by hash (mimics ordered_map's index)
   std::sort(bd.index.begin(), bd.index.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

   // Generate lookup targets (random existing hashes)
   std::mt19937 rng(42);
   std::uniform_int_distribution<size_t> dist(0, n - 1);
   bd.lookup_hashes.reserve(num_lookups);
   for (size_t i = 0; i < num_lookups; ++i) {
      bd.lookup_hashes.push_back(bd.index[dist(rng)].first);
   }

   return bd;
}

int main()
{
   for (size_t n : {32, 64, 128, 256, 512, 1024, 4096}) {
      auto data = generate_data(n);
      const size_t num_lookups = data.lookup_hashes.size();

      bencher::stage stage;
      stage.name = "Binary search (n=" + std::to_string(n) + ")";
      stage.min_execution_count = 100;
      stage.cold_cache = false; // We want hot-cache to measure branch prediction

      stage.run("std::lower_bound", [&] {
         uint64_t found = 0;
         for (uint32_t h : data.lookup_hashes) {
            auto it = std::lower_bound(data.index.begin(), data.index.end(), h,
                                       [](const auto& elem, uint32_t hash) { return elem.first < hash; });
            found += (it != data.index.end() && it->first == h);
         }
         bencher::do_not_optimize(found);
         return num_lookups * sizeof(uint32_t);
      });

      stage.run("branchless", [&] {
         uint64_t found = 0;
         for (uint32_t h : data.lookup_hashes) {
            auto* p = branchless_lower_bound(data.index.data(), data.index.size(), h);
            found += (p != data.index.data() + data.index.size() && p->first == h);
         }
         bencher::do_not_optimize(found);
         return num_lookups * sizeof(uint32_t);
      });

      bencher::print_results(stage);
   }

   return 0;
}
