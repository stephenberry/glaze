// Glaze Library
// For the license information refer to glaze.hpp

#include <chrono>
#include <iostream>
#include <random>
#include <any>
#include <forward_list>

#include "boost/ut.hpp"
#include "glaze/json/json_ptr.hpp"
#include "glaze/json/overwrite.hpp"
#include "glaze/json/read.hpp"
#include "glaze/json/write.hpp"

struct SubThing
{
   double a{3.14};
   std::string b{"stuff"};
};

template <>
struct glaze::meta<SubThing> {
   static constexpr auto value = glaze::object(
      "a", &SubThing::a, "Test comment 1"_c,  //
      "b", &SubThing::b, "Test comment 2"_c   //
   );
};

struct SubThing2
{
   double a{3.14};
   std::string b{"stuff"};
   double c{999.342494903};
   double d{0.000000000001};
   double e{203082348402.1};
   double f{89.089};
   double g{12380.00000013};
   double h{1000000.000001};
};

template <>
struct glaze::meta<SubThing2> {
   using T = SubThing2;
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
   SubThing thing{};
   std::array<SubThing2, 1> thing2array{};
   V3 vec3{};
   std::list<int> list{6, 7, 8, 2};
   std::array<std::string, 4> array = {"as\"df\\ghjkl", "pie", "42", "foo"};
   std::vector<V3> vector = {{9.0, 6.7, 3.1}, {}};
   int i{8};
   double d{2};
   bool b{};
   char c{'W'};
   std::vector<bool> vb = {true, false, false, true, true, true, true};
   std::shared_ptr<SubThing> sptr = std::make_shared<SubThing>();
   std::optional<V3> optional{};
   std::deque<double> deque = {9.0, 6.7, 3.1};
   std::map<std::string, int> map = {{"a", 4}, {"f", 7}, {"b", 12}};
   std::map<int, double> mapi{{5, 3.14}, {7, 7.42}, {2, 9.63}};
   SubThing* thing_ptr{};

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
      SubThing obj{.a = 77.2, .b = "not a lizard"};
      std::string buffer{};
      glaze::write_json(obj, buffer);
      expect(buffer == "{\"a\":77.2,\"b\":\"not a lizard\"}");

      glaze::read_json(obj, "{\"a\":999,\"b\":\"a boat of goldfish\"}");
      expect(obj.a == 999.0 && obj.b == "a boat of goldfish");

      //Should skip invalid keys
      expect(nothrow([&] {
         //vireo::read_json(obj,"{/**/ \"b\":\"fox\", \"c\":7.7/**/, \"d\": {\"a\": \"}\"} //\n   /**/, \"a\":322}");
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
      expect(thing.thing_ptr == glaze::get<SubThing*>(thing, "/thing_ptr"));

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
   // *Look at the current vireo since its got a million good tests.

   basic_types();
   container_types();
   nullable_types();
   user_types();
   json_pointer();
   early_end(); 
   bench();
}
