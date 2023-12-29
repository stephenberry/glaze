// Glaze Library
// For the license information refer to glaze.hpp

#include <any>
#include <bitset>
#include <chrono>
#include <complex>
#include <deque>
#include <forward_list>
#include <initializer_list>
#include <iostream>
#include <list>
#include <map>
#include <random>
#include <ranges>
#if defined(__STDCPP_FLOAT128_T__)
#include <stdfloat>
#endif
#include <tuple>
#include <unordered_map>
#include <variant>

#include "boost/ut.hpp"
#include "glaze/api/impl.hpp"
#include "glaze/core/macros.hpp"
#include "glaze/file/hostname_include.hpp"
#include "glaze/json/json_ptr.hpp"
#include "glaze/json/prettify.hpp"
#include "glaze/json/ptr.hpp"
#include "glaze/json/quoted.hpp"
#include "glaze/json/read.hpp"
#include "glaze/json/write.hpp"
#include "glaze/record/recorder.hpp"
#include "glaze/util/progress_bar.hpp"

using namespace boost::ut;

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
      "i", [](auto&& v) { return v.i; }, //
      "d", &T::d, //
      "hello", &T::hello, //
      "arr", &T::arr //
   );
};

suite starter = [] {
   "example"_test = [] {
      my_struct s{};
      std::string buffer{};
      glz::write_json(s, buffer);
      expect(buffer == R"({"i":287,"d":3.14,"hello":"Hello World","arr":[1,2,3]})");
      expect(glz::prettify(buffer) == R"({
   "i": 287,
   "d": 3.14,
   "hello": "Hello World",
   "arr": [
      1,
      2,
      3
   ]
})");
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
   static constexpr auto value = object(
      "a", &sub_thing::a, "Test comment 1"_c, //
      "b", [](auto&& v) -> auto& { return v.b; }, comment("Test comment 2") //
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
   static constexpr auto value = object("a", &T::a, "Test comment 1", //
                                        "b", &T::b, "Test comment 2", //
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

static_assert(glz::detail::get_enum_name(Color::Green) == "Green");

suite get_enum_name_tests = [] {
   "get_enum_name"_test = [] {
      auto color = Color::Green;

      const auto name = glz::detail::get_enum_name(color);
      expect(name == "Green");
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

   Thing() : thing_ptr(&thing){};
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
      "i", [](auto&& v) -> auto& { return v.i; }, glz::schema{.minimum = 2}, //
      "d", &T::d, "double is the best type", //
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
   static constexpr auto value = object(R"(escaped"key)", &T::escaped_key, //
                                        R"(escaped""key2)", &T::escaped_key2, R"(escape_chars)", &T::escape_chars);
};

suite escaping_tests = [] {
   "escaped_key"_test = [] {
      std::string out;
      Escaped obj{};
      glz::write_json(obj, out);

      expect(out == R"({"escaped\"key":0,"escaped\"\"key2":"hi","escape_chars":""})");

      std::string in = R"({"escaped\"key":5,"escaped\"\"key2":"bye"})";
      expect(glz::read_json(obj, in) == glz::error_code::none);
      expect(obj.escaped_key == 5);
      expect(obj.escaped_key2 == "bye");
   };

   "escaped_characters read"_test = [] {
      std::string in = R"({"escape_chars":"\b\f\n\r\t\u11FF"})";
      Escaped obj{};

      expect(glz::read_json(obj, in) == glz::error_code::none);

      expect(obj.escape_chars == "\b\f\n\r\tᇿ") << obj.escape_chars;
   };

   "escaped_char read"_test = [] {
      std::string in = R"("\b")";
      char c;
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

      in = R"("\u11FF")";
      char32_t c32{};
      expect(glz::read_json(c32, in) == glz::error_code::none);
      expect(static_cast<uint32_t>(c32) == 0x11FF);

      in = R"("\u732B")";
      char16_t c16{};
      expect(glz::read_json(c16, in) == glz::error_code::none);
      char16_t uc = u'\u732b';
      expect(c16 == uc);
   };

   "escaped_characters write"_test = [] {
      std::string str = "\"\\\b\f\n\r\tᇿ";
      std::string buffer{};
      glz::write_json(str, buffer);
      expect(buffer == R"("\"\\\b\f\n\r\tᇿ")");
   };

   "escaped_char write"_test = [] {
      std::string out{};
      char c = '\b';
      glz::write_json(c, out);
      expect(out == R"("\b")");

      c = '\f';
      glz::write_json(c, out);
      expect(out == R"("\f")");

      c = '\n';
      glz::write_json(c, out);
      expect(out == R"("\n")");

      c = '\r';
      glz::write_json(c, out);
      expect(out == R"("\r")");

      c = '\t';
      glz::write_json(c, out);
      expect(out == R"("\t")");
   };
};

