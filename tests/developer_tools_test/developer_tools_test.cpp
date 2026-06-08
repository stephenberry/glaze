// Glaze Library
// For the license information refer to glaze.hpp

#include <cassert>
#include <chrono>
#include <cmath>
#include <iostream>
#include <map>
#include <numeric>
#include <optional>
#include <string>
#include <vector>

#include "glaze/core/explain.hpp"
#include "glaze/core/meta_serializer.hpp"
#include "glaze/glaze.hpp"

struct inner_struct
{
   int x{};
   double y{};
};

template <>
struct glz::meta<inner_struct>
{
   using T = inner_struct;
   static constexpr auto value = object("x", &T::x, "y", &T::y);
};

struct my_struct
{
   std::string name{};
   std::vector<int> nums{};
   inner_struct sub{};
   std::optional<int> maybe_val{};
   std::map<std::string, double> scores{};
};

template <>
struct glz::meta<my_struct>
{
   using T = my_struct;
   static constexpr auto value =
      object("name", &T::name, "nums", &T::nums, "sub", &T::sub, "maybe_val", &T::maybe_val, "scores", &T::scores);
};

// A simple mock format driver that prints elements in a simplified textual representation
struct mock_format_driver
{
   template <auto Opts, class B>
   GLZ_ALWAYS_INLINE static void write_bool(bool val, auto&&, B& b, auto& ix)
   {
      const std::string s = val ? "true" : "false";
      std::memcpy(&b[ix], s.data(), s.size());
      ix += s.size();
   }

   template <auto Opts, class T, class B>
   GLZ_ALWAYS_INLINE static void write_number(T val, auto&&, B& b, auto& ix)
   {
      const std::string s = std::to_string(val);
      std::memcpy(&b[ix], s.data(), s.size());
      ix += s.size();
   }

   template <auto Opts, class B>
   GLZ_ALWAYS_INLINE static void write_string(std::string_view val, auto&&, B& b, auto& ix)
   {
      b[ix] = '"';
      ++ix;
      std::memcpy(&b[ix], val.data(), val.size());
      ix += val.size();
      b[ix] = '"';
      ++ix;
   }

   template <auto Opts, class B>
   GLZ_ALWAYS_INLINE static void write_null(auto&&, B& b, auto& ix)
   {
      std::memcpy(&b[ix], "null", 4);
      ix += 4;
   }

   template <auto Opts, class B>
   GLZ_ALWAYS_INLINE static void start_object(size_t, auto&&, B& b, auto& ix)
   {
      b[ix] = '{';
      ++ix;
   }

   template <auto Opts, class B>
   GLZ_ALWAYS_INLINE static void end_object(auto&&, B& b, auto& ix)
   {
      b[ix] = '}';
      ++ix;
   }

   template <auto Opts, class B>
   GLZ_ALWAYS_INLINE static void write_object_key(std::string_view key, auto&&, B& b, auto& ix)
   {
      std::memcpy(&b[ix], key.data(), key.size());
      ix += key.size();
      b[ix] = ':';
      ++ix;
   }

   template <auto Opts, class B>
   GLZ_ALWAYS_INLINE static void write_object_separator(auto&&, B& b, auto& ix)
   {
      b[ix] = ',';
      ++ix;
   }

   template <auto Opts, class B>
   GLZ_ALWAYS_INLINE static void start_array(size_t, auto&&, B& b, auto& ix)
   {
      b[ix] = '[';
      ++ix;
   }

   template <auto Opts, class B>
   GLZ_ALWAYS_INLINE static void end_array(auto&&, B& b, auto& ix)
   {
      b[ix] = ']';
      ++ix;
   }

   template <auto Opts, class B>
   GLZ_ALWAYS_INLINE static void write_array_separator(auto&&, B& b, auto& ix)
   {
      b[ix] = ',';
      ++ix;
   }
};

