#define UT_RUN_TIME_ONLY

#include <deque>
#include <iostream>
#include <map>
#include <random>
#include <unordered_map>

#include "glaze/glaze.hpp"
#include "ut/ut.hpp"

using namespace ut;

template <class T>
bool test_samples()
{
   std::mt19937 gen{std::random_device{}()};
   std::uniform_int_distribution<T> dist{std::numeric_limits<T>::lowest(), (std::numeric_limits<T>::max)()};

   std::string buffer{};
   bool valid = true;
   T sample{};

   for (size_t i = 0; i < 100000; ++i) {
      sample = dist(gen);
      if (glz::write_json(sample, buffer)) {
         valid = false;
         break;
      }
      T value{};
      if (glz::read_json(value, buffer)) {
         valid = false;
         break;
      }
      if (value != sample) {
         valid = false;
         break;
      }
   }
   expect(valid) << sample;

   // test max and min values
   sample = (std::numeric_limits<T>::max)();
   if (glz::write_json(sample, buffer)) {
      valid = false;
   }
   T value{};
   if (glz::read_json(value, buffer)) {
      valid = false;
   }
   if (value != sample) {
      valid = false;
   }

   sample = std::numeric_limits<T>::lowest();
   if (glz::write_json(sample, buffer)) {
      valid = false;
   }
   value = {};
   if (glz::read_json(value, buffer)) {
      valid = false;
   }
   if (value != sample) {
      valid = false;
   }

   expect(valid) << sample;
   return valid;
}

template <class T>
bool test_to_max()
{
   std::string buffer{};
   bool valid = true;
   if constexpr (std::is_unsigned_v<T>) {
      for (uint64_t i = 0; i < (std::numeric_limits<T>::max)(); ++i) {
         if (glz::write_json(i, buffer)) {
            valid = false;
            break;
         }
         T value{};
         if (glz::read_json(value, buffer)) {
            valid = false;
            break;
         }
         if (value != T(i)) {
            valid = false;
            break;
         }
      }
   }
   else {
      for (int64_t i = std::numeric_limits<T>::lowest(); i < (std::numeric_limits<T>::max)(); ++i) {
         if (glz::write_json(i, buffer)) {
            valid = false;
            break;
         }
         T value{};
         if (glz::read_json(value, buffer)) {
            valid = false;
            break;
         }
         if (value != T(i)) {
            valid = false;
            break;
         }
      }
   }
   expect(valid) << buffer;
   return valid;
}

template <class T>
bool test_performance()
{
#ifdef NDEBUG
   constexpr size_t n = 10000000;
#else
   constexpr size_t n = 100000;
#endif

   std::mt19937 gen{};
   std::uniform_int_distribution<T> dist{std::numeric_limits<T>::lowest(), (std::numeric_limits<T>::max)()};

   std::string buffer;
   auto t0 = std::chrono::steady_clock::now();
   bool valid = true;
   for (size_t i = 0; i < n; ++i) {
      const auto sample = dist(gen);
      std::ignore = glz::write_json(sample, buffer);
      T v{};
      const auto e = glz::read_json(v, buffer);
      if (bool(e)) {
         valid = false;
         break;
      }
      if (v != sample) {
         valid = false;
         break;
      }
   }
   auto t1 = std::chrono::steady_clock::now();
   auto duration = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count() * 1e-6;
   std::cout << glz::name_v<T> << " read/write: " << duration << '\n';
   return valid;
}

template <class T>
struct vector_object
{
   std::vector<T> vec{};
};

template <class T>
void test_struct_with_array_minified()
{
   std::mt19937 gen{};
   std::uniform_int_distribution<T> dist{std::numeric_limits<T>::lowest(), (std::numeric_limits<T>::max)()};

   vector_object<T> obj;

   for (size_t i = 0; i < 1000; ++i) {
      obj.vec.emplace_back(dist(gen));
   }

   std::string buffer{};
   expect(not glz::write_json(obj, buffer));
   expect(not glz::read<glz::opts{.minified = true}>(obj, buffer));
}