suite basic_types = [] {
   using namespace boost::ut;

   "double write"_test = [] {
      std::string buffer{};
      glz::write_json(3.14, buffer);
      expect(buffer == "3.14") << buffer;
      buffer.clear();
      glz::write_json(9.81, buffer);
      expect(buffer == "9.81") << buffer;
      buffer.clear();
      glz::write_json(0.0, buffer);
      expect(buffer == "0") << buffer;
      buffer.clear();
      glz::write_json(-0.0, buffer);
      expect(buffer == "-0") << buffer;
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

   "int write"_test = [] {
      std::string buffer{};
      glz::write_json(0, buffer);
      expect(buffer == "0");
      buffer.clear();
      glz::write_json(999, buffer);
      expect(buffer == "999");
      buffer.clear();
      glz::write_json(-6, buffer);
      expect(buffer == "-6");
      buffer.clear();
      glz::write_json(10000, buffer);
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
      expect(num == 33);
      expect(glz::read_json(num, "{}") == glz::error_code::parse_number_failure);
      expect(num == 33);
      expect(glz::read_json(num, "[]") == glz::error_code::parse_number_failure);
      expect(num == 33);
      expect(glz::read_json(num, ".") == glz::error_code::parse_number_failure);
      expect(num == 33);
   };

   "bool write"_test = [] {
      std::string buffer{};
      glz::write_json(true, buffer);
      expect(buffer == "true");
      buffer.clear();
      glz::write_json(false, buffer);
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
      glz::write_json("fish", buffer);
      expect(buffer == "\"fish\"");
      buffer.clear();
      glz::write_json("as\"df\\ghjkl", buffer);
      expect(buffer == "\"as\\\"df\\\\ghjkl\"");

      "empty"_test = [] {
         static constexpr std::string_view expected_empty{"\"\""};
         static constexpr std::string_view expected_nothing{};
         expect(that % glz::write_json(std::string_view{}) == expected_empty);
         expect(that % glz::write_json(std::string{}) == expected_empty);
         expect(that % glz::write_json("") == expected_empty);

         auto write_raw = [](const auto& input) {
            std::string result{};
            glz::write<glz::opts{.raw = true}>(input, result);
            return result;
         };
         expect(that % write_raw(std::string_view{}) == expected_nothing);
         expect(that % write_raw(std::string{}) == expected_nothing);
         expect(that % write_raw("") == expected_nothing);

         auto write_raw_str = [](const auto& input) {
            std::string result{};
            glz::write<glz::opts{.raw_string = true}>(input, result);
            return result;
         };
         expect(that % write_raw_str(std::string_view{}) == expected_empty);
         expect(that % write_raw_str(std::string{}) == expected_empty);
         expect(that % write_raw_str("") == expected_empty);

         auto write_num = [](const auto& input) {
            std::string result{};
            glz::write<glz::opts{.number = true}>(input, result);
            return result;
         };
         expect(that % write_num(std::string_view{}) == expected_nothing);
         expect(that % write_num(std::string{}) == expected_nothing);
         expect(that % write_num("") == expected_nothing);
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

suite container_types = [] {
   using namespace boost::ut;
   "vector int roundtrip"_test = [] {
      std::vector<int> vec(100);
      for (auto& item : vec) item = rand();
      std::string buffer{};
      std::vector<int> vec2{};
      glz::write_json(vec, buffer);
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
      glz::write_json(vec, buffer);
      expect(glz::read_json(vec2, buffer) == glz::error_code::none);
      expect(vec == vec2);
   };
   "vector double roundtrip"_test = [] {
      std::vector<double> vec(100);
      for (auto& item : vec) item = rand() / (1.0 + rand());
      std::string buffer{};
      std::vector<double> vec2{};
      glz::write_json(vec, buffer);
      expect(glz::read_json(vec2, buffer) == glz::error_code::none);
      expect(vec == vec2);
   };
   "vector bool roundtrip"_test = [] {
      std::vector<bool> vec(100);
      for (auto&& item : vec) item = rand() / (1.0 + rand());
      std::string buffer{};
      std::vector<bool> vec2{};
      glz::write_json(vec, buffer);
      expect(glz::read_json(vec2, buffer) == glz::error_code::none);
      expect(vec == vec2);
   };
   "deque roundtrip"_test = [] {
      std::vector<int> deq(100);
      for (auto& item : deq) item = rand();
      std::string buffer{};
      std::vector<int> deq2{};
      glz::write_json(deq, buffer);
      expect(glz::read_json(deq2, buffer) == glz::error_code::none);
      expect(deq == deq2);
   };
   "list roundtrip"_test = [] {
      std::list<int> lis(100);
      for (auto& item : lis) item = rand();
      std::string buffer{};
      std::list<int> lis2{};
      glz::write_json(lis, buffer);
      expect(glz::read_json(lis2, buffer) == glz::error_code::none);
      expect(lis == lis2);
   };
   "forward_list roundtrip"_test = [] {
      std::forward_list<int> lis(100);
      for (auto& item : lis) item = rand();
      std::string buffer{};
      std::forward_list<int> lis2{};
      glz::write_json(lis, buffer);
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
      glz::write_json(map, buffer);
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
      glz::write_json(map, buffer);
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
      glz::write_json(map, buffer);
      expect(glz::read_json(map2, buffer) == glz::error_code::none);
      // expect(map == map2);
      for (auto& it : map) {
         expect(map2[it.first] == it.second);
      }
   };
   "tuple roundtrip"_test = [] {
      auto tuple = std::make_tuple(3, 2.7, std::string("curry"));
      decltype(tuple) tuple2{};
      std::string buffer{};
      glz::write_json(tuple, buffer);
      expect(glz::read_json(tuple2, buffer) == glz::error_code::none);
      expect(tuple == tuple2);
   };
   "pair roundtrip"_test = [] {
      auto pair = std::make_pair(std::string("water"), 5.2);
      decltype(pair) pair2{};
      std::string buffer{};
      glz::write_json(pair, buffer);
      expect(glz::read_json(pair2, buffer) == glz::error_code::none);
      expect(pair == pair2);
   };
};

suite nullable_types = [] {
   using namespace boost::ut;
   "optional"_test = [] {
      std::optional<int> oint{};
      std::string buffer{};
      glz::write_json(oint, buffer);
      expect(buffer == "null");

      expect(glz::read_json(oint, "5") == glz::error_code::none);
      expect(bool(oint) && *oint == 5);
      buffer.clear();
      glz::write_json(oint, buffer);
      expect(buffer == "5");

      expect(glz::read_json(oint, "null") == glz::error_code::none);
      expect(!bool(oint));
      buffer.clear();
      glz::write_json(oint, buffer);
      expect(buffer == "null");
   };
   "shared_ptr"_test = [] {
      std::shared_ptr<int> ptr{};
      std::string buffer{};
      glz::write_json(ptr, buffer);
      expect(buffer == "null");

      expect(glz::read_json(ptr, "5") == glz::error_code::none);
      expect(bool(ptr) && *ptr == 5);
      buffer.clear();
      glz::write_json(ptr, buffer);
      expect(buffer == "5");

      expect(glz::read_json(ptr, "null") == glz::error_code::none);
      expect(!bool(ptr));
      buffer.clear();
      glz::write_json(ptr, buffer);
      expect(buffer == "null");
   };
   "unique_ptr"_test = [] {
      std::unique_ptr<int> ptr{};
      std::string buffer{};
      glz::write_json(ptr, buffer);
      expect(buffer == "null");

      expect(glz::read_json(ptr, "5") == glz::error_code::none);
      expect(bool(ptr) && *ptr == 5);
      buffer.clear();
      glz::write_json(ptr, buffer);
      expect(buffer == "5");

      expect(glz::read_json(ptr, "null") == glz::error_code::none);
      expect(!bool(ptr));
      buffer.clear();
      glz::write_json(ptr, buffer);
      expect(buffer == "null");
   };
};

suite enum_types = [] {
   using namespace boost::ut;
   "enum"_test = [] {
      Color color = Color::Red;
      std::string buffer{};
      glz::write_json(color, buffer);
      expect(buffer == "\"Red\"");

      expect(glz::read_json(color, "\"Green\"") == glz::error_code::none);
      expect(color == Color::Green);
      buffer.clear();
      glz::write_json(color, buffer);
      expect(buffer == "\"Green\"");
   };

   "invalid enum"_test = [] {
      Color color = Color::Red;
      expect(glz::read_json(color, "\"Silver\"") == glz::error_code::unexpected_enum);
      expect(color == Color::Red);
   };
};

suite user_types = [] {
   using namespace boost::ut;

   "user array"_test = [] {
      V3 v3{9.1, 7.2, 1.9};
      std::string buffer{};
      glz::write_json(v3, buffer);
      expect(buffer == "[9.1,7.2,1.9]");

      expect(glz::read_json(v3, "[42.1,99.2,55.3]") == glz::error_code::none);
      expect(v3.x == 42.1 && v3.y == 99.2 && v3.z == 55.3);
   };

   "simple user obect"_test = [] {
      sub_thing obj{.a = 77.2, .b = "not a lizard"};
      std::string buffer{};
      glz::write_json(obj, buffer);
      expect(buffer == "{\"a\":77.2,\"b\":\"not a lizard\"}");

      expect(glz::read_json(obj, "{\"a\":999,\"b\":\"a boat of goldfish\"}") == glz::error_code::none);
      expect(obj.a == 999.0 && obj.b == "a boat of goldfish");

      // Should skip invalid keys
      // glaze::read_json(obj,"{/**/ \"b\":\"fox\", \"c\":7.7/**/, \"d\": {\"a\": \"}\"} //\n   /**/, \"a\":322}");
      expect(glz::read<glz::opts{.error_on_unknown_keys = false}>(obj,
                                                                  R"({/**/ "b":"fox", "c":7.7/**/, "d": {"a": "}"} //
/**/, "a":322})") == glz::error_code::none);

      // glaze::read_json(obj,"{/**/ \"b\":\"fox\", \"c\":7.7/**/, \"d\": {\"a\": \"}\"} //\n   /**/, \"a\":322}");
      auto ec = glz::read_json(obj,
                               R"({/**/ "b":"fox", "c":7.7/**/, "d": {"a": "}"} //
   /**/, "a":322})");
      expect(ec != glz::error_code::none);
      expect(obj.a == 322.0 && obj.b == "fox");
   };

   "complex user obect"_test = [] {
      Thing obj{};
      std::string buffer{};
      glz::write_json(obj, buffer);
      expect(
         buffer ==
         R"({"thing":{"a":3.14,"b":"stuff"},"thing2array":[{"a":3.14,"b":"stuff","c":999.342494903,"d":1E-12,"e":203082348402.1,"f":89.089,"g":12380.00000013,"h":1000000.000001}],"vec3":[3.14,2.7,6.5],"list":[6,7,8,2],"deque":[9,6.7,3.1],"vector":[[9,6.7,3.1],[3.14,2.7,6.5]],"i":8,"d":2,"b":false,"c":"W","v":{"x":0},"color":"Green","vb":[true,false,false,true,true,true,true],"sptr":{"a":3.14,"b":"stuff"},"array":["as\"df\\ghjkl","pie","42","foo"],"map":{"a":4,"b":12,"f":7},"mapi":{"2":9.63,"5":3.14,"7":7.42},"thing_ptr":{"a":3.14,"b":"stuff"}})")
         << buffer;

      buffer.clear();
      glz::write<glz::opts{.skip_null_members = false}>(obj, buffer);
      expect(
         buffer ==
         R"({"thing":{"a":3.14,"b":"stuff"},"thing2array":[{"a":3.14,"b":"stuff","c":999.342494903,"d":1E-12,"e":203082348402.1,"f":89.089,"g":12380.00000013,"h":1000000.000001}],"vec3":[3.14,2.7,6.5],"list":[6,7,8,2],"deque":[9,6.7,3.1],"vector":[[9,6.7,3.1],[3.14,2.7,6.5]],"i":8,"d":2,"b":false,"c":"W","v":{"x":0},"color":"Green","vb":[true,false,false,true,true,true,true],"sptr":{"a":3.14,"b":"stuff"},"optional":null,"array":["as\"df\\ghjkl","pie","42","foo"],"map":{"a":4,"b":12,"f":7},"mapi":{"2":9.63,"5":3.14,"7":7.42},"thing_ptr":{"a":3.14,"b":"stuff"}})")
         << buffer;

      expect(glz::read_json(obj, buffer) == glz::error_code::none);

      buffer.clear();
      glz::write_jsonc(obj, buffer);
      expect(
         buffer ==
         R"({"thing":{"a":3.14/*Test comment 1*/,"b":"stuff"/*Test comment 2*/},"thing2array":[{"a":3.14/*Test comment 1*/,"b":"stuff"/*Test comment 2*/,"c":999.342494903,"d":1E-12,"e":203082348402.1,"f":89.089,"g":12380.00000013,"h":1000000.000001}],"vec3":[3.14,2.7,6.5],"list":[6,7,8,2],"deque":[9,6.7,3.1],"vector":[[9,6.7,3.1],[3.14,2.7,6.5]],"i":8,"d":2/*double is the best type*/,"b":false,"c":"W","v":{"x":0},"color":"Green","vb":[true,false,false,true,true,true,true],"sptr":{"a":3.14/*Test comment 1*/,"b":"stuff"/*Test comment 2*/},"array":["as\"df\\ghjkl","pie","42","foo"],"map":{"a":4,"b":12,"f":7},"mapi":{"2":9.63,"5":3.14,"7":7.42},"thing_ptr":{"a":3.14/*Test comment 1*/,"b":"stuff"/*Test comment 2*/}})")
         << buffer;
      expect(glz::read_json(obj, buffer) == glz::error_code::none);
   };

   "complex user obect prettify"_test = [] {
      Thing obj{};
      std::string buffer{};
      glz::write<glz::opts{.prettify = true}>(obj, buffer);
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
      glz::write<glz::opts{.skip_null_members = false}>(obj, buffer); // Sets sptr to null

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
             "glz::tuplet::tuple<sub_thing,std::array<sub_thing2,1>,V3,std::list<int32_t>,std::deque<double>,std::"
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
   using namespace boost::ut;

   "large_length_range"_test = [] {
      large_length_range_t obj{};
      std::string_view s = R"({"a":55,"another_integer_value":77})";
      expect(!glz::read_json(obj, s));
      expect(obj.a == 55);
      expect(obj.another_integer_value == 77);
   };
};

suite json_pointer = [] {
   using namespace boost::ut;

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
      [[maybe_unused]] constexpr bool is_valid = glz::valid<Thing, "/thing/a", double>(); // Verify constexpr

      expect(glz::valid<Thing, "/thing_ptr/a", double>() == true);
      expect(glz::valid<Thing, "/thing_ptr/a", int>() == false);
      expect(glz::valid<Thing, "/thing_ptr/b">() == true);
      expect(glz::valid<Thing, "/thing_ptr/z">() == false);

      expect(glz::valid<Thing, "/vec3/2", double>() == true);
      expect(glz::valid<Thing, "/vec3/3", double>() == false);

      expect(glz::valid<Thing, "/map/f", int>() == true);
      expect(glz::valid<Thing, "/vector", std::vector<V3>>() == true);
      expect(glz::valid<Thing, "/vector/1", V3>() == true);
      expect(glz::valid<Thing, "/vector/1/0", double>() == true);
   };
};

suite early_end = [] {
   using namespace boost::ut;

   "early_end"_test = [] {
      Thing obj{};
      glz::json_t json{};
      glz::skip skip_me{};
      std::string buffer_data =
         R"({"thing":{"a":3.14/*Test comment 1*/,"b":"stuff"/*Test comment 2*/},"thing2array":[{"a":3.14/*Test comment 1*/,"b":"stuff"/*Test comment 2*/,"c":999.342494903,"d":1e-12,"e":203082348402.1,"f":89.089,"g":12380.00000013,"h":1000000.000001}],"vec3":[3.14,2.7,6.5],"list":[6,7,8,2],"deque":[9,6.7,3.1],"vector":[[9,6.7,3.1],[3.14,2.7,6.5]],"i":8,"d":2/*double is the best type*/,"b":false,"c":"W","vb":[true,false,false,true,true,true,true],"sptr":{"a":3.14/*Test comment 1*/,"b":"stuff"/*Test comment 2*/},"optional":null,"array":["as\"df\\ghjkl","pie","42","foo"],"map":{"a":4,"b":12,"f":7},"mapi":{"2":9.63,"5":3.14,"7":7.42},"thing_ptr":{"a":3.14/*Test comment 1*/,"b":"stuff"/*Test comment 2*/}})";
      std::string_view buffer = buffer_data;
      while (buffer.size() > 0) {
         buffer_data.pop_back();
         buffer = buffer_data;
         // This is mainly to check if all our end checks are in place.
         auto err = glz::read_json(obj, buffer);
         expect(err != glz::error_code::none);
         expect(err.location <= buffer.size());
         err = glz::read_json(json, buffer);
         expect(err != glz::error_code::none);
         expect(err.location <= buffer.size());
         err = glz::read_json(skip_me, buffer);
         expect(err != glz::error_code::none);
         expect(err.location <= buffer.size());
      }
   };
};

suite prettified_custom_object = [] {
   using namespace boost::ut;

   "prettified_custom_object"_test = [] {
      Thing obj{};
      std::string buffer = glz::write_json(obj);
      buffer = glz::prettify(buffer);
      expect(glz::read_json(obj, buffer) == glz::error_code::none);
   };
};