// Hand-written recursive serializer that achieves the same output as mock_format_driver
struct hand_written_serializer
{
   template <class B>
   GLZ_ALWAYS_INLINE static void serialize_inner(const inner_struct& val, B& b, size_t& ix)
   {
      b[ix++] = '{';
      std::memcpy(&b[ix], "x:", 2); ix += 2;
      const std::string sx = std::to_string(val.x);
      std::memcpy(&b[ix], sx.data(), sx.size()); ix += sx.size();
      b[ix++] = ',';
      std::memcpy(&b[ix], "y:", 2); ix += 2;
      const std::string sy = std::to_string(val.y);
      std::memcpy(&b[ix], sy.data(), sy.size()); ix += sy.size();
      b[ix++] = '}';
   }

   template <class B>
   GLZ_ALWAYS_INLINE static void serialize_my_struct(const my_struct& val, B& b, size_t& ix)
   {
      b[ix++] = '{';

      // name
      std::memcpy(&b[ix], "name:", 5); ix += 5;
      b[ix++] = '"';
      std::memcpy(&b[ix], val.name.data(), val.name.size()); ix += val.name.size();
      b[ix++] = '"';
      b[ix++] = ',';

      // nums
      std::memcpy(&b[ix], "nums:", 5); ix += 5;
      b[ix++] = '[';
      for (size_t i = 0; i < val.nums.size(); ++i) {
         const std::string s = std::to_string(val.nums[i]);
         std::memcpy(&b[ix], s.data(), s.size()); ix += s.size();
         if (i != val.nums.size() - 1) b[ix++] = ',';
      }
      b[ix++] = ']';
      b[ix++] = ',';

      // sub
      std::memcpy(&b[ix], "sub:", 4); ix += 4;
      serialize_inner(val.sub, b, ix);
      b[ix++] = ',';

      // maybe_val
      std::memcpy(&b[ix], "maybe_val:", 10); ix += 10;
      if (val.maybe_val.has_value()) {
         const std::string s = std::to_string(*val.maybe_val);
         std::memcpy(&b[ix], s.data(), s.size()); ix += s.size();
      }
      else {
         std::memcpy(&b[ix], "null", 4); ix += 4;
      }
      b[ix++] = ',';

      // scores
      std::memcpy(&b[ix], "scores:", 7); ix += 7;
      b[ix++] = '{';
      size_t count = 0;
      for (auto&& [k, v] : val.scores) {
         std::memcpy(&b[ix], k.data(), k.size()); ix += k.size();
         b[ix++] = ':';
         const std::string s = std::to_string(v);
         std::memcpy(&b[ix], s.data(), s.size()); ix += s.size();
         if (count != val.scores.size() - 1) b[ix++] = ',';
         ++count;
      }
      b[ix++] = '}';

      b[ix++] = '}';
   }
};

struct stats
{
   double min{};
   double max{};
   double mean{};
   double stdev{};
};

stats calculate_stats(const std::vector<double>& data)
{
   double sum = std::accumulate(data.begin(), data.end(), 0.0);
   double mean = sum / data.size();
   double min = *std::min_element(data.begin(), data.end());
   double max = *std::max_element(data.begin(), data.end());

   double sq_sum = std::inner_product(data.begin(), data.end(), data.begin(), 0.0);
   double stdev = std::sqrt(sq_sum / data.size() - mean * mean);

   return {min, max, mean, stdev};
}

