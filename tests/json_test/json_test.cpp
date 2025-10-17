// Glaze Library
// For the license information refer to glaze.hpp

#include <algorithm>
#include <any>
#include <atomic>
#include <bitset>
#include <chrono>
#include <complex>
#include <deque>
#include <forward_list>
#include <initializer_list>
#include <iostream>
#include <list>
#include <map>
#include <numbers>
#include <random>
#include <ranges>
#include <set>
#if defined(__STDCPP_FLOAT128_T__)
#include <stdfloat>
#endif
#include <tuple>
#include <unordered_map>
#include <variant>

#include "glaze/api/impl.hpp"
#include "glaze/containers/flat_map.hpp"
#include "glaze/file/hostname_include.hpp"
#include "glaze/file/raw_or_file.hpp"
#include "glaze/hardware/volatile_array.hpp"
#include "glaze/json.hpp"
#include "glaze/json/study.hpp"
#include "glaze/record/recorder.hpp"
#include "glaze/trace/trace.hpp"
#include "ut/ut.hpp"

using namespace ut;

glz::trace trace{};

struct jsonc_comment_config
{
   std::vector<int> array_1{};
   std::vector<int> array_2{};
};

suite start_trace = [] { trace.begin("json_test", "Full test suite duration."); };

// Regression test: empty JSON string to char should yield '\0'
struct CharRoundtrip
{
   char char_val{};
   unsigned char uchar_val{};
   int int_val{};
};

namespace glz
{
   template <>
   struct meta<CharRoundtrip>
   {
      using T = CharRoundtrip;
      static constexpr auto value = object(&T::char_val, &T::uchar_val, &T::int_val);
   };
} // namespace glz

suite char_empty_string = [] {
   "char empty string deserializes to null char"_test = [] {
      CharRoundtrip original{};
      std::string buffer{};
      expect(not glz::write_json(original, buffer));

      CharRoundtrip deserialized{'a', 'b', 1};
      auto ec = glz::read_json(deserialized, buffer);
      expect(ec == glz::error_code::none) << glz::format_error(ec, buffer);
      expect(deserialized.char_val == 0);
      expect(deserialized.uchar_val == 0);
      expect(deserialized.int_val == 0);
   };
};

// Test structs for pointer tests (using pure reflection, no glz::meta)
struct ptr_struct
{
   int* val{};
};

struct multi_ptr_struct
{
   int* ptr1{};
   double* ptr2{};
   std::string* ptr3{};
};

struct ptr_opt_struct
{
   int* ptr{};
   std::optional<int> opt{};
   int value{42};
};

struct as_array_details
{
   std::string_view name;
   std::string_view surname;
   std::string_view city;
   std::string_view street;
};

struct as_array_person
{
   int id{};
   as_array_details person{};
};

template <>
struct glz::meta<as_array_person>
{
   using T = as_array_person;
   static constexpr auto value = object("id", &T::id, "person", glz::as_array<&T::person>);
};

suite as_array_wrapper_tests = [] {
   "array to struct via as_array"_test = [] {
      std::string buffer = R"({
         "id": 1,
         "person": ["Joe", "Doe", "London", "Chamber St"]
      })";

      as_array_person value{};
      expect(!glz::read_json(value, buffer));
      expect(value.id == 1);
      expect(value.person.city == "London");
      expect(value.person.street == "Chamber St");

      auto written = glz::write_json(value).value();
      expect(written == R"({"id":1,"person":["Joe","Doe","London","Chamber St"]})");
   };
};

struct my_struct
{
   int i = 287;
   double d = 3.14;
   std::string hello = "Hello World";
   std::array<uint64_t, 3> arr = {1, 2, 3};
};

template <>
struct glz::meta<my_struct>
{
   static constexpr std::string_view name = "my_struct";
   using T = my_struct;
   static constexpr auto value = object(
      "i", [](auto&& v) -> auto& { return v.i; }, //
      "d", &T::d, //
      "hello", &T::hello, //
      "arr", &T::arr //
   );
};

static_assert(glz::write_supported<my_struct, glz::JSON>);
static_assert(glz::read_supported<my_struct, glz::JSON>);

struct Issue1866
{
   std::string uniqueName;
   std::string name{};
   std::string description{};
   bool codexSecret{};
   std::optional<bool> excludeFromCodex{};
   std::string parentName{};
};

struct MemberFunctionThing
{
   std::string name{};
   auto get_description() const -> std::string { return "something"; }
};

namespace glz
{
   template <>
   struct meta<MemberFunctionThing>
   {
      using T = MemberFunctionThing;
      static constexpr auto value = object("name", &T::name, "description", &T::get_description);
   };
} // namespace glz

suite starter = [] {
   "example"_test = [] {
      my_struct s{};
      std::string buffer{};
      expect(not glz::write_json(s, buffer));
      expect(buffer == R"({"i":287,"d":3.14,"hello":"Hello World","arr":[1,2,3]})");
      auto pretty = glz::prettify_json(buffer);
      expect(pretty == R"({
   "i": 287,
   "d": 3.14,
   "hello": "Hello World",
   "arr": [
      1,
      2,
      3
   ]
})") << pretty;

      pretty = glz::prettify_json<glz::opts{.new_lines_in_arrays = false}>(buffer);
      expect(pretty == R"({
   "i": 287,
   "d": 3.14,
   "hello": "Hello World",
   "arr": [1, 2, 3]
})") << pretty;
   };
};

struct sub_thing
{
   double a{3.14};
   std::string b{"stuff"};
};

template <>
struct glz::meta<sub_thing>
{
   static constexpr std::string_view name = "sub_thing";
   static constexpr auto value = object("a", &sub_thing::a, //
                                        "b", [](auto&& v) -> auto& { return v.b; } //
   );
};

struct sub_thing2
{
   double a{3.14};
   std::string b{"stuff"};
   double c{999.342494903};
   double d{0.000000000001};
   double e{203082348402.1};
   float f{89.089f};
   double g{12380.00000013};
   double h{1000000.000001};
};

template <>
struct glz::meta<sub_thing2>
{
   using T = sub_thing2;
   static constexpr std::string_view name = "sub_thing2";
   static constexpr auto value = object("a", &T::a, //
                                        "b", &T::b, //
                                        "c", &T::c, //
                                        "d", &T::d, //
                                        "e", &T::e, //
                                        "f", &T::f, //
                                        "g", &T::g, //
                                        "h", &T::h //
   );
};

struct V3
{
   double x{3.14};
   double y{2.7};
   double z{6.5};

   bool operator==(const V3& rhs) const { return (x == rhs.x) && (y == rhs.y) && (z == rhs.z); }
};

template <>
struct glz::meta<V3>
{
   static constexpr std::string_view name = "V3";
   static constexpr auto value = array(&V3::x, &V3::y, &V3::z);
};

enum class Color { Red, Green, Blue };

template <>
struct glz::meta<Color>
{
   static constexpr std::string_view name = "Color";
   static constexpr auto value = enumerate("Red", Color::Red, //
                                           "Green", Color::Green, //
                                           "Blue", Color::Blue //
   );
};

static_assert(glz::enum_name_v<Color::Red> == "Red");

static_assert(glz::get_enum_name(Color::Green) == "Green");

struct enum_array_t
{
   Color array[3]{};
};

template <>
struct glz::meta<enum_array_t>
{
   using T = enum_array_t;
   static constexpr auto value = object("array", [](auto& s) { return std::span{s.array, 3}; });
};

suite enum_tests = [] {
   "get_enum_name"_test = [] {
      auto color = Color::Green;

      const auto name = glz::get_enum_name(color);
      expect(name == "Green");
   };

   "array of enums"_test = [] {
      std::array<Color, 3> arr{};
      std::string_view buffer = R"(["Green", "Red", "Blue"])";
      expect(not glz::read_json(arr, buffer));
      using enum Color;
      expect(arr[0] == Green);
      expect(arr[1] == Red);
      expect(arr[2] == Blue);
   };

   "enum_array_t"_test = [] {
      enum_array_t obj{};
      std::string_view buffer = R"({"array": ["Green", "Red", "Blue"]})";
      expect(not glz::read_json(obj, buffer));
      using enum Color;
      expect(obj.array[0] == Green);
      expect(obj.array[1] == Red);
      expect(obj.array[2] == Blue);
   };
};

static constexpr auto MY_ARRAY_MAX = 2;

struct MyArrayStruct
{
   uint8_t my_array[MY_ARRAY_MAX]{};
};

enum UnscopedEnum : uint8_t {
   ENUM_VALUE_0 = 0,
   ENUM_VALUE_1,
};

template <>
struct glz::meta<UnscopedEnum>
{
   static constexpr auto value = enumerate(ENUM_VALUE_0, ENUM_VALUE_1);
};

template <>
struct glz::meta<MyArrayStruct>
{
   using T = MyArrayStruct;
   // Mapping enum parsing to a uint8_t array
   static constexpr auto value =
      object("my_array", [](auto& s) { return std::span{reinterpret_cast<UnscopedEnum*>(s.my_array), MY_ARRAY_MAX}; });
};

suite unscoped_enum_tests = [] {
   "enum_array"_test = [] {
      MyArrayStruct s;
      std::string buffer = R"({"my_array": ["ENUM_VALUE_0", "ENUM_VALUE_1"]})";

      auto ec = glz::read_json(s, buffer);
      expect(not ec) << glz::format_error(ec, buffer);
      expect(s.my_array[0] == 0);
      expect(s.my_array[1] == 1);
   };
};

enum struct Vehicle : uint32_t { Car, Truck, Plane };

enum struct Shapes : uint32_t { circ, sq, triangle };

template <>
struct glz::meta<Vehicle>
{
   using enum Vehicle;
   static constexpr std::array keys{"Car", "Truck", "Plane"};
   static constexpr std::array value{Car, Truck, Plane};
};

template <>
struct glz::meta<Shapes>
{
   using enum Shapes;
   static constexpr std::array keys{"Circle", "Square", "Triangle"};
   static constexpr std::array value{circ, sq, triangle};
};

// static_assert(glz::reflect<Vehicle>::keys[uint32_t(Vehicle::Truck)] == "Truck");

suite glz_enum_test = [] {
   "glz_enum"_test = [] {
      auto name = glz::write_json(Vehicle::Plane).value();
      expect(name == R"("Plane")") << name;

      Vehicle vehicle{};
      auto ec = glz::read_json(vehicle, name);
      expect(not ec) << glz::format_error(ec, name);
      expect(vehicle == Vehicle::Plane);
   };

   "glz_enum_map"_test = [] {
      auto name = glz::write_json(Shapes::sq).value();
      expect(name == R"("Square")") << name;

      Shapes shape{};
      expect(not glz::read_json(shape, name));
      expect(shape == Shapes::sq);
   };
};

enum class TestData : uint8_t { None, A, B, C, D, ERROR_E = 0xFF };

struct DummyData
{
   uint32_t id{0};

   int32_t a{0};

   TestData b{TestData::None};
   TestData c{TestData::None};
   TestData d{TestData::None};
   TestData e{TestData::None};

   int64_t f{0};
};

template <>
struct glz::meta<TestData>
{
   using enum TestData;
   static constexpr auto value = enumerate(None, A, B, C, D, ERROR_E);
};

template <>
struct glz::meta<DummyData>
{
   using T = DummyData;
   static constexpr std::string_view name = "DummyData";
   static constexpr auto value = object(&T::id, &T::a, &T::b, &T::c, &T::d, &T::e, &T::f);
};

suite test_data_struct_tests = [] {
   "test_data_struct"_test = [] {
      const std::vector<DummyData> test_data = {
         DummyData{
            .id = 0,
            .a = 0,
            .b = TestData::None,
            .c = TestData::None,
            .d = TestData::None,
            .e = TestData::None,
            .f = 0x00000000,
         },
         DummyData{
            .id = 1,
            .a = 1,
            .b = TestData::A,
            .c = TestData::B,
            .d = TestData::A,
            .e = TestData::B,
            .f = 0xDDDDDDDD,
         },
         DummyData{
            .id = 2,
            .a = 6,
            .b = TestData::A,
            .c = TestData::B,
            .d = TestData::C,
            .e = TestData::D,
            .f = 0xEEEEEEEE,
         },
         DummyData{
            .id = 3,
            .a = -1,
            .b = TestData::ERROR_E,
            .c = TestData::ERROR_E,
            .d = TestData::ERROR_E,
            .e = TestData::ERROR_E,
            .f = 0xFFFFFFFF,
         },
      };
      expect(glz::reflect<TestData>::keys.size() == 6);
      std::string s = glz::write_json(test_data).value_or("error");
      expect(s != "error") << s;
      std::vector<DummyData> in_test_data;
      expect(not glz::read_json(in_test_data, s));
   };
};

struct var1_t
{
   double x{};
};

template <>
struct glz::meta<var1_t>
{
   using T = var1_t;
   static constexpr std::string_view name = "var1_t";
   static constexpr auto value = object("x", &T::x);
};

struct var2_t
{
   double y{};
};

template <>
struct glz::meta<var2_t>
{
   using T = var2_t;
   static constexpr std::string_view name = "var2_t";
   static constexpr auto value = object("y", &T::y);
};

struct Thing
{
   sub_thing thing{};
   std::array<sub_thing2, 1> thing2array{};
   V3 vec3{};
   std::list<int> list{6, 7, 8, 2};
   std::array<std::string, 4> array = {"as\"df\\ghjkl", "pie", "42", "foo"};
   std::vector<V3> vector = {{9.0, 6.7, 3.1}, {}};
   int i{8};
   double d{2};
   bool b{};
   char c{'W'};
   std::variant<var1_t, var2_t> v{};
   Color color{Color::Green};
   std::vector<bool> vb = {true, false, false, true, true, true, true};
   std::shared_ptr<sub_thing> sptr = std::make_shared<sub_thing>();
   std::optional<V3> optional{};
   std::deque<double> deque = {9.0, 6.7, 3.1};
   std::map<std::string, int> map = {{"a", 4}, {"f", 7}, {"b", 12}};
   std::map<int, double> mapi{{5, 3.14}, {7, 7.42}, {2, 9.63}};
   sub_thing* thing_ptr{};

   Thing() : thing_ptr(&thing) {};
};

template <>
struct glz::meta<Thing>
{
   using T = Thing;
   static constexpr std::string_view name = "Thing";
   static constexpr std::array<std::string_view, 2> required = {"thing", "i"};
   static constexpr std::array<std::string_view, 1> examples = {R"({"thing":{},"i":42})"};
   static constexpr auto value = object(
      "thing", &T::thing, //
      "thing2array", &T::thing2array, //
      "vec3", &T::vec3, //
      "list", &T::list, //
      "deque", &T::deque, //
      "vector", [](auto&& v) -> auto& { return v.vector; }, //
      "i", [](auto&& v) -> auto& { return v.i; }, //
      "d", &T::d, //
      "b", &T::b, //
      "c", &T::c, //
      "v", &T::v, //
      "color", &T::color, //
      "vb", &T::vb, //
      "sptr", &T::sptr, //
      "optional", &T::optional, //
      "array", &T::array, //
      "map", &T::map, //
      "mapi", &T::mapi, //
      "thing_ptr", &T::thing_ptr //
   );
};

struct Escaped
{
   int escaped_key{};
   std::string escaped_key2{"hi"};
   std::string escape_chars{""};
};

template <>
struct glz::meta<Escaped>
{
   static constexpr std::string_view name = "Escaped";
   using T = Escaped;
   static constexpr auto value = object(R"(escaped\"key)", &T::escaped_key, //
                                        R"(escaped\"\"key2)", &T::escaped_key2, R"(escape_chars)", &T::escape_chars);
};

suite escaping_tests = [] {
   "escaped_key"_test = [] {
      std::string out;
      Escaped obj{};
      expect(not glz::write_json(obj, out));

      expect(out == R"({"escaped\"key":0,"escaped\"\"key2":"hi","escape_chars":""})");

      std::string in = R"({"escaped\"key":5,"escaped\"\"key2":"bye"})";
      expect(!glz::read_json(obj, in));
      expect(obj.escaped_key == 5);
      expect(obj.escaped_key2 == "bye");
   };

   "ᇿ read"_test = [] {
      std::string in = R"("\u11FF")";
      std::string str{};
      expect(!glz::read_json(str, in));
      expect(str == "ᇿ") << str;
   };

   "escaped_characters read"_test = [] {
      std::string in = R"({"escape_chars":"\b\f\n\r\t\u11FF"})";
      Escaped obj{};

      expect(glz::read_json(obj, in) == glz::error_code::none);

      expect(obj.escape_chars == "\b\f\n\r\tᇿ") << obj.escape_chars;
   };

   "escaped_char read"_test = [] {
      std::string in = R"("\b")";
      char c{};
      expect(glz::read_json(c, in) == glz::error_code::none);
      expect(c == '\b');

      in = R"("\f")";
      expect(glz::read_json(c, in) == glz::error_code::none);
      expect(c == '\f');

      in = R"("\n")";
      expect(glz::read_json(c, in) == glz::error_code::none);
      expect(c == '\n');

      in = R"("\r")";
      expect(glz::read_json(c, in) == glz::error_code::none);
      expect(c == '\r');

      in = R"("\t")";
      expect(glz::read_json(c, in) == glz::error_code::none);
      expect(c == '\t');
   };

   "escaped_characters write"_test = [] {
      std::string str = "\"\\\b\f\n\r\tᇿ";
      std::string buffer{};
      expect(not glz::write_json(str, buffer));
      expect(buffer == R"("\"\\\b\f\n\r\tᇿ")") << buffer;
   };

   "escaped_char write"_test = [] {
      std::string out{};
      char c = '\b';
      expect(not glz::write_json(c, out));
      expect(out == R"("\b")");

      c = '\f';
      expect(not glz::write_json(c, out));
      expect(out == R"("\f")");

      c = '\n';
      expect(not glz::write_json(c, out));
      expect(out == R"("\n")");

      c = '\r';
      expect(not glz::write_json(c, out));
      expect(out == R"("\r")");

      c = '\t';
      expect(not glz::write_json(c, out));
      expect(out == R"("\t")");
   };
};

template <std::floating_point T>
T generate_uniform()
{
   static std::mt19937 gen(std::random_device{}());
   static std::uniform_real_distribution<double> dis(0, 1);

   int sign = (dis(gen) < 0.5) ? -1 : 1;

   int min_exp = std::numeric_limits<T>::min_exponent;
   int max_exp = std::numeric_limits<T>::max_exponent;
   std::uniform_int_distribution<> exp_dis(min_exp, max_exp);
   int exp = exp_dis(gen);

   // Generate random significand
   T sig = static_cast<T>(dis(gen));

   // Combine to create the random float
   return sign * std::ldexp(sig, exp);
}

template <std::floating_point T>
bool equal_within_ulps(T x, T y, size_t n)
{
   if (std::abs(x) < (std::numeric_limits<T>::min)()) {
      // a denormalized value - check that it is close enough to zero.
      return std::abs(y) < (std::numeric_limits<T>::min)();
   }

   // Since `epsilon()` is the gap size (ULP, unit in the last place)
   // of floating-point numbers in interval [1, 2), we can scale it to
   // the gap size in interval [2^e, 2^{e+1}), where `e` is the exponent
   // of `x` and `y`.

   // If `x` and `y` have different gap sizes (which means they have
   // different exponents), we take the smaller one. Taking the bigger
   // one is also reasonable, I guess.
   const T m = (std::min)(std::abs(x), std::abs(y));

   // Subnormal numbers have fixed exponent, which is `min_exponent - 1`.
   const int exp = m < (std::numeric_limits<T>::min)() ? std::numeric_limits<T>::min_exponent - 1 : std::ilogb(m);

   // We consider `x` and `y` equal if the difference between them is
   // within `n` ULPs.
   return std::abs(x - y) <= n * std::ldexp(std::numeric_limits<T>::epsilon(), exp);
}

template <std::floating_point T>
bool equal(T x, T y)
{
   if (std::abs(x) < (std::numeric_limits<T>::min)()) {
      // a denormalized value - check that it is close enough to zero.
      return std::abs(y) < (std::numeric_limits<T>::min)();
   }

   return x == y;
}

suite basic_types = [] {
   using namespace ut;

   "double write"_test = [] {
      std::string buffer{};
      expect(not glz::write_json(3.14, buffer));
      expect(buffer == "3.14") << buffer;
      expect(not glz::write_json(9.81, buffer));
      expect(buffer == "9.81") << buffer;
      expect(not glz::write_json(0.0, buffer));
      expect(buffer == "0") << buffer;
      expect(not glz::write_json(-0.0, buffer));
      expect(buffer == "-0") << buffer;
      expect(not glz::write_json(-8536070.0, buffer));
      expect(buffer == "-8536070") << buffer;
      expect(not glz::write_json(8536070.0, buffer));
      expect(buffer == "8536070") << buffer;
      expect(not glz::write_json(std::numeric_limits<double>::infinity(), buffer));
      expect(buffer == "null") << buffer;
      expect(not glz::write_json(1.0, buffer));
      expect(buffer == "1") << buffer;
      expect(not glz::write_json(10.0, buffer));
      expect(buffer == "10") << buffer;
      expect(not glz::write_json(100.0, buffer));
      expect(buffer == "100") << buffer;
      expect(not glz::write_json(0.1, buffer));
      expect(buffer == "0.1") << buffer;
      expect(not glz::write_json(0.01, buffer));
      expect(buffer == "0.01") << buffer;
      expect(not glz::write_json(0.001, buffer));
      expect(buffer == "0.001") << buffer;
   };

   "double roundtrip"_test = [] {
      for (const double expected : {-0x1.e42427b42cb42p+949, -0x1.3ffff0d0ddb37p+725, 0x1.73d40c08b20ffp-395}) {
         double d{expected};
         auto str = glz::write_json(d).value();
         auto restored = glz::read_json<double>(str);
         expect(restored.has_value());
         expect(restored.value() == d);
      }
   };

   "float write"_test = [] {
      std::string buffer{};
      expect(not glz::write_json(3.14f, buffer));
      expect(buffer == "3.14") << buffer;
      expect(not glz::write_json(9.81f, buffer));
      expect(buffer == "9.81") << buffer;
      expect(not glz::write_json(0.0f, buffer));
      expect(buffer == "0") << buffer;
      expect(not glz::write_json(-0.0f, buffer));
      expect(buffer == "-0") << buffer;
      expect(not glz::write_json(-8536070.f, buffer));
      expect(buffer == "-8536070") << buffer;
      expect(not glz::write_json(8536070.f, buffer));
      expect(buffer == "8536070") << buffer;
      expect(not glz::write_json(std::numeric_limits<float>::infinity(), buffer));
      expect(buffer == "null") << buffer;
      expect(not glz::write_json(1.0f, buffer));
      expect(buffer == "1") << buffer;
      expect(not glz::write_json(10.0f, buffer));
      expect(buffer == "10") << buffer;
      expect(not glz::write_json(100.0f, buffer));
      expect(buffer == "100") << buffer;
      expect(not glz::write_json(0.1f, buffer));
      expect(buffer == "0.1") << buffer;
      expect(not glz::write_json(0.01f, buffer));
      expect(buffer == "0.01") << buffer;
      expect(not glz::write_json(0.001f, buffer));
      expect(buffer == "0.001") << buffer;
   };

   "double read valid"_test = [] {
      double num{};
      expect(glz::read_json(num, "3.14") == glz::error_code::none);
      expect(num == 3.14);
      expect(glz::read_json(num, "9.81") == glz::error_code::none);
      expect(num == 9.81);
      expect(glz::read_json(num, "0") == glz::error_code::none);
      expect(num == 0);
      expect(glz::read_json(num, "-0") == glz::error_code::none);
      expect(num == -0);
   };

   "double write/read"_test = [] {
      double x{};
      auto buffer = glz::write_json(-1.40129846e-45).value();
      auto ec = glz::read_json(x, buffer);
      expect(not ec) << glz::format_error(ec, buffer);
   };

   "random double"_test = [] {
      for (size_t i = 0; i < 10000; ++i) {
         double x = generate_uniform<double>();
         auto buffer = glz::write_json(x).value();
         double y{};
         auto ec = glz::read_json(y, buffer);
         expect(not ec) << glz::format_error(ec, buffer);
         expect(equal(x, y)) << x << ", " << y;
      }
   };

   "float write/read"_test = [] {
      float x{};
      auto buffer = glz::write_json(-1.40129846e-45f).value();
      auto ec = glz::read_json(x, buffer);
      expect(not ec) << glz::format_error(ec, buffer);
   };

   "random float"_test = [] {
      for (size_t i = 0; i < 10000; ++i) {
         float x = generate_uniform<float>();
         auto buffer = glz::write_json(x).value();
         float y{};
         auto ec = glz::read_json(y, buffer);
         expect(not ec) << glz::format_error(ec, buffer);
         expect(equal(x, y)) << x << ", " << y;
      }
   };

   "int write"_test = [] {
      std::string buffer{};
      expect(not glz::write_json(0, buffer));
      expect(buffer == "0");
      buffer.clear();
      expect(not glz::write_json(999, buffer));
      expect(buffer == "999");
      buffer.clear();
      expect(not glz::write_json(-6, buffer));
      expect(buffer == "-6");
      buffer.clear();
      expect(not glz::write_json(10000, buffer));
      expect(buffer == "10000");
   };

   "int read valid"_test = [] {
      int num{};
      expect(glz::read_json(num, "-1") == glz::error_code::none);
      expect(num == -1);
      expect(glz::read_json(num, "0") == glz::error_code::none);
      expect(num == 0);
      expect(glz::read_json(num, "999") == glz::error_code::none);
      expect(num == 999);
      expect(glz::read_json(num, "1e4") == glz::error_code::none);
      expect(num == 10000);
      uint64_t num64{};
      expect(glz::read_json(num64, "32948729483739289") == glz::error_code::none);
      expect(num64 == 32948729483739289);
   };

   "int read invalid"_test = [] {
      int num{33};
      expect(glz::read_json(num, ";adsfa") == glz::error_code::parse_number_failure);
      expect(glz::read_json(num, "{}") == glz::error_code::parse_number_failure);
      expect(glz::read_json(num, "[]") == glz::error_code::parse_number_failure);
      expect(glz::read_json(num, ".") == glz::error_code::parse_number_failure);
      expect(glz::read_json(num, "0045") == glz::error_code::parse_number_failure);
   };

   "bool write"_test = [] {
      std::string buffer{};
      expect(not glz::write_json(true, buffer));
      expect(buffer == "true");
      buffer.clear();
      expect(not glz::write_json(false, buffer));
      expect(buffer == "false");
   };

   "bool read valid"_test = [] {
      bool val{};
      expect(glz::read_json(val, "true") == glz::error_code::none);
      expect(val == true);
      expect(glz::read_json(val, "false") == glz::error_code::none);
      expect(val == false);
   };

   "bool read invalid"_test = [] {
      bool val{};
      expect(glz::read_json(val, "tru") != glz::error_code::none);
      expect(glz::read_json(val, "alse") != glz::error_code::none);
   };

   "string write"_test = [] {
      std::string buffer{};
      expect(not glz::write_json("fish", buffer));
      expect(buffer == "\"fish\"");
      buffer.clear();
      expect(not glz::write_json("as\"df\\ghjkl", buffer));
      expect(buffer == "\"as\\\"df\\\\ghjkl\"");

      "empty"_test = [] {
         static constexpr std::string_view expected_empty{"\"\""};
         static constexpr std::string_view expected_nothing{};
         expect(glz::write_json(std::string_view{}) == expected_empty);
         expect(glz::write_json(std::string{}) == expected_empty);
         expect(glz::write_json("") == expected_empty);

         auto write_raw = [](const auto& input) {
            std::string result{};
            expect(not glz::write<glz::opts{.raw = true}>(input, result));
            return result;
         };
         expect(write_raw(std::string_view{}) == expected_nothing);
         expect(write_raw(std::string{}) == expected_nothing);
         expect(write_raw("") == expected_nothing);

         auto write_raw_str = [](const auto& input) {
            std::string result{};
            expect(not glz::write<glz::opts{.raw_string = true}>(input, result));
            return result;
         };
         expect(write_raw_str(std::string_view{}) == expected_empty);
         expect(write_raw_str(std::string{}) == expected_empty);
         expect(write_raw_str("") == expected_empty);

         auto write_num = [](const auto& input) {
            std::string result{};
            expect(not glz::write<glz::opts{.number = true}>(input, result));
            return result;
         };
         expect(write_num(std::string_view{}) == expected_nothing);
         expect(write_num(std::string{}) == expected_nothing);
         expect(write_num("") == expected_nothing);
      };
   };

   "backslash testing"_test = [] {
      std::string val{};
      expect(glz::read_json(val, "\"fish\"") == glz::error_code::none);
      expect(val == "fish");
      expect(glz::read_json(val, "\"as\\\"df\\\\ghjkl\"") == glz::error_code::none);
      expect(val == "as\"df\\ghjkl");
   };

   "string_view read"_test = [] {
      std::string_view val{};
      expect(glz::read_json(val, "\"fish\"") == glz::error_code::none);
      expect(val == "fish");
      expect(glz::read_json(val, "\"as\\\"df\\\\ghjkl\"") == glz::error_code::none);
      expect(val == "as\\\"df\\\\ghjkl");
   };
};

struct opts_concatenate : glz::opts
{
   bool concatenate = true;
};

suite container_types = [] {
   using namespace ut;
   "vector int roundtrip"_test = [] {
      std::vector<int> vec(100);
      for (auto& item : vec) item = rand();
      std::string buffer{};
      std::vector<int> vec2{};
      expect(not glz::write_json(vec, buffer));
      expect(glz::read_json(vec2, buffer) == glz::error_code::none);
      expect(vec == vec2);
   };
   "vector uint64_t roundtrip"_test = [] {
      std::uniform_int_distribution<uint64_t> dist((std::numeric_limits<uint64_t>::min)(),
                                                   (std::numeric_limits<uint64_t>::max)());
      std::mt19937 gen{};
      std::vector<uint64_t> vec(100);
      for (auto& item : vec) item = dist(gen);
      std::string buffer{};
      std::vector<uint64_t> vec2{};
      expect(not glz::write_json(vec, buffer));
      expect(glz::read_json(vec2, buffer) == glz::error_code::none);
      expect(vec == vec2);
   };
   "vector double roundtrip"_test = [] {
      std::vector<double> vec(100);
      for (auto& item : vec) item = rand() / (1.0 + rand());
      std::string buffer{};
      std::vector<double> vec2{};
      expect(not glz::write_json(vec, buffer));
      expect(glz::read_json(vec2, buffer) == glz::error_code::none);
      expect(vec == vec2);
   };
   "vector float roundtrip"_test = [] {
      std::vector<float> vec(100);
      for (auto& item : vec) item = float(rand() / (1.0 + rand()));
      std::string buffer{};
      std::vector<float> vec2{};
      expect(not glz::write_json(vec, buffer));
      expect(not glz::read_json(vec2, buffer));
      expect(vec == vec2);
   };
   "vector bool roundtrip"_test = [] {
      std::vector<bool> vec(100);
      for (auto&& item : vec) item = rand() / (1.0 + rand());
      std::string buffer{};
      std::vector<bool> vec2{};
      expect(not glz::write_json(vec, buffer));
      expect(glz::read_json(vec2, buffer) == glz::error_code::none);
      expect(vec == vec2);
   };
   "vector pair"_test = [] {
      std::vector<std::pair<int, int>> v;
      expect(!glz::read<opts_concatenate{{}, false}>(v, R"([{"1":2},{"3":4}])"));
      const auto s = glz::write<opts_concatenate{{}, false}>(v).value_or("error");
      expect(s == R"([{"1":2},{"3":4}])") << s;
   };
   "vector pair"_test = [] {
      std::vector<std::pair<int, int>> v;
      expect(!glz::read<opts_concatenate{{}, false}>(v, R"([{"1":2},{"3":4}])"));
      // constexpr glz::opts opts{.prettify = true};
      const auto s = glz::write<opts_concatenate{{glz::opts{.prettify = true}}, false}>(v).value_or("error");
      expect(s == R"([
   {
      "1": 2
   },
   {
      "3": 4
   }
])") << s;
   };
   "vector pair roundtrip"_test = [] {
      std::vector<std::pair<int, int>> v;
      expect(!glz::read_json(v, R"({"1":2,"3":4})"));
      const auto s = glz::write_json(v).value_or("error");
      expect(s == R"({"1":2,"3":4})") << s;
   };
   "vector pair roundtrip"_test = [] {
      std::vector<std::pair<int, int>> v;
      expect(!glz::read_json(v, R"({"1":2,"3":4})"));
      const auto s = glz::write<glz::opts{.prettify = true}>(v).value_or("error");
      expect(s == R"({
   "1": 2,
   "3": 4
})") << s;
   };
   "deque roundtrip"_test = [] {
      std::vector<int> deq(100);
      for (auto& item : deq) item = rand();
      std::string buffer{};
      std::vector<int> deq2{};
      expect(not glz::write_json(deq, buffer));
      expect(glz::read_json(deq2, buffer) == glz::error_code::none);
      expect(deq == deq2);
   };
   "list roundtrip"_test = [] {
      std::list<int> lis(100);
      for (auto& item : lis) item = rand();
      std::string buffer{};
      std::list<int> lis2{};
      expect(not glz::write_json(lis, buffer));
      expect(glz::read_json(lis2, buffer) == glz::error_code::none);
      expect(lis == lis2);
   };
   "forward_list roundtrip"_test = [] {
      std::forward_list<int> lis(100);
      for (auto& item : lis) item = rand();
      std::string buffer{};
      std::forward_list<int> lis2{};
      expect(not glz::write_json(lis, buffer));
      expect(glz::read_json(lis2, buffer) == glz::error_code::none);
      expect(lis == lis2);
   };
   "map string keys roundtrip"_test = [] {
      std::map<std::string, int> map;
      std::string str{"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"};
      std::mt19937 g{};
      for (auto i = 0; i < 20; ++i) {
         std::shuffle(str.begin(), str.end(), g);
         map[str] = rand();
      }
      std::string buffer{};
      std::map<std::string, int> map2{};
      static_assert(glz::writable_map_t<decltype(map2)>);
      expect(not glz::write_json(map, buffer));
      expect(glz::read_json(map2, buffer) == glz::error_code::none);
      // expect(map == map2);
      for (auto& it : map) {
         expect(map2[it.first] == it.second);
      }
   };
   "map int keys roundtrip"_test = [] {
      std::map<int, int> map;
      for (auto i = 0; i < 20; ++i) {
         map[rand()] = rand();
      }
      std::string buffer{};
      std::map<int, int> map2{};
      expect(not glz::write_json(map, buffer));
      expect(glz::read_json(map2, buffer) == glz::error_code::none);
      // expect(map == map2);
      for (auto& it : map) {
         expect(map2[it.first] == it.second);
      }
   };
   "unordered_map int keys roundtrip"_test = [] {
      std::unordered_map<int, int> map;
      for (auto i = 0; i < 20; ++i) {
         map[rand()] = rand();
      }
      std::string buffer{};
      std::unordered_map<int, int> map2{};
      expect(not glz::write_json(map, buffer));
      expect(glz::read_json(map2, buffer) == glz::error_code::none);
      // expect(map == map2);
      for (auto& it : map) {
         expect(map2[it.first] == it.second);
      }
   };
   "unordered_map<int, std::string> roundtrip"_test = [] {
      std::unordered_map<int, std::string> map;
      for (auto i = 0; i < 5; ++i) {
         map[rand()] = std::to_string(rand());
      }
      std::string buffer{};
      std::unordered_map<int, std::string> map2{};
      expect(not glz::write_json(map, buffer));
      expect(!glz::read_json(map2, buffer));
      for (auto& it : map) {
         expect(map2[it.first] == it.second);
      }
   };
   "tuple roundtrip"_test = [] {
      auto tuple = std::make_tuple(3, 2.7, std::string("curry"));
      decltype(tuple) tuple2{};
      std::string buffer{};
      expect(not glz::write_json(tuple, buffer));
      expect(glz::read_json(tuple2, buffer) == glz::error_code::none);
      expect(tuple == tuple2);
   };
   "pair roundtrip"_test = [] {
      auto pair = std::make_pair(std::string("water"), 5.2);
      decltype(pair) pair2{};
      std::string buffer{};
      expect(not glz::write_json(pair, buffer));
      expect(glz::read_json(pair2, buffer) == glz::error_code::none);
      expect(pair == pair2);
   };
};

suite nullable_types = [] {
   using namespace ut;
   "optional"_test = [] {
      std::optional<int> oint{};
      std::string buffer{};
      expect(not glz::write_json(oint, buffer));
      expect(buffer == "null");

      expect(glz::read_json(oint, "5") == glz::error_code::none);
      expect(bool(oint) && *oint == 5);
      buffer.clear();
      expect(not glz::write_json(oint, buffer));
      expect(buffer == "5");

      expect(glz::read_json(oint, "null") == glz::error_code::none);
      expect(!bool(oint));
      buffer.clear();
      expect(not glz::write_json(oint, buffer));
      expect(buffer == "null");
   };
   "shared_ptr"_test = [] {
      std::shared_ptr<int> ptr{};
      std::string buffer{};
      expect(not glz::write_json(ptr, buffer));
      expect(buffer == "null");

      expect(glz::read_json(ptr, "5") == glz::error_code::none);
      expect(bool(ptr) && *ptr == 5);
      buffer.clear();
      expect(not glz::write_json(ptr, buffer));
      expect(buffer == "5");

      expect(glz::read_json(ptr, "null") == glz::error_code::none);
      expect(!bool(ptr));
      buffer.clear();
      expect(not glz::write_json(ptr, buffer));
      expect(buffer == "null");
   };
   "unique_ptr"_test = [] {
      std::unique_ptr<int> ptr{};
      std::string buffer{};
      expect(not glz::write_json(ptr, buffer));
      expect(buffer == "null");

      expect(glz::read_json(ptr, "5") == glz::error_code::none);
      expect(bool(ptr) && *ptr == 5);
      buffer.clear();
      expect(not glz::write_json(ptr, buffer));
      expect(buffer == "5");

      expect(glz::read_json(ptr, "null") == glz::error_code::none);
      expect(!bool(ptr));
      buffer.clear();
      expect(not glz::write_json(ptr, buffer));
      expect(buffer == "null");
   };
   "raw_pointer"_test = [] {
      int* ptr = nullptr;
      std::string buffer{};
      expect(not glz::write_json(ptr, buffer));
      expect(buffer == "null");

      int value = 42;
      ptr = &value;
      buffer.clear();
      expect(not glz::write_json(ptr, buffer));
      expect(buffer == "42");

      ptr = nullptr;
      buffer.clear();
      expect(not glz::write_json(ptr, buffer));
      expect(buffer == "null");
   };
   "raw_pointer_in_struct"_test = [] {
      ptr_struct obj;
      std::string buffer{};
      expect(not glz::write_json(obj, buffer));
      expect(buffer == "{}");

      int value = 99;
      obj.val = &value;
      buffer.clear();
      expect(not glz::write_json(obj, buffer));
      expect(buffer == R"({"val":99})");

      obj.val = nullptr;
      buffer.clear();
      expect(not glz::write_json(obj, buffer));
      expect(buffer == "{}");
   };
   "multiple_pointers_in_struct"_test = [] {
      multi_ptr_struct obj;
      std::string buffer{};
      expect(not glz::write_json(obj, buffer));
      expect(buffer == "{}");

      int i = 123;
      double d = 3.14;
      std::string s = "hello";
      obj.ptr1 = &i;
      obj.ptr2 = &d;
      obj.ptr3 = &s;
      buffer.clear();
      expect(not glz::write_json(obj, buffer));
      expect(buffer == R"({"ptr1":123,"ptr2":3.14,"ptr3":"hello"})");

      obj.ptr2 = nullptr;
      buffer.clear();
      expect(not glz::write_json(obj, buffer));
      expect(buffer == R"({"ptr1":123,"ptr3":"hello"})");
   };
   "pointer_skip_null_members_false"_test = [] {
      constexpr auto opts = glz::opts{.skip_null_members = false};
      ptr_opt_struct obj;
      std::string buffer{};

      // Both pointer and optional should write null when skip_null_members = false
      expect(not glz::write<opts>(obj, buffer));
      expect(buffer == R"({"ptr":null,"opt":null,"value":42})");

      // Set values and verify they're written
      int val = 99;
      obj.ptr = &val;
      obj.opt = 88;
      buffer.clear();
      expect(not glz::write<opts>(obj, buffer));
      expect(buffer == R"({"ptr":99,"opt":88,"value":42})");
   };
   "pointer_skip_null_members_true"_test = [] {
      // Default is skip_null_members = true
      ptr_opt_struct obj;
      std::string buffer{};

      // Both pointer and optional should be omitted when null
      expect(not glz::write_json(obj, buffer));
      expect(buffer == R"({"value":42})");

      // Set values and verify they're written
      int val = 99;
      obj.ptr = &val;
      obj.opt = 88;
      buffer.clear();
      expect(not glz::write_json(obj, buffer));
      expect(buffer == R"({"ptr":99,"opt":88,"value":42})");
   };
};

suite enum_types = [] {
   using namespace ut;
   "enum"_test = [] {
      Color color = Color::Red;
      std::string buffer{};
      expect(not glz::write_json(color, buffer));
      expect(buffer == "\"Red\"");

      expect(glz::read_json(color, "\"Green\"") == glz::error_code::none);
      expect(color == Color::Green);
      buffer.clear();
      expect(not glz::write_json(color, buffer));
      expect(buffer == "\"Green\"");
   };

   "invalid enum"_test = [] {
      Color color = Color::Red;
      expect(glz::read_json(color, "\"Silver\"") == glz::error_code::unexpected_enum);
      expect(color == Color::Red);
   };
};

suite user_types = [] {
   using namespace ut;

   "user array"_test = [] {
      V3 v3{9.1, 7.2, 1.9};
      std::string buffer{};
      expect(not glz::write_json(v3, buffer));
      expect(buffer == "[9.1,7.2,1.9]");

      expect(glz::read_json(v3, "[42.1,99.2,55.3]") == glz::error_code::none);
      expect(v3.x == 42.1 && v3.y == 99.2 && v3.z == 55.3);
   };

   "simple user obect"_test = [] {
      sub_thing obj{.a = 77.2, .b = "not a lizard"};
      std::string buffer{};
      expect(not glz::write_json(obj, buffer));
      expect(buffer == "{\"a\":77.2,\"b\":\"not a lizard\"}");

      expect(glz::read_json(obj, "{\"a\":999,\"b\":\"a boat of goldfish\"}") == glz::error_code::none);
      expect(obj.a == 999.0 && obj.b == "a boat of goldfish");

      // Should skip invalid keys
      // glaze::read_json(obj,"{/**/ \"b\":\"fox\", \"c\":7.7/**/, \"d\": {\"a\": \"}\"} //\n   /**/, \"a\":322}");
      expect(glz::read<glz::opts{.comments = true, .error_on_unknown_keys = false}>(
                obj,
                R"({/**/ "b":"fox", "c":7.7/**/, "d": {"a": "}"} //
/**/, "a":322})") == glz::error_code::none);

      // glaze::read_json(obj,"{/**/ \"b\":\"fox\", \"c\":7.7/**/, \"d\": {\"a\": \"}\"} //\n   /**/, \"a\":322}");
      auto ec = glz::read<glz::opts{.comments = true}>(obj,
                                                       R"({/**/ "b":"fox", "c":7.7/**/, "d": {"a": "}"} //
   /**/, "a":322})");
      expect(ec != glz::error_code::none);
      expect(obj.a == 322.0 && obj.b == "fox");
   };

   "complex user obect"_test = [] {
      Thing obj{};
      std::string buffer{};
      expect(not glz::write_json(obj, buffer));
      expect(
         buffer ==
         R"({"thing":{"a":3.14,"b":"stuff"},"thing2array":[{"a":3.14,"b":"stuff","c":999.342494903,"d":1E-12,"e":203082348402.1,"f":89.089,"g":12380.00000013,"h":1000000.000001}],"vec3":[3.14,2.7,6.5],"list":[6,7,8,2],"deque":[9,6.7,3.1],"vector":[[9,6.7,3.1],[3.14,2.7,6.5]],"i":8,"d":2,"b":false,"c":"W","v":{"x":0},"color":"Green","vb":[true,false,false,true,true,true,true],"sptr":{"a":3.14,"b":"stuff"},"array":["as\"df\\ghjkl","pie","42","foo"],"map":{"a":4,"b":12,"f":7},"mapi":{"2":9.63,"5":3.14,"7":7.42},"thing_ptr":{"a":3.14,"b":"stuff"}})")
         << buffer;

      buffer.clear();
      expect(not glz::write<glz::opts{.skip_null_members = false}>(obj, buffer));
      expect(
         buffer ==
         R"({"thing":{"a":3.14,"b":"stuff"},"thing2array":[{"a":3.14,"b":"stuff","c":999.342494903,"d":1E-12,"e":203082348402.1,"f":89.089,"g":12380.00000013,"h":1000000.000001}],"vec3":[3.14,2.7,6.5],"list":[6,7,8,2],"deque":[9,6.7,3.1],"vector":[[9,6.7,3.1],[3.14,2.7,6.5]],"i":8,"d":2,"b":false,"c":"W","v":{"x":0},"color":"Green","vb":[true,false,false,true,true,true,true],"sptr":{"a":3.14,"b":"stuff"},"optional":null,"array":["as\"df\\ghjkl","pie","42","foo"],"map":{"a":4,"b":12,"f":7},"mapi":{"2":9.63,"5":3.14,"7":7.42},"thing_ptr":{"a":3.14,"b":"stuff"}})")
         << buffer;

      expect(glz::read_json(obj, buffer) == glz::error_code::none);

      buffer.clear();
      expect(not glz::write_jsonc(obj, buffer));
      expect(
         buffer ==
         R"({"thing":{"a":3.14,"b":"stuff"},"thing2array":[{"a":3.14,"b":"stuff","c":999.342494903,"d":1E-12,"e":203082348402.1,"f":89.089,"g":12380.00000013,"h":1000000.000001}],"vec3":[3.14,2.7,6.5],"list":[6,7,8,2],"deque":[9,6.7,3.1],"vector":[[9,6.7,3.1],[3.14,2.7,6.5]],"i":8,"d":2,"b":false,"c":"W","v":{"x":0},"color":"Green","vb":[true,false,false,true,true,true,true],"sptr":{"a":3.14,"b":"stuff"},"array":["as\"df\\ghjkl","pie","42","foo"],"map":{"a":4,"b":12,"f":7},"mapi":{"2":9.63,"5":3.14,"7":7.42},"thing_ptr":{"a":3.14,"b":"stuff"}})")
         << buffer;
      expect(glz::read_jsonc(obj, buffer) == glz::error_code::none);
   };

   "complex user obect opts prettify"_test = [] {
      Thing obj{};
      std::string buffer{};
      expect(not glz::write<glz::opts{.prettify = true}>(obj, buffer));
      std::string thing_pretty = R"({
   "thing": {
      "a": 3.14,
      "b": "stuff"
   },
   "thing2array": [
      {
         "a": 3.14,
         "b": "stuff",
         "c": 999.342494903,
         "d": 1E-12,
         "e": 203082348402.1,
         "f": 89.089,
         "g": 12380.00000013,
         "h": 1000000.000001
      }
   ],
   "vec3": [
      3.14,
      2.7,
      6.5
   ],
   "list": [
      6,
      7,
      8,
      2
   ],
   "deque": [
      9,
      6.7,
      3.1
   ],
   "vector": [
      [
         9,
         6.7,
         3.1
      ],
      [
         3.14,
         2.7,
         6.5
      ]
   ],
   "i": 8,
   "d": 2,
   "b": false,
   "c": "W",
   "v": {
      "x": 0
   },
   "color": "Green",
   "vb": [
      true,
      false,
      false,
      true,
      true,
      true,
      true
   ],
   "sptr": {
      "a": 3.14,
      "b": "stuff"
   },
   "array": [
      "as\"df\\ghjkl",
      "pie",
      "42",
      "foo"
   ],
   "map": {
      "a": 4,
      "b": 12,
      "f": 7
   },
   "mapi": {
      "2": 9.63,
      "5": 3.14,
      "7": 7.42
   },
   "thing_ptr": {
      "a": 3.14,
      "b": "stuff"
   }
})";
      expect(thing_pretty == buffer) << buffer;
   };

   "complex user obect opts prettify, new_lines_in_arrays = false"_test = [] {
      Thing obj{};
      std::string buffer{};
      expect(not glz::write<glz::opts{.prettify = true, .new_lines_in_arrays = false}>(obj, buffer));
      std::string_view thing_pretty = R"({
   "thing": {
      "a": 3.14,
      "b": "stuff"
   },
   "thing2array": [{
      "a": 3.14,
      "b": "stuff",
      "c": 999.342494903,
      "d": 1E-12,
      "e": 203082348402.1,
      "f": 89.089,
      "g": 12380.00000013,
      "h": 1000000.000001
   }],
   "vec3": [3.14, 2.7, 6.5],
   "list": [6, 7, 8, 2],
   "deque": [9, 6.7, 3.1],
   "vector": [[9, 6.7, 3.1], [3.14, 2.7, 6.5]],
   "i": 8,
   "d": 2,
   "b": false,
   "c": "W",
   "v": {
      "x": 0
   },
   "color": "Green",
   "vb": [true, false, false, true, true, true, true],
   "sptr": {
      "a": 3.14,
      "b": "stuff"
   },
   "array": ["as\"df\\ghjkl", "pie", "42", "foo"],
   "map": {
      "a": 4,
      "b": 12,
      "f": 7
   },
   "mapi": {
      "2": 9.63,
      "5": 3.14,
      "7": 7.42
   },
   "thing_ptr": {
      "a": 3.14,
      "b": "stuff"
   }
})";
      expect(thing_pretty == buffer) << buffer;
   };

   "complex user obect prettify_json/minify_json"_test = [] {
      Thing obj{};
      std::string json{};
      expect(not glz::write_json(obj, json));
      std::string buffer{};
      glz::prettify_json(json, buffer);

      std::string thing_pretty = R"({
   "thing": {
      "a": 3.14,
      "b": "stuff"
   },
   "thing2array": [
      {
         "a": 3.14,
         "b": "stuff",
         "c": 999.342494903,
         "d": 1E-12,
         "e": 203082348402.1,
         "f": 89.089,
         "g": 12380.00000013,
         "h": 1000000.000001
      }
   ],
   "vec3": [
      3.14,
      2.7,
      6.5
   ],
   "list": [
      6,
      7,
      8,
      2
   ],
   "deque": [
      9,
      6.7,
      3.1
   ],
   "vector": [
      [
         9,
         6.7,
         3.1
      ],
      [
         3.14,
         2.7,
         6.5
      ]
   ],
   "i": 8,
   "d": 2,
   "b": false,
   "c": "W",
   "v": {
      "x": 0
   },
   "color": "Green",
   "vb": [
      true,
      false,
      false,
      true,
      true,
      true,
      true
   ],
   "sptr": {
      "a": 3.14,
      "b": "stuff"
   },
   "array": [
      "as\"df\\ghjkl",
      "pie",
      "42",
      "foo"
   ],
   "map": {
      "a": 4,
      "b": 12,
      "f": 7
   },
   "mapi": {
      "2": 9.63,
      "5": 3.14,
      "7": 7.42
   },
   "thing_ptr": {
      "a": 3.14,
      "b": "stuff"
   }
})";
      expect(thing_pretty == buffer);

      const auto minified = glz::minify_json(thing_pretty);
      expect(json == minified);
      const auto ec = glz::read<glz::opts{.minified = true}>(obj, minified);
      expect(!ec) << glz::format_error(ec, minified);
   };

   "complex user obect prettify_jsonc/minify_jsonc"_test = [] {
      Thing obj{};
      std::string json{};
      expect(not glz::write_jsonc(obj, json));
      std::string buffer{};
      glz::prettify_jsonc(json, buffer);

      std::string thing_pretty = R"({
   "thing": {
      "a": 3.14,
      "b": "stuff"
   },
   "thing2array": [
      {
         "a": 3.14,
         "b": "stuff",
         "c": 999.342494903,
         "d": 1E-12,
         "e": 203082348402.1,
         "f": 89.089,
         "g": 12380.00000013,
         "h": 1000000.000001
      }
   ],
   "vec3": [
      3.14,
      2.7,
      6.5
   ],
   "list": [
      6,
      7,
      8,
      2
   ],
   "deque": [
      9,
      6.7,
      3.1
   ],
   "vector": [
      [
         9,
         6.7,
         3.1
      ],
      [
         3.14,
         2.7,
         6.5
      ]
   ],
   "i": 8,
   "d": 2,
   "b": false,
   "c": "W",
   "v": {
      "x": 0
   },
   "color": "Green",
   "vb": [
      true,
      false,
      false,
      true,
      true,
      true,
      true
   ],
   "sptr": {
      "a": 3.14,
      "b": "stuff"
   },
   "array": [
      "as\"df\\ghjkl",
      "pie",
      "42",
      "foo"
   ],
   "map": {
      "a": 4,
      "b": 12,
      "f": 7
   },
   "mapi": {
      "2": 9.63,
      "5": 3.14,
      "7": 7.42
   },
   "thing_ptr": {
      "a": 3.14,
      "b": "stuff"
   }
})";

      expect(thing_pretty == buffer);
      expect(!glz::read_jsonc(obj, buffer));
      const auto minified = glz::minify_jsonc(thing_pretty);
      expect(json == minified);
      expect(!glz::read_jsonc(obj, minified));
   };

   "complex user obect roundtrip"_test = [] {
      std::string buffer{};

      Thing obj{};
      obj.thing.a = 5.7;
      obj.thing2array[0].a = 992;
      obj.vec3.x = 1.004;
      obj.list = {9, 3, 7, 4, 2};
      obj.array = {"life", "of", "pi", "!"};
      obj.vector = {{7, 7, 7}, {3, 6, 7}};
      obj.i = 4;
      obj.d = 0.9;
      obj.b = true;
      obj.c = 'L';
      obj.v = std::variant_alternative_t<1, decltype(obj.v)>{};
      obj.color = Color::Blue;
      obj.vb = {false, true, true, false, false, true, true};
      obj.sptr = nullptr;
      obj.optional = {1, 2, 3};
      obj.deque = {0.0, 2.2, 3.9};
      obj.map = {{"a", 7}, {"f", 3}, {"b", 4}};
      obj.mapi = {{5, 5.0}, {7, 7.1}, {2, 2.22222}};

      // glz::write_json(obj, buffer);
      expect(not glz::write<glz::opts{.skip_null_members = false}>(obj, buffer)); // Sets sptr to null

      Thing obj2{};
      expect(glz::read_json(obj2, buffer) == glz::error_code::none);

      expect(obj2.thing.a == 5.7);
      expect(obj2.thing.a == 5.7);
      expect(obj2.thing2array[0].a == 992);
      expect(obj2.vec3.x == 1.004);
      expect(obj2.list == decltype(obj2.list){9, 3, 7, 4, 2});
      expect(obj2.array == decltype(obj2.array){"life", "of", "pi", "!"});
      expect(obj2.vector == decltype(obj2.vector){{7, 7, 7}, {3, 6, 7}});
      expect(obj2.i == 4);
      expect(obj2.d == 0.9);
      expect(obj2.b == true);
      expect(obj2.c == 'L');
      expect(obj2.v.index() == 1);
      expect(obj2.color == Color::Blue);
      expect(obj2.vb == decltype(obj2.vb){false, true, true, false, false, true, true});
      expect(obj2.sptr == nullptr);
      expect(obj2.optional == std::make_optional<V3>(V3{1, 2, 3}));
      expect(obj2.deque == decltype(obj2.deque){0.0, 2.2, 3.9});
      expect(obj2.map == decltype(obj2.map){{"a", 7}, {"f", 3}, {"b", 4}});
      expect(obj2.mapi == decltype(obj2.mapi){{5, 5.0}, {7, 7.1}, {2, 2.22222}});
   };

   "complex user obect member names"_test = [] {
      expect(glz::name_v<glz::detail::member_tuple_t<Thing>> ==
             "glz::tuple<sub_thing,std::array<sub_thing2,1>,V3,std::list<int32_t>,std::deque<double>,std::"
             "vector<V3>,int32_t,double,bool,char,std::variant<var1_t,var2_t>,Color,std::vector<bool>,std::shared_ptr<"
             "sub_thing>,std::optional<V3>,std::array<std::string,4>,std::map<std::string,int32_t>,std::map<int32_t,"
             "double>,sub_thing*>");
   };
};

struct large_length_range_t
{
   int a{};
   int another_integer_value{};

   struct glaze
   {
      using T = large_length_range_t;
      static constexpr auto value = glz::object(&T::a, &T::another_integer_value);
   };
};

suite large_length_range = [] {
   using namespace ut;

   "large_length_range"_test = [] {
      large_length_range_t obj{};
      std::string_view s = R"({"a":55,"another_integer_value":77})";
      expect(!glz::read_json(obj, s));
      expect(obj.a == 55);
      expect(obj.another_integer_value == 77);
   };
};

suite json_pointer = [] {
   using namespace ut;

   "seek"_test = [] {
      Thing thing{};
      std::any a{};
      glz::seek([&](auto&& val) { a = val; }, thing, "/thing_ptr/a");
      expect(a.has_value() && std::any_cast<double>(a) == thing.thing_ptr->a);
   };

   "seek lambda"_test = [] {
      Thing thing{};
      std::any b{};
      glz::seek([&](auto&& val) { b = val; }, thing, "/thing/b");
      expect(b.has_value() && std::any_cast<std::string>(b) == thing.thing.b);
   };

   "get"_test = [] {
      Thing thing{};
      expect(thing.thing.a == glz::get<double>(thing, "/thing_ptr/a"));
      expect(&thing.map["f"] == glz::get_if<int>(thing, "/map/f"));
      expect(&thing.vector == glz::get_if<std::vector<V3>>(thing, "/vector"));
      expect(&thing.vector[1] == glz::get_if<V3>(thing, "/vector/1"));
      expect(thing.vector[1].x == glz::get<double>(thing, "/vector/1/0"));
      expect(thing.thing_ptr == glz::get<sub_thing*>(thing, "/thing_ptr"));

      // Invalid lookup
      expect(glz::get<char>(thing, "/thing_ptr/a").has_value() == false);
      expect(glz::get_if<char>(thing, "/thing_ptr/a") == nullptr);
      expect(glz::get<double>(thing, "/thing_ptr/c").has_value() == false);
      expect(glz::get_if<double>(thing, "/thing_ptr/c") == nullptr);
   };

   "set"_test = [] {
      Thing thing{};
      glz::set(thing, "/thing_ptr/a", 42.0);
      glz::set(thing, "/thing_ptr/b", "Value was set.");
      expect(thing.thing_ptr->a == 42.0);
      expect(thing.thing_ptr->b == "Value was set.");
   };

   /*"set tuplet"_test = [] {
      auto tuple = glz::tuplet::make_tuple(3, 2.7, std::string("curry"));
      glz::set(tuple, "/0", 5);
      glz::set(tuple, "/1", 42.0);
      glz::set(tuple, "/2", "fish");
      expect(glz::get<0>(tuple) == 5.0);
      expect(glz::get<1>(tuple) == 42.0);
      expect(glz::get<2>(tuple) == "fish");
   };*/

   "set tuple"_test = [] {
      auto tuple = std::make_tuple(3, 2.7, std::string("curry"));
      glz::set(tuple, "/0", 5);
      glz::set(tuple, "/1", 42.0);
      glz::set(tuple, "/2", "fish");
      expect(std::get<0>(tuple) == 5.0);
      expect(std::get<1>(tuple) == 42.0);
      expect(std::get<2>(tuple) == "fish");
   };

   "read_as_json"_test = [] {
      Thing thing{};
      glz::read_as_json(thing, "/vec3", "[7.6, 1292.1, 0.333]");
      expect(thing.vec3.x == 7.6 && thing.vec3.y == 1292.1 && thing.vec3.z == 0.333);

      glz::read_as_json(thing, "/vec3/2", "999.9");
      expect(thing.vec3.z == 999.9);
   };

   "valid"_test = [] {
      // Compile time JSON Pointer syntax validation
      static_assert(glz::valid<Thing, "/thing_ptr/a", double>());
      static_assert(glz::valid<Thing, "/thing_ptr/a", int>() == false);
      static_assert(glz::valid<Thing, "/thing_ptr/b">());
      static_assert(glz::valid<Thing, "/thing_ptr/z">() == false);

      static_assert(glz::valid<Thing, "/vec3/2", double>());
      static_assert(glz::valid<Thing, "/vec3/3", double>() == false);

      static_assert(glz::valid<Thing, "/map/f", int>());
      static_assert(glz::valid<Thing, "/vector", std::vector<V3>>());
      static_assert(glz::valid<Thing, "/vector/1", V3>());
      static_assert(glz::valid<Thing, "/vector/1/0", double>());
   };

   "id bug"_test = [] {
      std::string json = R"({
        "data": [
            {
                "id": 44
            },
            {
                "id": 88
            },
        ]
    })";

      auto maybe_id = glz::get_sv_json<"/data/1/id">(json);
      expect(maybe_id.has_value());
      expect(maybe_id.value() == "88") << maybe_id.value();
   };
};

suite early_end = [] {
   using namespace ut;

   "should error"_test = [] {
      std::string_view buffer =
         R"({"thing":{"a":3.14/*Test comment 1*/,"b":"stuff"/*Test comment 2*/},"thing2array":[{"a":3.14/*Test comment 1*/,"b":"stuff"/*Test comment 2*/,"c":999.342494903,"d":1e-12,"e":203082348402.1,"f":89.089,"g":12380.00000013,)";

      glz::generic json{};
      static constexpr glz::opts options{.comments = true};
      expect(glz::read<options>(json, buffer));
   };

   "early_end comments"_test = [] {
      trace.begin("early_end");

      static constexpr glz::opts options{.comments = true};
      Thing obj{};
      glz::generic json{};
      glz::skip skip_me{};
      std::string buffer_data =
         R"({"thing":{"a":3.14/*Test comment 1*/,"b":"stuff"/*Test comment 2*/},"thing2array":[{"a":3.14/*Test comment 1*/,"b":"stuff"/*Test comment 2*/,"c":999.342494903,"d":1e-12,"e":203082348402.1,"f":89.089,"g":12380.00000013,"h":1000000.000001}],"vec3":[3.14,2.7,6.5],"list":[6,7,8,2],"deque":[9,6.7,3.1],"vector":[[9,6.7,3.1],[3.14,2.7,6.5]],"i":8,"d":2/*double is the best type*/,"b":false,"c":"W","vb":[true,false,false,true,true,true,true],"sptr":{"a":3.14/*Test comment 1*/,"b":"stuff"/*Test comment 2*/},"optional":null,"array":["as\"df\\ghjkl","pie","42","foo"],"map":{"a":4,"b":12,"f":7},"mapi":{"2":9.63,"5":3.14,"7":7.42},"thing_ptr":{"a":3.14/*Test comment 1*/,"b":"stuff"/*Test comment 2*/}})";
      std::string_view buffer = buffer_data;

      while (buffer.size() > 0) {
         buffer_data.pop_back();
         buffer = buffer_data;
         // This is mainly to check if all our end checks are in place.
         auto ec = glz::read<options>(obj, buffer);
         expect(ec);
         expect(ec.location <= buffer.size());
         ec = glz::read<options>(json, buffer);
         expect(ec);
         expect(ec.location <= buffer.size());
         ec = glz::read<options>(skip_me, buffer);
         expect(ec);
         expect(ec.location <= buffer.size());
      }
   };

   "early_end"_test = [] {
      Thing obj{};
      glz::generic json{};
      glz::skip skip_me{};
      std::string buffer_data =
         R"({"thing":{"a":3.14,"b":"stuff"},"thing2array":[{"a":3.14,"b":"stuff","c":999.342494903,"d":1e-12,"e":203082348402.1,"f":89.089,"g":12380.00000013,"h":1000000.000001}],"vec3":[3.14,2.7,6.5],"list":[6,7,8,2],"deque":[9,6.7,3.1],"vector":[[9,6.7,3.1],[3.14,2.7,6.5]],"i":8,"d":2,"b":false,"c":"W","vb":[true,false,false,true,true,true,true],"sptr":{"a":3.14,"b":"stuff"},"optional":null,"array":["as\"df\\ghjkl","pie","42","foo"],"map":{"a":4,"b":12,"f":7},"mapi":{"2":9.63,"5":3.14,"7":7.42},"thing_ptr":{"a":3.14,"b":"stuff"}})";
      std::string_view buffer = buffer_data;
      while (buffer.size() > 0) {
         buffer_data.pop_back();
         buffer = buffer_data;
         // This is mainly to check if all our end checks are in place.
         auto ec = glz::read_json(obj, buffer);
         expect(ec);
         expect(ec.location <= buffer.size());
         ec = glz::read_json(json, buffer);
         expect(ec);
         expect(ec.location <= buffer.size());
         ec = glz::read_json(skip_me, buffer);
         expect(ec);
         expect(ec.location <= buffer.size());
      }
   };

   "early_end !null terminated"_test = [] {
      static constexpr glz::opts options{.null_terminated = false};

      Thing obj{};
      glz::generic json{};
      glz::skip skip_me{};
      std::string_view buffer_data =
         R"({"thing":{"a":3.14,"b":"stuff"},"thing2array":[{"a":3.14,"b":"stuff","c":999.342494903,"d":1e-12,"e":203082348402.1,"f":89.089,"g":12380.00000013,"h":1000000.000001}],"vec3":[3.14,2.7,6.5],"list":[6,7,8,2],"deque":[9,6.7,3.1],"vector":[[9,6.7,3.1],[3.14,2.7,6.5]],"i":8,"d":2,"b":false,"c":"W","vb":[true,false,false,true,true,true,true],"sptr":{"a":3.14,"b":"stuff"},"optional":null,"array":["as\"df\\ghjkl","pie","42","foo"],"map":{"a":4,"b":12,"f":7},"mapi":{"2":9.63,"5":3.14,"7":7.42},"thing_ptr":{"a":3.14,"b":"stuff"}})";
      std::vector<char> temp{buffer_data.begin(), buffer_data.end()};
      std::string_view buffer{temp.data(), temp.data() + temp.size()};
      while (buffer.size() > 0) {
         temp.pop_back();
         buffer = {temp.data(), temp.data() + temp.size()};
         // This is mainly to check if all our end checks are in place.
         auto ec = glz::read<options>(obj, buffer);
         expect(ec);
         expect(ec.location <= buffer.size());
         ec = glz::read<options>(json, buffer);
         expect(ec);
         expect(ec.location <= buffer.size());
         ec = glz::read<options>(skip_me, buffer);
         expect(ec);
         expect(ec.location <= buffer.size());
      }
   };

   trace.end("early_end");
};

suite minified_custom_object = [] {
   using namespace ut;

   "minified_custom_object"_test = [] {
      Thing obj{};
      std::string buffer = glz::write_json(obj).value_or("error");
      std::string prettified = glz::prettify_json(buffer);
      std::string minified = glz::minify_json(prettified);
      expect(!glz::read_json(obj, minified));
      expect(buffer == minified);
   };

   "minified compile time option"_test = [] {
      Thing obj{};
      std::string buffer = glz::write_json(obj).value_or("error");
      std::string prettified = glz::prettify_json(buffer);
      std::string minified = glz::minify_json(prettified);
      expect(!glz::read<glz::opts{.minified = true}>(obj, minified));
      expect(buffer == minified);
   };
};

suite prettified_custom_object = [] {
   using namespace ut;

   "prettified_custom_object"_test = [] {
      Thing obj{};
      std::string buffer = glz::write_json(obj).value_or("error");
      buffer = glz::prettify_json(buffer);
      expect(glz::read_json(obj, buffer) == glz::error_code::none);
   };
};

suite bench = [] {
   using namespace ut;
   "bench"_test = [] {
      trace.begin("bench");
      std::cout << "\nPerformance regresion test: \n";
#ifdef NDEBUG
      size_t repeat = 100000;
#else
      size_t repeat = 1000;
#endif
      Thing thing{};

      std::string buffer;
      expect(not glz::write_json(thing, buffer));

      trace.begin("write_bench", "JSON writing benchmark");
      auto tstart = std::chrono::high_resolution_clock::now();
      for (size_t i{}; i < repeat; ++i) {
         buffer.clear();
         expect(not glz::write_json(thing, buffer));
      }
      auto tend = std::chrono::high_resolution_clock::now();
      trace.end("write_bench");
      auto duration = std::chrono::duration_cast<std::chrono::duration<double>>(tend - tstart).count();
      auto mbytes_per_sec = repeat * buffer.size() / (duration * 1048576);
      std::cout << "write_json size: " << buffer.size() << " bytes\n";
      std::cout << "write_json: " << duration << " s, " << mbytes_per_sec << " MB/s"
                << "\n";

      trace.begin("read_bench");
      tstart = std::chrono::high_resolution_clock::now();
      for (size_t i{}; i < repeat; ++i) {
         expect(glz::read_json(thing, buffer) == glz::error_code::none);
      }
      tend = std::chrono::high_resolution_clock::now();
      trace.end("read_bench", "JSON reading benchmark");
      duration = std::chrono::duration_cast<std::chrono::duration<double>>(tend - tstart).count();
      mbytes_per_sec = repeat * buffer.size() / (duration * 1048576);
      std::cout << "read_json: " << duration << " s, " << mbytes_per_sec << " MB/s"
                << "\n";

      trace.begin("json_ptr_bench");
      tstart = std::chrono::high_resolution_clock::now();
      for (size_t i{}; i < repeat; ++i) {
         std::ignore = glz::get<std::string>(thing, "/thing_ptr/b");
      }
      tend = std::chrono::high_resolution_clock::now();
      trace.end("json_ptr_bench", "JSON pointer benchmark");
      duration = std::chrono::duration_cast<std::chrono::duration<double>>(tend - tstart).count();
      std::cout << "get: " << duration << " s, " << (repeat / duration) << " gets/s"
                << "\n\n";
      trace.end("bench");
   };
};

struct v3
{
   double x{}, y{}, z{};

   auto operator<=>(const v3&) const = default;
};

struct oob
{
   v3 v{};
   int n{};
};

template <>
struct glz::meta<v3>
{
   static constexpr std::string_view name = "v3";
   static constexpr auto value = array(&v3::x, &v3::y, &v3::z);
};

static_assert(glz::is_specialization_v<std::decay_t<decltype(glz::meta<v3>::value)>, glz::detail::Array>, "");

template <>
struct glz::meta<oob>
{
   static constexpr std::string_view name = "oob";
   static constexpr auto value = object("v", &oob::v, "n", &oob::n);
};

template <typename Pair_key, typename Pair_value>
struct Read_pair_test_case
{
   Pair_key expected_key{};
   Pair_value expected_value{};
   std::string_view input_json{};
};
template <typename Pair_key, typename Pair_value>
Read_pair_test_case(Pair_key, Pair_value, std::string_view) -> Read_pair_test_case<Pair_key, Pair_value>;

suite read_tests = [] {
   using namespace ut;

   "string read"_test = [] {
      std::string s{"3958713"};
      int i{};
      expect(glz::read_json(i, s) == glz::error_code::none);
      expect(i == 3958713);

      s.clear();
      s = R"({"v":[0.1, 0.2, 0.3]})";
      oob obj{};
      expect(glz::read_json(obj, s) == glz::error_code::none);
      expect(obj.v == v3{0.1, 0.2, 0.3});
   };

   "Read floating point types"_test = [] {
      {
         std::string s = "0.96875";
         float f{};
         expect(glz::read_json(f, s) == glz::error_code::none);
         expect(f == 0.96875f);
      }
      {
         std::string s = "0.96875";
         double f{};
         expect(glz::read_json(f, s) == glz::error_code::none);
         expect(f == 0.96875);
      }
      {
         std::string str = "0.96875";
         std::vector<char> s(str.begin(), str.end());
         s.emplace_back('\0'); // null terminate buffer
         double f{};
         expect(glz::read_json(f, s) == glz::error_code::none);
         expect(f == 0.96875);
      }
   };

   "Read integral types"_test = [] {
      {
         std::string s = "true";
         bool v{};
         expect(glz::read_json(v, s) == glz::error_code::none);
         expect(v);
      }
      {
         std::string s = "1";
         short v{};
         expect(glz::read_json(v, s) == glz::error_code::none);
         expect(v == 1);
      }
      {
         std::string s = "1";
         int v{};
         expect(glz::read_json(v, s) == glz::error_code::none);
         expect(v == 1);
      }
      {
         std::string s = "1";
         long v{};
         expect(glz::read_json(v, s) == glz::error_code::none);
         expect(v == 1);
      }
      {
         std::string s = "1";
         long long v{};
         expect(glz::read_json(v, s) == glz::error_code::none);
         expect(v == 1);
      }
      {
         std::string s = "1";
         unsigned short v{};
         expect(glz::read_json(v, s) == glz::error_code::none);
         expect(v == 1);
      }
      {
         std::string s = "1";
         unsigned int v{};
         expect(glz::read_json(v, s) == glz::error_code::none);
         expect(v == 1);
      }
      {
         std::string s = "1";
         unsigned long v{};
         expect(glz::read_json(v, s) == glz::error_code::none);
         expect(v == 1);
      }
      {
         std::string s = "1";
         unsigned long long v{};
         expect(glz::read_json(v, s) == glz::error_code::none);
         expect(v == 1);
      }
   };

   "comments"_test = [] {
      {
         std::string b = "1/*a comment*/00";
         int a{};
         expect(glz::read_jsonc(a, b) == glz::error_code::none);
         expect(a == 1);
      }
      {
         std::string b = R"([100, // a comment
20])";
         std::vector<int> a{};
         expect(glz::read_jsonc(a, b) == glz::error_code::none);
         expect(a[0] == 100);
         expect(a[1] == 20);
      }
      {
         std::string json = R"({
    // Comment 1
    "array_1": [],
    // Comment 2
    "array_2": []
})";
         jsonc_comment_config cfg{};
         expect(glz::read_jsonc(cfg, json) == glz::error_code::none);
         expect(cfg.array_1.empty());
         expect(cfg.array_2.empty());
      }
   };

   "Failed character read"_test = [] {
      std::string err;
      {
         char b;
         expect(glz::read_json(b, err) != glz::error_code::none);
      }
   };

   "Read array type"_test = [] {
      std::string in = "    [ 3.25 , 1.125 , 3.0625 ]   ";
      v3 v{};
      expect(glz::read_json(v, in) == glz::error_code::none);

      expect(v.x == 3.25);
      expect(v.y == 1.125);
      expect(v.z == 3.0625);
   };

   "Read partial array type"_test = [] {
      // partial reading of fixed sized arrays
      {
         std::string in = "    [ 3.25 , 3.125 ]   ";
         [[maybe_unused]] v3 v{};
         expect(glz::read_json(v, in) == glz::error_code::none);

         expect(v.x == 3.25);
         expect(v.y == 3.125);
         expect(v.z == 0.0);
      }
   };

   "Read object type"_test = [] {
      std::string in = R"(    { "v" :  [ 3.25 , 1.125 , 3.0625 ]   , "n" : 5 } )";
      oob oob{};
      expect(glz::read_json(oob, in) == glz::error_code::none);

      expect(oob.v.x == 3.25);
      expect(oob.v.y == 1.125);
      expect(oob.v.z == 3.0625);
      expect(oob.n == 5);
   };

   "Read partial object type"_test = [] {
      std::string in = R"(    { "v" :  [ 3.25 , null , 3.0625 ]   , "n" : null } )";
      oob oob{};

      expect(glz::read_json(oob, in) != glz::error_code::none);
   };

   "Reversed object"_test = [] {
      std::string in = R"(    {  "n" : 5   ,  "v" :  [ 3.25 , 1.125 , 3.0625 ] } )";
      oob oob{};
      expect(glz::read_json(oob, in) == glz::error_code::none);

      expect(oob.v.x == 3.25);
      expect(oob.v.y == 1.125);
      expect(oob.v.z == 3.0625);
      expect(oob.n == 5);
   };

   "Read list"_test = [] {
      std::string in = "[1, 2, 3, 4]";
      std::list<int> l, lr{1, 2, 3, 4};
      expect(glz::read_json(l, in) == glz::error_code::none);

      expect(l == lr);
   };

   "Read forward list"_test = [] {
      std::string in = "[1, 2, 3, 4]";
      std::forward_list<int> l, lr{1, 2, 3, 4};
      expect(glz::read_json(l, in) == glz::error_code::none);

      expect(l == lr);
   };

   "Read deque"_test = [] {
      {
         std::string in = "[1, 2, 3, 4]";
         std::deque<int> l, lr{1, 2, 3, 4};
         expect(glz::read_json(l, in) == glz::error_code::none);

         expect(l == lr);
      }
      {
         std::string in = "[1, 2, 3, 4]";
         std::deque<int> l{8, 9}, lr{1, 2, 3, 4};
         expect(glz::read_json(l, in) == glz::error_code::none);

         expect(l == lr);
      }
   };

   "Read into returned data"_test = [] {
      const std::string s = "[1, 2, 3, 4, 5, 6]";
      const std::vector<int> v{1, 2, 3, 4, 5, 6};
      std::vector<int> vr;
      expect(glz::read_json(vr, s) == glz::error_code::none);
      expect(vr == v);
   };

   "Read array"_test = [] {
      {
         std::string in = R"(    [1, 5, 232, 75, 123, 54, 89] )";
         std::array<int, 7> v1{}, v2{99}, v3{99, 99, 99, 99, 99}, vr{1, 5, 232, 75, 123, 54, 89};
         expect(glz::read_json(v1, in) == glz::error_code::none);
         expect(glz::read_json(v2, in) == glz::error_code::none);
         expect(glz::read_json(v3, in) == glz::error_code::none);
         expect(v1 == vr);
         expect(v2 == vr);
         expect(v3 == vr);
      }
   };

   "Read vector"_test = [] {
      {
         std::string in = R"(    [1, 5, 232, 75, 123, 54, 89] )";
         std::vector<int> v, vr{1, 5, 232, 75, 123, 54, 89};
         expect(glz::read_json(v, in) == glz::error_code::none);

         expect(v == vr);
      }
      {
         std::string in = R"([true, false, true, false])";
         std::vector<bool> v, vr{true, false, true, false};
         expect(glz::read_json(v, in) == glz::error_code::none);

         expect(v == vr);
      }
      {
         std::string in = R"(    [1, 5, 232, 75, 123, 54, 89] )";
         std::vector<int> v{1, 2, 3, 4}, vr{1, 5, 232, 75, 123, 54, 89};
         expect(glz::read_json(v, in) == glz::error_code::none);

         expect(v == vr);
      }
      {
         std::string in = R"(    [1, 5, 232, 75, 123, 54, 89] )";
         std::vector<int> v{1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, vr{1, 5, 232, 75, 123, 54, 89};
         expect(glz::read_json(v, in) == glz::error_code::none);

         expect(v == vr);
      }
   };

   "Read partial vector"_test = [] {
      std::string in = R"(    [1, 5, 232, 75, null, 54, 89] )";
      std::vector<int> v, vr{1, 5, 232, 75, 0, 54, 89};

      expect(glz::read_json(v, in) != glz::error_code::none);
   };

   "Read pair"_test = [] {
      auto tester = [](const auto& test_case) {
         const std::pair expected{test_case.expected_key, test_case.expected_value};
         std::remove_cv_t<decltype(expected)> parsed{};
         const auto err = glz::read_json(parsed, test_case.input_json);
         expect(err == glz::error_code::none) << glz::format_error(err, test_case.input_json);
         expect(parsed == expected) << glz::write_json(parsed).value_or("error");
      };

      std::tuple tests{
         Read_pair_test_case{1, 2, R"({"1":2})"},
         Read_pair_test_case{std::string{"key"}, 2, R"({"key":2})"},
         Read_pair_test_case{std::string{"key"}, std::string{"value"}, R"({"key":"value"})"},
         Read_pair_test_case{std::array{1, 2, 3}, std::array{4, 5, 6}, R"({"[1,2,3]":[4,5,6]})"},
      };

      glz::for_each_apply(tester, tests);
   };

   "Read map"_test = [] {
      constexpr std::string_view in = R"(   { "as" : 1, "so" : 2, "make" : 3 } )";
      {
         std::map<std::string, int> v, vr{{"as", 1}, {"so", 2}, {"make", 3}};
         expect(glz::read_json(v, in) == glz::error_code::none);
         const bool equal = (v == vr);
         expect(equal);
      }
      {
         std::map<std::string, int> v{{"as", -1}, {"make", 10000}}, vr{{"as", 1}, {"so", 2}, {"make", 3}};
         expect(glz::read_json(v, in) == glz::error_code::none);
         const bool equal = (v == vr);
         expect(equal);
      }
      {
         std::map<std::string_view, int> v, vr{{"as", 1}, {"so", 2}, {"make", 3}};
         expect(glz::read_json(v, in) == glz::error_code::none);
         const bool equal = (v == vr);
         expect(equal);
      }
      {
         std::map<std::string_view, int> v{{"as", -1}, {"make", 10000}}, vr{{"as", 1}, {"so", 2}, {"make", 3}};
         expect(glz::read_json(v, in) == glz::error_code::none);
         const bool equal = (v == vr);
         expect(equal);
      }
      {
         // allow unknown keys
         std::map<std::string_view, int> v{{"as", -1}, {"make", 10000}}, vr{{"as", 1}, {"so", 2}, {"make", 3}};
         const auto err = glz::read<glz::opts{.error_on_unknown_keys = false}>(v, in);
         expect(err == glz::error_code::none);
         const bool equal = (v == vr);
         expect(equal);
      }
   };

   "Read partial map"_test = [] {
      std::string in = R"(   { "as" : 1, "so" : null, "make" : 3 } )";
      std::map<std::string, int> v, vr{{"as", 1}, {"so", 0}, {"make", 3}};

      expect(glz::read_json(v, in) != glz::error_code::none);
   };

   "Read boolean"_test = [] {
      {
         std::string in = R"(true)";
         bool res{};
         expect(glz::read_json(res, in) == glz::error_code::none);

         expect(res == true);
      }
      {
         std::string in = R"(false)";
         bool res{true};
         expect(glz::read_json(res, in) == glz::error_code::none);

         expect(res == false);
      }
      {
         std::string in = R"(null)";
         bool res{false};

         expect(glz::read_json(res, in) != glz::error_code::none);
      }
   };

   "Read integer"_test = [] {
      {
         std::string in = R"(-1224125asdasf)";
         int res{};
         expect(glz::read_json(res, in) == glz::error_code::none);
         expect(res == -1224125);
      }
      {
         std::string in = R"(null)";
         int res{};
         expect(glz::read_json(res, in) == glz::error_code::parse_number_failure);
      }
      {
         auto in = R"(1.000000000000000000000000000000001)";
         uint64_t res{};
         expect(glz::read_json(res, in) == glz::error_code::parse_number_failure);
      }
      {
         auto in = R"(1.99999999999999999999999999)";
         uint64_t res{};
         expect(glz::read_json(res, in) == glz::error_code::parse_number_failure);
      }
      {
         auto in = R"(122.2345678910)";
         uint64_t res{};
         expect(glz::read_json(res, in) == glz::error_code::parse_number_failure);
      }
      {
         auto in = R"(100000.300e7)";
         uint64_t res{};
         expect(glz::read_json(res, in) == glz::error_code::parse_number_failure);
      }
      {
         auto in = R"(1002.34e+9)";
         uint64_t res{};
         expect(glz::read_json(res, in) == glz::error_code::parse_number_failure);
      }
      {
         std::mt19937_64 gen{std::random_device{}()};
         std::uniform_real_distribution<double> dist{-1.0e9, 1.0e9};
         std::string buffer{};
         for (size_t i = 0; i < 1000; ++i) {
            const auto f = dist(gen);
            expect(not glz::write_json(f, buffer));

            // Check if 'f' is exactly an integer
            const bool is_integer = (std::floor(f) == f);

            int64_t integer{};
            if (is_integer) {
               auto ec = glz::read_json(integer, buffer);
               expect(!ec); // Expect no error when reading as integer
               expect(integer == int64_t(f));
            }
            else {
               auto ec = glz::read_json(integer, buffer);
               expect(ec); // Expect an error when reading a non-integer as integer
            }
         }
      }
   };

   "Read double"_test = [] {
      {
         std::string in = R"(0.072265625flkka)";
         double res{};
         expect(glz::read_json(res, in) == glz::error_code::none);
         expect(res == 0.072265625);
      }
      {
         std::string in = R"(1e5das)";
         double res{};
         expect(glz::read_json(res, in) == glz::error_code::none);
         expect(res == 1e5);
      }
      {
         std::string in = R"(-0)";
         double res{};
         expect(glz::read_json(res, in) == glz::error_code::none);
         expect(res == -0.0);
      }
      {
         std::string in = R"(0e5)";
         double res{};
         expect(glz::read_json(res, in) == glz::error_code::none);
         expect(res == 0.0);
      }
      {
         std::string in = R"(0)";
         double res{};
         expect(glz::read_json(res, in) == glz::error_code::none);
         expect(res == 0.0);
      }
      {
         std::string in = R"(11)";
         double res{};
         expect(glz::read_json(res, in) == glz::error_code::none);
         expect(res == 11.0);
      }
      {
         std::string in = R"(0a)";
         double res{};
         expect(glz::read_json(res, in) == glz::error_code::none);
         expect(res == 0.0);
      }
      {
         std::string in = R"(11.0)";
         double res{};
         expect(glz::read_json(res, in) == glz::error_code::none);
         expect(res == 11.0);
      }
      {
         std::string in = R"(11e5)";
         double res{};
         expect(glz::read_json(res, in) == glz::error_code::none);
         expect(res == 11.0e5);
      }
      {
         std::string res = R"(success)";
         double d{};
         expect(glz::read_json(d, res) != glz::error_code::none);
      }
      {
         std::string res = R"(-success)";
         double d{};
         expect(glz::read_json(d, res) != glz::error_code::none);
      }
      {
         std::string res = R"(1.a)";
         double d{};
         expect(glz::read_json(d, res) != glz::error_code::none);
      }
      {
         std::string res = R"()";
         double d{};
         expect(glz::read_json(d, res) != glz::error_code::none);
      }
      {
         std::string res = R"(-)";
         double d{};
         expect(glz::read_json(d, res) != glz::error_code::none);
      }
      {
         std::string res = R"(1.)";
         double d{};
         expect(glz::read_json(d, res) != glz::error_code::none);
      }
      {
         std::string res = R"(1.0e)";
         double d{};
         expect(not glz::read_json(d, res));
         expect(d == 1.0);
      }
      {
         std::string res = R"(1.0e-)";
         double d{};
         expect(not glz::read_json(d, res));
         expect(d == 1.0);
      }
   };

   "random doubles"_test = [] {
      std::mt19937_64 g{std::random_device{}()};
      std::uniform_real_distribution<double> dist{};

      std::string buffer{};
      for (size_t i = 0; i < 1000; ++i) {
         double x = dist(g);
         expect(not glz::write_json(x, buffer));
         expect(not glz::read_json(x, buffer));
      }
   };

   "Read string"_test = [] {
      std::string in_nothrow = R"("asljl{}121231212441[]123::,,;,;,,::,Q~123\\a13dqwdwqwq")";
      std::string res{};

      expect(glz::read_json(res, in_nothrow) == glz::error_code::none);
      expect(res == "asljl{}121231212441[]123::,,;,;,,::,Q~123\\a13dqwdwqwq");

      std::string in_throw = R"("asljl{}121231212441[]123::,,;,;,,::,Q~123\a13dqwdwqwq")";
      res.clear();

      expect(glz::read_json(res, in_throw) != glz::error_code::none);
   };

   "Nested array"_test = [] {
      std::vector<v3> v;
      std::string buf = R"([[1.000000,0.000000,3.000000],[2.000000,0.000000,0.000000]])";

      expect(glz::read_json(v, buf) == glz::error_code::none);
      expect(v[0].x == 1.0);
      expect(v[0].z == 3.0);
      expect(v[1].x == 2.0);
   };

   "Nested map"_test = [] {
      std::map<std::string, v3> m;
      std::string buf = R"({"1":[4.000000,0.000000,0.000000],"2":[5.000000,0.000000,0.000000]})";

      expect(glz::read_json(m, buf) == glz::error_code::none);
      expect(m["1"].x == 4.0);
      expect(m["2"].x == 5.0);
   };

   "Nested map 2"_test = [] {
      std::map<std::string, std::vector<double>> m;
      std::string buf = R"({"1":[4.000000,0.000000,0.000000],"2":[5.000000,0.000000,0.000000,4.000000]})";

      expect(glz::read_json(m, buf) == glz::error_code::none);
      expect(m["1"][0] == 4.0);
      expect(m["2"][0] == 5.0);
      expect(m["2"][3] == 4.0);
   };

   "Integer keyed map"_test = [] {
      std::map<int, std::vector<double>> m;
      std::string buf = R"({"1":[4.000000,0.000000,0.000000],"2":[5.000000,0.000000,0.000000,4.000000]})";

      expect(glz::read_json(m, buf) == glz::error_code::none);
      expect(m[1][0] == 4.0);
      expect(m[2][0] == 5.0);
      expect(m[2][3] == 4.0);
   };
};

using Geodetic = v3;

struct ThreeODetic
{
   Geodetic g1{};
   int x1;
};

struct NineODetic
{
   ThreeODetic t1{};
   Geodetic g1;
};

struct Named
{
   std::string name;
   NineODetic value;
};

template <>
struct glz::meta<ThreeODetic>
{
   static constexpr std::string_view name = "ThreeODetic";
   using t = ThreeODetic;
   static constexpr auto value = array("geo", &t::g1, "int", &t::x1);
};

template <>
struct glz::meta<NineODetic>
{
   static constexpr std::string_view name = "NineODetic";
   using n = NineODetic;
   static constexpr auto value = array(&n::t1, &n::g1);
};

template <>
struct glz::meta<Named>
{
   static constexpr std::string_view name = "Named";
   using n = Named;
   static constexpr auto value = glz::object("name", &n::name, "value", &n::value);
};

struct EmptyArray
{};

template <>
struct glz::meta<EmptyArray>
{
   static constexpr std::string_view name = "EmptyArray";
   static constexpr auto value = array();
};

struct EmptyObject
{};

//* Empty object not allowed
template <>
struct glz::meta<EmptyObject>
{
   static constexpr std::string_view name = "EmptyObject";
   static constexpr auto value = object();
};

struct json_module_id
{
   uint64_t value{};

   auto operator<=>(const json_module_id&) const = default;
};

struct json_cast_module_id
{
   uint64_t value{};

   auto operator<=>(const json_cast_module_id&) const = default;
};

template <>
struct glz::meta<json_module_id>
{
   static constexpr auto value = &json_module_id::value;
};

template <>
struct glz::meta<json_cast_module_id>
{
   static constexpr auto value = glz::cast<&json_cast_module_id::value, uint64_t>;
};

template <>
struct std::hash<json_module_id>
{
   size_t operator()(const json_module_id& id) const noexcept { return std::hash<uint64_t>{}(id.value); }
};

template <>
struct std::hash<json_cast_module_id>
{
   size_t operator()(const json_cast_module_id& id) const noexcept { return std::hash<uint64_t>{}(id.value); }
};

suite strong_id_json_tests = [] {
   "map json_module_id"_test = [] {
      const std::map<json_module_id, std::string> src{{json_module_id{42}, "life"}, {json_module_id{9001}, "power"}};

      auto encoded = glz::write_json(src);
      expect(encoded.has_value());
      expect(*encoded == R"({"42":"life","9001":"power"})");

      std::map<json_module_id, std::string> dst{};
      expect(!glz::read_json(dst, *encoded));
      expect(dst == src);
   };

   "unordered_map json_module_id"_test = [] {
      const std::unordered_map<json_module_id, int> src{
         {json_module_id{1}, 7}, {json_module_id{2}, 11}, {json_module_id{99}, -4}};

      auto encoded = glz::write_json(src);
      expect(encoded.has_value());

      std::unordered_map<json_module_id, int> dst{};
      expect(!glz::read_json(dst, *encoded));
      expect(dst == src);
   };

   "map json_cast_module_id"_test = [] {
      const std::map<json_cast_module_id, int> src{{json_cast_module_id{3}, 9}, {json_cast_module_id{5}, 25}};

      auto encoded = glz::write_json(src);
      expect(encoded.has_value());
      expect(*encoded == R"({"3":9,"5":25})");

      std::map<json_cast_module_id, int> dst{};
      expect(!glz::read_json(dst, *encoded));
      expect(dst == src);
   };
};

template <typename Pair_key, typename Pair_value>
struct Write_pair_test_case
{
   Pair_key key{};
   Pair_value value{};
   std::string_view expected_json{};
};
template <typename Pair_key, typename Pair_value>
Write_pair_test_case(Pair_key, Pair_value, std::string_view) -> Write_pair_test_case<Pair_key, Pair_value>;

suite write_tests = [] {
   using namespace ut;

   "Write floating point types"_test = [] {
      {
         std::string s;
         float f{0.96875f};
         expect(not glz::write_json(f, s));
         expect(s == "0.96875") << s;
      }
      {
         std::string s;
         double f{0.96875};
         expect(not glz::write_json(f, s));
         expect(s == "0.96875") << s;
      }
   };

   "Write integral types"_test = [] {
      {
         std::string s;
         bool v{1};
         expect(not glz::write_json(v, s));
         expect(s == "true");
      }
      //* Two test below fail, unless changed to the following:
      {
         std::string s;
         char v{'a'};
         expect(not glz::write_json(v, s));
         // Is this what we want instead?
         expect(s == R"("a")"); // std::to_string(static_cast<int>('a')));
      }
      {
         std::string s;
         short v{1};
         expect(not glz::write_json(v, s));
         expect(s == "1");
      }
      {
         std::string s;
         int v{1};
         expect(not glz::write_json(v, s));
         expect(s == "1");
      }
      {
         std::string s;
         long v{1};
         expect(not glz::write_json(v, s));
         expect(s == "1");
      }
      {
         std::string s;
         long long v{-193582804324766};
         expect(not glz::write_json(v, s));
         expect(s == "-193582804324766");
      }
      {
         std::string s;
         unsigned short v{1};
         expect(not glz::write_json(v, s));
         expect(s == "1");
      }
      {
         std::string s;
         unsigned int v{1};
         expect(not glz::write_json(v, s));
         expect(s == "1");
      }
      {
         std::string s;
         unsigned long v{1};
         expect(not glz::write_json(v, s));
         expect(s == "1");
      }
      {
         std::string s;
         unsigned long long v{193582804324766};
         expect(not glz::write_json(v, s));
         expect(s == "193582804324766");
      }
   };

   "Write variant"_test = [] {
      std::variant<int, double, Geodetic> var;

      var = 1;
      std::string ibuf{};
      expect(not glz::write_json(var, ibuf));
      expect(ibuf == R"(1)");

      var = 2.2;
      std::string dbuf{};
      expect(not glz::write_json(var, dbuf));
      expect(dbuf == R"(2.2)");

      var = Geodetic{1.0, 2.0, 5.0};
      std::string gbuf{};
      expect(not glz::write_json(var, gbuf));
      expect(gbuf == R"([1,2,5])") << gbuf;
   };

   "Write empty array structure"_test = [] {
      EmptyArray e;
      std::string buf;
      expect(not glz::write_json(e, buf));
      expect(buf == R"([])");
   };

   "Read empty array structure"_test = [] {
      EmptyArray e;
      expect(glz::read_json(e, "[]") == glz::error_code::none);
      expect(glz::read_json(e, " [   ] ") == glz::error_code::none);
      expect(glz::read_json(e, "[1,2,3]") == glz::error_code::expected_bracket);
   };

   //* Empty object not allowed
   "Write empty object structure"_test = [] {
      EmptyObject e;
      std::string buf;
      expect(not glz::write_json(e, buf));
      expect(buf == R"({})");
   };

   "Read empty object structure"_test = [] {
      EmptyObject e;
      static_assert(glz::glaze_object_t<EmptyObject>);
      expect(glz::read_json(e, "{}") == glz::error_code::none);
      expect(glz::read_json(e, " {    } ") == glz::error_code::none);
      expect(glz::read_json(e, "{ \"reject\": 44 }") == glz::error_code::unknown_key);
      expect(glz::read<glz::opts{.error_on_unknown_keys = false}>(e, "{ \"skipped\": 44 }") == glz::error_code::none);
   };

   "Write c-string"_test = [] {
      const char* const c = "aasdf";
      std::string buf;
      expect(not glz::write_json(c, buf));
      expect(buf == R"("aasdf")");
   };

   "Write constant double"_test = [] {
      const double d = 6.125;
      std::string buf;
      expect(not glz::write_json(d, buf));
      expect(buf == R"(6.125)");
   };

   "Write constant bool"_test = [] {
      const bool b = true;
      std::string buf;
      expect(not glz::write_json(b, buf));
      expect(buf == R"(true)");
   };

   "Write constant int"_test = [] {
      const int i = 505;
      std::string buf;
      expect(not glz::write_json(i, buf));
      expect(buf == R"(505)");
   };

   "Write vector"_test = [] {
      {
         std::vector<double> v{1.1, 2.2, 3.3, 4.4};
         std::string s;
         expect(not glz::write_json(v, s));

         expect(s == "[1.1,2.2,3.3,4.4]");
      }
      {
         std::vector<bool> v{true, false, true, false};
         std::string s;
         expect(not glz::write_json(v, s));

         expect(s == "[true,false,true,false]");
      }
   };

   "Write list"_test = [] {
      std::string in, inr = "[1,2,3,4]";
      std::list<int> l{1, 2, 3, 4};
      expect(not glz::write_json(l, in));

      expect(in == inr);
   };

   "Write forward list"_test = [] {
      std::string in, inr = "[1,2,3,4]";
      std::forward_list<int> l{1, 2, 3, 4};
      expect(not glz::write_json(l, in));

      expect(in == inr);
   };

   "Write deque"_test = [] {
      std::string in, inr = "[1,2,3,4]";
      std::deque<int> l{1, 2, 3, 4};
      expect(not glz::write_json(l, in));

      expect(in == inr);
   };

   "Write array"_test = [] {
      std::string s;
      std::array<double, 4> v{1.1, 2.2, 3.3, 4.4};
      expect(not glz::write_json(v, s));
      expect(s == "[1.1,2.2,3.3,4.4]");
   };

   "Write array-like input range"_test = [] {
#ifdef __cpp_lib_ranges
      "sized range"_test = [] { expect(glz::write_json(std::views::iota(0, 3)) == glz::sv{R"([0,1,2])"}); };

      "unsized range"_test = [] {
         auto unsized_range = std::views::iota(0, 5) //
                              | std::views::filter([](const auto i) { return i % 2 == 0; });
         static_assert(!std::ranges::sized_range<decltype(unsized_range)>);
         expect(glz::write_json(unsized_range) == glz::sv{"[0,2,4]"});
      };

      "uncommon range"_test = [] {
         auto uncommon_range = std::views::iota(0) //
                               | std::views::take(5) //
                               | std::views::filter([](const auto i) { return i % 2 == 0; });
         static_assert(!std::ranges::common_range<decltype(uncommon_range)>);
         expect(glz::write_json(uncommon_range) == glz::sv{"[0,2,4]"});
      };
#endif

      "initializer list"_test = [] {
         auto init_list = {0, 1, 2};
         expect(glz::write_json(init_list) == glz::sv{R"([0,1,2])"});
      };
   };

   "Write map"_test = [] {
      std::string s;
      const std::map<std::string, double> m{{"a", 2.2}, {"b", 11.111}, {"c", 211.2}};
      expect(not glz::write_json(m, s));
      expect(s == R"({"a":2.2,"b":11.111,"c":211.2})");

      const std::map<std::string, std::optional<double>> nullable{
         {"a", std::nullopt}, {"b", 13.4}, {"c", std::nullopt}, {"d", 211.2}, {"e", std::nullopt}};
      expect(not glz::write_json(nullable, s));
      expect(s == glz::sv{R"({"b":13.4,"d":211.2})"});
   };

   "Write pair"_test = [] {
      auto tester = [](const auto& test_case) {
         const std::pair value{test_case.key, test_case.value};
         expect(glz::write_json(value) == test_case.expected_json);
      };

      std::tuple tests{
         Write_pair_test_case{"key", "value", R"({"key":"value"})"},
         Write_pair_test_case{0, "value", R"({"0":"value"})"},
         Write_pair_test_case{0.78, std::array{1, 2, 3}, R"({"0.78":[1,2,3]})"},
         Write_pair_test_case{"k", glz::obj{"in1", 1, "in2", "v"}, R"({"k":{"in1":1,"in2":"v"}})"},
         Write_pair_test_case{std::array{1, 2}, 99, R"({"[1,2]":99})"},
         Write_pair_test_case{std::array{"one", "two"}, 99, R"({"[\"one\",\"two\"]":99})"},
         Write_pair_test_case{"knot", std::nullopt, "{}"}, // nullopt_t, not std::optional
         Write_pair_test_case{"kmaybe", std::optional<int>{}, "{}"},
      };

      glz::for_each_apply(tester, tests);
   };

#ifdef __cpp_lib_ranges
   "Write map-like input range"_test = [] {
      "input range of pairs"_test = [] {
         auto num_view =
            std::views::iota(-2, 3) | std::views::transform([](const auto i) { return std::pair(i, i * i); });
         expect(glz::write_json(num_view) == glz::sv{R"({"-2":4,"-1":1,"0":0,"1":1,"2":4})"});

         auto str_view = std::views::iota(-2, 3) |
                         std::views::transform([](const auto i) { return std::pair(i, std::to_string(i * i)); });
         expect(glz::write_json(str_view) == glz::sv{R"({"-2":"4","-1":"1","0":"0","1":"1","2":"4"})"});
      };

      "unsized range of pairs"_test = [] {
         auto base_rng = std::views::iota(-2, 3) | std::views::filter([](const auto i) { return i < 0; });

         auto num_view = base_rng | std::views::transform([](const auto i) { return std::pair(i, i * i); });
         static_assert(!std::ranges::sized_range<decltype(num_view)>);
         expect(glz::write_json(num_view) == glz::sv{R"({"-2":4,"-1":1})"});

         auto str_view =
            base_rng | std::views::transform([](const auto i) { return std::pair(i, std::to_string(i * i)); });
         static_assert(!std::ranges::sized_range<decltype(str_view)>);
         expect(glz::write_json(str_view) == glz::sv{R"({"-2":"4","-1":"1"})"});
      };

      "initializer list w/ ranges"_test = [] {
         auto remap_user_port = [](const auto port) { return port + 1024; };
         auto user_ports = {std::pair("tcp", std::views::iota(80, 83) | std::views::transform(remap_user_port)),
                            std::pair("udp", std::views::iota(21, 25) | std::views::transform(remap_user_port))};
         expect(glz::write_json(user_ports) == glz::sv{R"({"tcp":[1104,1105,1106],"udp":[1045,1046,1047,1048]})"});
      };

      "single pair view"_test = [] {
         const auto single_pair = std::ranges::single_view{std::pair{false, true}};
         expect(glz::write_json(single_pair) == glz::sv{R"({"false":true})"});
      };
   };
#endif

   "Write integer map"_test = [] {
      std::map<int, double> m{{3, 2.2}, {5, 211.2}, {7, 11.111}};
      std::string s;
      expect(not glz::write_json(m, s));

      expect(s == R"({"3":2.2,"5":211.2,"7":11.111})");
   };

   "Write object"_test = [] {
      ThreeODetic t{};

      std::string s;
      s.reserve(1000);
      expect(not glz::write_json(t, s));
      expect(s == R"(["geo",[0,0,0],"int",0])") << s;

      Named n{"Hello, world!", {{{21, 15, 13}, 0}, {0}}};
      expect(not glz::write_json(n, s));

      expect(s == R"({"name":"Hello, world!","value":[["geo",[21,15,13],"int",0],[0,0,0]]})") << s;
   };

   "Write boolean"_test = [] {
      {
         bool b = true;
         std::string s;
         expect(not glz::write_json(b, s));

         expect(s == R"(true)");
      }
      {
         bool b = false;
         std::string s;
         expect(not glz::write_json(b, s));

         expect(s == R"(false)");
      }
   };

   "Hello World"_test = [] {
      std::unordered_map<std::string, std::string> m;
      m["Hello"] = "World";
      std::string buf;
      expect(not glz::write_json(m, buf));

      expect(buf == R"({"Hello":"World"})");
   };

   "Number"_test = [] {
      std::unordered_map<std::string, double> x;
      x["number"] = 5.55;

      std::string jx;
      expect(not glz::write_json(x, jx));

      expect(jx == R"({"number":5.55})");
   };

   "Nested array"_test = [] {
      std::vector<Geodetic> v(2);
      std::string buf;

      expect(not glz::write_json(v, buf));
      expect(buf == R"([[0,0,0],[0,0,0]])");
   };

   "Nested map"_test = [] {
      std::map<std::string, Geodetic> m;
      m["1"];
      m["2"];
      std::string buf;

      expect(not glz::write_json(m, buf));
      expect(buf == R"({"1":[0,0,0],"2":[0,0,0]})");
   };

   "Nested map 2"_test = [] {
      std::map<std::string, std::vector<double>> m;
      m["1"] = {4.0, 0.0, 0.0};
      m["2"] = {5.0, 0.0, 0.0, 4.0};
      std::string buf;

      expect(not glz::write_json(m, buf));
      expect(buf == R"({"1":[4,0,0],"2":[5,0,0,4]})");
   };
};

struct error_comma_obj
{
   struct error_comma_data
   {
      std::string instId;
   };

   std::string code;
   std::string msg;
   std::vector<error_comma_data> data;
};

suite error_outputs = [] {
   "invalid character"_test = [] {
      std::string s = R"({"Hello":"World"x, "color": "red"})";
      std::map<std::string, std::string> m;
      auto pe = glz::read_json(m, s);
      expect(pe != glz::error_code::none);
      auto err = glz::format_error(pe, s);
      expect(err == "1:17: expected_comma\n   {\"Hello\":\"World\"x, \"color\": \"red\"}\n                   ^") << err;
   };

   "invalid character with tabs in json"_test = [] {
      std::string s = R"({"Hello":	"World"x, "color": 	"red"})";
      std::map<std::string, std::string> m;
      auto pe = glz::read_json(m, s);
      expect(pe != glz::error_code::none);
      auto err = glz::format_error(pe, s);
      expect(err == "1:18: expected_comma\n   {\"Hello\": \"World\"x, \"color\":  \"red\"}\n                    ^")
         << err;
   };

   "extra comma"_test = [] {
      std::string s = R"({
      "code": "0",
      "msg": "",
      "data": [ {
          "instId": "USDT"
        },
        {
          "instId": "BTC"
        },
     ]
  })";
      auto ex = glz::read_json<error_comma_obj>(s);
      expect(!ex.has_value());
      auto err = glz::format_error(ex.error(), s);
      expect(err == "10:6: expected_brace\n        ]\n        ^") << err;
   };
};

struct study_obj
{
   size_t x{};
   size_t y{};
};

template <>
struct glz::meta<study_obj>
{
   static constexpr std::string_view name = "study_obj";
   using T = study_obj;
   static constexpr auto value = object("x", &T::x, "y", &T::y);
};

suite study_tests = [] {
   "study"_test = [] {
      glz::study::design design;
      design.params = {{.ptr = "/x", .distribution = "linspace", .range = {"0", "1", "10"}}};

      glz::study::full_factorial generator{study_obj{}, design};

      std::vector<size_t> results;
      std::mutex mtx{};
      glz::study::run_study(generator, [&](const auto& point, [[maybe_unused]] const auto job_num) {
         std::unique_lock lock{mtx};
         results.emplace_back(point.value().x);
      });

      std::sort(results.begin(), results.end());

      expect(results[0] == 0);
      expect(results[10] == 10);
   };

   "doe"_test = [] {
      glz::study::design design;
      design.params = {{"/x", "linspace", {"0", "1", "3"}}, {"/y", "linspace", {"0", "1", "2"}}};

      glz::study::full_factorial g{study_obj{}, design};

      std::vector<std::string> results;
      for (size_t i = 0; i < g.size(); ++i) {
         const auto point = g.generate(i).value();
         results.emplace_back(std::to_string(point.x) + "|" + std::to_string(point.y));
      }

      std::sort(results.begin(), results.end());

      std::vector<std::string> results2;
      std::mutex mtx{};
      glz::study::run_study(g, [&](const auto& point, [[maybe_unused]] const auto job_num) {
         std::unique_lock lock{mtx};
         results2.emplace_back(std::to_string(point.value().x) + "|" + std::to_string(point.value().y));
      });

      std::sort(results2.begin(), results2.end());

      expect(results == results2);
   };
};

suite thread_pool = [] {
   "thread pool"_test = [] {
      glz::pool pool(2);

      std::atomic<int> x = 0;

      auto f = [&](auto) { ++x; };

      for (auto i = 0; i < 1000; ++i) {
         pool.emplace_back(f);
      }

      pool.wait();

      expect(x == 1000);
   };

   "thread pool no thread number"_test = [] {
      glz::pool pool(4);

      std::atomic<int> x = 0;

      auto f = [&] { ++x; };

      for (auto i = 0; i < 1000; ++i) {
         pool.emplace_back(f);
      }

      pool.wait();

      expect(x == 1000);
   };

   "generate_random_numbers"_test = [] {
      glz::pool pool;

      auto f = [] {
         std::mt19937_64 generator{};
         std::uniform_int_distribution<size_t> dist{0, 100};

         return dist(generator);
      };

      std::vector<std::future<size_t>> numbers;

      for (auto i = 0; i < 1000; ++i) {
         numbers.emplace_back(pool.emplace_back(f));
      }

      pool.wait();

      expect(numbers.size() == 1000);
   };
};

struct local_meta
{
   double x{};
   int y{};

   struct glaze
   {
      static constexpr std::string_view name = "local_meta";
      using T = local_meta;
      static constexpr auto value = glz::object("x", &T::x, //
                                                "y", &T::y);
   };
};

static_assert(glz::glaze_t<local_meta>);
static_assert(glz::glaze_object_t<local_meta>);
static_assert(glz::local_meta_t<local_meta>);

suite local_meta_tests = [] {
   "local_meta"_test = [] {
      std::string out;
      local_meta m{};
      expect(not glz::write_json(m, out));
      expect(out == R"({"x":0,"y":0})");
      expect(glz::named<local_meta> == true);
      expect(glz::name_v<local_meta> == "local_meta");
   };
};

suite raw_json_tests = [] {
   "round_trip_raw_json"_test = [] {
      std::vector<glz::raw_json> v{"0", "1", "2"};
      std::string s;
      expect(not glz::write_json(v, s));
      expect(s == R"([0,1,2])");
      expect(not glz::read_json(v, s));
   };
   "raw_json_view_read"_test = [] {
      std::vector<glz::raw_json_view> v{};
      std::string s = R"([0,1,2])";
      expect(not glz::read_json(v, s));
      expect(v[0].str == "0");
      expect(v[1].str == "1");
      expect(v[2].str == "2");
   };
   "glz::raw_json"_test = [] {
      glz::raw_json v{};
      std::string s = R"(12345678)";
      expect(not glz::read_json(v, s));
      expect(v.str == "12345678");
   };
};

// Test struct for raw_json whitespace issue (GitHub issue)
struct Properties
{
   std::string type;
   std::map<glz::sv, glz::raw_json> properties;
};

template <>
struct glz::meta<Properties>
{
   using T = Properties;
   static constexpr auto value = object("type", &T::type);

   static constexpr auto unknown_write{&T::properties};
   static constexpr auto unknown_read{&T::properties};
};

// Same struct but with generic for comparison
struct PropertiesJsonT
{
   std::string type;
   std::map<glz::sv, glz::generic> properties;
};

template <>
struct glz::meta<PropertiesJsonT>
{
   using T = PropertiesJsonT;
   static constexpr auto value = object("type", &T::type);

   static constexpr auto unknown_write{&T::properties};
   static constexpr auto unknown_read{&T::properties};
};

suite raw_json_whitespace_tests = [] {
   "raw_json_unknown_keys_with_whitespace"_test = [] {
      // Test raw_json with formatted JSON content
      std::string input_json = R"({
                "type": "mytype",
                "ident": {
                    "id": "aaa77fd3-2df3-4366-ae08-183b6233cefd"
                }
            })";

      Properties props;
      glz::context ctx;
      auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(props, input_json, ctx);
      expect(not ec) << glz::format_error(ec, input_json);

      // Verify the data was read correctly
      expect(props.type == "mytype");
      expect(props.properties.size() == 1);
      expect(props.properties.contains("ident"));

      // Verify the raw_json content contains the formatted JSON
      auto& ident_raw = props.properties["ident"];
      expect(ident_raw.str.contains("aaa77fd3-2df3-4366-ae08-183b6233cefd"));
      expect(ident_raw.str.contains("id"));

      // Test writing back - this should not truncate
      std::string output_json;
      ec = glz::write_json(props, output_json);
      expect(not ec);

      // Test prettification - the main issue from the bug report
      std::string prettified = glz::prettify_json(output_json);
      expect(prettified.contains("mytype"));
      expect(prettified.contains("ident"));
      expect(prettified.contains("aaa77fd3-2df3-4366-ae08-183b6233cefd"));
      expect(prettified.contains("id"));

      // Ensure prettified output is not truncated (should end with closing brace)
      expect(prettified.back() == '}');

      // The prettified output should be parseable back
      Properties props2;
      ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(props2, prettified);
      expect(not ec) << "Prettified output should be valid JSON";
      expect(props2.type == "mytype");
      expect(props2.properties.size() == 1);
   };

   "raw_json_vs_generic_comparison"_test = [] {
      // Test that both raw_json and generic work correctly with formatted content
      std::string input_json = R"({
                "type": "mytype", 
                "formatted_field": {
                    "nested": {
                        "value": "test"
                    },
                    "array": [1, 2, 3]
                }
            })";

      // Test with raw_json
      Properties props_raw;
      auto ec1 = glz::read<glz::opts{.error_on_unknown_keys = false}>(props_raw, input_json);
      expect(not ec1);

      std::string output_raw;
      ec1 = glz::write_json(props_raw, output_raw);
      expect(not ec1);

      std::string prettified_raw = glz::prettify_json(output_raw);
      expect(prettified_raw.contains("formatted_field"));
      expect(prettified_raw.contains("nested"));
      expect(prettified_raw.back() == '}'); // Should not be truncated

      // Test with generic
      PropertiesJsonT props_jsonf;
      auto ec2 = glz::read<glz::opts{.error_on_unknown_keys = false}>(props_jsonf, input_json);
      expect(not ec2);

      std::string output_jsonf;
      ec2 = glz::write_json(props_jsonf, output_jsonf);
      expect(not ec2);

      std::string prettified_jsonf = glz::prettify_json(output_jsonf);
      expect(prettified_jsonf.contains("formatted_field"));
      expect(prettified_jsonf.contains("nested"));
      expect(prettified_jsonf.back() == '}'); // Should not be truncated

      // Both should produce valid JSON that can be read back
      Properties test_raw;
      PropertiesJsonT test_jsonf;
      expect(not glz::read<glz::opts{.error_on_unknown_keys = false}>(test_raw, prettified_raw));
      expect(not glz::read<glz::opts{.error_on_unknown_keys = false}>(test_jsonf, prettified_jsonf));
   };

   "raw_json_minify_with_whitespace"_test = [] {
      // Test that minification also works correctly with raw_json containing whitespace
      std::string input_json = R"({
                "type": "test",
                "data": {
                    "formatted": "content",
                    "more": {
                        "nested": "values"
                    }
                }
            })";

      Properties props;
      auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(props, input_json);
      expect(not ec);

      std::string output_json;
      ec = glz::write_json(props, output_json);
      expect(not ec);

      // Test minification
      std::string minified = glz::minify_json(output_json);
      expect(minified.contains("test"));
      expect(minified.contains("data"));
      expect(minified.contains("formatted"));
      expect(minified.contains("nested"));

      // Minified should be valid JSON
      Properties props_minified;
      ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(props_minified, minified);
      expect(not ec) << "Minified output should be valid JSON";
      expect(props_minified.type == "test");
   };

   "raw_json_whitespace_edge_cases"_test = [] {
      // Test various whitespace scenarios that could cause issues
      std::string input_with_tabs = R"({
	"type": "tab_test",
	"tab_content": {
		"indented": "with_tabs"
	}
})";

      Properties props_tabs;
      auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(props_tabs, input_with_tabs);
      expect(not ec);

      std::string output_tabs;
      expect(not glz::write_json(props_tabs, output_tabs));

      std::string prettified_tabs = glz::prettify_json(output_tabs);
      expect(prettified_tabs.contains("tab_content"));
      expect(prettified_tabs.back() == '}');

      // Test with mixed whitespace
      std::string input_mixed = "{\r\n    \"type\": \"mixed\",\n\t\"field\": {\r\n        \"value\": 42\r\n    }\r\n}";

      Properties props_mixed;
      ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(props_mixed, input_mixed);
      expect(not ec);

      std::string output_mixed;
      expect(not glz::write_json(props_mixed, output_mixed));

      std::string prettified_mixed = glz::prettify_json(output_mixed);
      expect(prettified_mixed.contains("field"));
      expect(prettified_mixed.back() == '}');
   };
};

suite json_helpers = [] {
   "json_helpers"_test = [] {
      my_struct v{};
      auto json = glz::write_json(v).value_or("error");
      expect(json == R"({"i":287,"d":3.14,"hello":"Hello World","arr":[1,2,3]})");

      v = glz::read_json<my_struct>(json).value();
   };
};

suite allocated_write = [] {
   "allocated_write"_test = [] {
      my_struct v{};
      std::string s{};
      s.resize(100);
      auto length = glz::write_json(v, s.data()).value();
      s.resize(length);
      expect(s == R"({"i":287,"d":3.14,"hello":"Hello World","arr":[1,2,3]})");
   };
};

suite nan_tests = [] {
   "nan_write_tests"_test = [] {
      double d = NAN;
      std::string s{};
      expect(not glz::write_json(d, s));
      expect(s == "null");

      d = 0.0;
      expect(glz::read_json(d, s));
   };

   "nan_read_tests"_test = [] {
      double d = 0.0;
      std::string s = "null";
      expect(glz::read_json(d, s));

      d = 0.0;
      s = "NaN";
      expect(glz::read_json(d, s));

      d = 0.0;
      s = "nan";
      expect(glz::read_json(d, s));

      std::array<double, 5> d_array{};
      s = "[null, nan, NaN, -nan, 3.14]";
      expect(glz::read_json(d_array, s));
   };
};

struct put_action
{
   std::map<std::string, int> data{};
   [[nodiscard]] bool operator==(const put_action&) const = default;
};

template <>
struct glz::meta<put_action>
{
   using T = put_action;
   static constexpr std::string_view name = "put_action";
   static constexpr auto value = object("data", &T::data);
};

struct delete_action
{
   std::string data{};
   [[nodiscard]] bool operator==(const delete_action&) const = default;
};

template <>
struct glz::meta<delete_action>
{
   using T = delete_action;
   static constexpr std::string_view name = "delete_action";
   static constexpr auto value = object("data", &T::data);
};

using tagged_variant = std::variant<put_action, delete_action>;

template <>
struct glz::meta<tagged_variant>
{
   static constexpr std::string_view tag = "action";
   static constexpr auto ids = std::array{"PUT", "DELETE"}; // Defaults to glz::name_v of the type
};

// Test automatic ids
using tagged_variant2 = std::variant<put_action, delete_action, std::monostate>;
template <>
struct glz::meta<tagged_variant2>
{
   static constexpr std::string_view tag = "type";
   // ids defaults to glz::name_v of the type
};

// Test array based variant (experimental, not meant for external usage since api might change)
using num_variant = std::variant<double, int32_t, uint64_t, int8_t, float>;
struct holds_some_num
{
   num_variant num{};
};
template <>
struct glz::meta<holds_some_num>
{
   using T = holds_some_num;
   static constexpr auto value = object("num", glz::array_variant{&T::num});
};

struct OptionA
{
   std::string tag{};
   int a{};
};

struct OptionB
{
   std::string tag{};
   int a{};
};

using TaggedObject = std::variant<OptionA, OptionB>;

template <>
struct glz::meta<TaggedObject>
{
   static constexpr std::string_view tag = "tag";
   static constexpr auto ids = std::array{"A", "B"};
};

suite tagged_variant_tests = [] {
   "TaggedObject"_test = [] {
      TaggedObject content;
      std::string data = R"({ "tag": "A", "a": 2 })";
      expect(!glz::read_json<TaggedObject>(content, data));
      expect(std::get<OptionA>(content).a == 2);
   };

   "tagged_variant_read_tests"_test = [] {
      tagged_variant var{};
      expect(glz::read_json(var, R"({"action":"DELETE","data":"the_internet"})") == glz::error_code::none);
      expect(std::holds_alternative<delete_action>(var));
      expect(std::get<delete_action>(var).data == "the_internet");

      // tag at end
      expect(glz::read_json(var, R"({"data":"the_internet","action":"DELETE"})") == glz::error_code::none);
      expect(std::holds_alternative<delete_action>(var));
      expect(std::get<delete_action>(var).data == "the_internet");

      tagged_variant2 var2{};
      expect(glz::read_json(var2, R"({"type":"put_action","data":{"x":100,"y":200}})") == glz::error_code::none);
      expect(std::holds_alternative<put_action>(var2));
      expect(std::get<put_action>(var2).data["x"] == 100);
      expect(std::get<put_action>(var2).data["y"] == 200);

      // tag at end
      expect(glz::read_json(var2, R"({"data":{"x":100,"y":200},"type":"put_action"})") == glz::error_code::none);
      expect(std::holds_alternative<put_action>(var2));
      expect(std::get<put_action>(var2).data["x"] == 100);
      expect(std::get<put_action>(var2).data["y"] == 200);

      //
      const auto err = glz::read<glz::opts{.error_on_unknown_keys = false}>(
         var2, R"({"type":"put_action","data":{"x":100,"y":200}})");
      expect(err == glz::error_code::none);
      expect(std::holds_alternative<put_action>(var2));
      expect(std::get<put_action>(var2).data["x"] == 100);
      expect(std::get<put_action>(var2).data["y"] == 200);
   };

   "tagged_variant_write_tests"_test = [] {
      // custom tagged discriminator ids
      tagged_variant var = delete_action{{"the_internet"}};
      std::string s{};
      expect(not glz::write_json(var, s));
      expect(s == R"({"action":"DELETE","data":"the_internet"})");
      s.clear();

      // Automatic tagged discriminator ids
      tagged_variant2 var2 = put_action{{{"x", 100}, {"y", 200}}};
      expect(not glz::write_json(var2, s));
      expect(s == R"({"type":"put_action","data":{"x":100,"y":200}})");
      s.clear();

      // prettifies valid JSON
      expect(not glz::write<glz::opts{.prettify = true}>(var, s));
      tagged_variant parsed_var;
      expect(glz::read_json(parsed_var, s) == glz::error_code::none);
      expect(parsed_var == var);
   };

   "tagged_variant_schema_tests"_test = [] {
      auto s = glz::write_json_schema<tagged_variant>().value_or("error");
      expect(
         s ==
         R"({"type":["object"],"$defs":{"int32_t":{"type":["integer"],"minimum":-2147483648,"maximum":2147483647},"std::map<std::string,int32_t>":{"type":["object"],"additionalProperties":{"$ref":"#/$defs/int32_t"}},"std::string":{"type":["string"]}},"oneOf":[{"type":["object"],"properties":{"action":{"const":"PUT"},"data":{"$ref":"#/$defs/std::map<std::string,int32_t>"}},"additionalProperties":false,"required":["action"],"title":"PUT"},{"type":["object"],"properties":{"action":{"const":"DELETE"},"data":{"$ref":"#/$defs/std::string"}},"additionalProperties":false,"required":["action"],"title":"DELETE"}],"title":"std::variant<put_action, delete_action>"})")
         << s;
   };

   "array_variant_tests"_test = [] {
      // Test array based variant (experimental, not meant for external usage since api might change)

      holds_some_num obj{};
      auto ec = glz::read_json(obj, R"({"num":["float", 3.14]})");
      std::string b = R"({"num":["float", 3.14]})";
      expect(ec == glz::error_code::none) << glz::format_error(ec, b);
      expect(std::get<float>(obj.num) == 3.14f);
      expect(not glz::read_json(obj, R"({"num":["uint64_t", 5]})"));
      expect(std::get<uint64_t>(obj.num) == 5);
      expect(not glz::read_json(obj, R"({"num":["int8_t", -3]})"));
      expect(std::get<int8_t>(obj.num) == -3);
      expect(not glz::read_json(obj, R"({"num":["int32_t", -2]})"));
      expect(std::get<int32_t>(obj.num) == -2);

      obj.num = 5.0;
      std::string s{};
      expect(not glz::write_json(obj, s));
      expect(s == R"({"num":["double",5]})");
      obj.num = uint64_t{3};
      expect(not glz::write_json(obj, s));
      expect(s == R"({"num":["uint64_t",3]})");
      obj.num = int8_t{-5};
      expect(not glz::write_json(obj, s));
      expect(s == R"({"num":["int8_t",-5]})");
   };

   "shared_ptr variant schema"_test = [] {
      const auto schema = glz::write_json_schema<std::shared_ptr<tagged_variant2>>().value_or("error");
      expect(
         schema ==
         R"({"type":["object","null"],"$defs":{"int32_t":{"type":["integer"],"minimum":-2147483648,"maximum":2147483647},"std::map<std::string,int32_t>":{"type":["object"],"additionalProperties":{"$ref":"#/$defs/int32_t"}},"std::string":{"type":["string"]}},"oneOf":[{"type":["object"],"properties":{"data":{"$ref":"#/$defs/std::map<std::string,int32_t>"},"type":{"const":"put_action"}},"additionalProperties":false,"required":["type"],"title":"put_action"},{"type":["object"],"properties":{"data":{"$ref":"#/$defs/std::string"},"type":{"const":"delete_action"}},"additionalProperties":false,"required":["type"],"title":"delete_action"},{"type":["null"],"title":"std::monostate","const":null}],"title":"std::shared_ptr<std::variant<put_action, delete_action, std::monostate>>"})")
         << schema;
   };
};

struct variant_obj
{
   std::variant<double, std::string> v{};
};

template <>
struct glz::meta<variant_obj>
{
   static constexpr std::string_view name = "variant_obj";
   using T = variant_obj;
   static constexpr auto value = object("v", &T::v);
};

struct var_a1
{
   int i{};
};

struct var_a2
{
   double i{};
};

suite variant_tests = [] {
   "variant_write_tests"_test = [] {
      std::variant<double, std::string> d = "not_a_fish";
      std::string s{};

      expect(not glz::write_json(d, s));
      expect(s == R"("not_a_fish")");

      d = 5.7;
      s.clear();
      expect(not glz::write_json(d, s));
      expect(s == "5.7");

      std::variant<std::monostate, int, std::string> m{};
      expect(not glz::write_json(m, s));
      expect(s == "null") << s;
   };

   "variant_read_"_test = [] {
      std::variant<int32_t, double> x = 44;
      expect(glz::read_json(x, "33") == glz::error_code::none);
      expect(std::get<int32_t>(x) == 33);
   };

   // TODO: Make reading into the active element work here
   /*"variant read active"_test = [] {
      std::variant<var_a1, var_a2> v = var_a2{};
      std::string json = R"({"i":6})";
      expect(not glz::read_json(v, json));
      expect(v.index() == 1);
      expect(std::get<var_a2>(v).i == 6);
   };*/

   "variant_read_auto"_test = [] {
      // Auto deduce variant with no conflicting basic types
      std::variant<std::monostate, int, std::string, bool, std::map<std::string, double>, std::vector<std::string>> m{};
      expect(glz::read_json(m, R"("Hello World")") == glz::error_code::none);
      expect[std::holds_alternative<std::string>(m)];
      expect(std::get<std::string>(m) == "Hello World");

      expect(glz::read_json(m, R"(872)") == glz::error_code::none);
      expect[std::holds_alternative<int>(m)];
      expect(std::get<int>(m) == 872);

      expect(glz::read_json(m, R"({"pi":3.14})") == glz::error_code::none);
      expect[std::holds_alternative<std::map<std::string, double>>(m)];
      expect(std::get<std::map<std::string, double>>(m)["pi"] == 3.14);

      expect(glz::read_json(m, R"(true)") == glz::error_code::none);
      expect[std::holds_alternative<bool>(m)];
      expect(std::get<bool>(m) == true);

      expect(glz::read_json(m, R"(["a", "b", "c"])") == glz::error_code::none);
      expect[std::holds_alternative<std::vector<std::string>>(m)];
      expect(std::get<std::vector<std::string>>(m)[1] == "b");

      expect(glz::read_json(m, "null") == glz::error_code::none);
      expect[std::holds_alternative<std::monostate>(m)];
   };

   "variant_read_obj"_test = [] {
      variant_obj obj{};

      obj.v = double{};
      expect(glz::read_json(obj, R"({"v": 5.5})") == glz::error_code::none);

      expect(std::get<double>(obj.v) == 5.5);
   };

   "variant_request"_test = [] {
      std::map<std::string, std::variant<std::string, int, bool>> request;

      request["username"] = "paulo";
      request["password"] = "123456";
      request["remember"] = true;

      auto str = glz::write_json(request).value_or("error");

      expect(str == R"({"password":"123456","remember":true,"username":"paulo"})") << str;
   };

   "variant write/read enum"_test = [] {
      std::variant<Color, std::uint16_t> var{Color::Red};
      auto res{glz::write_json(var).value_or("error")};
      expect(res == "\"Red\"") << res;
      auto read{glz::read_json<std::variant<Color, std::uint16_t>>(res)};
      expect(read.has_value());
      expect(std::holds_alternative<Color>(read.value()));
      expect(std::get<Color>(read.value()) == Color::Red);
   };

   "variant read tuple"_test = [] {
      using int_int_tuple_t = std::tuple<int, int>;
      std::variant<int, int_int_tuple_t, std::string> var;

      expect(glz::read_json(var, R"(1)") == glz::error_code::none);
      expect(std::get<int>(var) == 1);

      expect(glz::read_json(var, R"("str")") == glz::error_code::none);
      expect(std::get<std::string>(var) == "str");

      expect(glz::read_json(var, R"([2, 3])") == glz::error_code::none);
      expect(std::get<int_int_tuple_t>(var) == int_int_tuple_t{2, 3});
   };
};

// Tests for std::vector<std::variant<...>> with purely reflected structs
struct reflected_person
{
   std::string name{};
   int age{};
   double height{};
};

struct reflected_animal
{
   std::string species{};
   std::string name{};
   int weight{};
};

struct reflected_vehicle
{
   std::string make{};
   std::string model{};
   int year{};
   double price{};
};

struct reflected_book
{
   std::string title{};
   std::string author{};
   int pages{};
   std::string isbn{};
};

static_assert(glz::reflectable<reflected_person>);
static_assert(glz::reflectable<reflected_animal>);
static_assert(glz::reflectable<reflected_vehicle>);
static_assert(glz::reflectable<reflected_book>);

suite vector_variant_reflection_tests = [] {
   "vector of variant with two reflected structs"_test = [] {
      using entity_variant = std::variant<reflected_person, reflected_animal>;
      std::vector<entity_variant> entities;

      entities.push_back(reflected_person{"Alice", 30, 165.5});
      entities.push_back(reflected_animal{"Dog", "Buddy", 25});
      entities.push_back(reflected_person{"Bob", 25, 180.0});
      entities.push_back(reflected_animal{"Cat", "Whiskers", 4});

      std::string json;
      expect(!glz::write_json(entities, json));

      std::vector<entity_variant> read_entities;
      expect(glz::read_json(read_entities, json) == glz::error_code::none);

      expect(read_entities.size() == 4);

      expect(std::holds_alternative<reflected_person>(read_entities[0]));
      auto& p1 = std::get<reflected_person>(read_entities[0]);
      expect(p1.name == "Alice");
      expect(p1.age == 30);
      expect(p1.height == 165.5);

      expect(std::holds_alternative<reflected_animal>(read_entities[1]));
      auto& a1 = std::get<reflected_animal>(read_entities[1]);
      expect(a1.species == "Dog");
      expect(a1.name == "Buddy");
      expect(a1.weight == 25);

      expect(std::holds_alternative<reflected_person>(read_entities[2]));
      auto& p2 = std::get<reflected_person>(read_entities[2]);
      expect(p2.name == "Bob");
      expect(p2.age == 25);
      expect(p2.height == 180.0);

      expect(std::holds_alternative<reflected_animal>(read_entities[3]));
      auto& a2 = std::get<reflected_animal>(read_entities[3]);
      expect(a2.species == "Cat");
      expect(a2.name == "Whiskers");
      expect(a2.weight == 4);
   };

   "vector of variant with three reflected structs"_test = [] {
      using item_variant = std::variant<reflected_person, reflected_vehicle, reflected_book>;
      std::vector<item_variant> items;

      items.push_back(reflected_person{"Charlie", 35, 175.0});
      items.push_back(reflected_vehicle{"Toyota", "Camry", 2022, 25000.0});
      items.push_back(reflected_book{"The Great Gatsby", "F. Scott Fitzgerald", 180, "978-0-7432-7356-5"});
      items.push_back(reflected_person{"Diana", 28, 160.0});

      std::string json;
      expect(!glz::write_json(items, json));

      std::vector<item_variant> read_items;
      expect(glz::read_json(read_items, json) == glz::error_code::none);

      expect(read_items.size() == 4);

      expect(std::holds_alternative<reflected_person>(read_items[0]));
      expect(std::holds_alternative<reflected_vehicle>(read_items[1]));
      expect(std::holds_alternative<reflected_book>(read_items[2]));
      expect(std::holds_alternative<reflected_person>(read_items[3]));

      auto& vehicle = std::get<reflected_vehicle>(read_items[1]);
      expect(vehicle.make == "Toyota");
      expect(vehicle.model == "Camry");
      expect(vehicle.year == 2022);
      expect(vehicle.price == 25000.0);

      auto& book = std::get<reflected_book>(read_items[2]);
      expect(book.title == "The Great Gatsby");
      expect(book.author == "F. Scott Fitzgerald");
      expect(book.pages == 180);
      expect(book.isbn == "978-0-7432-7356-5");
   };

   "empty vector of variant"_test = [] {
      using entity_variant = std::variant<reflected_person, reflected_animal>;
      std::vector<entity_variant> entities;

      std::string json;
      expect(!glz::write_json(entities, json));
      expect(json == "[]");

      std::vector<entity_variant> read_entities;
      expect(glz::read_json(read_entities, json) == glz::error_code::none);
      expect(read_entities.empty());
   };

   "vector with single variant element"_test = [] {
      using entity_variant = std::variant<reflected_person, reflected_animal>;
      std::vector<entity_variant> entities;

      entities.push_back(reflected_person{"Eve", 40, 170.0});

      std::string json;
      expect(!glz::write_json(entities, json));

      std::vector<entity_variant> read_entities;
      expect(glz::read_json(read_entities, json) == glz::error_code::none);

      expect(read_entities.size() == 1);
      expect(std::holds_alternative<reflected_person>(read_entities[0]));
      auto& person = std::get<reflected_person>(read_entities[0]);
      expect(person.name == "Eve");
      expect(person.age == 40);
      expect(person.height == 170.0);
   };

   "roundtrip with mixed types"_test = [] {
      using mixed_variant = std::variant<reflected_person, reflected_animal, reflected_vehicle, reflected_book>;
      std::vector<mixed_variant> original;

      original.push_back(reflected_book{"1984", "George Orwell", 328, "978-0-452-28423-4"});
      original.push_back(reflected_animal{"Horse", "Thunder", 500});
      original.push_back(reflected_vehicle{"Honda", "Accord", 2023, 27000.0});
      original.push_back(reflected_person{"Frank", 45, 185.0});
      original.push_back(reflected_book{"To Kill a Mockingbird", "Harper Lee", 281, "978-0-06-112008-4"});

      std::string json;
      expect(!glz::write_json(original, json));

      std::vector<mixed_variant> decoded;
      expect(glz::read_json(decoded, json) == glz::error_code::none);

      expect(decoded.size() == original.size());

      for (size_t i = 0; i < original.size(); ++i) {
         expect(original[i].index() == decoded[i].index());
      }
   };

   "prettified json output"_test = [] {
      using entity_variant = std::variant<reflected_person, reflected_animal>;
      std::vector<entity_variant> entities;

      entities.push_back(reflected_person{"Grace", 32, 168.0});
      entities.push_back(reflected_animal{"Bird", "Tweety", 1});

      std::string json;
      expect(!glz::write<glz::opts{.prettify = true}>(entities, json));

      expect(json.find("\n") != std::string::npos); // Should contain newlines
      expect(json.find("   ") != std::string::npos); // Should contain indentation

      std::vector<entity_variant> read_entities;
      expect(glz::read_json(read_entities, json) == glz::error_code::none);
      expect(read_entities.size() == 2);
   };

   "vector of variant with structs having overlapping field names"_test = [] {
      // Both structs have a 'name' field, but different other fields
      using ambiguous_variant = std::variant<reflected_person, reflected_animal>;
      std::vector<ambiguous_variant> items;

      // The variant should deduce the correct type based on all fields present
      items.push_back(reflected_person{"Henry", 50, 175.5});
      items.push_back(reflected_animal{"Lion", "Simba", 190});

      std::string json;
      expect(!glz::write_json(items, json));

      std::vector<ambiguous_variant> read_items;
      expect(glz::read_json(read_items, json) == glz::error_code::none);

      expect(read_items.size() == 2);
      expect(std::holds_alternative<reflected_person>(read_items[0]));
      expect(std::holds_alternative<reflected_animal>(read_items[1]));
   };
};

struct holder0_t
{
   int i{};
};

template <>
struct glz::meta<holder0_t>
{
   static constexpr std::string_view name = "holder0_t";
   using T = holder0_t;
   static constexpr auto value = object("i", &T::i);
};

struct holder1_t
{
   holder0_t a{};
};

template <>
struct glz::meta<holder1_t>
{
   static constexpr std::string_view name = "holder1_t";
   using T = holder1_t;
   static constexpr auto value = object("a", &T::a);
};

struct holder2_t
{
   std::vector<holder1_t> vec{};
};

template <>
struct glz::meta<holder2_t>
{
   static constexpr std::string_view name = "holder2_t";
   using T = holder2_t;
   static constexpr auto value = object("vec", &T::vec);
};

suite array_of_objects = [] {
   "array_of_objects_tests"_test = [] {
      std::string s = R"({"vec": [{"a": {"i":5}}, {"a":{ "i":2 }}]})";
      holder2_t arr{};
      expect(glz::read_json(arr, s) == glz::error_code::none);
   };
};

struct file_struct
{
   std::string name;
   std::string label;

   struct glaze
   {
      using T = file_struct;
      static constexpr auto value = glz::object("name", &T::name, "label", &T::label);
   };
};

suite read_file_test = [] {
   "read_file valid"_test = [] {
      std::string filename = "../file.json";
      {
         std::ofstream out(filename);
         expect(bool(out));
         if (out) {
            out << R"({
     "name": "my",
     "label": "label"
   })";
         }
      }

      file_struct s;
      std::string buffer{};
      expect(glz::read_file_json(s, filename, buffer) == glz::error_code::none);
   };

   "read_file invalid"_test = [] {
      file_struct s;
      expect(glz::read_file_json(s, "../nonexsistant_file.json", std::string{}) != glz::error_code::none);
   };
};

struct includer_struct
{
   glz::file_include include{};
   std::string str = "Hello";
   int i = 55;
};

suite file_include_test = [] {
   "file_include"_test = [] {
      includer_struct obj{};

      expect(glz::write_file_json(obj, "../alabastar.json", std::string{}) == glz::error_code::none);

      obj.str = "";

      std::string s = R"({"include": "../alabastar.json", "i": 100})";
      expect(glz::read_json(obj, s) == glz::error_code::none);

      expect(obj.str == "Hello") << obj.str;
      expect(obj.i == 100) << obj.i;

      obj.str = "";

      std::string buffer{};
      expect(!glz::read_file_json(obj, "../alabastar.json", buffer));
      expect(obj.str == "Hello") << obj.str;
      expect(obj.i == 55) << obj.i;
   };

   "file_include error handling"_test = [] {
      includer_struct obj{};

      auto output = glz::write_json(obj).value_or("error");
      output.erase(0, 1); // create an error

      expect(glz::buffer_to_file(output, "../alabastar.json") == glz::error_code::none);

      obj.str = "";

      std::string s = R"({"include": "../alabastar.json", "i": 100})";
      const auto ec = glz::read_json(obj, s);
      expect(bool(ec));
   };

   "file_include error handling"_test = [] {
      includer_struct obj{};

      auto output = glz::write_json(obj).value_or("error");

      expect(glz::buffer_to_file(output, "../alabastar.json") == glz::error_code::none);

      obj.str = "";

      // make the path incorrect
      std::string s = R"({"include": "../abs.json", "i": 100})";
      const auto ec = glz::read_json(obj, s);
      expect(bool(ec));
   };
};

suite file_include_test_auto = [] {
   "file_include_test_auto"_test = [] {
      includer_struct obj{};

      expect(glz::write_file_json(obj, "./auto.json", std::string{}) == false);

      obj.str = "";

      std::string s = R"({"include": "./auto.json", "i": 100})";
      expect(glz::read_json(obj, s) == glz::error_code::none);

      expect(obj.str == "Hello") << obj.str;
      expect(obj.i == 100) << obj.i;

      obj.str = "";

      expect(!glz::read_file_json(obj, "./auto.json", std::string{}));
      expect(obj.str == "Hello") << obj.str;
      expect(obj.i == 55) << obj.i;
   };
};

struct nested0
{
   includer_struct a{};
   includer_struct b{};
};

template <>
struct glz::meta<nested0>
{
   static constexpr std::string_view name = "nested0";
   using T = nested0;
   static constexpr auto value = object("include", glz::file_include{}, "a", &T::a, "b", &T::b);
};

suite nested_file_include_test = [] {
   "nested_file_include"_test = [] {
      nested0 obj;

      std::string a = R"({"include": "../b/b.json"})";
      {
         std::filesystem::create_directory("a");
         std::ofstream a_file{"./a/a.json"};

         a_file << a;
      }

      {
         std::filesystem::create_directory("b");

         obj.b.i = 13;

         expect(!glz::write_file_json(obj.b, "./b/b.json", std::string{}));
      }

      obj.b.i = 0;

      std::string s = R"({ "a": { "include": "./a/a.json" }, "b": { "include": "./b/b.json" } })";

      const auto ec = glz::read_json(obj, s);
      expect(!ec) << glz::format_error(ec, s);

      expect(obj.a.i == 13);
   };
};

// Shrink to fit is nonbinding and cannot be properly tested
/*suite shrink_to_fit = [] {
   "shrink_to_fit"_test = [] {
      std::vector<int> v = { 1, 2, 3, 4, 5, 6 };
      std::string b = R"([1,2,3])";
      expect(glz::read_json(v, b) == glz::error_code::none);

      expect(v.size() == 3);
      expect(v.capacity() > 3);

      v = { 1, 2, 3, 4, 5, 6 };

      expect(glz::read<glz::opts{.shrink_to_fit = true}>(v, b) == glz::error_code::none);
      expect(v.size() == 3);
      expect(v.capacity() == 3);
   };
};*/

suite recorder_test = [] {
   "recorder_to_file"_test = [] {
      glz::recorder<double, float> rec;

      double x = 0.0;
      float y = 0.f;

      rec["x"] = x;
      rec["y"] = y;

      for (int i = 0; i < 100; ++i) {
         x += 1.5;
         y += static_cast<float>(i);
         rec.update();
      }

      std::string s{};
      expect(not glz::write_json(rec, s));

      expect(glz::read_json(rec, s) == glz::error_code::none);

      expect(glz::write_file_json(rec, "recorder_out.json", std::string{}) == glz::error_code::none);
   };
};

suite reference_wrapper_test = [] {
   "reference_wrapper"_test = [] {
      int x = 55;
      std::reference_wrapper<int> ref = x;

      std::string s = glz::write_json(ref).value_or("error");

      expect(s == "55");

      expect(glz::read_json(ref, R"(66)") == glz::error_code::none);
      expect(x == 66);
   };
};

suite small_chars = [] {
   "small_chars"_test = [] {
      uint8_t x = 5;
      std::string s = glz::write_json(x).value_or("error");

      expect(s == "5");

      expect(glz::read_json(x, "10") == glz::error_code::none);
      expect(x == 10);
   };
};

suite ndjson_test = [] {
   "ndjson"_test = [] {
      std::vector<std::string> x = {"Hello", "World", "Ice", "Cream"};
      std::string s = glz::write_ndjson(x).value_or("error");

      expect(s ==
             R"("Hello"
"World"
"Ice"
"Cream")");

      x.clear();

      expect(!glz::read_ndjson(x, s));
      expect(x[0] == "Hello");
      expect(x[1] == "World");
      expect(x[2] == "Ice");
      expect(x[3] == "Cream");
   };

   "ndjson_list"_test = [] {
      std::list<std::string> x = {"Hello", "World", "Ice", "Cream"};
      std::string s = glz::write_ndjson(x).value_or("error");

      expect(s ==
             R"("Hello"
"World"
"Ice"
"Cream")");

      x.clear();

      expect(glz::read_ndjson(x, s) == glz::error_code::none);
      auto it = x.begin();
      expect(*it == "Hello");
      ++it;
      expect(*it == "World");
      ++it;
      expect(*it == "Ice");
      ++it;
      expect(*it == "Cream");
   };

   "ndjson_object"_test = [] {
      std::tuple<my_struct, sub_thing> x{};

      std::string s = glz::write_ndjson(x).value_or("");

      expect(s ==
             R"({"i":287,"d":3.14,"hello":"Hello World","arr":[1,2,3]}
{"a":3.14,"b":"stuff"})");

      auto& first = std::get<0>(x);
      auto& second = std::get<1>(x);

      first.hello.clear();
      first.arr[0] = 0;
      second.a = 0.0;
      second.b.clear();

      expect(glz::read_ndjson(x, s) == glz::error_code::none);

      expect(first.hello == "Hello World");
      expect(first.arr[0] = 1);
      expect(second.a == 3.14);
      expect(second.b == "stuff");
   };

   "ndjson generic"_test = [] {
      std::string buffer = R"({"arr":[1,2,3],"d":3.14,"hello":"Hello World","i":287}
{"a":3.14,"b":"stuff"})";

      std::vector<glz::generic> x{};
      expect(not glz::read_ndjson(x, buffer));

      auto out = glz::write_ndjson(x).value_or("error");
      expect(out == buffer) << out;
   };

   "empty json lines input"_test = [] {
      std::vector<int> foo;
      const auto json = glz::write_ndjson(foo).value();
      expect(json == R"()");
      std::vector<int> v{1, 2, 3};
      expect(not glz::read_ndjson<>(v, json));
      expect(v.size() == 0);
   };
};

suite std_function_handling = [] {
   "std_function"_test = [] {
      int x = 1;
      std::function<void()> increment = [&] { ++x; };
      std::string s{};
      expect(not glz::write_json(increment, s));

      expect(s == R"("std::function<void()>")") << s;

      expect(glz::read_json(increment, s) == glz::error_code::none);
   };
};

struct hide_struct
{
   int i = 287;
   double d = 3.14;
   std::string hello = "Hello World";
};

template <>
struct glz::meta<hide_struct>
{
   static constexpr std::string_view name = "hide_struct";
   using T = hide_struct;
   static constexpr auto value = object("i", &T::i, //
                                        "d", &T::d, //
                                        "hello", hide{&T::hello});
};

suite hide_tests = [] {
   "hide_write"_test = [] {
      hide_struct s{};
      std::string b{};
      expect(not glz::write_json(s, b));

      expect(b == R"({"i":287,"d":3.14})");
   };

   "hide_read"_test = [] {
      std::string b = R"({"i":287,"d":3.14,"hello":"Hello World"})";

      hide_struct s{};

      expect(glz::read_json(s, b) != glz::error_code::none);
   };
};

struct mem_f_struct
{
   int i = 0;
   int& access() { return i; }
};

template <>
struct glz::meta<mem_f_struct>
{
   static constexpr std::string_view name = "mem_f_struct";
   using T = mem_f_struct;
   static constexpr auto value = object("i", &T::i, //
                                        "access", &T::access);
};

suite member_function_tests = [] {
   "member_function2"_test = [] {
      mem_f_struct s{};

      auto i = glz::call<int&>(s, "/access");
      ++i.value();

      expect(s.i == 1);
   };
};

struct dog
{
   int age{};
   void eat() { ++age; };
};

template <>
struct glz::meta<dog>
{
   static constexpr std::string_view name = "dog";
   using T = dog;
   static constexpr auto value = object("age", &T::age, "eat", &T::eat);
};

struct cat
{
   int age{};
   void eat() { ++age; };

   void purr() {}
};

template <>
struct glz::meta<cat>
{
   static constexpr std::string_view name = "cat";
   using T = cat;
   static constexpr auto value = object("age", &T::age, "eat", &T::eat, "purr", &T::purr);
};

struct person
{
   void eat(const std::string&) {};
};

template <>
struct glz::meta<person>
{
   static constexpr std::string_view name = "person";
   static constexpr auto value = object("eat", &person::eat);
};

struct animal
{
   int age{};
   void eat() {}
};

template <>
struct glz::meta<animal>
{
   static constexpr std::string_view name = "animal";
   using T = animal;
   static constexpr auto value = object("age", &T::age, "eat", &T::eat);
};

struct complex_function_call_t
{
   std::string string(const std::string_view s, const int y) { return std::string(s) + ":" + std::to_string(y); }
};

template <>
struct glz::meta<complex_function_call_t>
{
   static constexpr std::string_view name = "complex_function_call_t";
   using T = complex_function_call_t;
   static constexpr auto value = object("string", &T::string);
};

struct string_t
{
   std::string string(const std::string_view, const int) { return ""; }
};

template <>
struct glz::meta<string_t>
{
   static constexpr std::string_view name = "string_t";
   using T = ::string_t;
   static constexpr auto value = object("string", &T::string);
};

static constexpr std::string_view json0 = R"(
{
   "fixed_object": {
      "int_array": [0, 1, 2, 3, 4, 5, 6],
      "float_array": [0.1, 0.2, 0.3, 0.4, 0.5, 0.6],
      "double_array": [3288398.238, 233e22, 289e-1, 0.928759872, 0.22222848, 0.1, 0.2, 0.3, 0.4]
   },
   "fixed_name_object": {
      "name0": "James",
      "name1": "Abraham",
      "name2": "Susan",
      "name3": "Frank",
      "name4": "Alicia"
   },
   "another_object": {
      "string": "here is some text",
      "another_string": "Hello World",
      "boolean": false,
      "nested_object": {
         "v3s": [[0.12345, 0.23456, 0.001345],
                  [0.3894675, 97.39827, 297.92387],
                  [18.18, 87.289, 2988.298]],
         "id": "298728949872"
      }
   },
   "string_array": ["Cat", "Dog", "Elephant", "Tiger"],
   "string": "Hello world",
   "number": 3.14,
   "boolean": true,
   "another_bool": false
}
)";

struct fixed_object_t
{
   std::vector<int> int_array;
   std::vector<float> float_array;
   std::vector<double> double_array;
};

struct fixed_name_object_t
{
   std::string name0{};
   std::string name1{};
   std::string name2{};
   std::string name3{};
   std::string name4{};
};

struct nested_object_t
{
   std::vector<std::array<double, 3>> v3s{};
   std::string id{};
};

struct another_object_t
{
   std::string string{};
   std::string another_string{};
   bool boolean{};
   nested_object_t nested_object{};
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
struct glz::meta<fixed_object_t>
{
   static constexpr std::string_view name = "fixed_object_t";
   using T = fixed_object_t;
   static constexpr auto value =
      object("int_array", &T::int_array, "float_array", &T::float_array, "double_array", &T::double_array);
};

template <>
struct glz::meta<fixed_name_object_t>
{
   static constexpr std::string_view name = "fixed_name_object_t";
   using T = fixed_name_object_t;
   static constexpr auto value =
      object("name0", &T::name0, "name1", &T::name1, "name2", &T::name2, "name3", &T::name3, "name4", &T::name4);
};

template <>
struct glz::meta<nested_object_t>
{
   static constexpr std::string_view name = "nested_object_t";
   using T = nested_object_t;
   static constexpr auto value = object("v3s", &T::v3s, "id", &T::id);
};

template <>
struct glz::meta<another_object_t>
{
   static constexpr std::string_view name = "another_object_t";
   using T = another_object_t;
   static constexpr auto value = object("string", &T::string, "another_string", &T::another_string, "boolean",
                                        &T::boolean, "nested_object", &T::nested_object);
};

template <>
struct glz::meta<obj_t>
{
   static constexpr std::string_view name = "obj_t";
   using T = obj_t;
   static constexpr auto value =
      object("fixed_object", &T::fixed_object, "fixed_name_object", &T::fixed_name_object, "another_object",
             &T::another_object, "string_array", &T::string_array, "string", &T::string, "number", &T::number,
             "boolean", &T::boolean, "another_bool", &T::another_bool);
};

suite json_performance = [] {
   "json performance"_test = [] {
      std::string buffer{json0};

      obj_t obj{};

      expect(glz::read_json(obj, buffer) == glz::error_code::none);
      buffer.clear();

      expect(not glz::write_json(obj, buffer));

      expect(
         buffer ==
         R"({"fixed_object":{"int_array":[0,1,2,3,4,5,6],"float_array":[0.1,0.2,0.3,0.4,0.5,0.6],"double_array":[3288398.238,2.33E24,28.9,0.928759872,0.22222848,0.1,0.2,0.3,0.4]},"fixed_name_object":{"name0":"James","name1":"Abraham","name2":"Susan","name3":"Frank","name4":"Alicia"},"another_object":{"string":"here is some text","another_string":"Hello World","boolean":false,"nested_object":{"v3s":[[0.12345,0.23456,0.001345],[0.3894675,97.39827,297.92387],[18.18,87.289,2988.298]],"id":"298728949872"}},"string_array":["Cat","Dog","Elephant","Tiger"],"string":"Hello world","number":3.14,"boolean":true,"another_bool":false})")
         << buffer;
   };
};

suite json_schema = [] {
   "json schema"_test = [] {
      Thing obj{};
      std::string schema = glz::write_json_schema<Thing>().value_or("error");
      // Note: Check schema and sample output against a json schema validator like https://www.jsonschemavalidator.net/
      // when you update this string
      expect(
         schema ==
         R"({"type":["object"],"properties":{"array":{"$ref":"#/$defs/std::array<std::string,4>"},"b":{"$ref":"#/$defs/bool"},"c":{"$ref":"#/$defs/char"},"color":{"$ref":"#/$defs/Color"},"d":{"$ref":"#/$defs/double"},"deque":{"$ref":"#/$defs/std::deque<double>"},"i":{"$ref":"#/$defs/int32_t"},"list":{"$ref":"#/$defs/std::list<int32_t>"},"map":{"$ref":"#/$defs/std::map<std::string,int32_t>"},"mapi":{"$ref":"#/$defs/std::map<int32_t,double>"},"optional":{"$ref":"#/$defs/std::optional<V3>"},"sptr":{"$ref":"#/$defs/std::shared_ptr<sub_thing>"},"thing":{"$ref":"#/$defs/sub_thing"},"thing2array":{"$ref":"#/$defs/std::array<sub_thing2,1>"},"thing_ptr":{"$ref":"#/$defs/sub_thing*"},"v":{"$ref":"#/$defs/std::variant<var1_t,var2_t>"},"vb":{"$ref":"#/$defs/std::vector<bool>"},"vec3":{"$ref":"#/$defs/V3"},"vector":{"$ref":"#/$defs/std::vector<V3>"}},"additionalProperties":false,"$defs":{"Color":{"type":["string"],"oneOf":[{"title":"Red","const":"Red"},{"title":"Green","const":"Green"},{"title":"Blue","const":"Blue"}]},"V3":{"type":["array"]},"bool":{"type":["boolean"]},"char":{"type":["string"]},"double":{"type":["number"],"minimum":-1.7976931348623157E308,"maximum":1.7976931348623157E308},"float":{"type":["number"],"minimum":-3.4028234663852886E38,"maximum":3.4028234663852886E38},"int32_t":{"type":["integer"],"minimum":-2147483648,"maximum":2147483647},"std::array<std::string,4>":{"type":["array"],"items":{"$ref":"#/$defs/std::string"},"minItems":4,"maxItems":4},"std::array<sub_thing2,1>":{"type":["array"],"items":{"$ref":"#/$defs/sub_thing2"},"minItems":1,"maxItems":1},"std::deque<double>":{"type":["array"],"items":{"$ref":"#/$defs/double"}},"std::list<int32_t>":{"type":["array"],"items":{"$ref":"#/$defs/int32_t"}},"std::map<int32_t,double>":{"type":["object"],"additionalProperties":{"$ref":"#/$defs/double"}},"std::map<std::string,int32_t>":{"type":["object"],"additionalProperties":{"$ref":"#/$defs/int32_t"}},"std::optional<V3>":{"type":["array","null"]},"std::shared_ptr<sub_thing>":{"type":["object","null"],"properties":{"a":{"$ref":"#/$defs/double"},"b":{"$ref":"#/$defs/std::string"}},"additionalProperties":false},"std::string":{"type":["string"]},"std::variant<var1_t,var2_t>":{"type":["object"],"oneOf":[{"type":["object"],"properties":{"x":{"$ref":"#/$defs/double"}},"additionalProperties":false,"title":"var1_t"},{"type":["object"],"properties":{"y":{"$ref":"#/$defs/double"}},"additionalProperties":false,"title":"var2_t"}]},"std::vector<V3>":{"type":["array"],"items":{"$ref":"#/$defs/V3"}},"std::vector<bool>":{"type":["array"],"items":{"$ref":"#/$defs/bool"}},"sub_thing":{"type":["object"],"properties":{"a":{"$ref":"#/$defs/double"},"b":{"$ref":"#/$defs/std::string"}},"additionalProperties":false},"sub_thing*":{"type":["object","null"],"properties":{"a":{"$ref":"#/$defs/double"},"b":{"$ref":"#/$defs/std::string"}},"additionalProperties":false},"sub_thing2":{"type":["object"],"properties":{"a":{"$ref":"#/$defs/double"},"b":{"$ref":"#/$defs/std::string"},"c":{"$ref":"#/$defs/double"},"d":{"$ref":"#/$defs/double"},"e":{"$ref":"#/$defs/double"},"f":{"$ref":"#/$defs/float"},"g":{"$ref":"#/$defs/double"},"h":{"$ref":"#/$defs/double"}},"additionalProperties":false}},"examples":[{"thing":{},"i":42}],"required":["thing","i"],"title":"Thing"})")
         << schema;
   };
};

// custom type handling
struct date
{
   uint64_t data;
   std::string human_readable;
};

template <>
struct glz::meta<date>
{
   using T = date;
   static constexpr auto value = object("date", &T::human_readable);
};

namespace glz
{
   template <>
   struct from<JSON, date>
   {
      template <auto Opts>
      static void op(date& value, auto&&... args)
      {
         parse<JSON>::op<Opts>(value.human_readable, args...);
         value.data = std::stoi(value.human_readable);
      }
   };

   template <>
   struct to<JSON, date>
   {
      template <auto Opts>
      static void op(date& value, auto&&... args) noexcept
      {
         value.human_readable = std::to_string(value.data);
         serialize<JSON>::op<Opts>(value.human_readable, args...);
      }
   };
}

suite date_test = [] {
   "date"_test = [] {
      date d{};
      d.data = 55;

      std::string s{};
      expect(not glz::write_json(d, s));

      expect(s == R"("55")");

      d.data = 0;
      expect(not glz::read_json(d, s));
      expect(d.data == 55);
   };
};

struct date_base
{
   uint64_t data;
   std::string human_readable;
};

template <class T>
   requires std::derived_from<T, date_base>
struct glz::meta<T>
{
   static constexpr auto value = glz::object("date", &T::human_readable);
   static constexpr auto custom_read = true;
   static constexpr auto custom_write = true;
};

namespace glz
{
   template <class T>
      requires std::derived_from<T, date_base>
   struct from<JSON, T>
   {
      template <auto Opts>
      static void op(date_base& value, auto&&... args)
      {
         parse<JSON>::op<Opts>(value.human_readable, args...);
         value.data = std::stoi(value.human_readable);
      }
   };

   template <class T>
      requires std::derived_from<T, date_base>
   struct to<JSON, T>
   {
      template <auto Opts>
      static void op(date_base& value, auto&&... args) noexcept
      {
         value.human_readable = std::to_string(value.data);
         serialize<JSON>::op<Opts>(value.human_readable, args...);
      }
   };
}

struct date_derived : date_base
{};

suite date_base_test = [] {
   "date_base"_test = [] {
      date_derived d{};
      d.data = 55;

      std::string s{};
      expect(not glz::write_json(d, s));

      expect(s == R"("55")");

      d.data = 0;
      expect(not glz::read_json(d, s));
      expect(d.data == 55);
   };
};

struct unicode_keys_t
{
   std::string happy{};
};

template <>
struct glz::meta<unicode_keys_t>
{
   using T = unicode_keys_t;
   static constexpr auto value = object("😀", &T::happy);
};

struct question_t
{
   std::string text{};
};

template <>
struct glz::meta<question_t>
{
   using T = question_t;
   static constexpr auto value = object("ᇿ", &T::text);
};

struct question_escaped_t
{
   std::string text{};
};

template <>
struct glz::meta<question_escaped_t>
{
   using T = question_escaped_t;
   static constexpr auto value = object("ᇿ", &T::text, escape_unicode<"ᇿ">, &T::text);
};

static_assert(glz::escape_unicode<"ᇿ"> == R"(\u11FF)");

suite unicode_tests = [] {
   "unicode"_test = [] {
      std::string str = "😀😃😄🍌💐🌹🥀🌺🌷🌸💮🏵️🌻🌼";

      std::string buffer{};
      expect(not glz::write_json(str, buffer));

      str.clear();
      expect(not glz::read_json(str, buffer));

      expect(str == "😀😃😄🍌💐🌹🥀🌺🌷🌸💮🏵️🌻🌼");
   };

   "unicode_unescaped_smile"_test = [] {
      std::string str = R"({"😀":"smile"})";
      unicode_keys_t obj{};
      expect(not glz::read_json(obj, str));

      expect(obj.happy == "smile");
   };

   "unicode_escaped_smile"_test = [] {
      // more than 4 characters in unicode is not valid JSON
      std::string str = R"({"\u1F600":"smile"})";
      unicode_keys_t obj{};
      expect(glz::read_json(obj, str) != glz::error_code::none);
   };

   "unicode_unescaped"_test = [] {
      std::string str = R"({"ᇿ":"ᇿ"})";
      question_t obj{};
      expect(not glz::read_json(obj, str));

      expect(obj.text == "ᇿ");
   };

   "unicode_escaped"_test = [] {
      std::string str = R"({"\u11FF":"\u11FF"})";
      question_escaped_t obj{};
      expect(not glz::read_json(obj, str));

      expect(obj.text == "ᇿ");
   };

   "surrogate pair"_test = [] {
      const char* json = R"("\uD83C\uDF40")"; // 🍀
      std::string val;
      expect(!glz::read_json(val, json));
      expect(val == "🍀");
   };

   "mixed unicode"_test = [] {
      const char* json = R"("\u11FF\uD83C\uDF40ᇿ🍀\u11FF")"; // ᇿ🍀ᇿ🍀ᇿ
      std::string val;
      expect(!glz::read_json(val, json));
      expect(val == "ᇿ🍀ᇿ🍀ᇿ");
   };

   "multi surrogate unicode"_test = [] {
      const char* json = R"("\uD83D\uDE00\uD83C\uDF40😀🍀\uD83D\uDE00")"; // 😀🍀😀🍀😀
      std::string val;
      expect(!glz::read_json(val, json));
      expect(val == "😀🍀😀🍀😀");
   };
};

// Custom options to enable control character escaping
struct opts_escape_control_characters : glz::opts
{
   bool escape_control_characters = true;
};

// Structure for escaped control chars
struct control_char_escaped_t
{
   std::string value{};
};

template <>
struct glz::meta<control_char_escaped_t>
{
   using T = control_char_escaped_t;
   static constexpr auto value = object(escape_unicode<"\x01">, &T::value);
};

static_assert(glz::escape_unicode<"\x01"> == R"(\u0001)");
static_assert(glz::escape_unicode<"\x1F"> == R"(\u001F)");

suite control_character_tests = [] {
   "basic_control_char_escaping"_test = [] {
      // Test basic escaping of control characters in strings
      std::string str = "Hello\x01World\x02!";

      std::string buffer{};
      expect(not glz::write<opts_escape_control_characters{}>(str, buffer));

      // Buffer should contain escaped control chars
      expect(buffer.find("\\u0001") != std::string::npos) << "SOH control character should be escaped";
      expect(buffer.find("\\u0002") != std::string::npos) << "STX control character should be escaped";

      // Reading back the escaped JSON
      std::string result;
      expect(not glz::read_json(result, buffer));
      expect(result == str) << "Original string should match after read/write";
   };

   "null_character_escaping"_test = [] {
      // Test NULL character (U+0000)
      std::string str = "before\0after", expected = "before\0after";
      str.resize(11); // Ensure null byte is included in size
      expected.resize(11);

      std::string buffer{};
      expect(not glz::write<opts_escape_control_characters{}>(str, buffer));

      // Buffer should contain escaped NULL
      expect(buffer.find("\\u0000") != std::string::npos) << "NULL character should be escaped";

      // Reading back the escaped JSON
      std::string result;
      expect(not glz::read_json(result, buffer));
      expect(result.size() == 11) << "String with NULL should preserve size";
      expect(result == expected) << "String with NULL should match original";
   };

   "control_char_explicitly_escaped"_test = [] {
      // Test explicitly escaped control character
      std::string str = R"({"\u0001":"control char value"})";
      control_char_escaped_t obj{};

      expect(not glz::read_json(obj, str));
      expect(obj.value == "control char value") << "Value with escaped control char key should be read correctly";
   };

   "mixed_control_and_regular"_test = [] {
      // Test mix of control and regular characters
      // Build string using individual characters to avoid compiler issues
      std::string str = "Regular";
      str += char(0x00);
      str += char(0x01);
      str += char(0x02);
      str += char(0x03);
      str += "Control";
      str += char(0x1F);
      str += "Chars";

      std::string buffer{};
      expect(not glz::write<opts_escape_control_characters{}>(str, buffer));

      expect(buffer == R"("Regular\u0000\u0001\u0002\u0003Control\u001FChars")");

      // Reading back
      std::string result;
      expect(not glz::read_json(result, buffer));
      expect(result == str) << "Mixed string should match original";
   };

   "multiple_control_chars"_test = [] {
      // String with multiple consecutive control characters
      std::string str = "\x01\x02\x03\x04\x05";

      std::string buffer{};
      expect(not glz::write<opts_escape_control_characters{}>(str, buffer));

      // Buffer should have all characters escaped
      expect(buffer.find("\\u0001\\u0002\\u0003\\u0004\\u0005") != std::string::npos)
         << "Multiple consecutive control chars should be escaped";

      // Reading back
      std::string result;
      expect(not glz::read_json(result, buffer));
      expect(result == str) << "Multiple control chars string should match original";
   };

   "control_with_surrogate_pairs"_test = [] {
      // Mix control chars with surrogate pairs
      std::string str = "\x01🍀\x02😀\x03";

      std::string buffer{};
      expect(not glz::write<opts_escape_control_characters{}>(str, buffer));

      // Reading back
      std::string result;
      expect(not glz::read_json(result, buffer));
      expect(result == str) << "Control chars with surrogate pairs should match original";
   };

   "object_with_control_chars"_test = [] {
      // Test object with control chars in both keys and values
      std::string json = R"({"normal":"\u0001\u0002\u0003","key\u0004":"value"})";

      std::map<std::string, std::string> obj;
      expect(not glz::read_json(obj, json));

      // Check the values were parsed correctly
      expect(obj["normal"] == "\x01\x02\x03") << "Control chars in values should be decoded";
      expect(obj.find("key\x04") != obj.end()) << "Control chars in keys should be decoded";
      expect(obj["key\x04"] == "value") << "Value for key with control char should match";
   };

   "invalid_escape_sequences"_test = [] {
      // Test invalid Unicode escape sequences
      const char* json = R"("\u001")"; // incomplete escape sequence
      std::string val;
      expect(glz::read_json(val, json) != glz::error_code::none) << "Invalid escape sequence should fail";
   };

   "all_ascii_control_chars"_test = [] {
      // Test all ASCII control characters (0-31 plus DEL=127)
      std::string str;
      for (int i = 0; i < 32; i++) {
         str += static_cast<char>(i);
      }
      str += static_cast<char>(127); // DEL

      std::string buffer{};
      expect(not glz::write<opts_escape_control_characters{}>(str, buffer))
         << "Should successfully write all control characters";

      std::string result;
      expect(not glz::read_json(result, buffer)) << "Should successfully read all control characters";

      expect(result == str) << "All control characters should roundtrip correctly";
   };

   "string_view_wrapped_char_array"_test = [] {
      // Wrap a char array with embedded nulls into a string_view to preserve length information
      char raw[4] = {0, 0, 1, 0};
      std::string_view view{raw, sizeof(raw)};

      std::string buffer{};
      expect(not glz::write<opts_escape_control_characters{}>(view, buffer));

      expect(buffer == R"("\u0000\u0000\u0001\u0000")")
         << "string_view constructed with explicit length should escape all bytes";

      std::string roundtrip;
      expect(not glz::read_json(roundtrip, buffer));
      expect(roundtrip.size() == view.size());
      expect(std::equal(roundtrip.begin(), roundtrip.end(), view.begin()))
         << "Roundtripped data should match original byte sequence";
   };

   "string_view_wrapped_std_array"_test = [] {
      // Same scenario using std::array documented for structured bindings
      const std::array<char, 5> arr{{'A', 0, 'B', 0, 'C'}};
      const std::string_view view{arr.data(), arr.size()};

      std::string buffer{};
      expect(not glz::write<opts_escape_control_characters{}>(view, buffer));

      expect(buffer == R"("A\u0000B\u0000C")") << "std::array wrapped in string_view should retain embedded nulls";

      std::string parsed;
      expect(not glz::read_json(parsed, buffer));
      expect(parsed.size() == view.size());
      expect(std::equal(parsed.begin(), parsed.end(), view.begin()))
         << "Parsed content should match the original array contents";
   };
};

struct value_t
{
   int x{};
};

template <>
struct glz::meta<value_t>
{
   using T = value_t;
   static constexpr auto value{&T::x};
};

struct lambda_value_t
{
   int x{};
};

template <>
struct glz::meta<lambda_value_t>
{
   static constexpr auto value = [](auto&& self) -> auto& { return self.x; };
};

suite value_test = [] {
   "value"_test = [] {
      std::string s = "5";
      value_t v{};

      expect(glz::read_json(v, s) == glz::error_code::none);
      expect(v.x == 5);

      s.clear();
      expect(not glz::write_json(v, s));
      expect(s == "5");
   };

   "lambda value"_test = [] {
      std::string s = "5";
      lambda_value_t v{};

      expect(glz::read_json(v, s) == glz::error_code::none);
      expect(v.x == 5);

      s.clear();
      expect(not glz::write_json(v, s));
      expect(s == "5");
   };
};

struct TestMsg
{
   uint64_t id{};
   std::string val{};
};

template <>
struct glz::meta<TestMsg>
{
   static constexpr std::string_view name = "TestMsg";
   using T = TestMsg;
   static constexpr auto value = object("id", &T::id, "val", &T::val);
};

suite byte_buffer = [] {
   "uint8_t buffer"_test = [] {
      TestMsg msg{};
      msg.id = 5;
      msg.val = "hello";
      std::vector<uint8_t> buffer{};
      expect(not glz::write_json(msg, buffer));

      buffer.emplace_back('\0');

      msg.id = 0;
      msg.val = "";

      expect(!glz::read_json(msg, buffer));
      expect(msg.id == 5);
      expect(msg.val == "hello");
   };

   "std::byte buffer"_test = [] {
      TestMsg msg{};
      msg.id = 5;
      msg.val = "hello";
      std::vector<std::byte> buffer{};
      expect(not glz::write_json(msg, buffer));

      buffer.emplace_back(static_cast<std::byte>('\0'));

      msg.id = 0;
      msg.val = "";

      expect(!glz::read_json(msg, buffer));
      expect(msg.id == 5);
      expect(msg.val == "hello");
   };

   "char8_t buffer"_test = [] {
      TestMsg msg{};
      msg.id = 5;
      msg.val = "hello";
      std::vector<char8_t> buffer{};
      expect(not glz::write_json(msg, buffer));

      buffer.emplace_back('\0');

      msg.id = 0;
      msg.val = "";

      expect(!glz::read_json(msg, buffer));
      expect(msg.id == 5);
      expect(msg.val == "hello");
   };
};

template <class T>
struct custom_unique
{
   std::unique_ptr<T> x{};

   custom_unique(std::unique_ptr<T>&& in) : x(std::move(in)) {}

   operator bool() { return bool(x); }

   T& operator*() { return *x; }

   void reset() { x.reset(); }
};

template <class T, class... Args>
inline auto make_custom_unique(Args&&... args)
{
   return custom_unique{std::make_unique<T>(std::forward<Args>(args)...)};
}

template <class T>
struct glz::meta<custom_unique<T>>
{
   static constexpr auto construct = [] { return make_custom_unique<T>(); };
};

suite custom_unique_tests = [] {
   "custom unique"_test = [] {
      auto c = make_custom_unique<int>(5);

      std::string s = "5";
      expect(glz::read_json(c, s) == glz::error_code::none);

      expect(*c.x == 5);

      s.clear();
      expect(not glz::write_json(c, s));
      expect(s == "5");

      s = "null";
      expect(glz::read_json(c, s) == glz::error_code::none);
      expect(!c);

      s = "5";
      expect(glz::read_json(c, s) == glz::error_code::none);

      expect(*c.x == 5);
   };
};

#include <set>
#include <unordered_set>

static_assert(glz::emplaceable<std::set<std::string>>);

suite sets = [] {
   "std::unordered_set"_test = [] {
      std::unordered_set<std::string> set;
      expect(glz::read_json(set, "[]") == glz::error_code::none);
      expect(set.empty());

      set = {"hello", "world"};
      std::string b{};
      expect(not glz::write_json(set, b));

      expect(b == R"(["hello","world"])" || b == R"(["world","hello"])");

      set.clear();

      std::string_view withSpaces = R"(
      [
         "hello",
         "world"
      ]
      )";

      expect(glz::read_json(set, withSpaces) == glz::error_code::none);

      expect(set.count("hello") == 1);
      expect(set.count("world") == 1);
   };

   "std::set<int>"_test = [] {
      std::set<int> set;
      expect(glz::read_json(set, "[]") == glz::error_code::none);
      expect(set.empty());

      expect(glz::read_json(set, " [  ] ") == glz::error_code::none);
      expect(set.empty());

      set = {5, 4, 3, 2, 1};
      std::string b{};
      expect(not glz::write_json(set, b));

      expect(b == R"([1,2,3,4,5])");

      set.clear();

      expect(glz::read_json(set, b) == glz::error_code::none);

      expect(set.count(1) == 1);
      expect(set.count(2) == 1);
      expect(set.count(3) == 1);
      expect(set.count(4) == 1);
      expect(set.count(5) == 1);

      b = "[6,7,8,9,10]";
      expect(!glz::read_json(set, b)); // second reading
      expect(set.size() == 5);

      std::set<int> set2;
      std::string_view withSpaces = R"(
      [
         6,
         7,
         8,
         9,
         10
      ]
      )";
      expect(!glz::read_json(set2, withSpaces)); // third reading with spaces
      expect(set == set2);
   };

   "std::set<std::string>"_test = [] {
      std::set<std::string> set;
      expect(glz::read_json(set, "[]") == glz::error_code::none);
      expect(set.empty());

      set = {"a", "b", "c", "d", "e"};
      std::string b{};
      expect(not glz::write_json(set, b));

      expect(b == R"(["a","b","c","d","e"])");

      set.clear();

      expect(glz::read_json(set, b) == glz::error_code::none);

      expect(set.count("a") == 1);
      expect(set.count("b") == 1);
      expect(set.count("c") == 1);
      expect(set.count("d") == 1);
      expect(set.count("e") == 1);

      b = R"(["f","g","h","i","j"])";
      expect(!glz::read_json(set, b)); // second reading
      expect(set.size() == 5);
   };

   "std::multiset"_test = [] {
      std::multiset<int> set;
      expect(glz::read_json(set, "[]") == glz::error_code::none);
      expect(set.empty());

      set = {5, 4, 3, 2, 1, 4, 1};
      std::string b{};
      expect(not glz::write_json(set, b));

      expect(b == R"([1,1,2,3,4,4,5])");

      set.clear();

      expect(glz::read_json(set, b) == glz::error_code::none);

      expect(set.count(1) == 2);
      expect(set.count(2) == 1);
      expect(set.count(3) == 1);
      expect(set.count(4) == 2);
      expect(set.count(5) == 1);
   };

   "std::set<std::map<>>"_test = [] {
      // test is important: map is amended in its parsing, not replaced like other entry types
      using Entry = std::map<std::string, int>;
      std::set<Entry> things;
      const auto input_string = R"([
        {"one": 1},
        {"two": 2},
        {"three": 3},
        {"four": 4},
        {"five": 5}
      ])";
      expect(!glz::read_json(things, input_string));
      auto s = glz::write_json(things).value_or("error");
      expect(s == R"([{"five":5},{"four":4},{"one":1},{"three":3},{"two":2}])") << s;
   };
};

struct flags_t
{
   bool x{true};
   bool y{};
   bool z{true};
};

template <>
struct glz::meta<flags_t>
{
   using T = flags_t;
   static constexpr auto value = flags("x", &T::x, "y", &T::y, "z", &T::z);
};

suite flag_test = [] {
   "flags"_test = [] {
      flags_t s{};

      std::string b{};
      expect(not glz::write_json(s, b));

      expect(b == R"(["x","z"])");

      s.x = false;
      s.z = false;

      expect(glz::read_json(s, b) == glz::error_code::none);

      expect(s.x);
      expect(s.z);
   };
};

struct xy_t
{
   int x{};
   int y{};
};

template <>
struct glz::meta<xy_t>
{
   using T = xy_t;
   static constexpr auto value = object("x", &T::x, "y", &T::y);
};

struct bomb_t
{
   xy_t data{};
};

template <>
struct glz::meta<bomb_t>
{
   using T = bomb_t;
   static constexpr auto value = object("action", skip{}, "data", &T::data);
};

suite get_sv = [] {
   "get_sv"_test = [] {
      std::string s = R"({"obj":{"x":5.5}})";
      const auto x = glz::get_view_json<"/obj/x">(s).value();

      auto str = glz::sv{x.data(), x.size()};
      expect(str == "5.5");

      double y;
      expect(glz::read_json(y, x) == glz::error_code::none);

      auto z = glz::get_as_json<double, "/obj/x">(s);

      expect(z == 5.5);

      auto view = glz::get_sv_json<"/obj/x">(s);

      expect(view == "5.5");
   };

   "get_sv_array"_test = [] {
      std::string s = R"({"obj":{"x":[0,1,2]}})";

      auto x = glz::get_as_json<std::vector<int>, "/obj/x">(s);
      expect(x == std::vector<int>{0, 1, 2});
      auto x0 = glz::get_as_json<int, "/obj/x/0">(s);
      expect(x0 == 0);
   };

   "get_as_json valid"_test = [] {
      std::string_view data = R"({ "data": [ {"a": true} ] })";

      auto a = glz::get_as_json<bool, "/data/0/a">(data);
      expect(a.has_value());
      expect(a.value());
   };

   "action"_test = [] {
      std::string buffer = R"( { "action": "DELETE", "data": { "x": 10, "y": 200 }})";

      auto action = glz::get_sv_json<"/action">(buffer);

      expect(action == R"("DELETE")");
      if (action == R"("DELETE")") {
         auto bomb = glz::read_json<bomb_t>(buffer);
         expect(bomb->data.x == 10);
         expect(bomb->data.y == 200);
      }
   };

   "write_at"_test = [] {
      std::string buffer = R"( { "action": "DELETE", "data": { "x": 10, "y": 200 }})";

      auto ec = glz::write_at<"/action">(R"("GO!")", buffer);
      expect(not ec);
      expect(buffer == R"( { "action": "GO!", "data": { "x": 10, "y": 200 }})");
   };

   "write_at"_test = [] {
      std::string buffer = R"({"str":"hello","number":3.14,"sub":{"target":"X"}})";

      auto ec = glz::write_at<"/sub/target">("42", buffer);
      expect(not ec);
      expect(buffer == R"({"str":"hello","number":3.14,"sub":{"target":42}})");
   };
};

suite no_except_tests = [] {
   "no except"_test = [] {
      my_struct s{};
      std::string b = R"({"i":5,,})";
      auto ec = glz::read_json(s, b);
      expect(ec != glz::error_code::none) << static_cast<uint32_t>(ec);
   };
};

struct opts_validate_trailing_whitespace : glz::opts
{
   bool validate_trailing_whitespace = false;
};

suite validation_tests = [] {
   "validate_json"_test = [] {
      glz::generic json{};

      // Tests are taken from the https://www.json.org/JSON_checker/ test suite

      std::string fail10 = R"({"Extra value after close": true} "misplaced quoted value")";
      auto ec_fail10 = glz::read<opts_validate_trailing_whitespace{true}>(json, fail10);
      expect(ec_fail10 != glz::error_code::none);
      expect(glz::validate_json(fail10) != glz::error_code::none);

      std::string fail11 = R"({"Illegal expression": 1 + 2})";
      auto ec_fail11 = glz::read_json(json, fail11);
      expect(ec_fail11 != glz::error_code::none);
      expect(glz::validate_json(fail11) != glz::error_code::none);

      std::string fail12 = R"({"Illegal invocation": alert()})";
      auto ec_fail12 = glz::read_json(json, fail12);
      expect(ec_fail12 != glz::error_code::none);
      expect(glz::validate_json(fail12) != glz::error_code::none);

      std::string fail13 = R"({"Numbers cannot have leading zeroes": 013})";
      auto ec_fail13 = glz::read_json(json, fail13);
      expect(ec_fail13 != glz::error_code::none);
      expect(glz::validate_json(fail13) != glz::error_code::none);

      std::string fail14 = R"({"Numbers cannot be hex": 0x14})";
      auto ec_fail14 = glz::read_json(json, fail14);
      expect(ec_fail14 != glz::error_code::none);
      expect(glz::validate_json(fail14) != glz::error_code::none);

      std::string fail15 = R"(["Illegal backslash escape: \x15"])";
      auto ec_fail15 = glz::read_json(json, fail15);
      expect(ec_fail15 != glz::error_code::none);
      expect(glz::validate_json(fail15) != glz::error_code::none);

      std::string fail16 = R"([\naked])";
      auto ec_fail16 = glz::read_json(json, fail16);
      expect(ec_fail16 != glz::error_code::none);
      expect(glz::validate_json(fail16) != glz::error_code::none);

      std::string fail17 = R"(["Illegal backslash escape: \017"])";
      auto ec_fail17 = glz::read_json(json, fail17);
      expect(ec_fail17 != glz::error_code::none);
      expect(glz::validate_json(fail17) != glz::error_code::none);

      std::string fail19 = R"({"Missing colon" null})";
      auto ec_fail19 = glz::read_json(json, fail19);
      expect(ec_fail19 != glz::error_code::none);
      expect(glz::validate_json(fail19) != glz::error_code::none);

      std::string fail2 = R"(["Unclosed array")";
      auto ec_fail2 = glz::read_json(json, fail2);
      expect(ec_fail2 != glz::error_code::none);
      expect(glz::validate_json(fail2) != glz::error_code::none);

      std::string fail20 = R"({"Double colon":: null})";
      auto ec_fail20 = glz::read_json(json, fail20);
      expect(ec_fail20 != glz::error_code::none);
      expect(glz::validate_json(fail20) != glz::error_code::none);

      std::string fail21 = R"({"Comma instead of colon", null})";
      auto ec_fail21 = glz::read_json(json, fail21);
      expect(ec_fail21 != glz::error_code::none);
      expect(glz::validate_json(fail21) != glz::error_code::none);

      std::string fail22 = R"(["Colon instead of comma": false])";
      auto ec_fail22 = glz::read_json(json, fail22);
      expect(ec_fail22 != glz::error_code::none);
      expect(glz::validate_json(fail22) != glz::error_code::none);

      std::string fail23 = R"(["Bad value", truth])";
      auto ec_fail23 = glz::read_json(json, fail23);
      expect(ec_fail23 != glz::error_code::none);
      expect(glz::validate_json(fail23) != glz::error_code::none);

      std::string fail24 = R"(['single quote'])";
      auto ec_fail24 = glz::read_json(json, fail24);
      expect(ec_fail24 != glz::error_code::none);
      expect(glz::validate_json(fail24) != glz::error_code::none);

      std::string fail25 = R"(["	tab	character	in	string	"])";
      auto ec_fail25 = glz::read_json(json, fail25);
      expect(ec_fail25 != glz::error_code::none);
      expect(glz::validate_json(fail25) != glz::error_code::none);

      std::string fail26 = R"(["tab\   character\   in\  string\  "])";
      auto ec_fail26 = glz::read_json(json, fail26);
      expect(ec_fail26 != glz::error_code::none);
      expect(glz::validate_json(fail26) != glz::error_code::none);

      std::string fail27 = R"(["line
break"])";
      auto ec_fail27 = glz::read_json(json, fail27);
      expect(ec_fail27 != glz::error_code::none);
      expect(glz::validate_json(fail27) != glz::error_code::none);

      std::string fail28 = R"(["line\
break"])";
      auto ec_fail28 = glz::read_json(json, fail28);
      expect(ec_fail28 != glz::error_code::none);
      expect(glz::validate_json(fail28) != glz::error_code::none);

      std::string fail29 = R"([0e])";
      auto ec_fail29 = glz::read_json(json, fail29);
      expect(ec_fail29 != glz::error_code::none);
      expect(glz::validate_json(fail29) != glz::error_code::none);

      std::string fail3 = R"({unquoted_key: "keys must be quoted"})";
      auto ec_fail3 = glz::read_json(json, fail3);
      expect(ec_fail3 != glz::error_code::none);
      expect(glz::validate_json(fail3) != glz::error_code::none);

      std::string fail30 = R"([0e+])";
      auto ec_fail30 = glz::read_json(json, fail30);
      expect(ec_fail30 != glz::error_code::none);
      expect(glz::validate_json(fail30) != glz::error_code::none);

      std::string fail31 = R"([0e+-1])";
      auto ec_fail31 = glz::read_json(json, fail31);
      expect(ec_fail31 != glz::error_code::none);
      expect(glz::validate_json(fail31) != glz::error_code::none);

      std::string fail32 = R"({"Comma instead if closing brace": true,)";
      auto ec_fail32 = glz::read_json(json, fail32);
      expect(ec_fail32 != glz::error_code::none);
      expect(glz::validate_json(fail32) != glz::error_code::none);

      std::string fail33 = R"(["mismatch"})";
      auto ec_fail33 = glz::read_json(json, fail33);
      expect(ec_fail33 != glz::error_code::none);
      expect(glz::validate_json(fail33) != glz::error_code::none);

      std::string fail4 = R"(["extra comma",])";
      auto ec_fail4 = glz::read_json(json, fail4);
      expect(ec_fail4 != glz::error_code::none);
      expect(glz::validate_json(fail4) != glz::error_code::none);

      std::string fail5 = R"(["double extra comma",,])";
      auto ec_fail5 = glz::read_json(json, fail5);
      expect(ec_fail5 != glz::error_code::none);
      expect(glz::validate_json(fail5) != glz::error_code::none);

      std::string fail6 = R"([   , "<-- missing value"])";
      auto ec_fail6 = glz::read_json(json, fail6);
      expect(ec_fail6 != glz::error_code::none);
      expect(glz::validate_json(fail6) != glz::error_code::none);

      std::string fail7 = R"(["Comma after the close"],)";
      auto ec_fail7 = glz::read<opts_validate_trailing_whitespace{true}>(json, fail7);
      expect(ec_fail7 != glz::error_code::none);
      expect(glz::validate_json(fail7) != glz::error_code::none);

      std::string fail8 = R"(["Extra close"]])";
      auto ec_fail8 = glz::read<opts_validate_trailing_whitespace{true}>(json, fail8);
      expect(ec_fail8 != glz::error_code::none);
      expect(glz::validate_json(fail8) != glz::error_code::none);

      std::string fail9 = R"({"Extra comma": true,})";
      auto ec_fail9 = glz::read_json(json, fail9);
      expect(ec_fail9 != glz::error_code::none);
      expect(glz::validate_json(fail9) != glz::error_code::none);

      std::string pass1 = R"([
    "JSON Test Pattern pass1",
    {"object with 1 member":["array with 1 element"]},
    {},
    [],
    -42,
    true,
    false,
    null,
    {
        "integer": 1234567890,
        "real": -9876.543210,
        "e": 0.123456789e-12,
        "E": 1.234567890E+34,
        "":  23456789012E66,
        "zero": 0,
        "one": 1,
        "space": " ",
        "quote": "\"",
        "backslash": "\\",
        "controls": "\b\f\n\r\t",
        "slash": "/ & \/",
        "alpha": "abcdefghijklmnopqrstuvwyz",
        "ALPHA": "ABCDEFGHIJKLMNOPQRSTUVWYZ",
        "digit": "0123456789",
        "0123456789": "digit",
        "special": "`1~!@#$%^&*()_+-={':[,]}|;.</>?",
        "hex": "\u0123\u4567\u89AB\uCDEF\uabcd\uef4A",
        "true": true,
        "false": false,
        "null": null,
        "array":[  ],
        "object":{  },
        "address": "50 St. James Street",
        "url": "http://www.JSON.org/",
        "comment": "// /* <!-- --",
        "# -- --> */": " ",
        " s p a c e d " :[1,2 , 3

,

4 , 5        ,          6           ,7        ],"compact":[1,2,3,4,5,6,7],
        "jsontext": "{\"object with 1 member\":[\"array with 1 element\"]}",
        "quotes": "&#34; \u0022 %22 0x22 034 &#x22;",
        "\/\\\"\uCAFE\uBABE\uAB98\uFCDE\ubcda\uef4A\b\f\n\r\t`1~!@#$%^&*()_+-=[]{}|;:',./<>?"
: "A key can be any string"
    },
    0.5 ,98.6
,
99.44
,

1066,
1e1,
0.1e1,
1e-1,
1e00,2e+00,2e-00
,"rosebud"])";
      auto ec_pass1 = glz::read_json(json, pass1);
      expect(ec_pass1 == glz::error_code::none) << glz::format_error(ec_pass1, pass1);
      expect(!glz::validate_json(pass1));

      std::string pass2 = R"([[[[[[[[[[[[[[[[[[["Not too deep"]]]]]]]]]]]]]]]]]]])";
      auto ec_pass2 = glz::read_json(json, pass2);
      expect(ec_pass2 == glz::error_code::none);
      expect(glz::validate_json(pass2) == glz::error_code::none);

      std::string pass3 = R"({
    "JSON Test Pattern pass3": {
        "The outermost value": "must be an object or array.",
        "In this test": "It is an object."
    }
}
)";
      auto ec_pass3 = glz::read_json(json, pass3);
      expect(!ec_pass3);
      expect(!glz::validate_json(pass3));
   };
};

suite utf8_validate = [] {
   "utf8_validate"_test = [] {
      // In ANSI (GBK) encoding
      {
         auto e = glz::validate_json(R"({"key":"value中文"})");
         expect(!e);
      }

      // In UTF-8
      {
         auto e = glz::validate_json((const char*)u8R"({"key":"value中文"})");
         expect(!e);
      }
   };
};

struct StructE
{
   std::string e;
};

struct Sample
{
   int a;
   StructE d;
};

suite invalid_keys = [] {
   "invalid_keys"_test = [] {
      std::string test_str = R"({"a":1,"bbbbbb":"0","c":"Hello World","d":{"e":"123"} })";
      auto s = Sample{};

      expect(glz::read<glz::opts{.error_on_unknown_keys = true}>(s, test_str) != glz::error_code::none);
      expect(glz::read<glz::opts{.error_on_unknown_keys = false}>(s, test_str) == glz::error_code::none);
   };
};

struct yz_t
{
   int y{};
   int z{};
};

template <>
struct glz::meta<yz_t>
{
   using T = yz_t;
   static constexpr auto value = object("y", &T::y, "z", &T::z);
};

struct xz_t
{
   int x{};
   int z{};
};

template <>
struct glz::meta<xz_t>
{
   using T = xz_t;
   static constexpr auto value = object("x", &T::x, "z", &T::z);
};

suite metaobject_variant_auto_deduction = [] {
   "metaobject_variant_auto_deduction"_test = [] {
      std::variant<xy_t, yz_t, xz_t> var{};

      std::string b = R"({"y":1,"z":2})";
      expect(glz::read_json(var, b) == glz::error_code::none);
      expect(std::holds_alternative<yz_t>(var));
      expect(std::get<yz_t>(var).y == 1);
      expect(std::get<yz_t>(var).z == 2);

      b = R"({"x":5,"y":7})";
      expect(glz::read_json(var, b) == glz::error_code::none);
      expect(std::holds_alternative<xy_t>(var));
      expect(std::get<xy_t>(var).x == 5);
      expect(std::get<xy_t>(var).y == 7);

      b = R"({"z":3,"x":4})";
      expect(glz::read_json(var, b) == glz::error_code::none);
      expect(std::holds_alternative<xz_t>(var));
      expect(std::get<xz_t>(var).z == 3);
      expect(std::get<xz_t>(var).x == 4);
   };
};

struct my_struct2
{
   std::string string1;
   std::string string2;
   struct glaze
   {
      using T = my_struct2;
      static constexpr auto value = glz::object("jsonrpc", &T::string1, "method", &T::string2);
   };
};

suite invalid_array_as_object = [] {
   "invalid_array_as_object"_test = [] {
      {
         std::string raw_json = R"([1])";
         my_struct2 request_object;
         expect(glz::read_json<my_struct2>(request_object, raw_json) != glz::error_code::none);
      }
      {
         const std::string raw_json = R"(
          [1]
        )";
         my_struct2 request_object;
         expect(glz::read_json<my_struct2>(request_object, raw_json) != glz::error_code::none);
      }
   };
};

struct OKX_OrderBook_Data
{
   std::string alias;
   std::string baseCcy;
   std::string category;
   std::string ctMult;
   std::string ctType;
   std::string ctVal;
   std::string ctValCcy;
   std::string expTime;
   std::string instFamily;
   std::string instId;
   std::string instType;
   std::string lever;
   std::string listTime;
   std::string lotSz;
   std::string maxIcebergSz;
   std::string maxLmtSz;
   std::string maxMktSz;
   std::string maxStopSz;
   std::string maxTriggerSz;
   std::string maxTwapSz;
   std::string minSz;

   std::string optType;
   std::string quoteCcy;
   std::string settleCcy;
   std::string state;
   std::string stk;
   std::string tickSz;
   std::string uly;
};

struct OKX_OrderBook
{
   std::string code;
   std::vector<OKX_OrderBook_Data> data;
   std::string msg;
};

template <>
struct glz::meta<OKX_OrderBook>
{
   using T = OKX_OrderBook;
   static constexpr auto value = object("code", &T::code, "data", &T::data, "msg", &T::msg);
};

suite long_object = [] {
   "long_object"_test = [] {
      std::string_view order_book_str = R"(
    {"code":"0","data":[{"alias":"","baseCcy":"BTC","category":"1","ctMult":"","ctType":"","ctVal":"",
    "ctValCcy":"","expTime":"","instFamily":"","instId":"BTC-USDT",
    "instType":"SPOT","lever":"10","listTime":"1548133413000","lotSz":"0.00000001","maxIcebergSz":"9999999999",
    "maxLmtSz":"9999999999","maxMktSz":"1000000","maxStopSz":"1000000","maxTriggerSz":"9999999999","maxTwapSz":"9999999999",
    "minSz":"0.00001","optType":"","quoteCcy":"USDT","settleCcy":"","state":"live","stk":"","tickSz":"0.1","uly":""}],
    "msg":""}
)";

      OKX_OrderBook order_book{};
      auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(order_book, order_book_str);
      expect(ec == glz::error_code::none);

      std::string buffer{};
      expect(not glz::write_json(order_book, buffer));
      expect(order_book.data[0].instType == "SPOT");
   };
};

struct A
{
   double x;
   std::vector<uint32_t> y;
   std::vector<std::vector<uint32_t>> z;
};

template <>
struct glz::meta<A>
{
   static constexpr auto value =
      object("x", glz::quoted_num<&A::x>, "y", glz::quoted_num<&A::y>, "z", glz::quoted_num<&A::z>);
};

suite lamda_wrapper = [] {
   "lamda_wrapper"_test = [] {
      A a{3.14, {1, 2, 3}, {{1, 2, 3}}};
      std::string buffer{};
      expect(not glz::write_json(a, buffer));
      expect(buffer == R"({"x":"3.14","y":["1","2","3"],"z":[["1","2","3"]]})");

      buffer = R"({"x":"999.2","y":["4","5","6"],"z":[["4","5"]]})";
      expect(glz::read_json(a, buffer) == glz::error_code::none);
      expect(a.x == 999.2);
      expect(a.y == std::vector<uint32_t>{4, 5, 6});
      expect(a.z == std::vector<std::vector<uint32_t>>{{4, 5}});
   };
   "lamda_wrapper_error_on_missing_keys"_test = [] {
      A a{3.14, {1, 2, 3}, {{1, 2, 3}}};
      std::string buffer{};
      expect(not glz::write_json(a, buffer));
      expect(buffer == R"({"x":"3.14","y":["1","2","3"],"z":[["1","2","3"]]})");

      buffer = R"({"x":"999.2","y":["4","5","6"],"z":[["4","5"]]})";
      auto ec = glz::read<glz::opts{.error_on_missing_keys = true}>(a, buffer);
      expect(ec == glz::error_code::none) << glz::format_error(ec, buffer);
      expect(a.x == 999.2);
      expect(a.y == std::vector<uint32_t>{4, 5, 6});
      expect(a.z == std::vector<std::vector<uint32_t>>{{4, 5}});
   };
};

struct map_quoted_num
{
   std::map<uint32_t, uint64_t> x;
};

template <>
struct glz::meta<map_quoted_num>
{
   static constexpr auto value = object("x", glz::quoted_num<&map_quoted_num::x>);
};

suite quote_map = [] {
   "map_quoted_num"_test = [] {
      map_quoted_num a{{{1, 2}}};
      std::string buffer{};
      expect(not glz::write_json(a, buffer));
      expect(buffer == R"({"x":{"1":"2"}})");

      a = {};
      buffer = R"({"x":{"3":"4"}})";
      expect(glz::read_json(a, buffer) == glz::error_code::none);
      expect(a.x == std::map<uint32_t, uint64_t>{{3, 4}});
   };
};

struct nullable_quoted_num_t
{
   std::optional<int> i{};

   struct glaze
   {
      using T = nullable_quoted_num_t;
      static constexpr auto value = glz::object("i", glz::quoted_num<&T::i>);
   };
};

suite nullable_quoted_num = [] {
   "nullable_quoted_num"_test = [] {
      std::string_view json = R"({"i":"42"})";
      nullable_quoted_num_t obj{};
      expect(!glz::read_json(obj, json));
      expect(obj.i.value() == 42);

      json = R"({"i":null})";
      expect(!glz::read_json(obj, json));
      expect(!obj.i.has_value());

      json = R"({"i":""})";
      expect(glz::read_json(obj, json) == glz::error_code::parse_number_failure);
   };

   "nullable_quoted_num error_on_missing_keys"_test = [] {
      std::string_view json = R"({})";
      nullable_quoted_num_t obj{};
      expect(!glz::read<glz::opts{.error_on_missing_keys = true}>(obj, json));
      expect(!obj.i.has_value());
   };

   "nullable_quoted_num null value"_test = [] {
      nullable_quoted_num_t obj{};
      expect(glz::write_json(obj) == R"({})");
   };
};

struct bool_map
{
   std::map<bool, std::string> x;
};

template <>
struct glz::meta<bool_map>
{
   static constexpr auto value = object("x", &bool_map::x);
};

suite map_with_bool_key = [] {
   "bool_map"_test = [] {
      bool_map a{{{true, "true"}}};
      std::string buffer{};
      expect(not glz::write_json(a, buffer));
      expect(buffer == R"({"x":{"true":"true"}})");

      a = {};
      buffer = R"({"x":{"false":"false"}})";
      expect(glz::read_json(a, buffer) == glz::error_code::none);
      expect(a.x == std::map<bool, std::string>{{false, "false"}});
   };
};

struct array_map
{
   std::map<std::array<int, 3>, std::string> x;
};

template <>
struct glz::meta<array_map>
{
   static constexpr auto value = object("x", &array_map::x);
};

struct custom_key_map
{
   struct key_type
   {
      int field1{};
      std::string field2{};

      [[nodiscard]] std::strong_ordering operator<=>(const key_type&) const noexcept = default;

      struct glaze
      {
         static constexpr auto value = glz::object("field1", &key_type::field1, "field2", &key_type::field2);
      };
   };

   std::map<key_type, std::string> x;
};

template <>
struct glz::meta<custom_key_map>
{
   static constexpr auto value = object("x", &custom_key_map::x);
};

template <typename Map>
struct Arbitrary_key_test_case
{
   std::string_view name{};
   Map input{};
   std::string_view serialized{};
};

suite arbitrary_key_maps = [] {
   using namespace ut;
   "arbitrary_key_maps"_test = [] {
      auto tester = [](const auto& test_case) {
         const auto& [name, input, serialized] = test_case;
         std::string buffer{};
         expect(not glz::write_json(input, buffer));
         expect(buffer == serialized);

         std::decay_t<decltype(input)> parsed{};
         expect(glz::read_json(parsed, serialized) == glz::error_code::none);
         expect(parsed.x == input.x);
      };

      std::tuple tests{
         Arbitrary_key_test_case<array_map>{.name = "array_map",
                                            .input = {.x = {{{1, 2, 3}, "hello"}, {{4, 5, 6}, "goodbye"}}},
                                            .serialized = R"({"x":{"[1,2,3]":"hello","[4,5,6]":"goodbye"}})"},
         Arbitrary_key_test_case<custom_key_map>{
            .name = "custom_key_map",
            .input = {.x = {{{-1, "k.2"}, "value"}}},
            .serialized = R"({"x":{"{\"field1\":-1,\"field2\":\"k.2\"}":"value"}})",
         }};

      glz::for_each_apply(tester, tests);
   };
};

suite char_array = [] {
   "char array write"_test = [] {
      char arr[12] = "Hello World";

      std::string s{};
      expect(not glz::write_json(arr, s));
      expect(s == R"("Hello World")");

      char arr2[12] = "Hello\0World";

      expect(not glz::write_json(arr2, s));
      expect(s == R"("Hello")");
   };

   "char array read"_test = [] {
      char arr[12];
      std::string s = R"("Hello World")";
      expect(glz::read_json(arr, s) == glz::error_code::none);
      expect(std::string(arr) == "Hello World");

      s = R"("Hello")";
      expect(glz::read_json(arr, s) == glz::error_code::none);
      expect(std::string(arr) == "Hello");

      s = R"("Text that is too long")";
      expect(glz::read_json(arr, s) != glz::error_code::none);

      s = R"("Hello WorldX")";
      expect(glz::read_json(arr, s) != glz::error_code::none);
   };
};

struct nullable_keys
{
   double req{};
   std::optional<double> opt{};
   double req2{};
   std::optional<double> opt2{};
};

template <>
struct glz::meta<nullable_keys>
{
   using T = nullable_keys;
   static constexpr auto value = object("req", &T::req, "opt", &T::opt, "req2", &T::req2, "opt2", &T::opt2);
};

suite required_keys = [] {
   "required_keys"_test = [] {
      std::string buffer{};
      my_struct obj{};

      buffer = R"({"i":287,"d":3.14,"hello":"Hello World","arr":[1,2,3]})";
      expect(glz::read_json(obj, buffer) == glz::error_code::none);
      expect(glz::read<glz::opts{.error_on_missing_keys = true}>(obj, buffer) == glz::error_code::none);
      buffer = R"({"d":3.14,"arr":[1,2,3],"hello":"Hello World","i":287})";
      expect(glz::read_json(obj, buffer) == glz::error_code::none);
      expect(glz::read<glz::opts{.error_on_missing_keys = true}>(obj, buffer) == glz::error_code::none);
      buffer = R"({"d":3.14,"hello":"Hello World","arr":[1,2,3]})";
      expect(glz::read_json(obj, buffer) == glz::error_code::none);
      expect(glz::read<glz::opts{.error_on_missing_keys = true}>(obj, buffer) != glz::error_code::none);
      buffer = R"({"i":287,"hello":"Hello World","arr":[1,2,3]})";
      expect(glz::read_json(obj, buffer) == glz::error_code::none);
      expect(glz::read<glz::opts{.error_on_missing_keys = true}>(obj, buffer) != glz::error_code::none);
      buffer = R"({"i":287,"d":3.14,"arr":[1,2,3]})";
      expect(glz::read_json(obj, buffer) == glz::error_code::none);
      expect(glz::read<glz::opts{.error_on_missing_keys = true}>(obj, buffer) != glz::error_code::none);
      buffer = R"({"i":287,"d":3.14,"hello":"Hello World"})";
      expect(glz::read_json(obj, buffer) == glz::error_code::none);
      expect(glz::read<glz::opts{.error_on_missing_keys = true}>(obj, buffer) != glz::error_code::none);
   };

   "required_keys_with_nullable"_test = [] {
      std::string buffer{};
      nullable_keys obj{};

      buffer = R"({"req": 0, "opt": null, "req2": 0, "opt2": 0})";
      expect(glz::read_json(obj, buffer) == glz::error_code::none);
      expect(glz::read<glz::opts{.error_on_missing_keys = true}>(obj, buffer) == glz::error_code::none);
      buffer = R"({"req": 0, "opt": null, "opt2": 0})";
      expect(glz::read_json(obj, buffer) == glz::error_code::none);
      expect(glz::read<glz::opts{.error_on_missing_keys = true}>(obj, buffer) == glz::error_code::missing_key);
      buffer = R"({"opt": null, "req2": 0, "opt2": 0})";
      expect(glz::read_json(obj, buffer) == glz::error_code::none);
      expect(glz::read<glz::opts{.error_on_missing_keys = true}>(obj, buffer) == glz::error_code::missing_key);
      buffer = R"({"req": 0, "req2": 0, "opt2": 0})";
      expect(glz::read_json(obj, buffer) == glz::error_code::none);
      expect(glz::read<glz::opts{.error_on_missing_keys = true}>(obj, buffer) == glz::error_code::none);
      buffer = R"({"req": 0, "req2": 0})";
      expect(glz::read_json(obj, buffer) == glz::error_code::none);
      expect(glz::read<glz::opts{.error_on_missing_keys = true}>(obj, buffer) == glz::error_code::none);
   };

   "required_keys_long_object"_test = [] {
      std::string_view order_book_str = R"(
         {"code":"0","data":[{"alias":"","baseCcy":"BTC","category":"1","ctMult":"","ctType":"","ctVal":"",
         "ctValCcy":"","expTime":"","instFamily":"","instId":"BTC-USDT",
         "instType":"SPOT","lever":"10","listTime":"1548133413000","lotSz":"0.00000001","maxIcebergSz":"9999999999",
         "maxLmtSz":"9999999999","maxMktSz":"1000000","maxStopSz":"1000000","maxTriggerSz":"9999999999","maxTwapSz":"9999999999",
         "minSz":"0.00001","optType":"","quoteCcy":"USDT","settleCcy":"","state":"live","stk":"","tickSz":"0.1","uly":""}],
         "msg":""}
      )";

      OKX_OrderBook order_book{};
      auto ec = glz::read<glz::opts{.error_on_unknown_keys = false, .error_on_missing_keys = true}>(order_book,
                                                                                                    order_book_str);
      expect(ec == glz::error_code::none);

      std::string_view order_book_str_missing = R"(
         {"code":"0","data":[{"alias":"","baseCcy":"BTC","ctMult":"","ctType":"","ctVal":"",
         "ctValCcy":"","expTime":"","instFamily":"","instId":"BTC-USDT",
         "instType":"SPOT","lever":"10","listTime":"1548133413000","lotSz":"0.00000001","maxIcebergSz":"9999999999",
         "maxLmtSz":"9999999999","maxMktSz":"1000000","maxStopSz":"1000000","maxTriggerSz":"9999999999","maxTwapSz":"9999999999",
         "minSz":"0.00001","optType":"","quoteCcy":"USDT","settleCcy":"","state":"live","stk":"","tickSz":"0.1","uly":""}],
         "msg":""}
      )";
      ec = glz::read<glz::opts{.error_on_unknown_keys = false, .error_on_missing_keys = true}>(order_book,
                                                                                               order_book_str_missing);
      expect(ec == glz::error_code::missing_key);
   };

   "required_keys_format_error"_test = [] {
      my_struct obj{};
      std::string buffer = R"({"i":287,"hello":"Hello World","arr":[1,2,3]})";
      auto err = glz::read<glz::opts{.error_on_missing_keys = true}>(obj, buffer);
      expect(bool(err));
      auto err_msg = glz::format_error(err, buffer);
      expect(err_msg == R"(1:45: missing_key
   {"i":287,"hello":"Hello World","arr":[1,2,3]}
                                               ^ d)")
         << err_msg;

      buffer = R"({"i":287,"d":0.0,"arr":[1,2,3]})";
      err = glz::read<glz::opts{.error_on_missing_keys = true}>(obj, buffer);
      expect(bool(err));
      err_msg = glz::format_error(err, buffer);
      expect(err_msg == R"(1:31: missing_key
   {"i":287,"d":0.0,"arr":[1,2,3]}
                                 ^ hello)")
         << err_msg;

      std::vector<my_struct> vec{};
      buffer = R"([{"i":287,"d":0.0,"arr":[1,2,3]}])";
      err = glz::read<glz::opts{.error_on_missing_keys = true}>(vec, buffer);
      expect(bool(err));
      err_msg = glz::format_error(err, buffer);
      expect(err_msg ==
             R"(1:32: missing_key
   [{"i":287,"d":0.0,"arr":[1,2,3]}]
                                  ^ hello)")
         << err_msg;
   };

   "required_keys_format_error_issue1866"_test = [] {
      std::vector<Issue1866> exports;
      std::vector<Issue1866> gen;
      std::string buffer{
         R"([{"uniqueName": "/Lotus/Characters/TwinQueens","name": "Twin Queens","description": "Rulers of the Grineer with their own banner.","codexSecret": false,"parentName": "/Lotus/Characters"}])"};

      std::string buffer1{
         R"([{"uniqueName": "/Lotus/Characters/TwinQueens","name": "Twin Queens","description": "Rulers of the Grineer with their own banner.","codexSecret": false}])"};
      auto er = glz::read<glz::opts{.error_on_missing_keys = true}>(exports, buffer);
      expect(!er);

      auto er1 = glz::read<glz::opts{.error_on_missing_keys = true}>(gen, buffer1);
      expect(er1);
      expect(er1.custom_error_message == "parentName");
   };
};

struct numbers_as_strings
{
   std::string x{};
   std::string y{};
};

template <>
struct glz::meta<numbers_as_strings>
{
   using T = numbers_as_strings;
   static constexpr auto value = object("x", glz::number<&T::x>, "y", glz::number<&T::y>);
};

struct numbers_as_strings2
{
   std::string i{};
   std::string d{};
   std::string hello{};
};

template <>
struct glz::meta<numbers_as_strings2>
{
   using T = numbers_as_strings2;
   static constexpr auto value = object("i", number<&T::i>, "d", number<&T::d>, &T::hello);
};

suite numbers_as_strings_suite = [] {
   "numbers_as_strings"_test = [] {
      numbers_as_strings obj{};

      std::string input = R"({"x":555,"y":3.14})";
      expect(!glz::read_json(obj, input));
      expect(obj.x == "555");
      expect(obj.y == "3.14");

      std::string output;
      expect(not glz::write_json(obj, output));
      expect(input == output);
   };

   "numbers_as_strings2"_test = [] {
      const std::string buffer = R"({"i":287,"d":3.14,"hello":"Hello World"})";

      numbers_as_strings2 value{};
      const auto error_ctx = glz::read_json(value, buffer);
      expect(not error_ctx) << glz::format_error(error_ctx, buffer);
      expect(value.i == "287");
      expect(value.d == "3.14");
   };
};

enum struct MyEnum { VALUE_1 = 200, VALUE_2 = 300, VALUE_3 = 400, UNUSED_VALUE = 500 };

suite numeric_enums_suite = [] {
   "numeric_enums"_test = [] {
      std::vector<MyEnum> v{};

      std::string input = R"([200, 300, 400])";
      expect(glz::read_json(v, input) == glz::error_code::none);
      expect(v[0] == MyEnum::VALUE_1);
      expect(v[1] == MyEnum::VALUE_2);
      expect(v[2] == MyEnum::VALUE_3);
   };
};

struct data_type
{
   std::string hello;
};

suite json_logging = [] {
   "json_logging"_test = [] {
      glz::arr vec = {1, 2, 3};
      glz::obj map = {"a", 1, "b", 2, "c", 3};
      auto obj = glz::obj{
         "pi", 3.141, "happy", true, "name", "Stephen", "map", map, "arr", glz::arr{"Hello", "World", 2}, "vec", vec};

      glz::get<0>(map.value) = "aa"; // testing lvalue reference storage

      std::string s{};
      expect(not glz::write_json(obj, s));

      expect(
         s ==
         R"({"pi":3.141,"happy":true,"name":"Stephen","map":{"aa":1,"b":2,"c":3},"arr":["Hello","World",2],"vec":[1,2,3]})")
         << s;
   };

   "json_custom_logging"_test = [] {
      std::vector vec = {1, 2, 3};
      std::map<std::string_view, int> map = {{"a", 1}, {"b", 2}, {"c", 3}};
      auto obj =
         glz::obj{"pi", 3.141, "happy", true, "name", "Stephen", "map", map, "vec", vec, "my_struct", my_struct{}};

      map["a"] = 0; // testing lvalue reference storage

      std::string s{};
      expect(not glz::write_json(obj, s));

      expect(
         s ==
         R"({"pi":3.141,"happy":true,"name":"Stephen","map":{"a":0,"b":2,"c":3},"vec":[1,2,3],"my_struct":{"i":287,"d":3.14,"hello":"Hello World","arr":[1,2,3]}})")
         << s;
   };

   "merge_obj"_test = [] {
      glz::obj obj0{"pi", 3.141};
      glz::obj obj1{"happy", true};
      auto merged = glz::merge{obj0, obj1, glz::obj{"arr", glz::arr{"Hello", "World", 2}}};
      glz::get<0>(obj0.value) = "pie"; // testing that we have an lvalue reference
      std::string s{};
      expect(not glz::write_json(merged, s));

      expect(s == R"({"pie":3.141,"happy":true,"arr":["Hello","World",2]})") << s;
   };

   "merge_custom"_test = [] {
      glz::obj obj0{"pi", 3.141};
      std::map<std::string_view, int> map = {{"a", 1}, {"b", 2}, {"c", 3}};
      auto merged = glz::merge{obj0, map, my_struct{}};
      map["a"] = 0; // testing lvalue reference storage
      std::string s{};
      expect(not glz::write_json(merged, s));

      expect(s == R"({"pi":3.141,"a":0,"b":2,"c":3,"i":287,"d":3.14,"hello":"Hello World","arr":[1,2,3]})") << s;
   };

   "merged potentiallyContainsUnknownJSON"_test = [] {
      data_type data{};
      std::vector<int> my_arr;
      std::map<std::string_view, glz::raw_json_view> potentiallyContainsUnknownJSON;

      data.hello = "goodbye";
      std::string buffer{};
      expect(not glz::write_json(glz::merge{glz::obj{"my_arr", my_arr, "data", data}, potentiallyContainsUnknownJSON},
                                 buffer));

      expect(buffer == R"({"my_arr":[],"data":{"hello":"goodbye"}})") << buffer;
   };
};

struct non_cx_values
{
   std::string_view info{"information"};
   int index{42};
   std::string value{};
};

static_assert(std::is_same_v<glz::member_t<non_cx_values, decltype(&non_cx_values::info)>, std::string_view&>);

struct cx_values
{
   static constexpr std::string_view info{"information"};
   static constexpr auto index{42};
   std::string value{};
};

static_assert(std::is_same_v<glz::member_t<cx_values, decltype(&cx_values::info)>, const std::string_view&>);

template <>
struct glz::meta<cx_values>
{
   using T = cx_values;
   static constexpr auto value{glz::object("info", &T::info, "index", &T::index, "value", &T::value)};
};

struct direct_cx_value_conversion
{
   static constexpr std::uint64_t const_v{42};
   struct glaze
   {
      static constexpr auto value{&direct_cx_value_conversion::const_v};
   };
};
static_assert(glz::glaze_const_value_t<direct_cx_value_conversion>);

struct direct_cx_value_conversion_different_value
{
   static constexpr std::uint64_t const_v{1337};
   struct glaze
   {
      static constexpr auto value{&direct_cx_value_conversion_different_value::const_v};
   };
};
static_assert(glz::glaze_const_value_t<direct_cx_value_conversion_different_value>);

struct string_direct_cx_value_conversion
{
   static constexpr std::string_view const_v{"other"};
   struct glaze
   {
      static constexpr auto value{&string_direct_cx_value_conversion::const_v};
   };
};
static_assert(glz::glaze_const_value_t<string_direct_cx_value_conversion>);

struct string_two_direct_cx_value_conversion
{
   static constexpr std::string_view const_v{"two"};
   struct glaze
   {
      static constexpr auto value{&string_two_direct_cx_value_conversion::const_v};
   };
};
static_assert(glz::glaze_const_value_t<string_two_direct_cx_value_conversion>);

struct array_direct_cx_value_conversion
{
   static constexpr std::array<std::string_view, 2> const_v{"one", "two"};
   struct glaze
   {
      static constexpr auto value{&array_direct_cx_value_conversion::const_v};
   };
};
static_assert(glz::glaze_const_value_t<array_direct_cx_value_conversion>);

struct array_two_direct_cx_value_conversion
{
   static constexpr std::array<std::string_view, 2> const_v{"two", "one"};
   struct glaze
   {
      static constexpr auto value{&array_two_direct_cx_value_conversion::const_v};
   };
};
static_assert(glz::glaze_const_value_t<array_two_direct_cx_value_conversion>);

struct non_cx_direct_value_conversion
{
   std::string some_other{"other"};
   struct glaze
   {
      static constexpr auto value{&non_cx_direct_value_conversion::some_other};
   };
};
static_assert(!glz::glaze_const_value_t<non_cx_direct_value_conversion>);

struct const_red
{
   static constexpr auto const_v{Color::Red};
   struct glaze
   {
      static constexpr auto value{&const_red::const_v};
   };
};

struct const_green
{
   static constexpr auto const_v{Color::Green};
   struct glaze
   {
      static constexpr auto value{&const_green::const_v};
   };
};

template <typename T>
struct variant_to_tuple;
template <typename... Ts>
struct variant_to_tuple<std::variant<Ts...>>
{
   using type = std::tuple<Ts...>;
};
suite constexpr_values_test = [] {
   "constexpr_values_write"_test = [] {
      cx_values obj{};
      std::string s{};
      expect(not glz::write_json(obj, s));
      expect(s == R"({"info":"information","index":42,"value":""})");
   };

   "constexpr_values_read"_test = [] {
      cx_values obj{};
      std::string s = R"({"info":"hello","index":2,"value":"special"})";
      expect(!glz::read_json(obj, s));
      expect(obj.info == "information");
      expect(obj.index == 42);
      expect(obj.value == "special");
   };

   using const_only_variant =
      std::variant<direct_cx_value_conversion_different_value, direct_cx_value_conversion,
                   string_direct_cx_value_conversion, string_two_direct_cx_value_conversion,
                   array_direct_cx_value_conversion, array_two_direct_cx_value_conversion, const_red, const_green>;
   "constexpr blend with non constexpr variant string"_test = [] {
      auto tester = [](auto&& v) {
         using const_t = std::remove_reference_t<decltype(v)>;
         const_only_variant var{v};
         std::string s{};
         expect(not glz::write_json(var, s));
         std::string expected{};
         expect(not glz::write_json(const_t::const_v, expected));
         expect(s == expected) << s;
         auto parse_err{glz::read_json(var, s)};
         expect(parse_err == glz::error_code::none) << glz::format_error(parse_err, s);
         expect(std::holds_alternative<const_t>(var));
      };

      // variant_to_tuple<const_only_variant>::type tests{};

      // glz::for_each_apply(tester, tests);

      tester(string_two_direct_cx_value_conversion{});
   };

   "parse error direct_conversion_variant cx int"_test = [] {
      const_only_variant var{direct_cx_value_conversion{}};
      const auto parse_err{glz::read_json(var, R"(33)")};
      expect(parse_err == glz::error_code::no_matching_variant_type);
   };

   "constexpr blend with non constexpr variant"_test = [] {
      std::variant<std::monostate, direct_cx_value_conversion_different_value, direct_cx_value_conversion,
                   std::uint64_t>
         var{std::uint64_t{111}};
      std::string s{};
      expect(not glz::write_json(var, s));
      expect(s == R"(111)");
      auto parse_err{glz::read_json(var, s)};
      expect(parse_err == glz::error_code::none) << glz::format_error(parse_err, s);
      expect(std::holds_alternative<std::uint64_t>(var));
   };
};

enum class my_enum_type { value_0, value_1 };

struct test_enum_struct
{
   my_enum_type type = my_enum_type::value_1;
};

template <>
struct glz::meta<test_enum_struct>
{
   using T = test_enum_struct;
   static constexpr auto value = object("type", &T::type);
};

suite numeric_enum_tests = [] {
   "numeric_enum"_test = [] {
      test_enum_struct obj{};
      std::string s{};
      expect(not glz::write_json(obj, s));
      expect(s == R"({"type":1})");

      obj.type = my_enum_type::value_0;
      expect(!glz::read_json(obj, s));
      expect(obj.type == my_enum_type::value_1);
   };
};

suite optional_optional = [] {
   "optional_optional"_test = [] {
      std::optional<std::optional<int>> o = std::optional<int>{};
      std::string s{};
      expect(not glz::write_json(o, s));
      expect(s == "null");

      o = {};
      expect(not glz::write_json(o, s));
      expect(s == "null");

      expect(!glz::read_json(o, s));
      expect(!o);

      s = "5";
      expect(!glz::read_json(o, s));
      expect(o.value().value() == 5);
   };
};

struct invoke_struct
{
   int y{};
   std::function<void(int x)> square{};
   void add_one() { ++y; }

   // MSVC requires this constructor for 'this' to be captured
   invoke_struct()
   {
      square = [&](int x) { y = x * x; };
   }
};

template <>
struct glz::meta<invoke_struct>
{
   using T = invoke_struct;
   static constexpr auto value = object("square", invoke<&T::square>, "add_one", invoke<&T::add_one>);
};

suite invoke_test = [] {
   "invoke"_test = [] {
      invoke_struct obj{};
      std::string s = R"(
{
   "square":[5],
   "add_one":[]
})";
      auto ec = glz::read_json(obj, s);
      expect(!ec) << glz::format_error(ec, s);
      expect(obj.y == 26); // 5 * 5 + 1
   };
};

suite char_buffer = [] {
   "null char*"_test = [] {
      char* str{};
      std::string s{};
      expect(not glz::write_json(str, s));
      expect(s == R"("")");
   };

   "char*"_test = [] {
      std::string str = "Spiders";
      char* ptr = str.data();
      std::string s{};
      expect(not glz::write_json(ptr, s));
      expect(s == R"("Spiders")");
   };
};

static_assert(!glz::char_array_t<char*>);

suite enum_map = [] {
   "enum map key"_test = [] {
      std::map<Color, std::string> color_map{{Color::Red, "red"}, {Color::Green, "green"}, {Color::Blue, "blue"}};

      std::string s{};
      expect(not glz::write_json(color_map, s));

      expect(s == R"({"Red":"red","Green":"green","Blue":"blue"})");

      color_map.clear();
      expect(!glz::read_json(color_map, s));
      expect(color_map.at(Color::Red) == "red");
      expect(color_map.at(Color::Green) == "green");
      expect(color_map.at(Color::Blue) == "blue");
   };

   "enum map key vector pair concatenate"_test = [] {
      std::vector<std::pair<Color, std::string>> colors{
         {Color::Red, "red"}, {Color::Green, "green"}, {Color::Blue, "blue"}};

      std::string s{};
      expect(not glz::write_json(colors, s));

      expect(s == R"({"Red":"red","Green":"green","Blue":"blue"})");

      auto expected = colors;
      colors.clear();
      expect(!glz::read_json(colors, s));
      expect(colors == expected);
   };

   "enum map value"_test = [] {
      std::map<int, Color> color_map{{0, Color::Red}, {1, Color::Green}, {2, Color::Blue}};

      std::string s{};
      expect(not glz::write_json(color_map, s));

      expect(s == R"({"0":"Red","1":"Green","2":"Blue"})");

      auto expectedMap = color_map;
      color_map.clear();
      expect(!glz::read_json(color_map, s));
      expect(expectedMap == color_map);
   };

   "enum map value vector pair concatenate"_test = [] {
      std::vector<std::pair<int, Color>> colors{{0, Color::Red}, {1, Color::Green}, {2, Color::Blue}};

      std::string s{};
      expect(not glz::write_json(colors, s));
      expect(s == R"({"0":"Red","1":"Green","2":"Blue"})");

      auto expected = colors;
      colors.clear();
      expect(!glz::read_json(colors, s));
      expect(colors == expected);
   };
};

suite obj_handling = [] {
   "obj handling"_test = [] {
      size_t cnt = 0;
      glz::obj o{"count", size_t{cnt}};
      std::vector<std::decay_t<decltype(o)>> vec;
      for (; cnt < 10; ++cnt) {
         vec.emplace_back(glz::obj{"count", size_t{cnt}});
      }
      for (size_t i = 0; i < vec.size(); ++i) {
         expect(i == glz::get<1>(vec[i].value));
      }
   };

   "obj_copy handling"_test = [] {
      size_t cnt = 0;
      glz::obj_copy o{"cnt", cnt};
      std::vector<std::decay_t<decltype(o)>> vec;
      for (; cnt < 5; ++cnt) {
         vec.emplace_back(glz::obj_copy{"cnt", cnt});
      }
      for (size_t i = 0; i < vec.size(); ++i) {
         expect(i == glz::get<1>(vec[i].value));
      }

      auto s = glz::write_json(vec).value_or("error");
      expect(s == R"([{"cnt":0},{"cnt":1},{"cnt":2},{"cnt":3},{"cnt":4}])") << s;
   };
};

suite obj_nested_merge = [] {
   "obj_nested_merge"_test = [] {
      glz::obj o{"not", "important"};
      glz::obj o2{"map", glz::obj{"a", 1, "b", 2, "c", 3}};
      auto merged = glz::merge{o, o2};
      std::string s{};
      expect(not glz::write_json(merged, s));
      expect(s == R"({"not":"important","map":{"a":1,"b":2,"c":3}})") << s;
   };

   "obj_generic_merge"_test = [] {
      glz::generic json;
      expect(
         !glz::read_json(json, "{\"key1\":42,\"key2\":\"hello world\",\"v\":[1,2,3],\"m\":{\"a\":1,\"b\":2,\"c\":3}}"));
      glz::obj obj{"not", "important"};
      auto s = glz::write_json(glz::merge{obj, json}).value_or("error");
      expect(s == R"({"not":"important","key1":42,"key2":"hello world","m":{"a":1,"b":2,"c":3},"v":[1,2,3]})") << s;
   };
};

suite write_to_map = [] {
   "write_obj_to_map"_test = [] {
      std::map<std::string, glz::raw_json> map;
      glz::obj obj{"arr", glz::arr{1, 2, 3}, "hello", "world"};
      using T = std::decay_t<decltype(obj.value)>;
      glz::for_each<glz::tuple_size_v<T>>([&]<auto I>() {
         if constexpr (I % 2 == 0) {
            map[std::string(glz::get<I>(obj.value))] = glz::write_json(glz::get<I + 1>(obj.value)).value();
         }
      });

      auto s = glz::write_json(map).value_or("error");
      expect(s == R"({"arr":[1,2,3],"hello":"world"})") << s;
   };

   "write_generic_to_map"_test = [] {
      std::map<std::string, glz::raw_json> map;

      glz::generic obj{{"arr", {1, 2, 3}}, {"hello", "world"}};
      auto& o = obj.get<glz::generic::object_t>();
      for (auto& [key, value] : o) {
         map[key] = glz::write_json(value).value_or("error");
      }

      auto s = glz::write_json(map).value_or("error");
      expect(s == R"({"arr":[1,2,3],"hello":"world"})") << s;
   };
};

suite negatives_with_unsiged = [] {
   "negatives_with_unsiged"_test = [] {
      uint8_t x8{};
      std::string s = "-8";
      expect(glz::read_json(x8, s) == glz::error_code::parse_number_failure);

      uint16_t x16{};
      expect(glz::read_json(x16, s) == glz::error_code::parse_number_failure);

      uint32_t x32{};
      expect(glz::read_json(x32, s) == glz::error_code::parse_number_failure);

      uint64_t x64{};
      expect(glz::read_json(x64, s) == glz::error_code::parse_number_failure);

      s = "  -8";
      expect(glz::read_json(x64, s) == glz::error_code::parse_number_failure);

      s = "  -  8";
      expect(glz::read_json(x64, s) == glz::error_code::parse_number_failure);
   };
};

suite integer_over_under_flow = [] {
   "integer_over_under_flow"_test = [] {
      int8_t x8{};
      std::string s = "300";
      expect(glz::read_json(x8, s) == glz::error_code::parse_number_failure);

      s = "-300";
      expect(glz::read_json(x8, s) == glz::error_code::parse_number_failure);

      int16_t x16{};
      s = "209380980";
      expect(glz::read_json(x16, s) == glz::error_code::parse_number_failure);

      s = "-209380980";
      expect(glz::read_json(x16, s) == glz::error_code::parse_number_failure);

      int32_t x32{};
      s = "4294967297";
      expect(glz::read_json(x32, s) == glz::error_code::parse_number_failure);

      s = "-4294967297";
      expect(glz::read_json(x32, s) == glz::error_code::parse_number_failure);
   };
};

suite number_reading = [] {
   "long float"_test = [] {
      std::string_view buffer{"0.00666666666666666600"};
      int i{5};
      expect(glz::read_json(i, buffer));

      buffer = "0.0000666666666666666600";
      expect(glz::read_json(i, buffer));

      buffer = "0.00000000000000000000000";
      expect(glz::read_json(i, buffer));

      buffer = "6E19";
      expect(glz::read_json(i, buffer) == glz::error_code::parse_number_failure);

      buffer = "e5555511116";
      expect(glz::read_json(i, buffer) == glz::error_code::parse_number_failure);
   };

   "long float uint64_t"_test = [] {
      std::string_view buffer{"0.00666666666666666600"};
      uint64_t i{5};
      expect(glz::read_json(i, buffer));

      buffer = "0.0000666666666666666600";
      expect(glz::read_json(i, buffer));

      buffer = "0.00000000000000000000000";
      expect(glz::read_json(i, buffer));

      buffer = "6E19";
      expect(glz::read_json(i, buffer) == glz::error_code::parse_number_failure);

      buffer = "0.1e999999999999999999";
      expect(glz::read_json(i, buffer) == glz::error_code::parse_number_failure);

      buffer = "0.1e-999999999999999999";
      expect(glz::read_json(i, buffer));
   };

   "long float double"_test = [] {
      std::string_view buffer{"0.00000000000000000000000"};
      double d{3.14};
      expect(!glz::read_json(d, buffer));
      expect(d == 0.0);
   };

   "minimum int32_t"_test = [] {
      std::string buffer{"-2147483648"};
      int32_t i{};
      expect(!glz::read_json(i, buffer));
      expect(i == (std::numeric_limits<int32_t>::min)());

      expect(not glz::write_json(i, buffer));
      expect(buffer == "-2147483648");
   };

   "minimum int64_t"_test = [] {
      std::string buffer{"-9223372036854775808"};
      int64_t i{};
      expect(!glz::read_json(i, buffer));
      expect(i == (std::numeric_limits<int64_t>::min)());

      expect(not glz::write_json(i, buffer));
      expect(buffer == "-9223372036854775808");
   };
};

suite whitespace_testing = [] {
   "whitespace error"_test = [] {
      std::string_view buffer{"{\"0\"/\n/"};
      my_struct value{};
      glz::context ctx{};
      expect(glz::read_json(value, buffer) == glz::error_code::unknown_key);
   };
};

suite write_as_json_raw = [] {
   "write_as_json_raw"_test = [] {
      static std::array<char, 128> b{};
      my_struct obj{};
      expect(glz::write_as_json(obj, "/i", b.data()));
      expect(std::string_view{b.data()} == "287");
   };
};

suite const_read_error = [] {
   "const_read_error"_test = [] {
      const std::string hello = "world";
      std::string s = R"(explode)";
      constexpr glz::opts opts{.error_on_const_read = true};
      expect(glz::read<opts>(hello, s) == glz::error_code::attempt_const_read);
   };
};

struct test_mapping_t
{
   int64_t id;
   double latitude;
   double longitude;
};

struct coordinates_t
{
   double* latitude;
   double* longitude;
};

template <>
struct glz::meta<coordinates_t>
{
   using T = coordinates_t;
   static constexpr auto value = object("latitude", &T::latitude, "longitude", &T::longitude);
};

template <>
struct glz::meta<test_mapping_t>
{
   using T = test_mapping_t;
   static constexpr auto value =
      object("id", &T::id, "coordinates", [](auto& self) { return coordinates_t{&self.latitude, &self.longitude}; });
};

suite mapping_struct = [] {
   "mapping_struct"_test = [] {
      test_mapping_t obj{};
      std::string s = R"({
  "id": 12,
  "coordinates": {
    "latitude": 1.23456789,
    "longitude": 9.87654321
  }
})";
      expect(!glz::read_json(obj, s));
      expect(obj.id == 12);
      expect(obj.latitude == 1.23456789);
      expect(obj.longitude == 9.87654321);
   };
};

struct name_t
{
   std::string first{};
   std::string last{};
};

suite error_message_test = [] {
   "error_message"_test = [] {
      std::vector<name_t> arr{};
      std::string s =
         R"([{"first":"George","last":"Martin"},{"first":"Sally","last":"Adams"},{"first":"Caleb","middle":"Patrick","last":"Boardwalk"},{"first":"James","last":"Brown"}])";
      const auto error = glz::read_json(arr, s);
      expect(error == glz::error_code::unknown_key) << glz::format_error(error, s);
   };
};

struct Person
{
   std::string name{};
   int age{};
   std::string city{};
   std::string residence{};

   void getAge(const std::string /*birthdateStr*/)
   {
      // Example code is commented out to avoid unit tests breaking as the date changes
      /*std::tm birthdate = {};
      std::istringstream ss(birthdateStr);
      ss >> std::get_time(&birthdate, "%d/%m/%Y");

      if (ss.fail()) {
         std::cout << "Failed to parse birthdate: " << birthdateStr << "\n";
      }

      const auto birth = std::chrono::system_clock::from_time_t(std::mktime(&birthdate));
      const auto age_seconds =
         std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - birth);

      age = static_cast<int>(age_seconds.count()) / (60 * 60 * 24 * 365);*/
      age = 33;
   }
};

template <>
struct glz::meta<Person>
{
   using T = Person;
   static constexpr auto value =
      glz::object("name", &T::name, "full_name", &T::name, "age", &T::age, "years_old", &T::age, "date_of_birth",
                  invoke<&T::getAge>, "city", &T::city, "residence", &T::residence);
};

suite function_call = [] {
   "function_call"_test = [] {
      Person obj{};
      std::string s = R"({
            "full_name": "Brian Smith",
            "date_of_birth": ["01/01/1990"],
            "residence": "San Francisco"
        })";
      expect(!glz::read_json(obj, s));
      expect(obj.age == 33);
   };
};

struct named_always_null : std::monostate
{};
template <>
struct glz::meta<named_always_null>
{
   static constexpr std::string_view name = "named_always_null";
};
suite nullable_type = [] { "named_always_null"_test = [] { expect("null" == glz::write_json(named_always_null{})); }; };

struct pointer_wrapper
{
   std::unique_ptr<int> x = std::make_unique<int>(5);

   struct glaze
   {
      using T = pointer_wrapper;
      static constexpr auto value = glz::object("x", [](auto& self) { return self.x.get(); });
   };
};

suite pointer_wrapper_test = [] {
   "pointer_wrapper"_test = [] {
      pointer_wrapper obj{};
      std::string s = R"({"x": 3})";
      expect(!glz::read_json(obj, s));
      expect(*obj.x == 3);
   };
};

struct custom_encoding
{
   uint64_t x{};
   std::string y{};
   std::array<uint32_t, 3> z{};

   void read_x(const std::string& s) { x = std::stoi(s); }

   uint64_t write_x() { return x; }

   void read_y(const std::string& s) { y = "hello" + s; }

   auto& write_z()
   {
      z[0] = 5;
      return z;
   }
};

template <>
struct glz::meta<custom_encoding>
{
   using T = custom_encoding;
   static constexpr auto value = object("x", custom<&T::read_x, &T::write_x>, //
                                        "y", custom<&T::read_y, &T::y>, //
                                        "z", custom<&T::z, &T::write_z>);
};

suite custom_encoding_test = [] {
   "custom_reading"_test = [] {
      custom_encoding obj{};
      std::string s = R"({"x":"3","y":"world","z":[1,2,3]})";
      expect(!glz::read_json(obj, s));
      expect(obj.x == 3);
      expect(obj.y == "helloworld");
      expect(obj.z == std::array<uint32_t, 3>{1, 2, 3});
   };

   "custom_writing"_test = [] {
      custom_encoding obj{};
      std::string s = R"({"x":"3","y":"world","z":[1,2,3]})";
      expect(!glz::read_json(obj, s));
      std::string out{};
      expect(not glz::write_json(obj, out));
      expect(out == R"({"x":3,"y":"helloworld","z":[5,2,3]})");
   };
};

struct custom_load_t
{
   std::vector<int> x{};
   std::vector<int> y{};

   struct glaze
   {
      static constexpr auto read_x = [](auto& s) -> auto& { return s.x; };
      static constexpr auto write_x = [](auto& s) -> auto& { return s.y; };
      static constexpr auto value = glz::object("x", glz::custom<read_x, write_x>);
   };
};

suite custom_load_test = [] {
   "custom_load"_test = [] {
      custom_load_t obj{};
      std::string s = R"({"x":[1,2,3]})";
      expect(!glz::read_json(obj, s));
      expect(obj.x[0] == 1);
      expect(obj.x[1] == 2);
      expect(obj.x[2] == 3);
      s.clear();
      expect(not glz::write_json(obj, s));
      expect(s == R"({"x":[]})");
      expect(obj.x[0] == 1);
      expect(obj.x[1] == 2);
      expect(obj.x[2] == 3);
   };
};

struct custom_buffer_input
{
   std::string str{};
};

template <>
struct glz::meta<custom_buffer_input>
{
   static constexpr auto read_x = [](custom_buffer_input& s, const std::string& input) { s.str = input; };
   static constexpr auto write_x = [](auto& s) -> auto& { return s.str; };
   static constexpr auto value = glz::object("str", glz::custom<read_x, write_x>);
};

suite custom_buffer_input_test = [] {
   "custom_buffer_input"_test = [] {
      custom_buffer_input obj{};
      std::string s = R"({"str":"Hello!"})";
      expect(!glz::read_json(obj, s));
      expect(obj.str == "Hello!");
      s.clear();
      expect(not glz::write_json(obj, s));
      expect(s == R"({"str":"Hello!"})");
      expect(obj.str == "Hello!");
   };
};

struct age_custom_error_obj
{
   int age{};
};

template <>
struct glz::meta<age_custom_error_obj>
{
   using T = age_custom_error_obj;
   static constexpr auto read_x = [](T& s, int age, glz::context& ctx) {
      if (age < 21) {
         ctx.error = glz::error_code::constraint_violated;
         ctx.custom_error_message = "age too young";
      }
      else {
         s.age = age;
      }
   };
   static constexpr auto value = object("age", glz::custom<read_x, &T::age>);
};

suite custom_error_tests = [] {
   "age_custom_error_obj"_test = [] {
      age_custom_error_obj obj{};
      std::string s = R"({"age":18})";
      auto ec = glz::read_json(obj, s);
      auto err_msg = glz::format_error(ec, s);
      expect(bool(ec)) << err_msg;
      // std::cout << err_msg << '\n';
      expect(err_msg == "1:10: constraint_violated\n   {\"age\":18}\n            ^ age too young");

      expect(not glz::write_json(obj, s));
      expect(s == R"({"age":0})");

      obj.age = 21;
      expect(not glz::write_json(obj, s));
      expect(s == R"({"age":21})");

      obj.age = 0;

      expect(not glz::read_json(obj, s));
      expect(obj.age == 21);
   };
};

class class_with_const_mem_func
{
  public:
   int get_i() const { return i; }
   void set_i(int I) { i = I; }

  private:
   int i = 0;
};

template <>
struct glz::meta<class_with_const_mem_func>
{
   using T = class_with_const_mem_func;
   static constexpr auto value = object("i", custom<&T::set_i, &T::get_i>);
};

suite const_mem_func_tests = [] {
   "const_mem_func"_test = [] {
      class_with_const_mem_func obj{};
      std::string s = R"({"i":55})";
      expect(!glz::read_json(obj, s));
      expect(obj.get_i() == 55);
      s.clear();
      expect(not glz::write_json(obj, s));
      expect(s == R"({"i":55})");
   };
};

struct constrained_object
{
   int age{};
   std::string name{};
};

template <>
struct glz::meta<constrained_object>
{
   using T = constrained_object;
   static constexpr auto limit_age = [](const T&, int age) { return (age >= 0 && age <= 120); };

   static constexpr auto limit_name = [](const T&, const std::string& name) { return name.size() <= 8; };

   static constexpr auto value = object("age", read_constraint<&T::age, limit_age, "Age out of range">, //
                                        "name", read_constraint<&T::name, limit_name, "Name is too long">);
};

struct constrained_optional_object
{
   std::string name{};
   std::optional<std::vector<int>> data{};
};

template <>
struct glz::meta<constrained_optional_object>
{
   using T = constrained_optional_object;

   static constexpr auto validate_data = [](const T&, const std::optional<std::vector<int>>& values) {
      if (!values) {
         return true;
      }
      for (const auto v : *values) {
         if (v < -1) {
            return false;
         }
      }
      return true;
   };

   static constexpr auto value = object("name", &T::name, //
                                        "data", read_constraint<&T::data, validate_data, "invalid data value">);
};

suite constraint_tests = [] {
   "constrained_object"_test = [] {
      constrained_object obj{};

      expect(not glz::read_json(obj, R"({"age": 25, "name": "José"})"));
      expect(obj.age == 25);
      expect(obj.name == "José");

      std::string buffer = R"({"age": -1, "name": "Victor"})";
      auto ec = glz::read_json(obj, buffer);
      expect(bool(ec));
      auto error_message = glz::format_error(ec, buffer);
      expect(error_message ==
             "1:11: constraint_violated\n   {\"age\": -1, \"name\": \"Victor\"}\n             ^ Age out of range")
         << error_message << '\n';

      buffer = R"({"age": 10, "name": "Abra Cadabra"})";
      ec = glz::read_json(obj, buffer);
      expect(obj.age == 10);
      expect(bool(ec));
      error_message = glz::format_error(ec, buffer);
      expect(error_message ==
             "1:35: constraint_violated\n   {\"age\": 10, \"name\": \"Abra Cadabra\"}\n                                "
             "     ^ Name is too long")
         << error_message << '\n';

      expect(not glz::write_json(obj, buffer));
      expect(buffer == R"({"age":10,"name":"José"})") << buffer;
   };

   "optional read_constraint respects missing keys"_test = [] {
      constrained_optional_object obj{};
      constexpr glz::opts opts{.error_on_missing_keys = true};

      const std::string with_data = R"({"name":"FakeName","data":[1,2,3]})";
      auto ec = glz::read<opts>(obj, with_data);
      expect(ec == glz::error_code::none);
      expect(bool(obj.data));
      expect(obj.data->size() == 3);

      obj.data.reset();
      const std::string without_data = R"({"name":"FakeName"})";
      ec = glz::read<opts>(obj, without_data);
      expect(ec == glz::error_code::none);
      expect(not obj.data.has_value());

      const std::string invalid_data = R"({"name":"FakeName","data":[-2]})";
      ec = glz::read<opts>(obj, invalid_data);
      expect(ec == glz::error_code::constraint_violated);
   };
};

struct cross_constrained_object
{
   int age{};
   std::string name{};
};

template <>
struct glz::meta<cross_constrained_object>
{
   using T = cross_constrained_object;

   static constexpr auto composite_constraint = [](const T& value) {
      if (value.name.empty()) {
         return value.age > 5;
      }
      if (value.name.front() == 'A') {
         return value.age > 10;
      }
      return value.age > 5;
   };

   static constexpr auto value = object(&T::age, &T::name);
   static constexpr auto self_constraint = glz::self_constraint<composite_constraint, "Age/name combination invalid">;
};

suite self_constraint_tests = [] {
   "self constraint enforces combined fields"_test = [] {
      cross_constrained_object obj{};

      auto ec = glz::read_json(obj, R"({"age":6,"name":"Bob"})");
      expect(ec == glz::error_code::none);
      expect(obj.age == 6);
      expect(obj.name == "Bob");

      std::string buffer = R"({"age":9,"name":"Alice"})";
      ec = glz::read_json(obj, buffer);
      expect(ec == glz::error_code::constraint_violated);
      auto message = glz::format_error(ec, buffer);
      expect(message.find("Age/name combination invalid") != std::string::npos) << message;

      buffer = R"({"name":"Alice","age":9})";
      ec = glz::read_json(obj, buffer);
      expect(ec == glz::error_code::constraint_violated);
      message = glz::format_error(ec, buffer);
      expect(message.find("Age/name combination invalid") != std::string::npos) << message;

      buffer = R"({"age":12,"name":"Alice"})";
      ec = glz::read_json(obj, buffer);
      expect(ec == glz::error_code::none);
      expect(obj.age == 12);
      expect(obj.name == "Alice");
   };
};

struct registration_request
{
   std::string username{};
   std::string password{};
   std::string confirm_password{};
   std::optional<std::string> email{};
};

template <>
struct glz::meta<registration_request>
{
   using T = registration_request;

   static constexpr auto strong_credentials = [](const T& value) {
      const auto strong_length = value.password.size() >= 12;
      const auto matches = value.password == value.confirm_password;
      const auto has_username = !value.username.empty();
      return strong_length && matches && has_username;
   };

   static constexpr auto value = object(&T::username, &T::password, &T::confirm_password, &T::email);

   static constexpr auto self_constraint =
      glz::self_constraint<strong_credentials, "Password must be at least 12 characters and match confirmation">;
};

struct inventory_request
{
   std::string customer_tier{};
   int quantity{};
   std::optional<int> inventory_limit{};
   std::optional<std::string> discount_code{};
};

template <>
struct glz::meta<inventory_request>
{
   using T = inventory_request;

   static constexpr auto business_rules = [](const T& value) {
      if (value.inventory_limit && value.quantity > *value.inventory_limit) {
         return false;
      }

      if (value.discount_code && value.customer_tier == "guest") {
         return false;
      }

      return value.quantity > 0;
   };

   static constexpr auto value = object(&T::customer_tier, &T::quantity, &T::inventory_limit, &T::discount_code);

   static constexpr auto self_constraint =
      glz::self_constraint<business_rules, "Order violates inventory or discount policy">;
};

suite self_constraint_real_world = [] {
   "registration request enforces strong credentials"_test = [] {
      {
         registration_request req{};
         const auto ok =
            glz::read_json(req, R"({"username":"coder","password":"longpass123!","confirm_password":"longpass123!"})");
         expect(ok == glz::error_code::none);
         expect(req.email == std::nullopt);
      }

      {
         registration_request req{};
         const auto mismatch =
            glz::read_json(req, R"({"username":"coder","password":"longpass123!","confirm_password":"different"})");
         expect(mismatch == glz::error_code::constraint_violated);
         const auto mismatch_message = glz::format_error(
            mismatch, R"({"username":"coder","password":"longpass123!","confirm_password":"different"})");
         expect(mismatch_message.find("Password must be at least 12 characters") != std::string::npos)
            << mismatch_message;
      }

      {
         registration_request req{};
         const auto short_pw =
            glz::read_json(req, R"({"username":"coder","password":"short","confirm_password":"short"})");
         expect(short_pw == glz::error_code::constraint_violated);
      }
   };

   "inventory request validates tier and limits"_test = [] {
      {
         inventory_request request{};
         const auto ok = glz::read_json(
            request, R"({"customer_tier":"pro","quantity":5,"inventory_limit":10,"discount_code":"SPRING"})");
         expect(ok == glz::error_code::none);
         expect(request.discount_code.has_value());
      }

      {
         inventory_request request{};
         const auto exceeds_limit =
            glz::read_json(request, R"({"customer_tier":"pro","quantity":15,"inventory_limit":10})");
         expect(exceeds_limit == glz::error_code::constraint_violated);
      }

      {
         inventory_request request{};
         const auto guest_discount =
            glz::read_json(request, R"({"customer_tier":"guest","quantity":1,"discount_code":"WELCOME"})");
         expect(guest_discount == glz::error_code::constraint_violated);
      }

      {
         inventory_request request{};
         const auto minimal = glz::read_json(request, R"({"quantity":1,"customer_tier":"guest"})");
         expect(minimal == glz::error_code::none);
         expect(!request.discount_code);
      }
   };
};

struct client_state
{
   uint64_t id{};
   std::map<std::string, std::vector<std::string>> layouts{};
};

template <>
struct glz::meta<client_state>
{
   using T = client_state;
   static constexpr auto value = object("id", &T::id, "layouts", quoted<&T::layouts>);
};

suite unquote_test = [] {
   "unquote"_test = [] {
      client_state obj{};
      std::string s = R"({
  "id": 4848,
  "layouts": "{\"first layout\": [ \"inner1\", \"inner2\" ] }"
})";
      expect(!glz::read_json(obj, s));
      expect(obj.id == 4848);
      expect(obj.layouts.at("first layout") == std::vector<std::string>{"inner1", "inner2"});

      std::string out{};
      expect(not glz::write_json(obj, out));
      expect(out == R"({"id":4848,"layouts":"{\"first layout\":[\"inner1\",\"inner2\"]}"})");
   };
};

suite complex_test = [] {
   "complex"_test = [] {
      std::complex<double> cx{};
      std::string s = R"([1,2])";
      expect(!glz::read_json(cx, s));
      expect(cx.real() == 1.0);
      expect(cx.imag() == 2.0);

      s.clear();
      expect(not glz::write_json(cx, s));
      expect(s == R"([1,2])");

      cx = {};
      s = R"([
1,
2
])";
      expect(!glz::read_json(cx, s));
      expect(cx.real() == 1.0);
      expect(cx.imag() == 2.0);
   };

   "vector_complex"_test = [] {
      constexpr std::string_view s = //
         R"([
  [
    1,
    2
  ],
  [
    3,
    4
  ]
])";
      std::vector<std::complex<float>> cx{};
      expect(!glz::read_json(cx, s));
      expect(cx[0].real() == 1.f);
      expect(cx[0].imag() == 2.f);
      expect(cx[1].real() == 3.f);
      expect(cx[1].imag() == 4.f);
   };
};

struct manage_x
{
   std::vector<int> x{};
   std::vector<int> y{};

   bool read_x()
   {
      y = x;
      return true;
   }

   bool write_x()
   {
      x = y;
      return true;
   }
};

template <>
struct glz::meta<manage_x>
{
   using T = manage_x;
   static constexpr auto value = object("x", manage<&T::x, &T::read_x, &T::write_x>);
};

struct manage_x_lambda
{
   std::vector<int> x{};
   std::vector<int> y{};
};

template <>
struct glz::meta<manage_x_lambda>
{
   using T = manage_x_lambda;
   static constexpr auto read_x = [](auto& s) {
      s.y = s.x;
      return true;
   };
   static constexpr auto write_x = [](auto& s) {
      s.x = s.y;
      return true;
   };
   [[maybe_unused]] static constexpr auto value = object("x", manage<&T::x, read_x, write_x>);
};

struct manage_test_struct
{
   std::string a{};
   std::string b{};

   bool read_a() { return true; }
   bool write_a() { return false; }
};

template <>
struct glz::meta<manage_test_struct>
{
   using T = manage_test_struct;
   static constexpr auto value = object("a", manage<&T::a, &T::read_a, &T::write_a>, "b", &T::b);
};

suite manage_test = [] {
   "manage"_test = [] {
      manage_x obj{};
      std::string s = R"({"x":[1,2,3]})";
      expect(!glz::read_json(obj, s));
      expect(obj.y[0] == 1);
      expect(obj.y[1] == 2);
      expect(obj.y[2] == 3);
      obj.x.clear();
      s.clear();
      expect(not glz::write_json(obj, s));
      expect(s == R"({"x":[1,2,3]})");
      expect(obj.x[0] == 1);
      expect(obj.x[1] == 2);
      expect(obj.x[2] == 3);
   };

   "manage_lambdas"_test = [] {
      manage_x_lambda obj{};
      std::string s = R"({"x":[1,2,3]})";
      expect(!glz::read_json(obj, s));
      expect(obj.y[0] == 1);
      expect(obj.y[1] == 2);
      expect(obj.y[2] == 3);
      obj.x.clear();
      s.clear();
      expect(not glz::write_json(obj, s));
      expect(s == R"({"x":[1,2,3]})");
      expect(obj.x[0] == 1);
      expect(obj.x[1] == 2);
      expect(obj.x[2] == 3);
   };

   "manage_test_struct"_test = [] {
      manage_test_struct obj{.a = "aaa", .b = "bbb"};
      std::string s{};
      const auto ec = glz::write<glz::opts{}>(obj, s);
      expect(ec != glz::error_code::none);
   };
};

struct varx
{
   struct glaze
   {
      static constexpr std::string_view name = "varx";
      static constexpr auto value = glz::object();
   };
};
static_assert(glz::name_v<varx> == "varx");
struct vary
{
   struct glaze
   {
      static constexpr std::string_view name = "vary";
      static constexpr auto value = glz::object();
   };
};

using vari = std::variant<varx, vary>;

template <>
struct glz::meta<vari>
{
   static constexpr std::string_view name = "vari";
   static constexpr std::string_view tag = "type";
};

static_assert(glz::named<vari>);
static_assert(glz::name_v<vari> == "vari");

struct var_schema
{
   std::string schema{};
   vari variant{};

   struct glaze
   {
      using T = var_schema;
      static constexpr auto value = glz::object("$schema", &T::schema, &T::variant);
   };
};

suite empty_variant_objects = [] {
   "empty_variant_objects"_test = [] {
      vari v = varx{};
      std::string s;
      expect(not glz::write_json(v, s));
      expect(s == R"({"type":"varx"})");

      v = vary{};

      expect(!glz::read_json(v, s));
      expect(std::holds_alternative<varx>(v));
   };

   "empty_variant_objects schema"_test = [] {
      const auto s = glz::write_json_schema<var_schema>().value_or("error");
      expect(
         s ==
         R"({"type":["object"],"properties":{"$schema":{"$ref":"#/$defs/std::string"},"variant":{"$ref":"#/$defs/vari"}},"additionalProperties":false,"$defs":{"std::string":{"type":["string"]},"vari":{"type":["object"],"oneOf":[{"type":["object"],"properties":{"type":{"const":"varx"}},"additionalProperties":false,"required":["type"],"title":"varx"},{"type":["object"],"properties":{"type":{"const":"vary"}},"additionalProperties":false,"required":["type"],"title":"vary"}]}},"title":"var_schema"})")
         << s;
   };
};

template <typename PARAMS>
struct request_t
{
   int id = -1;
   std::optional<bool> proxy;
   std::string method;
   PARAMS params;

   // meta
   struct glaze
   {
      using T = request_t<PARAMS>;
      static constexpr auto value =
         glz::object("id", &T::id, "proxy", &T::proxy, "method", &T::method, "params", &T::params);
   };
};

struct QuoteData
{
   // session
   uint64_t time{};
   std::string action{}; // send, recv
   std::string quote{}; // order, kill
   std::string account{};
   uint32_t uid{};
   uint32_t session_id{};
   uint32_t request_id{};
   // order
   int state = 0;
   std::string order_id = "";
   std::string exchange = "";
   std::string type = "";
   std::string tif = "";
   std::string offset = "";
   std::string side = "";
   std::string symbol = "";
   double price = 0;
   double quantity = 0;
   double traded = 0;
};

typedef request_t<QuoteData> SaveQuote;

suite trade_quote_test = [] {
   "trade_quote"_test = [] {
      SaveQuote q{};
      q.id = 706;
      q.method = "save_quote";
      q.params.time = 1698627291351456360;
      q.params.action = "send";
      q.params.quote = "kill";
      q.params.account = "603302";
      q.params.uid = 11;
      q.params.session_id = 1;
      q.params.request_id = 41;
      q.params.state = 0;
      q.params.order_id = "2023103000180021";
      q.params.exchange = "CZCE";
      q.params.symbol = "SPD RM401&RM403";

      std::string buffer;
      expect(not glz::write<glz::opts{}>(q, buffer));

      expect(
         buffer ==
         R"({"id":706,"method":"save_quote","params":{"time":1698627291351456360,"action":"send","quote":"kill","account":"603302","uid":11,"session_id":1,"request_id":41,"state":0,"order_id":"2023103000180021","exchange":"CZCE","type":"","tif":"","offset":"","side":"","symbol":"SPD RM401&RM403","price":0,"quantity":0,"traded":0}})")
         << buffer;
   };
};

suite invoke_update_test = [] {
   "invoke"_test = [] {
      int x = 5;

      std::map<std::string, glz::invoke_update<void()>> funcs;
      funcs["square"] = [&] { x *= x; };
      funcs["add_one"] = [&] { x += 1; };

      std::string s = R"(
 {
    "square":[],
    "add_one":[]
 })";
      expect(!glz::read_json(funcs, s));
      expect(x == 5);

      std::string s2 = R"(
 {
    "square":[],
    "add_one":[ ]
 })";
      expect(!glz::read_json(funcs, s2));
      expect(x == 6);

      std::string s3 = R"(
 {
    "square":[ ],
    "add_one":[ ]
 })";
      expect(!glz::read_json(funcs, s3));
      expect(x == 36);
   };
};

struct updater
{
   int x = 5;
   glz::invoke_update<void()> square;
   glz::invoke_update<void()> add_one;

   // constructor required by MSVC
   updater()
   {
      square = [&] { x *= x; };
      add_one = [&] { x += 1; };
   }
};

template <>
struct glz::meta<updater>
{
   using T = updater;
   static constexpr auto value = object(&T::x, &T::square, &T::add_one);
};

suite invoke_updater_test = [] {
   "invoke_updater"_test = [] {
      updater obj{};
      auto& x = obj.x;

      std::string s = R"(
 {
    "square":[],
    "add_one":[]
 })";
      expect(!glz::read_json(obj, s));
      expect(x == 5) << x;

      std::string s2 = R"(
 {
    "square":[],
    "add_one":[ ]
 })";
      expect(!glz::read_json(obj, s2));
      expect(x == 6) << x;

      std::string s3 = R"(
 {
    "square":[ ],
    "add_one":[ ]
 })";
      expect(!glz::read_json(obj, s3));
      expect(x == 36) << x;
   };
};

struct raw_stuff
{
   std::string a{};
   std::string b{};
   std::string c{};

   struct glaze
   {
      using T = raw_stuff;
      static constexpr auto value = glz::object("a", &T::a, "b", &T::b, "c", &T::c);
   };
};

struct raw_stuff_wrapper
{
   raw_stuff data{};

   struct glaze
   {
      using T = raw_stuff_wrapper;
      static constexpr auto value{glz::raw_string<&T::data>};
   };
};

struct raw_stuff_escaped
{
   raw_stuff data{};

   struct glaze
   {
      using T = raw_stuff_escaped;
      static constexpr auto value{glz::escaped<&T::data>};
   };
};

suite raw_string_test = [] {
   "raw_string"_test = [] {
      raw_stuff obj{};
      std::string buffer = R"({"a":"Hello\nWorld","b":"Hello World","c":"\tHello\bWorld"})";

      expect(!glz::read<glz::opts{.raw_string = true}>(obj, buffer));
      expect(obj.a == R"(Hello\nWorld)");
      expect(obj.b == R"(Hello World)");
      expect(obj.c == R"(\tHello\bWorld)");

      buffer.clear();
      expect(not glz::write<glz::opts{.raw_string = true}>(obj, buffer));
      expect(buffer == R"({"a":"Hello\nWorld","b":"Hello World","c":"\tHello\bWorld"})");
   };

   "raw_string_wrapper"_test = [] {
      raw_stuff_wrapper obj{};
      std::string buffer = R"({"a":"Hello\nWorld","b":"Hello World","c":"\tHello\bWorld"})";

      expect(!glz::read_json(obj, buffer));
      expect(obj.data.a == R"(Hello\nWorld)");
      expect(obj.data.b == R"(Hello World)");
      expect(obj.data.c == R"(\tHello\bWorld)");

      buffer.clear();
      expect(not glz::write_json(obj, buffer));
      expect(buffer == R"({"a":"Hello\nWorld","b":"Hello World","c":"\tHello\bWorld"})");
   };

   "raw_string_escaped"_test = [] {
      raw_stuff_escaped obj{};
      std::string buffer = R"({"a":"Hello\nWorld"})";

      expect(!glz::read_json(obj, buffer));
      expect(obj.data.a ==
             R"(Hello
World)");

      buffer.clear();
      expect(not glz::write_json(obj, buffer));
      expect(buffer == R"({"a":"Hello\nWorld","b":"","c":""})");
   };
};

struct Update
{
   int64_t time;
};

suite ndjson_error_test = [] {
   "ndjson_error"_test = [] {
      auto x = glz::read_ndjson<std::vector<Update>>("{\"t\":73}\n{\"t\":37}");
      expect(x.error() == glz::error_code::unknown_key);
   };
};

suite bitset = [] {
   "bitset8"_test = [] {
      std::bitset<8> b = 0b10101010;

      std::string s{};
      expect(not glz::write_json(b, s));

      expect(s == R"("10101010")") << s;

      b.reset();
      expect(!glz::read_json(b, s));
      expect(b == 0b10101010);
   };

   "bitset16"_test = [] {
      std::bitset<16> b = 0b10010010'00000010;

      std::string s{};
      expect(not glz::write_json(b, s));

      expect(s == R"("1001001000000010")") << s;

      b.reset();
      expect(!glz::read_json(b, s));
      expect(b == 0b10010010'00000010);
   };
};

#if defined(__STDCPP_FLOAT128_T__) && !defined(__APPLE__)
suite float128_test = [] {
   "float128"_test = [] {
      std::float128_t x = 3.14;

      std::string s{};
      expect(not glz::write_json(x, s));

      x = 0.0;
      expect(!glz::read_json(x, s));
      expect(x == 3.14);
   };
};
#endif

struct unknown_fields_member
{
   std::string a;
   std::string missing;
   std::string end;
   std::map<glz::sv, glz::raw_json> extra;
};

template <>
struct glz::meta<unknown_fields_member>
{
   using T = unknown_fields_member;
   static constexpr auto value = object("a", &T::a, "missing", &T::missing, "end", &T::end);
   static constexpr auto unknown_write{&T::extra};
   static constexpr auto unknown_read{&T::extra};
};

struct unknown_fields_2
{
   std::shared_ptr<std::string> a{};
   std::shared_ptr<std::string> missing{};
   std::shared_ptr<std::string> end{};
   std::map<glz::sv, glz::raw_json> extra;
};

template <>
struct glz::meta<unknown_fields_2>
{
   using T = unknown_fields_2;
   static constexpr auto value = object("a", &T::a, "missing", &T::missing, "end", &T::end);
   static constexpr auto unknown_write{&T::extra};
   static constexpr auto unknown_read{&T::extra};
};

struct my_unknown_struct
{
   int i = 287;
   std::unordered_map<std::string, std::string> unknown;
};

template <>
struct glz::meta<my_unknown_struct>
{
   using T = my_unknown_struct;
   static constexpr auto value = glz::object("i", &T::i);

   static constexpr auto unknown_write{&T::unknown};
   // static constexpr auto unknown_read{
   //     &T::unknown
   // };
};

suite unknown_fields_member_test = [] {
   "decode_unknown"_test = [] {
      unknown_fields_member obj{};

      std::string buffer = R"({"a":"aaa","unk":"zzz", "unk2":{"sub":3,"sub2":[{"a":"b"}]},"unk3":[], "end":"end"})";

      expect(not glz::read<glz::opts{.error_on_unknown_keys = false}>(obj, buffer));

      expect(obj.extra["unk"].str == R"("zzz")");
      expect(obj.extra["unk2"].str == R"({"sub":3,"sub2":[{"a":"b"}]})");
      expect(obj.extra["unk3"].str == R"([])");
   };

   "encode_unknown"_test = [] {
      unknown_fields_member obj{};
      glz::context ctx{};
      obj.a = "aaa";
      obj.end = "end";
      obj.extra["unk"] = R"("zzz")";
      obj.extra["unk2"] = R"({"sub":3,"sub2":[{"a":"b"}]})";
      obj.extra["unk3"] = R"([])";

      std::string result =
         R"({"a":"aaa","missing":"","end":"end","unk":"zzz","unk2":{"sub":3,"sub2":[{"a":"b"}]},"unk3":[]})";
      expect(glz::write_json(obj) == result);
   };

   "unknown_fields_2"_test = [] {
      unknown_fields_2 obj{};
      std::string buffer = R"({"unk":"zzz", "unk2":{"sub":3,"sub2":[{"a":"b"}]},"unk3":[]})";
      auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(obj, buffer);
      expect(not ec) << glz::format_error(ec, buffer);
      std::string out{};
      expect(not glz::write_json(obj, out));
      expect(out == R"({"unk":"zzz","unk2":{"sub":3,"sub2":[{"a":"b"}]},"unk3":[]})") << out;
   };

   "my_unknown_struct"_test = [] {
      my_unknown_struct obj;
      std::string buffer;
      expect(not glz::write<glz::opts{.prettify = true}>(obj, buffer));
      expect(buffer == R"({
   "i": 287
})") << buffer;

      expect(not glz::write<glz::opts{}>(obj, buffer));
      expect(buffer == R"({"i":287})") << buffer;
   };
};

struct unknown_fields_method
{
   std::string a;
   std::string missing;
   std::string end;
   unknown_fields_member sub; // test writing of sub extras too
   std::map<glz::sv, glz::raw_json> extra;

   void my_unknown_read(const glz::sv& key, const glz::raw_json& value) { extra[key] = value; };

   std::map<glz::sv, glz::raw_json> my_unknown_write() const { return extra; }
};

template <>
struct glz::meta<unknown_fields_method>
{
   using T = unknown_fields_method;
   static constexpr auto value = object("a", &T::a, "missing", &T::missing, "end", &T::end, "sub", &T::sub);
   static constexpr auto unknown_write{&T::my_unknown_write};
   static constexpr auto unknown_read{&T::my_unknown_read};
};

suite unknown_fields_method_test = [] {
   "decode_unknown"_test = [] {
      unknown_fields_method obj{};

      std::string buffer = R"({"a":"aaa","unk":"zzz", "unk2":{"sub":3,"sub2":[{"a":"b"}]},"unk3":[], "end":"end"})";

      glz::context ctx{};

      expect(!glz::read<glz::opts{.error_on_unknown_keys = false}>(obj, buffer, ctx));

      expect(obj.extra["unk"].str == R"("zzz")");
      expect(obj.extra["unk2"].str == R"({"sub":3,"sub2":[{"a":"b"}]})");
      expect(obj.extra["unk3"].str == R"([])");
   };

   "encode_unknown"_test = [] {
      unknown_fields_method obj{};
      glz::context ctx{};
      obj.a = "aaa";
      obj.end = "end";
      obj.my_unknown_read("unk", R"("zzz")");
      obj.my_unknown_read("unk2", R"({"sub":3,"sub2":[{"a":"b"}]})");
      obj.my_unknown_read("unk3", R"([])");
      obj.sub.extra["subextra"] = R"("subextraval")";
      std::string result =
         R"({"a":"aaa","missing":"","end":"end","sub":{"a":"","missing":"","end":"","subextra":"subextraval"},"unk":"zzz","unk2":{"sub":3,"sub2":[{"a":"b"}]},"unk3":[]})";
      expect(glz::write_json(obj) == result);
   };
};

struct unknown_fields_known_type
{
   std::string a;
   std::string missing;
   std::string end;
   std::map<glz::sv, int> extra;
};

template <>
struct glz::meta<unknown_fields_known_type>
{
   using T = unknown_fields_known_type;
   static constexpr auto value = object("a", &T::a, "missing", &T::missing, "end", &T::end);
   static constexpr auto unknown_write{&T::extra};
   static constexpr auto unknown_read{&T::extra};
};

suite unknown_fields_known_type_test = [] {
   "decode_unknown"_test = [] {
      std::string buffer = R"({"a":"aaa","unk":5, "unk2":22,"unk3":355, "end":"end"})";

      unknown_fields_known_type obj{};
      expect(!glz::read<glz::opts{.error_on_unknown_keys = false}>(obj, buffer));

      expect(obj.extra["unk"] == 5);
      expect(obj.extra["unk2"] == 22);
      expect(obj.extra["unk3"] == 355);
   };
};

struct unknown_fields_string_key
{
   std::string a;
   std::string end;
   // Use std::string for keys and raw_json for values
   std::map<std::string, glz::raw_json> extra;
};

template <>
struct glz::meta<unknown_fields_string_key>
{
   using T = unknown_fields_string_key;
   static constexpr auto value = object("a", &T::a, "end", &T::end);
   static constexpr auto unknown_write{&T::extra};
   static constexpr auto unknown_read{&T::extra};
};

suite unknown_fields_string_key_test = [] {
   "decode_unknown_string_key_map"_test = [] {
      unknown_fields_string_key obj{};
      std::string buffer = R"({"a":"aaa","unk":"zzz", "unk2":{"sub":3,"sub2":[{"a":"b"}]},"unk3":[], "end":"end"})";
      auto ec = glz::read<glz::opts{.error_on_unknown_keys = false}>(obj, buffer);
      expect(not ec) << glz::format_error(ec, buffer);
      expect(obj.extra["unk"].str == R"("zzz")");
      expect(obj.extra["unk2"].str == R"({"sub":3,"sub2":[{"a":"b"}]})");
      expect(obj.extra["unk3"].str == R"([])");
   };

   "encode_unknown_string_key_map"_test = [] {
      unknown_fields_string_key obj{};
      obj.a = "aaa";
      obj.end = "end";
      obj.extra["unk"] = R"("zzz")";
      obj.extra["unk2"] = R"({"sub":3,"sub2":[{"a":"b"}]})";
      obj.extra["unk3"] = R"([])";
      std::string out{};
      auto ec = glz::write_json(obj, out);
      expect(not ec) << glz::format_error(ec, out);
      expect(out == R"({"a":"aaa","end":"end","unk":"zzz","unk2":{"sub":3,"sub2":[{"a":"b"}]},"unk3":[]})") << out;
   };
};

struct key_reflection
{
   int i = 287;
   double d = 3.14;
   std::string hello = "Hello World";
   std::array<uint64_t, 3> arr = {1, 2, 3};
};

template <>
struct glz::meta<key_reflection>
{
   static constexpr std::string_view name = "key_reflection";
   using T = key_reflection;
   static constexpr auto value = object(&T::i, //
                                        &T::d, //
                                        &T::hello, //
                                        &T::arr //
   );
};

suite key_reflection_tests = [] {
   "reflect keys from glz::meta"_test = [] {
      std::string s;
      key_reflection obj{};
      expect(not glz::write_json(obj, s));

      expect(s == R"({"i":287,"d":3.14,"hello":"Hello World","arr":[1,2,3]})") << s;

      obj.i = 0;
      obj.d = 0;
      obj.hello = "";
      obj.arr = {};
      expect(!glz::read_json(obj, s));

      expect(obj.i == 287);
      expect(obj.d == 3.14);
      expect(obj.hello == "Hello World");
      expect(obj.arr == std::array<uint64_t, 3>{1, 2, 3});
   };
};

suite write_buffer_generator = [] {
   "write_buffer_generator"_test = [] {
      key_reflection obj{};
      auto s = glz::write_json(obj).value_or("error");

      expect(s == R"({"i":287,"d":3.14,"hello":"Hello World","arr":[1,2,3]})") << s;
   };
};

struct lambda_tester
{
   int x{};
   int* ptr{&x};

   struct glaze
   {
      static constexpr auto value = [](auto& self) { return self.ptr; };
   };
};

suite value_lambda_test = [] {
   "value lambda"_test = [] {
      lambda_tester obj{};
      obj.x = 55;

      auto s = glz::write_json(obj).value_or("error");
      expect(s == "55") << s;

      obj.x = 0;
      expect(!glz::read_json(obj, s));
      expect(obj.x == 55);
   };
};

struct reader_writer1
{
   void read(const std::string&) {}
   std::vector<std::string> write() { return {"1", "2", "3"}; }
   struct glaze
   {
      using T = reader_writer1;
      static constexpr auto value = glz::custom<&T::read, &T::write>;
   };
};

struct reader_writer2
{
   std::vector<reader_writer1> r{{}};
   struct glaze
   {
      static constexpr auto value = &reader_writer2::r;
   };
};

suite reader_writer_test = [] {
   "reader_writer"_test = [] {
      reader_writer2 obj{};
      std::string s;
      expect(not glz::write_json(obj, s));
      expect(s == R"([["1","2","3"]])") << s;
   };
};

struct Obj1
{
   int value;
   std::string text;
};

template <>
struct glz::meta<Obj1>
{
   using T = Obj1;
   static constexpr auto list_write = [](T& obj1) {
      const auto& value = obj1.value;
      return std::vector<int>{value, value + 1, value + 2};
   };
   static constexpr auto value = object(&T::value, &T::text, "list", glz::custom<skip{}, list_write>);
};

struct Obj2
{
   int value;
   std::string text;
   Obj1 obj1;
};

suite custom_object_variant_test = [] {
   "custom_object_variant"_test = [] {
      using Serializable = std::variant<Obj1, Obj2>;
      std::vector<Serializable> objects{
         Obj1{1, "text 1"},
         Obj1{2, "text 2"},
         Obj2{3, "text 3", 10, "1000"},
         Obj1{4, "text 4"},
      };

      constexpr auto prettify_json = glz::opts{.prettify = true};

      std::string data = glz::write<prettify_json>(objects).value_or("error");

      expect(data == R"([
   {
      "value": 1,
      "text": "text 1",
      "list": [
         1,
         2,
         3
      ]
   },
   {
      "value": 2,
      "text": "text 2",
      "list": [
         2,
         3,
         4
      ]
   },
   {
      "value": 3,
      "text": "text 3",
      "obj1": {
         "value": 10,
         "text": "1000",
         "list": [
            10,
            11,
            12
         ]
      }
   },
   {
      "value": 4,
      "text": "text 4",
      "list": [
         4,
         5,
         6
      ]
   }
])");

      objects.clear();

      expect(!glz::read_json(objects, data));

      expect(data == glz::write<prettify_json>(objects));
   };
};

struct hostname_include_struct
{
   glz::hostname_include hostname_include{};
   std::string str = "Hello";
   int i = 55;
};

static_assert(glz::detail::count_members<hostname_include_struct> == 3);

suite hostname_include_test = [] {
   "hostname_include"_test = [] {
      hostname_include_struct obj{};

      glz::context ctx{};
      const auto hostname = glz::get_hostname(ctx);

      std::string file_name = "../{}_config.json";
      glz::replace_first_braces(file_name, hostname);

      const auto config_buffer = R"(
// testing opening whitespace and comment
)" + glz::write_json(obj).value_or("error");
      expect(glz::buffer_to_file(config_buffer, file_name) == glz::error_code::none);
      // expect(glz::write_file_json(obj, file_name, std::string{}) == glz::error_code::none);

      obj.str = "";
      obj.i = 0;

      std::string_view s = R"(
// testing opening whitespace and comment
{"hostname_include": "../{}_config.json", "i": 100})";
      const auto ec = glz::read_jsonc(obj, s);
      expect(ec == glz::error_code::none) << glz::format_error(ec, s);

      expect(obj.str == "Hello") << obj.str;
      expect(obj.i == 100) << obj.i;

      obj.str = "";

      std::string buffer{};
      expect(!glz::read_file_jsonc(obj, file_name, buffer));
      expect(obj.str == "Hello") << obj.str;
      expect(obj.i == 55) << obj.i;

      s = R"({"i": 100, "hostname_include": "../{}_config.json"})";
      expect(!glz::read_jsonc(obj, s));

      expect(obj.str == "Hello") << obj.str;
      expect(obj.i == 55) << obj.i;
   };
};

struct core_struct
{
   glz::file_include include{};
   float number{3.14f};
};

struct nested_include_struct
{
   glz::hostname_include hostname_include{};
   std::string str = "Hello";
   int integer = 55;
   core_struct core{};
};

suite nested_include_tests = [] {
   "nested_include"_test = [] {
      expect(glz::error_code::none == glz::buffer_to_file(std::string_view{R"({"number":3.5})"}, "./core.jsonc"));

      glz::context ctx{};
      const auto hostname = glz::get_hostname(ctx);

      std::string file_name = "./{}_include_test.jsonc";
      glz::replace_first_braces(file_name, hostname);
      expect(glz::error_code::none ==
             glz::buffer_to_file(std::string_view{R"({"core":{"include": "./core.jsonc"}})"}, file_name));

      expect(glz::error_code::none ==
             glz::buffer_to_file(
                std::string_view{R"({"str":"goodbye","integer":4,"hostname_include":"./{}_include_test.jsonc"})"},
                "./start.jsonc"));

      nested_include_struct obj{};
      std::string buffer{};
      auto ec = glz::read_file_jsonc(obj, "./start.jsonc", buffer);
      expect(not ec) << glz::format_error(ec, buffer);
      expect(obj.str == "goodbye");
      expect(obj.integer = 4);
      expect(obj.core.number == 3.5f);
   };
};

enum class some_enum {
   first,
   second,
};

struct enum_glaze_struct
{
   some_enum e{};
   int i{};

   struct glaze
   {
      using T = enum_glaze_struct;
      static constexpr auto value = glz::object(&T::e, &T::i);
   };
};

template <some_enum E, class DataType>
struct wrapper_struct
{
   some_enum type{E};
   DataType data{};

   struct glaze
   {
      using B = wrapper_struct;
      static constexpr auto value = glz::object(&B::type, &B::data);
   };
};

suite enum_in_object_reflection_test = [] {
   "enum_in_object_reflection"_test = [] {
      enum_glaze_struct obj{};
      expect(glz::write_json(obj) == R"({"e":0,"i":0})");
   };

   "enum_in_object_reflection2"_test = [] {
      wrapper_struct<some_enum::second, int> obj{};
      expect(glz::write_json(obj) == R"({"type":1,"data":0})");
   };
};

struct unicode_keys
{
   float field1;
   float field2;
   std::uint8_t field3;
   std::string field4;
   std::string field5;
   std::string field6;
   std::string field7;
};

template <>
struct glz::meta<unicode_keys>
{
   using T = unicode_keys;
   static constexpr auto value = object("alpha", &T::field1, "bravo", &T::field2, "charlie", &T::field3, "♥️",
                                        &T::field4, "delta", &T::field5, "echo", &T::field6, "😄", &T::field7);
};

struct unicode_keys2
{
   float field1;
   float field2;
   std::uint8_t field3;
};

template <>
struct glz::meta<unicode_keys2>
{
   using T = unicode_keys2;
   static constexpr auto value = object("😄", &T::field1, "💔", &T::field2, "alpha", &T::field3);
};

struct unicode_keys3
{
   float field0;
   float field1;
   float field2;
   std::uint8_t field3;
   std::string field4;
   std::string field5;
   std::string field6;
};

template <>
struct glz::meta<unicode_keys3>
{
   using T = unicode_keys3;
   static constexpr auto value = object("简体汉字", &T::field0, // simplified chinese characters
                                        "漢字寿限無寿限無五劫", &T::field1, // traditional chinese characters / kanji
                                        "こんにちはむところやぶら", &T::field2, // katakana
                                        "한국인", &T::field3, // korean
                                        "русский", &T::field4, // cyrillic
                                        "สวัสดี", &T::field5, // thai
                                        "english", &T::field6);
};

suite unicode_keys_test = [] {
   "unicode_keys"_test = [] {
      unicode_keys obj{};
      std::string buffer{};
      expect(not glz::write_json(obj, buffer));

      expect(!glz::read_json(obj, buffer));
   };

   "unicode_keys2"_test = [] {
      unicode_keys2 obj{};
      std::string buffer{};
      expect(not glz::write_json(obj, buffer));

      expect(!glz::read_json(obj, buffer));
   };

   "unicode_keys3"_test = [] {
      unicode_keys3 obj{};
      std::string buffer{};
      expect(not glz::write_json(obj, buffer));

      expect(!glz::read_json(obj, buffer));
   };
};

struct string_tester
{
   std::string val1{};
};

template <>
struct glz::meta<string_tester>
{
   using T = string_tester;
   static constexpr auto value = object("val1", &T::val1);
};

template <size_t N>
std::vector<char> make_vector(const char (&literal)[N])
{
   // include the null
   return {literal, literal + N};
};

// Former address santizer issues
suite address_sanitizer_test = [] {
   "address_sanitizer"_test = [] {
      string_tester obj{};
      std::string buffer{R"({"val1":"1234567890123456"})"}; // 16 character string value
      auto res = glz::read_json<string_tester>(buffer);
      expect(bool(res));
      [[maybe_unused]] auto parsed = glz::write_json(res.value());
   };

   "address_santizer string read"_test = [] {
      std::string buffer = "\x9e";
      my_struct obj{};
      expect(glz::read_json(obj, buffer));
   };

   "invalid generic read 1"_test = [] {
      glz::generic json{};
      auto blah = std::vector<char>{0x22, 0x5c, static_cast<char>(0xff), 0x22, 0x00};
      expect(glz::read_json(json, blah));
   };

   "invalid generic 2"_test = [] {
      glz::generic json{};
      auto blah = std::vector<char>{0x22, 0x5c, 0x75, static_cast<char>(0xff), 0x22, 0x00};
      expect(glz::read_json(json, blah));
   };

   "invalid generic 3"_test = [] {
      glz::generic json{};

      std::vector<unsigned char> udata{
         0x22, 0x5c, 0x75, 0x22, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65,
         0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65,
         0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65,
         0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65,
         0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65,
         0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x65, 0x4b, 0x00};
      std::vector<unsigned char> data{udata.begin(), udata.end()};
      auto r = glz::read_json(json, data);
      expect(r);
   };

   "generic 4"_test = []() {
      glz::generic json{};
      std::string data = "\"\\uDBDD\" DDDD";
      auto r = glz::read_json(json, data);
      expect(r);
   };

   "generic 5"_test = []() {
      glz::generic json{};
      auto data = make_vector("\"\\udb0f \"df33 ");
      auto r = glz::read_json(json, data);
      expect(r);
   };
};

struct Sinks
{
   bool file = false;
   bool console = false;

   struct glaze
   {
      using T = Sinks;
      static constexpr auto value = glz::flags("file", &T::file, "console", &T::console);
   };
};

suite flags_test = [] {
   const auto opt = glz::read_json<Sinks>(R"([  "file"])");
   expect(opt.has_value());
   expect(opt.value().file);
};

struct Header
{
   std::string id{};
   std::string type{};
};

struct HeaderFlipped
{
   std::string type{};
   std::string id{};
};

struct NestedPartialRead
{
   std::string method{};
   Header header{};
   int number{};
};

suite partial_read_tests = [] {
   using namespace ut;

   static constexpr glz::opts partial_read{.partial_read = true};

   "partial read"_test = [] {
      Header h{};
      std::string buf = R"({"id":"51e2affb","type":"message_type","unknown key":"value"})";

      expect(!glz::read<partial_read>(h, buf));
      expect(h.id == "51e2affb");
      expect(h.type == "message_type");
   };

   "partial read 2"_test = [] {
      Header h{};
      // closing curly bracket is missing
      std::string buf = R"({"id":"51e2affb","type":"message_type","unknown key":"value")";

      expect(!glz::read<partial_read>(h, buf));
      expect(h.id == "51e2affb");
      expect(h.type == "message_type");
   };

   "partial read unknown key"_test = [] {
      Header h{};
      std::string buf = R"({"id":"51e2affb","unknown key":"value","type":"message_type"})";

      expect(glz::read<partial_read>(h, buf) == glz::error_code::unknown_key);
      expect(h.id == "51e2affb");
      expect(h.type.empty());
   };

   "partial read unknown key 2"_test = [] {
      Header h{};
      std::string buf = R"({"id":"51e2affb","unknown key":"value","type":"message_type"})";

      expect(!glz::read<glz::opts{.error_on_unknown_keys = false, .partial_read = true}>(h, buf));
      expect(h.id == "51e2affb");
      expect(h.type == "message_type");
   };

   "partial read don't read garbage"_test = [] {
      Header h{};
      std::string buf = R"({"id":"51e2affb","unknown key":"value","type":"message_type"garbage})";

      expect(!glz::read<glz::opts{.error_on_unknown_keys = false, .partial_read = true}>(h, buf));
      expect(h.id == "51e2affb");
      expect(h.type == "message_type");
   };

   "partial read missing key"_test = [] {
      Header h{};
      std::string buf = R"({"id":"51e2affb","unknown key":"value"})";

      expect(glz::read<glz::opts{.error_on_unknown_keys = false, .partial_read = true}>(h, buf) !=
             glz::error_code::missing_key);
      expect(h.id == "51e2affb");
      expect(h.type.empty());
   };

   "partial read missing key 2"_test = [] {
      Header h{};
      std::string buf = R"({"id":"51e2affb","unknown key":"value"})";

      expect(!glz::read<glz::opts{.error_on_unknown_keys = false, .partial_read = true}>(h, buf));
      expect(h.id == "51e2affb");
      expect(h.type.empty());
   };

   "partial read HeaderFlipped"_test = [] {
      HeaderFlipped h{};
      std::string buf = R"({"id":"51e2affb","type":"message_type","unknown key":"value"})";

      expect(not glz::read<partial_read>(h, buf));
      expect(h.id == "51e2affb");
      expect(h.type == "message_type");
   };

   "partial read HeaderFlipped unknown key"_test = [] {
      HeaderFlipped h{};
      std::string buf = R"({"id":"51e2affb","unknown key":"value","type":"message_type"})";

      expect(glz::read<partial_read>(h, buf) == glz::error_code::unknown_key);
      expect(h.id == "51e2affb");
      expect(h.type.empty());
   };

   "partial read unknown key 2 HeaderFlipped"_test = [] {
      HeaderFlipped h{};
      std::string buf = R"({"id":"51e2affb","unknown key":"value","type":"message_type","another_field":409845})";

      expect(glz::read<glz::opts{.error_on_unknown_keys = false, .partial_read = true}>(h, buf) ==
             glz::error_code::none);
      expect(h.id == "51e2affb");
      expect(h.type == "message_type");
   };
};

// Test structs for optional/required field tests
struct OptionalFieldTest
{
   std::optional<int> optional_field;
   std::string required_field;
};

struct RequiredFieldTest
{
   int required_field1;
   std::string required_field2;
};

suite nested_partial_read_tests = [] {
   using namespace ut;

   static constexpr glz::opts partial_read{.partial_read = true};

   "nested object partial read"_test = [] {
      NestedPartialRead n{};
      std::string buf =
         R"({"method":"m1","header":{"id":"51e2affb","type":"message_type","unknown key":"value"},"number":51})";

      expect(not glz::read<partial_read>(n, buf));
      expect(n.method == "m1");
      expect(n.header.id == "51e2affb");
      expect(n.header.type == "message_type");
      expect(n.number == 0);
   };

   "nested object partial read, don't read garbage"_test = [] {
      NestedPartialRead n{};
      std::string buf =
         R"({"method":"m1","header":{"id":"51e2affb","type":"message_type","unknown key":"value",garbage},"number":51})";

      expect(not glz::read<partial_read>(n, buf));
      expect(n.method == "m1");
      expect(n.header.id == "51e2affb");
      expect(n.header.type == "message_type");
      expect(n.number == 0);
   };

   // Test for optional fields with partial_read and error_on_missing_keys
   "optional field with partial_read and error_on_missing_keys"_test = [] {
      OptionalFieldTest obj{};
      std::string buf = R"({"required_field":"test"})";

      // Both partial_read and error_on_missing_keys are true
      constexpr auto opts = glz::opts{.skip_null_members = true, .error_on_missing_keys = true, .partial_read = true};

      // Should not error on missing optional field
      auto ec = glz::read<opts>(obj, buf);
      expect(not ec) << glz::format_error(ec, buf);
      expect(not obj.optional_field.has_value());
      expect(obj.required_field == "test");
   };

   // Test that required fields still trigger errors
   "required field with partial_read and error_on_missing_keys"_test = [] {
      RequiredFieldTest obj{};
      std::string buf = R"({"required_field1":42})";

      constexpr auto opts = glz::opts{.skip_null_members = true, .error_on_missing_keys = true, .partial_read = true};

      // Should error on missing required field
      auto ec = glz::read<opts>(obj, buf);
      expect(ec == glz::error_code::missing_key);
   };
};

struct array_holder_t
{
   std::vector<int> x{0, 0, 0, 0, 0};
   std::vector<int> y{0};
};

suite nested_array_partial_read_tests = [] {
   using namespace ut;

   static constexpr glz::opts partial_read{.partial_read = true};

   "nested array partial read"_test = [] {
      std::vector<std::vector<int>> v{{0, 0}};
      std::string buf = "[[1,2],[3,4],[5,6]]";

      expect(not glz::read<partial_read>(v, buf));
      expect(v.size() == 1);
      expect(v[0].size() == 2);
      expect(v[0][0] == 1);
      expect(v[0][1] == 2);
   };

   "array_holder_t"_test = [] {
      array_holder_t obj{};
      std::string buf = R"({"x":[1,2,3],"y":[1,2,3,4]})";

      auto ec = glz::read<partial_read>(obj, buf);
      expect(not ec) << glz::format_error(ec, buf);
      expect(obj.x.size() == 3);
      expect(obj.x[0] == 1);
      expect(obj.x[1] == 2);
      expect(obj.x[2] == 3);
      expect(obj.y.size() == 1);
      expect(obj.y[0] == 1);
   };
};

struct AccountUpdateInner
{
   char a[16];
   double wb;
};

template <>
struct glz::meta<AccountUpdateInner>
{
   using T = AccountUpdateInner;
   static constexpr auto value = object("a", &T::a, "wb", glz::quoted_num<&T::wb>);
};

struct AccountUpdateData
{
   std::vector<AccountUpdateInner> B;
};

struct AccountUpdate
{
   static void fromJson(AccountUpdate& accountUpdate, const std::string& jSon);
   AccountUpdateData a;
};

inline void AccountUpdate::fromJson(AccountUpdate& accountUpdate, const std::string& jSon)
{
   auto ec = glz::read<glz::opts{.error_on_unknown_keys = false, .raw_string = true}>(accountUpdate, jSon);
   expect(not ec) << glz::format_error(ec, jSon);
}

suite account_update_partial_read_tests = [] {
   "AccountUpdate partial read"_test = [] {
      const std::string json = R"({
  "e": "ACCOUNT_UPDATE",
  "E": 1564745798939,
  "T": 1564745798938,
  "a": {
    "m": "ORDER",
    "B": [
      {
        "a": "USDT",
        "wb": "122624.12345678",
        "cw": "100.12345678",
        "bc": "50.12345678"
      },
      {
        "a": "BUSD",
        "wb": "1.00000000",
        "cw": "0.00000000",
        "bc": "-49.12345678"
      }
    ]
  }
})";

      AccountUpdate obj{};
      AccountUpdate::fromJson(obj, json);

      expect(std::string_view{obj.a.B.at(0).a} == "USDT");
      expect(std::string_view{obj.a.B.at(1).a} == "BUSD");
   };
};

struct meta_schema_t
{
   int x{};
   std::string file_name{};
   bool is_valid{};
};

template <>
struct glz::meta<meta_schema_t>
{
   using T = meta_schema_t;
   static constexpr auto value = object(&T::x, &T::file_name, &T::is_valid);
};

template <>
struct glz::json_schema<meta_schema_t>
{
   schema x{.description = "x is a special integer"};
   schema file_name{.description = "provide a file name to load"};
   schema is_valid{.description = "for validation"};
};

static_assert(glz::json_schema_t<meta_schema_t>);
static_assert(glz::detail::count_members<glz::json_schema_type<meta_schema_t>> > 0);

suite meta_schema_tests = [] {
   "meta_schema"_test = [] {
      meta_schema_t obj;
      std::string buffer{};
      expect(not glz::write_json(obj, buffer));
      expect(buffer == R"({"x":0,"file_name":"","is_valid":false})") << buffer;

      const auto json_schema = glz::write_json_schema<meta_schema_t>().value_or("error");
      expect(
         json_schema ==
         R"({"type":["object"],"properties":{"file_name":{"$ref":"#/$defs/std::string","description":"provide a file name to load"},"is_valid":{"$ref":"#/$defs/bool","description":"for validation"},"x":{"$ref":"#/$defs/int32_t","description":"x is a special integer"}},"additionalProperties":false,"$defs":{"bool":{"type":["boolean"]},"int32_t":{"type":["integer"],"minimum":-2147483648,"maximum":2147483647},"std::string":{"type":["string"]}},"title":"meta_schema_t"})")
         << json_schema;
   };

   "meta_schema prettified"_test = [] {
      meta_schema_t obj;
      std::string buffer{};
      expect(not glz::write_json(obj, buffer));
      expect(buffer == R"({"x":0,"file_name":"","is_valid":false})") << buffer;

      const auto json_schema = glz::write_json_schema<meta_schema_t, glz::opts{.prettify = true}>().value_or("error");
      expect(json_schema ==
             R"({
   "type": [
      "object"
   ],
   "properties": {
      "file_name": {
         "$ref": "#/$defs/std::string",
         "description": "provide a file name to load"
      },
      "is_valid": {
         "$ref": "#/$defs/bool",
         "description": "for validation"
      },
      "x": {
         "$ref": "#/$defs/int32_t",
         "description": "x is a special integer"
      }
   },
   "additionalProperties": false,
   "$defs": {
      "bool": {
         "type": [
            "boolean"
         ]
      },
      "int32_t": {
         "type": [
            "integer"
         ],
         "minimum": -2147483648,
         "maximum": 2147483647
      },
      "std::string": {
         "type": [
            "string"
         ]
      }
   },
   "title": "meta_schema_t"
})") << json_schema;
   };
};

suite glz_text_tests = [] {
   "glz_text"_test = [] {
      glz::text text = "Hello World";
      std::string out{};
      expect(not glz::write_json(text, out));
      expect(out == "Hello World");

      text.str.clear();
      expect(!glz::read_json(text, out));
      expect(text.str == "Hello World");
   };
};

struct raw_or_file_tester
{
   glz::raw_or_file input{};
   std::string name{};
};

static_assert(glz::detail::count_members<raw_or_file_tester> == 2);

suite raw_or_file_tests = [] {
   "raw_or_file"_test = [] {
      raw_or_file_tester obj{};

      std::string s{};
      expect(not glz::write_json(obj, s));
      expect(s == R"({"input":"","name":""})");

      const std::string secondary_file = "./secondary.json";
      glz::obj primary{"input", secondary_file, "name", "Edward"};
      const auto primary_json = glz::write_json(primary).value_or("error");

      {
         std::vector<int> x{1, 2, 3};
         const auto ec = glz::write_file_json(x, secondary_file, std::string{});
         expect(!bool(ec));
      }

      expect(!glz::read_json(obj, primary_json));
      expect(obj.input.str == R"([1,2,3])");
      expect(obj.name == "Edward");

      expect(not glz::write_json(obj, s));
      expect(s == R"({"input":[1,2,3],"name":"Edward"})");

      obj = {};
      expect(!glz::read_json(obj, s));
      expect(obj.input.str == R"([1,2,3])");
      expect(obj.name == "Edward");

      {
         std::string hello = "Hello from Mars";
         const auto ec = glz::write_file_json(hello, secondary_file, std::string{});
         expect(!bool(ec));
      }

      expect(!glz::read_json(obj, primary_json));
      expect(obj.input.str == R"("Hello from Mars")");
      expect(obj.name == "Edward");

      expect(not glz::write_json(obj, s));
      expect(s == R"({"input":"Hello from Mars","name":"Edward"})");

      obj = {};
      expect(!glz::read_json(obj, s));
      expect(obj.input.str == R"("Hello from Mars")");
      expect(obj.name == "Edward");
   };
};

struct animals_t
{
   std::string lion = "Lion";
   std::string tiger = "Tiger";
   std::string panda = "Panda";

   struct glaze
   {
      using T = animals_t;
      static constexpr auto value = glz::object(&T::lion, &T::tiger, &T::panda);
   };
};

struct zoo_t
{
   animals_t animals{};
   std::string name{"My Awesome Zoo"};

   struct glaze
   {
      using T = zoo_t;
      static constexpr auto value = glz::object(&T::animals, &T::name);
   };
};

struct animals_reflection_t
{
   std::string lion = "Lion";
   std::string tiger = "Tiger";
   std::string panda = "Panda";
};

struct zoo_reflection_t
{
   animals_reflection_t animals{};
   std::string name{"My Awesome Zoo"};
};

struct partial_write_tester
{
   int magnitude{};
   int thresh_hi{};
   int thresh_lo{};
   int option_one{};
   int option_two{};
};

suite partial_write_tests = [] {
   "partial write"_test = [] {
      static constexpr auto partial = glz::json_ptrs("/name", "/animals/tiger");

      zoo_t obj{};
      std::string s{};
      const auto ec = glz::write_json<partial>(obj, s);
      expect(!ec);
      expect(s == R"({"animals":{"tiger":"Tiger"},"name":"My Awesome Zoo"})") << s;
   };

   "partial write const qualified"_test = [] {
      static constexpr auto partial = glz::json_ptrs("/name", "/animals/tiger");

      const zoo_t obj{};
      std::string s{};
      const auto ec = glz::write_json<partial>(obj, s);
      expect(!ec);
      expect(s == R"({"animals":{"tiger":"Tiger"},"name":"My Awesome Zoo"})") << s;
   };

   "reflection partial write const qualified"_test = [] {
      static constexpr auto partial = glz::json_ptrs("/name", "/animals/tiger");

      const zoo_reflection_t obj{};
      std::string s{};
      const auto ec = glz::write_json<partial>(obj, s);
      expect(!ec);
      expect(s == R"({"animals":{"tiger":"Tiger"},"name":"My Awesome Zoo"})") << s;
   };

   "partial write with raw buffer"_test = [] {
      static constexpr auto json_ptrs = glz::json_ptrs("/name");
      zoo_t obj{};
      char buf[32]{};
      const auto length = glz::write_json<json_ptrs>(obj, buf);
      expect(length.has_value());
      expect(std::string_view{buf} == R"({"name":"My Awesome Zoo"})");
   };

   "partial_write_tester"_test = [] {
      static constexpr auto partial = glz::json_ptrs("/magnitude", "/thresh_hi", "/thresh_lo");
      partial_write_tester obj{};
      std::string buffer{};
      expect(not glz::write_json<partial>(obj, buffer));
      expect(buffer == R"({"magnitude":0,"thresh_hi":0,"thresh_lo":0})") << buffer;
   };
};

struct S0
{
   std::string f1{}; /*, f1misc is ignored*/
};

suite error_on_unknown_keys_test = [] {
   "error_on_unknown_keys"_test = [] {
      auto input = R"({"f1":"main","f1misc":"this should be dropped silently"})";
      S0 obj{};
      expect(!glz::read<glz::opts{.error_on_unknown_keys = false}>(obj, input));
      const auto s = glz::write_json(obj).value_or("error");
      expect(s == R"({"f1":"main"})") << s;
      expect(obj.f1 == "main");
   };
};

suite expected_tests = [] {
   "expected<std::string, int>"_test = [] {
      glz::expected<std::string, int> obj = "hello";
      std::string s{};
      expect(not glz::write_json(obj, s));
      expect(s == R"("hello")") << s;

      obj = glz::unexpected(5);
      expect(not glz::write_json(obj, s));
      expect(s == R"({"unexpected":5})") << s;

      obj = "hello";
      expect(!glz::read_json(obj, s));
      expect(!obj);
      expect(obj.error() == 5);

      s = R"("hello")";
      expect(!glz::read_json(obj, s));
      expect(bool(obj));
      expect(obj.value() == "hello");
   };
};

struct custom_struct
{
   auto operator<=>(const custom_struct&) const noexcept = default;

   std::string str{};
};

namespace glz
{
   template <>
   struct from<JSON, custom_struct>
   {
      template <auto Opts>
      static void op(custom_struct& value, auto&&... args)
      {
         parse<JSON>::op<Opts>(value.str, args...);
         value.str += "read";
      }
   };

   template <>
   struct to<JSON, custom_struct>
   {
      template <auto Opts>
      static void op(custom_struct& value, auto&&... args) noexcept
      {
         value.str += "write";
         serialize<JSON>::op<Opts>(value.str, args...);
      }

      // For std::set testing, because iterators are const
      template <auto Opts>
      static void op(const custom_struct& value, auto&&... args) noexcept
      {
         serialize<JSON>::op<Opts>(value.str, args...);
      }
   };
}

suite custom_struct_tests = [] {
   "custom_struct"_test = [] {
      custom_struct obj{};
      std::string s{};
      expect(not glz::write_json(obj, s));
      expect(s == R"("write")");

      expect(!glz::read_json(obj, s));
      expect(obj.str == R"(writeread)") << obj.str;

      using custom_struct_set = std::set<custom_struct>;

      custom_struct_set obj_set{custom_struct{"hello"}, custom_struct{"world"}};

      expect(not glz::write_json(obj_set, s));
      expect(s == R"(["hello","world"])");

      obj_set.clear();

      std::string_view withSpaces = R"(
      [
        "hello",
        "world"
      ]
      )";

      expect(!glz::read_json(obj_set, withSpaces));
      expect(obj_set == custom_struct_set{custom_struct{"helloread"}, custom_struct{"worldread"}});
   };
};

struct struct_for_volatile
{
   glz::volatile_array<uint16_t, 4> a{};
   bool b{};
   int32_t c{};
   double d{};
   uint32_t e{};
};

template <>
struct glz::meta<struct_for_volatile>
{
   using T = struct_for_volatile;
   static constexpr auto value = object(&T::a, &T::b, &T::c, &T::d, &T::e);
};

struct my_volatile_struct
{
   glz::volatile_array<uint16_t, 4> a{};
   bool b{};
   int32_t c{};
   double d{};
   uint32_t e{};
};

suite volatile_tests = [] {
   "basic volatile"_test = [] {
      volatile int i = 5;
      std::string s{};
      expect(not glz::write_json(i, s));
      expect(s == "5");
      expect(!glz::read_json(i, "42"));
      expect(i == 42);

      volatile uint64_t u = 99;
      expect(not glz::write_json(u, s));
      expect(s == "99");
      expect(!glz::read_json(u, "51"));
      expect(u == 51);
   };

   "basic volatile pointer"_test = [] {
      volatile int i = 5;
      volatile int* ptr = &i;
      std::string s{};
      expect(not glz::write_json(ptr, s));
      expect(s == "5");

      expect(!glz::read_json(i, "42"));
      expect(*ptr == 42);
      expect(i == 42);
   };

   "volatile struct_for_volatile"_test = [] {
      volatile struct_for_volatile obj{{1, 2, 3, 4}, true, -7, 9.9, 12};
      std::string s{};
      expect(not glz::write_json(obj, s));
      expect(s == R"({"a":[1,2,3,4],"b":true,"c":-7,"d":9.9,"e":12})") << s;

      obj.a.fill(0);
      obj.b = false;
      obj.c = 0;
      obj.d = 0.0;
      obj.e = 0;

      expect(!glz::read_json(obj, s));
      expect(obj.a == glz::volatile_array<uint16_t, 4>{1, 2, 3, 4});
      expect(obj.b == true);
      expect(obj.c == -7);
      expect(obj.d == 9.9);
      expect(obj.e == 12);
   };

   "volatile my_volatile_struct"_test = [] {
      volatile my_volatile_struct obj{{1, 2, 3, 4}, true, -7, 9.9, 12};
      std::string s{};
      expect(not glz::write_json(obj, s));
      expect(s == R"({"a":[1,2,3,4],"b":true,"c":-7,"d":9.9,"e":12})") << s;

      obj.a.fill(0);
      obj.b = false;
      obj.c = 0;
      obj.d = 0.0;
      obj.e = 0;

      expect(!glz::read_json(obj, s));
      expect(obj.a == glz::volatile_array<uint16_t, 4>{1, 2, 3, 4});
      expect(obj.b == true);
      expect(obj.c == -7);
      expect(obj.d == 9.9);
      expect(obj.e == 12);

      expect(glz::get<volatile uint16_t>(obj, "/a/0") == uint16_t(1));
      expect(glz::get<volatile uint16_t>(obj, "/a/1") == uint16_t(2));
      expect(glz::get<volatile bool>(obj, "/b") == true);
      expect(glz::get<volatile int32_t>(obj, "/c") == int32_t(-7));
      expect(glz::get<volatile double>(obj, "/d") == 9.9);
      expect(glz::get<volatile uint32_t>(obj, "/e") == uint32_t(12));
   };
};

struct path_test_struct
{
   uint32_t i{0};
   std::filesystem::path p{"./my_path"};
};

template <>
struct glz::meta<path_test_struct>
{
   using T = path_test_struct;
   static constexpr auto value = object(&T::i, &T::p);
};

suite filesystem_tests = [] {
   static_assert(glz::filesystem_path<std::filesystem::path>);

   "std::filesystem::path"_test = [] {
      std::filesystem::path p{"."};
      std::string s = "C:/123";
      expect(!glz::read_json(p, R"("C:/123")"));

      expect(glz::write_json(p) == R"("C:/123")");
   };

   "path_test_struct"_test = [] {
      path_test_struct obj{};
      std::string buffer = glz::write_json(obj).value_or("error");
      expect(buffer == R"({"i":0,"p":"./my_path"})");

      obj.p.clear();
      expect(!glz::read_json(obj, buffer));
      expect(obj.p == "./my_path");
   };
};

static_assert(glz::readable_array_t<std::span<double, 4>>);

struct struct_c_arrays
{
   uint16_t ints[2]{1, 2};
   float floats[1]{3.14f};

   struct glaze
   {
      using T = struct_c_arrays;
      static constexpr auto value = glz::object(&T::ints, &T::floats);
   };
};

struct struct_c_arrays_meta
{
   uint16_t ints[2]{1, 2};
   float floats[1]{3.14f};
};

template <>
struct glz::meta<struct_c_arrays_meta>
{
   using T = struct_c_arrays_meta;
   static constexpr auto value = object(&T::ints, &T::floats);
};

suite c_style_arrays = [] {
   "uint32_t c array"_test = [] {
      uint32_t arr[4] = {1, 2, 3, 4};
      std::string s{};
      expect(not glz::write_json(arr, s));
      expect(s == "[1,2,3,4]") << s;
      std::memset(arr, 0, 4 * sizeof(uint32_t));
      expect(arr[0] == 0);
      expect(!glz::read_json(arr, s));
      expect(arr[0] == 1);
      expect(arr[1] == 2);
      expect(arr[2] == 3);
      expect(arr[3] == 4);
   };

   "const double c array"_test = [] {
      const double arr[4] = {1.1, 2.2, 3.3, 4.4};
      std::string s{};
      expect(not glz::write_json(arr, s));
      expect(s == "[1.1,2.2,3.3,4.4]") << s;
   };

   "double c array"_test = [] {
      double arr[4] = {1.1, 2.2, 3.3, 4.4};
      std::string s{};
      expect(not glz::write_json(arr, s));
      expect(s == "[1.1,2.2,3.3,4.4]") << s;
      std::memset(arr, 0, 4 * sizeof(double));
      expect(arr[0] == 0.0);
      expect(!glz::read_json(arr, s));
      expect(arr[0] == 1.1);
      expect(arr[1] == 2.2);
      expect(arr[2] == 3.3);
      expect(arr[3] == 4.4);
   };

   "struct_c_arrays"_test = [] {
      struct_c_arrays obj{};
      std::string s{};
      expect(not glz::write_json(obj, s));
      expect(s == R"({"ints":[1,2],"floats":[3.14]})") << s;

      obj.ints[0] = 0;
      obj.ints[1] = 1;
      obj.floats[0] = 0.f;
      expect(!glz::read_json(obj, s));
      expect(obj.ints[0] == 1);
      expect(obj.ints[1] == 2);
      expect(obj.floats[0] == 3.14f);
   };

   "struct_c_arrays_meta"_test = [] {
      struct_c_arrays_meta obj{};
      std::string s{};
      expect(not glz::write_beve(obj, s));

      obj.ints[0] = 0;
      obj.ints[1] = 1;
      obj.floats[0] = 0.f;
      expect(!glz::read_beve(obj, s));
      expect(obj.ints[0] == 1);
      expect(obj.ints[1] == 2);
      expect(obj.floats[0] == 3.14f);
   };
};

struct sum_hash_obj_t
{
   int aa{};
   int aab{};
   int cab{};
   int zac{};
};

template <>
struct glz::meta<sum_hash_obj_t>
{
   using T = sum_hash_obj_t;
   static constexpr auto value = object(&T::aa, &T::aab, &T::cab, &T::zac);
};

suite sum_hash_obj_test = [] {
   "sum_hash_obj"_test = [] {
      sum_hash_obj_t obj{};
      const auto s = glz::write_json(obj).value_or("error");
      expect(s == R"({"aa":0,"aab":0,"cab":0,"zac":0})");
      expect(!glz::read_json(obj, s));
   };
};

struct write_precision_t
{
   double pi = std::numbers::pi_v<double>;

   struct glaze
   {
      using T = write_precision_t;
      static constexpr auto value = glz::object("pi", glz::write_float32<&T::pi>);
   };
};

struct float_opts : glz::opts
{
   glz::float_precision float_max_write_precision{};
};

suite max_write_precision_tests = [] {
   "max_write_precision"_test = [] {
      double pi = std::numbers::pi_v<double>;
      std::string json_double = glz::write_json(pi).value_or("error");

      constexpr float_opts options{{}, glz::float_precision::float32};
      std::string json_float = glz::write<options>(pi).value_or("error");
      expect(json_double != json_float);
      expect(json_float == glz::write_json(std::numbers::pi_v<float>));
      expect(!glz::read_json(pi, json_float));

      std::vector<double> double_array{pi, 2 * pi};
      json_double = glz::write_json(double_array).value_or("error");
      json_float = glz::write<options>(double_array).value_or("error");
      expect(json_double != json_float);
      expect(json_float == glz::write_json(std::array{std::numbers::pi_v<float>, 2 * std::numbers::pi_v<float>}));
      expect(!glz::read_json(double_array, json_float));
   };

   "write_precision_t"_test = [] {
      write_precision_t obj{};
      std::string json_float = glz::write<float_opts{}>(obj).value_or("error");
      expect(json_float == R"({"pi":3.1415927})") << json_float;
   };
};

struct short_keys_t
{
   int a = 1;
   int aa = 2;
   int ab = 3;
   int ba = 4;
   int bab = 5;
   int aaa = 6;
   int cab = 7;
   int bca = 8;
   int cca = 9;
};

suite short_keys_tests = [] {
   "short_keys"_test = [] {
      short_keys_t obj{};
      std::string s{};
      expect(not glz::write_json(obj, s));
      expect(s == R"({"a":1,"aa":2,"ab":3,"ba":4,"bab":5,"aaa":6,"cab":7,"bca":8,"cca":9})") << s;
      expect(!glz::read_json(obj, s));
   };
};

struct long_keys_t
{
   int axxxxxxxxxx = 1;
   int aaxxxxxxxxxx = 2;
   int abxxxxxxxxxx = 3;
   int baxxxxxxxxxx = 4;
   int babxxxxxxxxxx = 5;
   int aaaxxxxxxxxxx = 6;
   int cabxxxxxxxxxx = 7;
   int bcaxxxxxxxxxx = 8;
   int ccaxxxxxxxxxx = 9;
};

suite long_keys_tests = [] {
   "long_keys"_test = [] {
      long_keys_t obj{};
      std::string s{};
      expect(not glz::write_json(obj, s));
      expect(!glz::read_json(obj, s));
   };
};

struct skip_obj
{
   struct glaze
   {
      using T = skip_obj;
      static constexpr auto value = glz::object("str", glz::skip{}, "opt", glz::skip{});
   };
};

suite skip_tests = [] {
   "skip"_test = [] {
      skip_obj obj{};
      std::string_view json = R"({"str":"hello","opt":null})";
      expect(!glz::read_json(obj, json));
      expect(glz::write_json(obj) == "{}");
   };
};

struct specify_only_skip_obj
{
   int i{1};
   int j{2};
   int k{3};
};

template <>
struct glz::meta<specify_only_skip_obj>
{
   static constexpr bool skip(const std::string_view key, const meta_context&) { return key == "j"; }
};

suite specify_only_skip_obj_tests = [] {
   "skip_only_one"_test = [] {
      specify_only_skip_obj obj{};
      expect(glz::write_json(obj) == R"({"i":1,"k":3})") << glz::write_json(obj).value();
   };
};

struct skip_hidden_elements
{
   int i{1};
   int hidden_j{2};
   int hidden_k{3};
   int l{4};
};

template <>
struct glz::meta<skip_hidden_elements>
{
   static constexpr bool skip(const std::string_view key, const meta_context&)
   {
      return key.starts_with(std::string_view{"hidden"});
   }
};

suite skip_hidden_elements_tests = [] {
   "skip_hidden_elements"_test = [] {
      skip_hidden_elements obj{};
      expect(glz::write_json(obj) == R"({"i":1,"l":4})") << glz::write_json(obj).value();
   };
};

struct skip_first_and_last
{
   int i{1};
   int hidden_j{2};
   int hidden_k{3};
   int l{4};
};

template <>
struct glz::meta<skip_first_and_last>
{
   static constexpr bool skip(const std::string_view key, const meta_context&) { return key == "i" || key == "l"; }
};

suite skip_first_and_last_tests = [] {
   "skip_first_and_last_tests"_test = [] {
      skip_first_and_last obj{};
      expect(glz::write_json(obj) == R"({"hidden_j":2,"hidden_k":3})") << glz::write_json(obj).value();
   };
};

template <size_t N>
struct FixedName
{
   std::array<char, N> buf;
   uint16_t len{};

   struct glaze
   {
      // If you change this to an lvalue then this will cause stack overflow errors and is difficult to diagnose
      // A const lvalue will work as intended
      static constexpr auto value = [](FixedName&& self) -> auto {
         return std::string_view(self.buf.data(), self.len);
      };
   };
};

struct Address
{
   std::string test = "Hello";
};

template <>
struct glz::meta<Address>
{
   static constexpr auto value = [](Address& self) -> FixedName<10> {
      FixedName<10> val;
      std::memcpy(val.buf.data(), self.test.data(), self.test.size() + 1);
      val.len = uint16_t(self.test.size());
      return val;
   };
};

suite stack_allocated_string = [] {
   "stack_allocated_string"_test = [] {
      Address obj{};
      std::string s = glz::write_json(obj).value_or("error");
      expect(s == R"("Hello")");
   };
};

struct price_t
{
   uint64_t price{};
   uint64_t volume{};
};

struct ticker_t
{
   uint64_t time{};
   std::string exchange{};
   std::string symbol{};
   std::vector<price_t> asks{};
   std::vector<price_t> bids{};
   uint64_t price{};
   uint64_t volume{};
   uint64_t open_interest{};
   uint64_t ceiling{};
   uint64_t floor{};
};

suite ticker_tests = [] {
   std::string json = R"({
  "time": 1686621452000000000,
  "exchange": "SHFE",
  "symbol": "rb2310",
  "asks": [
    {
      "price": 3698,
      "volume": 2882
    }
  ],
  "bids": [
    {
      "price": 3693,
      "volume": 789
    }
  ],
  "price": 3693,
  "volume": 820389,
  "open_interest": 1881506,
  "ceiling": 4075,
  "floor": 3268
})";

   "ticker_t"_test = [&] {
      ticker_t obj{};

      const auto ec = glz::read_json(obj, json);
      expect(!ec) << glz::format_error(ec, json);
      std::string s = glz::write_json(obj).value_or("error");
      expect(s == glz::minify_json(json)) << s;
   };

   "vector<ticker_t>"_test = [&] {
      std::vector<ticker_t> v{};
      v.resize(4);
      for (size_t i = 0; i < v.size(); ++i) {
         expect(!glz::read_json(v[i], json));
      }

      std::string s = glz::write_json(v).value_or("error");
      const auto ec = glz::read_json(v, s);
      expect(!ec) << glz::format_error(ec, s);
   };
};

struct my_float_struct
{
   float f;
};

suite single_float_struct = [] {
   "single_float_struct"_test = [] {
      std::vector<uint8_t> buf;
      my_float_struct obj{};
      expect(not glz::write_json(obj, buf));
      std::string out{};
      out.resize(buf.size());
      std::memcpy(out.data(), buf.data(), buf.size());
      expect(out == R"({"f":0})") << out;
   };
};

struct raw_struct
{
   std::string str{};
   Color color{};
};

template <>
struct glz::meta<raw_struct>
{
   using T = raw_struct;
   static constexpr auto value = object("str", glz::raw<&T::str>, "color", glz::raw<&T::color>);
};

suite raw_test = [] {
   "raw"_test = [] {
      raw_struct obj{.str = R"("Hello")", .color = Color::Blue};
      expect(glz::write_json(obj) == R"({"str":"Hello","color":Blue})");
   };
};

struct bools_as_numbers_struct
{
   bool a{};
   bool b{};
   bool c{};
   bool d{};

   struct glaze
   {
      using T = bools_as_numbers_struct;
      static constexpr auto value =
         glz::object("a", glz::bools_as_numbers<&T::a>, "b", glz::bools_as_numbers<&T::b>, &T::c, &T::d);
   };
};

suite bools_as_numbers_test = [] {
   "bools_as_numbers"_test = [] {
      std::string s = R"({"a":1,"b":0,"c":true,"d":false})";
      bools_as_numbers_struct obj{};
      expect(!glz::read_json(obj, s));
      expect(obj.a == true);
      expect(obj.b == false);
      expect(glz::write_json(obj) == s);
   };

   "bools_as_numbers_array"_test = [] {
      std::string s = R"([1,0,1,0])";
      std::array<bool, 4> obj{};
      constexpr glz::opts opts{.bools_as_numbers = true};
      expect(!glz::read<opts>(obj, s));
      expect(glz::write<opts>(obj) == s);
   };

   "bools_as_numbers_vector"_test = [] {
      std::string s = R"([1,0,1,0])";
      std::vector<bool> obj{};
      constexpr glz::opts opts{.bools_as_numbers = true};
      expect(!glz::read<opts>(obj, s));
      expect(glz::write<opts>(obj) == s);
   };
};

struct partial_struct
{
   std::string string{};
   int32_t integer{};
};

suite read_allocated_tests = [] {
   static constexpr glz::opts partial{.partial_read = true};

   "partial_read tuple"_test = [] {
      std::string s = R"(["hello",88,"a string we don't care about"])";
      std::tuple<std::string, int> obj{};
      expect(!glz::read<partial>(obj, s));
      expect(std::get<0>(obj) == "hello");
      expect(std::get<1>(obj) == 88);
   };

   "partial_read vector"_test = [] {
      std::string s = R"([1,2,3,4,5])";
      std::vector<int> v(2);
      expect(!glz::read<partial>(v, s));
      expect(v.size() == 2);
      expect(v[0] == 1);
      expect(v[1] == 2);
   };

   "partial_read map"_test = [] {
      std::string s = R"({"1":1,"2":2,"3":3})";
      std::map<std::string, int> obj{{"2", 0}};
      expect(!glz::read<partial>(obj, s));
      expect(obj.size() == 1);
      expect(obj.at("2") == 2);
   };

   "partial_read partial_struct"_test = [] {
      std::string s = R"({"integer":400,"string":"ha!",ignore})";
      partial_struct obj{};
      expect(!glz::read<glz::opts{.partial_read = true}>(obj, s));
      expect(obj.string == "ha!");
      expect(obj.integer == 400);
   };

   "partial_read partial_struct, error_on_unknown_keys = false"_test = [] {
      std::string s = R"({"skip":null,"integer":400,"string":"ha!",ignore})";
      partial_struct obj{};
      expect(!glz::read<glz::opts{.error_on_unknown_keys = false, .partial_read = true}>(obj, s));
      expect(obj.string == "ha!");
      expect(obj.integer == 400);
   };

   "nested partial_struct"_test = [] {
      std::map<std::string, partial_struct> obj{{"one", {"ONE", 1}}, {"two", {"TWO", 2}}};
      std::string s = R"({"zero":{}, "one":{"skip":null,"integer":400,"string":"ha!","skip again":[1,2,3]}})";
      auto ec = glz::read<glz::opts{.error_on_unknown_keys = false, .partial_read = true}>(obj, s);
      expect(!ec) << glz::format_error(ec, s);
      expect(obj.at("one").string == "ha!");
      expect(obj.at("one").integer == 400);
   };
};

struct Trade
{
   int64_t T{};
   char s[16];
};

template <>
struct glz::meta<Trade>
{
   using T = Trade;
   static constexpr auto value = object(&T::T, &T::s);
};

suite raw_char_buffer_tests = [] {
   "binance_trade"_test = [] {
      const auto* payload = R"(
            {
                "T": 123456788,
                "s": "ETHBTC"
            }
        )";
      auto result = glz::read_json<Trade>(payload);
      expect(result.has_value()) << glz::format_error(result, payload);
   };
};

struct single_symbol_info_js
{
   std::string symbol;
   std::string contractType;
   std::vector<std::unordered_map<std::string, std::variant<std::string, int64_t>>> filters;
};

suite error_on_missing_keys_symbols_tests = [] {
   "error_on_missing_keys_symbols"_test = [] {
      std::string_view payload = R"(
              {
                  "symbol": "BTCUSDT",
                  "contractType": "PERPETUAL",
                  "filters": [
                      {
                          "filterType": "PRICE_FILTER",
                          "minPrice": "0.01",
                          "maxPrice": "1000000",
                          "tickSize": "0.01"
                      },
                      {
                          "filterType": "MAX_NUM_ORDERS",
                          "maxNumOrders": 200
                      },
                      {
                          "filterType": "MAX_NUM_ALGO_ORDERS",
                          "maxNumAlgoOrders": 5
                      },
                      {
                          "filterType": "MAX_NUM_ICEBERG_ORDERS",
                          "maxNumIcebergOrders": 10
                      },
                      {
                          "filterType": "MAX_POSITION",
                          "maxPosition": 1000000
                      }
                  ]
              }
          )";

      single_symbol_info_js result;
      auto ec = glz::read<glz::opts{
                             .error_on_unknown_keys = false,
                             .error_on_missing_keys = true,
                             .quoted_num = false,
                          },
                          single_symbol_info_js>(result, payload);
      expect(not ec);
   };
};

struct large_struct_t
{
   bool a = false;
   bool b = false;
   bool c = false;
   bool d = false;
   bool e = false;
   bool f = false;
   bool g = false;
   bool h = false;
   bool i = false;
   bool j = false;
   bool k = false;
   bool l = false;
   bool m = false;
   bool n = false;
   bool o = false;
   bool p = false;
   bool q = false;
   bool r = false;
   bool s = false;
   bool t = false;
   bool u = false;
   bool v = false;
   bool w = false;
   bool x = false;
   bool y = false;
   bool z = false;
   bool one = false;
   bool two = false;
   bool three = false;
   bool four = false;
   bool five = false;
   bool six = false;
   bool seven = false;
};

template <>
struct glz::meta<large_struct_t>
{
   using T = large_struct_t;
   static constexpr auto value =
      object(&T::a, &T::b, &T::c, &T::d, &T::e, &T::f, &T::g, &T::h, &T::i, &T::j, &T::k, &T::l, &T::m, &T::n, &T::o,
             &T::p, &T::q, &T::r, &T::s, &T::t, &T::u, &T::v, &T::w, &T::x, &T::y, &T::z, &T::one, &T::two, &T::three,
             &T::four, &T::five, &T::six, &T::seven);
};

suite large_struct_tests = [] {
   "large_struct"_test = [] {
      large_struct_t obj{};
      std::string s = glz::write_json(obj).value_or("error");
      expect(
         s ==
         R"({"a":false,"b":false,"c":false,"d":false,"e":false,"f":false,"g":false,"h":false,"i":false,"j":false,"k":false,"l":false,"m":false,"n":false,"o":false,"p":false,"q":false,"r":false,"s":false,"t":false,"u":false,"v":false,"w":false,"x":false,"y":false,"z":false,"one":false,"two":false,"three":false,"four":false,"five":false,"six":false,"seven":false})")
         << s;
      expect(not glz::read_json(obj, s));
   };
};

struct thread_msg
{
   uint64_t id{};
   std::string val{};
};

suite threading_tests = [] {
   "threading"_test = [] {
      auto serialize = [](const auto& msg) {
         std::vector<uint8_t> buf{};
         if (glz::write_json(msg, buf)) {
            std::abort();
         }
         buf.push_back('\0');
         return buf;
      };

      auto deserialize = [](std::vector<uint8_t>&& stream) -> std::optional<thread_msg> {
         thread_msg msg{};
         auto err = glz::read_json(msg, stream);

         if (err) {
            return {};
         }
         return {msg};
      };

      std::array<std::future<void>, 8> threads{};
      for (uint_fast32_t i = 0; i < threads.size(); i++) {
         threads[i] = std::async([&] {
            thread_msg msg{20, "five hundred"};
            for (size_t j = 0; j < 1'000; ++j) {
               auto res = serialize(msg);
               auto msg2 = deserialize(std::move(res));
               if (msg2->id != msg.id || msg2->val != msg.val) {
                  std::abort();
               }
            }
         });
      }
   };
};

// JSON concepts
static_assert(glz::json_string<std::string>);
static_assert(glz::json_string<std::string_view>);
static_assert(glz::json_object<my_struct>);
static_assert(glz::json_array<std::array<float, 3>>);
static_assert(glz::json_boolean<bool>);
static_assert(glz::json_number<float>);
static_assert(glz::json_integer<uint64_t>);
static_assert(glz::json_null<std::nullptr_t>);

suite directory_tests = [] {
   "directory"_test = [] {
      std::map<std::filesystem::path, my_struct> files{{"./dir/alpha.json", {}}, {"./dir/beta.json", {.i = 0}}};
      expect(not glz::write_directory(files, "./dir"));

      std::map<std::filesystem::path, my_struct> input{};
      expect(not glz::read_directory(input, "./dir"));
      expect(input.size() == 2);
      expect(input.contains("./dir/alpha.json"));
      expect(input.contains("./dir/beta.json"));
      expect(input["./dir/beta.json"].i == 0);
   };
};

struct WorkshopModConfig
{
   uint32_t type;

   std::string title;
   std::string version;
   std::string author;
};

suite msvc_ice_tests = [] {
   "WorkshopModConfig"_test = [] {
      std::string buffer;

      WorkshopModConfig settings{};

      auto ec = glz::write<glz::opts{
         .comments = true, .error_on_unknown_keys = true, .skip_null_members = true, .prettify = false}>(settings,
                                                                                                         buffer);
      expect(not ec) << glz::format_error(ec, buffer);
   };
};

suite error_codes_test = [] {
   expect(glz::format_error(glz::error_ctx{glz::error_code::none}) == "none");
   expect(glz::format_error(glz::error_ctx{glz::error_code::expected_brace}) == "expected_brace");
};

struct front_16_t
{
   int aa{0};
   int ab{0};
   int acc{0};
   int cb{0};
};

static_assert(bool(glz::hash_info<front_16_t>.type));

suite front_16_test = [] {
   "front_16"_test = [] {
      front_16_t obj{};

      std::string buffer = R"({"aa":1,"ab":2,"acc":3,"cb":4})";
      auto ec = glz::read_json(obj, buffer);
      expect(not ec) << glz::format_error(ec, buffer);
      expect(obj.aa = 1);
      expect(obj.ab = 2);
      expect(obj.acc = 3);
      expect(obj.cb = 4);
   };
};

struct custom_errors_t
{
   uint32_t a{};
   uint32_t alpha{};
};

template <>
struct glz::meta<custom_errors_t>
{
   using T = custom_errors_t;
   static constexpr auto value = object(&T::a, &T::alpha);
};

namespace glz
{
   template <>
   struct from<JSON, custom_errors_t>
   {
      template <auto Opts>
      static void op(auto&, is_context auto& ctx, auto&&...)
      {
         ctx.custom_error_message = "custom_errors_t read error";
      }
   };

   template <>
   struct to<JSON, custom_errors_t>
   {
      template <auto Opts>
      static void op(auto&, is_context auto& ctx, auto&&...) noexcept
      {
         ctx.custom_error_message = "custom_errors_t write error";
      }
   };
}

suite custom_error = [] {
   "custom_errors_t"_test = [] {
      custom_errors_t obj{};

      std::string buffer{};
      auto ec = glz::write_json(obj, buffer);
      expect(ec.custom_error_message == "custom_errors_t write error");

      ec = glz::read_json(obj, "{}");
      expect(ec.custom_error_message == "custom_errors_t read error");
   };
};

struct var_a
{
   int m1;

   struct glaze
   {
      static constexpr auto value = glz::object("a", &var_a::m1);
   };
};

struct var_b
{
   std::vector<var_a> m1;
   bool m2;

   struct glaze
   {
      static constexpr auto value = glz::object("b", &var_b::m1, "c", &var_b::m2);
   };
};

struct var_c
{
   std::vector<var_a> m1;
   struct glaze
   {
      static constexpr auto value = &var_c::m1;
   };
};

struct var_abc_t
{
   std::variant<var_a, var_b, var_c> m1;
   struct glaze
   {
      static constexpr auto value = &var_abc_t::m1;
   };
};

suite nested_variants = [] {
   "nested_variants"_test = [] {
      var_abc_t v{};

      auto ec = glz::read_json(v, std::string{R"({"a":5})"});

      expect(not ec) << glz::format_error(ec);
      expect(std::get<var_a>(v.m1).m1 == 5);
   };
};

suite minify_prettify_safety = [] {
   "invalid minify"_test = [] {
      std::string buffer("f");
      auto minified = glz::minify_json(buffer);
      expect(minified == "false");

      buffer = "\"";
      minified = glz::minify_json(buffer);
      expect(minified == "");

      buffer = "\" ";
      minified = glz::minify_json(buffer);
      expect(minified == "");
   };

   "invalid prettify"_test = [] {
      std::string_view buffer = "\"";
      auto prettified = glz::prettify_json(buffer);
      expect(prettified == "");
   };

   "invalid prettify"_test = [] {
      std::array<char, 4> buffer{'7', '7', '7', '['}; // non-null terminated
      auto prettified = glz::prettify_json(buffer);
      expect(prettified == "777[") << prettified;
   };

   "prettify"_test = [] {
      const char* d = "{{{{{{{{{{{[{{{[{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{";
      std::vector<char> buf{d, d + std::strlen(d)};
      buf.push_back('\0');
      auto beautiful = glz::prettify_json(buf);
   };
};

struct TestSettingsData
{
   std::string VERSION = "0.0.2";

   std::map<std::string, float> video = {{"scale", 0.5F}, {"monitor", 2.F}};
   std::map<std::string, std::string> controls = {{"jump", "A"}, {"crouch", "L_CNTRL"}};

   std::string username = "MISSING";
};

struct opts_allow_conversions : glz::opts
{
   bool allow_conversions = true;
};

suite TestSettingsData_test = [] {
   "TestSettingsData"_test = [] {
      TestSettingsData obj{};
      std::string buffer{};
      expect(not glz::write_json(obj, buffer));
      auto ec = glz::read_json(obj, buffer);
      expect(not ec) << glz::format_error(ec, buffer);
   };

   static constexpr opts_allow_conversions write_options{{glz::opts{.comments = true, .prettify = true}}};
   static constexpr opts_allow_conversions read_options{{glz::opts{
      .comments = true, .error_on_unknown_keys = false, .skip_null_members = true, .error_on_missing_keys = false}}};

   "TestSettingsData options"_test = [] {
      TestSettingsData obj{};
      std::string buffer{};
      expect(not glz::write<write_options>(obj, buffer));
      auto ec = glz::read<read_options>(obj, buffer);
      expect(not ec) << glz::format_error(ec, buffer);
   };

   "TestSettingsData options file"_test = [] {
      TestSettingsData obj{};
      std::string buffer{};
      expect(not glz::write_file_json<write_options>(obj, "test_settings.json", buffer));
      auto ec = glz::read_file_json<read_options>(obj, "test_settings.json", buffer);
      expect(not ec) << glz::format_error(ec, buffer);
   };
};

suite depth_limits_test = [] {
   // test to check for guard against stack overflow
   "massive [ depth"_test = [] {
      std::string buffer{};
      buffer.insert(0, 4096, '[');
      glz::generic json{};
      auto ec = glz::read_json(json, buffer);
      expect(ec);
   };

   "massive { depth"_test = [] {
      std::string buffer{};
      buffer.insert(0, 4096, '{');
      glz::generic json{};
      auto ec = glz::read_json(json, buffer);
      expect(ec);
   };

   "depth should be valid: 1"_test = [] {
      std::string buffer = R"({"keys":[)";
      for (size_t i = 0; i < 512; ++i) {
         buffer += R"({ "key": 1 },)";
         buffer += "\n";
      }
      buffer += R"({ "key": 1 }]})";
      glz::generic json{};
      auto ec = glz::read_json(json, buffer);
      expect(not ec) << glz::format_error(ec, buffer);
   };

   "depth should be valid: 2"_test = [] {
      std::string buffer = R"({"arrays":[)";
      for (size_t i = 0; i < 512; ++i) {
         buffer += R"([ 1, 2 ],)";
         buffer += "\n";
      }
      buffer += R"([ 1, 2 ]]})";
      glz::generic json{};
      auto ec = glz::read_json(json, buffer);
      expect(not ec) << glz::format_error(ec, buffer);
   };

   "depth should be valid: mixed"_test = [] {
      std::string buffer = R"({"values":[)";
      for (size_t i = 0; i < 512; ++i) {
         buffer += R"({ "key": 1 },)";
         buffer += "\n";
         buffer += R"([ 1, 2 ],)";
         buffer += "\n";
      }
      buffer += R"([ 1, 2 ]]})";
      glz::generic json{};
      auto ec = glz::read_json(json, buffer);
      expect(not ec) << glz::format_error(ec, buffer);
   };
};

struct hammerhead_t
{
   double length{};
};

struct mako_t
{
   double length{};
};

using shark_t = std::variant<hammerhead_t, mako_t>;

template <>
struct glz::meta<shark_t>
{
   static constexpr std::string_view tag = "name";
   static constexpr auto ids = std::array{"hammerhead", "mako"};
};

using shark_ptr_t = std::variant<std::shared_ptr<hammerhead_t>, std::shared_ptr<mako_t>>;

template <>
struct glz::meta<shark_ptr_t>
{
   static constexpr std::string_view tag = "name";
   static constexpr auto ids = std::array{"hammerhead", "mako"};
};

struct chair_t
{
   float height{};
   uint8_t number_of_legs{};
   bool has_back{};
};

struct bed_t
{
   float height{};
   bool has_headboard{};
};

using furniture_ptr_t = std::variant<std::shared_ptr<chair_t>, std::shared_ptr<bed_t>>;

suite shark_variant = [] {
   "shark_variant"_test = [] {
      shark_t shark{};
      auto ec = glz::read_json(shark, R"({"name":"mako","length":44.0})");
      expect(!ec);
      expect(std::holds_alternative<mako_t>(shark));
      expect(std::get<mako_t>(shark).length == 44.0);
   };

   "shark_ptr variant"_test = [] {
      shark_ptr_t shark{};
      auto ec = glz::read_json(shark, R"({"name":"mako","length":44.0})");
      expect(!ec);
      expect(std::holds_alternative<std::shared_ptr<mako_t>>(shark));
      expect(std::get<std::shared_ptr<mako_t>>(shark)->length == 44.0);
   };

   "furniture_ptr variant auto-deduction "_test = [] {
      furniture_ptr_t furniture{};
      auto ec = glz::read_json(furniture, R"({"height":44.0,"has_headboard":true})");
      expect(!ec);
      expect(std::holds_alternative<std::shared_ptr<bed_t>>(furniture));
      expect(std::get<std::shared_ptr<bed_t>>(furniture)->height == 44.0f);
      expect(std::get<std::shared_ptr<bed_t>>(furniture)->has_headboard);
   };
};

suite string_view_value_tests = [] {
   "string_view value"_test = [] {
      std::string_view value = "";
      expect(not glz::read_json(value, R"("hello")"));
      expect(value == "hello");
   };
};

suite array_char_tests = [] {
   "array<char... value"_test = [] {
      std::array<char, 8> value{};
      expect(not glz::read_json(value, R"("hello")"));
      expect(std::string_view{value.data()} == "hello");
      expect(glz::write_json(value).value_or("error") == R"("hello")");
      expect(glz::read_json(value, R"("hello---too long")"));
      expect(not glz::read_json(value, R"("bye")"));
      expect(std::string_view{value.data()} == "bye");
   };
};

template <size_t N>
struct naive_static_string_t
{
   using value_type = char;
   using size_type = size_t;

   naive_static_string_t() = default;
   naive_static_string_t(std::string_view sv) { assign(sv.data(), sv.size()); }
   operator std::string_view() const { return std::string_view(buffer, length); }

   size_t size() const { return length; }
   size_t capacity() const { return N; }
   const char* data() const { return buffer; }

   naive_static_string_t& assign(const char* v, size_t sz)
   {
      const auto bytes_to_copy = (std::min)(N, sz);
      length = bytes_to_copy;
      memcpy(buffer, v, bytes_to_copy);
      return *this;
   }

   void resize(size_t sz)
   {
      const auto bytes_to_keep = (std::min)(N, sz);
      length = bytes_to_keep;
   }

   size_t length{};
   char buffer[N]{};
};

template <size_t N>
struct glz::meta<naive_static_string_t<N>>
{
   static constexpr auto glaze_static_string = true;
};

static_assert(std::constructible_from<std::string_view, std::decay_t<naive_static_string_t<3>>>);
static_assert(glz::has_assign<naive_static_string_t<3>>);
static_assert(glz::is_static_string<naive_static_string_t<3>>);
static_assert(glz::static_string_t<naive_static_string_t<3>>);

suite static_string_tests = [] {
   "static_str<N> value"_test = [] {
      naive_static_string_t<6> value{};
      expect(not glz::read_json(value, R"("hello")"));
      expect(std::string_view{value} == "hello");
      expect(glz::write_json(value).value_or("error") == R"("hello")");

      expect(not glz::read_json(value, R"("hello!")"));
      expect(std::string_view{value} == "hello!");
      expect(glz::write_json(value).value_or("error") == R"("hello!")");

      // too long!!
      expect(glz::read_json(value, R"("hello!!")"));

      expect(not glz::read_json(value, R"("bye")"));
      expect(std::string_view{value} == "bye");
   };
};

template <class T>
struct response_t
{
   T& result;
   uint32_t id{};
   std::optional<std::string> error{};
};

template <class T>
response_t(T) -> response_t<T>;

template <>
struct response_t<void>
{
   uint32_t id{};
   std::optional<std::string> error{};
};

template <class V>
struct glz::meta<response_t<V>>
{
   using T = response_t<V>;
   static constexpr auto value =
      object("result", [](auto& s) -> auto& { return s.result; }, "id", &T::id, "error", &T::error);
};

template <>
struct glz::meta<response_t<void>>
{
   using T = response_t<void>;
   static constexpr auto value = object("id", &T::id, "error", &T::error);
};

struct float_entry
{
   std::string name{};
   float value{};
};

suite response_test = [] {
   "response"_test = [] {
      std::vector<float_entry> entries{{"bright", 5.6f}, {"dull", 6.7f}};
      response_t res{entries};

      using T = decltype(res);
      static_assert(std::same_as<glz::refl_t<T, 0>, std::vector<float_entry>&>);
      static_assert(std::same_as<glz::refl_t<T, 1>, uint32_t&>);
      static_assert(std::same_as<glz::refl_t<T, 2>, std::optional<std::string>&>);
      std::string buffer{};
      expect(not glz::write_json(res, buffer));
      expect(buffer == R"({"result":[{"name":"bright","value":5.6},{"name":"dull","value":6.7}],"id":0})") << buffer;
   };
};

struct A_empty
{};

struct B_empty
{};

using C_empty = std::variant<A_empty, B_empty>;

template <>
struct glz::meta<C_empty>
{
   static constexpr std::string_view tag = "op";
};

suite empty_variant_testing = [] {
   "empty_variant 1"_test = [] {
      std::string_view text = R"({"xxx":"x","op":"B_empty"})";

      C_empty c;
      auto ec = glz::read<glz::opts{.error_on_unknown_keys = false, .error_on_missing_keys = true}>(c, text);
      expect(not ec) << glz::format_error(ec, text);
      expect(c.index() == 1);
   };

   "empty_variant 2"_test = [] {
      std::string_view text = R"({"xx":"x","op":"B_empty"})";

      C_empty c;
      auto ec = glz::read<glz::opts{.error_on_unknown_keys = false, .error_on_missing_keys = true}>(c, text);
      expect(not ec) << glz::format_error(ec, text);
      expect(c.index() == 1);
   };
};

enum struct fishes { salmon, shark, tuna };

template <>
struct glz::meta<fishes>
{
   using enum fishes;
   static constexpr std::array keys{"salmon", "shark", "tuna"};
   static constexpr std::array value{salmon, shark, tuna};
};

suite meta_keys_tests = [] {
   "fishes enum"_test = [] {
      using enum fishes;
      fishes fish = shark;
      auto json = glz::write_json(fish).value();
      expect(json == R"("shark")");

      fish = salmon;
      expect(not glz::read_json(fish, json));
      expect(fish == shark);

      fish = tuna;
      json = glz::write_json(fish).value();
      expect(json == R"("tuna")");

      fish = salmon;
      expect(not glz::read_json(fish, json));
      expect(fish == tuna);
   };
};

struct point3d
{
   int x{}, y{}, z{};
};

suite simple_mod_hashes = [] {
   "mod hash"_test = [] {
      point3d obj{};
      expect(not glz::read_json(obj, R"({"x":1,"y":2,"z":3})"));
      expect(obj.x == 1);
      expect(obj.y == 2);
      expect(obj.z == 3);
   };
};

struct same_length_keys
{
   int abcdefghijk{0};
   int abcdefghijl{1};
   int abcdefghijm{2};
   int abcdefghijn{3};
   int abcdefghikk{4};
   int abcdefghill{5};
   int abcdefghlll{6};
};

suite same_length_keys_test = [] {
   "same_length_keys"_test = [] {
      static constexpr auto info = glz::make_keys_info(glz::reflect<same_length_keys>::keys);
      static_assert(info.type == glz::hash_type::full_flat);

      same_length_keys obj{};
      std::string buffer{};
      expect(not glz::write_json(obj, buffer));
      expect(not glz::read_json(obj, buffer));
      expect(obj.abcdefghill == 5);
      expect(obj.abcdefghlll == 6);
   };
};

struct offset_one
{
   int abcdefghijk{0};
   int abcdefghijl{1};
   int abcdefghijm{2};
   int abcdefghijn{3};
   int abcdefghikk{4};
   int abcdefghill{5};
   int abcdefghlllo{6};
};

suite offset_one_test = [] {
   "offset_one"_test = [] {
      static constexpr auto info = glz::make_keys_info(glz::reflect<same_length_keys>::keys);
      static_assert(info.type == glz::hash_type::full_flat);

      offset_one obj{};
      std::string buffer{};
      expect(not glz::write_json(obj, buffer));
      expect(not glz::read_json(obj, buffer));
      expect(obj.abcdefghill == 5);
      expect(obj.abcdefghlllo == 6);
   };
};

suite flat_map_tests = [] {
   "flat_map"_test = [] {
      glz::flat_map<std::string_view, int> map{{"one", 1}, {"two", 2}};

      std::string buffer{};
      expect(not glz::write_json(map, buffer));
      expect(buffer == R"({"one":1,"two":2})") << buffer;
      map.clear();
      expect(not glz::read_json(map, buffer));
      expect(map.at("one") == 1);
      expect(map.at("two") == 2);
   };
};

struct Foo
{
   int x;
};

suite ndjson_options = [] {
   "ndjson_options"_test = [] {
      std::vector<Foo> assets{};

      struct opts : glz::opts
      {
         bool validate_skipped = true;
      };

      const auto ec = glz::read<opts{{glz::opts{.format = glz::NDJSON, .error_on_unknown_keys = false}}}>(
         assets, "{\"x\":1}\n{\"x\":2}");
      expect(not ec);
   };
};

suite atomics = [] {
   "atomics"_test = [] {
      std::atomic<int> i{};
      static_assert(glz::is_atomic<decltype(i)>);
      expect(not glz::read_json(i, R"(55)"));
      expect(i.load() == 55);

      std::string buffer{};
      expect(not glz::write_json(i, buffer));
      expect(buffer == R"(55)");

      std::atomic<bool> b{};
      expect(not glz::read_json(b, R"(true)"));
      expect(b);

      expect(not glz::write_json(b, buffer));
      expect(buffer == R"(true)");
   };
};

namespace trr
{
   struct Address
   {
      std::string street;
   };

   struct Person
   {
      Person(Address* const p_add) : p_add(p_add) {};
      std::string name;
      Address* const p_add; // pointer is const, Address object is mutable
   };
}

template <>
struct glz::meta<trr::Person>
{
   using T = trr::Person;
   static constexpr auto value = object(&T::name, &T::p_add);
};

suite const_pointer_tests = [] {
   "const pointer"_test = [] {
      std::string buffer = R"({"name":"Foo Bar","p_add":{"street":"Baz Yaz"}})";
      trr::Address add{};
      trr::Person p{&add};
      auto ec = glz::read<glz::opts{.format = glz::JSON, .error_on_const_read = true}>(p, buffer);
      if (ec) {
         std::cout << glz::format_error(ec, buffer) << std::endl;
      }
      expect(p.name == "Foo Bar");
      expect(p.p_add->street == "Baz Yaz");
   };
};

template <class T>
struct custom_nullable_t
{
   std::optional<T> val{};

   bool has_value() const { return val.has_value(); }

   T& value() { return *val; }

   const T& value() const { return *val; }

   template <class... Args>
   void emplace(Args&&... args)
   {
      val.emplace(std::forward<Args>(args)...);
   }
};

struct custom_nullable_container_t
{
   custom_nullable_t<double> x{};
};

suite custom_nullable_with_specialization = [] {
   "custom_nullable_with_specialization"_test = [] {
      custom_nullable_t<double> obj{};
      obj.val = 3.14;
      std::string buffer{};
      expect(not glz::write_json(obj, buffer));
      obj.val = {};
      expect(not glz::read_json(obj, buffer));
      expect(obj.val == 3.14);
      obj.val = {};
      expect(not glz::write_json(obj, buffer));
      expect(buffer == "null");
   };

   "custom_nullable_with_specialization_container"_test = [] {
      custom_nullable_container_t obj{};
      obj.x.val = 3.14;
      std::string buffer{};
      expect(not glz::write_json(obj, buffer));
      obj.x.val = {};
      expect(not glz::read_json(obj, buffer));
      expect(obj.x.val == 3.14);
      obj.x.val = {};
      expect(not glz::write_json(obj, buffer));
      expect(buffer == "{}");
   };
};

struct A1
{
   int p{};
};

struct B1
{
   float p{};
};

using X1 = std::variant<A1>;
using Y1 = std::variant<A1, B1>;

template <>
struct glz::meta<A1>
{
   static constexpr auto value = object("p", &A1::p);
};

template <>
struct glz::meta<B1>
{
   static constexpr auto value = object("p", &B1::p);
};

template <>
struct glz::meta<X1>
{
   static constexpr std::string_view tag = "tag";
};

template <>
struct glz::meta<Y1>
{
   static constexpr std::string_view tag = "tag";
};

suite variant_tag_tests = [] {
   "variant tag"_test = [] {
      auto xString = glz::write_json(X1(A1()));
      expect(xString.has_value());

      auto x = glz::read_json<X1>(*xString);
      expect(bool(x));
      if (not x.has_value()) {
         std::cerr << glz::format_error(x.error(), *xString);
      }
   };
};

struct birds
{
   std::string crow{};
   std::string sparrow{};
   std::string hawk{};
};

template <>
struct glz::meta<birds>
{
   using T = birds;
   static constexpr std::array keys{"crow", "sparrow", "hawk"};
   static constexpr glz::tuple value{&T::crow, &T::sparrow, &T::hawk};
};

suite meta_keys_for_struct = [] {
   "meta_keys birds"_test = [] {
      birds obj{"caw", "chirp", "screech"};

      std::string buffer{};
      expect(not glz::write_json(obj, buffer));
      expect(buffer == R"({"crow":"caw","sparrow":"chirp","hawk":"screech"})") << buffer;

      obj = {};
      expect(not glz::read_json(obj, buffer));
      expect(obj.crow == "caw");
      expect(obj.sparrow == "chirp");
      expect(obj.hawk == "screech");
   };
};

struct append_obj
{
   std::vector<std::string> names{};
   std::vector<std::array<int, 2>> arrays{};
};

template <>
struct glz::meta<append_obj>
{
   using T = append_obj;
   static constexpr auto value = object("names", append_arrays<&T::names>, "arrays", append_arrays<&T::arrays>);
};

suite append_arrays_tests = [] {
   "append_arrays vector"_test = [] {
      std::vector<int> v{};
      constexpr glz::opts append_opts{.append_arrays = true};
      expect(not glz::read<append_opts>(v, "[1,2,3]"));
      expect(v == std::vector<int>{1, 2, 3});
      expect(not glz::read<append_opts>(v, "[4,5,6]"));
      expect(v == std::vector<int>{1, 2, 3, 4, 5, 6});
   };

   "append_arrays deque"_test = [] {
      std::deque<int> v{};
      constexpr glz::opts append_opts{.append_arrays = true};
      expect(not glz::read<append_opts>(v, "[1,2,3]"));
      expect(v == std::deque<int>{1, 2, 3});
      expect(not glz::read<append_opts>(v, "[4,5,6]"));
      expect(v == std::deque<int>{1, 2, 3, 4, 5, 6});
   };

   "append_arrays append_obj"_test = [] {
      append_obj obj{};
      expect(not glz::read_json(obj, R"({"names":["Bob"],"arrays":[[0,0]]})"));
      expect(obj.names == std::vector<std::string>{"Bob"});
      expect(obj.arrays == std::vector<std::array<int, 2>>{{0, 0}});

      expect(not glz::read_json(obj, R"({"names":["Liz"],"arrays":[[1,1]]})"));
      expect(obj.names == std::vector<std::string>{"Bob", "Liz"});
      expect(obj.arrays == std::vector<std::array<int, 2>>{{0, 0}, {1, 1}});
   };
};

suite asan_test = [] {
   "asan_non_null_terminated"_test = [] {
      const std::string_view data = R"({"x":"")";
      const auto heap_buf = std::make_unique_for_overwrite<char[]>(data.size());
      std::ranges::copy(data, heap_buf.get());
      const std::string_view buf{heap_buf.get(), data.size()};

      constexpr glz::opts OPTS{
         .null_terminated = false,
         .error_on_unknown_keys = false,
         .minified = true,
      };

      struct
      {
      } t;
      expect(glz::read<OPTS>(t, buf));
   };
};

struct Number
{
   std::optional<double> minimum;
   std::optional<double> maximum;
};

template <>
struct glz::meta<Number>
{
   static constexpr auto value = glz::object(&Number::minimum, &Number::maximum);
};

struct Boolean
{};

template <>
struct glz::meta<Boolean>
{
   static constexpr auto value = glz::object();
};

struct Integer
{
   std::optional<int> minimum;
   std::optional<int> maximum;
};

template <>
struct glz::meta<Integer>
{
   static constexpr auto value = glz::object(&Integer::minimum, &Integer::maximum);
};

using Data = std::variant<Number, Integer>;

template <>
struct glz::meta<Data>
{
   static constexpr std::string_view tag = "type";
   static constexpr auto ids = std::array{"number", "integer"};
};

struct Array
{
   Data items;
};

template <>
struct glz::meta<Array>
{
   static constexpr auto value = glz::object(&Array::items);
};

using Data2 = std::variant<Number, Boolean>;

template <>
struct glz::meta<Data2>
{
   static constexpr std::string_view tag = "type";
   static constexpr auto ids = std::array{"number", "boolean"};
};

struct Array2
{
   Data2 items;
};

template <>
struct glz::meta<Array2>
{
   static constexpr auto value = glz::object(&Array2::items);
};

suite tagged_variant_null_members = [] {
   "tagged_variant_null_members"_test = [] {
      Array var = Array{Number{}};

      std::string s{};
      expect(not glz::write_json(var, s));
      expect(s == R"({"items":{"type":"number"}})") << s;
   };

   "variant deduction"_test = [] {
      Array2 var;
      std::string str = R"({"items": { "type" : "boolean"}})";

      auto pe = glz::read_json(var, str);
      expect(not pe) << glz::format_error(pe, str);
   };
};

class private_fields_t
{
  private:
   double cash = 22.0;
   std::string currency = "$";

   friend struct glz::meta<private_fields_t>;
};

template <>
struct glz::meta<private_fields_t>
{
   using T = private_fields_t;
   static constexpr auto value = object(&T::cash, &T::currency);
};

suite private_fields_tests = [] {
   "private fields"_test = [] {
      private_fields_t obj{};
      std::string buffer{};
      expect(not glz::write_json(obj, buffer));
      expect(buffer == R"({"cash":22,"currency":"$"})");

      buffer = R"({"cash":2200.0, "currency":"¢"})";
      expect(not glz::read_json(obj, buffer));
      buffer.clear();
      expect(not glz::write_json(obj, buffer));
      expect(buffer == R"({"cash":2200,"currency":"¢"})");
   };
};

struct ImmutableStruct
{
   const int val1;
   const float val2;
};

struct MyStruct
{
   std::vector<ImmutableStruct> vals;
};

struct ImmutableStructInserter_t
{
   std::vector<ImmutableStruct>& container;
   int parsedInt{};
   float parsedFloat{};
};

template <auto Member>
constexpr auto ImmutableStructInserter = [] { return [](auto&& v) { return ImmutableStructInserter_t{v.*Member}; }; }();

template <>
struct glz::meta<ImmutableStructInserter_t>
{
   using T = ImmutableStructInserter_t;
   static constexpr auto insert = [](ImmutableStructInserter_t& s) {
      s.container.emplace_back(s.parsedInt, s.parsedFloat);
      return true;
   };

   static constexpr auto value = object("val1", &T::parsedInt, "val2", manage<&T::parsedFloat, insert, nullptr>);
};

template <>
struct glz::meta<MyStruct>
{
   using T = MyStruct;
   static constexpr auto value = object("vals", custom<array_apply<ImmutableStructInserter<&T::vals>>, &T::vals>);
};

suite immutable_array_read_tests = [] {
   "immutable_read"_test = [] {
      MyStruct myStruct;
      myStruct.vals.emplace_back(1, 1.1f);
      myStruct.vals.emplace_back(2, 2.1f);
      myStruct.vals.emplace_back(3, 3.1f);

      std::string buffer = glz::write_json(myStruct).value_or("error");

      myStruct.vals.clear();
      expect(not glz::read_json(myStruct, buffer));
      buffer = glz::write<glz::opts{.format = glz::JSON}>(myStruct).value_or("error");
      expect(buffer == R"({"vals":[{"val1":1,"val2":1.1},{"val1":2,"val2":2.1},{"val1":3,"val2":3.1}]})") << buffer;
   };
};

suite factor8_strings = [] {
   // string parsing invokes separate path when there's >= 8 chars to parse.  There was a bug where
   // inputs of exact factors of 8 chars caused overwriting, therefore not terminated correctly.

   "exactly 8"_test = [] {
      const auto payload = R"("abcdefg")"; // 8 chars after open quote
      const auto parsed = glz::read_json<std::string>(payload);
      expect(parsed.has_value());
      expect(*parsed->end() == '\0');
   };

   "factor of 8"_test = [] {
      const auto payload = R"("abcdefghijklmno")"; // 16 chars after open quote
      const auto parsed = glz::read_json<std::string>(payload);
      expect(parsed.has_value());
      expect(*parsed->end() == '\0');
   };
};

struct cast_obj
{
   int integer{};
};

template <>
struct glz::meta<cast_obj>
{
   using T = cast_obj;
   static constexpr auto value = object("integer", cast<&T::integer, double>, //
                                        "indirect", cast<[](T& s) -> auto& { return s.integer; }, double>);
};

struct cast_nullable_obj
{
   std::optional<int> a;
   std::string b;
};

template <>
struct glz::meta<cast_nullable_obj>
{
   using T = cast_nullable_obj;
   static constexpr auto value = object("a", cast<&T::a, std::optional<double>>, "b", &T::b);
};

suite cast_tests = [] {
   "cast"_test = [] {
      cast_obj obj{};

      std::string buffer = R"({"integer":5.7})";
      expect(not glz::read_json(obj, buffer));

      expect(obj.integer == 5);

      obj.integer = 77;
      expect(not glz::write_json(obj, buffer));
      expect(buffer == R"({"integer":77,"indirect":77})");

      buffer = R"({"indirect":33.5})";
      expect(not glz::read_json(obj, buffer));
      expect(obj.integer == 33);

      obj.integer = 77;
      expect(not glz::write_json(obj, buffer));
      expect(buffer == R"({"integer":77,"indirect":77})");
   };

   "cast nullable with error_on_missing_keys"_test = [] {
      constexpr auto opts = glz::opts{
         .format = glz::JSON,
         .skip_null_members = true,
         .error_on_missing_keys = true,
      };

      std::string_view data = R"({"b":"hello"})";
      cast_nullable_obj obj{};

      auto ec = glz::read<opts>(obj, data);
      expect(!ec) << glz::format_error(ec, data);
      expect(!obj.a.has_value());
      expect(obj.b == "hello");

      // Test with value present
      data = R"({"a":42.5,"b":"world"})";
      ec = glz::read<opts>(obj, data);
      expect(!ec) << glz::format_error(ec, data);
      expect(obj.a.has_value());
      expect(obj.a.value() == 42);
      expect(obj.b == "world");

      // Test missing required field
      data = R"({"a":null})";
      ec = glz::read<opts>(obj, data);
      expect(ec == glz::error_code::missing_key);
   };
};

struct Command401
{
   int code{};
   int indent{};
   std::vector<std::string> parameters{};
};

struct Command250Params
{
   std::string name{};
   int volume{};
   int pitch{};
   int pan{};
};

struct Command250
{
   int code{};
   int indent{};
   std::vector<Command250Params> parameters{};
};

using CommandVariant = std::variant<Command250, Command401>;

template <>
struct glz::meta<CommandVariant>
{
   static constexpr std::string_view tag = "code";
   static constexpr std::array ids = {250, 401};
};

suite integer_id_variant_tests = [] {
   "command variant"_test = [] {
      std::vector<CommandVariant> v{};

      std::string buffer =
         R"([{"code":401,"indent":0,"parameters":["You light the torch."]},{"code":250,"indent":0,"parameters":[{"name":"fnh_book1","volume":90,"pitch":100,"pan":0}]}])";

      auto ec = glz::read_json(v, buffer);
      expect(not ec) << glz::format_error(ec, buffer);

      std::string out{};
      expect(not glz::write_json(v, out));

      expect(out == buffer) << out;

      expect(not glz::write<glz::opts{.prettify = true}>(v, out));
      expect(out == R"([
   {
      "code": 401,
      "indent": 0,
      "parameters": [
         "You light the torch."
      ]
   },
   {
      "code": 250,
      "indent": 0,
      "parameters": [
         {
            "name": "fnh_book1",
            "volume": 90,
            "pitch": 100,
            "pan": 0
         }
      ]
   }
])") << out;
   };
};

struct versioned_data_t
{
   std::string name{};
   int version{};
   std::string computed_field{};
   std::string input_only_field{};
};

template <>
struct glz::meta<versioned_data_t>
{
   static constexpr bool skip(const std::string_view key, const meta_context& ctx)
   {
      // Skip computed_field during parsing (reading) - it should only be written
      if (key == "computed_field" && ctx.op == glz::operation::parse) {
         return true;
      }

      // Skip input_only_field during serialization (writing) - it should only be read
      if (key == "input_only_field" && ctx.op == glz::operation::serialize) {
         return true;
      }

      return false;
   }
};

suite operation_specific_skipping_tests = [] {
   "operation_specific_skipping"_test = [] {
      versioned_data_t obj{"TestData", 1, "computed_value", "input_value"};
      std::string buffer{};

      // Writing JSON - skips input_only_field
      expect(not glz::write_json(obj, buffer));
      expect(buffer == R"({"name":"TestData","version":1,"computed_field":"computed_value"})") << buffer;

      // Reading JSON - skips computed_field
      const char* json = R"({"name":"NewData","version":2,"computed_field":"ignored","input_only_field":"new_input"})";
      expect(glz::read_json(obj, json) == glz::error_code::none);
      expect(obj.name == "NewData");
      expect(obj.version == 2);
      expect(obj.computed_field == "computed_value"); // Should retain original value
      expect(obj.input_only_field == "new_input");
   };
};

suite glaze_error_category_tests = [] {
   "error_category_name"_test = [] {
      const auto& category = glz::error_category;
      expect(std::string_view(category.name()) == "glaze");
   };

   "make_error_code_function"_test = [] {
      auto ec = glz::make_error_code(glz::error_code::parse_error);

      expect(ec.category() == glz::error_category);
      expect(ec.value() == static_cast<int>(glz::error_code::parse_error));
      expect(ec.message() == "parse_error");
      expect(bool(ec) == true); // non-zero error codes are truthy
   };

   "make_error_code_none"_test = [] {
      auto ec = glz::make_error_code(glz::error_code::none);

      expect(ec.category() == glz::error_category);
      expect(ec.value() == static_cast<int>(glz::error_code::none));
      expect(ec.message() == "none");
      expect(bool(ec) == false); // error_code::none should be falsy
   };

   "error_code_enum_compatibility"_test = [] {
      // Test that glz::error_code can be implicitly converted to std::error_code
      std::error_code ec = glz::error_code::expected_brace;

      expect(ec.category() == glz::error_category);
      expect(ec.value() == static_cast<int>(glz::error_code::expected_brace));
      expect(ec.message() == "expected_brace");
   };

   "error_code_comparison"_test = [] {
      auto ec1 = glz::make_error_code(glz::error_code::syntax_error);
      auto ec2 = glz::make_error_code(glz::error_code::syntax_error);
      auto ec3 = glz::make_error_code(glz::error_code::parse_error);

      expect(ec1 == ec2);
      expect(ec1 != ec3);
      expect(ec1.value() == ec2.value());
      expect(ec1.value() != ec3.value());
   };

   "error_code_direct_comparison"_test = [] {
      std::error_code ec = glz::error_code::unknown_key;

      // Test direct comparison with glz::error_code
      expect(ec == glz::error_code::unknown_key);
      expect(ec != glz::error_code::missing_key);
   };

   "error_code_boolean_conversion"_test = [] {
      std::error_code none_ec = glz::error_code::none;
      std::error_code error_ec = glz::error_code::parse_error;

      expect(!none_ec); // none should be falsy
      expect(bool(error_ec)); // any non-none error should be truthy
   };

   "error_code_message_consistency"_test = [] {
      // Test that the message from error_code and error_category are consistent

      using enum glz::error_code;
      for (auto err : {none, parse_error, expected_brace, expected_bracket, expected_quote, syntax_error, unknown_key,
                       missing_key, constraint_violated}) {
         std::error_code ec = err;
         expect(ec.message() == glz::error_category.message(static_cast<int>(err)));
      }
   };

   "error_ctx_compatibility"_test = [] {
      glz::error_ctx ctx{};
      ctx.ec = glz::error_code::expected_comma;
      ctx.custom_error_message = "custom message";

      // Test that error_ctx works with the error category
      std::error_code ec = ctx.ec;
      expect(ec.category() == glz::error_category);
      expect(ec.message() == "expected_comma");
      expect(bool(ctx) == true); // ctx should be truthy when error is set
      expect(ctx == glz::error_code::expected_comma);
   };
};

suite explicit_string_view_support = [] {
   "write json from explicit string_view"_test = [] {
      struct explicit_string_view_type
      {
         std::string storage{};

         explicit explicit_string_view_type(std::string_view s) : storage(s) {}

         explicit operator std::string_view() const noexcept { return storage; }
      };

      explicit_string_view_type value{std::string_view{"explicit"}};

      std::string buffer{};
      expect(not glz::write_json(value, buffer));
      expect(buffer == R"("explicit")");

      buffer.clear();
      expect(not glz::write<glz::opts{.raw_string = true}>(value, buffer));
      expect(buffer == R"("explicit")");
   };
};

suite member_function_pointer_serialization = [] {
   "member function pointer skipped in json write"_test = [] {
      MemberFunctionThing thing{};
      thing.name = "test_item";
      std::string buffer{};
      expect(not glz::write_json(thing, buffer));
      expect(buffer == R"({"name":"test_item"})") << buffer;
   };

   "member function pointer opt-in write produces legacy output"_test = [] {
      MemberFunctionThing thing{};
      thing.name = "test_item";

      struct opts_with_member_functions : glz::opts
      {
         bool write_member_functions = true;
      };

      std::string buffer{};
      expect(not glz::write<opts_with_member_functions{}>(thing, buffer));
      expect(buffer == R"({"name":"test_item","description":})") << buffer;
   };
};

int main()
{
   trace.end("json_test");
   const auto ec = glz::write_file_json(trace, "json_test.trace.json", std::string{});
   if (ec) {
      std::cerr << "trace output failed\n";
   }

   return 0;
}
