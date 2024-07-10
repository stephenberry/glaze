#define UT_RUN_TIME_ONLY

#include <deque>
#include <iostream>
#include <map>
#include <random>
#include <unordered_map>

#include "glaze/glaze.hpp"
#include "ut/ut.hpp"

using namespace ut;

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
#ifdef NDEBUG
      constexpr size_t n = 10000;
#else
      constexpr size_t n = 100;
#endif

      std::vector<std::string> vec;
      vec.reserve(n);

      for (size_t i = 0; i < n; ++i) {
         vec.emplace_back(generate_string());
      }

      std::string buffer;
      auto t0 = std::chrono::steady_clock::now();
      for (auto i = 0; i < 100; ++i) {
         std::ignore = glz::write_json(vec, buffer);
      }
      auto t1 = std::chrono::steady_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count() * 1e-6;
      std::cout << "write: " << duration << '\n';

      vec.clear();
      t0 = std::chrono::steady_clock::now();
      glz::error_ctx e;
      for (auto i = 0; i < 100; ++i) {
         vec.clear();
         e = glz::read_json(vec, buffer);
      }
      t1 = std::chrono::steady_clock::now();

      expect(!e) << glz::format_error(e, buffer);

      duration = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count() * 1e-6;
      std::cout << "read: " << duration << '\n';
      std::cout << '\n';
   };
};

struct integers
{
   int32_t a{};
   uint32_t b{};
   int64_t c{};
   uint64_t d{};
};

suite default_numerics = [] {
   "default numerics"_test = [] {
#ifdef NDEBUG
      constexpr size_t n = 10000000;
#else
      constexpr size_t n = 100000;
#endif

      integers ints_obj{};

      std::string buffer;
      auto t0 = std::chrono::steady_clock::now();
      for (size_t i = 0; i < n; ++i) {
         std::ignore = glz::write_json(ints_obj, buffer);
      }
      auto t1 = std::chrono::steady_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count() * 1e-6;
      std::cout << "write integer: " << duration << '\n';

      t0 = std::chrono::steady_clock::now();
      glz::error_ctx e;
      for (size_t i = 0; i < n; ++i) {
         e = glz::read_json(ints_obj, buffer);
      }
      t1 = std::chrono::steady_clock::now();

      expect(!e) << glz::format_error(e, buffer);

      duration = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count() * 1e-6;
      std::cout << "read integer: " << duration << '\n';
      std::cout << '\n';
   };
};

[[maybe_unused]] constexpr std::string_view json_minified =
   R"({"fixed_object":{"int_array":[0,1,2,3,4,5,6],"float_array":[0.1,0.2,0.3,0.4,0.5,0.6],"double_array":[3288398.238,2.33e+24,28.9,0.928759872,0.22222848,0.1,0.2,0.3,0.4]},"fixed_name_object":{"name0":"James","name1":"Abraham","name2":"Susan","name3":"Frank","name4":"Alicia"},"another_object":{"string":"here is some text","another_string":"Hello World","escaped_text":"{\"some key\":\"some string value\"}","boolean":false,"nested_object":{"v3s":[[0.12345,0.23456,0.001345],[0.3894675,97.39827,297.92387],[18.18,87.289,2988.298]],"id":"298728949872"}},"string_array":["Cat","Dog","Elephant","Tiger"],"string":"Hello world","number":3.14,"boolean":true,"another_bool":false})";

// #define USE_PURE_REFLECTION

struct fixed_object_t
{
   std::vector<int> int_array;
   std::vector<float> float_array;
   std::vector<double> double_array;
};

#ifndef USE_PURE_REFLECTION
template <>
struct glz::meta<fixed_object_t>
{
   using T = fixed_object_t;
   static constexpr auto value = object(&T::int_array, &T::float_array, &T::double_array);
};
#endif

struct fixed_name_object_t
{
   std::string name0{};
   std::string name1{};
   std::string name2{};
   std::string name3{};
   std::string name4{};
};

#ifndef USE_PURE_REFLECTION
template <>
struct glz::meta<fixed_name_object_t>
{
   using T = fixed_name_object_t;
   static constexpr auto value = object(&T::name0, &T::name1, &T::name2, &T::name3, &T::name4);
};
#endif

struct nested_object_t
{
   std::vector<std::array<double, 3>> v3s{};
   std::string id{};
};

#ifndef USE_PURE_REFLECTION
template <>
struct glz::meta<nested_object_t>
{
   using T = nested_object_t;
   static constexpr auto value = object(&T::v3s, &T::id);
};
#endif

struct another_object_t
{
   std::string string{};
   std::string another_string{};
   std::string escaped_text{};
   bool boolean{};
   nested_object_t nested_object{};
};

#ifndef USE_PURE_REFLECTION
template <>
struct glz::meta<another_object_t>
{
   using T = another_object_t;
   static constexpr auto value =
      object(&T::string, &T::another_string, &T::escaped_text, &T::boolean, &T::nested_object);
};
#endif

struct obj_t
{
   fixed_object_t fixed_object{};
   fixed_name_object_t fixed_name_object{};
   another_object_t another_object{};
   std::vector<std::string> string_array{};
   std::string string{};
   double number{};
   bool boolean{};
   bool another_bool{};
};

