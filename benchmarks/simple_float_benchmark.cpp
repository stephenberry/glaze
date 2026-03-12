#include <charconv>
#include <cstring>
#include <random>
#include <vector>

#include "bencher/bencher.hpp"
#include "bencher/diagnostics.hpp"
#include "glaze/util/glaze_fast_float.hpp"
#include "glaze/util/simple_float.hpp"

// Generate test data
std::vector<float> generate_random_floats(size_t count, uint32_t seed = 12345)
{
   std::mt19937 rng(seed);
   std::uniform_int_distribution<uint32_t> dist;

   std::vector<float> result;
   result.reserve(count);

   for (size_t i = 0; i < count; ++i) {
      uint32_t bits = dist(rng);
      float value;
      std::memcpy(&value, &bits, sizeof(float));

      // Skip NaN and Inf
      if (!std::isnan(value) && !std::isinf(value)) {
         result.push_back(value);
      }
      else {
         --i; // retry
      }
   }
   return result;
}

std::vector<double> generate_random_doubles(size_t count, uint32_t seed = 12345)
{
   std::mt19937_64 rng(seed);
   std::uniform_int_distribution<uint64_t> dist;

   std::vector<double> result;
   result.reserve(count);

   for (size_t i = 0; i < count; ++i) {
      uint64_t bits = dist(rng);
      double value;
      std::memcpy(&value, &bits, sizeof(double));

      if (!std::isnan(value) && !std::isinf(value)) {
         result.push_back(value);
      }
      else {
         --i;
      }
   }
   return result;
}

std::vector<std::string> serialize_floats(const std::vector<float>& floats)
{
   std::vector<std::string> result;
   result.reserve(floats.size());
   char buf[32];
   for (float f : floats) {
      char* end = glz::simple_float::to_chars(buf, f);
      result.emplace_back(buf, end - buf);
   }
   return result;
}

std::vector<std::string> serialize_doubles(const std::vector<double>& doubles)
{
   std::vector<std::string> result;
   result.reserve(doubles.size());
   char buf[32];
   for (double d : doubles) {
      char* end = glz::simple_float::to_chars(buf, d);
      result.emplace_back(buf, end - buf);
   }
   return result;
}

