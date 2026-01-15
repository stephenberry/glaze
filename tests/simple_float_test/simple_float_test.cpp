// Glaze Library
// For the license information refer to glaze.hpp

// Comprehensive tests for simple_float.hpp
// Tests roundtrip correctness of simple_float implementations

#include "glaze/util/simple_float.hpp"

#include <atomic>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "glaze/glaze.hpp"
#include "glaze/util/dtoa.hpp"
#include "glaze/util/glaze_fast_float.hpp"
#include "ut/ut.hpp"

using namespace ut;

// Helper to convert float bits to float
inline float bits_to_float(uint32_t bits)
{
   float f;
   std::memcpy(&f, &bits, sizeof(float));
   return f;
}

// Helper to convert double bits to double
inline double bits_to_double(uint64_t bits)
{
   double d;
   std::memcpy(&d, &bits, sizeof(double));
   return d;
}

// Helper to get float bits
inline uint32_t float_to_bits(float f)
{
   uint32_t bits;
   std::memcpy(&bits, &f, sizeof(float));
   return bits;
}

// Helper to get double bits
inline uint64_t double_to_bits(double d)
{
   uint64_t bits;
   std::memcpy(&bits, &d, sizeof(double));
   return bits;
}

// Compare two floats for roundtrip - allow zero sign mismatch for JSON compatibility
inline bool floats_roundtrip_equal(float a, float b)
{
   if (std::isnan(a) && std::isnan(b)) return true;
   if (a == 0.0f && b == 0.0f) return true; // Don't require sign of zero to match
   return a == b;
}

// Compare two doubles for roundtrip - allow zero sign mismatch for JSON compatibility
inline bool doubles_roundtrip_equal(double a, double b)
{
   if (std::isnan(a) && std::isnan(b)) return true;
   if (a == 0.0 && b == 0.0) return true; // Don't require sign of zero to match
   return a == b;
}

// ULP distance calculation (useful for debugging precision issues)
inline int64_t double_ulp_distance(double a, double b)
{
   if (a == b) return 0;
   if (std::isnan(a) || std::isnan(b)) return INT64_MAX;

   int64_t a_bits, b_bits;
   std::memcpy(&a_bits, &a, sizeof(double));
   std::memcpy(&b_bits, &b, sizeof(double));

   // Handle sign difference
   if ((a_bits < 0) != (b_bits < 0)) return INT64_MAX;

   return std::abs(a_bits - b_bits);
}

// Test parsing: compare simple_float::from_chars vs glz::from_chars (fast_float)
template <typename T>
bool test_parse_equivalence(const char* input)
{
   T simple_result{};
   T fast_result{};

   const char* end = input + std::strlen(input);

   auto [simple_ptr, simple_ec] = glz::simple_float::from_chars<false>(input, end, simple_result);
   auto [fast_ptr, fast_ec] = glz::from_chars<false>(input, end, fast_result);

   // Both should succeed or both should fail
   if ((simple_ec == std::errc{}) != (fast_ec == std::errc{})) {
      return false;
   }

   // If both failed, that's fine
   if (simple_ec != std::errc{}) {
      return true;
   }

   // Check that values match exactly (allowing sign of zero to differ)
   // 128-bit integer arithmetic ensures exact results on all platforms
   if constexpr (std::is_same_v<T, float>) {
      return floats_roundtrip_equal(simple_result, fast_result);
   }
   else {
      return doubles_roundtrip_equal(simple_result, fast_result);
   }
}

// Test roundtrip: value -> string -> value
// Requires exact match for both floats and doubles on all platforms
// 128-bit integer arithmetic ensures correct rounding
template <typename T>
bool test_roundtrip(T value)
{
   if (std::isnan(value) || std::isinf(value)) {
      // These serialize to "null" which doesn't roundtrip
      return true;
   }

   char buf[64]{};
   char* end = glz::simple_float::to_chars(buf, value);

   T parsed{};
   auto [ptr, ec] = glz::simple_float::from_chars<true>(buf, end, parsed);

   if (ec != std::errc{}) {
      return false;
   }

   if constexpr (std::is_same_v<T, float>) {
      return floats_roundtrip_equal(parsed, value);
   }
   else {
      return doubles_roundtrip_equal(parsed, value);
   }
}

