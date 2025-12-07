// Benchmark performance tests - split from json_performance.cpp for faster compilation
// This file tests large objects with 26 vector fields
#include <limits>
#include <random>

#include "glaze/glaze.hpp"
#include "json_perf_common.hpp"
#include "ut/ut.hpp"

using namespace ut;
using namespace glz::perf;

inline std::string emoji_unicode(auto& generator)
{
   // Define Unicode ranges for emojis
   static const std::vector<std::pair<char32_t, char32_t>> emoji_ranges = {
      {0x1F600, 0x1F64F}, // Emoticons
      {0x1F300, 0x1F5FF}, // Misc Symbols and Pictographs
      {0x1F680, 0x1F6FF}, // Transport and Map Symbols
      {0x2600, 0x26FF}, // Misc symbols
      {0x2700, 0x27BF}, // Dingbats
      {0x1F900, 0x1F9FF}, // Supplemental Symbols and Pictographs
      {0x1FA70, 0x1FAFF} // Symbols and Pictographs Extended-A
   };

   // Calculate total number of emojis
   size_t total_emojis = 0;
   for (const auto& range : emoji_ranges) {
      total_emojis += range.second - range.first + 1;
   }

   // Generate a random emoji code point
   std::uniform_int_distribution<size_t> dis(0, total_emojis - 1);
   size_t random_index = dis(generator);

   char32_t cpoint = 0;
   for (const auto& range : emoji_ranges) {
      size_t range_size = range.second - range.first + 1;
      if (random_index < range_size) {
         cpoint = char32_t(range.first + random_index);
         break;
      }
      random_index -= range_size;
   }

   // Convert the code point to UTF-8
   std::string result;
   if (cpoint <= 0x7F) {
      result.push_back(static_cast<char>(cpoint));
   }
   else if (cpoint <= 0x7FF) {
      result.push_back(static_cast<char>(0xC0 | ((cpoint >> 6) & 0x1F)));
      result.push_back(static_cast<char>(0x80 | (cpoint & 0x3F)));
   }
   else if (cpoint <= 0xFFFF) {
      result.push_back(static_cast<char>(0xE0 | ((cpoint >> 12) & 0x0F)));
      result.push_back(static_cast<char>(0x80 | ((cpoint >> 6) & 0x3F)));
      result.push_back(static_cast<char>(0x80 | (cpoint & 0x3F)));
   }
   else {
      result.push_back(static_cast<char>(0xF0 | ((cpoint >> 18) & 0x07)));
      result.push_back(static_cast<char>(0x80 | ((cpoint >> 12) & 0x3F)));
      result.push_back(static_cast<char>(0x80 | ((cpoint >> 6) & 0x3F)));
      result.push_back(static_cast<char>(0x80 | (cpoint & 0x3F)));
   }

   return result;
}

struct test_struct
{
   std::vector<std::string> testStrings{};
   std::vector<uint64_t> testUints{};
   std::vector<double> testDoubles{};
   std::vector<int64_t> testInts{};
   std::vector<bool> testBools{};
};

template <>
struct glz::meta<test_struct>
{
   using T = test_struct;
   static constexpr auto parseValue =
      object(&T::testStrings, &T::testUints, &T::testDoubles, &T::testInts, &T::testBools);
};

template <class T>
struct test_generator
{
   std::vector<T> a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z;

   std::mt19937_64 gen{1};

   static constexpr std::string_view charset{
      "!#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[]^_`abcdefghijklmnopqrstuvwxyz{|}~\"\\\r\b\f\t\n"};

   template <typename value_type_new>
   value_type_new randomizeNumberNormal(value_type_new mean, value_type_new stdDeviation)
   {
      std::normal_distribution<> normalDistributionTwo{static_cast<double>(mean), static_cast<double>(stdDeviation)};
      auto theResult = normalDistributionTwo(gen);
      if (theResult < 0) {
         theResult = -theResult;
      }
      return static_cast<value_type_new>(theResult);
   }

   template <class V>
   V randomizeNumberUniform(V range)
   {
      std::uniform_int_distribution<uint64_t> dis(0, uint64_t(range));
      return static_cast<V>(dis(gen));
   }

   void insertUnicodeInJSON(std::string& jsonString) { jsonString += emoji_unicode(gen); }

   std::string generateString()
   {
      auto length{randomizeNumberNormal(64.0f, 16.0f)};
      static constexpr auto charsetSize = charset.size();
      auto unicodeCount = randomizeNumberUniform(length / 8);
      std::string result{};
      for (int32_t ix = 0; ix < length; ++ix) {
         if (ix == static_cast<int32_t>(length / unicodeCount)) {
            insertUnicodeInJSON(result);
         }
         result += charset[randomizeNumberUniform(charsetSize - 1)];
      }
      return result;
   }

   double generateDouble()
   {
      auto newValue = randomizeNumberNormal(double{}, (std::numeric_limits<double>::max)() / 50000000);
      return generateBool() ? newValue : -newValue;
   };

   bool generateBool() { return static_cast<bool>(randomizeNumberNormal(50.0f, 50.0f) >= 50.0f); };

   uint64_t generateUint()
   {
      return randomizeNumberNormal((std::numeric_limits<uint64_t>::max)() / 2,
                                   (std::numeric_limits<uint64_t>::max)() / 2);
   };

   int64_t generateInt()
   {
      auto newValue = randomizeNumberNormal(int64_t{}, (std::numeric_limits<int64_t>::max)());
      return generateBool() ? newValue : -newValue;
   };

