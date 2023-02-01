// Glaze Library
// For the license information refer to glaze.hpp

#include <chrono>
#include <iostream>
#include <random>
#include <any>
#include <forward_list>
#include <unordered_map>
#include <map>
#include <list>
#include <deque>

#include "glaze/core/macros.hpp"
#include "boost/ut.hpp"
#include "glaze/json/json_ptr.hpp"
#include "glaze/json/ptr.hpp"
#include "glaze/json/read.hpp"
#include "glaze/json/write.hpp"
#include "glaze/json/prettify.hpp"
#include "glaze/util/progress_bar.hpp"
#include "glaze/api/impl.hpp"
#include "glaze/record/recorder.hpp"

using namespace boost::ut;

struct my_struct
{
  int i = 287;
  double d = 3.14;
  std::string hello = "Hello World";
  std::array<uint64_t, 3> arr = { 1, 2, 3 };
};

template <>
struct glz::meta<my_struct> {
   static constexpr std::string_view name = "my_struct";
   using T = my_struct;
   static constexpr auto value = object(
      "i", [](auto&& v) { return v.i; },  //
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
struct glz::meta<sub_thing> {
   static constexpr std::string_view name = "sub_thing";
   static constexpr auto value = object(
      "a", &sub_thing::a, "Test comment 1",  //
      "b", [](auto&& v) -> auto& { return v.b; }, "Test comment 2"  //
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
struct glz::meta<sub_thing2> {
   using T = sub_thing2;
   static constexpr std::string_view name = "sub_thing2";
   static constexpr auto value = object(
      "a", &T::a, "Test comment 1",    //
      "b", &T::b, "Test comment 2",    //
      "c", &T::c,                      //
      "d", &T::d,                      //
      "e", &T::e,                      //
      "f", &T::f,                      //
      "g", &T::g,                      //
      "h", &T::h                       //
   );
};

struct V3
{
   double x{3.14};
   double y{2.7};
   double z{6.5};

   bool operator==(const V3& rhs) const
   {
      return (x == rhs.x) && (y == rhs.y) && (z == rhs.z);
   }
};

template <>
struct glz::meta<V3> {
   static constexpr std::string_view name = "V3";
   static constexpr auto value = array(&V3::x, &V3::y, &V3::z);
};

enum class Color {
   Red,
   Green,
   Blue
};

template <>
struct glz::meta<Color>
{
   static constexpr std::string_view name = "Color";
   using enum Color;
   static constexpr auto value = enumerate("Red", Red,      //
                                           "Green", Green,  //
                                           "Blue", Blue     //
   );
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
struct glz::meta<Thing> {
   using T = Thing;
   static constexpr std::string_view name = "Thing";
   static constexpr auto value = object(
      "thing",       &T::thing,                                     //
      "thing2array", &T::thing2array,                               //
      "vec3",        &T::vec3,                                      //
      "list",        &T::list,                                      //
      "deque",       &T::deque,                                     //
      "vector",      [](auto&& v) -> auto& { return v.vector; },    //
      "i",           [](auto&& v) -> auto& { return v.i; },         //
      "d",           &T::d,           "double is the best type",    //
      "b",           &T::b,                                         //
      "c",           &T::c,                                         //
      "v",           &T::v,                                         //
      "color",       &T::color,                                     //
      "vb",          &T::vb,                                        //
      "sptr",        &T::sptr,                                      //
      "optional",    &T::optional,                                  //
      "array",       &T::array,                                     //
      "map",         &T::map,                                       //
      "mapi",        &T::mapi,                                      //
      "thing_ptr",   &T::thing_ptr                                  //
   );
};

struct Escaped
{
   int escaped_key{};
   std::string escaped_key2{ "hi" };
   std::string escape_chars{""};
};

template <>
struct glz::meta<Escaped> {
   static constexpr std::string_view name = "Escaped";
   using T = Escaped;
   static constexpr auto value = object(R"(escaped"key)", &T::escaped_key, //
                                        R"(escaped""key2)", &T::escaped_key2,
                                        R"(escape_chars)", &T::escape_chars);
};

suite escaping_tests = [] {
   "escaped_key"_test = [] {
      std::string out;
      Escaped obj{};
      glz::write_json(obj, out);
      
      expect(out == R"({"escaped\"key":0,"escaped\"\"key2":"hi","escape_chars":""})");
      
      std::string in = R"({"escaped\"key":5,"escaped\"\"key2":"bye"})";
      glz::read_json(obj, in);
      expect(obj.escaped_key == 5);
      expect(obj.escaped_key2 == "bye");
   };

   "escaped_characters read"_test = [] {
      std::string in = R"({"escape_chars":"\b\f\n\r\t\u11FF"})";
      Escaped obj{};

      glz::read_json(obj, in);

      expect(obj.escape_chars == "\b\f\n\r\tᇿ") << obj.escape_chars;
   };

   "escaped_char read"_test = [] {
      std::string in = R"("\b")";
      char c;
      glz::read_json(c, in);
      expect(c == '\b');

      in = R"("\f")";
      glz::read_json(c, in);
      expect(c == '\f');

      in = R"("\n")";
      glz::read_json(c, in);
      expect(c == '\n');

      in = R"("\r")";
      glz::read_json(c, in);
      expect(c == '\r');

      in = R"("\t")";
      glz::read_json(c, in);
      expect(c == '\t');

      in = R"("\u11FF")";
      char32_t c32;
      glz::read_json(c32, in);
      expect(static_cast<uint32_t>(c32) == 0x11FF);

      in = R"("\u732B")";
      char16_t c16;
      glz::read_json(c16, in);
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

void basic_types() {
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
      glz::read_json(num, "3.14");
      expect(num == 3.14);
      glz::read_json(num, "9.81");
      expect(num == 9.81);
      glz::read_json(num, "0");
      expect(num == 0);
      glz::read_json(num, "-0");
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
      glz::read_json(num, "-1");
      expect(num == -1);
      glz::read_json(num, "0");
      expect(num == 0);
      glz::read_json(num, "999");
      expect(num == 999);
      glz::read_json(num, "1e4");
      expect(num == 10000);
      uint64_t num64{};
      glz::read_json(num64, "32948729483739289");
      expect(num64 == 32948729483739289);
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
      glz::read_json(val, "true");
      expect(val == true);
      glz::read_json(val, "false");
      expect(val == false);
   };
   
   "bool read invalid"_test = [] {
      bool val{};
      expect(throws([&]{ glz::read_json(val, "tru"); }));
      expect(throws([&]{ glz::read_json(val, "alse"); }));
   };

   "string write"_test = [] {
      std::string buffer{};
      glz::write_json("fish", buffer);
      expect(buffer == "\"fish\"");
      buffer.clear();
      glz::write_json("as\"df\\ghjkl", buffer);
      expect(buffer == "\"as\\\"df\\\\ghjkl\"");
   };

   "backslash testing"_test = [] {
      std::string val{};
      glz::read_json(val, "\"fish\"");
      expect(val == "fish");
      glz::read_json(val, "\"as\\\"df\\\\ghjkl\"");
      expect(val == "as\"df\\ghjkl");
   };
}

void container_types() {
   using namespace boost::ut;
   "vector int roundtrip"_test = [] {
      std::vector<int> vec(100);
      for (auto& item : vec) item = rand();
      std::string buffer{};
      std::vector<int> vec2{};
      glz::write_json(vec, buffer);
      glz::read_json(vec2, buffer);
      expect(vec == vec2);
   };
   "vector uint64_t roundtrip"_test = [] {
      std::uniform_int_distribution<uint64_t> dist(std::numeric_limits<uint64_t>::min(),
                                                             std::numeric_limits<uint64_t>::max());
      std::mt19937 gen{};
      std::vector<uint64_t> vec(100);
      for (auto& item : vec) item = dist(gen);
      std::string buffer{};
      std::vector<uint64_t> vec2{};
      glz::write_json(vec, buffer);
      glz::read_json(vec2, buffer);
      expect(vec == vec2);
   };
   "vector double roundtrip"_test = [] {
      std::vector<double> vec(100);
      for (auto& item : vec) item = rand() / (1.0 + rand());
      std::string buffer{};
      std::vector<double> vec2{};
      glz::write_json(vec, buffer);
      glz::read_json(vec2, buffer);
      expect(vec == vec2);
   };
   "vector bool roundtrip"_test = [] {
      std::vector<bool> vec(100);
      for (auto&& item : vec) item = rand() / (1.0 + rand());
      std::string buffer{};
      std::vector<bool> vec2{};
      glz::write_json(vec, buffer);
      glz::read_json(vec2, buffer);
      expect(vec == vec2);
   };
   "deque roundtrip"_test = [] {
      std::vector<int> deq(100);
      for (auto& item : deq) item = rand();
      std::string buffer{};
      std::vector<int> deq2{};
      glz::write_json(deq, buffer);
      glz::read_json(deq2, buffer);
      expect(deq == deq2);
   };
   "list roundtrip"_test = [] {
      std::list<int> lis(100);
      for (auto& item : lis) item = rand();
      std::string buffer{};
      std::list<int> lis2{};
      glz::write_json(lis, buffer);
      glz::read_json(lis2, buffer);
      expect(lis == lis2);
   };
   "forward_list roundtrip"_test = [] {
      std::forward_list<int> lis(100);
      for (auto& item : lis) item = rand();
      std::string buffer{};
      std::forward_list<int> lis2{};
      glz::write_json(lis, buffer);
      glz::read_json(lis2, buffer);
      expect(lis == lis2);
   };
   "map string keys roundtrip"_test = [] {
      std::map<std::string, int> map;
      std::string str {"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"};
      std::mt19937 g{};
      for (auto i = 0; i < 20; ++i) {
         std::shuffle(str.begin(), str.end(), g);
         map[str] = rand();
      }
      std::string buffer{};
      std::map<std::string, int> map2{};
      glz::write_json(map, buffer);
      glz::read_json(map2, buffer);
      //expect(map == map2);
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
      glz::read_json(map2, buffer);
      //expect(map == map2);
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
      glz::read_json(map2, buffer);
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
      glz::read_json(tuple2, buffer);
      expect(tuple == tuple2);
   };
   "pair roundtrip"_test = [] {
      auto pair = std::make_pair(std::string("water"), 5.2);
      decltype(pair) pair2{};
      std::string buffer{};
      glz::write_json(pair, buffer);
      glz::read_json(pair2, buffer);
      expect(pair == pair2);
   };
}

void nullable_types() {
   using namespace boost::ut;
   "optional"_test = [] {
      std::optional<int> oint{};
      std::string buffer{};
      glz::write_json(oint, buffer);
      expect(buffer == "null");

      glz::read_json(oint, "5");
      expect(bool(oint) && *oint == 5);
      buffer.clear();
      glz::write_json(oint, buffer);
      expect(buffer == "5");

      glz::read_json(oint, "null");
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

      glz::read_json(ptr, "5");
      expect(bool(ptr) && *ptr == 5);
      buffer.clear();
      glz::write_json(ptr, buffer);
      expect(buffer == "5");

      glz::read_json(ptr, "null");
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

      glz::read_json(ptr, "5");
      expect(bool(ptr) && *ptr == 5);
      buffer.clear();
      glz::write_json(ptr, buffer);
      expect(buffer == "5");

      glz::read_json(ptr, "null");
      expect(!bool(ptr));
      buffer.clear();
      glz::write_json(ptr, buffer);
      expect(buffer == "null");
   };
}

void enum_types()
{
   using namespace boost::ut;
   "enum"_test = [] {
      Color color = Color::Red;
      std::string buffer{};
      glz::write_json(color, buffer);
      expect(buffer == "\"Red\"");

      glz::read_json(color, "\"Green\"");
      expect(color == Color::Green);
      buffer.clear();
      glz::write_json(color, buffer);
      expect(buffer == "\"Green\"");
   };
}

void user_types() {
   using namespace boost::ut;

   "user array"_test = [] {
      V3 v3{9.1, 7.2, 1.9};
      std::string buffer{};
      glz::write_json(v3, buffer);
      expect(buffer == "[9.1,7.2,1.9]");

      glz::read_json(v3, "[42.1,99.2,55.3]");
      expect(v3.x == 42.1 && v3.y == 99.2 && v3.z == 55.3);
   };

   "simple user obect"_test = [] {
      sub_thing obj{.a = 77.2, .b = "not a lizard"};
      std::string buffer{};
      glz::write_json(obj, buffer);
      expect(buffer == "{\"a\":77.2,\"b\":\"not a lizard\"}");

      glz::read_json(obj, "{\"a\":999,\"b\":\"a boat of goldfish\"}");
      expect(obj.a == 999.0 && obj.b == "a boat of goldfish");

      //Should skip invalid keys
      expect(nothrow([&] {
         //glaze::read_json(obj,"{/**/ \"b\":\"fox\", \"c\":7.7/**/, \"d\": {\"a\": \"}\"} //\n   /**/, \"a\":322}");
         glz::read<glz::opts{.error_on_unknown_keys = false}>(obj,
                          R"({/**/ "b":"fox", "c":7.7/**/, "d": {"a": "}"} //
   /**/, "a":322})");
      }));
      
      expect(throws([&] {
         //glaze::read_json(obj,"{/**/ \"b\":\"fox\", \"c\":7.7/**/, \"d\": {\"a\": \"}\"} //\n   /**/, \"a\":322}");
         glz::read_json(obj,
                          R"({/**/ "b":"fox", "c":7.7/**/, "d": {"a": "}"} //
   /**/, "a":322})");
      }));
      expect(obj.a == 322.0 && obj.b == "fox");
   };

   "complex user obect"_test = [] {
      Thing obj{};
      std::string buffer{};
      glz::write_json(obj, buffer);
      expect(buffer == R"({"thing":{"a":3.14,"b":"stuff"},"thing2array":[{"a":3.14,"b":"stuff","c":999.342494903,"d":1E-12,"e":203082348402.1,"f":89.089,"g":12380.00000013,"h":1000000.000001}],"vec3":[3.14,2.7,6.5],"list":[6,7,8,2],"deque":[9,6.7,3.1],"vector":[[9,6.7,3.1],[3.14,2.7,6.5]],"i":8,"d":2,"b":false,"c":"W","v":{"type":"var1_t","x":0},"color":"Green","vb":[true,false,false,true,true,true,true],"sptr":{"a":3.14,"b":"stuff"},"array":["as\"df\\ghjkl","pie","42","foo"],"map":{"a":4,"b":12,"f":7},"mapi":{"2":9.63,"5":3.14,"7":7.42},"thing_ptr":{"a":3.14,"b":"stuff"}})") << buffer;

      buffer.clear();
      glz::write<glz::opts{.skip_null_members=false}>(obj, buffer);
      expect(buffer == R"({"thing":{"a":3.14,"b":"stuff"},"thing2array":[{"a":3.14,"b":"stuff","c":999.342494903,"d":1E-12,"e":203082348402.1,"f":89.089,"g":12380.00000013,"h":1000000.000001}],"vec3":[3.14,2.7,6.5],"list":[6,7,8,2],"deque":[9,6.7,3.1],"vector":[[9,6.7,3.1],[3.14,2.7,6.5]],"i":8,"d":2,"b":false,"c":"W","v":{"type":"var1_t","x":0},"color":"Green","vb":[true,false,false,true,true,true,true],"sptr":{"a":3.14,"b":"stuff"},"optional":null,"array":["as\"df\\ghjkl","pie","42","foo"],"map":{"a":4,"b":12,"f":7},"mapi":{"2":9.63,"5":3.14,"7":7.42},"thing_ptr":{"a":3.14,"b":"stuff"}})") << buffer;

      expect(nothrow([&] { glz::read_json(obj, buffer); }));

      buffer.clear();
      glz::write_jsonc(obj, buffer);
      expect(buffer == R"({"thing":{"a":3.14/*Test comment 1*/,"b":"stuff"/*Test comment 2*/},"thing2array":[{"a":3.14/*Test comment 1*/,"b":"stuff"/*Test comment 2*/,"c":999.342494903,"d":1E-12,"e":203082348402.1,"f":89.089,"g":12380.00000013,"h":1000000.000001}],"vec3":[3.14,2.7,6.5],"list":[6,7,8,2],"deque":[9,6.7,3.1],"vector":[[9,6.7,3.1],[3.14,2.7,6.5]],"i":8,"d":2/*double is the best type*/,"b":false,"c":"W","v":{"type":"var1_t","x":0},"color":"Green","vb":[true,false,false,true,true,true,true],"sptr":{"a":3.14/*Test comment 1*/,"b":"stuff"/*Test comment 2*/},"array":["as\"df\\ghjkl","pie","42","foo"],"map":{"a":4,"b":12,"f":7},"mapi":{"2":9.63,"5":3.14,"7":7.42},"thing_ptr":{"a":3.14/*Test comment 1*/,"b":"stuff"/*Test comment 2*/}})") << buffer;
      expect(nothrow([&] { glz::read_json(obj, buffer); }));
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
      "type": "var1_t",
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

      //glz::write_json(obj, buffer);
      glz::write<glz::opts{.skip_null_members = false}>(obj, buffer); //Sets sptr to null

      Thing obj2{};
      glz::read_json(obj2, buffer);

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
      expect(obj2.vb ==
             decltype(obj2.vb){false, true, true, false, false, true, true});
      expect(obj2.sptr == nullptr);
      expect(obj2.optional == std::make_optional<V3>(V3{1, 2, 3}));
      expect(obj2.deque == decltype(obj2.deque){0.0, 2.2, 3.9});
      expect(obj2.map == decltype(obj2.map){{"a", 7}, {"f", 3}, {"b", 4}});
      expect(obj2.mapi ==
             decltype(obj2.mapi){{5, 5.0}, {7, 7.1}, {2, 2.22222}});
   };

   "complex user obect member names"_test = [] {
      expect(
         glz::name_v<glz::detail::member_tuple_t<Thing>> == "glz::tuplet::tuple<sub_thing,std::array<sub_thing2,1>,V3,std::list<int32_t>,std::deque<double>,std::vector<V3>,int32_t,double,bool,char,std::variant<var1_t,var2_t>,Color,std::vector<bool>,std::shared_ptr<sub_thing>,std::optional<V3>,std::array<std::string,4>,std::map<std::string,int32_t>,std::map<int32_t,double>,sub_thing*>"
      );
   };
}