suite simple_float_parse_tests = [] {
   "parse_float_specific_cases"_test = [] {
      // Basic integers
      expect(test_parse_equivalence<float>("0"));
      expect(test_parse_equivalence<float>("1"));
      expect(test_parse_equivalence<float>("-1"));
      expect(test_parse_equivalence<float>("123"));
      expect(test_parse_equivalence<float>("-456"));
      expect(test_parse_equivalence<float>("999999"));

      // Basic decimals
      expect(test_parse_equivalence<float>("0.0"));
      expect(test_parse_equivalence<float>("0.5"));
      expect(test_parse_equivalence<float>("1.5"));
      expect(test_parse_equivalence<float>("-1.5"));
      expect(test_parse_equivalence<float>("3.14159"));
      expect(test_parse_equivalence<float>("0.123456"));

      // Scientific notation
      expect(test_parse_equivalence<float>("1e0"));
      expect(test_parse_equivalence<float>("1e1"));
      expect(test_parse_equivalence<float>("1e10"));
      expect(test_parse_equivalence<float>("1e-10"));
      expect(test_parse_equivalence<float>("1.5e5"));
      expect(test_parse_equivalence<float>("-2.5e-3"));
      expect(test_parse_equivalence<float>("1E10"));
      expect(test_parse_equivalence<float>("1e+10"));

      // Edge cases
      expect(test_parse_equivalence<float>("0.000001"));
      expect(test_parse_equivalence<float>("0.0000001"));
      expect(test_parse_equivalence<float>("1000000"));
      expect(test_parse_equivalence<float>("10000000"));

      // Very large/small
      expect(test_parse_equivalence<float>("1e30"));
      expect(test_parse_equivalence<float>("1e35"));
      expect(test_parse_equivalence<float>("1e-30"));
   };

   "parse_double_specific_cases"_test = [] {
      // Basic integers
      expect(test_parse_equivalence<double>("0"));
      expect(test_parse_equivalence<double>("1"));
      expect(test_parse_equivalence<double>("-1"));
      expect(test_parse_equivalence<double>("123"));
      expect(test_parse_equivalence<double>("-456"));
      expect(test_parse_equivalence<double>("999999999999"));

      // Basic decimals
      expect(test_parse_equivalence<double>("0.0"));
      expect(test_parse_equivalence<double>("0.5"));
      expect(test_parse_equivalence<double>("1.5"));
      expect(test_parse_equivalence<double>("-1.5"));
      expect(test_parse_equivalence<double>("3.141592653589793"));
      expect(test_parse_equivalence<double>("2.718281828459045"));
      expect(test_parse_equivalence<double>("0.123456789012345"));

      // Scientific notation
      expect(test_parse_equivalence<double>("1e0"));
      expect(test_parse_equivalence<double>("1e1"));
      expect(test_parse_equivalence<double>("1e100"));
      expect(test_parse_equivalence<double>("1e-100"));
      expect(test_parse_equivalence<double>("1.5e200"));
      expect(test_parse_equivalence<double>("-2.5e-200"));
   };
};

suite simple_float_roundtrip_tests = [] {
   "roundtrip_float_specific_cases"_test = [] {
      expect(test_roundtrip(0.0f));
      expect(test_roundtrip(-0.0f));
      expect(test_roundtrip(1.0f));
      expect(test_roundtrip(-1.0f));
      expect(test_roundtrip(0.5f));
      expect(test_roundtrip(3.14159f));
      expect(test_roundtrip(1e10f));
      expect(test_roundtrip(1e-10f));
      expect(test_roundtrip(1e20f));
      expect(test_roundtrip(1e-20f));
      expect(test_roundtrip(123456.789f));
      expect(test_roundtrip(0.00012345f));
   };

   "roundtrip_double_specific_cases"_test = [] {
      expect(test_roundtrip(0.0));
      expect(test_roundtrip(-0.0));
      expect(test_roundtrip(1.0));
      expect(test_roundtrip(-1.0));
      expect(test_roundtrip(0.5));
      expect(test_roundtrip(3.141592653589793));
      expect(test_roundtrip(2.718281828459045));
      expect(test_roundtrip(1e50));
      expect(test_roundtrip(1e-50));
      expect(test_roundtrip(123456789.123456789));
   };
};

