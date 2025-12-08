// Object performance tests - split from json_performance.cpp for faster compilation
#include "glaze/glaze.hpp"
#include "json_perf_common.hpp"
#include "ut/ut.hpp"

using namespace ut;
using namespace glz::perf;

[[maybe_unused]] constexpr std::string_view json_minified =
   R"({"fixed_object":{"int_array":[0,1,2,3,4,5,6],"float_array":[0.1,0.2,0.3,0.4,0.5,0.6],"double_array":[3288398.238,2.33e+24,28.9,0.928759872,0.22222848,0.1,0.2,0.3,0.4]},"fixed_name_object":{"name0":"James","name1":"Abraham","name2":"Susan","name3":"Frank","name4":"Alicia"},"another_object":{"string":"here is some text","another_string":"Hello World","escaped_text":"{\"some key\":\"some string value\"}","boolean":false,"nested_object":{"v3s":[[0.12345,0.23456,0.001345],[0.3894675,97.39827,297.92387],[18.18,87.289,2988.298]],"id":"298728949872"}},"string_array":["Cat","Dog","Elephant","Tiger"],"string":"Hello world","number":3.14,"boolean":true,"another_bool":false})";

struct fixed_object_t
{
   std::vector<int> int_array;
   std::vector<float> float_array;
   std::vector<double> double_array;
};

template <>
struct glz::meta<fixed_object_t>
{
   using T = fixed_object_t;
   static constexpr auto value = object(&T::int_array, &T::float_array, &T::double_array);
};

struct fixed_name_object_t
{
   std::string name0{};
   std::string name1{};
   std::string name2{};
   std::string name3{};
   std::string name4{};
};

template <>
struct glz::meta<fixed_name_object_t>
{
   using T = fixed_name_object_t;
   static constexpr auto value = object(&T::name0, &T::name1, &T::name2, &T::name3, &T::name4);
};

struct nested_object_t
{
   std::vector<std::array<double, 3>> v3s{};
   std::string id{};
};

template <>
struct glz::meta<nested_object_t>
{
   using T = nested_object_t;
   static constexpr auto value = object(&T::v3s, &T::id);
};

struct another_object_t
{
   std::string string{};
   std::string another_string{};
   std::string escaped_text{};
   bool boolean{};
   nested_object_t nested_object{};
};

template <>
struct glz::meta<another_object_t>
{
   using T = another_object_t;
   static constexpr auto value =
      object(&T::string, &T::another_string, &T::escaped_text, &T::boolean, &T::nested_object);
};

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

template <>
struct glz::meta<obj_t>
{
   using T = obj_t;
   static constexpr auto value = object(&T::fixed_object, &T::fixed_name_object, &T::another_object, &T::string_array,
                                        &T::string, &T::number, &T::boolean, &T::another_bool);
};

#ifdef NDEBUG
[[maybe_unused]] constexpr size_t iterations = 1'000'000;
#else
[[maybe_unused]] constexpr size_t iterations = 100'000;
#endif

template <auto Opts>
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

   // beve write performance

   t0 = std::chrono::steady_clock::now();

   for (size_t i = 0; i < iterations; ++i) {
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

   for (size_t i = 0; i < iterations; ++i) {
      if (glz::read_beve(obj, buffer)) {
         std::cout << "glaze error!\n";
         break;
      }
   }

   t1 = std::chrono::steady_clock::now();

   r.beve_read = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count() * 1e-6;

   // beve round trip

   t0 = std::chrono::steady_clock::now();

   for (size_t i = 0; i < iterations; ++i) {
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

suite object_performance = [] { "object_performance"_test = [] { glaze_test<glz::opts{}>(); }; };

int main() { return 0; }
