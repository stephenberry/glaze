// Glaze Library
// For the license information refer to glaze.hpp

#define UT_RUN_TIME_ONLY

#include <bit>
#include <bitset>
#include <chrono>
#include <complex>
#include <deque>
#include <list>
#include <map>
#include <numbers>
#include <random>
#include <set>
#include <unordered_set>

#include "glaze/api/impl.hpp"
#include "glaze/bson/read.hpp"
#include "glaze/bson/write.hpp"
#include "glaze/json/json_ptr.hpp"
#include "glaze/json/read.hpp"
#include "glaze/trace/trace.hpp"
#include "ut/ut.hpp"

using namespace ut;

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
                                        "arr", &T::arr, //
                                        "include", glz::file_include{} //
   );
};

static_assert(glz::write_bson_supported<my_struct>);
static_assert(glz::read_bson_supported<my_struct>);

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
   static constexpr auto value = object("include", glz::file_include{}, //
                                        "a", &T::a, //
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

suite write_tests = []
{
   using namespace ut;

   "bool"_test = [] {
      {
         bool b = true;
         std::string out;
         expect(not glz::write_bson(b, out));
         bool b2{};
         expect(!glz::read_bson(b2, out));
         expect(b == b2);
      }
   };

   "float"_test = [] {
      {
         float f = 1.5f;
         std::string out;
         expect(not glz::write_bson(f, out));
         float f2{};
         expect(!glz::read_bson(f2, out));
         expect(f == f2);
      }
   };

   /*"string"_test = [] {
      {
         std::string s = "Hello World";
         std::string out;
         expect(not glz::write_bson(s, out));
         std::string s2{};
         expect(!glz::read_bson(s2, out));
         expect(s == s2);
      }
   };

   "array"_test = [] {
      {
         std::array<float, 3> arr = {1.2f, 3434.343f, 0.f};
         std::string out;
         expect(not glz::write_bson(arr, out));
         std::array<float, 3> arr2{};
         expect(!glz::read_bson(arr2, out));
         expect(arr == arr2);
      }
   };

   "vector"_test = [] {
      {
         std::vector<float> v = {1.2f, 3434.343f, 0.f};
         std::string out;
         expect(not glz::write_bson(v, out));
         std::vector<float> v2;
         expect(!glz::read_bson(v2, out));
         expect(v == v2);
      }
   };

   "my_struct"_test = [] {
      my_struct s{};
      s.i = 5;
      s.hello = "Wow!";
      std::string out;
      expect(not glz::write_bson(s, out));
      my_struct s2{};
      expect(!glz::read_bson(s2, out));
      expect(s.i == s2.i);
      expect(s.hello == s2.hello);
   };

   "nullable"_test = [] {
      std::string out;

      std::optional<int> op_int{};
      expect(not glz::write_bson(op_int, out));

      std::optional<int> new_op{};
      expect(!glz::read_bson(new_op, out));

      expect(op_int == new_op);

      op_int = 10;
      out.clear();

      expect(not glz::write_bson(op_int, out));
      expect(!glz::read_bson(new_op, out));

      expect(op_int == new_op);

      out.clear();

      std::shared_ptr<float> sh_float = std::make_shared<float>(5.55f);
      expect(not glz::write_bson(sh_float, out));

      std::shared_ptr<float> out_flt;
      expect(!glz::read_bson(out_flt, out));

      expect(*sh_float == *out_flt);

      out.clear();

      std::unique_ptr<double> uni_dbl = std::make_unique<double>(5.55);
      expect(not glz::write_bson(uni_dbl, out));

      std::shared_ptr<double> out_dbl;
      expect(!glz::read_bson(out_dbl, out));

      expect(*uni_dbl == *out_dbl);
   };

   "map"_test = [] {
      std::string out;

      std::map<std::string, int> str_map{{"a", 1}, {"b", 10}, {"c", 100}, {"d", 1000}};

      expect(not glz::write_bson(str_map, out));

      std::map<std::string, int> str_read;

      expect(!glz::read_bson(str_read, out));

      for (auto& item : str_map) {
         expect(str_read[item.first] == item.second);
      }

      out.clear();

      std::map<int, double> dbl_map{{1, 5.55}, {3, 7.34}, {8, 44.332}, {0, 0.000}};
      expect(not glz::write_bson(dbl_map, out));

      std::map<int, double> dbl_read{};
      expect(!glz::read_bson(dbl_read, out));

      for (auto& item : dbl_map) {
         expect(dbl_read[item.first] == item.second);
      }
   };

   "enum"_test = [] {
      Color color = Color::Green;
      std::string buffer{};
      expect(not glz::write_bson(color, buffer));

      Color color_read = Color::Red;
      expect(!glz::read_bson(color_read, buffer));
      expect(color == color_read);
   };

   "complex user obect"_test = [] {
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

      expect(not glz::write_bson(obj, buffer));

      Thing obj2{};
      expect(!glz::read_bson(obj2, buffer));

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
   };*/
};

int main()
{
   return 0;
}