// Exhaustive test for all 2^32 float values - optimized for speed
// This test is disabled by default (glaze_SIMPLE_FLOAT_TEST=OFF) and runs in Release mode via dedicated CI
suite exhaustive_float_tests = [] {
   "exhaustive_float_roundtrip"_test = [] {
      std::cout << "\n=== Exhaustive float roundtrip test (all 2^32 values) ===" << std::endl;

      const unsigned num_threads = std::max(1u, std::thread::hardware_concurrency());
      std::cout << "Using " << num_threads << " threads" << std::endl;

      constexpr uint64_t total_values = 0x100000000ULL; // 2^32
      const uint64_t chunk_size = total_values / num_threads;

      // Thread-local results to avoid atomic contention
      struct ThreadResult
      {
         uint64_t passed{0};
         uint64_t skipped{0};
         uint32_t first_failure{UINT32_MAX};
      };
      std::vector<ThreadResult> results(num_threads);

      auto worker = [&](unsigned thread_id, uint64_t start, uint64_t end_range) {
         ThreadResult& result = results[thread_id];
         char buf[32]; // Minimal buffer for float serialization

         for (uint64_t bits = start; bits < end_range; ++bits) {
            float value;
            std::memcpy(&value, &bits, sizeof(float));

            // Skip NaN and Inf (they serialize to "null")
            if (std::isnan(value) || std::isinf(value)) {
               ++result.skipped;
               continue;
            }

            // Serialize
            char* buf_end = glz::simple_float::to_chars(buf, value);
            *buf_end = '\0'; // Null-terminate for from_chars<true>

            // Parse back
            float parsed;
            auto [ptr, ec] = glz::simple_float::from_chars<true>(buf, buf_end, parsed);

            if (ec == std::errc{} && floats_roundtrip_equal(parsed, value)) {
               ++result.passed;
            }
            else if (result.first_failure == UINT32_MAX) {
               result.first_failure = static_cast<uint32_t>(bits);
            }
         }
      };

      auto start_time = std::chrono::high_resolution_clock::now();

      std::vector<std::thread> threads;
      threads.reserve(num_threads);
      for (unsigned i = 0; i < num_threads; ++i) {
         uint64_t start = i * chunk_size;
         uint64_t end_range = (i == num_threads - 1) ? total_values : (i + 1) * chunk_size;
         threads.emplace_back(worker, i, start, end_range);
      }

      for (auto& t : threads) {
         t.join();
      }

      auto end_time = std::chrono::high_resolution_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

      // Aggregate results
      uint64_t total_passed = 0;
      uint64_t total_skipped = 0;
      uint32_t first_failure = UINT32_MAX;

      for (const auto& r : results) {
         total_passed += r.passed;
         total_skipped += r.skipped;
         if (r.first_failure < first_failure) {
            first_failure = r.first_failure;
         }
      }

      uint64_t expected_pass = total_values - total_skipped;
      uint64_t failures = expected_pass - total_passed;

      std::cout << "Exhaustive float roundtrip: total=" << total_values << ", passed=" << total_passed
                << ", skipped=" << total_skipped << std::endl;
      std::cout << "Time: " << duration.count() << " ms (" << (total_values * 1000 / (duration.count() + 1))
                << " values/sec)" << std::endl;

      if (failures > 0 && first_failure != UINT32_MAX) {
         float fail_value = bits_to_float(first_failure);
         char buf[64]{};
         char* end = glz::simple_float::to_chars(buf, fail_value);
         std::cerr << "First failure at bits=0x" << std::hex << first_failure << std::dec << " value=" << fail_value
                   << " serialized=" << std::string_view(buf, end - buf) << std::endl;
      }

      std::cout << "Failures: " << failures << std::endl;
      expect(failures == 0) << "All non-special floats must roundtrip exactly";
   };
};

// Random tests for double values
suite random_double_tests = [] {
   "random_double_roundtrip"_test = [] {
      std::random_device rd;
      uint64_t seed = rd();
      std::cout << "\n=== Random double roundtrip test (seed=" << seed << ") ===" << std::endl;

      std::mt19937_64 rng(seed);
      std::uniform_int_distribution<uint64_t> dist;

      constexpr uint64_t num_tests = 1'000'000; // 1 million random doubles
      uint64_t passed = 0;
      uint64_t skipped = 0;
      bool first_failure_logged = false;

      for (uint64_t i = 0; i < num_tests; ++i) {
         uint64_t bits = dist(rng);
         double value = bits_to_double(bits);

         if (std::isnan(value) || std::isinf(value)) {
            ++skipped;
            continue;
         }

         if (test_roundtrip(value)) {
            ++passed;
         }
         else if (!first_failure_logged) {
            first_failure_logged = true;
            char buf[64]{};
            char* end = glz::simple_float::to_chars(buf, value);
            std::cerr << "First double roundtrip failure at bits=0x" << std::hex << bits << std::dec
                      << " value=" << value << " serialized=" << std::string_view(buf, end - buf) << std::endl;
         }
      }

      std::cout << "Random double roundtrip: total=" << num_tests << ", passed=" << passed << ", skipped=" << skipped
                << std::endl;

      uint64_t tested = num_tests - skipped;
      uint64_t failures = tested - passed;
      std::cout << "Failures: " << failures << std::endl;

      // Require 0% failure rate - we fixed the bugs that caused failures
      expect(failures == 0) << "All random doubles must roundtrip exactly";
   };

   "random_double_parse_equivalence"_test = [] {
      std::random_device rd;
      uint64_t seed = rd();
      std::cout << "\n=== Random double parse equivalence test (seed=" << seed << ") ===" << std::endl;

      std::mt19937_64 rng(seed);
      std::uniform_int_distribution<uint64_t> dist;

      constexpr uint64_t num_tests = 1'000'000; // 1 million tests
      uint64_t passed = 0;
      uint64_t skipped = 0;
      bool first_failure_logged = false;

      for (uint64_t i = 0; i < num_tests; ++i) {
         uint64_t bits = dist(rng);
         double value = bits_to_double(bits);

         if (std::isnan(value) || std::isinf(value)) {
            ++skipped;
            continue;
         }

         // Serialize with dragonbox, then parse with both parsers
         char buf[64]{};
         char* end = glz::to_chars(buf, value);
         *end = '\0';

         if (test_parse_equivalence<double>(buf)) {
            ++passed;
         }
         else if (!first_failure_logged) {
            first_failure_logged = true;
            std::cerr << "First double parse failure for input: " << buf << std::endl;
         }
      }

      std::cout << "Random double parse equivalence: total=" << num_tests << ", passed=" << passed
                << ", skipped=" << skipped << std::endl;

      uint64_t tested = num_tests - skipped;
      uint64_t failures = tested - passed;
      std::cout << "Failures: " << failures << std::endl;

      // Require exact equivalence with fast_float
      expect(failures == 0) << "All doubles must parse identically to fast_float";
   };
};