template <class T>
bool test_single_char_performance()
{
#ifdef NDEBUG
   constexpr size_t n = 10000000;
#else
   constexpr size_t n = 100000;
#endif

   std::mt19937 gen{};
   std::uniform_int_distribution<T> dist{0, 9};

   std::string buffer;
   auto t0 = std::chrono::steady_clock::now();
   bool valid = true;
   for (size_t i = 0; i < n; ++i) {
      const auto sample = dist(gen);
      std::ignore = glz::write_json(sample, buffer);
      T v{};
      const auto e = glz::read_json(v, buffer);
      if (bool(e)) {
         valid = false;
         break;
      }
      if (v != sample) {
         valid = false;
         break;
      }
   }
   auto t1 = std::chrono::steady_clock::now();
   auto duration = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count() * 1e-6;
   std::cout << glz::name_v<T> << " read/write: " << duration << '\n';
   return valid;
}

template <class T>
bool test_lengths()
{
   using pair = std::pair<std::string_view, uint64_t>;
   static constexpr std::array<pair, 21> samples = {
      pair{"", 0ull}, //
      pair{"1", 1ull}, //
      pair{"12", 12ull}, //
      pair{"123", 123ull}, //
      pair{"1234", 1234ull}, //
      pair{"12345", 12345ull}, //
      pair{"123456", 123456ull}, //
      pair{"1234567", 1234567ull}, //
      pair{"12345678", 12345678ull}, //
      pair{"123456789", 123456789ull}, //
      pair{"1234567890", 1234567890ull}, //
      pair{"12345678901", 12345678901ull}, //
      pair{"123456789012", 123456789012ull}, //
      pair{"1234567890123", 1234567890123ull}, //
      pair{"12345678901234", 12345678901234ull}, //
      pair{"123456789012345", 123456789012345ull}, //
      pair{"1234567890123456", 1234567890123456ull}, //
      pair{"12345678901234567", 12345678901234567ull}, //
      pair{"123456789012345678", 123456789012345678ull}, //
      pair{"1234567890123456789", 1234567890123456789ull}, //
      pair{"12345678901234567890", 12345678901234567890ull}, //
   };

   std::string buffer{};
   bool valid = true;
   size_t max_length{};
   if constexpr (sizeof(T) == 1) {
      max_length = 3;
   }
   else if constexpr (sizeof(T) == 2) {
      max_length = 5;
   }
   else if constexpr (sizeof(T) == 4) {
      max_length = 10;
   }
   else if constexpr (std::same_as<T, uint64_t>) {
      max_length = 20;
   }
   else if constexpr (std::same_as<T, int64_t>) {
      max_length = 19;
   }

   for (size_t i = 1; i <= max_length; ++i) {
      const auto& p = samples[i];
      T value{};
      if (glz::read_json(value, p.first)) {
         valid = false;
         break;
      }
      if (uint64_t(value) != p.second) {
         valid = false;
         break;
      }
   }
   return valid;
}