void json_pointer() {
   using namespace boost::ut;

   "seek"_test = [] {
      Thing thing{};
      std::any a{};
      glz::seek([&](auto&& val) { a = val; }, thing, "/thing_ptr/a");
      expect(a.has_value() && a.type() == typeid(double) &&
             std::any_cast<double>(a) == thing.thing_ptr->a);
   };

   "seek lambda"_test = [] {
      Thing thing{};
      std::any b{};
      glz::seek([&](auto&& val) { b = val; }, thing, "/thing/b");
      expect(b.has_value() && b.type() == typeid(std::string) &&
             std::any_cast<std::string>(b) == thing.thing.b);
   };

   "get"_test = [] {
      Thing thing{};
      expect(thing.thing.a == glz::get<double>(thing, "/thing_ptr/a"));
      expect(&thing.map["f"] == glz::get_if<int>(thing, "/map/f"));
      expect(&thing.vector == glz::get_if<std::vector<V3>>(thing, "/vector"));
      expect(&thing.vector[1] == glz::get_if<V3>(thing, "/vector/1"));
      expect(thing.vector[1].x == glz::get<double>(thing, "/vector/1/0"));
      expect(thing.thing_ptr == glz::get<sub_thing*>(thing, "/thing_ptr"));

      //Invalid lookup
      expect(throws([&] { glz::get<char>(thing, "/thing_ptr/a"); }));
      expect(nothrow([&] { glz::get_if<char>(thing, "/thing_ptr/a"); }));
      expect(throws([&] { glz::get<double>(thing, "/thing_ptr/c"); }));
      expect(nothrow([&] { glz::get_if<double>(thing, "/thing_ptr/c"); }));
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
      expect(glz::tuplet::get<0>(tuple) == 5.0);
      expect(glz::tuplet::get<1>(tuple) == 42.0);
      expect(glz::tuplet::get<2>(tuple) == "fish");
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
      expect(thing.vec3.x == 7.6 && thing.vec3.y == 1292.1 &&
             thing.vec3.z == 0.333);

      glz::read_as_json(thing, "/vec3/2", "999.9");
      expect(thing.vec3.z == 999.9);
   };

   "valid"_test = [] {
      [[maybe_unused]] constexpr bool is_valid = glz::valid<Thing, "/thing/a", double>(); //Verify constexpr

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
}

void early_end()
{
   using namespace boost::ut;

   "early_end"_test = [] {
      Thing obj{};
      std::string buffer_data =
         R"({"thing":{"a":3.14/*Test comment 1*/,"b":"stuff"/*Test comment 2*/},"thing2array":[{"a":3.14/*Test comment 1*/,"b":"stuff"/*Test comment 2*/,"c":999.342494903,"d":1e-12,"e":203082348402.1,"f":89.089,"g":12380.00000013,"h":1000000.000001}],"vec3":[3.14,2.7,6.5],"list":[6,7,8,2],"deque":[9,6.7,3.1],"vector":[[9,6.7,3.1],[3.14,2.7,6.5]],"i":8,"d":2/*double is the best type*/,"b":false,"c":"W","vb":[true,false,false,true,true,true,true],"sptr":{"a":3.14/*Test comment 1*/,"b":"stuff"/*Test comment 2*/},"optional":null,"array":["as\"df\\ghjkl","pie","42","foo"],"map":{"a":4,"b":12,"f":7},"mapi":{"2":9.63,"5":3.14,"7":7.42},"thing_ptr":{"a":3.14/*Test comment 1*/,"b":"stuff"/*Test comment 2*/}})";
      std::string_view buffer = buffer_data;
      while (buffer.size() > 0)
      {
         buffer_data.pop_back();
         buffer = buffer_data;
         // This is mainly to check if all our end checks are in place. In debug mode it should check if we try to read past the end and abort.
         expect(throws([&] { glz::read_json(obj, buffer); }));
      }
   };
}