// Regression tests for known hard cases discovered during development
suite regression_tests = [] {
   "leading_fractional_zeros_double"_test = [] {
      // These values test the parser fix for leading fractional zeros
      // The bug was counting leading zeros after decimal point as significant digits
      std::vector<const char*> test_cases = {
         "0.00000000000000001",
         "0.000000000000000001",
         "0.0000000000000000001",
         "0.00000000000000000001",
         "0.000000000000000000001",
         "0.0023051120089116243", // Original failing case
         "0.001",
         "0.0001",
         "0.00001",
         "0.000001",
         "0.0000001",
         "0.00000001",
         "-0.00000000000000001",
         "-0.0023051120089116243",
      };

      int passed = 0;
      for (const char* input : test_cases) {
         double parsed{};
         const char* end = input + std::strlen(input);

         // Parse with simple_float
         auto result = glz::simple_float::from_chars<false>(input, end, parsed);

         if (result.ec == std::errc{}) {
            // Now test roundtrip
            char buf[64]{};
            char* buf_end = glz::simple_float::to_chars(buf, parsed);

            double reparsed{};
            auto reparse_result = glz::simple_float::from_chars<true>(buf, buf_end, reparsed);

            if (reparse_result.ec == std::errc{} && doubles_roundtrip_equal(reparsed, parsed)) {
               ++passed;
            }
            else {
               std::cerr << "Leading zeros roundtrip failure: " << input << " -> "
                         << std::string_view(buf, buf_end - buf) << std::endl;
            }
         }
         else {
            std::cerr << "Leading zeros parse failure: " << input << std::endl;
         }
      }

      std::cout << "Leading fractional zeros: " << passed << "/" << test_cases.size() << " passed" << std::endl;
      expect(passed == static_cast<int>(test_cases.size()));
   };

   "known_hard_bit_patterns_double"_test = [] {
      // Specific bit patterns that were known to cause failures before fixes
      std::vector<uint64_t> hard_patterns = {
         0xfface22e6775c7bc, // Required rounding fix (-1.0141348953347229734e+307)
         0x7fefffffffffffff, // Largest normal double
         0x0010000000000000, // Smallest normal double
         0x000fffffffffffff, // Largest subnormal double
         0x0000000000000001, // Smallest positive subnormal
         0x8000000000000001, // Smallest negative subnormal
         0x4340000000000000, // 2^53 (integer boundary)
         0x4330000000000000, // 2^52
         0x3ff0000000000000, // 1.0
         0xbff0000000000000, // -1.0
         0x3fe0000000000000, // 0.5
         0x3fb999999999999a, // 0.1 (not exactly representable)
         0x3fd3333333333333, // 0.3 (not exactly representable)
      };

      int passed = 0;
      for (uint64_t bits : hard_patterns) {
         double value = bits_to_double(bits);

         if (std::isnan(value) || std::isinf(value)) {
            ++passed; // Skip special values
            continue;
         }

         if (test_roundtrip(value)) {
            ++passed;
         }
         else {
            char buf[64]{};
            char* end = glz::simple_float::to_chars(buf, value);
            std::cerr << "Hard pattern failure: bits=0x" << std::hex << bits << std::dec << " value=" << value
                      << " serialized=" << std::string_view(buf, end - buf) << std::endl;
         }
      }

      std::cout << "Known hard bit patterns: " << passed << "/" << hard_patterns.size() << " passed" << std::endl;
      expect(passed == static_cast<int>(hard_patterns.size()));
   };

   "rounding_boundary_values"_test = [] {
      // Test values where the 17th digit is exactly 5 (rounding boundary)
      // These require correct round-half-up behavior
      std::vector<const char*> boundary_cases = {
         "1.2345678901234565e100", "1.2345678901234565e-100", "9.9999999999999995e200",
         "1.0000000000000005e0",   "-1.2345678901234565e100", "-9.9999999999999995e200",
      };

      int passed = 0;
      for (const char* input : boundary_cases) {
         double parsed{};
         const char* end = input + std::strlen(input);
         auto result = glz::simple_float::from_chars<false>(input, end, parsed);

         if (result.ec == std::errc{}) {
            if (test_roundtrip(parsed)) {
               ++passed;
            }
         }
      }

      std::cout << "Rounding boundary values: " << passed << "/" << boundary_cases.size() << " passed" << std::endl;
      // Note: Not all boundary cases may parse to distinct values
      expect(passed >= static_cast<int>(boundary_cases.size()) - 2);
   };

   "sequential_doubles_near_critical_regions"_test = [] {
      // Test 100 consecutive doubles near critical exponent regions
      std::vector<uint64_t> critical_starts = {
         0x7fe0000000000000, // Near max exponent
         0x0010000000000000, // Near min normal
         0x000fffffffffffff, // Subnormal region
         0x4340000000000000, // Near 2^53
         0x3ff0000000000000, // Near 1.0
      };

      int total_passed = 0;
      int total_tested = 0;

      for (uint64_t start : critical_starts) {
         for (uint64_t offset = 0; offset < 100; ++offset) {
            uint64_t bits = start + offset;
            double value = bits_to_double(bits);

            if (std::isnan(value) || std::isinf(value)) {
               continue;
            }

            ++total_tested;
            if (test_roundtrip(value)) {
               ++total_passed;
            }
         }
      }

      std::cout << "Sequential doubles near critical regions: " << total_passed << "/" << total_tested << " passed"
                << std::endl;
      expect(total_passed == total_tested);
   };
};

