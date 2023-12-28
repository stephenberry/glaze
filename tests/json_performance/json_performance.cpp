#include <deque>
#include <iostream>
#include <map>
#include <random>
#include <unordered_map>

#include "boost/ut.hpp"
#include "glaze/glaze.hpp"

using namespace boost::ut;

std::mt19937 gen{};

static constexpr std::string_view charset{
   "!#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[]^_`abcdefghijklmnopqrstuvwxyz{|}~\"\\\r\b\f\t\n"};

inline std::string generate_string()
{
   auto length = std::uniform_int_distribution<uint32_t>{0, 512}(gen);
   const auto charsetSize = charset.size();
   std::uniform_int_distribution<uint32_t> distribution(0, charsetSize - 1);
   std::string result{};
   result.reserve(length);
   for (uint32_t x = 0; x < length; ++x) {
      result += charset[distribution(gen)];
   }
   return result;
}

suite string_performance = [] {
   "string_performance"_test = [] {
      constexpr size_t n = 100; // make this number bigger when profiling

      std::vector<std::string> vec;
      vec.reserve(n);

      for (size_t i = 0; i < n; ++i) {
         vec.emplace_back(generate_string());
      }

      std::string buffer;
      glz::write_json(vec, buffer);
      vec.clear();
      auto t0 = std::chrono::steady_clock::now();
      glz::parse_error e;
      for (auto i = 0; i < 100; ++i) {
         vec.clear();
         e = glz::read_json(vec, buffer);
      }
      auto t1 = std::chrono::steady_clock::now();

      expect(!e);

      auto duration = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count() * 1e-6;
      std::cerr << duration << '\n';
   };
};

int main()
{ // Explicitly run registered test suites and report errors
   // This prevents potential issues with thread local variables
   const auto result = boost::ut::cfg<>.run({.report_errors = true});
   return result;
}