void bench()
{
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
      auto duration = std::chrono::duration_cast<std::chrono::duration<double>>(
                         tend - tstart)
                         .count();
      auto mbytes_per_sec = repeat * buffer.size() / (duration * 1048576);
      std::cout << "write_json size: " << buffer.size() << " bytes\n";
      std::cout << "write_json: " << duration << " s, " << mbytes_per_sec
                << " MB/s"
                << "\n";

      tstart = std::chrono::high_resolution_clock::now();
      for (size_t i{}; i < repeat; ++i) {
         glz::read_json(thing, buffer);
      }
      tend = std::chrono::high_resolution_clock::now();
      duration = std::chrono::duration_cast<std::chrono::duration<double>>(
                    tend - tstart)
                    .count();
      mbytes_per_sec = repeat * buffer.size() / (duration * 1048576);
      std::cout << "read_json: " << duration << " s, " << mbytes_per_sec
                     << " MB/s" << "\n";

      tstart = std::chrono::high_resolution_clock::now();
      for (size_t i{}; i < repeat; ++i) {
         glz::get<std::string>(thing, "/thing_ptr/b");
      }
      tend = std::chrono::high_resolution_clock::now();
      duration = std::chrono::duration_cast<std::chrono::duration<double>>(
                    tend - tstart)
                    .count();
      std::cout << "get: " << duration << " s, " << (repeat / duration)
                << " gets/s"
                << "\n\n";
   };
}

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