// Subnormal (denormalized) double tests
suite subnormal_tests = [] {
   "subnormal_double_roundtrip"_test = [] {
      // Test subnormal doubles (exponent field is 0, mantissa != 0)
      // These are the smallest representable positive doubles
      std::cout << "\n=== Subnormal double roundtrip test ===" << std::endl;

      int passed = 0;
      int total = 0;

      // Test specific subnormal patterns
      std::vector<uint64_t> subnormal_patterns = {
         0x0000000000000001, // Smallest positive subnormal
         0x0000000000000002,
         0x0000000000000010,
         0x0000000000000100,
         0x0000000000001000,
         0x0000000000010000,
         0x0000000000100000,
         0x0000000001000000,
         0x0000000010000000,
         0x0000000100000000,
         0x0000001000000000,
         0x0000010000000000,
         0x0000100000000000,
         0x0001000000000000,
         0x000fffffffffffff, // Largest subnormal
         0x0008000000000000, // Middle subnormal
         0x0004000000000000,
         0x0002000000000000,
         // Negative subnormals
         0x8000000000000001,
         0x800fffffffffffff,
      };

      for (uint64_t bits : subnormal_patterns) {
         double value = bits_to_double(bits);
         ++total;

         if (test_roundtrip(value)) {
            ++passed;
         }
         else {
            char buf[64]{};
            char* end = glz::simple_float::to_chars(buf, value);
            std::cerr << "Subnormal failure: bits=0x" << std::hex << bits << std::dec << " value=" << value
                      << " serialized=" << std::string_view(buf, end - buf) << std::endl;
         }
      }

      std::cout << "Subnormal patterns: " << passed << "/" << total << " passed" << std::endl;
      expect(passed == total);
   };

   "random_subnormal_roundtrip"_test = [] {
      // Test random subnormals (more comprehensive)
      std::random_device rd;
      uint64_t seed = rd();
      std::cout << "Random subnormals (seed=" << seed << "): ";

      std::mt19937_64 rng(seed);
      std::uniform_int_distribution<uint64_t> dist(1, 0x000fffffffffffffULL);

      constexpr int num_tests = 10000;
      int passed = 0;

      for (int i = 0; i < num_tests; ++i) {
         uint64_t mantissa = dist(rng);
         uint64_t sign = (rng() & 1) ? 0x8000000000000000ULL : 0;
         uint64_t bits = sign | mantissa;
         double value = bits_to_double(bits);

         if (test_roundtrip(value)) {
            ++passed;
         }
      }

      double pass_rate = static_cast<double>(passed) / num_tests * 100.0;
      std::cout << passed << "/" << num_tests << " passed (" << pass_rate << "%)" << std::endl;

      expect(passed == num_tests) << "All subnormals should roundtrip exactly";
   };
};

// Test extreme exponent values
suite extreme_exponent_tests = [] {
   "extreme_positive_exponents"_test = [] {
      // Test doubles with very large positive exponents (near overflow)
      int passed = 0;
      int total = 0;

      // Start from near the maximum exponent
      for (int exp = 300; exp <= 308; ++exp) {
         for (double mantissa = 1.0; mantissa < 10.0; mantissa += 0.5) {
            double value = mantissa * std::pow(10.0, exp);
            if (!std::isinf(value)) {
               ++total;
               if (test_roundtrip(value)) {
                  ++passed;
               }
            }
         }
      }

      std::cout << "Extreme positive exponents: " << passed << "/" << total << " passed" << std::endl;
      expect(passed == total);
   };

   "extreme_negative_exponents"_test = [] {
      // Test doubles with very small negative exponents (near underflow)
      int passed = 0;
      int total = 0;

      for (int exp = -300; exp >= -308; --exp) {
         for (double mantissa = 1.0; mantissa < 10.0; mantissa += 0.5) {
            double value = mantissa * std::pow(10.0, exp);
            if (value != 0.0) {
               ++total;
               if (test_roundtrip(value)) {
                  ++passed;
               }
            }
         }
      }

      std::cout << "Extreme negative exponents: " << passed << "/" << total << " passed" << std::endl;
      expect(passed == total);
   };
};

