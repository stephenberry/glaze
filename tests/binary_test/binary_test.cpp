// Glaze Library
// For the license information refer to glaze.hpp

#ifndef BOOST_UT_DISABLE_MODULE
#define BOOST_UT_DISABLE_MODULE
#endif
#include <bit>
#include <chrono>
#include <deque>
#include <list>
#include <map>

#include "boost/ut.hpp"
#include "glaze/binary/read.hpp"
#include "glaze/binary/write.hpp"

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
   using T = my_struct;
   static constexpr auto value = object("i", &T::i, //
                                        "d", &T::d, //
                                        "hello", &T::hello, //
                                        "arr", &T::arr //
   );
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
      "a", &sub_thing::a, "Test comment 1", //
      "b", [](auto&& v) -> auto& { return v.b; }, "Test comment 2" //
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
   static constexpr auto value = object(
      "thing", &T::thing, //
      "thing2array", &T::thing2array, //
      "vec3", &T::vec3, //
      "list", &T::list, //
      "deque", &T::deque, //
      "vector", [](auto&& v) -> auto& { return v.vector; }, //
      "i", [](auto&& v) -> auto& { return v.i; }, //
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

void write_tests()
{
   using namespace boost::ut;

   "round_trip"_test = [] {
      {
         std::vector<std::byte> s;
         float f{0.96875f};
         auto start = f;
         s.resize(sizeof(float));
         std::memcpy(s.data(), &f, sizeof(float));
         std::memcpy(&f, s.data(), sizeof(float));
         expect(start == f);
      }
   };

   "bool"_test = [] {
      {
         bool b = true;
         std::vector<std::byte> out;
         glz::write_binary(b, out);
         bool b2{};
         expect(!glz::read_binary(b2, out));
         expect(b == b2);
      }
   };

   "float"_test = [] {
      {
         float f = 1.5f;
         std::vector<std::byte> out;
         glz::write_binary(f, out);
         float f2{};
         expect(!glz::read_binary(f2, out));
         expect(f == f2);
      }
   };

   "string"_test = [] {
      {
         std::string s = "Hello World";
         std::vector<std::byte> out;
         glz::write_binary(s, out);
         std::string s2{};
         expect(!glz::read_binary(s2, out));
         expect(s == s2);
      }
   };

   "array"_test = [] {
      {
         std::array<float, 3> arr = {1.2f, 3434.343f, 0.f};
         std::vector<std::byte> out;
         glz::write_binary(arr, out);
         std::array<float, 3> arr2{};
         expect(!glz::read_binary(arr2, out));
         expect(arr == arr2);
      }
   };

   "vector"_test = [] {
      {
         std::vector<float> v = {1.2f, 3434.343f, 0.f};
         std::vector<std::byte> out;
         glz::write_binary(v, out);
         std::vector<float> v2;
         expect(!glz::read_binary(v2, out));
         expect(v == v2);
      }
   };

   "my_struct"_test = [] {
      my_struct s{};
      s.i = 5;
      s.hello = "Wow!";
      std::vector<std::byte> out;
      glz::write_binary(s, out);
      my_struct s2{};
      expect(!glz::read_binary(s2, out));
      expect(s.i == s2.i);
      expect(s.hello == s2.hello);
   };

   "nullable"_test = [] {
      std::vector<std::byte> out;

      std::optional<int> op_int{};
      glz::write_binary(op_int, out);

      std::optional<int> new_op{};
      expect(!glz::read_binary(new_op, out));

      expect(op_int == new_op);

      op_int = 10;
      out.clear();

      glz::write_binary(op_int, out);
      expect(!glz::read_binary(new_op, out));

      expect(op_int == new_op);

      out.clear();

      std::shared_ptr<float> sh_float = std::make_shared<float>(5.55f);
      glz::write_binary(sh_float, out);

      std::shared_ptr<float> out_flt;
      expect(!glz::read_binary(out_flt, out));

      expect(*sh_float == *out_flt);

      out.clear();

      std::unique_ptr<double> uni_dbl = std::make_unique<double>(5.55);
      glz::write_binary(uni_dbl, out);

      std::shared_ptr<double> out_dbl;
      expect(!glz::read_binary(out_dbl, out));

      expect(*uni_dbl == *out_dbl);
   };

   "map"_test = [] {
      std::vector<std::byte> out;

      std::map<std::string, int> str_map{{"a", 1}, {"b", 10}, {"c", 100}, {"d", 1000}};

      glz::write_binary(str_map, out);

      std::map<std::string, int> str_read;

      expect(!glz::read_binary(str_read, out));

      for (auto& item : str_map) {
         expect(str_read[item.first] == item.second);
      }

      out.clear();

      std::map<int, double> dbl_map{{1, 5.55}, {3, 7.34}, {8, 44.332}, {0, 0.000}};
      glz::write_binary(dbl_map, out);

      std::map<int, double> dbl_read{};
      expect(!glz::read_binary(dbl_read, out));

      for (auto& item : dbl_map) {
         expect(dbl_read[item.first] == item.second);
      }
   };

   "enum"_test = [] {
      Color color = Color::Green;
      std::vector<std::byte> buffer{};
      glz::write_binary(color, buffer);

      Color color_read = Color::Red;
      expect(!glz::read_binary(color_read, buffer));
      expect(color == color_read);
   };

   "complex user obect"_test = [] {
      std::vector<std::byte> buffer{};

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

      glz::write_binary(obj, buffer);

      Thing obj2{};
      expect(!glz::read_binary(obj2, buffer));

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

      std::vector<std::byte> buffer;
      // std::string buffer;

      auto tstart = std::chrono::high_resolution_clock::now();
      for (size_t i{}; i < repeat; ++i) {
         buffer.clear();
         glz::write_binary(thing, buffer);
      }
      auto tend = std::chrono::high_resolution_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::duration<double>>(tend - tstart).count();
      auto mbytes_per_sec = repeat * buffer.size() / (duration * 1048576);
      std::cout << "to_binary size: " << buffer.size() << " bytes\n";
      std::cout << "to_binary: " << duration << " s, " << mbytes_per_sec << " MB/s"
                << "\n";

      tstart = std::chrono::high_resolution_clock::now();
      for (size_t i{}; i < repeat; ++i) {
         expect(!glz::read_binary(thing, buffer));
      }
      tend = std::chrono::high_resolution_clock::now();
      duration = std::chrono::duration_cast<std::chrono::duration<double>>(tend - tstart).count();
      mbytes_per_sec = repeat * buffer.size() / (duration * 1048576);
      std::cout << "from_binary: " << duration << " s, " << mbytes_per_sec << " MB/s"
                << "\n";
   };
}

using namespace boost::ut;

suite binary_helpers = [] {
   "binary_helpers"_test = [] {
      my_struct v{22, 5.76, "ufo", {9, 5, 1}};

      std::string b;

      b = glz::write_binary(v);

      auto res = glz::read_binary<my_struct>(b);
      expect(bool(res));
      auto v2 = *res;

      expect(v2.i == 22);
      expect(v2.d == 5.76);
      expect(v2.hello == "ufo");
      expect(v2.arr == std::array<uint64_t, 3>{9, 5, 1});
   };
};

struct sub_t
{
   double x = 400.0;
   double y = 200.0;
};

template <>
struct glz::meta<sub_t>
{
   static constexpr std::string_view name = "sub";
   using T = sub_t;
   static constexpr auto value = object("x", &T::x, "y", &T::y);
};

struct some_struct
{
   int i = 287;
   double d = 3.14;
   Color c = Color::Red;
   std::string hello = "Hello World";
   std::array<uint64_t, 3> arr = {1, 2, 3};
   sub_t sub{};
   std::map<std::string, int> map{};
};

template <>
struct glz::meta<some_struct>
{
   static constexpr std::string_view name = "some_struct";
   using T = some_struct;
   static constexpr auto value = object(
      //"i", [](auto&& v) -> auto& { return v.i; },  //
      "i", &T::i, "d", &T::d, //
      "c", &T::c, "hello", &T::hello, //
      "arr", &T::arr, //
      "sub", &T::sub, //
      "map", &T::map //
   );
};

#include "glaze/api/impl.hpp"
#include "glaze/json/json_ptr.hpp"
#include "glaze/json/read.hpp"

void test_partial()
{
   expect(
      glz::name_v<glz::detail::member_tuple_t<some_struct>> ==
      R"(glz::tuplet::tuple<int32_t,double,Color,std::string,std::array<uint64_t,3>,sub,std::map<std::string,int32_t>>)");

   some_struct s{};
   some_struct s2{};
   std::string buffer = R"({"i":2,"map":{"fish":5,"cake":2,"bear":3}})";
   expect(glz::read_json(s, buffer) == false);

   std::vector<std::byte> out;
   static constexpr auto partial = glz::json_ptrs("/i", "/d", "/hello", "/sub/x", "/sub/y", "/map/fish", "/map/bear");

   static constexpr auto sorted = glz::sort_json_ptrs(partial);

   static constexpr auto groups = glz::group_json_ptrs<sorted>();

   static constexpr auto N = std::tuple_size_v<decltype(groups)>;
   glz::for_each<N>([&](auto I) {
      const auto group = glz::tuplet::get<I>(groups);
      std::cout << std::get<0>(group) << ": ";
      for (auto& rest : std::get<1>(group)) {
         std::cout << rest << ", ";
      }
      std::cout << '\n';
   });

   expect(!glz::write_binary<partial>(s, out));

   s2.i = 5;
   s2.hello = "text";
   s2.d = 5.5;
   s2.sub.x = 0.0;
   s2.sub.y = 20;
   expect(!glz::read_binary(s2, out));

   expect(s2.i == 2);
   expect(s2.d == 3.14);
   expect(s2.hello == "Hello World");
   expect(s2.sub.x == 400.0);
   expect(s2.sub.y == 200.0);
}

struct includer_struct
{
   std::string str = "Hello";
   int i = 55;
};

template <>
struct glz::meta<includer_struct>
{
   using T = includer_struct;
   static constexpr auto value = object("#include", glz::file_include{}, "str", &T::str, "i", &T::i);
};

void file_include_test()
{
   includer_struct obj{};

   expect(glz::write_file_binary(obj, "../alabastar.eve", std::string{}) == glz::error_code::none);

   obj.str = "";
   obj.i = 0;

   expect(glz::read_file_binary(obj, "../alabastar.eve", std::string{}) == glz::error_code::none);

   expect(obj.str == "Hello") << obj.str;
   expect(obj.i == 55) << obj.i;
}

void container_types()
{
   using namespace boost::ut;
   "vector int roundtrip"_test = [] {
      std::vector<int> vec(100);
      for (auto& item : vec) item = rand();
      std::string buffer{};
      std::vector<int> vec2{};
      glz::write_binary(vec, buffer);
      expect(!glz::read_binary(vec2, buffer));
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
      glz::write_binary(vec, buffer);
      expect(!glz::read_binary(vec2, buffer));
      expect(vec == vec2);
   };
   "vector double roundtrip"_test = [] {
      std::vector<double> vec(100);
      for (auto& item : vec) item = rand() / (1.0 + rand());
      std::string buffer{};
      std::vector<double> vec2{};
      glz::write_binary(vec, buffer);
      expect(!glz::read_binary(vec2, buffer));
      expect(vec == vec2);
   };
   "vector bool roundtrip"_test = [] {
      std::vector<bool> vec(100);
      for (auto&& item : vec) item = rand() / (1.0 + rand()) > 0.5;
      std::string buffer{};
      std::vector<bool> vec2{};
      glz::write_binary(vec, buffer);
      expect(!glz::read_binary(vec2, buffer));
      expect(vec == vec2);
   };
   "deque roundtrip"_test = [] {
      std::vector<int> deq(100);
      for (auto& item : deq) item = rand();
      std::string buffer{};
      std::vector<int> deq2{};
      glz::write_binary(deq, buffer);
      expect(!glz::read_binary(deq2, buffer));
      expect(deq == deq2);
   };
   "list roundtrip"_test = [] {
      std::list<int> lis(100);
      for (auto& item : lis) item = rand();
      std::string buffer{};
      std::list<int> lis2{};
      glz::write_binary(lis, buffer);
      expect(!glz::read_binary(lis2, buffer));
      expect(lis == lis2);
   };
   "map string keys roundtrip"_test = [] {
      std::map<std::string, int> map1;
      std::string str{"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"};
      std::mt19937 g{};
      for (auto i = 0; i < 20; ++i) {
         std::shuffle(str.begin(), str.end(), g);
         map1[str] = rand();
      }
      std::string buffer{};
      std::map<std::string, int> map2{};
      glz::write_binary(map1, buffer);
      expect(!glz::read_binary(map2, buffer));
      for (auto& it : map1) {
         expect(map2[it.first] == it.second);
      }
   };
   "map int keys roundtrip"_test = [] {
      std::map<int, int> map1;
      for (auto i = 0; i < 20; ++i) {
         map1[rand()] = rand();
      }
      std::string buffer{};
      std::map<int, int> map2{};
      glz::write_binary(map1, buffer);
      expect(!glz::read_binary(map2, buffer));
      for (auto& it : map1) {
         expect(map2[it.first] == it.second);
      }
   };
   "unordered_map int keys roundtrip"_test = [] {
      std::unordered_map<int, int> map1;
      for (auto i = 0; i < 20; ++i) {
         map1[rand()] = rand();
      }
      std::string buffer{};
      std::unordered_map<int, int> map2{};
      glz::write_binary(map1, buffer);
      expect(!glz::read_binary(map2, buffer));
      for (auto& it : map1) {
         expect(map2[it.first] == it.second);
      }
   };
   "tuple roundtrip"_test = [] {
      auto tuple1 = std::make_tuple(3, 2.7, std::string("curry"));
      decltype(tuple1) tuple2{};
      std::string buffer{};
      glz::write_binary(tuple1, buffer);
      expect(!glz::read_binary(tuple2, buffer));
      expect(tuple1 == tuple2);
   };
   "pair roundtrip"_test = [] {
      auto pair = std::make_pair(std::string("water"), 5.2);
      decltype(pair) pair2{};
      std::string buffer{};
      glz::write_binary(pair, buffer);
      expect(!glz::read_binary(pair2, buffer));
      expect(pair == pair2);
   };
}

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
      std::string s{};

      value_t v{};
      v.x = 5;
      glz::write_binary(v, s);
      v.x = 0;

      expect(!glz::read_binary(v, s));
      expect(v.x == 5);
   };

   "lambda value"_test = [] {
      std::string s{};

      lambda_value_t v{};
      v.x = 5;
      glz::write_binary(v, s);
      v.x = 0;

      expect(!glz::read_binary(v, s));
      expect(v.x == 5);
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
      glz::write_binary(msg, buffer);

      buffer.emplace_back('\0');

      msg.id = 0;
      msg.val = "";

      expect(!glz::read_binary(msg, buffer));
      expect(msg.id == 5);
      expect(msg.val == "hello");
   };

   "std::byte buffer"_test = [] {
      TestMsg msg{};
      msg.id = 5;
      msg.val = "hello";
      std::vector<std::byte> buffer{};
      glz::write_binary(msg, buffer);

      buffer.emplace_back(static_cast<std::byte>('\0'));

      msg.id = 0;
      msg.val = "";

      expect(!glz::read_binary(msg, buffer));
      expect(msg.id == 5);
      expect(msg.val == "hello");
   };

   "char8_t buffer"_test = [] {
      TestMsg msg{};
      msg.id = 5;
      msg.val = "hello";
      std::vector<char8_t> buffer{};
      glz::write_binary(msg, buffer);

      buffer.emplace_back('\0');

      msg.id = 0;
      msg.val = "";

      expect(!glz::read_binary(msg, buffer));
      expect(msg.id == 5);
      expect(msg.val == "hello");
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
      glz::write_binary(s, b);

      s.x = false;
      s.z = false;

      expect(!glz::read_binary(s, b));

      expect(s.x);
      expect(s.z);
   };
};

struct falcon0
{
   double d{};
};

template <>
struct glz::meta<falcon0>
{
   using T = falcon0;
   static constexpr auto value = object("d", &T::d);
};

struct falcon1
{
   int i{};
   double d{};
};

template <>
struct glz::meta<falcon1>
{
   using T = falcon1;
   static constexpr auto value = object("i", &T::i, "d", &T::d);
};

suite falcon_test = [] {
   "partial read"_test = [] {
      falcon0 f0{3.14};
      std::string s;
      glz::write_binary(f0, s);

      falcon1 f1{};
      expect(!glz::read_binary(f1, s));
      expect(f1.d == 3.14);
   };
};

int main()
{
   using namespace boost::ut;

   write_tests();
   bench();
   test_partial();
   file_include_test();
   container_types();
}
