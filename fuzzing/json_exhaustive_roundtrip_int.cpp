#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <glaze/glaze.hpp>
#include <iostream>
#include <thread>
#include <vector>

// must be outside test(), compilation fails on gcc 13 otherwise
template <typename T>
struct Value
{
   T value{};
};

template <typename T>
void test()
{
   using S = Value<T>;
   using UT = std::make_unsigned_t<T>;

   auto testone = [](const UT loopvar, auto& outbuf) {
      S s;
      std::memcpy(&s.value, &loopvar, sizeof(T));

      outbuf.clear();
      const auto writeec = glz::write_json(s, outbuf);

      if (writeec) [[unlikely]] {
         std::cerr << "failed writing " << s.value << " to json\n";
         std::abort();
      }

      auto restored = glz::read_json<S>(outbuf);
      if (!restored) [[unlikely]] {
         std::cerr << "failed parsing " << outbuf << '\n';
         std::abort();
      }

      if (const auto r = restored.value().value; r != s.value) [[unlikely]] {
         std::cerr << "failed roundtrip, got " << r << " instead of " << s.value << //
            " (diff is " << r - s.value << ") when parsing " << outbuf << '\n';
         std::abort();
      }
   };

   auto testrange = [&](const UT start, const UT stop) {
      std::string outbuf;
      for (UT i = start; i < stop; ++i) {
         testone(i, outbuf);
      }
   };

   const auto nthreads = std::thread::hardware_concurrency();
   const UT step = (std::numeric_limits<UT>::max)() / nthreads;

   // can't use jthread, does not exist in all stdlibs.
   std::vector<std::thread> threads;
   threads.reserve(nthreads);
   for (size_t threadi = 0; threadi < nthreads; ++threadi) {
      const UT start = threadi * step;
      const UT stop = (threadi == nthreads - 1) ? (std::numeric_limits<UT>::max)() : start + step;
      // std::cout << "thread i=" << threadi << " goes from " << start << " to " << stop << '\n';
      threads.emplace_back(testrange, start, stop);
   }
   // test the last value here.
   {
      std::string buf;
      testone((std::numeric_limits<UT>::max)(), buf);
   }

   std::cout << "started testing in " << nthreads << " threads." << std::endl;
   for (auto& t : threads) {
      t.join();
   }
   std::cout << "tested " << (std::numeric_limits<UT>::max)() << " values of " << (std::is_unsigned_v<T> ? "un" : "")
             << "signed type of size " << sizeof(T) << std::endl;
}

void testonetype(int bits)
{
   switch (bits) {
   case 16:
      test<std::int16_t>();
      test<std::uint16_t>();
      break;
   case 32:
      test<std::int32_t>();
      test<std::uint32_t>();
      break;
   }
}

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
   testonetype(16);
   testonetype(32);
}