// Edge case tests for specific problematic patterns
suite edge_case_tests = [] {
   "powers_of_two_float"_test = [] {
      // Test powers of 2 representable as float
      int passed = 0;
      int total = 0;
      for (int exp = -126; exp <= 127; ++exp) {
         float value = std::ldexp(1.0f, exp);
         ++total;
         if (test_roundtrip(value)) {
            ++passed;
         }
      }
      std::cout << "Powers of 2 (float): " << passed << "/" << total << " passed" << std::endl;
      expect(passed == total);
   };

   "powers_of_two_double"_test = [] {
      // Test powers of 2 representable as double
      int passed = 0;
      int total = 0;
      for (int exp = -1022; exp <= 1023; ++exp) {
         double value = std::ldexp(1.0, exp);
         ++total;
         if (test_roundtrip(value)) {
            ++passed;
         }
      }
      std::cout << "Powers of 2 (double): " << passed << "/" << total << " passed" << std::endl;
      expect(passed == total);
   };

   "powers_of_ten_float"_test = [] {
      // Test powers of 10 in float range
      int passed = 0;
      int total = 0;
      for (int exp = -38; exp <= 38; ++exp) {
         float value = std::pow(10.0f, static_cast<float>(exp));
         if (!std::isinf(value) && value != 0.0f) {
            ++total;
            if (test_roundtrip(value)) {
               ++passed;
            }
         }
      }
      std::cout << "Powers of 10 (float): " << passed << "/" << total << " passed" << std::endl;
      expect(passed == total);
   };

   "powers_of_ten_double"_test = [] {
      // Test powers of 10 in double range
      int passed = 0;
      int total = 0;
      for (int exp = -300; exp <= 300; ++exp) {
         double value = std::pow(10.0, static_cast<double>(exp));
         if (!std::isinf(value) && value != 0.0) {
            ++total;
            if (test_roundtrip(value)) {
               ++passed;
            }
         }
      }
      std::cout << "Powers of 10 (double): " << passed << "/" << total << " passed" << std::endl;
      expect(passed == total);
   };

   "integer_values"_test = [] {
      // Test integer values that should have exact representation
      int passed = 0;
      int total = 0;
      for (int64_t i = -10000; i <= 10000; ++i) {
         double value = static_cast<double>(i);
         ++total;
         if (test_roundtrip(value)) {
            ++passed;
         }
      }
      std::cout << "Integer values: " << passed << "/" << total << " passed" << std::endl;
      expect(passed == total);
   };

   "common_fractions"_test = [] {
      // Test common fractions
      std::vector<double> fractions = {0.1, 0.2, 0.25,  0.3,   0.4,   0.5,   0.6,    0.7,   0.75,
                                       0.8, 0.9, 0.125, 0.375, 0.625, 0.875, 0.0625, 0.1875};
      int passed = 0;
      for (double f : fractions) {
         if (test_roundtrip(f) && test_roundtrip(-f)) {
            ++passed;
         }
      }
      std::cout << "Common fractions: " << passed << "/" << fractions.size() << " passed" << std::endl;
      expect(passed == static_cast<int>(fractions.size()));
   };
};