suite bench = [] {
   using namespace boost::ut;
   "bench"_test = [] {
      std::cout << "\nPerformance regresion test: \n";
#ifdef NDEBUG
      size_t repeat = 100000;
#else
      size_t repeat = 1000;
#endif
      Thing thing{};

      std::string buffer;
      glz::write_json(thing, buffer);

      auto tstart = std::chrono::high_resolution_clock::now();
      for (size_t i{}; i < repeat; ++i) {
         buffer.clear();
         glz::write_json(thing, buffer);
      }
      auto tend = std::chrono::high_resolution_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::duration<double>>(tend - tstart).count();
      auto mbytes_per_sec = repeat * buffer.size() / (duration * 1048576);
      std::cout << "write_json size: " << buffer.size() << " bytes\n";
      std::cout << "write_json: " << duration << " s, " << mbytes_per_sec << " MB/s"
                << "\n";

      tstart = std::chrono::high_resolution_clock::now();
      for (size_t i{}; i < repeat; ++i) {
         expect(glz::read_json(thing, buffer) == glz::error_code::none);
      }
      tend = std::chrono::high_resolution_clock::now();
      duration = std::chrono::duration_cast<std::chrono::duration<double>>(tend - tstart).count();
      mbytes_per_sec = repeat * buffer.size() / (duration * 1048576);
      std::cout << "read_json: " << duration << " s, " << mbytes_per_sec << " MB/s"
                << "\n";

      tstart = std::chrono::high_resolution_clock::now();
      for (size_t i{}; i < repeat; ++i) {
         glz::get<std::string>(thing, "/thing_ptr/b");
      }
      tend = std::chrono::high_resolution_clock::now();
      duration = std::chrono::duration_cast<std::chrono::duration<double>>(tend - tstart).count();
      std::cout << "get: " << duration << " s, " << (repeat / duration) << " gets/s"
                << "\n\n";
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
   using namespace boost::ut;

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

   "multiple int from double text"_test = [] {
      std::vector<int> v;
      std::string buffer = "[1.66, 3.24, 5.555]";
      expect(glz::read_json(v, buffer) == glz::error_code::none);
      expect(v.size() == 3);
      expect(v[0] == 1);
      expect(v[1] == 3);
      expect(v[2] == 5);
   };

   "comments"_test = [] {
      {
         std::string b = "1/*a comment*/00";
         int a{};
         expect(glz::read_json(a, b) == glz::error_code::none);
         expect(a == 1);
      }
      {
         std::string b = R"([100, // a comment
20])";
         std::vector<int> a{};
         expect(glz::read_json(a, b) == glz::error_code::none);
         expect(a[0] == 100);
         expect(a[1] == 20);
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

   "Read pair"_test =
      [](const auto& test_case) {
         const std::pair expected{test_case.expected_key, test_case.expected_value};
         std::remove_cv_t<decltype(expected)> parsed{};
         const auto err = glz::read_json(parsed, test_case.input_json);
         expect(err == glz::error_code::none) << glz::format_error(err, test_case.input_json);
         expect(parsed == expected) << glz::write_json(parsed);
      } |
      std::tuple{
         Read_pair_test_case{1, 2, R"({"1":2})"},
         Read_pair_test_case{std::string{"key"}, 2, R"({"key":2})"},
         Read_pair_test_case{std::string{"key"}, std::string{"value"}, R"({"key":"value"})"},
         Read_pair_test_case{std::array{1, 2, 3}, std::array{4, 5, 6}, R"({"[1,2,3]":[4,5,6]})"},
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
         double d;
         expect(glz::read_json(d, res) != glz::error_code::none);
      }
      {
         std::string res = R"(-success)";
         double d;
         expect(glz::read_json(d, res) != glz::error_code::none);
      }
      {
         std::string res = R"(1.a)";
         double d;

         expect(glz::read_json(d, res) == glz::error_code::none);
      }
      {
         std::string res = R"()";
         double d;
         expect(glz::read_json(d, res) != glz::error_code::none);
      }
      {
         std::string res = R"(-)";
         double d;
         expect(glz::read_json(d, res) != glz::error_code::none);
      }
      {
         std::string res = R"(1.)";
         double d;

         expect(glz::read_json(d, res) == glz::error_code::none);
      }
      {
         std::string res = R"(1.0e)";
         double d;

         expect(glz::read_json(d, res) == glz::error_code::none);
      }
      {
         std::string res = R"(1.0e-)";
         double d;

         expect(glz::read_json(d, res) == glz::error_code::none);
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
   using namespace boost::ut;

   "Write floating point types"_test = [] {
      {
         std::string s;
         float f{0.96875f};
         glz::write_json(f, s);
         expect(s == "0.96875") << s;
      }
      {
         std::string s;
         double f{0.96875};
         glz::write_json(f, s);
         expect(s == "0.96875") << s;
      }
   };

   "Write integral types"_test = [] {
      {
         std::string s;
         bool v{1};
         glz::write_json(v, s);
         expect(s == "true");
      }
      //* Two test below fail, unless changed to the following:
      {
         std::string s;
         char v{'a'};
         glz::write_json(v, s);
         // Is this what we want instead?
         expect(s == R"("a")"); // std::to_string(static_cast<int>('a')));
      }
      {
         std::string s;
         wchar_t v{'a'};
         glz::write_json(v, s); // This line gives warning about converting wchar to char, is that fine? Should we
                                // write a to_buffer template to handle type wchar?
         // Is the below what we actually expect?
         expect(s == R"("a")"); // std::to_string(static_cast<int>('a')));
      }
      {
         std::string s;
         short v{1};
         glz::write_json(v, s);
         expect(s == "1");
      }
      {
         std::string s;
         int v{1};
         glz::write_json(v, s);
         expect(s == "1");
      }
      {
         std::string s;
         long v{1};
         glz::write_json(v, s);
         expect(s == "1");
      }
      {
         std::string s;
         long long v{-193582804324766};
         glz::write_json(v, s);
         expect(s == "-193582804324766");
      }
      {
         std::string s;
         unsigned short v{1};
         glz::write_json(v, s);
         expect(s == "1");
      }
      {
         std::string s;
         unsigned int v{1};
         glz::write_json(v, s);
         expect(s == "1");
      }
      {
         std::string s;
         unsigned long v{1};
         glz::write_json(v, s);
         expect(s == "1");
      }
      {
         std::string s;
         unsigned long long v{193582804324766};
         glz::write_json(v, s);
         expect(s == "193582804324766");
      }
   };

   "Write variant"_test = [] {
      std::variant<int, double, Geodetic> var;

      var = 1;
      std::string ibuf{};
      glz::write_json(var, ibuf);
      expect(ibuf == R"(1)");

      var = 2.2;
      std::string dbuf{};
      glz::write_json(var, dbuf);
      expect(dbuf == R"(2.2)");

      var = Geodetic{1.0, 2.0, 5.0};
      std::string gbuf{};
      glz::write_json(var, gbuf);
      expect(gbuf == R"([1,2,5])") << gbuf;
   };

   "Write empty array structure"_test = [] {
      EmptyArray e;
      std::string buf;
      glz::write_json(e, buf);
      expect(buf == R"([])");
   };

   "Read empty array structure"_test = [] {
      EmptyArray e;
      expect(glz::read_json(e, "[]") == glz::error_code::none);
      expect(glz::read_json(e, " [   ] ") == glz::error_code::none);
      expect(glz::read_json(e, "[1,2,3]") == glz::error_code::syntax_error);
   };

   //* Empty object not allowed
   "Write empty object structure"_test = [] {
      EmptyObject e;
      std::string buf;
      glz::write_json(e, buf);
      expect(buf == R"({})");
   };

   "Read empty object structure"_test = [] {
      EmptyObject e;
      static_assert(glz::detail::glaze_object_t<EmptyObject>);
      expect(glz::read_json(e, "{}") == glz::error_code::none);
      expect(glz::read_json(e, " {    } ") == glz::error_code::none);
      expect(glz::read_json(e, "{ \"reject\": 44 }") == glz::error_code::unknown_key);
      expect(glz::read<glz::opts{.error_on_unknown_keys = false}>(e, "{ \"skipped\": 44 }") == glz::error_code::none);
   };

   "Write c-string"_test = [] {
      const char* const c = "aasdf";
      std::string buf;
      glz::write_json(c, buf);
      expect(buf == R"("aasdf")");
   };

   "Write constant double"_test = [] {
      const double d = 6.125;
      std::string buf;
      glz::write_json(d, buf);
      expect(buf == R"(6.125)");
   };

   "Write constant bool"_test = [] {
      const bool b = true;
      std::string buf;
      glz::write_json(b, buf);
      expect(buf == R"(true)");
   };

   "Write constant int"_test = [] {
      const int i = 505;
      std::string buf;
      glz::write_json(i, buf);
      expect(buf == R"(505)");
   };

   "Write vector"_test = [] {
      {
         std::vector<double> v{1.1, 2.2, 3.3, 4.4};
         std::string s;
         glz::write_json(v, s);

         expect(s == "[1.1,2.2,3.3,4.4]");
      }
      {
         std::vector<bool> v{true, false, true, false};
         std::string s;
         glz::write_json(v, s);

         expect(s == "[true,false,true,false]");
      }
   };

   "Write list"_test = [] {
      std::string in, inr = "[1,2,3,4]";
      std::list<int> l{1, 2, 3, 4};
      glz::write_json(l, in);

      expect(in == inr);
   };

   "Write forward list"_test = [] {
      std::string in, inr = "[1,2,3,4]";
      std::forward_list<int> l{1, 2, 3, 4};
      glz::write_json(l, in);

      expect(in == inr);
   };

   "Write deque"_test = [] {
      std::string in, inr = "[1,2,3,4]";
      std::deque<int> l{1, 2, 3, 4};
      glz::write_json(l, in);

      expect(in == inr);
   };

   "Write array"_test = [] {
      std::string s;
      std::array<double, 4> v{1.1, 2.2, 3.3, 4.4};
      glz::write_json(v, s);
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
      glz::write_json(m, s);
      expect(s == R"({"a":2.2,"b":11.111,"c":211.2})");

      const std::map<std::string, std::optional<double>> nullable{
         {"a", std::nullopt}, {"b", 13.4}, {"c", std::nullopt}, {"d", 211.2}, {"e", std::nullopt}};
      glz::write_json(nullable, s);
      expect(s == glz::sv{R"({"b":13.4,"d":211.2})"});
   };

   "Write pair"_test =
      [](const auto& test_case) {
         const std::pair value{test_case.key, test_case.value};
         expect(glz::write_json(value) == test_case.expected_json);
      } |
      std::tuple{
         Write_pair_test_case{"key", "value", R"({"key":"value"})"},
         Write_pair_test_case{0, "value", R"({"0":"value"})"},
         Write_pair_test_case{0.78, std::array{1, 2, 3}, R"({"0.78":[1,2,3]})"},
         Write_pair_test_case{"k", glz::obj{"in1", 1, "in2", "v"}, R"({"k":{"in1":1,"in2":"v"}})"},
         Write_pair_test_case{std::array{1, 2}, 99, R"({"[1,2]":99})"},
         Write_pair_test_case{std::array{"one", "two"}, 99, R"({"[\"one\",\"two\"]":99})"},
         Write_pair_test_case{"knot", std::nullopt, "{}"}, // nullopt_t, not std::optional
         Write_pair_test_case{"kmaybe", std::optional<int>{}, "{}"},
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
      glz::write_json(m, s);

      expect(s == R"({"3":2.2,"5":211.2,"7":11.111})");
   };

   "Write object"_test = [] {
      ThreeODetic t{};

      std::string s;
      s.reserve(1000);
      glz::write_json(t, s);
      expect(s == R"(["geo",[0,0,0],"int",0])") << s;

      Named n{"Hello, world!", {{{21, 15, 13}, 0}, {0}}};
      glz::write_json(n, s);

      expect(s == R"({"name":"Hello, world!","value":[["geo",[21,15,13],"int",0],[0,0,0]]})") << s;
   };

   "Write boolean"_test = [] {
      {
         bool b = true;
         std::string s;
         glz::write_json(b, s);

         expect(s == R"(true)");
      }
      {
         bool b = false;
         std::string s;
         glz::write_json(b, s);

         expect(s == R"(false)");
      }
   };

   "Hello World"_test = [] {
      std::unordered_map<std::string, std::string> m;
      m["Hello"] = "World";
      std::string buf;
      glz::write_json(m, buf);

      expect(buf == R"({"Hello":"World"})");
   };

   "Number"_test = [] {
      std::unordered_map<std::string, double> x;
      x["number"] = 5.55;

      std::string jx;
      glz::write_json(x, jx);

      expect(jx == R"({"number":5.55})");
   };

   "Nested array"_test = [] {
      std::vector<Geodetic> v(2);
      std::string buf;

      glz::write_json(v, buf);
      expect(buf == R"([[0,0,0],[0,0,0]])");
   };

   "Nested map"_test = [] {
      std::map<std::string, Geodetic> m;
      m["1"];
      m["2"];
      std::string buf;

      glz::write_json(m, buf);
      expect(buf == R"({"1":[0,0,0],"2":[0,0,0]})");
   };

   "Nested map 2"_test = [] {
      std::map<std::string, std::vector<double>> m;
      m["1"] = {4.0, 0.0, 0.0};
      m["2"] = {5.0, 0.0, 0.0, 4.0};
      std::string buf;

      glz::write_json(m, buf);
      expect(buf == R"({"1":[4,0,0],"2":[5,0,0,4]})");
   };
};

struct error_comma_obj
{
   struct error_comma_data
   {
      std::string instId;
      GLZ_LOCAL_META(error_comma_data, instId);
   };

   std::string code;
   std::string msg;
   std::vector<error_comma_data> data;
   GLZ_LOCAL_META(error_comma_obj, data, code, msg);
};

suite error_outputs = [] {
   "invalid character"_test = [] {
      std::string s = R"({"Hello":"World"x, "color": "red"})";
      std::map<std::string, std::string> m;
      auto pe = glz::read_json(m, s);
      expect(pe != glz::error_code::none);
      auto err = glz::format_error(pe, s);
      expect(err == "1:17: syntax_error\n   {\"Hello\":\"World\"x, \"color\": \"red\"}\n                   ^\n") << err;
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
      expect(err == "10:6: syntax_error\n        ]\n        ^\n") << err;
   };
};

#include "glaze/json/study.hpp"

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

      auto f = [&](auto /*thread_number*/) { ++x; };

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

suite progress_bar_tests = [] {
   "progress bar 30%"_test = [] {
      glz::progress_bar bar{.width = 12, .completed = 3, .total = 10, .time_taken = 30.0};
      expect(bar.string() == "[===-------] 30% | ETA: 1m 10s | 3/10") << bar.string();
   };

   "progress bar 100%"_test = [] {
      glz::progress_bar bar{.width = 12, .completed = 10, .total = 10, .time_taken = 30.0};
      expect(bar.string() == "[==========] 100% | ETA: 0m 0s | 10/10") << bar.string();
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
      static constexpr auto value = glz::object("x", &T::x, "A comment for x", //
                                                "y", &T::y, "A comment for y");
   };
};

static_assert(glz::detail::glaze_t<local_meta>);
static_assert(glz::detail::glaze_object_t<local_meta>);
static_assert(glz::detail::local_meta_t<local_meta>);

suite local_meta_tests = [] {
   "local_meta"_test = [] {
      std::string out;
      local_meta m{};
      glz::write_json(m, out);
      expect(out == R"({"x":0,"y":0})");
      expect(glz::named<local_meta> == true);
      expect(glz::name_v<local_meta> == "local_meta");
   };
};

suite raw_json_tests = [] {
   "round_trip_raw_json"_test = [] {
      std::vector<glz::raw_json> v{"0", "1", "2"};
      std::string s;
      glz::write_json(v, s);
      expect(s == R"([0,1,2])");
      expect(glz::read_json(v, s) == glz::error_code::none);
   };
   "raw_json_view_read"_test = [] {
      std::vector<glz::raw_json_view> v{};
      std::string s = R"([0,1,2])";
      expect(glz::read_json(v, s) == glz::error_code::none);
      expect(v[0].str == "0");
      expect(v[1].str == "1");
      expect(v[2].str == "2");
   };
};

suite json_helpers = [] {
   "json_helpers"_test = [] {
      my_struct v{};
      auto json = glz::write_json(v);
      expect(json == R"({"i":287,"d":3.14,"hello":"Hello World","arr":[1,2,3]})");

      v = glz::read_json<my_struct>(json).value();
   };
};

suite allocated_write = [] {
   "allocated_write"_test = [] {
      my_struct v{};
      std::string s{};
      s.resize(100);
      auto length = glz::write_json(v, s.data());
      s.resize(length);
      expect(s == R"({"i":287,"d":3.14,"hello":"Hello World","arr":[1,2,3]})");
   };
};

suite nan_tests = [] {
   "nan_write_tests"_test = [] {
      double d = NAN;
      std::string s{};
      glz::write_json(d, s);
      expect(s == "null");

      d = 0.0;
      expect(glz::read_json(d, s) == glz::error_code::none);
      expect(std::isnan(d));
   };

   "nan_read_tests"_test = [] {
      double d = 0.0;
      std::string s = "null";
      expect(glz::read_json(d, s) == glz::error_code::none);
      expect(std::isnan(d));

      d = 0.0;
      s = "NaN";
      expect(glz::read_json(d, s) == glz::error_code::none);
      expect(std::isnan(d));

      d = 0.0;
      s = "nan";
      expect(glz::read_json(d, s) == glz::error_code::none);
      expect(std::isnan(d));

      std::array<double, 5> d_array{};
      s = "[null, nan, NaN, -nan, 3.14]";
      expect(glz::read_json(d_array, s) == glz::error_code::none);
      expect(std::isnan(d_array[0]));
      expect(std::isnan(d_array[1]));
      expect(std::isnan(d_array[2]));
      expect(std::isnan(d_array[3]));
      expect(d_array[4] == 3.14);
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
   static constexpr auto value = object("num", glz::detail::array_variant{&T::num});
};

suite tagged_variant_tests = [] {
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
      glz::write_json(var, s);
      expect(s == R"({"action":"DELETE","data":"the_internet"})");
      s.clear();

      // Automatic tagged discriminator ids
      tagged_variant2 var2 = put_action{{{"x", 100}, {"y", 200}}};
      glz::write_json(var2, s);
      expect(s == R"({"type":"put_action","data":{"x":100,"y":200}})");
      s.clear();

      // prettifies valid JSON
      glz::write<glz::opts{.prettify = true}>(var, s);
      tagged_variant parsed_var;
      expect(glz::read_json(parsed_var, s) == glz::error_code::none);
      expect(parsed_var == var);
   };

   "array_variant_tests"_test = [] {
      // Test array based variant (experimental, not meant for external usage since api might change)

      holds_some_num obj{};
      auto ec = glz::read_json(obj, R"({"num":["float", 3.14]})");
      std::string b = R"({"num":["float", 3.14]})";
      expect(ec == glz::error_code::none) << glz::format_error(ec, b);
      expect(std::get<float>(obj.num) == 3.14f);
      expect(glz::read_json(obj, R"({"num":["uint64_t", 5]})") == glz::error_code::none);
      expect(std::get<uint64_t>(obj.num) == 5);
      expect(glz::read_json(obj, R"({"num":["int8_t", -3]})") == glz::error_code::none);
      expect(std::get<int8_t>(obj.num) == -3);
      expect(glz::read_json(obj, R"({"num":["int32_t", -2]})") == glz::error_code::none);
      expect(std::get<int32_t>(obj.num) == -2);

      obj.num = 5.0;
      std::string s{};
      glz::write_json(obj, s);
      expect(s == R"({"num":["double",5]})");
      obj.num = uint64_t{3};
      glz::write_json(obj, s);
      expect(s == R"({"num":["uint64_t",3]})");
      obj.num = int8_t{-5};
      glz::write_json(obj, s);
      expect(s == R"({"num":["int8_t",-5]})");
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

suite variant_tests = [] {
   "variant_write_tests"_test = [] {
      std::variant<double, std::string> d = "not_a_fish";
      std::string s{};

      glz::write_json(d, s);
      expect(s == R"("not_a_fish")");

      d = 5.7;
      s.clear();
      glz::write_json(d, s);
      expect(s == "5.7");

      std::variant<std::monostate, int, std::string> m{};
      glz::write_json(m, s);
      expect(s == "null") << s;
   };

   "variant_read_"_test = [] {
      std::variant<int32_t, double> x = 44;
      expect(glz::read_json(x, "33") == glz::error_code::none);
      expect(std::get<int32_t>(x) == 33);
   };

   "variant_read_auto"_test = [] {
      // Auto deduce variant with no conflicting basic types
      std::variant<std::monostate, int, std::string, bool, std::map<std::string, double>, std::vector<std::string>> m{};
      expect(glz::read_json(m, R"("Hello World")") == glz::error_code::none);
      expect(std::holds_alternative<std::string>(m) >> fatal);
      expect(std::get<std::string>(m) == "Hello World");

      expect(glz::read_json(m, R"(872)") == glz::error_code::none);
      expect(std::holds_alternative<int>(m) >> fatal);
      expect(std::get<int>(m) == 872);

      expect(glz::read_json(m, R"({"pi":3.14})") == glz::error_code::none);
      expect(std::holds_alternative<std::map<std::string, double>>(m) >> fatal);
      expect(std::get<std::map<std::string, double>>(m)["pi"] == 3.14);

      expect(glz::read_json(m, R"(true)") == glz::error_code::none);
      expect(std::holds_alternative<bool>(m) >> fatal);
      expect(std::get<bool>(m) == true);

      expect(glz::read_json(m, R"(["a", "b", "c"])") == glz::error_code::none);
      expect(std::holds_alternative<std::vector<std::string>>(m) >> fatal);
      expect(std::get<std::vector<std::string>>(m)[1] == "b");

      expect(glz::read_json(m, "null") == glz::error_code::none);
      expect(std::holds_alternative<std::monostate>(m) >> fatal);
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

      auto str = glz::write_json(request);

      expect(str == R"({"password":"123456","remember":true,"username":"paulo"})") << str;
   };

   "variant write/read enum"_test = [] {
      std::variant<Color, std::uint16_t> var{Color::Red};
      auto res{glz::write_json(var)};
      expect(res == "\"Red\"") << res;
      auto read{glz::read_json<std::variant<Color, std::uint16_t>>(res)};
      expect(read.has_value());
      expect(std::holds_alternative<Color>(read.value()));
      expect(std::get<Color>(read.value()) == Color::Red);
   };
};

suite generic_json_tests = [] {
   "generic_json_write"_test = [] {
      glz::json_t json = {{"pi", 3.141},
                          {"happy", true},
                          {"name", "Niels"},
                          {"nothing", nullptr},
                          {"answer", {{"everything", 42.0}}},
                          {"list", {1.0, 0.0, 2.0}},
                          {"object", {{"currency", "USD"}, {"value", 42.99}}}};
      std::string buffer{};
      glz::write_json(json, buffer);
      expect(
         buffer ==
         R"({"answer":{"everything":42},"happy":true,"list":[1,0,2],"name":"Niels","nothing":null,"object":{"currency":"USD","value":42.99},"pi":3.141})")
         << buffer;
   };

   "generic_json_read"_test = [] {
      glz::json_t json{};
      std::string buffer = R"([5,"Hello World",{"pi":3.14},null])";
      expect(glz::read_json(json, buffer) == glz::error_code::none);
      expect(json[0].get<double>() == 5.0);
      expect(json[1].get<std::string>() == "Hello World");
      expect(json[2]["pi"].get<double>() == 3.14);
      expect(json[3].holds<glz::json_t::null_t>());
   };

   "generic_json_roundtrip"_test = [] {
      glz::json_t json{};
      std::string buffer = R"([5,"Hello World",{"pi":3.14},null])";
      expect(glz::read_json(json, buffer) == glz::error_code::none);
      expect(glz::write_json(json) == buffer);
   };

   "generic_json_const"_test = [] {
      auto foo = [](const glz::json_t& json) { return json["s"].get<std::string>(); };
      glz::json_t json = {{"s", "hello world"}};
      expect(foo(json) == "hello world");
   };

   "generic_json_int"_test = [] {
      glz::json_t json = {{"i", 1}};
      expect(json["i"].get<double>() == 1);
   };

   "generic_json_as"_test = [] {
      glz::json_t json = {{"pi", 3.141},
                          {"happy", true},
                          {"name", "Niels"},
                          {"nothing", nullptr},
                          {"answer", {{"everything", 42.0}}},
                          {"list", {1.0, 0.0, 2.0}},
                          {"object", {{"currency", "USD"}, {"value", 42.99}}}};
      expect(json["list"][2].as<int>() == 2);
      expect(json["pi"].as<double>() == 3.141);
      expect(json["name"].as<std::string_view>() == "Niels");
   };

   "generic_json_nested_initialization"_test = [] {
      static const glz::json_t messageSchema = {{"type", "struct"},
                                                {"fields",
                                                 {
                                                    {{"field", "branch"}, {"type", "string"}},
                                                 }}};
      std::string buffer{};
      glz::write_json(messageSchema, buffer);
      expect(buffer == R"({"fields":[{"field":"branch","type":"string"}],"type":"struct"})") << buffer;
   };

   "json_t_contains"_test = [] {
      auto json = glz::read_json<glz::json_t>(R"({"foo":"bar"})");
      expect(!json->contains("id"));
      expect(json->contains("foo"));
   };

   "buffer underrun"_test = [] {
      std::string buffer{"000000000000000000000"};
      glz::json_t json{};
      expect(glz::read_json(json, buffer) == glz::error_code::parse_number_failure);
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

struct macro_t
{
   double x = 5.0;
   std::string y = "yay!";
   int z = 55;
};

GLZ_META(macro_t, x, y, z);

struct local_macro_t
{
   double x = 5.0;
   std::string y = "yay!";
   int z = 55;

   GLZ_LOCAL_META(local_macro_t, x, y, z);
};

suite macro_tests = [] {
   "macro test"_test = [] {
      macro_t obj{};
      std::string b{};
      glz::write_json(obj, b);

      expect(b == R"({"x":5,"y":"yay!","z":55})");
   };

   "local macro test"_test = [] {
      local_macro_t obj{};
      std::string b{};
      glz::write_json(obj, b);

      expect(b == R"({"x":5,"y":"yay!","z":55})");
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
      expect(glz::read_file(s, filename, buffer) == glz::error_code::none);
   };

   "read_file invalid"_test = [] {
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
      expect(glz::read_file(s, "../nonexsistant_file.json", std::string{}) != glz::error_code::none);
   };
};

struct includer_struct
{
   std::string str = "Hello";
   int i = 55;
};

template <>
struct glz::meta<includer_struct>
{
   static constexpr std::string_view name = "includer_struct";
   using T = includer_struct;
   static constexpr auto value = object("#include", glz::file_include{}, "str", &T::str, "i", &T::i);
};

suite file_include_test = [] {
   includer_struct obj{};

   expect(glz::write_file_json(obj, "../alabastar.json", std::string{}) == glz::error_code::none);

   obj.str = "";

   std::string s = R"({"#include": "../alabastar.json", "i": 100})";
   expect(glz::read_json(obj, s) == glz::error_code::none);

   expect(obj.str == "Hello") << obj.str;
   expect(obj.i == 100) << obj.i;

   obj.str = "";

   std::string buffer{};
   glz::read_file_json(obj, "../alabastar.json", buffer);
   expect(obj.str == "Hello") << obj.str;
   expect(obj.i == 55) << obj.i;
};

suite file_include_test_auto = [] {
   includer_struct obj{};

   expect(glz::write_file(obj, "./auto.json", std::string{}) == false);

   obj.str = "";

   std::string s = R"({"#include": "./auto.json", "i": 100})";
   expect(glz::read_json(obj, s) == glz::error_code::none);

   expect(obj.str == "Hello") << obj.str;
   expect(obj.i == 100) << obj.i;

   obj.str = "";

   glz::read_file(obj, "./auto.json", std::string{});
   expect(obj.str == "Hello") << obj.str;
   expect(obj.i == 55) << obj.i;
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
   static constexpr auto value = object("#include", glz::file_include{}, "a", &T::a, "b", &T::b);
};

suite nested_file_include_test = [] {
   nested0 obj;

   std::string a = R"({"#include": "../b/b.json"})";
   {
      std::filesystem::create_directory("a");
      std::ofstream a_file{"./a/a.json"};

      a_file << a;
   }

   {
      std::filesystem::create_directory("b");

      obj.b.i = 13;

      expect(glz::write_file_json(obj.b, "./b/b.json", std::string{}) == glz::error_code::none);
   }

   obj.b.i = 0;

   std::string s = R"({ "a": { "#include": "./a/a.json" }, "b": { "#include": "./b/b.json" } })";

   expect(glz::read_json(obj, s) == glz::error_code::none);

   expect(obj.a.i == 13);
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
      glz::write_json(rec, s);

      expect(glz::read_json(rec, s) == glz::error_code::none);

      expect(glz::write_file_json(rec, "recorder_out.json", std::string{}) == glz::error_code::none);
   };
};

suite reference_wrapper_test = [] {
   "reference_wrapper"_test = [] {
      int x = 55;
      std::reference_wrapper<int> ref = x;

      std::string s = glz::write_json(ref);

      expect(s == "55");

      expect(glz::read_json(ref, R"(66)") == glz::error_code::none);
      expect(x == 66);
   };
};

suite small_chars = [] {
   "small_chars"_test = [] {
      uint8_t x = 5;
      std::string s = glz::write_json(x);

      expect(s == "5");

      expect(glz::read_json(x, "10") == glz::error_code::none);
      expect(x == 10);
   };
};

suite char16_test = [] {
   "char16_test"_test = [] {
      {
         char16_t c{};
         expect(glz::read_json(c, R"("H")") == glz::error_code::none);

         expect(c == u'H');
      }

      {
         // TODO: Support non-ascii
         /*char16_t c{};
         glz::read_json(c, R"("∆")");

         expect(c == u'∆');*/
      }

      /*std::basic_string<char16_t> x;
      glz::read_json(x, "Hello World");

      expect(x == u"Hello World");*/
   };
};

suite ndjson_test = [] {
   "ndjson"_test = [] {
      std::vector<std::string> x = {"Hello", "World", "Ice", "Cream"};
      std::string s = glz::write_ndjson(x);

      expect(s ==
             R"("Hello"
"World"
"Ice"
"Cream")");

      x.clear();

      expect(glz::read_ndjson(x, s) == glz::error_code::none);
      expect(x[0] == "Hello");
      expect(x[1] == "World");
      expect(x[2] == "Ice");
      expect(x[3] == "Cream");
   };

   "ndjson_list"_test = [] {
      std::list<std::string> x = {"Hello", "World", "Ice", "Cream"};
      std::string s = glz::write_ndjson(x);

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

      std::string s = glz::write_ndjson(x);

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
};

suite std_function_handling = [] {
   "std_function"_test = [] {
      int x = 1;
      std::function<void()> increment = [&] { ++x; };
      std::string s{};
      glz::write_json(increment, s);

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
      glz::write_json(s, b);

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
   void eat(const std::string&){};
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
   using T = string_t;
   static constexpr auto value = object("string", &T::string);
};

suite poly_tests = [] {
   "poly"_test = [] {
      std::array<glz::poly<animal>, 2> a{dog{}, cat{}};

      a[0].call<"eat">();
      a[1].call<"eat">();

      expect(a[0].get<"age">() == 1);
   };

   "poly person"_test = [] {
      // std::array<glz::poly<animal>, 2> a{ dog{}, person{} };
      //  This should static_assert
   };

   "poly pointer"_test = [] {
      dog d{};
      glz::poly<animal> a{&d};

      a.call<"eat">();

      expect(d.age == 1);
      expect(&a.get<"age">() == &d.age);
   };

   "complex_function"_test = [] {
      glz::poly<string_t> p{complex_function_call_t{}};

      expect(p.call<"string">("x", 5) == "x:5");
   };
};

suite any_tests = [] {
   "any"_test = [] {
      glz::any a = 5.5;

      expect(glz::any_cast<double>(a) == 5.5);

      auto* data = a.data();
      *static_cast<double*>(data) = 6.6;

      expect(glz::any_cast<double>(a) == 6.6);
   };
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

      glz::write_json(obj, buffer);

      expect(
         buffer ==
         R"({"fixed_object":{"int_array":[0,1,2,3,4,5,6],"float_array":[0.1,0.2,0.3,0.4,0.5,0.6],"double_array":[3288398.238,2.33E24,28.9,0.928759872,0.22222848,0.1,0.2,0.3,0.4]},"fixed_name_object":{"name0":"James","name1":"Abraham","name2":"Susan","name3":"Frank","name4":"Alicia"},"another_object":{"string":"here is some text","another_string":"Hello World","boolean":false,"nested_object":{"v3s":[[0.12345,0.23456,0.001345],[0.3894675,97.39827,297.92387],[18.18,87.289,2988.298]],"id":"298728949872"}},"string_array":["Cat","Dog","Elephant","Tiger"],"string":"Hello world","number":3.14,"boolean":true,"another_bool":false})")
         << buffer;
   };
};

suite json_schema = [] {
   "json schema"_test = [] {
      Thing obj{};
      std::string schema = glz::write_json_schema<Thing>();
      // Note: Check schema and sample output against a json schema validator like https://www.jsonschemavalidator.net/
      // when you update this string
      expect(
         schema ==
         R"({"type":["object"],"properties":{"array":{"$ref":"#/$defs/std::array<std::string,4>"},"b":{"$ref":"#/$defs/bool"},"c":{"$ref":"#/$defs/char"},"color":{"$ref":"#/$defs/Color"},"d":{"$ref":"#/$defs/double","description":"double is the best type"},"deque":{"$ref":"#/$defs/std::deque<double>"},"i":{"$ref":"#/$defs/int32_t","minimum":2},"list":{"$ref":"#/$defs/std::list<int32_t>"},"map":{"$ref":"#/$defs/std::map<std::string,int32_t>"},"mapi":{"$ref":"#/$defs/std::map<int32_t,double>"},"optional":{"$ref":"#/$defs/std::optional<V3>"},"sptr":{"$ref":"#/$defs/std::shared_ptr<sub_thing>"},"thing":{"$ref":"#/$defs/sub_thing"},"thing2array":{"$ref":"#/$defs/std::array<sub_thing2,1>"},"thing_ptr":{"$ref":"#/$defs/sub_thing*"},"v":{"$ref":"#/$defs/std::variant<var1_t,var2_t>"},"vb":{"$ref":"#/$defs/std::vector<bool>"},"vec3":{"$ref":"#/$defs/V3"},"vector":{"$ref":"#/$defs/std::vector<V3>"}},"additionalProperties":false,"$defs":{"Color":{"type":["string"],"oneOf":[{"const":"Red"},{"const":"Green"},{"const":"Blue"}]},"V3":{"type":["array"]},"bool":{"type":["boolean"]},"char":{"type":["string"]},"double":{"type":["number"]},"float":{"type":["number"]},"int32_t":{"type":["integer"]},"std::array<std::string,4>":{"type":["array"],"items":{"$ref":"#/$defs/std::string"}},"std::array<sub_thing2,1>":{"type":["array"],"items":{"$ref":"#/$defs/sub_thing2"}},"std::deque<double>":{"type":["array"],"items":{"$ref":"#/$defs/double"}},"std::list<int32_t>":{"type":["array"],"items":{"$ref":"#/$defs/int32_t"}},"std::map<int32_t,double>":{"type":["object"],"additionalProperties":{"$ref":"#/$defs/double"}},"std::map<std::string,int32_t>":{"type":["object"],"additionalProperties":{"$ref":"#/$defs/int32_t"}},"std::optional<V3>":{"type":["array","null"]},"std::shared_ptr<sub_thing>":{"type":["object","null"],"properties":{"a":{"$ref":"#/$defs/double","description":"Test comment 1"},"b":{"$ref":"#/$defs/std::string","description":"Test comment 2"}},"additionalProperties":false},"std::string":{"type":["string"]},"std::variant<var1_t,var2_t>":{"type":["number","string","boolean","object","array","null"],"oneOf":[{"type":["object"],"properties":{"x":{"$ref":"#/$defs/double"}},"additionalProperties":false},{"type":["object"],"properties":{"y":{"$ref":"#/$defs/double"}},"additionalProperties":false}]},"std::vector<V3>":{"type":["array"],"items":{"$ref":"#/$defs/V3"}},"std::vector<bool>":{"type":["array"],"items":{"$ref":"#/$defs/bool"}},"sub_thing":{"type":["object"],"properties":{"a":{"$ref":"#/$defs/double","description":"Test comment 1"},"b":{"$ref":"#/$defs/std::string","description":"Test comment 2"}},"additionalProperties":false},"sub_thing*":{"type":["object","null"],"properties":{"a":{"$ref":"#/$defs/double","description":"Test comment 1"},"b":{"$ref":"#/$defs/std::string","description":"Test comment 2"}},"additionalProperties":false},"sub_thing2":{"type":["object"],"properties":{"a":{"$ref":"#/$defs/double","description":"Test comment 1"},"b":{"$ref":"#/$defs/std::string","description":"Test comment 2"},"c":{"$ref":"#/$defs/double"},"d":{"$ref":"#/$defs/double"},"e":{"$ref":"#/$defs/double"},"f":{"$ref":"#/$defs/float"},"g":{"$ref":"#/$defs/double"},"h":{"$ref":"#/$defs/double"}},"additionalProperties":false}},"required":["thing","i"],"examples":[{"thing":{},"i":42}]})")
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

namespace glz::detail
{
   template <>
   struct from_json<date>
   {
      template <auto Opts>
      static void op(date& value, auto&&... args)
      {
         read<json>::op<Opts>(value.human_readable, args...);
         value.data = std::stoi(value.human_readable);
      }
   };

   template <>
   struct to_json<date>
   {
      template <auto Opts>
      static void op(date& value, auto&&... args) noexcept
      {
         value.human_readable = std::to_string(value.data);
         write<json>::op<Opts>(value.human_readable, args...);
      }
   };
}

suite date_test = [] {
   "date"_test = [] {
      date d{};
      d.data = 55;

      std::string s{};
      glz::write_json(d, s);

      expect(s == R"("55")");

      d.data = 0;
      expect(glz::read_json(d, s) == glz::error_code::none);
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

suite unicode_tests = [] {
   "unicode"_test = [] {
      std::string str = "😀😃😄🍌💐🌹🥀🌺🌷🌸💮🏵️🌻🌼";

      std::string buffer{};
      glz::write_json(str, buffer);

      str.clear();
      expect(glz::read_json(str, buffer) == glz::error_code::none);

      expect(str == "😀😃😄🍌💐🌹🥀🌺🌷🌸💮🏵️🌻🌼");
   };

   "unicode_unescaped_smile"_test = [] {
      std::string str = R"({"😀":"smile"})";
      unicode_keys_t obj{};
      expect(glz::read_json(obj, str) == glz::error_code::none);

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
      expect(glz::read_json(obj, str) == glz::error_code::none);

      expect(obj.text == "ᇿ");
   };

   "unicode_escaped"_test = [] {
      std::string str = R"({"\u11FF":"\u11FF"})";
      question_t obj{};
      expect(glz::read_json(obj, str) == glz::error_code::none);

      expect(obj.text == "ᇿ");
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
      glz::write_json(v, s);
      expect(s == "5");
   };

   "lambda value"_test = [] {
      std::string s = "5";
      lambda_value_t v{};

      expect(glz::read_json(v, s) == glz::error_code::none);
      expect(v.x == 5);

      s.clear();
      glz::write_json(v, s);
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
      glz::write_json(msg, buffer);

      buffer.emplace_back('\0');

      msg.id = 0;
      msg.val = "";

      expect(glz::read_json(msg, buffer) == glz::error_code::none);
      expect(msg.id == 5);
      expect(msg.val == "hello");
   };

   "std::byte buffer"_test = [] {
      TestMsg msg{};
      msg.id = 5;
      msg.val = "hello";
      std::vector<std::byte> buffer{};
      glz::write_json(msg, buffer);

      buffer.emplace_back(static_cast<std::byte>('\0'));

      msg.id = 0;
      msg.val = "";

      expect(glz::read_json(msg, buffer) == glz::error_code::none);
      expect(msg.id == 5);
      expect(msg.val == "hello");
   };

   "char8_t buffer"_test = [] {
      TestMsg msg{};
      msg.id = 5;
      msg.val = "hello";
      std::vector<char8_t> buffer{};
      glz::write_json(msg, buffer);

      buffer.emplace_back('\0');

      msg.id = 0;
      msg.val = "";

      expect(glz::read_json(msg, buffer) == glz::error_code::none);
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
      glz::write_json(c, s);
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

static_assert(glz::detail::emplaceable<std::set<std::string>>);

suite sets = [] {
   "std::unordered_set"_test = [] {
      std::unordered_set<std::string> set;
      expect(glz::read_json(set, "[]") == glz::error_code::none);
      expect(set.empty());

      set = {"hello", "world"};
      std::string b{};
      glz::write_json(set, b);

      expect(b == R"(["hello","world"])" || b == R"(["world","hello"])");

      set.clear();

      expect(glz::read_json(set, b) == glz::error_code::none);

      expect(set.count("hello") == 1);
      expect(set.count("world") == 1);
   };

   "std::set<int>"_test = [] {
      std::set<int> set;
      expect(glz::read_json(set, "[]") == glz::error_code::none);
      expect(set.empty());

      set = {5, 4, 3, 2, 1};
      std::string b{};
      glz::write_json(set, b);

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
   };

   "std::set<std::string>"_test = [] {
      std::set<std::string> set;
      expect(glz::read_json(set, "[]") == glz::error_code::none);
      expect(set.empty());

      set = {"a", "b", "c", "d", "e"};
      std::string b{};
      glz::write_json(set, b);

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
      glz::write_json(set, b);

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
      auto s = glz::write_json(things);
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
      glz::write_json(s, b);

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

   "get_sv_arry"_test = [] {
      std::string s = R"({"obj":{"x":[0,1,2]}})";

      auto x = glz::get_as_json<std::vector<int>, "/obj/x">(s);
      expect(x == std::vector<int>{0, 1, 2});
      auto x0 = glz::get_as_json<int, "/obj/x/0">(s);
      expect(x0 == 0);
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
};

suite no_except_tests = [] {
   "no except"_test = [] {
      my_struct s{};
      std::string b = R"({"i":5,,})";
      auto ec = glz::read_json(s, b);
      expect(ec != glz::error_code::none) << static_cast<uint32_t>(ec);
   };
};

suite validation_tests = [] {
   "validate_json"_test = [] {
      glz::json_t json{};

      // Tests are taken from the https://www.json.org/JSON_checker/ test suite

      // I disagree with this
      // std::string fail1 = R"("A JSON payload should be an object or array, not a string.")";
      // auto ec_fail1 = glz::read<glz::opts{.force_conformance = true}>(json, fail1);
      // expect(ec_fail1 != glz::error_code::none);
      ////expect(glz::validate_json(fail1) != glz::error_code::none);

      std::string fail10 = R"({"Extra value after close": true} "misplaced quoted value")";
      auto ec_fail10 = glz::read<glz::opts{.force_conformance = true}>(json, fail10);
      expect(ec_fail10 != glz::error_code::none);
      expect(glz::validate_json(fail10) != glz::error_code::none);

      std::string fail11 = R"({"Illegal expression": 1 + 2})";
      auto ec_fail11 = glz::read<glz::opts{.force_conformance = true}>(json, fail11);
      expect(ec_fail11 != glz::error_code::none);
      expect(glz::validate_json(fail11) != glz::error_code::none);

      std::string fail12 = R"({"Illegal invocation": alert()})";
      auto ec_fail12 = glz::read<glz::opts{.force_conformance = true}>(json, fail12);
      expect(ec_fail12 != glz::error_code::none);
      expect(glz::validate_json(fail12) != glz::error_code::none);

      std::string fail13 = R"({"Numbers cannot have leading zeroes": 013})";
      auto ec_fail13 = glz::read<glz::opts{.force_conformance = true}>(json, fail13);
      expect(ec_fail13 != glz::error_code::none);
      expect(glz::validate_json(fail13) != glz::error_code::none);

      std::string fail14 = R"({"Numbers cannot be hex": 0x14})";
      auto ec_fail14 = glz::read<glz::opts{.force_conformance = true}>(json, fail14);
      expect(ec_fail14 != glz::error_code::none);
      expect(glz::validate_json(fail14) != glz::error_code::none);

      std::string fail15 = R"(["Illegal backslash escape: \x15"])";
      auto ec_fail15 = glz::read<glz::opts{.force_conformance = true}>(json, fail15);
      expect(ec_fail15 != glz::error_code::none);
      expect(glz::validate_json(fail15) != glz::error_code::none);

      std::string fail16 = R"([\naked])";
      auto ec_fail16 = glz::read<glz::opts{.force_conformance = true}>(json, fail16);
      expect(ec_fail16 != glz::error_code::none);
      expect(glz::validate_json(fail16) != glz::error_code::none);

      std::string fail17 = R"(["Illegal backslash escape: \017"])";
      auto ec_fail17 = glz::read<glz::opts{.force_conformance = true}>(json, fail17);
      expect(ec_fail17 != glz::error_code::none);
      expect(glz::validate_json(fail17) != glz::error_code::none);

      // JSON spec does not specify a nesting limit to my knowledge
      // std::string fail18 = R"([[[[[[[[[[[[[[[[[[[["Too deep"]]]]]]]]]]]]]]]]]]]])";
      // auto ec_fail18 = glz::read<glz::opts{.force_conformance = true}>(json, fail18);
      // expect(ec_fail18 != glz::error_code::none);
      // expect(glz::validate_json(fail18) != glz::error_code::none);

      std::string fail19 = R"({"Missing colon" null})";
      auto ec_fail19 = glz::read<glz::opts{.force_conformance = true}>(json, fail19);
      expect(ec_fail19 != glz::error_code::none);
      expect(glz::validate_json(fail19) != glz::error_code::none);

      std::string fail2 = R"(["Unclosed array")";
      auto ec_fail2 = glz::read<glz::opts{.force_conformance = true}>(json, fail2);
      expect(ec_fail2 != glz::error_code::none);
      expect(glz::validate_json(fail2) != glz::error_code::none);

      std::string fail20 = R"({"Double colon":: null})";
      auto ec_fail20 = glz::read<glz::opts{.force_conformance = true}>(json, fail20);
      expect(ec_fail20 != glz::error_code::none);
      expect(glz::validate_json(fail20) != glz::error_code::none);

      std::string fail21 = R"({"Comma instead of colon", null})";
      auto ec_fail21 = glz::read<glz::opts{.force_conformance = true}>(json, fail21);
      expect(ec_fail21 != glz::error_code::none);
      expect(glz::validate_json(fail21) != glz::error_code::none);

      std::string fail22 = R"(["Colon instead of comma": false])";
      auto ec_fail22 = glz::read<glz::opts{.force_conformance = true}>(json, fail22);
      expect(ec_fail22 != glz::error_code::none);
      expect(glz::validate_json(fail22) != glz::error_code::none);

      std::string fail23 = R"(["Bad value", truth])";
      auto ec_fail23 = glz::read<glz::opts{.force_conformance = true}>(json, fail23);
      expect(ec_fail23 != glz::error_code::none);
      expect(glz::validate_json(fail23) != glz::error_code::none);

      std::string fail24 = R"(['single quote'])";
      auto ec_fail24 = glz::read<glz::opts{.force_conformance = true}>(json, fail24);
      expect(ec_fail24 != glz::error_code::none);
      expect(glz::validate_json(fail24) != glz::error_code::none);

      std::string fail25 = R"(["	tab	character	in	string	"])";
      auto ec_fail25 = glz::read<glz::opts{.force_conformance = true}>(json, fail25);
      expect(ec_fail25 != glz::error_code::none);
      expect(glz::validate_json(fail25) != glz::error_code::none);

      std::string fail26 = R"(["tab\   character\   in\  string\  "])";
      auto ec_fail26 = glz::read<glz::opts{.force_conformance = true}>(json, fail26);
      expect(ec_fail26 != glz::error_code::none);
      expect(glz::validate_json(fail26) != glz::error_code::none);

      std::string fail27 = R"(["line
break"])";
      auto ec_fail27 = glz::read<glz::opts{.force_conformance = true}>(json, fail27);
      expect(ec_fail27 != glz::error_code::none);
      expect(glz::validate_json(fail27) != glz::error_code::none);

      std::string fail28 = R"(["line\
break"])";
      auto ec_fail28 = glz::read<glz::opts{.force_conformance = true}>(json, fail28);
      expect(ec_fail28 != glz::error_code::none);
      expect(glz::validate_json(fail28) != glz::error_code::none);

      std::string fail29 = R"([0e])";
      auto ec_fail29 = glz::read<glz::opts{.force_conformance = true}>(json, fail29);
      expect(ec_fail29 != glz::error_code::none);
      expect(glz::validate_json(fail29) != glz::error_code::none);

      std::string fail3 = R"({unquoted_key: "keys must be quoted"})";
      auto ec_fail3 = glz::read<glz::opts{.force_conformance = true}>(json, fail3);
      expect(ec_fail3 != glz::error_code::none);
      expect(glz::validate_json(fail3) != glz::error_code::none);

      std::string fail30 = R"([0e+])";
      auto ec_fail30 = glz::read<glz::opts{.force_conformance = true}>(json, fail30);
      expect(ec_fail30 != glz::error_code::none);
      expect(glz::validate_json(fail30) != glz::error_code::none);

      std::string fail31 = R"([0e+-1])";
      auto ec_fail31 = glz::read<glz::opts{.force_conformance = true}>(json, fail31);
      expect(ec_fail31 != glz::error_code::none);
      expect(glz::validate_json(fail31) != glz::error_code::none);

      std::string fail32 = R"({"Comma instead if closing brace": true,)";
      auto ec_fail32 = glz::read<glz::opts{.force_conformance = true}>(json, fail32);
      expect(ec_fail32 != glz::error_code::none);
      expect(glz::validate_json(fail32) != glz::error_code::none);

      std::string fail33 = R"(["mismatch"})";
      auto ec_fail33 = glz::read<glz::opts{.force_conformance = true}>(json, fail33);
      expect(ec_fail33 != glz::error_code::none);
      expect(glz::validate_json(fail33) != glz::error_code::none);

      std::string fail4 = R"(["extra comma",])";
      auto ec_fail4 = glz::read<glz::opts{.force_conformance = true}>(json, fail4);
      expect(ec_fail4 != glz::error_code::none);
      expect(glz::validate_json(fail4) != glz::error_code::none);

      std::string fail5 = R"(["double extra comma",,])";
      auto ec_fail5 = glz::read<glz::opts{.force_conformance = true}>(json, fail5);
      expect(ec_fail5 != glz::error_code::none);
      expect(glz::validate_json(fail5) != glz::error_code::none);

      std::string fail6 = R"([   , "<-- missing value"])";
      auto ec_fail6 = glz::read<glz::opts{.force_conformance = true}>(json, fail6);
      expect(ec_fail6 != glz::error_code::none);
      expect(glz::validate_json(fail6) != glz::error_code::none);

      std::string fail7 = R"(["Comma after the close"],)";
      auto ec_fail7 = glz::read<glz::opts{.force_conformance = true}>(json, fail7);
      expect(ec_fail7 != glz::error_code::none);
      expect(glz::validate_json(fail7) != glz::error_code::none);

      std::string fail8 = R"(["Extra close"]])";
      auto ec_fail8 = glz::read<glz::opts{.force_conformance = true}>(json, fail8);
      expect(ec_fail8 != glz::error_code::none);
      expect(glz::validate_json(fail8) != glz::error_code::none);

      std::string fail9 = R"({"Extra comma": true,})";
      auto ec_fail9 = glz::read<glz::opts{.force_conformance = true}>(json, fail9);
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
      auto ec_pass1 = glz::read<glz::opts{.force_conformance = true}>(json, pass1);
      expect(ec_pass1 == glz::error_code::none);
      expect(glz::validate_json(pass1) == glz::error_code::none);

      std::string pass2 = R"([[[[[[[[[[[[[[[[[[["Not too deep"]]]]]]]]]]]]]]]]]]])";
      auto ec_pass2 = glz::read<glz::opts{.force_conformance = true}>(json, pass2);
      expect(ec_pass2 == glz::error_code::none);
      expect(glz::validate_json(pass2) == glz::error_code::none);

      std::string pass3 = R"({
    "JSON Test Pattern pass3": {
        "The outermost value": "must be an object or array.",
        "In this test": "It is an object."
    }
}
)";
      auto ec_pass3 = glz::read<glz::opts{.force_conformance = true}>(json, pass3);
      expect(ec_pass3 == glz::error_code::none);
      expect(glz::validate_json(pass3) == glz::error_code::none);
   };
};

// TODO: Perhaps add bit field support
/*struct bit_field_t
{
   uint32_t x : 27;
   unsigned char : 0;
   bool b : 1;
   uint64_t i : 63;
};

template <>
struct glz::meta<bit_field_t>
{
   using T = bit_field_t;
   static constexpr auto value = object("x", getset{ [](auto& s) { return s.x; }, [](auto& s, auto value) { s.x = value;
} });
};

suite bit_field_test = []
{
   "bit field"_test = []
   {
      bit_field_t s{};
      std::string b = R"({"x":19,"b":true,"i":5})";
      auto ec = glz::read_json(s, b);
      expect(ec == glz::error_code::none);
      expect(s.x == 19);
      expect(s.b);
      expect(s.i == 5);
   };
};*/

struct StructE
{
   std::string e;
   GLZ_LOCAL_META(StructE, e);
};

struct Sample
{
   int a;
   StructE d;
   GLZ_LOCAL_META(Sample, a, d);
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

   GLZ_LOCAL_META(OKX_OrderBook_Data, alias, baseCcy, category, ctMult, ctType, ctVal, ctValCcy, expTime, instFamily,
                  instId, instType, lever, listTime, lotSz, maxIcebergSz, maxLmtSz, maxMktSz, maxStopSz, maxTriggerSz,
                  maxTwapSz, minSz, optType, quoteCcy, settleCcy, state, stk, tickSz, uly);
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
      glz::write_json(order_book, buffer);
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
      glz::write_json(a, buffer);
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
      glz::write_json(a, buffer);
      expect(buffer == R"({"x":"3.14","y":["1","2","3"],"z":[["1","2","3"]]})");

      buffer = R"({"x":"999.2","y":["4","5","6"],"z":[["4","5"]]})";
      expect(glz::read<glz::opts{.error_on_missing_keys = true}>(a, buffer) == glz::error_code::none);
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
      glz::write_json(a, buffer);
      expect(buffer == R"({"x":{"1":"2"}})");

      a = {};
      buffer = R"({"x":{"3":"4"}})";
      expect(glz::read_json(a, buffer) == glz::error_code::none);
      expect(a.x == std::map<uint32_t, uint64_t>{{3, 4}});
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
      glz::write_json(a, buffer);
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
   using namespace boost::ut;
   using namespace boost::ut::operators;
   "arbitrary_key_maps"_test =
      [](const auto& test_case) {
         const auto& [name, input, serialized] = test_case;
         std::string buffer{};
         glz::write_json(input, buffer);
         expect(buffer == serialized);

         std::decay_t<decltype(input)> parsed{};
         expect(glz::read_json(parsed, serialized) == glz::error_code::none);
         expect(parsed.x == input.x);
      } |
      std::tuple{Arbitrary_key_test_case<array_map>{.name = "array_map",
                                                    .input = {.x = {{{1, 2, 3}, "hello"}, {{4, 5, 6}, "goodbye"}}},
                                                    .serialized = R"({"x":{"[1,2,3]":"hello","[4,5,6]":"goodbye"}})"},
                 Arbitrary_key_test_case<custom_key_map>{
                    .name = "custom_key_map",
                    .input = {.x = {{{-1, "k.2"}, "value"}}},
                    .serialized = R"({"x":{"{\"field1\":-1,\"field2\":\"k.2\"}":"value"}})",
                 }};
};

suite char_array = [] {
   "char array write"_test = [] {
      char arr[12] = "Hello World";

      std::string s{};
      glz::write_json(arr, s);
      expect(s == R"("Hello World")");

      char arr2[12] = "Hello\0World";

      glz::write_json(arr2, s);
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
      expect(err != glz::error_code::none);
      expect(glz::format_error(err, buffer) == "missing_key");
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

suite numbers_as_strings_suite = [] {
   "numbers_as_strings"_test = [] {
      numbers_as_strings obj{};

      std::string input = R"({"x":555,"y":3.14})";
      expect(glz::read_json(obj, input) == glz::error_code::none);
      expect(obj.x == "555");
      expect(obj.y == "3.14");

      std::string output;
      glz::write_json(obj, output);
      expect(input == output);
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

suite json_logging = [] {
   "json_logging"_test = [] {
      glz::arr vec = {1, 2, 3};
      glz::obj map = {"a", 1, "b", 2, "c", 3};
      auto obj = glz::obj{
         "pi", 3.141, "happy", true, "name", "Stephen", "map", map, "arr", glz::arr{"Hello", "World", 2}, "vec", vec};

      glz::get<0>(map.value) = "aa"; // testing lvalue reference storage

      std::string s{};
      glz::write_json(obj, s);

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
      glz::write_json(obj, s);

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
      glz::write_json(merged, s);

      expect(s == R"({"pie":3.141,"happy":true,"arr":["Hello","World",2]})") << s;
   };

   "merge_custom"_test = [] {
      glz::obj obj0{"pi", 3.141};
      std::map<std::string_view, int> map = {{"a", 1}, {"b", 2}, {"c", 3}};
      auto merged = glz::merge{obj0, map, my_struct{}};
      map["a"] = 0; // testing lvalue reference storage
      std::string s{};
      glz::write_json(merged, s);

      expect(s == R"({"pi":3.141,"a":0,"b":2,"c":3,"i":287,"d":3.14,"hello":"Hello World","arr":[1,2,3]})") << s;
   };
};

struct non_cx_values
{
   std::string_view info{"information"};
   int index{42};
   std::string value{};
};

static_assert(std::is_same_v<glz::detail::member_t<non_cx_values, decltype(&non_cx_values::info)>, std::string_view&>);

struct cx_values
{
   static constexpr std::string_view info{"information"};
   static constexpr auto index{42};
   std::string value{};
};

static_assert(std::is_same_v<glz::detail::member_t<cx_values, decltype(&cx_values::info)>, const std::string_view&>);

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
static_assert(glz::detail::glaze_const_value_t<direct_cx_value_conversion>);

struct direct_cx_value_conversion_different_value
{
   static constexpr std::uint64_t const_v{1337};
   struct glaze
   {
      static constexpr auto value{&direct_cx_value_conversion_different_value::const_v};
   };
};
static_assert(glz::detail::glaze_const_value_t<direct_cx_value_conversion_different_value>);

struct string_direct_cx_value_conversion
{
   static constexpr std::string_view const_v{"other"};
   struct glaze
   {
      static constexpr auto value{&string_direct_cx_value_conversion::const_v};
   };
};
static_assert(glz::detail::glaze_const_value_t<string_direct_cx_value_conversion>);

struct string_two_direct_cx_value_conversion
{
   static constexpr std::string_view const_v{"two"};
   struct glaze
   {
      static constexpr auto value{&string_two_direct_cx_value_conversion::const_v};
   };
};
static_assert(glz::detail::glaze_const_value_t<string_two_direct_cx_value_conversion>);

struct array_direct_cx_value_conversion
{
   static constexpr std::array<std::string_view, 2> const_v{"one", "two"};
   struct glaze
   {
      static constexpr auto value{&array_direct_cx_value_conversion::const_v};
   };
};
static_assert(glz::detail::glaze_const_value_t<array_direct_cx_value_conversion>);

struct array_two_direct_cx_value_conversion
{
   static constexpr std::array<std::string_view, 2> const_v{"two", "one"};
   struct glaze
   {
      static constexpr auto value{&array_two_direct_cx_value_conversion::const_v};
   };
};
static_assert(glz::detail::glaze_const_value_t<array_two_direct_cx_value_conversion>);

struct non_cx_direct_value_conversion
{
   std::string some_other{"other"};
   struct glaze
   {
      static constexpr auto value{&non_cx_direct_value_conversion::some_other};
   };
};
static_assert(!glz::detail::glaze_const_value_t<non_cx_direct_value_conversion>);

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
      glz::write_json(obj, s);
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
   "constexpr blend with non constexpr variant string"_test = []<typename const_t>() {
      const_only_variant var{const_t{}};
      std::string s{};
      glz::write_json(var, s);
      std::string expected{};
      glz::write_json(const_t::const_v, expected);
      expect(s == expected) << s;
      auto parse_err{glz::read_json(var, s)};
      expect(parse_err == glz::error_code::none) << glz::format_error(parse_err, s);
      expect(std::holds_alternative<const_t>(var));
   } | variant_to_tuple<const_only_variant>::type{};

   "parse error direct_conversion_variant cx int"_test = [] {
      const_only_variant var{direct_cx_value_conversion{}};
      auto const parse_err{glz::read_json(var, R"(33)")};
      expect(parse_err == glz::error_code::no_matching_variant_type);
   };

   "constexpr blend with non constexpr variant"_test = [] {
      std::variant<std::monostate, direct_cx_value_conversion_different_value, direct_cx_value_conversion,
                   std::uint64_t>
         var{std::uint64_t{111}};
      std::string s{};
      glz::write_json(var, s);
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
      glz::write_json(obj, s);
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
      glz::write_json(o, s);
      expect(s == "null");

      o = {};
      glz::write_json(o, s);
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
      expect(!glz::read_json(obj, s));
      expect(obj.y == 26); // 5 * 5 + 1
   };
};

suite char_buffer = [] {
   "null char*"_test = [] {
      char* str{};
      std::string s{};
      glz::write_json(str, s);
      expect(s == R"("")");
   };

   "char*"_test = [] {
      std::string str = "Spiders";
      char* ptr = str.data();
      std::string s{};
      glz::write_json(ptr, s);
      expect(s == R"("Spiders")");
   };
};

static_assert(!glz::detail::char_array_t<char*>);

suite enum_map = [] {
   "enum map"_test = [] {
      std::map<Color, std::string> color_map;
      color_map[Color::Red] = "red";
      color_map[Color::Green] = "green";
      color_map[Color::Blue] = "blue";

      std::string s{};
      glz::write_json(color_map, s);

      expect(s == R"({"Red":"red","Green":"green","Blue":"blue"})");

      color_map.clear();
      expect(!glz::read_json(color_map, s));
      expect(color_map.at(Color::Red) == "red");
      expect(color_map.at(Color::Green) == "green");
      expect(color_map.at(Color::Blue) == "blue");
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

      auto s = glz::write_json(vec);
      expect(s == R"([{"cnt":0},{"cnt":1},{"cnt":2},{"cnt":3},{"cnt":4}])") << s;
   };
};

suite obj_nested_merge = [] {
   "obj_nested_merge"_test = [] {
      glz::obj o{"not", "important"};
      glz::obj o2{"map", glz::obj{"a", 1, "b", 2, "c", 3}};
      auto merged = glz::merge{o, o2};
      std::string s{};
      glz::write_json(merged, s);
      expect(s == R"({"not":"important","map":{"a":1,"b":2,"c":3}})") << s;
   };

   "obj_json_t_merge"_test = [] {
      glz::json_t json;
      expect(
         !glz::read_json(json, "{\"key1\":42,\"key2\":\"hello world\",\"v\":[1,2,3],\"m\":{\"a\":1,\"b\":2,\"c\":3}}"));
      glz::obj obj{"not", "important"};
      auto s = glz::write_json(glz::merge{obj, json});
      expect(s == R"({"not":"important","key1":42,"key2":"hello world","m":{"a":1,"b":2,"c":3},"v":[1,2,3]})") << s;
   };
};

suite write_to_map = [] {
   "write_obj_to_map"_test = [] {
      std::map<std::string, glz::raw_json> map;
      glz::obj obj{"arr", glz::arr{1, 2, 3}, "hello", "world"};
      using T = std::decay_t<decltype(obj.value)>;
      glz::for_each<std::tuple_size_v<T>>([&](auto I) {
         if constexpr (I % 2 == 0) {
            map[std::string(glz::get<I>(obj.value))] = glz::write_json(glz::get<I + 1>(obj.value));
         }
      });

      auto s = glz::write_json(map);
      expect(s == R"({"arr":[1,2,3],"hello":"world"})") << s;
   };

   "write_json_t_to_map"_test = [] {
      std::map<std::string, glz::raw_json> map;

      glz::json_t obj{{"arr", {1, 2, 3}}, {"hello", "world"}};
      auto& o = obj.get<glz::json_t::object_t>();
      for (auto& [key, value] : o) {
         map[key] = glz::write_json(value);
      }

      auto s = glz::write_json(map);
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
      expect(!glz::read_json(i, buffer));
      expect(i == 0);

      buffer = "0.0000666666666666666600";
      i = 5;
      expect(!glz::read_json(i, buffer));
      expect(i == 0);

      buffer = "0.00000000000000000000000";
      i = 5;
      expect(!glz::read_json(i, buffer));
      expect(i == 0);

      buffer = "6E19";
      expect(glz::read_json(i, buffer) == glz::error_code::parse_number_failure);

      buffer = "e5555511116";
      expect(glz::read_json(i, buffer) == glz::error_code::parse_number_failure);
   };

   "long float uint64_t"_test = [] {
      std::string_view buffer{"0.00666666666666666600"};
      uint64_t i{5};
      expect(!glz::read_json(i, buffer));
      expect(i == 0);

      buffer = "0.0000666666666666666600";
      i = 5;
      expect(!glz::read_json(i, buffer));
      expect(i == 0);

      buffer = "0.00000000000000000000000";
      i = 5;
      expect(!glz::read_json(i, buffer));
      expect(i == 0);

      buffer = "6E19";
      expect(glz::read_json(i, buffer) == glz::error_code::parse_number_failure);

      buffer = "0.1e999999999999999999";
      expect(glz::read_json(i, buffer) == glz::error_code::parse_number_failure);

      buffer = "0.1e-999999999999999999";
      expect(!glz::read_json(i, buffer));
      expect(i == 0);
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

      glz::write_json(i, buffer);
      expect(buffer == "-2147483648");
   };

   "minimum int64_t"_test = [] {
      std::string buffer{"-9223372036854775808"};
      int64_t i{};
      expect(!glz::read_json(i, buffer));
      expect(i == (std::numeric_limits<int64_t>::min)());

      glz::write_json(i, buffer);
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

suite read_as_json_raw = [] {
   "read_as_json_raw"_test = [] {
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
   static constexpr auto value = object("id", &T::id, "coordinates", [](auto& self) {
      return coordinates_t{&self.latitude, &self.longitude};
   });
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

   GLZ_LOCAL_META(name_t, first, last);
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
      glz::write_json(obj, out);
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
      glz::write_json(obj, s);
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
      glz::write_json(obj, s);
      expect(s == R"({"str":"Hello!"})");
      expect(obj.str == "Hello!");
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
      glz::write_json(obj, s);
      expect(s == R"({"i":55})");
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
      glz::write_json(obj, out);
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
      glz::write_json(cx, s);
      expect(s == R"([1,2])");
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
      glz::write_json(obj, s);
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
      glz::write_json(obj, s);
      expect(s == R"({"x":[1,2,3]})");
      expect(obj.x[0] == 1);
      expect(obj.x[1] == 2);
      expect(obj.x[2] == 3);
   };
};

struct varx
{
   GLZ_LOCAL_META(varx);
};
struct vary
{
   GLZ_LOCAL_META(vary);
};

using vari = std::variant<varx, vary>;

template <>
struct glz::meta<vari>
{
   static constexpr std::string_view tag = "type";
};

suite empty_variant_objects = [] {
   "empty_variant_objects"_test = [] {
      vari v = varx{};
      std::string s;
      glz::write_json(v, s);
      expect(s == R"({"type":"varx"})");

      v = vary{};

      expect(!glz::read_json(v, s));
      expect(std::holds_alternative<varx>(v));
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
   uint64_t time;
   std::string action; // send, recv
   std::string quote; // order, kill
   std::string account;
   uint32_t uid;
   uint32_t session_id;
   uint32_t request_id;
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

GLZ_META(QuoteData, time, action, quote, account, uid, session_id, request_id, state, order_id, exchange, type, tif,
         offset, side, symbol, price, quantity, traded);

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
      glz::write<glz::opts{}>(q, buffer);

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

   GLZ_LOCAL_META(updater, x, square, add_one);
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
      glz::write<glz::opts{.raw_string = true}>(obj, buffer);
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
      glz::write_json(obj, buffer);
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
      glz::write_json(obj, buffer);
      expect(buffer == R"({"a":"Hello\nWorld","b":"","c":""})");
   };
};

struct Update
{
   int64_t time;
   GLZ_LOCAL_META(Update, time);
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
      glz::write_json(b, s);

      expect(s == R"("10101010")") << s;

      b.reset();
      expect(!glz::read_json(b, s));
      expect(b == 0b10101010);
   };

   "bitset16"_test = [] {
      std::bitset<16> b = 0b10010010'00000010;

      std::string s{};
      glz::write_json(b, s);

      expect(s == R"("1001001000000010")") << s;

      b.reset();
      expect(!glz::read_json(b, s));
      expect(b == 0b10010010'00000010);
   };
};

#if defined(__STDCPP_FLOAT128_T__)
suite float128_test = [] {
   "float128"_test = [] {
      std::float128_t x = 3.14;

      std::string s{};
      glz::write_json(x, s);

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

suite unknown_fields_member_test = [] {
   "decode_unknown"_test = [] {
      unknown_fields_member obj{};

      std::string buffer = R"({"a":"aaa","unk":"zzz", "unk2":{"sub":3,"sub2":[{"a":"b"}]},"unk3":[], "end":"end"})";

      glz::context ctx{};

      expect(!glz::read<glz::opts{.error_on_unknown_keys = false}>(obj, buffer, ctx));

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
      glz::write_json(obj, s);

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
      auto s = glz::write_json(obj);

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

      auto s = glz::write_json(obj);
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
      glz::write_json(obj, s);
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

      constexpr auto prettify = glz::opts{.prettify = true};

      std::string data = glz::write<prettify>(objects);

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

      expect(data == glz::write<prettify>(objects));
   };
};

struct hostname_include_struct
{
   std::string str = "Hello";
   int i = 55;
};

template <>
struct glz::meta<hostname_include_struct>
{
   using T = hostname_include_struct;
   static constexpr auto value = object("#hostname_include", glz::hostname_include{}, "str", &T::str, "i", &T::i);
};

suite hostname_include_test = [] {
   hostname_include_struct obj{};

   glz::context ctx{};
   const auto hostname = glz::detail::get_hostname(ctx);

   std::string file_name = "../{}_config.json";
   glz::detail::replace_first_braces(file_name, hostname);

   expect(glz::write_file_json(obj, file_name, std::string{}) == glz::error_code::none);

   obj.str = "";
   obj.i = 0;

   std::string s = R"({"#hostname_include": "../{}_config.json", "i": 100})";
   const auto ec = glz::read_json(obj, s);
   expect(ec == glz::error_code::none) << glz::format_error(ec, s);

   expect(obj.str == "Hello") << obj.str;
   expect(obj.i == 100) << obj.i;

   obj.str = "";

   std::string buffer{};
   glz::read_file_json(obj, file_name, buffer);
   expect(obj.str == "Hello") << obj.str;
   expect(obj.i == 55) << obj.i;
};

int main()
{
   // Explicitly run registered test suites and report errors
   // This prevents potential issues with thread local variables
   const auto result = boost::ut::cfg<>.run({.report_errors = true});
   return result;
}