#ifndef USE_PURE_REFLECTION
template <>
struct glz::meta<obj_t>
{
   using T = obj_t;
   static constexpr auto value = object(&T::fixed_object, &T::fixed_name_object, &T::another_object, &T::string_array,
                                        &T::string, &T::number, &T::boolean, &T::another_bool);
};
#endif

// We scale all speeds by the minified JSON byte length, so that libraries which do not efficiently write JSON do not
// get an unfair advantage We want to know how fast the libraries will serialize/deserialize with repsect to one another
[[maybe_unused]] size_t minified_byte_length{};
#ifdef NDEBUG
[[maybe_unused]] constexpr size_t iterations = 1'000'000;
#else
[[maybe_unused]] constexpr size_t iterations = 100'000;
#endif

struct results
{
   std::string_view name{};
   std::string_view url{};
   size_t iterations{};

   std::optional<size_t> json_byte_length{};
   std::optional<double> json_read{};
   std::optional<double> json_write{};
   std::optional<double> json_roundtrip{};

   std::optional<size_t> binary_byte_length{};
   std::optional<double> binary_write{};
   std::optional<double> binary_read{};
   std::optional<double> binary_roundtrip{};

   void print(bool use_minified = true)
   {
      if (json_roundtrip) {
         std::cout << name << " json roundtrip: " << *json_roundtrip << " s\n";
      }

      if (json_byte_length) {
         std::cout << name << " json byte length: " << *json_byte_length << '\n';
      }

      if (json_write) {
         if (json_byte_length) {
            const auto byte_length = use_minified ? minified_byte_length : *json_byte_length;
            const auto MBs = iterations * byte_length / (*json_write * 1048576);
            std::cout << name << " json write: " << *json_write << " s, " << MBs << " MB/s\n";
         }
         else {
            std::cout << name << " json write: " << *json_write << " s\n";
         }
      }

      if (json_read) {
         if (json_byte_length) {
            const auto byte_length = use_minified ? minified_byte_length : *json_byte_length;
            const auto MBs = iterations * byte_length / (*json_read * 1048576);
            std::cout << name << " json read: " << *json_read << " s, " << MBs << " MB/s\n";
         }
         else {
            std::cout << name << " json read: " << *json_read << " s\n";
         }
      }

      if (binary_roundtrip) {
         std::cout << '\n';
         std::cout << name << " binary roundtrip: " << *binary_roundtrip << " s\n";
      }

      if (binary_byte_length) {
         std::cout << name << " binary byte length: " << *binary_byte_length << '\n';
      }

      if (binary_write) {
         if (binary_byte_length) {
            const auto MBs = iterations * *binary_byte_length / (*binary_write * 1048576);
            std::cout << name << " binary write: " << *binary_write << " s, " << MBs << " MB/s\n";
         }
         else {
            std::cout << name << " binary write: " << *binary_write << " s\n";
         }
      }

      if (binary_read) {
         if (binary_byte_length) {
            const auto MBs = iterations * *binary_byte_length / (*binary_read * 1048576);
            std::cout << name << " binary read: " << *binary_read << " s, " << MBs << " MB/s\n";
         }
         else {
            std::cout << name << " binary read: " << *binary_read << " s\n";
         }
      }

      std::cout << "\n---\n" << std::endl;
   }
};

template <glz::opts Opts>
auto glaze_test()
{
   std::string buffer{json_minified};

   obj_t obj;

   auto t0 = std::chrono::steady_clock::now();

   for (size_t i = 0; i < iterations; ++i) {
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

   results r{Opts.minified ? "Glaze (.minified)" : "Glaze", "https://github.com/stephenberry/glaze", iterations};
   r.json_roundtrip = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count() * 1e-6;

   // write performance
   t0 = std::chrono::steady_clock::now();

   for (size_t i = 0; i < iterations; ++i) {
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

   for (size_t i = 0; i < iterations; ++i) {
      if (glz::read_json(obj, buffer)) {
         std::cout << "glaze error!\n";
         break;
      }
   }

   t1 = std::chrono::steady_clock::now();

   r.json_read = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count() * 1e-6;

   // binary write performance

   t0 = std::chrono::steady_clock::now();

   for (size_t i = 0; i < iterations; ++i) {
      if (glz::write_binary(obj, buffer)) {
         std::cout << "glaze error!\n";
         break;
      }
   }

   t1 = std::chrono::steady_clock::now();

   r.binary_byte_length = buffer.size();
   r.binary_write = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count() * 1e-6;

   // binary read performance

   t0 = std::chrono::steady_clock::now();

   for (size_t i = 0; i < iterations; ++i) {
      if (glz::read_binary(obj, buffer)) {
         std::cout << "glaze error!\n";
         break;
      }
   }

   t1 = std::chrono::steady_clock::now();

   r.binary_read = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count() * 1e-6;

   // binary round trip

   t0 = std::chrono::steady_clock::now();

   for (size_t i = 0; i < iterations; ++i) {
      if (glz::read_binary(obj, buffer)) {
         std::cout << "glaze error!\n";
         break;
      }
      if (glz::write_binary(obj, buffer)) {
         std::cout << "glaze error!\n";
         break;
      }
   }

   t1 = std::chrono::steady_clock::now();

   r.binary_roundtrip = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count() * 1e-6;

   r.print();

   return r;
}

suite object_performance = [] { "object_performance"_test = [] { glaze_test<glz::opts{}>(); }; };

int main() { return 0; }
