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

#include "boost/ut.hpp"
#include "glaze/json/json_ptr.hpp"
#include "glaze/json/overwrite.hpp"
#include "glaze/json/read.hpp"
#include "glaze/json/write.hpp"
#include "glaze/json/prettify.hpp"

using namespace boost::ut;

struct my_struct
{
  int i = 287;
  double d = 3.14;
  std::string hello = "Hello World";
  std::array<uint64_t, 3> arr = { 1, 2, 3 };
};

template <>
struct glaze::meta<my_struct> {
   using T = my_struct;
   static constexpr auto value = glaze::object(
      "i", &T::i, //
      "d", &T::d, //
      "hello", &T::hello, //
      "arr", &T::arr //
   );
};

suite starter = [] {

    "example"_test = [] {
       my_struct s{};
       std::string buffer{};
       glaze::write_json(s, buffer);
       expect(buffer == R"({"i":287,"d":3.14,"hello":"Hello World","arr":[1,2,3]})");
       expect(glaze::prettify(buffer) == R"({
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
struct glaze::meta<sub_thing> {
   static constexpr auto value = glaze::object(
      "a", &sub_thing::a, "Test comment 1"_c,  //
      "b", &sub_thing::b, "Test comment 2"_c   //
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
struct glaze::meta<sub_thing2> {
   using T = sub_thing2;
   static constexpr auto value = glaze::object(
      "a", &T::a, "Test comment 1"_c,  //
      "b", &T::b, "Test comment 2"_c,  //
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
};

template <>
struct glaze::meta<V3> {
   static constexpr auto value = glaze::array(&V3::x, &V3::y, &V3::z);
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
struct glaze::meta<Thing> {
   using T = Thing;
   static constexpr auto value = glaze::object(
      "thing",       &T::thing,                                     //
      "thing2array", &T::thing2array,                               //
      "vec3",        &T::vec3,                                      //
      "list",        &T::list,                                      //
      "deque",       &T::deque,                                     //
      "vector",      &T::vector,                                    //
      "i",           &T::i,                                         //
      "d",           &T::d,           "double is the best type"_c,  //
      "b",           &T::b,                                         //
      "c",           &T::c,                                         //
      "vb",          &T::vb,                                        //
      "sptr",        &T::sptr,                                      //
      "optional",    &T::optional,                                  //
      "array",       &T::array,                                     //
      "map",         &T::map,                                       //
      "mapi",        &T::mapi,                                      //
      "thing_ptr",   &T::thing_ptr                                  //
   );
};

void basic_types() {
   using namespace boost::ut;

   "double write"_test = [] {
      std::string buffer{};
      glaze::write_json(3.14, buffer);
      expect(buffer == "3.14");
      buffer.clear();
      glaze::write_json(9.81, buffer);
      expect(buffer == "9.81");
      buffer.clear();
      glaze::write_json(0.0, buffer);
      expect(buffer == "0");
      buffer.clear();
      glaze::write_json(-0.0, buffer);
      expect(buffer == "-0");
   };

   "double read valid"_test = [] {
      double num{};
      glaze::read_json(num, "3.14");
      expect(num == 3.14);
      glaze::read_json(num, "9.81");
      expect(num == 9.81);
      glaze::read_json(num, "0");
      expect(num == 0);
      glaze::read_json(num, "-0");
      expect(num == -0);
   };

   "int write"_test = [] {
      std::string buffer{};
      glaze::write_json(0, buffer);
      expect(buffer == "0");
      buffer.clear();
      glaze::write_json(999, buffer);
      expect(buffer == "999");
      buffer.clear();
      glaze::write_json(-6, buffer);
      expect(buffer == "-6");
      buffer.clear();
      glaze::write_json(10000, buffer);
      expect(buffer == "10000");
   };

   "int read valid"_test = [] {
      int num{};
      glaze::read_json(num, "-1");
      expect(num == -1);
      glaze::read_json(num, "0");
      expect(num == 0);
      glaze::read_json(num, "999");
      expect(num == 999);
      glaze::read_json(num, "1e4");
      expect(num == 10000);
   };

   "bool write"_test = [] {
      std::string buffer{};
      glaze::write_json(true, buffer);
      expect(buffer == "true");
      buffer.clear();
      glaze::write_json(false, buffer);
      expect(buffer == "false");
   };

   "bool read valid"_test = [] {
      bool val{};
      glaze::read_json(val, "true");
      expect(val == true);
      glaze::read_json(val, "false");
      expect(val == false);
   };

   "string write"_test = [] {
      std::string buffer{};
      glaze::write_json("fish", buffer);
      expect(buffer == "\"fish\"");
      buffer.clear();
      glaze::write_json("as\"df\\ghjkl", buffer);
      expect(buffer == "\"as\\\"df\\\\ghjkl\"");
   };

   "bool read valid"_test = [] {
      std::string val{};
      glaze::read_json(val, "\"fish\"");
      expect(val == "fish");
      glaze::read_json(val, "\"as\\\"df\\\\ghjkl\"");
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
      glaze::write_json(vec, buffer);
      glaze::read_json(vec2, buffer);
      expect(vec == vec2);
   };
   "vector double roundtrip"_test = [] {
      std::vector<double> vec(100);
      for (auto& item : vec) item = rand() / (1.0 + rand());
      std::string buffer{};
      std::vector<double> vec2{};
      glaze::write_json(vec, buffer);
      glaze::read_json(vec2, buffer);
      expect(vec == vec2);
   };
   "vector bool roundtrip"_test = [] {
      std::vector<bool> vec(100);
      for (auto&& item : vec) item = rand() / (1.0 + rand());
      std::string buffer{};
      std::vector<bool> vec2{};
      glaze::write_json(vec, buffer);
      glaze::read_json(vec2, buffer);
      expect(vec == vec2);
   };
   "deque roundtrip"_test = [] {
      std::vector<int> deq(100);
      for (auto& item : deq) item = rand();
      std::string buffer{};
      std::vector<int> deq2{};
      glaze::write_json(deq, buffer);
      glaze::read_json(deq2, buffer);
      expect(deq == deq2);
   };
   "list roundtrip"_test = [] {
      std::list<int> lis(100);
      for (auto& item : lis) item = rand();
      std::string buffer{};
      std::list<int> lis2{};
      glaze::write_json(lis, buffer);
      glaze::read_json(lis2, buffer);
      expect(lis == lis2);
   };
   "forward_list roundtrip"_test = [] {
      std::forward_list<int> lis(100);
      for (auto& item : lis) item = rand();
      std::string buffer{};
      std::forward_list<int> lis2{};
      glaze::write_json(lis, buffer);
      glaze::read_json(lis2, buffer);
      expect(lis == lis2);
   };
   "map string keys roundtrip"_test = [] {
      std::map<std::string, int> map;
      std::string str {"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"};
      std::mt19937 g{};
      for (auto i = 0; i < 20; ++i) {
         nano::ranges::shuffle(str, g);
         map[str] = rand();
      }
      std::string buffer{};
      std::map<std::string, int> map2{};
      glaze::write_json(map, buffer);
      glaze::read_json(map2, buffer);
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
      glaze::write_json(map, buffer);
      glaze::read_json(map2, buffer);
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
      glaze::write_json(map, buffer);
      glaze::read_json(map2, buffer);
      // expect(map == map2);
      for (auto& it : map) {
         expect(map2[it.first] == it.second);
      }
   };
   "tuple roundtrip"_test = [] {
      auto tuple = std::make_tuple(3, 2.7, std::string("curry"));
      decltype(tuple) tuple2{};
      std::string buffer{};
      glaze::write_json(tuple, buffer);
      glaze::read_json(tuple2, buffer);
      expect(tuple == tuple2);
   };
}

void nullable_types() {
   using namespace boost::ut;
   "optional"_test = [] {
      std::optional<int> oint{};
      std::string buffer{};
      glaze::write_json(oint, buffer);
      expect(buffer == "null");

      glaze::read_json(oint, "5");
      expect(bool(oint) && *oint == 5);
      buffer.clear();
      glaze::write_json(oint, buffer);
      expect(buffer == "5");

      glaze::read_json(oint, "null");
      expect(!bool(oint));
      buffer.clear();
      glaze::write_json(oint, buffer);
      expect(buffer == "null");
   };
   "shared_ptr"_test = [] {
      std::shared_ptr<int> oint{};
      std::string buffer{};
      glaze::write_json(oint, buffer);
      expect(buffer == "null");

      glaze::read_json(oint, "5");
      expect(bool(oint) && *oint == 5);
      buffer.clear();
      glaze::write_json(oint, buffer);
      expect(buffer == "5");

      glaze::read_json(oint, "null");
      expect(!bool(oint));
      buffer.clear();
      glaze::write_json(oint, buffer);
      expect(buffer == "null");
   };
   "unique_ptr"_test = [] {
      std::unique_ptr<int> oint{};
      std::string buffer{};
      glaze::write_json(oint, buffer);
      expect(buffer == "null");

      glaze::read_json(oint, "5");
      expect(bool(oint) && *oint == 5);
      buffer.clear();
      glaze::write_json(oint, buffer);
      expect(buffer == "5");

      glaze::read_json(oint, "null");
      expect(!bool(oint));
      buffer.clear();
      glaze::write_json(oint, buffer);
      expect(buffer == "null");
   };
}

void user_types() {
   using namespace boost::ut;

   "user array"_test = [] {
      V3 v3{9.1, 7.2, 1.9};
      std::string buffer{};
      glaze::write_json(v3, buffer);
      expect(buffer == "[9.1,7.2,1.9]");

      glaze::read_json(v3, "[42.1,99.2,55.3]");
      expect(v3.x == 42.1 && v3.y == 99.2 && v3.z == 55.3);
   };

   "simple user obect"_test = [] {
      sub_thing obj{.a = 77.2, .b = "not a lizard"};
      std::string buffer{};
      glaze::write_json(obj, buffer);
      expect(buffer == "{\"a\":77.2,\"b\":\"not a lizard\"}");

      glaze::read_json(obj, "{\"a\":999,\"b\":\"a boat of goldfish\"}");
      expect(obj.a == 999.0 && obj.b == "a boat of goldfish");

      //Should skip invalid keys
      expect(nothrow([&] {
         //glaze::read_json(obj,"{/**/ \"b\":\"fox\", \"c\":7.7/**/, \"d\": {\"a\": \"}\"} //\n   /**/, \"a\":322}");
         glaze::read_json(obj,
                          R"({/**/ "b":"fox", "c":7.7/**/, "d": {"a": "}"} //
   /**/, "a":322})");
      }));
      expect(obj.a == 322.0 && obj.b == "fox");
   };

   "complex user obect"_test = [] {
      Thing obj{};
      std::string buffer{};
      glaze::write_json(obj, buffer);
      expect(buffer == R"({"thing":{"a":3.14,"b":"stuff"},"thing2array":[{"a":3.14,"b":"stuff","c":999.342494903,"d":1e-12,"e":203082348402.1,"f":89.089,"g":12380.00000013,"h":1000000.000001}],"vec3":[3.14,2.7,6.5],"list":[6,7,8,2],"deque":[9,6.7,3.1],"vector":[[9,6.7,3.1],[3.14,2.7,6.5]],"i":8,"d":2,"b":false,"c":"W","vb":[true,false,false,true,true,true,true],"sptr":{"a":3.14,"b":"stuff"},"optional":null,"array":["as\"df\\ghjkl","pie","42","foo"],"map":{"a":4,"b":12,"f":7},"mapi":{"2":9.63,"5":3.14,"7":7.42},"thing_ptr":{"a":3.14,"b":"stuff"}})");
      expect(nothrow([&] { glaze::read_json(obj, buffer); }));

      buffer.clear();
      glaze::write_jsonc(obj, buffer);
      expect(buffer == R"({"thing":{"a":3.14/*Test comment 1*/,"b":"stuff"/*Test comment 2*/},"thing2array":[{"a":3.14/*Test comment 1*/,"b":"stuff"/*Test comment 2*/,"c":999.342494903,"d":1e-12,"e":203082348402.1,"f":89.089,"g":12380.00000013,"h":1000000.000001}],"vec3":[3.14,2.7,6.5],"list":[6,7,8,2],"deque":[9,6.7,3.1],"vector":[[9,6.7,3.1],[3.14,2.7,6.5]],"i":8,"d":2/*double is the best type*/,"b":false,"c":"W","vb":[true,false,false,true,true,true,true],"sptr":{"a":3.14/*Test comment 1*/,"b":"stuff"/*Test comment 2*/},"optional":null,"array":["as\"df\\ghjkl","pie","42","foo"],"map":{"a":4,"b":12,"f":7},"mapi":{"2":9.63,"5":3.14,"7":7.42},"thing_ptr":{"a":3.14/*Test comment 1*/,"b":"stuff"/*Test comment 2*/}})");
      expect(nothrow([&] { glaze::read_json(obj, buffer); }));
   };
}

void json_pointer() {
   using namespace boost::ut;

   "seek"_test = [] {
      Thing thing{};
      std::any a{};
      glaze::seek([&](auto&& val) { a = val; }, thing, "/thing_ptr/a");
      expect(a.has_value() && a.type() == typeid(double) &&
             std::any_cast<double>(a) == thing.thing_ptr->a);
   };

   "get"_test = [] {
      Thing thing{};
      expect(thing.thing.a == glaze::get<double>(thing, "/thing_ptr/a"));
      expect(&thing.map["f"] == glaze::get_if<int>(thing, "/map/f"));
      expect(&thing.vector == glaze::get_if<std::vector<V3>>(thing, "/vector"));
      expect(&thing.vector[1] == glaze::get_if<V3>(thing, "/vector/1"));
      expect(thing.vector[1].x == glaze::get<double>(thing, "/vector/1/0"));
      expect(thing.thing_ptr == glaze::get<sub_thing*>(thing, "/thing_ptr"));

      //Invalid lookup
      expect(throws([&] { glaze::get<char>(thing, "/thing_ptr/a"); }));
      expect(nothrow([&] { glaze::get_if<char>(thing, "/thing_ptr/a"); }));
      expect(throws([&] { glaze::get<double>(thing, "/thing_ptr/c"); }));
      expect(nothrow([&] { glaze::get_if<double>(thing, "/thing_ptr/c"); }));
   };

   "set"_test = [] {
      Thing thing{};
      glaze::set(thing, "/thing_ptr/a", 42.0);
      glaze::set(thing, "/thing_ptr/b", "Value was set.");
      expect(thing.thing_ptr->a == 42.0);
      expect(thing.thing_ptr->b == "Value was set.");
   };

   "set tuple"_test = [] {
      auto tuple = std::make_tuple(3, 2.7, std::string("curry"));
      glaze::set(tuple, "/0", 5);
      glaze::set(tuple, "/1", 42.0);
      glaze::set(tuple, "/2", "fish");
      expect(std::get<0>(tuple) == 5.0);
      expect(std::get<1>(tuple) == 42.0);
      expect(std::get<2>(tuple) == "fish");
   };

   "overwrite"_test = [] {
      Thing thing{};
      glaze::overwrite(thing, "/vec3", "[7.6, 1292.1, 0.333]");
      expect(thing.vec3.x == 7.6 && thing.vec3.y == 1292.1 &&
             thing.vec3.z == 0.333);

      glaze::overwrite(thing, "/vec3/2", "999.9");
      expect(thing.vec3.z == 999.9);
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
         buffer = buffer.substr(0, buffer.size() - 1);
         // This is mainly to check if all our end checks are in place. In debug mode it should check if we try to read past the end and abort.
         expect(throws([&] { glaze::read_json(obj, buffer); }));
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
      glaze::write_json(thing, buffer);

      auto tstart = std::chrono::high_resolution_clock::now();
      for (size_t i{}; i < repeat; ++i) {
         buffer.clear();
         glaze::write_json(thing, buffer);
      }
      auto tend = std::chrono::high_resolution_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::duration<double>>(
                         tend - tstart)
                         .count();
      auto mbytes_per_sec = repeat * buffer.size() / (duration * 1048576);
      std::cout << "to_json: " << duration << " s, " << mbytes_per_sec
                << " MB/s"
                << "\n";

      tstart = std::chrono::high_resolution_clock::now();
      for (size_t i{}; i < repeat; ++i) {
         glaze::read_json(thing, buffer);
      }
      tend = std::chrono::high_resolution_clock::now();
      duration = std::chrono::duration_cast<std::chrono::duration<double>>(
                    tend - tstart)
                    .count();
      mbytes_per_sec = repeat * buffer.size() / (duration * 1048576);
      std::cout << "from_json: " << duration << " s, " << mbytes_per_sec
                     << " MB/s" << "\n";

      tstart = std::chrono::high_resolution_clock::now();
      for (size_t i{}; i < repeat; ++i) {
         glaze::get<std::string>(thing, "/thing_ptr/b");
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
};

struct oob
{
   v3 v{};
   int n{};
};

template <>
struct glaze::meta<v3>
{
   static constexpr auto value = glaze::array(&v3::x, &v3::y, &v3::z);
};

static_assert(glaze::detail::is_specialization_v<std::decay_t<decltype(glaze::meta<v3>::value)>, glaze::detail::Array>, "");

template <>
struct glaze::meta<oob>
{
  static constexpr auto value = glaze::object("v", &oob::v, "n", &oob::n);
};

void Read_tests() {
   using namespace boost::ut;
   
   "cout"_test = [] {
      std::stringstream ss{};
      ss << "3958713";
      int i{};
      glaze::read_json(i, ss);
      expect(i == 3958713);
   };
   
   "Read floating point types"_test = [] {
      {
         std::string s = "0.96875";
         float f{};
         glaze::read_json(f, s);
         expect(f == 0.96875f);
      }
      {
         std::string s = "0.96875";
         double f{};
         glaze::read_json(f, s);
         expect(f == 0.96875);
      }
      {
         std::string s = "0.96875";
         long double f{};
         glaze::read_json(f, s);
         expect(f == 0.96875L);
      }
   };

   "Read integral types"_test = [] {
      {
         std::string s = "true";
         bool v;
         glaze::read_json(v, s);
         expect(v);
      }
//*   // TODO add escaped char support for unicode
      /*{
         const auto a_num = static_cast<int>('15\u00f8C');
         std::string s = std::to_string(a_num);
         char v{};
         glaze::read_json(v, s);
         expect(v == a_num);
      }
      {
         const auto a_num = static_cast<int>('15\u00f8C');
         std::string s = std::to_string(a_num);
         wchar_t v{};
         glaze::read_json(v, s);
         expect(v == a_num);
      }*/
      {
         std::string s = "1";
         short v;
         glaze::read_json(v, s);
         expect(v == 1);
      }
      {
         std::string s = "1";
         int v;
         glaze::read_json(v, s);
         expect(v == 1);
      }
      {
         std::string s = "1";
         long v;
         glaze::read_json(v, s);
         expect(v == 1);
      }
      {
         std::string s = "1";
         long long v;
         glaze::read_json(v, s);
         expect(v == 1);
      }
      {
         std::string s = "1";
         unsigned short v;
         glaze::read_json(v, s);
         expect(v == 1);
      }
      {
         std::string s = "1";
         unsigned int v;
         glaze::read_json(v, s);
         expect(v == 1);
      }
      {
         std::string s = "1";
         unsigned long v;
         glaze::read_json(v, s);
         expect(v == 1);
      }
      {
         std::string s = "1";
         unsigned long long v;
         glaze::read_json(v, s);
         expect(v == 1);
      }
   };

   "multiple int from double text"_test = [] {
      std::vector<int> v;
      std::string buffer = "[1.66, 3.24, 5.555]";
      expect(nothrow([&] { glaze::read_json(v, buffer); }));
      expect(v.size() == 3);
      expect(v[0] == 1);
      expect(v[1] == 3);
      expect(v[2] == 5);
   };

   "comments"_test = [] {
      {
         std::string b = "1/*a comment*/00";
         int a{};
         glaze::read_json(a, b);
//*      fails test
         //expect(a == 100);
      }
      {
         std::string b = R"([100, // a comment
20])";
         std::vector<int> a{};
         glaze::read_json(a, b);
         expect(a[0] == 100);
         expect(a[1] == 20);
      }
   };

   "Failed character read"_test = [] {
      std::string err;
      {
         char b;
         expect(throws([&] { glaze::read_json(b, err); }));
      }
   };

   "Read array type"_test = [] {
      std::string in = "    [ 3.25 , 1.125 , 3.0625 ]   ";
      v3 v{};
      glaze::read_json(v, in);

      expect(v.x == 3.25);
      expect(v.y == 1.125);
      expect(v.z == 3.0625);
   };

   "Read partial array type"_test = [] {
      {
         std::string in = "    [ 3.25 , null , 3.125 ]   ";
         v3 v{};

         expect(throws([&] { glaze::read_json(v, in); }));
      }
 //*  // missing charictar bug?
      {
         std::string in = "    [ 3.25 , 3.125 ]   ";
         [[maybe_unused]] v3 v{};
         //glaze::read_json(v, in);

         //expect(v.x == 3.25);
         //expect(v.y == 3.125);
         //expect(v.z == 0.0);
      }
   };

   "Read object type"_test = [] {
      std::string in =
         R"(    { "v" :  [ 3.25 , 1.125 , 3.0625 ]   , "n" : 5 } )";
      oob oob{};
      glaze::read_json(oob, in);

      expect(oob.v.x == 3.25);
      expect(oob.v.y == 1.125);
      expect(oob.v.z == 3.0625);
      expect(oob.n == 5);
   };

   "Read partial object type"_test = [] {
      std::string in =
         R"(    { "v" :  [ 3.25 , null , 3.0625 ]   , "n" : null } )";
      oob oob{};

      expect(throws([&] { glaze::read_json(oob, in); }));
   };

   "Reversed object"_test = [] {
      std::string in =
         R"(    {  "n" : 5   ,  "v" :  [ 3.25 , 1.125 , 3.0625 ] } )";
      oob oob{};
      glaze::read_json(oob, in);

      expect(oob.v.x == 3.25);
      expect(oob.v.y == 1.125);
      expect(oob.v.z == 3.0625);
      expect(oob.n == 5);
   };

   "Read list"_test = [] {
      std::string in = "[1, 2, 3, 4]";
      std::list<int> l, lr{1, 2, 3, 4};
      glaze::read_json(l, in);

      expect(l == lr);
   };

   "Read forward list"_test = [] {
      std::string in = "[1, 2, 3, 4]";
      std::forward_list<int> l, lr{1, 2, 3, 4};
      glaze::read_json(l, in);

      expect(l == lr);
   };

   "Read deque"_test = [] {
      {
         std::string in = "[1, 2, 3, 4]";
         std::deque<int> l, lr{1, 2, 3, 4};
         glaze::read_json(l, in);

         expect(l == lr);
      }
      {
         std::string in = "[1, 2, 3, 4]";
         std::deque<int> l{8, 9}, lr{1, 2, 3, 4};
         glaze::read_json(l, in);

         expect(l == lr);
      }
   };

   "Read into returned data"_test = [] {
      const std::string s = "[1, 2, 3, 4, 5, 6]";
      const std::vector<int> v{1, 2, 3, 4, 5, 6};
      std::vector<int> vr;
      glaze::read_json(vr, s);
      expect(vr == v);
   };

   "Read array"_test = [] {
      {
         std::string in = R"(    [1, 5, 232, 75, 123, 54, 89] )";
         std::array<int, 7> v1{}, v2{99}, v3{99, 99, 99, 99, 99},
            vr{1, 5, 232, 75, 123, 54, 89};
         glaze::read_json(v1, in);
         glaze::read_json(v2, in);
         glaze::read_json(v3, in);
         expect(v1 == vr);
         expect(v2 == vr);
         expect(v3 == vr);
      }
   };

   "Read vector"_test = [] {
      {
         std::string in = R"(    [1, 5, 232, 75, 123, 54, 89] )";
         std::vector<int> v, vr{1, 5, 232, 75, 123, 54, 89};
         glaze::read_json(v, in);

         expect(v == vr);
      }
      {
         std::string in = R"([true, false, true, false])";
         std::vector<bool> v, vr{true, false, true, false};
         glaze::read_json(v, in);

         expect(v == vr);
      }
      {
         std::string in = R"(    [1, 5, 232, 75, 123, 54, 89] )";
         std::vector<int> v{1, 2, 3, 4}, vr{1, 5, 232, 75, 123, 54, 89};
         glaze::read_json(v, in);

         expect(v == vr);
      }
      {
         std::string in = R"(    [1, 5, 232, 75, 123, 54, 89] )";
         std::vector<int> v{1, 2, 3, 4, 5, 6, 7, 8, 9, 10},
            vr{1, 5, 232, 75, 123, 54, 89};
         glaze::read_json(v, in);

         expect(v == vr);
      }
   };

   "Read partial vector"_test = [] {
      std::string in = R"(    [1, 5, 232, 75, null, 54, 89] )";
      std::vector<int> v, vr{1, 5, 232, 75, 0, 54, 89};
      
      expect(throws([&] { glaze::read_json(v, in); }));
   };

//*  ISSUE UT cannot run this test
   "Read map"_test = [] {
      {
         std::string in = R"(   { "as" : 1, "so" : 2, "make" : 3 } )";
         std::map<std::string, int> v, vr{{"as", 1}, {"so", 2}, {"make", 3}};
         glaze::read_json(v, in);

         //expect(v == vr);
      }
      {
         std::string in = R"(   { "as" : 1, "so" : 2, "make" : 3 } )";
         std::map<std::string, int> v{{"as", -1}, {"make", 10000}},
            vr{{"as", 1}, {"so", 2}, {"make", 3}};
         glaze::read_json(v, in);

         //expect(v == vr);
      }
   };
   
   "Read partial map"_test = [] {
      std::string in = R"(   { "as" : 1, "so" : null, "make" : 3 } )";
      std::map<std::string, int> v, vr{{"as", 1}, {"so", 0}, {"make", 3}};

      expect(throws([&] { glaze::read_json(v, in); }));
   };

   "Read boolean"_test = [] {
      {
         std::string in = R"(true)";
         bool res{};
         glaze::read_json(res, in);

         expect(res == true);
      }
      {
         std::string in = R"(false)";
         bool res{true};
         glaze::read_json(res, in);

         expect(res == false);
      }
      {
         std::string in = R"(null)";
         bool res{false};
         
         expect(throws([&] {glaze::read_json(res, in); }));
      }
   };

   "Read integer"_test = [] {
      {
         std::string in = R"(-1224125asdasf)";
         int res{};
         glaze::read_json(res, in);

         expect(res == -1224125);
      }
      {
         std::string in = R"(null)";
         int res{};
         
         expect(throws([&] { glaze::read_json(res, in); }));
      }
   };

   "Read double"_test = [] {
      {
         std::string in = R"(0.072265625flkka)";
         double res{};
         glaze::read_json(res, in);
         expect(res == 0.072265625);
      }
      {
         std::string in = R"(1e5das)";
         double res{};
         glaze::read_json(res, in);
         expect(res == 1e5);
      }
      {
         std::string in = R"(-0)";
         double res{};
         glaze::read_json(res, in);
         expect(res == -0.0);
      }
      {
         std::string in = R"(0e5)";
         double res{};
         glaze::read_json(res, in);
         expect(res == 0.0);
      }
      {
         std::string in = R"(0)";
         double res{};
         glaze::read_json(res, in);
         expect(res == 0.0);
      }
      {
         std::string in = R"(11)";
         double res{};
         glaze::read_json(res, in);
         expect(res == 11.0);
      }
      {
         std::string in = R"(0a)";
         double res{};
         glaze::read_json(res, in);
         expect(res == 0.0);
      }
      {
         std::string in = R"(11.0)";
         double res{};
         glaze::read_json(res, in);
         expect(res == 11.0);
      }
      {
         std::string in = R"(11e5)";
         double res{};
         glaze::read_json(res, in);
         expect(res == 11.0e5);
      }
      {
         std::string in = R"(null)";
         double res{};
         
         expect(throws([&] {glaze::read_json(res, in); }));
      }
      {
         std::string res = R"(success)";
         double d;
         expect(throws([&] {glaze::read_json(d, res); }));
      }
      {
         std::string res = R"(-success)";
         double d;
         expect(throws([&] {glaze::read_json(d, res); }));
      }
      {
         std::string res = R"(1.a)";
         double d;

#ifdef CAN_USE_CHARCONV
         expect(nothrow([&] {glaze::read_json(d, res); }));
#else
         expect(throws([&] {glaze::read_json(d, res); }));
#endif
      }
      {
         std::string res = R"()";
         double d;
         expect(throws([&] {glaze::read_json(d, res); }));
      }
      {
         std::string res = R"(-)";
         double d;
         expect(throws([&] {glaze::read_json(d, res); }));
      }
      {
         std::string res = R"(1.)";
         double d;

#ifdef CAN_USE_CHARCONV
         expect(nothrow([&] { glaze::read_json(d, res); }));
#else
         expect(throws([&] { glaze::read_json(d, res); }));
#endif
      }
      {
         std::string res = R"(1.0e)";
         double d;

#ifdef CAN_USE_CHARCONV
         expect(nothrow([&] { glaze::read_json(d, res); }));
#else
         expect(throws([&] { glaze::read_json(d, res); }));
#endif
      }
      {
         std::string res = R"(1.0e-)";
         double d;

#ifdef CAN_USE_CHARCONV
         expect(nothrow([&] { glaze::read_json(d, res); }));
#else
         expect(throws([&] { glaze::read_json(d, res); }));
#endif
      }
   };

   "Read string"_test = [] {
      std::string in =
         R"("asljl{}121231212441[]123::,,;,;,,::,Q~123\a13dqwdwqwq")";
      std::string res{};
      glaze::read_json(res, in);
//*   function fails, does not recognize '\'
      //expect(res == "asljl{}121231212441[]123::,,;,;,,::,Q~123\\a13dqwdwqwq");
   };

   "Nested array"_test = [] {
      std::vector<v3> v;
      std::string buf =
         R"([[1.000000,0.000000,3.000000],[2.000000,0.000000,0.000000]])";

      glaze::read_json(v, buf);
      expect(v[0].x == 1.0);
      expect(v[0].z == 3.0);
      expect(v[1].x == 2.0);
   };

   "Nested map"_test = [] {
      std::map<std::string, v3> m;
      std::string buf =
         R"({"1":[4.000000,0.000000,0.000000],"2":[5.000000,0.000000,0.000000]})";

      glaze::read_json(m, buf);
      expect(m["1"].x == 4.0);
      expect(m["2"].x == 5.0);
   };

//*  Bellow doesn't initialize m.
//*  std::map<std::string, std::vector<double>> not allowed in c++20?
   /*"Nested map 2"_test = {
      std::map<std::string, std::vector<double>> m;
      std::string buf =
         R"({"1":[4.000000,0.000000,0.000000],"2":[5.000000,0.000000,0.000000,4.000000]})";

      glaze::read_json(m, buf);
      expect(m["1"][0] == 4.0);
      expect(m["2"][0] == 5.0);
      expect(m["2"][3] == 4.0);
   };*/

   "Integer keyed map"_test = [] {
      std::map<int, std::vector<double>> m;
      std::string buf =
         R"({"1":[4.000000,0.000000,0.000000],"2":[5.000000,0.000000,0.000000,4.000000]})";

      glaze::read_json(m, buf);
      expect(m[1][0] == 4.0);
      expect(m[2][0] == 5.0);
      expect(m[2][3] == 4.0);
   };
//*  glaze does not support this type
   "Invalid integer keyed map"_test = [] {
      std::map<int, std::vector<double>> m;
      // Numeric keys are not valid json but there is no harm in supporting them
      // when reading
      std::string buf =
         R"({1:[4.000000,0.000000,0.000000],2:[5.000000,0.000000,0.000000,4.000000]})";

      //glaze::read_json(m, buf);
      //expect(m[1][0] == 4.0);
      //expect(m[2][0] == 5.0);
      //expect(m[2][3] == 4.0);
   };
}

int main()
{
   using namespace boost::ut;
   // TODO:
   // *Way more invalid input tests.
   // *Valid but with combinations of comments and whitespace to validate that code is working correctly.
   // *More complex string and json pointer tests.
   // *Stream tests.
   // *Test other buffer types.
   // *More tests in general.

   basic_types();
   container_types();
   nullable_types();
   user_types();
   json_pointer();
   early_end(); 
   bench();
   Read_tests();
}