int main()
{
   constexpr size_t N = 100000;

   // Pre-generate test data
   auto floats = generate_random_floats(N);
   auto doubles = generate_random_doubles(N);
   auto float_strings = serialize_floats(floats);
   auto double_strings = serialize_doubles(doubles);

   std::cout << "=== Float Serialization Benchmarks ===" << std::endl;
   {
      bencher::stage stage{"Float to_chars"};
      stage.min_execution_count = 50;

      stage.run("simple_float::to_chars (float)", [&] {
         char buf[32];
         size_t total_bytes = 0;
         for (float f : floats) {
            char* end = glz::simple_float::to_chars(buf, f);
            total_bytes += (end - buf);
            bencher::do_not_optimize(buf);
         }
         return total_bytes;
      });

      stage.run("std::to_chars (float)", [&] {
         char buf[32];
         size_t total_bytes = 0;
         for (float f : floats) {
            auto [ptr, ec] = std::to_chars(buf, buf + 32, f);
            total_bytes += (ptr - buf);
            bencher::do_not_optimize(buf);
         }
         return total_bytes;
      });

      bencher::print_results(stage);
   }

   std::cout << "\n=== Float Parsing Benchmarks ===" << std::endl;
   {
      bencher::stage stage{"Float from_chars"};
      stage.min_execution_count = 50;

      stage.run("simple_float::from_chars (float)", [&] {
         float value;
         size_t total_bytes = 0;
         for (const auto& s : float_strings) {
            glz::simple_float::from_chars<false>(s.data(), s.data() + s.size(), value);
            total_bytes += s.size();
            bencher::do_not_optimize(value);
         }
         return total_bytes;
      });

      stage.run("glz::from_chars/fast_float (float)", [&] {
         float value;
         size_t total_bytes = 0;
         for (const auto& s : float_strings) {
            glz::from_chars<false>(s.data(), s.data() + s.size(), value);
            total_bytes += s.size();
            bencher::do_not_optimize(value);
         }
         return total_bytes;
      });

      stage.run("std::from_chars (float)", [&] {
         float value;
         size_t total_bytes = 0;
         for (const auto& s : float_strings) {
            std::from_chars(s.data(), s.data() + s.size(), value);
            total_bytes += s.size();
            bencher::do_not_optimize(value);
         }
         return total_bytes;
      });

      bencher::print_results(stage);
   }

   std::cout << "\n=== Double Serialization Benchmarks ===" << std::endl;
   {
      bencher::stage stage{"Double to_chars"};
      stage.min_execution_count = 50;

      stage.run("simple_float::to_chars (double)", [&] {
         char buf[32];
         size_t total_bytes = 0;
         for (double d : doubles) {
            char* end = glz::simple_float::to_chars(buf, d);
            total_bytes += (end - buf);
            bencher::do_not_optimize(buf);
         }
         return total_bytes;
      });

      stage.run("std::to_chars (double)", [&] {
         char buf[32];
         size_t total_bytes = 0;
         for (double d : doubles) {
            auto [ptr, ec] = std::to_chars(buf, buf + 32, d);
            total_bytes += (ptr - buf);
            bencher::do_not_optimize(buf);
         }
         return total_bytes;
      });

      bencher::print_results(stage);
   }

   std::cout << "\n=== Double Parsing Benchmarks ===" << std::endl;
   {
      bencher::stage stage{"Double from_chars"};
      stage.min_execution_count = 50;

      stage.run("simple_float::from_chars (double)", [&] {
         double value;
         size_t total_bytes = 0;
         for (const auto& s : double_strings) {
            glz::simple_float::from_chars<false>(s.data(), s.data() + s.size(), value);
            total_bytes += s.size();
            bencher::do_not_optimize(value);
         }
         return total_bytes;
      });

      stage.run("glz::from_chars/fast_float (double)", [&] {
         double value;
         size_t total_bytes = 0;
         for (const auto& s : double_strings) {
            glz::from_chars<false>(s.data(), s.data() + s.size(), value);
            total_bytes += s.size();
            bencher::do_not_optimize(value);
         }
         return total_bytes;
      });

      stage.run("std::from_chars (double)", [&] {
         double value;
         size_t total_bytes = 0;
         for (const auto& s : double_strings) {
            std::from_chars(s.data(), s.data() + s.size(), value);
            total_bytes += s.size();
            bencher::do_not_optimize(value);
         }
         return total_bytes;
      });

      bencher::print_results(stage);
   }

   // Profile individual operations
   std::cout << "\n=== Breakdown: simple_float parsing stages ===" << std::endl;
   {
      bencher::stage stage{"Parsing breakdown"};
      stage.min_execution_count = 50;

      // Just decimal parsing
      stage.run("parse_decimal_strict only", [&] {
         size_t total = 0;
         for (const auto& s : float_strings) {
            glz::simple_float::detail::decimal_number dec{};
            const char* end =
               glz::simple_float::detail::parse_decimal_strict<false>(s.data(), s.data() + s.size(), dec);
            total += (end ? 1 : 0);
            bencher::do_not_optimize(dec);
         }
         return total;
      });

      // Full parsing
      stage.run("full from_chars", [&] {
         size_t total = 0;
         float value;
         for (const auto& s : float_strings) {
            auto [ptr, ec] = glz::simple_float::from_chars<false>(s.data(), s.data() + s.size(), value);
            total += (ptr ? 1 : 0);
            bencher::do_not_optimize(value);
         }
         return total;
      });

      bencher::print_results(stage);
   }

   return 0;
}