void read_tests() {
   using namespace boost::ut;
   
   "string read"_test = [] {
      std::string s{"3958713"};
      int i{};
      glz::read_json(i, s);
      expect(i == 3958713);
      
      s.clear();
      s = R"({"v":[0.1, 0.2, 0.3]})";
      oob obj{};
      glz::read_json(obj, s);
      expect(obj.v == v3{ 0.1, 0.2, 0.3 });
   };
   
   "Read floating point types"_test = [] {
      {
         std::string s = "0.96875";
         float f{};
         glz::read_json(f, s);
         expect(f == 0.96875f);
      }
      {
         std::string s = "0.96875";
         double f{};
         glz::read_json(f, s);
         expect(f == 0.96875);
      }
      {
         std::string str = "0.96875";
         std::vector<char> s(str.begin(), str.end());
         s.emplace_back('\0'); // null terminate buffer
         double f{};
         glz::read_json(f, s);
         expect(f == 0.96875);
      }
   };

   "Read integral types"_test = [] {
      {
         std::string s = "true";
         bool v;
         glz::read_json(v, s);
         expect(v);
      }
      {
         std::string s = "1";
         short v;
         glz::read_json(v, s);
         expect(v == 1);
      }
      {
         std::string s = "1";
         int v;
         glz::read_json(v, s);
         expect(v == 1);
      }
      {
         std::string s = "1";
         long v;
         glz::read_json(v, s);
         expect(v == 1);
      }
      {
         std::string s = "1";
         long long v;
         glz::read_json(v, s);
         expect(v == 1);
      }
      {
         std::string s = "1";
         unsigned short v;
         glz::read_json(v, s);
         expect(v == 1);
      }
      {
         std::string s = "1";
         unsigned int v;
         glz::read_json(v, s);
         expect(v == 1);
      }
      {
         std::string s = "1";
         unsigned long v;
         glz::read_json(v, s);
         expect(v == 1);
      }
      {
         std::string s = "1";
         unsigned long long v;
         glz::read_json(v, s);
         expect(v == 1);
      }
   };

   "multiple int from double text"_test = [] {
      std::vector<int> v;
      std::string buffer = "[1.66, 3.24, 5.555]";
      expect(nothrow([&] { glz::read_json(v, buffer); }));
      expect(v.size() == 3);
      expect(v[0] == 1);
      expect(v[1] == 3);
      expect(v[2] == 5);
   };

   "comments"_test = [] {
      {
         std::string b = "1/*a comment*/00";
         int a{};
         glz::read_json(a, b);
         expect(a == 1);
      }
      {
         std::string b = R"([100, // a comment
20])";
         std::vector<int> a{};
         glz::read_json(a, b);
         expect(a[0] == 100);
         expect(a[1] == 20);
      }
   };

   "Failed character read"_test = [] {
      std::string err;
      {
         char b;
         expect(throws([&] { glz::read_json(b, err); }));
      }
   };

   "Read array type"_test = [] {
      std::string in = "    [ 3.25 , 1.125 , 3.0625 ]   ";
      v3 v{};
      glz::read_json(v, in);

      expect(v.x == 3.25);
      expect(v.y == 1.125);
      expect(v.z == 3.0625);
   };

   "Read partial array type"_test = [] {
      {
         std::string in = "    [ 3.25 , null , 3.125 ]   ";
         v3 v{};

         expect(throws([&] { glz::read_json(v, in); }));
      }
      
      // partial reading of fixed sized arrays
      {
         std::string in = "    [ 3.25 , 3.125 ]   ";
         [[maybe_unused]] v3 v{};
         glz::read_json(v, in);

         expect(v.x == 3.25);
         expect(v.y == 3.125);
         expect(v.z == 0.0);
      }
   };

   "Read object type"_test = [] {
      std::string in =
         R"(    { "v" :  [ 3.25 , 1.125 , 3.0625 ]   , "n" : 5 } )";
      oob oob{};
      glz::read_json(oob, in);

      expect(oob.v.x == 3.25);
      expect(oob.v.y == 1.125);
      expect(oob.v.z == 3.0625);
      expect(oob.n == 5);
   };

   "Read partial object type"_test = [] {
      std::string in =
         R"(    { "v" :  [ 3.25 , null , 3.0625 ]   , "n" : null } )";
      oob oob{};

      expect(throws([&] { glz::read_json(oob, in); }));
   };

   "Reversed object"_test = [] {
      std::string in =
         R"(    {  "n" : 5   ,  "v" :  [ 3.25 , 1.125 , 3.0625 ] } )";
      oob oob{};
      glz::read_json(oob, in);

      expect(oob.v.x == 3.25);
      expect(oob.v.y == 1.125);
      expect(oob.v.z == 3.0625);
      expect(oob.n == 5);
   };

   "Read list"_test = [] {
      std::string in = "[1, 2, 3, 4]";
      std::list<int> l, lr{1, 2, 3, 4};
      glz::read_json(l, in);

      expect(l == lr);
   };

   "Read forward list"_test = [] {
      std::string in = "[1, 2, 3, 4]";
      std::forward_list<int> l, lr{1, 2, 3, 4};
      glz::read_json(l, in);

      expect(l == lr);
   };

   "Read deque"_test = [] {
      {
         std::string in = "[1, 2, 3, 4]";
         std::deque<int> l, lr{1, 2, 3, 4};
         glz::read_json(l, in);

         expect(l == lr);
      }
      {
         std::string in = "[1, 2, 3, 4]";
         std::deque<int> l{8, 9}, lr{1, 2, 3, 4};
         glz::read_json(l, in);

         expect(l == lr);
      }
   };

   "Read into returned data"_test = [] {
      const std::string s = "[1, 2, 3, 4, 5, 6]";
      const std::vector<int> v{1, 2, 3, 4, 5, 6};
      std::vector<int> vr;
      glz::read_json(vr, s);
      expect(vr == v);
   };

   "Read array"_test = [] {
      {
         std::string in = R"(    [1, 5, 232, 75, 123, 54, 89] )";
         std::array<int, 7> v1{}, v2{99}, v3{99, 99, 99, 99, 99},
            vr{1, 5, 232, 75, 123, 54, 89};
         glz::read_json(v1, in);
         glz::read_json(v2, in);
         glz::read_json(v3, in);
         expect(v1 == vr);
         expect(v2 == vr);
         expect(v3 == vr);
      }
   };

   "Read vector"_test = [] {
      {
         std::string in = R"(    [1, 5, 232, 75, 123, 54, 89] )";
         std::vector<int> v, vr{1, 5, 232, 75, 123, 54, 89};
         glz::read_json(v, in);

         expect(v == vr);
      }
      {
         std::string in = R"([true, false, true, false])";
         std::vector<bool> v, vr{true, false, true, false};
         glz::read_json(v, in);

         expect(v == vr);
      }
      {
         std::string in = R"(    [1, 5, 232, 75, 123, 54, 89] )";
         std::vector<int> v{1, 2, 3, 4}, vr{1, 5, 232, 75, 123, 54, 89};
         glz::read_json(v, in);

         expect(v == vr);
      }
      {
         std::string in = R"(    [1, 5, 232, 75, 123, 54, 89] )";
         std::vector<int> v{1, 2, 3, 4, 5, 6, 7, 8, 9, 10},
            vr{1, 5, 232, 75, 123, 54, 89};
         glz::read_json(v, in);

         expect(v == vr);
      }
   };

   "Read partial vector"_test = [] {
      std::string in = R"(    [1, 5, 232, 75, null, 54, 89] )";
      std::vector<int> v, vr{1, 5, 232, 75, 0, 54, 89};
      
      expect(throws([&] { glz::read_json(v, in); }));
   };

//*  ISSUE UT cannot run this test
   "Read map"_test = [] {
      {
         std::string in = R"(   { "as" : 1, "so" : 2, "make" : 3 } )";
         std::map<std::string, int> v, vr{{"as", 1}, {"so", 2}, {"make", 3}};
         glz::read_json(v, in);
         
         //expect(v == vr);
      }
      {
         std::string in = R"(   { "as" : 1, "so" : 2, "make" : 3 } )";
         std::map<std::string, int> v{{"as", -1}, {"make", 10000}},
            vr{{"as", 1}, {"so", 2}, {"make", 3}};
         glz::read_json(v, in);

         //expect(v == vr);
      }
   };
   
   "Read partial map"_test = [] {
      std::string in = R"(   { "as" : 1, "so" : null, "make" : 3 } )";
      std::map<std::string, int> v, vr{{"as", 1}, {"so", 0}, {"make", 3}};

      expect(throws([&] { glz::read_json(v, in); }));
   };

   "Read boolean"_test = [] {
      {
         std::string in = R"(true)";
         bool res{};
         glz::read_json(res, in);

         expect(res == true);
      }
      {
         std::string in = R"(false)";
         bool res{true};
         glz::read_json(res, in);

         expect(res == false);
      }
      {
         std::string in = R"(null)";
         bool res{false};
         
         expect(throws([&] {glz::read_json(res, in); }));
      }
   };

   "Read integer"_test = [] {
      {
         std::string in = R"(-1224125asdasf)";
         int res{};
         glz::read_json(res, in);

         expect(res == -1224125);
      }
      {
         std::string in = R"(null)";
         int res{};
         
         expect(throws([&] { glz::read_json(res, in); }));
      }
   };

   "Read double"_test = [] {
      {
         std::string in = R"(0.072265625flkka)";
         double res{};
         glz::read_json(res, in);
         expect(res == 0.072265625);
      }
      {
         std::string in = R"(1e5das)";
         double res{};
         glz::read_json(res, in);
         expect(res == 1e5);
      }
      {
         std::string in = R"(-0)";
         double res{};
         glz::read_json(res, in);
         expect(res == -0.0);
      }
      {
         std::string in = R"(0e5)";
         double res{};
         glz::read_json(res, in);
         expect(res == 0.0);
      }
      {
         std::string in = R"(0)";
         double res{};
         glz::read_json(res, in);
         expect(res == 0.0);
      }
      {
         std::string in = R"(11)";
         double res{};
         glz::read_json(res, in);
         expect(res == 11.0);
      }
      {
         std::string in = R"(0a)";
         double res{};
         glz::read_json(res, in);
         expect(res == 0.0);
      }
      {
         std::string in = R"(11.0)";
         double res{};
         glz::read_json(res, in);
         expect(res == 11.0);
      }
      {
         std::string in = R"(11e5)";
         double res{};
         glz::read_json(res, in);
         expect(res == 11.0e5);
      }
      {
         std::string in = R"(null)";
         double res{};
         
         expect(throws([&] {glz::read_json(res, in); }));
      }
      {
         std::string res = R"(success)";
         double d;
         expect(throws([&] {glz::read_json(d, res); }));
      }
      {
         std::string res = R"(-success)";
         double d;
         expect(throws([&] {glz::read_json(d, res); }));
      }
      {
         std::string res = R"(1.a)";
         double d;
         
         expect(nothrow([&] {glz::read_json(d, res); }));
      }
      {
         std::string res = R"()";
         double d;
         expect(throws([&] {glz::read_json(d, res); }));
      }
      {
         std::string res = R"(-)";
         double d;
         expect(throws([&] {glz::read_json(d, res); }));
      }
      {
         std::string res = R"(1.)";
         double d;

         expect(nothrow([&] { glz::read_json(d, res); }));
      }
      {
         std::string res = R"(1.0e)";
         double d;

         expect(nothrow([&] { glz::read_json(d, res); }));
      }
      {
         std::string res = R"(1.0e-)";
         double d;

         expect(nothrow([&] { glz::read_json(d, res); }));
      }
   };

   "Read string"_test = [] {
      std::string in_nothrow = R"("asljl{}121231212441[]123::,,;,;,,::,Q~123\\a13dqwdwqwq")";
      std::string res{};

      glz::read_json(res, in_nothrow);
      expect(res == "asljl{}121231212441[]123::,,;,;,,::,Q~123\\a13dqwdwqwq");

      std::string in_throw = R"("asljl{}121231212441[]123::,,;,;,,::,Q~123\a13dqwdwqwq")";
      res.clear();

      expect(throws([&] { glz::read_json(res, in_throw); }));
   };

   "Nested array"_test = [] {
      std::vector<v3> v;
      std::string buf =
         R"([[1.000000,0.000000,3.000000],[2.000000,0.000000,0.000000]])";

      glz::read_json(v, buf);
      expect(v[0].x == 1.0);
      expect(v[0].z == 3.0);
      expect(v[1].x == 2.0);
   };

   "Nested map"_test = [] {
      std::map<std::string, v3> m;
      std::string buf =
         R"({"1":[4.000000,0.000000,0.000000],"2":[5.000000,0.000000,0.000000]})";

      glz::read_json(m, buf);
      expect(m["1"].x == 4.0);
      expect(m["2"].x == 5.0);
   };

   "Nested map 2"_test = [] {
      std::map<std::string, std::vector<double>> m;
      std::string buf =
         R"({"1":[4.000000,0.000000,0.000000],"2":[5.000000,0.000000,0.000000,4.000000]})";

      glz::read_json(m, buf);
      expect(m["1"][0] == 4.0);
      expect(m["2"][0] == 5.0);
      expect(m["2"][3] == 4.0);
   };

   "Integer keyed map"_test = [] {
      std::map<int, std::vector<double>> m;
      std::string buf =
         R"({"1":[4.000000,0.000000,0.000000],"2":[5.000000,0.000000,0.000000,4.000000]})";

      glz::read_json(m, buf);
      expect(m[1][0] == 4.0);
      expect(m[2][0] == 5.0);
      expect(m[2][3] == 4.0);
   };
}

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
   static constexpr auto
      glaze =
      glz::object("name", &n::name, "value", &n::value);
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