   test_generator()
   {
      auto fill = [&](auto& v) {
         auto arraySize01 = randomizeNumberNormal(35ull, 10ull);
         auto arraySize02 = randomizeNumberNormal(15ull, 10ull);
         auto arraySize03 = randomizeNumberNormal(5ull, 1ull);
         v.resize(arraySize01);
         for (uint64_t x = 0; x < arraySize01; ++x) {
            auto arr = randomizeNumberNormal(arraySize02, arraySize03);
            for (uint64_t y = 0; y < arr; ++y) {
               auto newString = generateString();
               v[x].testStrings.emplace_back(newString);
            }
            arr = randomizeNumberNormal(arraySize02, arraySize03);
            for (uint64_t y = 0; y < arr; ++y) {
               v[x].testUints.emplace_back(generateUint());
            }
            arr = randomizeNumberNormal(arraySize02, arraySize03);
            for (uint64_t y = 0; y < arr; ++y) {
               v[x].testInts.emplace_back(generateInt());
            }
            arr = randomizeNumberNormal(arraySize02, arraySize03);
            for (uint64_t y = 0; y < arr; ++y) {
               auto newBool = generateBool();
               v[x].testBools.emplace_back(newBool);
            }
            arr = randomizeNumberNormal(arraySize02, arraySize03);
            for (uint64_t y = 0; y < arr; ++y) {
               v[x].testDoubles.emplace_back(generateDouble());
            }
         }
      };

      fill(a);
      fill(b);
      fill(c);
      fill(d);
      fill(e);
      fill(f);
      fill(g);
      fill(h);
      fill(i);
      fill(j);
      fill(k);
      fill(l);
      fill(m);
      fill(n);
      fill(o);
      fill(p);
      fill(q);
      fill(r);
      fill(s);
      fill(t);
      fill(u);
      fill(v);
      fill(w);
      fill(x);
      fill(y);
      fill(z);
   }
};

template <>
struct glz::meta<test_generator<test_struct>>
{
   using T = test_generator<test_struct>;
   static constexpr auto value =
      object("a", &T::a, "b", &T::b, "c", &T::c, "d", &T::d, "e", &T::e, "f", &T::f, "g", &T::g, "h", &T::h, "i", &T::i,
             "j", &T::j, "k", &T::k, "l", &T::l, "m", &T::m, "n", &T::n, "o", &T::o, "p", &T::p, "q", &T::q, "r", &T::r,
             "s", &T::s, "t", &T::t, "u", &T::u, "v", &T::v, "w", &T::w, "x", &T::x, "y", &T::y, "z", &T::z);
};

template <auto Opts>
auto benchmark_tester()
{
   std::string buffer{};
   test_generator<test_struct> obj{};
#ifdef NDEBUG
   constexpr size_t N = 300;
#else
   constexpr size_t N = 30;
#endif

   expect(!glz::write_file_json(obj, "benchmark_minified.json", std::string{}));

   if (glz::write_json(obj, buffer)) {
      throw std::runtime_error("error");
   }

   auto t0 = std::chrono::steady_clock::now();

   for (size_t i = 0; i < N; ++i) {
      if (glz::read<Opts>(obj, buffer)) {
         std::cout << "glaze error!\n";
         break;
      }
      if (glz::write_json(obj, buffer)) {
         std::cout << "glaze error!\n";
         break;
      }
   }

   auto t1 = std::chrono::steady_clock::now();

   results r{Opts.minified ? "Glaze (.minified)" : "Glaze", "https://github.com/stephenberry/glaze", N};
   r.json_roundtrip = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count() * 1e-6;

   // write performance
   t0 = std::chrono::steady_clock::now();

   for (size_t i = 0; i < N; ++i) {
      if (glz::write<Opts>(obj, buffer)) {
         std::cout << "glaze error!\n";
         break;
      }
   }

   t1 = std::chrono::steady_clock::now();

   r.json_byte_length = buffer.size();
   minified_byte_length = *r.json_byte_length;
   r.json_write = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count() * 1e-6;

   // read performance

   t0 = std::chrono::steady_clock::now();

   for (size_t i = 0; i < N; ++i) {
      if (glz::read_json(obj, buffer)) {
         std::cout << "glaze error!\n";
         break;
      }
   }

   t1 = std::chrono::steady_clock::now();

   r.json_read = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count() * 1e-6;

   // beve write performance

   t0 = std::chrono::steady_clock::now();

   for (size_t i = 0; i < N; ++i) {
      if (glz::write_beve(obj, buffer)) {
         std::cout << "glaze error!\n";
         break;
      }
   }

   t1 = std::chrono::steady_clock::now();

   r.binary_byte_length = buffer.size();
   r.beve_write = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count() * 1e-6;

   // beve read performance

   t0 = std::chrono::steady_clock::now();

   for (size_t i = 0; i < N; ++i) {
      if (glz::read_beve(obj, buffer)) {
         std::cout << "glaze error!\n";
         break;
      }
   }

   t1 = std::chrono::steady_clock::now();

   r.beve_read = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count() * 1e-6;

   // beve round trip

   t0 = std::chrono::steady_clock::now();

   for (size_t i = 0; i < N; ++i) {
      if (glz::read_beve(obj, buffer)) {
         std::cout << "glaze error!\n";
         break;
      }
      if (glz::write_beve(obj, buffer)) {
         std::cout << "glaze error!\n";
         break;
      }
   }

   t1 = std::chrono::steady_clock::now();

   r.beve_roundtrip = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count() * 1e-6;

   r.print();

   return r;
}

suite benchmark_test = [] { "benchmark"_test = [] { benchmark_tester<glz::opts{}>(); }; };

int main() { return 0; }