// We test parsing an array so that early termination triggers errors
suite u8_test = [] {
   "u8 full"_test = [] { expect(test_to_max<uint8_t>()); };

   "u8 lengths"_test = [] { expect(test_lengths<uint8_t>()); };

   "u8"_test = [] {
      using V = uint8_t;
      std::array<V, 2> value{};
      expect(not glz::read_json(value, "[0, 0]"));
      expect(value == std::array<V, 2>{0, 0});

      expect(not glz::read_json(value, "[1e0, 1e1]"));
      expect(value == std::array<V, 2>{1, 10});

      expect(not glz::read_json(value, "[25e0, 25e1]"));
      expect(value == std::array<V, 2>{25, 250});

      expect(not glz::read_json(value, "[254, 255]"));
      expect(value == std::array<V, 2>{254, 255});

      expect(glz::read_json(value, "[1e-1]"));
      expect(glz::read_json(value, "[1.0]"));
      expect(glz::read_json(value, "[0.1]"));
      expect(glz::read_json(value, "[00]"));
      expect(glz::read_json(value, "[01]"));
      expect(glz::read_json(value, "[256]"));
   };

   "u8 performance"_test = [] {
#ifndef _MSC_VER
      expect(test_performance<uint8_t>());
#endif
   };

   "i8"_test = [] {
      using V = int8_t;
      std::array<V, 2> value{};
      expect(not glz::read_json(value, "[0, 0]"));
      expect(value == std::array<V, 2>{0, 0});

      expect(not glz::read_json(value, "[1e0, 1e1]"));
      expect(value == std::array<V, 2>{1, 10});

      expect(not glz::read_json(value, "[12e0, 12e1]"));
      expect(value == std::array<V, 2>{12, 120});

      expect(not glz::read_json(value, "[126, 127]"));
      expect(value == std::array<V, 2>{126, 127});

      expect(not glz::read_json(value, "[-127, -128]"));
      expect(value == std::array<V, 2>{-127, -128});

      expect(not glz::read_json(value, "[-2e1, -3e0]"));
      expect(value == std::array<V, 2>{-20, -3});

      expect(not glz::read_json(value, "[-99, -100]"));
      expect(value == std::array<V, 2>{-99, -100});

      expect(glz::read_json(value, "[1e-1]"));
      expect(glz::read_json(value, "[1.0]"));
      expect(glz::read_json(value, "[0.1]"));
      expect(glz::read_json(value, "[00]"));
      expect(glz::read_json(value, "[-00]"));
      expect(glz::read_json(value, "[01]"));
      expect(glz::read_json(value, "[128]"));
      expect(glz::read_json(value, "[1e3]"));
   };

   "i8 full"_test = [] { expect(test_to_max<int8_t>()); };

   "i8 lengths"_test = [] { expect(test_lengths<int8_t>()); };

   "u16"_test = [] {
      using V = uint16_t;
      std::array<V, 2> value{};
      expect(not glz::read_json(value, "[0, 0]"));
      expect(value == std::array<V, 2>{0, 0});

      expect(not glz::read_json(value, "[1e0, 1e1]"));
      expect(value == std::array<V, 2>{1, 10});

      expect(not glz::read_json(value, "[25e0, 25e1]"));
      expect(value == std::array<V, 2>{25, 250});

      expect(not glz::read_json(value, "[65534, 65535]"));
      expect(value == std::array<V, 2>{65534, 65535});

      expect(not glz::read_json(value, "[1e3, 1e3]"));
      expect(value == std::array<V, 2>{1000, 1000});

      expect(glz::read_json(value, "[1e-1]"));
      expect(glz::read_json(value, "[1.0]"));
      expect(glz::read_json(value, "[0.1]"));
      expect(glz::read_json(value, "[00]"));
      expect(glz::read_json(value, "[01]"));
      expect(glz::read_json(value, "[65536]"));
      expect(glz::read_json(value, "[65536e0]"));
      expect(glz::read_json(value, "[65535e1]"));
      expect(glz::read_json(value, "[1e7]"));
   };

   "u16 sample"_test = [] { expect(test_to_max<uint16_t>()); };

   "u16 lengths"_test = [] { expect(test_lengths<uint16_t>()); };

   "i16"_test = [] {
      using V = int16_t;
      std::array<V, 2> value{};
      expect(not glz::read_json(value, "[0, 0]"));
      expect(value == std::array<V, 2>{0, 0});

      expect(not glz::read_json(value, "[1e0, 1e1]"));
      expect(value == std::array<V, 2>{1, 10});

      expect(not glz::read_json(value, "[25e0, 25e1]"));
      expect(value == std::array<V, 2>{25, 250});

      expect(not glz::read_json(value, "[32766, 32767]"));
      expect(value == std::array<V, 2>{32766, 32767});

      expect(not glz::read_json(value, "[-32767, -32768]"));
      expect(value == std::array<V, 2>{-32767, -32768});

      expect(not glz::read_json(value, "[1e3, 1e3]"));
      expect(value == std::array<V, 2>{1000, 1000});

      expect(glz::read_json(value, "[1e-1]"));
      expect(glz::read_json(value, "[1.0]"));
      expect(glz::read_json(value, "[0.1]"));
      expect(glz::read_json(value, "[00]"));
      expect(glz::read_json(value, "[01]"));
      expect(glz::read_json(value, "[32768]"));
      expect(glz::read_json(value, "[65536]"));
      expect(glz::read_json(value, "[65536e0]"));
      expect(glz::read_json(value, "[65535e1]"));
      expect(glz::read_json(value, "[1e7]"));
   };

   "i16 full"_test = [] { expect(test_to_max<int16_t>()); };

   "i16 lengths"_test = [] { expect(test_lengths<int16_t>()); };

   "u32"_test = [] {
      using V = uint32_t;
      std::array<V, 2> value{};
      expect(not glz::read_json(value, "[0, 0]"));
      expect(value == std::array<V, 2>{0, 0});

      expect(not glz::read_json(value, "[1e0, 1e1]"));
      expect(value == std::array<V, 2>{1, 10});

      expect(not glz::read_json(value, "[25e0, 25e1]"));
      expect(value == std::array<V, 2>{25, 250});

      expect(not glz::read_json(value, "[4294967294, 4294967295]"));
      expect(value == std::array<V, 2>{4294967294, 4294967295});

      expect(not glz::read_json(value, "[3034613894, 3034613894]"));
      expect(value == std::array<V, 2>{3034613894, 3034613894});

      expect(not glz::read_json(value, "[1e7, 12e7]"));
      expect(value == std::array<V, 2>{10000000, 120000000});

      expect(glz::read_json(value, "[1e-1]"));
      expect(glz::read_json(value, "[1.0]"));
      expect(glz::read_json(value, "[0.1]"));
      expect(glz::read_json(value, "[00]"));
      expect(glz::read_json(value, "[01]"));
      expect(glz::read_json(value, "[4294967296]"));
      expect(glz::read_json(value, "[4294967296e0]"));
      expect(glz::read_json(value, "[1e10]"));
   };

   "u32 samples"_test = [] { expect(test_samples<uint32_t>()); };

   "u32 lengths"_test = [] { expect(test_lengths<uint32_t>()); };

   "i32"_test = [] {
      using V = int32_t;
      std::array<V, 2> value{};
      expect(not glz::read_json(value, "[0, 0]"));
      expect(value == std::array<V, 2>{0, 0});

      expect(not glz::read_json(value, "[1e0, 1e1]"));
      expect(value == std::array<V, 2>{1, 10});

      expect(not glz::read_json(value, "[25e0, 25e1]"));
      expect(value == std::array<V, 2>{25, 250});

      expect(not glz::read_json(value, "[2147483646, 2147483647]"));
      expect(value == std::array<V, 2>{2147483646, 2147483647});

      expect(not glz::read_json(value, "[-2147483647, -2147483648]"));
      expect(value == std::array<V, 2>{-2147483647, -2147483648});

      expect(not glz::read_json(value, "[1e7, 12e7]"));
      expect(value == std::array<V, 2>{10000000, 120000000});

      expect(glz::read_json(value, "[1e-1]"));
      expect(glz::read_json(value, "[1.0]"));
      expect(glz::read_json(value, "[0.1]"));
      expect(glz::read_json(value, "[00]"));
      expect(glz::read_json(value, "[01]"));
      expect(glz::read_json(value, "[2147483648]"));
      expect(glz::read_json(value, "[2147483648e0]"));
      expect(glz::read_json(value, "[1e10]"));
   };

   "i32 samples"_test = [] { expect(test_samples<int32_t>()); };

   "i32 lengths"_test = [] { expect(test_lengths<int32_t>()); };

   "u64"_test = [] {
      using V = uint64_t;
      std::array<V, 2> value{};
      expect(not glz::read_json(value, "[0, 0]"));
      expect(value == std::array<V, 2>{0, 0});

      expect(not glz::read_json(value, "[1e0, 1e1]"));
      expect(value == std::array<V, 2>{1, 10});

      expect(not glz::read_json(value, "[25e0, 25e1]"));
      expect(value == std::array<V, 2>{25, 250});

      expect(not glz::read_json(value, "[18446744073709551614, 18446744073709551615]"));
      expect(value == std::array<V, 2>{18446744073709551614ull, 18446744073709551615ull});

      expect(not glz::read_json(value, "[123456789, 123456789]"));
      expect(value == std::array<V, 2>{123456789, 123456789});

      expect(not glz::read_json(value, "[73241774740596, 73241774740596]"));
      expect(value == std::array<V, 2>{73241774740596, 73241774740596});

      expect(not glz::read_json(value, "[1e10, 12e10]"));
      expect(value == std::array<V, 2>{10000000000, 120000000000});

      expect(not glz::read<glz::opts{.minified = true}>(value, "[4774870093504525206]"));

      expect(glz::read_json(value, "[1e-1]"));
      expect(glz::read_json(value, "[1.0]"));
      expect(glz::read_json(value, "[0.1]"));
      expect(glz::read_json(value, "[00]"));
      expect(glz::read_json(value, "[01]"));
      expect(glz::read_json(value, "[18446744073709551616]"));
      expect(glz::read_json(value, "[18446744073709551616e0]"));
      expect(glz::read_json(value, "[1e20]"));
   };

   "u64 samples"_test = [] { expect(test_samples<uint64_t>()); };

   "u64 lengths"_test = [] { expect(test_lengths<uint64_t>()); };

   "u64 performance"_test = [] { expect(test_performance<uint64_t>()); };

   "u64 single char performance"_test = [] { expect(test_single_char_performance<uint64_t>()); };

   "u64 array minified"_test = [] { test_struct_with_array_minified<uint64_t>(); };

   "i64"_test = [] {
      using V = int64_t;
      std::array<V, 2> value{};
      expect(not glz::read_json(value, "[0, 0]"));
      expect(value == std::array<V, 2>{0, 0});
      
      expect(not glz::read_json(value, "[1e0, 1e1]"));
      expect(value == std::array<V, 2>{1, 10});
      
      expect(not glz::read_json(value, "[25e0, 25e1]"));
      expect(value == std::array<V, 2>{25, 250});
      
      expect(not glz::read_json(value, "[9223372036854775806, 9223372036854775807]"));
      expect(value == std::array<V, 2>{9223372036854775806ull, 9223372036854775807ull});
      
      expect(not glz::read_json(value, "[-9223372036854775808, -9223372036854775808e0]"));
      expect(value == std::array<V, 2>{std::numeric_limits<V>::lowest(), std::numeric_limits<V>::lowest()});
      
      expect(not glz::read_json(value, "[1e10, 12e10]"));
      expect(value == std::array<V, 2>{10000000000, 120000000000});
      
      expect(not glz::read_json(value, "[469490602178186175, 469490602178186175]"));
      expect(value == std::array<V, 2>{469490602178186175ull, 469490602178186175ull});
      
      expect(not glz::read_json(value, "[-356839120500334504, -356839120500334504]"));
      expect(value == std::array<V, 2>{-356839120500334504, -356839120500334504});
      
      expect(not glz::read_json(value, "[-5594732989048119398, -5594732989048119398]"));
      expect(value == std::array<V, 2>{-5594732989048119398, -5594732989048119398});
      
      expect(not glz::read<glz::opts{.minified = true}>(value, "[337184269,337184283]"));
      expect(not glz::read<glz::opts{.minified = true}>(value, "[-5637358391044507426,-4563386007050245647]"));

      expect(glz::read_json(value, "[1e-1]"));
      expect(glz::read_json(value, "[1.0]"));
      expect(glz::read_json(value, "[0.1]"));
      expect(glz::read_json(value, "[00]"));
      expect(glz::read_json(value, "[01]"));
      expect(glz::read_json(value, "[9223372036854775808]"));
      expect(glz::read_json(value, "[9223372036854775808e0]"));
      expect(glz::read_json(value, "[1e19]"));
   };

   "i64 samples"_test = [] {
#ifndef _MSC_VER
      expect(test_samples<int64_t>());
#endif
   };

   "i64 lengths"_test = [] { expect(test_lengths<int64_t>()); };

   "i64 performance"_test = [] { expect(test_performance<int64_t>()); };

   "i64 single char performance"_test = [] { expect(test_single_char_performance<int64_t>()); };

   "i64 array minified"_test = [] { test_struct_with_array_minified<int64_t>(); };
};

int main() { return 0; }
