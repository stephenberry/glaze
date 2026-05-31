// Float serialization throughput: glz::to_chars (zmij-based) fast + size-opt
// variants against libc++'s std::to_chars.

#include <charconv>
#include <cstring>
#include <random>
#include <vector>

#include "bencher/bencher.hpp"
#include "bencher/diagnostics.hpp"
#include "glaze/util/zmij.hpp"

namespace
{
   std::vector<float> generate_random_floats(size_t count, uint32_t seed = 12345)
   {
      std::mt19937 rng(seed);
      std::uniform_int_distribution<uint32_t> dist;
      std::vector<float> result;
      result.reserve(count);
      while (result.size() < count) {
         uint32_t bits = dist(rng);
         float value;
         std::memcpy(&value, &bits, sizeof(float));
         if (!std::isnan(value) && !std::isinf(value)) result.push_back(value);
      }
      return result;
   }

   std::vector<double> generate_random_doubles(size_t count, uint32_t seed = 12345)
   {
      std::mt19937_64 rng(seed);
      std::uniform_int_distribution<uint64_t> dist;
      std::vector<double> result;
      result.reserve(count);
      while (result.size() < count) {
         uint64_t bits = dist(rng);
         double value;
         std::memcpy(&value, &bits, sizeof(double));
         if (!std::isnan(value) && !std::isinf(value)) result.push_back(value);
      }
      return result;
   }
}

int main()
{
   constexpr size_t N = 100000;

   const auto floats = generate_random_floats(N);
   const auto doubles = generate_random_doubles(N);

   std::cout << "=== Float to_chars ===\n";
   {
      bencher::stage stage{"Float serialization"};
      stage.min_execution_count = 50;

      stage.run("glz::to_chars<float> (fast)", [&] {
         char buf[64];
         size_t total_bytes = 0;
         for (float f : floats) {
            char* end = glz::to_chars<float, /*OptSize=*/false>(buf, f);
            total_bytes += size_t(end - buf);
            bencher::do_not_optimize(buf);
         }
         return total_bytes;
      });

      stage.run("glz::to_chars<float,true> (size-opt)", [&] {
         char buf[64];
         size_t total_bytes = 0;
         for (float f : floats) {
            char* end = glz::to_chars<float, /*OptSize=*/true>(buf, f);
            total_bytes += size_t(end - buf);
            bencher::do_not_optimize(buf);
         }
         return total_bytes;
      });

      stage.run("std::to_chars (float)", [&] {
         char buf[64];
         size_t total_bytes = 0;
         for (float f : floats) {
            auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), f);
            total_bytes += size_t(ptr - buf);
            bencher::do_not_optimize(buf);
         }
         return total_bytes;
      });

      bencher::print_results(stage);
   }

   std::cout << "\n=== Double to_chars ===\n";
   {
      bencher::stage stage{"Double serialization"};
      stage.min_execution_count = 50;

      stage.run("glz::to_chars<double> (fast)", [&] {
         char buf[64];
         size_t total_bytes = 0;
         for (double d : doubles) {
            char* end = glz::to_chars<double, /*OptSize=*/false>(buf, d);
            total_bytes += size_t(end - buf);
            bencher::do_not_optimize(buf);
         }
         return total_bytes;
      });

      stage.run("glz::to_chars<double,true> (size-opt)", [&] {
         char buf[64];
         size_t total_bytes = 0;
         for (double d : doubles) {
            char* end = glz::to_chars<double, /*OptSize=*/true>(buf, d);
            total_bytes += size_t(end - buf);
            bencher::do_not_optimize(buf);
         }
         return total_bytes;
      });

      stage.run("std::to_chars (double)", [&] {
         char buf[64];
         size_t total_bytes = 0;
         for (double d : doubles) {
            auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), d);
            total_bytes += size_t(ptr - buf);
            bencher::do_not_optimize(buf);
         }
         return total_bytes;
      });

      bencher::print_results(stage);
   }

   return 0;
}
