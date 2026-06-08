// Glaze Library
// For the license information refer to glaze.hpp

#include <cassert>
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

#include "glaze/simd/neon.hpp"
#include "glaze/glaze.hpp"

int main()
{
#if defined(GLZ_USE_NEON)
   std::cout << "Starting NEON SIMD String Escape Benchmark..." << std::endl;
   
   std::string test_str = "This is a long string with some \"quotes\" and \\ backslashes to trigger escape handling. "
                          "We want it to be long enough so that NEON 64-byte wide path and 16-byte tail paths are both "
                          "heavily exercised. \"Quote\" and \\ backslash. Let's repeat this structure: "
                          "This is a long string with some \"quotes\" and \\ backslashes to trigger escape handling. "
                          "We want it to be long enough so that NEON 64-byte wide path and 16-byte tail paths are both "
                          "heavily exercised. \"Quote\" and \\ backslash.";
   
   std::string out_orig;
   out_orig.resize(test_str.size() * 2);
   std::string out_opt;
   out_opt.resize(test_str.size() * 2);

   constexpr size_t escape_iterations = 200000;
   
   // Warmup
   {
      const char* c = test_str.data();
      const char* e = test_str.data() + test_str.size();
      char* data = out_orig.data();
      glz::detail::original::neon_string_escape(c, e, data, test_str.size(), []() {});
      
      c = test_str.data();
      data = out_opt.data();
      glz::detail::optimized::neon_string_escape(c, e, data, test_str.size(), []() {});
   }

   // Benchmark Original
   auto start_orig = std::chrono::high_resolution_clock::now();
   for (size_t i = 0; i < escape_iterations; ++i) {
      const char* c = test_str.data();
      const char* e = test_str.data() + test_str.size();
      char* data = out_orig.data();
      glz::detail::original::neon_string_escape(c, e, data, test_str.size(), [&]() { *data++ = '\\'; *data++ = 't'; });
      asm volatile("" : "+g"(c), "+g"(data) : : "memory");
   }
   auto end_orig = std::chrono::high_resolution_clock::now();
   auto dur_orig = std::chrono::duration_cast<std::chrono::microseconds>(end_orig - start_orig).count();

   // Benchmark Optimized
   auto start_opt = std::chrono::high_resolution_clock::now();
   for (size_t i = 0; i < escape_iterations; ++i) {
      const char* c = test_str.data();
      const char* e = test_str.data() + test_str.size();
      char* data = out_opt.data();
      glz::detail::optimized::neon_string_escape(c, e, data, test_str.size(), [&]() { *data++ = '\\'; *data++ = 't'; });
      asm volatile("" : "+g"(c), "+g"(data) : : "memory");
   }
   auto end_opt = std::chrono::high_resolution_clock::now();
   auto dur_opt = std::chrono::duration_cast<std::chrono::microseconds>(end_opt - start_opt).count();

   std::cout << "\n==================================================" << std::endl;
   std::cout << "NEON SIMD STRING ESCAPE BENCHMARK RESULTS" << std::endl;
   std::cout << "==================================================" << std::endl;
   std::cout << "Original NEON (Scalar Scan):   " << dur_orig << " us" << std::endl;
   std::cout << "Optimized NEON (Movemask O(1)): " << dur_opt << " us" << std::endl;
   std::cout << "==================================================" << std::endl;
   double neon_overhead = (static_cast<double>(dur_opt - dur_orig) / dur_orig) * 100.0;
   std::cout << "NEON Optimization Difference: " << neon_overhead << "% (negative is faster)" << std::endl;
   std::cout << "==================================================\n" << std::endl;
#else
   std::cout << "NEON is not supported on this architecture. Benchmark skipped." << std::endl;
#endif

   return 0;
}