// Tests for invalid JSON number inputs
// The parser should reject these according to JSON spec (RFC 8259)
suite invalid_input_tests = [] {
   // Helper to test that parsing fails or doesn't consume all input
   auto should_reject = [](const char* input, const char* description) -> bool {
      float f{};
      double d{};
      const char* end = input + std::strlen(input);

      // Test float parsing
      auto [f_ptr, f_ec] = glz::simple_float::from_chars<false>(input, end, f);
      bool float_rejected = (f_ec != std::errc{}) || (f_ptr != end);

      // Test double parsing
      auto [d_ptr, d_ec] = glz::simple_float::from_chars<false>(input, end, d);
      bool double_rejected = (d_ec != std::errc{}) || (d_ptr != end);

      if (!float_rejected || !double_rejected) {
         std::cerr << "Should reject '" << input << "' (" << description << ")"
                   << " float_rejected=" << float_rejected << " double_rejected=" << double_rejected << std::endl;
      }

      return float_rejected && double_rejected;
   };

   // Helper to test that parsing fails completely (returns error, not partial parse)
   auto should_fail = [](const char* input, const char* description) -> bool {
      float f{};
      double d{};
      const char* end = input + std::strlen(input);

      auto [f_ptr, f_ec] = glz::simple_float::from_chars<false>(input, end, f);
      auto [d_ptr, d_ec] = glz::simple_float::from_chars<false>(input, end, d);

      bool float_failed = (f_ec != std::errc{});
      bool double_failed = (d_ec != std::errc{});

      if (!float_failed || !double_failed) {
         std::cerr << "Should fail on '" << input << "' (" << description << ")"
                   << " float_failed=" << float_failed << " double_failed=" << double_failed << std::endl;
      }

      return float_failed && double_failed;
   };

   "empty_and_whitespace"_test = [&] {
      std::cout << "Testing empty and whitespace inputs..." << std::endl;
      expect(should_fail("", "empty string"));
      expect(should_fail(" ", "single space"));
      expect(should_fail("  ", "multiple spaces"));
      expect(should_fail("\t", "tab"));
      expect(should_fail("\n", "newline"));
   };

   "sign_only"_test = [&] {
      std::cout << "Testing sign-only inputs..." << std::endl;
      expect(should_fail("-", "minus only"));
      expect(should_fail("+", "plus only"));
      expect(should_fail("--", "double minus"));
      expect(should_fail("++", "double plus"));
   };

   "leading_plus_sign"_test = [&] {
      // JSON does not allow leading + sign
      std::cout << "Testing leading plus sign (invalid in JSON)..." << std::endl;
      expect(should_reject("+1", "plus one"));
      expect(should_reject("+0", "plus zero"));
      expect(should_reject("+1.5", "plus 1.5"));
      expect(should_reject("+1e5", "plus with exponent"));
      expect(should_reject("+0.5", "plus 0.5"));
   };

   "leading_zeros"_test = [&] {
      // JSON does not allow leading zeros (except 0 itself and 0.xxx)
      std::cout << "Testing leading zeros (invalid in JSON)..." << std::endl;
      expect(should_reject("01", "zero-one"));
      expect(should_reject("007", "double-oh-seven"));
      expect(should_reject("00", "double zero"));
      expect(should_reject("00.5", "double zero point five"));
      expect(should_reject("-01", "negative zero-one"));
      expect(should_reject("-007", "negative double-oh-seven"));
   };

   "decimal_point_issues"_test = [&] {
      std::cout << "Testing decimal point issues..." << std::endl;
      // Just decimal point
      expect(should_fail(".", "decimal point only"));
      expect(should_fail("-.", "minus decimal point"));

      // Trailing decimal (no digits after)
      expect(should_reject("1.", "trailing decimal"));
      expect(should_reject("123.", "trailing decimal after digits"));
      expect(should_reject("-1.", "negative trailing decimal"));

      // Leading decimal (no digits before) - invalid in JSON
      expect(should_reject(".1", "leading decimal"));
      expect(should_reject(".5", "leading decimal .5"));
      expect(should_reject("-.5", "negative leading decimal"));
      expect(should_reject(".1e5", "leading decimal with exponent"));

      // Multiple decimal points
      expect(should_reject("1.2.3", "multiple decimals"));
      expect(should_reject("1..2", "double decimal"));
      expect(should_reject("..1", "double leading decimal"));
   };

   "exponent_issues"_test = [&] {
      std::cout << "Testing exponent issues..." << std::endl;
      // Empty exponent
      expect(should_reject("1e", "empty exponent lowercase"));
      expect(should_reject("1E", "empty exponent uppercase"));
      expect(should_reject("1e+", "exponent with plus only"));
      expect(should_reject("1e-", "exponent with minus only"));
      expect(should_reject("1.5e", "decimal with empty exponent"));
      expect(should_reject("1.5E+", "decimal with exponent plus only"));

      // Exponent without mantissa
      expect(should_fail("e5", "exponent without mantissa"));
      expect(should_fail("E10", "uppercase exponent without mantissa"));
      expect(should_fail("e+5", "exponent with sign, no mantissa"));

      // Multiple exponents
      expect(should_reject("1e2e3", "multiple exponents"));
      expect(should_reject("1E2E3", "multiple uppercase exponents"));
      expect(should_reject("1e2E3", "mixed case multiple exponents"));

      // Exponent with decimal
      expect(should_reject("1e2.5", "exponent with decimal"));
      expect(should_reject("1e.5", "exponent with leading decimal"));
   };

   "multiple_signs"_test = [&] {
      std::cout << "Testing multiple/misplaced signs..." << std::endl;
      expect(should_reject("--1", "double minus"));
      expect(should_reject("++1", "double plus"));
      expect(should_reject("-+1", "minus plus"));
      expect(should_reject("+-1", "plus minus"));
      expect(should_reject("1-", "trailing minus"));
      expect(should_reject("1+", "trailing plus"));
      expect(should_reject("1.5-", "decimal with trailing minus"));
      expect(should_reject("1.5+2", "plus in middle"));
      expect(should_reject("1.5-2", "minus in middle (not exponent)"));
   };

   "letters_and_invalid_chars"_test = [&] {
      std::cout << "Testing letters and invalid characters..." << std::endl;
      expect(should_reject("1a", "digit then letter"));
      expect(should_reject("a1", "letter then digit"));
      expect(should_reject("abc", "letters only"));
      expect(should_reject("1.2x3", "letter in decimal"));
      expect(should_reject("1,5", "comma instead of decimal"));
      expect(should_reject("1_000", "underscore separator"));
      expect(should_reject("1'000", "quote separator"));
      expect(should_reject("$100", "dollar sign"));
      expect(should_reject("1.5f", "float suffix"));
      expect(should_reject("1.5d", "double suffix"));
      expect(should_reject("1.5L", "long suffix"));
      expect(should_reject("0x1F", "hex literal"));
      expect(should_reject("0b101", "binary literal"));
      expect(should_reject("0o777", "octal literal"));
   };

   "special_values"_test = [&] {
      // These are not valid JSON numbers
      std::cout << "Testing special values (not valid JSON)..." << std::endl;
      expect(should_fail("NaN", "NaN uppercase"));
      expect(should_fail("nan", "nan lowercase"));
      expect(should_fail("NAN", "NAN all caps"));
      expect(should_fail("Inf", "Inf"));
      expect(should_fail("inf", "inf lowercase"));
      expect(should_fail("INF", "INF all caps"));
      expect(should_fail("Infinity", "Infinity"));
      expect(should_fail("infinity", "infinity lowercase"));
      expect(should_fail("-Infinity", "negative Infinity"));
      expect(should_fail("-inf", "negative inf"));
      expect(should_fail("+Infinity", "positive Infinity"));
      expect(should_fail("+inf", "positive inf"));
   };

   "whitespace_in_number"_test = [&] {
      std::cout << "Testing whitespace in number..." << std::endl;
      // Leading whitespace - should fail or not consume whitespace
      expect(should_reject(" 1", "leading space"));
      expect(should_reject("\t1", "leading tab"));

      // Trailing whitespace - parser may accept number and stop before whitespace
      // This is OK for from_chars style parsing, so we check it doesn't consume the space
      {
         const char* input = "1 ";
         const char* end = input + std::strlen(input);
         double d{};
         auto [ptr, ec] = glz::simple_float::from_chars<false>(input, end, d);
         // Should either fail or stop at the space (not consume it)
         expect((ec != std::errc{}) || (ptr == input + 1)) << "Should not consume trailing space";
      }

      // Whitespace in the middle
      expect(should_reject("1 .5", "space before decimal"));
      expect(should_reject("1. 5", "space after decimal"));
      expect(should_reject("1 e5", "space before exponent"));
      expect(should_reject("1e 5", "space in exponent"));
      expect(should_reject("1e+ 5", "space after exponent sign"));
      expect(should_reject("- 1", "space after minus"));
   };

   "overflow_and_underflow"_test = [&] {
      std::cout << "Testing overflow and underflow..." << std::endl;
      // These should either fail or return inf/0
      // The key is they shouldn't crash or produce garbage

      // Extreme overflow
      {
         const char* input = "1e999999999";
         const char* end = input + std::strlen(input);
         double d{};
         auto [ptr, ec] = glz::simple_float::from_chars<false>(input, end, d);
         // Should either fail or return inf
         expect((ec != std::errc{}) || std::isinf(d)) << "Extreme overflow should fail or return inf";
      }

      // Extreme underflow
      {
         const char* input = "1e-999999999";
         const char* end = input + std::strlen(input);
         double d{};
         auto [ptr, ec] = glz::simple_float::from_chars<false>(input, end, d);
         // Should either fail or return 0
         expect((ec != std::errc{}) || d == 0.0) << "Extreme underflow should fail or return 0";
      }

      // Very long mantissa
      {
         std::string long_mantissa = "1";
         for (int i = 0; i < 1000; ++i) {
            long_mantissa += "0";
         }
         const char* input = long_mantissa.c_str();
         const char* end = input + long_mantissa.size();
         double d{};
         [[maybe_unused]] auto result = glz::simple_float::from_chars<false>(input, end, d);
         // Should handle gracefully (not crash)
         expect(true) << "Long mantissa should not crash";
      }
   };

   "valid_edge_cases"_test = [&] {
      // These SHOULD be accepted - verify we don't reject valid input
      std::cout << "Testing valid edge cases (should be accepted)..." << std::endl;

      auto should_accept = [](const char* input, const char* description) -> bool {
         double d{};
         const char* end = input + std::strlen(input);
         auto [ptr, ec] = glz::simple_float::from_chars<false>(input, end, d);
         bool accepted = (ec == std::errc{}) && (ptr == end);
         if (!accepted) {
            std::cerr << "Should accept '" << input << "' (" << description << ")" << std::endl;
         }
         return accepted;
      };

      expect(should_accept("0", "zero"));
      expect(should_accept("-0", "negative zero"));
      expect(should_accept("0.0", "zero point zero"));
      expect(should_accept("0.5", "zero point five"));
      expect(should_accept("-0.5", "negative zero point five"));
      expect(should_accept("1", "one"));
      expect(should_accept("-1", "negative one"));
      expect(should_accept("123", "integer"));
      expect(should_accept("1.5", "simple decimal"));
      expect(should_accept("1e5", "exponent"));
      expect(should_accept("1E5", "uppercase exponent"));
      expect(should_accept("1e+5", "exponent with plus"));
      expect(should_accept("1e-5", "exponent with minus"));
      expect(should_accept("1.5e10", "decimal with exponent"));
      expect(should_accept("1.5E+10", "decimal with uppercase exponent and plus"));
      expect(should_accept("1.5e-10", "decimal with negative exponent"));
      expect(should_accept("0e0", "zero exponent"));
      expect(should_accept("0.0e0", "zero decimal with zero exponent"));
      expect(should_accept("123456789", "large integer"));
      expect(should_accept("0.123456789", "many decimal digits"));
      expect(should_accept("1.7976931348623157e308", "near max double"));
      expect(should_accept("2.2250738585072014e-308", "near min normal double"));
   };
};

int main() { return 0; }