void write_tests() {
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
         //Is this what we want instead?
         expect(s == R"("a")");  // std::to_string(static_cast<int>('a')));
      }
      {
         std::string s;
         wchar_t v{'a'};
         glz::write_json(v, s); //This line gives warning about converting wchar to char, is that fine? Should we write a to_buffer template to handle type wchar?
         // Is the below what we actually expect?
         expect(s == R"("a")");  // std::to_string(static_cast<int>('a')));
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
         long long v{1};
         glz::write_json(v, s);
         expect(s == "1");
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
         unsigned long long v{1};
         glz::write_json(v, s);
         expect(s == "1");
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

//* Empty object not allowed
   "Write empty object structure"_test = [] {
      EmptyObject e;
      std::string buf;
      glz::write_json(e, buf);
      //expect(buf == R"({})");
   };

   "Write c-string"_test = [] {
      std::string s = "aasdf";
      char *c = s.data();
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
      ;
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
      std::array<double, 4> v{1.1, 2.2, 3.3, 4.4};
      std::string s;
      glz::write_json(v, s);

      expect(s == "[1.1,2.2,3.3,4.4]");
   };

   "Write map"_test = [] {
      std::map<std::string, double> m{{"a", 2.2}, {"b", 11.111}, {"c", 211.2}};
      std::string s;
      glz::write_json(m, s);

      expect(s == R"({"a":2.2,"b":11.111,"c":211.2})");
   };

   "Write integer map"_test = [] {
      std::map<int, double> m{{3, 2.2}, {5, 211.2}, {7, 11.111}};
      std::string s;
      glz::write_json(m, s);

      expect(s == R"({"3":2.2,"5":211.2,"7":11.111})");
   };

//* TODO: Gives 23 errors. Errors come from an MSVC include file "utility": it claims that the base class is undifined.
   "Write object"_test = [] {
      Named n{"Hello, world!", {{{21, 15, 13}, 0}, {0}}};

      std::string s;
      s.reserve(1000);
      [[maybe_unused]] auto i = std::back_inserter(s);
      //glaze::write_json(n, s);

      //expect(
         //s ==
         //R"({"name":"Hello, world!","value":[{"geo":[21,15,13],"int":0},[0,0,0]]})");
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
}

suite error_outputs = [] {
   "invalid character"_test = [] {
      try
      {
         std::string s = R"({"Hello":"World"x, "color": "red"})";
         std::map<std::string, std::string> m;
         glz::read_json(m, s);
      }
      catch (const std::exception& e) {
         expect(std::string(e.what()) ==
                "1:17: Expected:,\n   {\"Hello\":\"World\"x, \"color\": \"red\"}\n                   ^\n");
      }
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

void study_tests()
{
   "study"_test = [] {
      glz::study::design design;
      design.params = { {.ptr = "/x", .distribution = "linspace", .range = { "0", "1", "10" } } };
      
      glz::study::full_factorial generator{ study_obj{}, design };
      
      std::vector<size_t> results;
      std::mutex mtx{};
      glz::study::run_study(generator, [&](const auto& point, [[maybe_unused]] const auto job_num){
         std::unique_lock lock{mtx};
         results.emplace_back(point.x);
      });
      
      std::sort(results.begin(), results.end());
      
      expect(results[0] == 0);
      expect(results[10] == 10);
   };
   
   "doe"_test = [] {
      glz::study::design design;
      design.params = { { "/x", "linspace", { "0", "1", "3" } },
         { "/y", "linspace", { "0", "1", "2" } }
      };
      
      glz::study::full_factorial g{ study_obj{}, design };
      
      std::vector<std::string> results;
      for (size_t i = 0; i < g.size(); ++i) {
         const auto point = g.generate(i);
         results.emplace_back(std::to_string(point.x) + "|" + std::to_string(point.y));
      }
      
      std::sort(results.begin(), results.end());
      
      std::vector<std::string> results2;
      std::mutex mtx{};
      glz::study::run_study(g, [&](const auto& point, [[maybe_unused]] const auto job_num){
         std::unique_lock lock{mtx};
         results2.emplace_back(std::to_string(point.x) + "|" + std::to_string(point.y));
      });
      
      std::sort(results2.begin(), results2.end());
      
      expect(results == results2);
   };
}

suite thread_pool = [] {
   "thread pool"_test = [] {
      glz::pool pool(2);
      
      std::atomic<int> x = 0;
      
      auto f = [&](auto thread_number) {
         ++x;
      };
      
      for (auto i = 0; i < 1000; ++i) {
         pool.emplace_back(f);
      }
      
      pool.wait();
      
      expect(x == 1000);
   };
   
   "thread pool no thread number"_test = [] {
      glz::pool pool(4);
      
      std::atomic<int> x = 0;
      
      auto f = [&] {
         ++x;
      };
      
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
         std::uniform_int_distribution<size_t> dist{ 0, 100 };
         
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
   };
};

suite raw_json_tests = [] {
   "round_trip_raw_json"_test = [] {
      std::vector<glz::raw_json> v{ "0", "1", "2" };
      std::string s;
      glz::write_json(v, s);
      expect(s == R"([0,1,2])");
      expect(nothrow([&] {
         glz::read_json(v, s);
      }));
   };
};

suite json_helpers = [] {
   "json_helpers"_test = [] {
      my_struct v{};
      auto json = glz::write_json(v);
      expect(json == R"({"i":287,"d":3.14,"hello":"Hello World","arr":[1,2,3]})");
      
      expect(nothrow([&] {
         v = glz::read_json<my_struct>(json);
      }));
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
   "nan_tests"_test = [] {
      double d = NAN;
      std::string s{};
      glz::write_json(d, s);
      // TODO: this output is -nan for MSVC and nan for clang
      expect(s == "nan" || s == "-nan");
      
      d = 0.0;
      glz::read_json(d, s);
      expect(std::isnan(d));
   };
};

struct variant_custom
{
   std::variant<var1_t, var2_t> v{};
};

template <>
struct glz::meta<variant_custom>
{
   static constexpr std::string_view name = "variant_custom";
   using T = variant_custom;
   static constexpr auto value = object("v", &T::v);
};

suite custom_variant_tests = [] {
   "custom_variant_write_tests"_test = [] {
      variant_custom obj{};
      obj.v = var1_t{ 5.5 };
      
      std::string s{};
      
      glz::write_json(obj, s);
      
      expect(s == R"({"v":{"type":"var1_t","x":5.5}})");
   };
   
   "custom_variant_read_tests"_test = [] {
      variant_custom obj{};
      
      glz::read_json(obj, R"({"v": { "type": "var1_t", "x": 5.5 }})");
      
      expect(std::get<var1_t>(obj.v).x == 5.5);
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
      expect(s == R"("std::monostate")") << s;
   };
   
   "variant_read_"_test = [] {
      std::variant<int32_t, double> x = 44;
      glz::read_json(x, "33");
      expect(std::get<int32_t>(x) == 33);
   };

   "variant_read_auto"_test = [] {
      // Auto deduce variant with no conflicting basic types
      std::variant<int, std::string, bool, std::map<std::string, double>, std::vector<std::string>> m{};
      glz::read_json(m, R"("Hello World")");
      expect(std::holds_alternative<std::string>(m) == true);
      expect(std::get<std::string>(m) == "Hello World");

      glz::read_json(m, R"(872)");
      expect(std::holds_alternative<int>(m) == true);
      expect(std::get<int>(m) == 872);

      glz::read_json(m, R"({"pi":3.14})");
      expect(std::holds_alternative<std::map<std::string, double>>(m) == true);
      expect(std::get<std::map<std::string, double>>(m)["pi"] == 3.14);

      glz::read_json(m, R"(true)");
      expect(std::holds_alternative<bool>(m) == true);
      expect(std::get<bool>(m) == true);

      glz::read_json(m, R"(["a", "b", "c"])");
      expect(std::holds_alternative<std::vector<std::string>>(m) == true);
      expect(std::get<std::vector<std::string>>(m)[1] == "b");
   };
   
   "variant_read_obj"_test = [] {
      variant_obj obj{};
      
      obj.v = double{};
      glz::read_json(obj, R"({"v": 5.5})");
      
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
};

suite generic_json_tests = [] {
   "generic_json_write"_test = [] {
      glz::json_t json = {
         {"pi", 3.141},
         {"happy", true},
         {"name", "Niels"},
         {"nothing", nullptr},
         {"answer", {{"everything", 42.0}}},
         {"list", {1.0, 0.0, 2.0}},
         {"object", {
            {"currency", "USD"},
            {"value", 42.99}
         }}
      };
      std::string buffer{};
      glz::write_json(json, buffer);
      expect(buffer == R"({"answer":{"everything":42},"happy":true,"list":[1,0,2],"name":"Niels","object":{"currency":"USD","value":42.99},"pi":3.141})") << buffer;
   };

   "generic_json_read"_test = [] {
      glz::json_t json{};
      std::string buffer = R"([5,"Hello World",{"pi":3.14}])";
      glz::read_json(json, buffer);
      expect(json[0].get<double>() == 5.0);
      expect(json[1].get<std::string>() == "Hello World");
      expect(json[2]["pi"].get<double>() == 3.14);
   };
};

struct holder0_t {
   int i{};
};

template <>
struct glz::meta<holder0_t>
{
   static constexpr std::string_view name = "holder0_t";
   using T = holder0_t;
   static constexpr auto value = object("i", &T::i);
};

struct holder1_t {
   holder0_t a{};
};

template <>
struct glz::meta<holder1_t>
{
   static constexpr std::string_view name = "holder1_t";
   using T = holder1_t;
   static constexpr auto value = object("a", &T::a);
};

struct holder2_t {
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
      expect(nothrow([&] {
         glz::read_json(arr, s);
      }));
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

void file_include_test()
{
   includer_struct obj{};
   
   glz::write_file_json(obj, "../alabastar.json");
   
   obj.str = "";
   
   std::string s = R"({"#include": "../alabastar.json", "i": 100})";
   glz::read_json(obj, s);
   
   expect(obj.str == "Hello") << obj.str;
   expect(obj.i == 100) << obj.i;
   
   obj.str = "";
   
   glz::read_file_json(obj, "../alabastar.json");
   expect(obj.str == "Hello") << obj.str;
   expect(obj.i == 55) << obj.i;
}

void file_include_test_auto()
{
   includer_struct obj{};
   
   glz::write_file(obj, "./auto.json");
   
   obj.str = "";
   
   std::string s = R"({"#include": "./auto.json", "i": 100})";
   glz::read_json(obj, s);
   
   expect(obj.str == "Hello") << obj.str;
   expect(obj.i == 100) << obj.i;
   
   obj.str = "";
   
   glz::read_file(obj, "./auto.json");
   expect(obj.str == "Hello") << obj.str;
   expect(obj.i == 55) << obj.i;
}

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

void nested_file_include_test()
{
   nested0 obj;
   
   std::string a = R"({"#include": "../b/b.json"})";
   {
      std::filesystem::create_directory("a");
      std::ofstream a_file{ "./a/a.json" };
      
      a_file << a;
   }
   
   {
      std::filesystem::create_directory("b");
      
      obj.b.i = 13;
      
      glz::write_file_json(obj.b, "./b/b.json");
   }
   
   obj.b.i = 0;
   
   std::string s = R"({ "a": { "#include": "./a/a.json" }, "b": { "#include": "./b/b.json" } })";
   
   glz::read_json(obj, s);
   
   expect(obj.a.i == 13);
}

suite shrink_to_fit = [] {
   "shrink_to_fit"_test = [] {
      std::vector<int> v = { 1, 2, 3, 4, 5, 6 };
      std::string b = R"([1,2,3])";
      glz::read_json(v, b);
      
      expect(v.size() == 3);
      expect(v.capacity() > 3);
      
      v = { 1, 2, 3, 4, 5, 6 };
      
      glz::read<glz::opts{.shrink_to_fit = true}>(v, b);
      expect(v.size() == 3);
      expect(v.capacity() == 3);
   };
};

suite recorder_test = [] {
   "recorder_to_file"_test = [] {
      
      glz::recorder<double, float> rec;
      
      double x = 0.0;
      float y = 0.f;
      
      rec["x"] = x;
      rec["y"] = y;
      
      for (int i = 0; i < 100; ++i)
      {
         x += 1.5;
         y += static_cast<float>(i);
         rec.update();
      }
      
      std::string s{};
      glz::write_json(rec, s);
      
      glz::read_json(rec, s);
      
      glz::write_file_json(rec, "recorder_out.json");
   };
};

suite reference_wrapper_test = [] {
   "reference_wrapper"_test = [] {
      
      int x = 55;
      std::reference_wrapper<int> ref = x;
      
      std::string s = glz::write_json(ref);
      
      expect(s == "55");
      
      glz::read_json(ref, R"(66)");
      expect(x == 66);
   };
};

suite small_chars = [] {
   "small_chars"_test = [] {
      
      uint8_t x = 5;
      std::string s = glz::write_json(x);
      
      expect(s == "5");
      
      glz::read_json(x, "10");
      expect(x == 10);
   };
};

suite char16_test = [] {
   "char16_test"_test = [] {
      
      {
         char16_t c{};
         glz::read_json(c, R"("H")");
         
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
      
      std::vector<std::string> x = { "Hello", "World", "Ice", "Cream" };
      std::string s = glz::write_ndjson(x);
      
      expect(s ==
R"("Hello"
"World"
"Ice"
"Cream")");
      
      x.clear();
      
      glz::read_ndjson(x, s);
      expect(x[0] == "Hello");
      expect(x[1] == "World");
      expect(x[2] == "Ice");
      expect(x[3] == "Cream");
   };
   
   "ndjson_list"_test = [] {
      
      std::list<std::string> x = { "Hello", "World", "Ice", "Cream" };
      std::string s = glz::write_ndjson(x);
      
      expect(s ==
R"("Hello"
"World"
"Ice"
"Cream")");
      
      x.clear();
      
      glz::read_ndjson(x, s);
      auto it = x.begin();
      expect(*it == "Hello"); ++it;
      expect(*it == "World"); ++it;
      expect(*it == "Ice"); ++it;
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
      
      glz::read_ndjson(x, s);
      
      expect(first.hello == "Hello World");
      expect(first.arr[0] = 1);
      expect(second.a == 3.14);
      expect(second.b == "stuff");
   };
};

suite std_function_handling = []
{
   "std_function"_test = [] {
      
      int x = 1;
      std::function<void()> increment = [&]{ ++x; };
      std::string s{};
      glz::write_json(increment, s);
      
      expect(s == R"("std::function<void()>")") << s;
      
      expect(nothrow([&] { glz::read_json(increment, s); }));
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
   static constexpr auto value = object("i", &T::i,  //
                                        "d", &T::d, //
                                        "hello", hide{&T::hello}
                                        );
};

suite hide_tests = []
{
   "hide_write"_test = [] {
      hide_struct s{};
      std::string b{};
      glz::write_json(s, b);
      
      expect(b == R"({"i":287,"d":3.14})");
   };
   
   "hide_read"_test = [] {
      
      std::string b = R"({"i":287,"d":3.14,"hello":"Hello World"})";
      
      hide_struct s{};
      
      expect(throws([&]{ glz::read_json(s, b); }));
   };
};

struct mem_f_struct
{
   int i = 0;
   int& access() {
      return i;
   }
};

template <>
struct glz::meta<mem_f_struct>
{
   static constexpr std::string_view name = "mem_f_struct";
   using T = mem_f_struct;
   static constexpr auto value = object("i", &T::i,  //
                                        "access", &T::access
                                        );
};

suite member_function_tests = []
{
   "member_function2"_test = []
   {
      mem_f_struct s{};
      
      using T = mem_f_struct;
      auto& i = glz::call<int&>(s, "/access");
      ++i;
      
      expect(s.i == 1);
   };
};

struct dog
{
   int age{};
   void eat() {
      ++age;
   };
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
   void eat() {
      ++age;
   };
   
   void purr() {
      
   }
};

template <>
struct glz::meta<cat>
{
   static constexpr std::string_view name = "cat";
   using T = cat;
   static constexpr auto value = object("age", &T::age, "eat", &T::eat, "purr", &T::purr);
};

struct person {
   void eat(const std::string& thing) {};
};

template <> struct glz::meta<person> {
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
   std::string string(const std::string_view s, const int y) {
      return std::string(s) + ":" + std::to_string(y);
   }
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
   std::string string(const std::string_view s, const int y) {
      return "";
   }
};

template <>
struct glz::meta<string_t>
{
   static constexpr std::string_view name = "string_t";
   using T = string_t;
   static constexpr auto value = object("string", &T::string);
};

suite poly_tests = []
{
   "poly"_test = []
   {
      std::array<glz::poly<animal>, 2> a{ dog{}, cat{} };
      
      a[0].call<"eat">();
      a[1].call<"eat">();
      
      expect(a[0].get<"age">() == 1);
   };
   
   "poly person"_test = []
   {
      //std::array<glz::poly<animal>, 2> a{ dog{}, person{} };
      // This should static_assert
   };
   
   "poly pointer"_test = []
   {
      dog d{};
      glz::poly<animal> a{ &d };
      
      a.call<"eat">();
      
      expect(d.age == 1);
      expect(&a.get<"age">() == &d.age);
   };
   
   "complex_function"_test = []
   {
      glz::poly<string_t> p{ complex_function_call_t{} };
      
      expect(p.call<"string">("x", 5) == "x:5");
   };
};

suite any_tests = []
{
   "any"_test = []
   {
      glz::any a = 5.5;
      
      expect(glz::any_cast<double>(a) == 5.5);
      
      auto* data = a.data();
      *static_cast<double*>(data) = 6.6;
      
      expect(glz::any_cast<double>(a) == 6.6);
      expect(throws([&]{ glz::any_cast<int>(a); }));
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
struct glz::meta<fixed_object_t> {
   static constexpr std::string_view name = "fixed_object_t";
   using T = fixed_object_t;
   static constexpr auto value = object(
      "int_array", &T::int_array,
      "float_array", &T::float_array,
      "double_array", &T::double_array
   );
};

template <>
struct glz::meta<fixed_name_object_t> {
   static constexpr std::string_view name = "fixed_name_object_t";
   using T = fixed_name_object_t;
   static constexpr auto value = object(
      "name0", &T::name0,
      "name1", &T::name1,
      "name2", &T::name2,
      "name3", &T::name3,
      "name4", &T::name4
   );
};

template <>
struct glz::meta<nested_object_t> {
   static constexpr std::string_view name = "nested_object_t";
   using T = nested_object_t;
   static constexpr auto value = object(
      "v3s", &T::v3s,
      "id", &T::id
   );
};

template <>
struct glz::meta<another_object_t> {
   static constexpr std::string_view name = "another_object_t";
   using T = another_object_t;
   static constexpr auto value = object(
      "string", &T::string,
      "another_string", &T::another_string,
      "boolean", &T::boolean,
      "nested_object", &T::nested_object
   );
};

template <>
struct glz::meta<obj_t> {
   static constexpr std::string_view name = "obj_t";
   using T = obj_t;
   static constexpr auto value = object(
      "fixed_object", &T::fixed_object,
      "fixed_name_object", &T::fixed_name_object,
      "another_object", &T::another_object,
      "string_array", &T::string_array,
      "string", &T::string,
      "number", &T::number,
      "boolean", &T::boolean,
      "another_bool", &T::another_bool
   );
};

suite json_performance = []
{
   "json performance"_test = [] {
      std::string buffer{ json0 };
      
      obj_t obj{};
      
      glz::read_json(obj, buffer);
      buffer.clear();
      
      glz::write_json(obj, buffer);
      
      expect(buffer == R"({"fixed_object":{"int_array":[0,1,2,3,4,5,6],"float_array":[0.1,0.2,0.3,0.4,0.5,0.6],"double_array":[3288398.238,2.33E24,28.9,0.928759872,0.22222848,0.1,0.2,0.3,0.4]},"fixed_name_object":{"name0":"James","name1":"Abraham","name2":"Susan","name3":"Frank","name4":"Alicia"},"another_object":{"string":"here is some text","another_string":"Hello World","boolean":false,"nested_object":{"v3s":[[0.12345,0.23456,0.001345],[0.3894675,97.39827,297.92387],[18.18,87.289,2988.298]],"id":"298728949872"}},"string_array":["Cat","Dog","Elephant","Tiger"],"string":"Hello world","number":3.14,"boolean":true,"another_bool":false})") << buffer;
   };
};

suite json_schema = [] {
   "json schema"_test = [] {
      Thing obj{};
      std::string schema = glz::write_json_schema<Thing>();
      //Note: Check schema and sample output against a json schema validator like https://www.jsonschemavalidator.net/ when you update this string
      expect(schema == R"({"type":["object"],"properties":{"array":{"$ref":"#/$defs/std::array<std::string,4>"},"b":{"$ref":"#/$defs/bool"},"c":{"$ref":"#/$defs/char"},"color":{"$ref":"#/$defs/Color"},"d":{"$ref":"#/$defs/double","description":"double is the best type"},"deque":{"$ref":"#/$defs/std::deque<double>"},"i":{"$ref":"#/$defs/int32_t"},"list":{"$ref":"#/$defs/std::list<int32_t>"},"map":{"$ref":"#/$defs/std::map<std::string,int32_t>"},"mapi":{"$ref":"#/$defs/std::map<int32_t,double>"},"optional":{"$ref":"#/$defs/std::optional<V3>"},"sptr":{"$ref":"#/$defs/std::shared_ptr<sub_thing>"},"thing":{"$ref":"#/$defs/sub_thing"},"thing2array":{"$ref":"#/$defs/std::array<sub_thing2,1>"},"thing_ptr":{"$ref":"#/$defs/sub_thing*"},"v":{"$ref":"#/$defs/std::variant<var1_t,var2_t>"},"vb":{"$ref":"#/$defs/std::vector<bool>"},"vec3":{"$ref":"#/$defs/V3"},"vector":{"$ref":"#/$defs/std::vector<V3>"}},"additionalProperties":false,"$defs":{"Color":{"type":["string"],"oneOf":[{"const":"Red"},{"const":"Green"},{"const":"Blue"}]},"V3":{"type":["array"]},"bool":{"type":["boolean"]},"char":{"type":["string"]},"double":{"type":["number"]},"float":{"type":["number"]},"int32_t":{"type":["integer"]},"std::array<std::string,4>":{"type":["array"],"items":{"$ref":"#/$defs/std::string"}},"std::array<sub_thing2,1>":{"type":["array"],"items":{"$ref":"#/$defs/sub_thing2"}},"std::deque<double>":{"type":["array"],"items":{"$ref":"#/$defs/double"}},"std::list<int32_t>":{"type":["array"],"items":{"$ref":"#/$defs/int32_t"}},"std::map<int32_t,double>":{"type":["object"],"additionalProperties":{"$ref":"#/$defs/double"}},"std::map<std::string,int32_t>":{"type":["object"],"additionalProperties":{"$ref":"#/$defs/int32_t"}},"std::optional<V3>":{"type":["array","null"]},"std::shared_ptr<sub_thing>":{"type":["object","null"],"properties":{"a":{"$ref":"#/$defs/double","description":"Test comment 1"},"b":{"$ref":"#/$defs/std::string","description":"Test comment 2"}},"additionalProperties":false},"std::string":{"type":["string"]},"std::variant<var1_t,var2_t>":{"type":["number","string","boolean","object","array","null"],"oneOf":[{"type":["object"],"properties":{"type":{"$ref":"#/$defs/std::string"},"x":{"$ref":"#/$defs/double"}},"additionalProperties":false},{"type":["object"],"properties":{"type":{"$ref":"#/$defs/std::string"},"y":{"$ref":"#/$defs/double"}},"additionalProperties":false}]},"std::vector<V3>":{"type":["array"],"items":{"$ref":"#/$defs/V3"}},"std::vector<bool>":{"type":["array"],"items":{"$ref":"#/$defs/bool"}},"sub_thing":{"type":["object"],"properties":{"a":{"$ref":"#/$defs/double","description":"Test comment 1"},"b":{"$ref":"#/$defs/std::string","description":"Test comment 2"}},"additionalProperties":false},"sub_thing*":{"type":["object","null"],"properties":{"a":{"$ref":"#/$defs/double","description":"Test comment 1"},"b":{"$ref":"#/$defs/std::string","description":"Test comment 2"}},"additionalProperties":false},"sub_thing2":{"type":["object"],"properties":{"a":{"$ref":"#/$defs/double","description":"Test comment 1"},"b":{"$ref":"#/$defs/std::string","description":"Test comment 2"},"c":{"$ref":"#/$defs/double"},"d":{"$ref":"#/$defs/double"},"e":{"$ref":"#/$defs/double"},"f":{"$ref":"#/$defs/float"},"g":{"$ref":"#/$defs/double"},"h":{"$ref":"#/$defs/double"}},"additionalProperties":false}}})");
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

suite date_test = []
{
   "date"_test = [] {
      date d{};
      d.data = 55;
      
      std::string s{};
      glz::write_json(d, s);
      
      expect(s == R"("55")");
      
      d.data = 0;
      glz::read_json(d, s);
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

suite unicode_tests = []
{
   "unicode"_test = [] {
      std::string str = "😀😃😄🍌💐🌹🥀🌺🌷🌸💮🏵️🌻🌼";
      
      std::string buffer{};
      glz::write_json(str, buffer);
      
      str.clear();
      glz::read_json(str, buffer);
      
      expect(str == "😀😃😄🍌💐🌹🥀🌺🌷🌸💮🏵️🌻🌼");
   };
   
   "unicode_unescaped_smile"_test = [] {
      std::string str = R"({"😀":"smile"})";
      unicode_keys_t obj{};
      glz::read_json(obj, str);
      
      expect(obj.happy == "smile");
   };
   
   "unicode_escaped_smile"_test = [] {
      // more than 4 characters in unicode is not valid JSON
      std::string str = R"({"\u1F600":"smile"})";
      unicode_keys_t obj{};
      expect(throws([&]{ glz::read_json(obj, str); }));
   };
   
   "unicode_unescaped"_test = [] {
      std::string str = R"({"ᇿ":"ᇿ"})";
      question_t obj{};
      glz::read_json(obj, str);
      
      expect(obj.text == "ᇿ");
   };
   
   "unicode_escaped"_test = [] {
      std::string str = R"({"\u11FF":"\u11FF"})";
      question_t obj{};
      glz::read_json(obj, str);
      
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
   static constexpr auto value{ &T::x };
};

struct lambda_value_t
{
   int x{};
};

template <>
struct glz::meta<lambda_value_t>
{
   static constexpr auto value = [](auto& self) -> auto& { return self.x; };
};

suite value_test = []
{
   "value"_test = [] {
      std::string s = "5";
      value_t v{};
      
      glz::read_json(v, s);
      expect(v.x == 5);
      
      s.clear();
      glz::write_json(v, s);
      expect(s == "5");
   };
   
   "lambda value"_test = [] {
      std::string s = "5";
      lambda_value_t v{};
      
      glz::read_json(v, s);
      expect(v.x == 5);
      
      s.clear();
      glz::write_json(v, s);
      expect(s == "5");
   };
};

struct TestMsg {
   uint64_t id{};
   std::string val{};
};

template <>
struct glz::meta<TestMsg> {
    static constexpr std::string_view name = "TestMsg";
    using T = TestMsg;
    static constexpr auto value = object(
            "id", &T::id,
            "val", &T::val
    );
};

suite byte_buffer = []
{
   "uint8_t buffer"_test = []
   {
      TestMsg msg{};
      msg.id = 5;
      msg.val = "hello";
      std::vector<uint8_t> buffer{};
      glz::write_json(msg, buffer);
      
      buffer.emplace_back('\0');
      
      msg.id = 0;
      msg.val = "";
      
      glz::read_json(msg, buffer);
      expect(msg.id == 5);
      expect(msg.val == "hello");
   };
   
   "std::byte buffer"_test = []
   {
      TestMsg msg{};
      msg.id = 5;
      msg.val = "hello";
      std::vector<std::byte> buffer{};
      glz::write_json(msg, buffer);
      
      buffer.emplace_back(static_cast<std::byte>('\0'));
      
      msg.id = 0;
      msg.val = "";
      
      glz::read_json(msg, buffer);
      expect(msg.id == 5);
      expect(msg.val == "hello");
   };
   
   "char8_t buffer"_test = []
   {
      TestMsg msg{};
      msg.id = 5;
      msg.val = "hello";
      std::vector<char8_t> buffer{};
      glz::write_json(msg, buffer);
      
      buffer.emplace_back('\0');
      
      msg.id = 0;
      msg.val = "";
      
      glz::read_json(msg, buffer);
      expect(msg.id == 5);
      expect(msg.val == "hello");
   };
};

template <class T>
struct custom_unique
{
   std::unique_ptr<T> x{};
   
   custom_unique(std::unique_ptr<T>&& in) : x(std::move(in)) {}
   
   operator bool() {
      return static_cast<bool>(x);
   }
   
   T& operator*() {
      return *x;
   }
   
   void reset() {
      x.reset();
   }
};

template <class T, class... Args>
inline auto make_custom_unique(Args&&... args) {
   return custom_unique{ std::make_unique<T>(std::forward<Args>(args)...) };
}

template <class T>
struct glz::meta<custom_unique<T>>
{
   static constexpr auto construct = [] { return make_custom_unique<T>(); };
};

suite custom_unique_tests = []
{
   "custom unique"_test = [] {
      auto c = make_custom_unique<int>(5);
      
      std::string s = "5";
      glz::read_json(c, s);
      
      expect(*c.x == 5);
      
      s.clear();
      glz::write_json(c, s);
      expect(s == "5");
      
      s = "null";
      glz::read_json(c, s);
      expect(!c);
      
      s = "5";
      glz::read_json(c, s);
      
      expect(*c.x == 5);
   };
};

#include <set>
#include <unordered_set>

static_assert(glz::detail::emplaceable<std::set<std::string>>);

suite sets = []
{
   "std::unordered_set"_test = [] {
      std::unordered_set<std::string> set;
      set.emplace("hello");
      set.emplace("world");
      
      std::string b{};
      
      glz::write_json(set, b);
      
      expect(b == R"(["hello","world"])" || b == R"(["world","hello"])");
      
      set.clear();
      
      glz::read_json(set, b);
      
      expect(set.count("hello") == 1);
      expect(set.count("world") == 1);
   };
   
   "std::set"_test = [] {
      std::set<int> set = { 5, 4, 3, 2, 1 };
      std::string b{};
      glz::write_json(set, b);
      
      expect(b == R"([1,2,3,4,5])");
      
      set.clear();
      
      glz::read_json(set, b);
      
      expect(set.count(1) == 1);
      expect(set.count(2) == 1);
      expect(set.count(3) == 1);
      expect(set.count(4) == 1);
      expect(set.count(5) == 1);
   };
};

struct flags_t
{
   bool x{ true };
   bool y{};
   bool z{ true };
};

template <>
struct glz::meta<flags_t>
{
   using T = flags_t;
   static constexpr auto value = flags("x", &T::x, "y", &T::y, "z", &T::z);
};

suite flag_test = []
{
   "flags"_test = []
   {
      flags_t s{};
      
      std::string b{};
      glz::write_json(s, b);
      
      expect(b == R"(["x","z"])");
      
      s.x = false;
      s.z = false;
      
      glz::read_json(s, b);
      
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

suite get_sv = []
{
   "get_sv"_test = []
   {
      std::string s = R"({"obj":{"x":5.5}})";
      const auto x = glz::get_view_json<"/obj/x">(s);
      
      auto str = glz::sv{x.data(), x.size()};
      expect(str == "5.5");
      
      double y;
      glz::read_json(y, x);
      
      auto z = glz::get_as_json<double, "/obj/x">(s);
      
      expect(z == 5.5);
      
      auto view = glz::get_sv_json<"/obj/x">(s);
      
      expect(view == "5.5");
   };
   
   "action"_test = []
   {
      std::string buffer = R"( { "action": "DELETE", "data": { "x": 10, "y": 200 }})";
      
      auto action = glz::get_sv_json<"/action">(buffer);
      
      expect(action == R"("DELETE")");
      if (action == R"("DELETE")") {
         auto bomb = glz::read_json<bomb_t>(buffer);
         expect(bomb.data.x == 10);
         expect(bomb.data.y == 200);
      }
   };
};

int main()
{
   using namespace boost::ut;
   
   basic_types();
   container_types();
   nullable_types();
   enum_types();
   user_types();
   json_pointer();
   early_end(); 
   bench();
   read_tests();
   write_tests();
   study_tests();
   file_include_test();
   file_include_test_auto();
   nested_file_include_test();
}