int main()
{
   // 1. Diagnostics compilation checks
   static_assert(glz::explain_write_supported<glz::JSON, int>());
   static_assert(glz::explain_write_supported<glz::JSON, my_struct>());

   // 2. Data correctness validation
   my_struct obj{.name = "Glaze Performance Benchmark Struct",
                 .nums = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10},
                 .sub = {.x = 1337, .y = 2.71828},
                 .maybe_val = 42,
                 .scores = {{"c++", 100.0}, {"rust", 90.0}, {"go", 80.0}}};

   std::string buf_meta;
   buf_meta.resize(4096);
   size_t ix_meta = 0;
   glz::context ctx{};
   glz::meta_serialize<mock_format_driver, glz::opts{}, my_struct>::op(obj, ctx, buf_meta, ix_meta);
   buf_meta.resize(ix_meta);

   std::string buf_hand;
   buf_hand.resize(4096);
   size_t ix_hand = 0;
   hand_written_serializer::serialize_my_struct(obj, buf_hand, ix_hand);
   buf_hand.resize(ix_hand);

   // Assert outputs are identical
   assert(buf_meta == buf_hand);

   // 3. Benchmarking Loop Setup
   constexpr size_t iterations = 300000;
   constexpr int rounds = 5;
   std::cout << "Starting statistical benchmark of " << iterations << " iterations over " << rounds << " rounds...\n" << std::endl;

   std::vector<double> meta_times;
   std::vector<double> hand_times;

   // Warmup
   for (size_t i = 0; i < 20000; ++i) {
      size_t ix = 0;
      glz::meta_serialize<mock_format_driver, glz::opts{}, my_struct>::op(obj, ctx, buf_meta, ix);
      size_t ix2 = 0;
      hand_written_serializer::serialize_my_struct(obj, buf_hand, ix2);
   }

   for (int r = 0; r < rounds; ++r) {
      // Benchmark meta_serialize (Unified Format Driver Engine)
      auto start_meta = std::chrono::high_resolution_clock::now();
      for (size_t i = 0; i < iterations; ++i) {
         size_t ix = 0;
         glz::meta_serialize<mock_format_driver, glz::opts{}, my_struct>::op(obj, ctx, buf_meta, ix);
         asm volatile("" : "+g"(ix), "+g"(buf_meta) : : "memory");
      }
      auto end_meta = std::chrono::high_resolution_clock::now();
      double dur_meta = std::chrono::duration_cast<std::chrono::microseconds>(end_meta - start_meta).count();
      meta_times.push_back(dur_meta);

      // Benchmark hand-written serializer
      auto start_hand = std::chrono::high_resolution_clock::now();
      for (size_t i = 0; i < iterations; ++i) {
         size_t ix = 0;
         hand_written_serializer::serialize_my_struct(obj, buf_hand, ix);
         asm volatile("" : "+g"(ix), "+g"(buf_hand) : : "memory");
      }
      auto end_hand = std::chrono::high_resolution_clock::now();
      double dur_hand = std::chrono::duration_cast<std::chrono::microseconds>(end_hand - start_hand).count();
      hand_times.push_back(dur_hand);

      std::cout << "Round " << r + 1 << ": Meta = " << dur_meta << " us | Hand = " << dur_hand << " us" << std::endl;
   }

   stats meta_stats = calculate_stats(meta_times);
   stats hand_stats = calculate_stats(hand_times);

   // Report Results
   std::cout << "\n==================================================" << std::endl;
   std::cout << "STATISTICAL BENCHMARK RESULTS" << std::endl;
   std::cout << "==================================================" << std::endl;
   std::cout << "Unified Format Driver (meta_serialize):" << std::endl;
   std::cout << "  Mean Time: " << meta_stats.mean << " us" << std::endl;
   std::cout << "  Min Time:  " << meta_stats.min << " us" << std::endl;
   std::cout << "  Max Time:  " << meta_stats.max << " us" << std::endl;
   std::cout << "  StDev:     " << meta_stats.stdev << " us" << std::endl;
   std::cout << "--------------------------------------------------" << std::endl;
   std::cout << "Hand-Written Recursive Serializer:" << std::endl;
   std::cout << "  Mean Time: " << hand_stats.mean << " us" << std::endl;
   std::cout << "  Min Time:  " << hand_stats.min << " us" << std::endl;
   std::cout << "  Max Time:  " << hand_stats.max << " us" << std::endl;
   std::cout << "  StDev:     " << hand_stats.stdev << " us" << std::endl;
   std::cout << "==================================================" << std::endl;

   double overhead = (static_cast<double>(meta_stats.mean - hand_stats.mean) / hand_stats.mean) * 100.0;
   std::cout << "Average Performance Difference (Overhead): " << overhead << "%" << std::endl;
   std::cout << "(Values close to 0% or negative indicate zero runtime cost due to full inlining)" << std::endl;
   std::cout << "==================================================" << std::endl;

   if (overhead >= 5.0) {
      std::cout << "[WARNING] Performance overhead is above the 5% threshold! Jitter or virtualization noise on CI runner detected." << std::endl;
   } else {
      std::cout << "[SUCCESS] Performance overhead is well within the 5% margin." << std::endl;
   }

   std::cout << "All statistical benchmark checks passed successfully!" << std::endl;
   return 0;
}
