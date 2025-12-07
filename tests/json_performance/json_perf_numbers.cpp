// Number performance tests - split from json_performance.cpp for faster compilation
#include <cstring>
#include <iostream>

#include "glaze/glaze.hpp"
#include "ut/ut.hpp"

using namespace ut;

struct integers
{
   int32_t a{};
   uint32_t b{};
   int64_t c{};
   uint64_t d{};
};

suite integers_test = [] {
   "integers"_test = [] {
#ifdef NDEBUG
      constexpr size_t n = 10000000;
#else
      constexpr size_t n = 100000;
#endif

      integers v{};

      std::string buffer;
      auto t0 = std::chrono::steady_clock::now();
      glz::error_ctx e;
      for (size_t i = 0; i < n; ++i) {
         v.a = int32_t(i);
         v.b = uint32_t(i);
         v.c = int64_t(i);
         v.d = uint64_t(i);
         std::ignore = glz::write_json(v, buffer);
         e = glz::read_json(v, buffer);
      }
      auto t1 = std::chrono::steady_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count() * 1e-6;
      std::cout << "integers read/write: " << duration << '\n';
   };
};

suite uint64_t_test = [] {
   "uint64_t"_test = [] {
#ifdef NDEBUG
      constexpr size_t n = 100000000;
#else
      constexpr size_t n = 100000;
#endif

      std::string buffer;
      auto t0 = std::chrono::steady_clock::now();
      glz::error_ctx e;
      for (size_t i = 0; i < n; ++i) {
         auto v = uint64_t(i);
         std::ignore = glz::write_json(v, buffer);
         e = glz::read_json(v, buffer);
         if (bool(e)) {
            break;
         }
      }
      expect(not e);
      auto t1 = std::chrono::steady_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count() * 1e-6;
      std::cout << "uint64_t read/write: " << duration << '\n';
   };
};

suite float_tests = [] {
   "float"_test = [] {
#ifdef NDEBUG
      constexpr size_t n = 10000000;
#else
      constexpr size_t n = 100000;
#endif

      float v{};

      std::string buffer;
      auto t0 = std::chrono::steady_clock::now();
      glz::error_ctx e;
      for (uint32_t i = 0; i < n; ++i) {
         std::ignore = glz::write_json(v, buffer);
         e = glz::read_json(v, buffer);
         std::memcpy(&v, &i, sizeof(float));
      }
      auto t1 = std::chrono::steady_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count() * 1e-6;
      std::cout << "float read/write: " << duration << '\n';
   };
};

int main() { return 0; }
