#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <future>
#include <glaze/glaze.hpp>
#include <iostream>
#include <thread>
#include <vector>

// must be outside test(), compilation fails on gcc 13 otherwise
struct Value
{
   // use a short name to reduce the number of characters written
   float v;
};

void test()
{
   using UT = std::uint32_t;
   static_assert(sizeof(float) == sizeof(UT));

   auto test_one_value = [](const UT loopvar, auto& outbuf) {
      Value s;
      std::memcpy(&s.v, &loopvar, sizeof(float));

      if (!std::isfinite(s.v)) {
         return;
      }

      outbuf.clear();
      const auto writeec = glz::write_json(s, outbuf);

      if (writeec) [[unlikely]] {
         std::cerr << "failed writing " << s.v << " to json\n";
         std::abort();
      }

      auto restored = glz::read_json<Value>(outbuf);
      if (!restored) [[unlikely]] {
         std::cerr << "failed parsing " << outbuf << '\n';
         std::abort();
      }

      if (const auto r = restored.value().v; r != s.v) [[unlikely]] {
         std::cerr << "failed roundtrip, got " << r << " instead of " << s.v << //
            " (diff is " << r - s.v << ") when parsing " << outbuf << '\n';
         std::abort();
      }
   };

   auto test_all_in_range = [&](const UT start, const UT stop) {
      std::string outbuf;
      for (UT i = start; i < stop; ++i) {
         test_one_value(i, outbuf);
      }
   };

   const auto nthreads = std::thread::hardware_concurrency();
   const UT step = (std::numeric_limits<UT>::max)() / nthreads;

   std::vector<std::thread> threads;
   threads.reserve(nthreads);
   for (size_t threadi = 0; threadi < nthreads; ++threadi) {
      const UT start = threadi * step;
      const UT stop = (threadi == nthreads - 1) ? (std::numeric_limits<UT>::max)() : start + step;
      // std::cout << "thread i=" << threadi << " goes from " << start << " to " << stop << '\n';
      threads.emplace_back(test_all_in_range, start, stop);
   }
   // test the last value here.
   {
      std::string buf;
      test_one_value((std::numeric_limits<UT>::max)(), buf);
   }

   std::cout << "started testing in " << nthreads << " threads." << std::endl;
   for (auto& t : threads) {
      t.join();
   }
   std::cout << "tested " << (std::numeric_limits<UT>::max)() << " values of float" << std::endl;
}

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) { test(); }
