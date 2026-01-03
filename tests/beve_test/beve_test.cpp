// Glaze Library
// For the license information refer to glaze.hpp

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
#include <span>
#include <unordered_map>
#include <unordered_set>

#include "glaze/api/impl.hpp"
#include "glaze/base64/base64.hpp"
#include "glaze/beve/beve_to_json.hpp"
#include "glaze/beve/key_traits.hpp"
#include "glaze/beve/read.hpp"
#include "glaze/beve/write.hpp"
#include "glaze/hardware/volatile_array.hpp"
#include "glaze/json/json_ptr.hpp"
#include "glaze/json/read.hpp"
#include "glaze/trace/trace.hpp"
#include "ut/ut.hpp"

using namespace ut;

inline glz::trace trace{};

struct ModuleID
{
   uint64_t value{};

   auto operator<=>(const ModuleID&) const = default;
};

template <>
struct glz::meta<ModuleID>
{
   static constexpr auto value = &ModuleID::value;
};

struct CastModuleID
{
   uint64_t value{};

   auto operator<=>(const CastModuleID&) const = default;
};

template <>
struct glz::meta<CastModuleID>
{
   static constexpr auto value = glz::cast<&CastModuleID::value, uint64_t>;
};

template <>
struct std::hash<ModuleID>
{
   size_t operator()(const ModuleID& id) const noexcept { return std::hash<uint64_t>{}(id.value); }
};

struct beve_concat_opts : glz::opts
{
   bool concatenate = true;
};

template <>
struct std::hash<CastModuleID>
{
   size_t operator()(const CastModuleID& id) const noexcept { return std::hash<uint64_t>{}(id.value); }
};

namespace
{
   template <class ID>
   constexpr ID make_id(const uint64_t value)
   {
      return ID{value};
   }

   template <class ID>
   void verify_map_roundtrip()
   {
      const std::map<ID, std::string> src{{make_id<ID>(42), "life"}, {make_id<ID>(9001), "power"}};

      std::string buffer{};
      expect(not glz::write_beve(src, buffer));

      expect(static_cast<uint8_t>(buffer[0]) == glz::beve_key_traits<ID>::header);

      std::map<ID, std::string> dst{};
      expect(!glz::read_beve(dst, buffer));
      expect(dst == src);

      std::string json{};
      expect(!glz::beve_to_json(buffer, json));
      expect(json == R"({"42":"life","9001":"power"})") << json;
   }

   template <class ID>
   void verify_unordered_map_roundtrip()
   {
      const std::unordered_map<ID, int> src{
         {make_id<ID>(1), 7},
         {make_id<ID>(2), 11},
         {make_id<ID>(99), -4},
      };

      std::string buffer{};
      expect(not glz::write_beve(src, buffer));

      std::unordered_map<ID, int> dst{};
      expect(!glz::read_beve(dst, buffer));
      expect(dst == src);

      std::string json{};
      expect(!glz::beve_to_json(buffer, json));

      std::map<std::string, int> decoded{};
      expect(!glz::read_json(decoded, json));
      expect(decoded == std::map<std::string, int>{{"1", 7}, {"2", 11}, {"99", -4}});
   }

   template <class ID>
   void verify_no_header_raw_bytes()
   {
      ID id{0x1122334455667788ULL};

      std::string buffer{};
      size_t ix{};
      glz::context ctx{};

      glz::serialize<glz::BEVE>::no_header<glz::opts{}>(id, ctx, buffer, ix);

      expect(ix == sizeof(uint64_t));
      expect(buffer.size() >= ix);

      uint64_t raw{};
      std::memcpy(&raw, buffer.data(), sizeof(raw));
      // BEVE uses little-endian wire format, so on big-endian systems
      // the memcpy'd value needs to be byteswapped to match the original
      if constexpr (std::endian::native == std::endian::big) {
         raw = std::byteswap(raw);
      }
      expect(raw == id.value);
   }

   template <class ID>
   void verify_vector_pair_roundtrip()
   {
      constexpr beve_concat_opts beve_concat{{glz::BEVE}};
      const std::vector<std::pair<ID, int>> src{{make_id<ID>(5), 13}, {make_id<ID>(7), 17}};

      std::string buffer{};
      expect(not glz::write<beve_concat>(src, buffer));

      std::vector<std::pair<ID, int>> dst{};
      expect(!glz::read<beve_concat>(dst, buffer));
      expect(dst == src);

      std::string json{};
      expect(!glz::beve_to_json(buffer, json));
      expect(json == R"({"5":13,"7":17})") << json;
   }
}

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

static_assert(glz::write_supported<my_struct, glz::BEVE>);
static_assert(glz::read_supported<my_struct, glz::BEVE>);

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

   Thing() : thing_ptr(&thing) {};
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

// Custom nullable type for testing nullable_value_t support
struct custom_nullable_value
{
   std::optional<double> val{};

   bool has_value() const { return val.has_value(); }

   double& value() { return *val; }

   const double& value() const { return *val; }

   void emplace() { val.emplace(); }

   void reset() { val.reset(); }
};

struct nullable_value_test_struct
{
   custom_nullable_value x{};
   int y = 42;
};

// Test struct for issue #1326
struct test_skip
{
   std::optional<char> o_;
};

// Test structs for nested skip_null_members
struct inner_skip_struct
{
   std::optional<int> inner_opt1{};
   int inner_value = 100;
   std::optional<double> inner_opt2{};
};

struct outer_skip_struct
{
   std::optional<std::string> outer_opt1{};
   inner_skip_struct nested{};
   int outer_value = 200;
   std::optional<bool> outer_opt2{};
};

void write_tests()
{
   using namespace ut;

   "round_trip"_test = [] {
      {
         std::string s;
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
         std::string out;
         expect(not glz::write_beve(b, out));
         bool b2{};
         expect(!glz::read_beve(b2, out));
         expect(b == b2);
      }
   };

   "float"_test = [] {
      {
         float f = 1.5f;
         std::string out;
         expect(not glz::write_beve(f, out));
         float f2{};
         expect(!glz::read_beve(f2, out));
         expect(f == f2);
      }
   };

   "string"_test = [] {
      {
         std::string s = "Hello World";
         std::string out;
         expect(not glz::write_beve(s, out));
         std::string s2{};
         expect(!glz::read_beve(s2, out));
         expect(s == s2);
      }
   };

   "array"_test = [] {
      {
         std::array<float, 3> arr = {1.2f, 3434.343f, 0.f};
         std::string out;
         expect(not glz::write_beve(arr, out));
         std::array<float, 3> arr2{};
         expect(!glz::read_beve(arr2, out));
         expect(arr == arr2);
      }
   };

   "vector"_test = [] {
      {
         std::vector<float> v = {1.2f, 3434.343f, 0.f};
         std::string out;
         expect(not glz::write_beve(v, out));
         std::vector<float> v2;
         expect(!glz::read_beve(v2, out));
         expect(v == v2);
      }
   };

   "my_struct"_test = [] {
      my_struct s{};
      s.i = 5;
      s.hello = "Wow!";
      std::string out;
      expect(not glz::write_beve(s, out));
      my_struct s2{};
      expect(!glz::read_beve(s2, out));
      expect(s.i == s2.i);
      expect(s.hello == s2.hello);
   };

   "nullable"_test = [] {
      std::string out;

      std::optional<int> op_int{};
      expect(not glz::write_beve(op_int, out));

      std::optional<int> new_op{};
      expect(!glz::read_beve(new_op, out));

      expect(op_int == new_op);

      op_int = 10;
      out.clear();

      expect(not glz::write_beve(op_int, out));
      expect(!glz::read_beve(new_op, out));

      expect(op_int == new_op);

      out.clear();

      std::shared_ptr<float> sh_float = std::make_shared<float>(5.55f);
      expect(not glz::write_beve(sh_float, out));

      std::shared_ptr<float> out_flt;
      expect(!glz::read_beve(out_flt, out));

      expect(*sh_float == *out_flt);

      out.clear();

      std::unique_ptr<double> uni_dbl = std::make_unique<double>(5.55);
      expect(not glz::write_beve(uni_dbl, out));

      std::shared_ptr<double> out_dbl;
      expect(!glz::read_beve(out_dbl, out));

      expect(*uni_dbl == *out_dbl);
   };

   "nullable_value_t"_test = [] {
      std::string out;

      // Test with value
      nullable_value_test_struct obj{};
      obj.x.val = 3.14;
      expect(not glz::write_beve(obj, out));

      nullable_value_test_struct obj2{};
      expect(!glz::read_beve(obj2, out));
      expect(obj2.x.has_value());
      expect(obj2.x.value() == 3.14);
      expect(obj2.y == 42);

      // Test with null (using skip_null_members = false to ensure null is written)
      out.clear();
      obj.x.val = {};
      expect(not glz::write<glz::opts{.format = glz::BEVE, .skip_null_members = false}>(obj, out));

      nullable_value_test_struct obj3{};
      obj3.x.val = 99.9; // Set a value to ensure it gets reset
      expect(!glz::read_beve(obj3, out));
      expect(!obj3.x.has_value());
      expect(obj3.y == 42);

      // Test standalone nullable_value_t
      out.clear();
      custom_nullable_value standalone{};
      standalone.val = 2.71;
      expect(not glz::write_beve(standalone, out));

      custom_nullable_value standalone2{};
      expect(!glz::read_beve(standalone2, out));
      expect(standalone2.has_value());
      expect(standalone2.value() == 2.71);

      // Test standalone null
      out.clear();
      standalone.val = {};
      expect(not glz::write_beve(standalone, out));

      standalone2.val = 1.0; // Set a value to ensure it gets reset
      expect(!glz::read_beve(standalone2, out));
      expect(!standalone2.has_value());
   };

   // Test for issue #1326: BEVE should skip null members like JSON does
   "issue_1326_skip_null_members"_test = [] {
      std::vector<test_skip> a{{}, {}};
      std::string json_buffer;
      std::vector<char> beve_buffer;

      auto json_err = glz::write_json(a, json_buffer);
      auto beve_err = glz::write_beve(a, beve_buffer);
      expect(!json_err && !beve_err);

      std::array<test_skip, 2> b{{{false}, {true}}};
      auto beve_b{b};

      auto json_err2 = glz::read_json(b, json_buffer);
      auto beve_err2 = glz::read_beve(beve_b, beve_buffer);
      expect(!json_err2 && !beve_err2);

      // Both should handle empty optionals the same way
      expect(b[0].o_ == beve_b[0].o_);
   };

   "nested_skip_null_members"_test = [] {
      std::string json_buffer;
      std::vector<char> beve_buffer;

      // Test 1: All optionals are null (should skip all of them)
      {
         outer_skip_struct obj1{};
         // All optionals are already std::nullopt by default

         auto json_err = glz::write_json(obj1, json_buffer);
         auto beve_err = glz::write_beve(obj1, beve_buffer);
         expect(!json_err && !beve_err);

         // Initialize with sentinel values to verify they DON'T change (proving skipping worked)
         outer_skip_struct json_obj1{};
         json_obj1.outer_opt1 = "should_not_change";
         json_obj1.outer_opt2 = true;
         json_obj1.nested.inner_opt1 = 9999;
         json_obj1.nested.inner_opt2 = 99.99;

         outer_skip_struct beve_obj1{};
         beve_obj1.outer_opt1 = "should_not_change";
         beve_obj1.outer_opt2 = true;
         beve_obj1.nested.inner_opt1 = 9999;
         beve_obj1.nested.inner_opt2 = 99.99;

         auto json_err2 = glz::read_json(json_obj1, json_buffer);
         auto beve_err2 = glz::read_beve(beve_obj1, beve_buffer);
         expect(!json_err2 && !beve_err2);

         // Verify both formats skip null members the same way - sentinel values should remain
         expect(json_obj1.outer_opt1 == beve_obj1.outer_opt1);
         expect(json_obj1.outer_opt1.value() == "should_not_change");
         expect(json_obj1.outer_opt2 == beve_obj1.outer_opt2);
         expect(json_obj1.outer_opt2.value() == true);
         expect(json_obj1.nested.inner_opt1 == beve_obj1.nested.inner_opt1);
         expect(json_obj1.nested.inner_opt1.value() == 9999);
         expect(json_obj1.nested.inner_opt2 == beve_obj1.nested.inner_opt2);
         expect(json_obj1.nested.inner_opt2.value() == 99.99);

         // Non-optional values should have been updated
         expect(json_obj1.outer_value == 200);
         expect(beve_obj1.outer_value == 200);
         expect(json_obj1.nested.inner_value == 100);
         expect(beve_obj1.nested.inner_value == 100);
      }

      // Test 2: Some optionals have values in both inner and outer
      {
         json_buffer.clear();
         beve_buffer.clear();

         outer_skip_struct obj2{};
         obj2.outer_opt1 = "outer_string";
         obj2.nested.inner_opt1 = 42;
         // outer_opt2 and inner_opt2 remain null (will be skipped)

         auto json_err = glz::write_json(obj2, json_buffer);
         auto beve_err = glz::write_beve(obj2, beve_buffer);
         expect(!json_err && !beve_err);

         // Initialize all optionals with sentinel values
         outer_skip_struct json_obj2{};
         json_obj2.outer_opt1 = "will_be_replaced";
         json_obj2.outer_opt2 = false; // Sentinel - should not change
         json_obj2.nested.inner_opt1 = 7777;
         json_obj2.nested.inner_opt2 = 77.77; // Sentinel - should not change

         outer_skip_struct beve_obj2{};
         beve_obj2.outer_opt1 = "will_be_replaced";
         beve_obj2.outer_opt2 = false; // Sentinel - should not change
         beve_obj2.nested.inner_opt1 = 7777;
         beve_obj2.nested.inner_opt2 = 77.77; // Sentinel - should not change

         auto json_err2 = glz::read_json(json_obj2, json_buffer);
         auto beve_err2 = glz::read_beve(beve_obj2, beve_buffer);
         expect(!json_err2 && !beve_err2);

         // Verify written values were updated
         expect(json_obj2.outer_opt1 == beve_obj2.outer_opt1);
         expect(json_obj2.outer_opt1.value() == "outer_string");
         expect(json_obj2.nested.inner_opt1 == beve_obj2.nested.inner_opt1);
         expect(json_obj2.nested.inner_opt1.value() == 42);

         // Verify null fields were skipped - sentinel values should remain
         expect(json_obj2.outer_opt2 == beve_obj2.outer_opt2);
         expect(json_obj2.outer_opt2.value() == false);
         expect(json_obj2.nested.inner_opt2 == beve_obj2.nested.inner_opt2);
         expect(json_obj2.nested.inner_opt2.value() == 77.77);
      }

      // Test 3: All optionals have values
      {
         json_buffer.clear();
         beve_buffer.clear();

         outer_skip_struct obj3{};
         obj3.outer_opt1 = "test";
         obj3.outer_opt2 = true;
         obj3.nested.inner_opt1 = 999;
         obj3.nested.inner_opt2 = 3.14159;

         auto json_err = glz::write_json(obj3, json_buffer);
         auto beve_err = glz::write_beve(obj3, beve_buffer);
         expect(!json_err && !beve_err);

         // Initialize with different sentinel values - all should be replaced
         outer_skip_struct json_obj3{};
         json_obj3.outer_opt1 = "sentinel1";
         json_obj3.outer_opt2 = false;
         json_obj3.nested.inner_opt1 = 5555;
         json_obj3.nested.inner_opt2 = 55.55;

         outer_skip_struct beve_obj3{};
         beve_obj3.outer_opt1 = "sentinel1";
         beve_obj3.outer_opt2 = false;
         beve_obj3.nested.inner_opt1 = 5555;
         beve_obj3.nested.inner_opt2 = 55.55;

         auto json_err2 = glz::read_json(json_obj3, json_buffer);
         auto beve_err2 = glz::read_beve(beve_obj3, beve_buffer);
         expect(!json_err2 && !beve_err2);

         // Verify all values were replaced with the serialized values
         expect(json_obj3.outer_opt1 == beve_obj3.outer_opt1);
         expect(json_obj3.outer_opt1.value() == "test");
         expect(json_obj3.outer_opt2 == beve_obj3.outer_opt2);
         expect(json_obj3.outer_opt2.value() == true);
         expect(json_obj3.nested.inner_opt1 == beve_obj3.nested.inner_opt1);
         expect(json_obj3.nested.inner_opt1.value() == 999);
         expect(json_obj3.nested.inner_opt2 == beve_obj3.nested.inner_opt2);
         expect(json_obj3.nested.inner_opt2.value() == 3.14159);
      }
   };

   "map"_test = [] {
      std::string out;

      std::map<std::string, int> str_map{{"a", 1}, {"b", 10}, {"c", 100}, {"d", 1000}};

      expect(not glz::write_beve(str_map, out));

      std::map<std::string, int> str_read;

      expect(!glz::read_beve(str_read, out));

      for (auto& item : str_map) {
         expect(str_read[item.first] == item.second);
      }

      out.clear();

      std::map<int, double> dbl_map{{1, 5.55}, {3, 7.34}, {8, 44.332}, {0, 0.000}};
      expect(not glz::write_beve(dbl_map, out));

      std::map<int, double> dbl_read{};
      expect(!glz::read_beve(dbl_read, out));

      for (auto& item : dbl_map) {
         expect(dbl_read[item.first] == item.second);
      }
   };

   "enum"_test = [] {
      Color color = Color::Green;
      std::string buffer{};
      expect(not glz::write_beve(color, buffer));

      Color color_read = Color::Red;
      expect(!glz::read_beve(color_read, buffer));
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

      expect(not glz::write<glz::opts{.format = glz::BEVE, .skip_null_members = false}>(obj, buffer));

      Thing obj2{};
      expect(!glz::read_beve(obj2, buffer));

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
      // std::string buffer;

      auto tstart = std::chrono::high_resolution_clock::now();
      for (size_t i{}; i < repeat; ++i) {
         buffer.clear();
         expect(not glz::write_beve(thing, buffer));
      }
      auto tend = std::chrono::high_resolution_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::duration<double>>(tend - tstart).count();
      auto mbytes_per_sec = repeat * buffer.size() / (duration * 1048576);
      std::cout << "to_beve size: " << buffer.size() << " bytes\n";
      std::cout << "to_beve: " << duration << " s, " << mbytes_per_sec << " MB/s"
                << "\n";

      tstart = std::chrono::high_resolution_clock::now();
      for (size_t i{}; i < repeat; ++i) {
         expect(!glz::read_beve(thing, buffer));
      }
      tend = std::chrono::high_resolution_clock::now();
      duration = std::chrono::duration_cast<std::chrono::duration<double>>(tend - tstart).count();
      mbytes_per_sec = repeat * buffer.size() / (duration * 1048576);
      std::cout << "from_beve: " << duration << " s, " << mbytes_per_sec << " MB/s"
                << "\n";
      trace.end("bench");
   };
}

using namespace ut;

suite beve_helpers = [] {
   "beve_helpers"_test = [] {
      my_struct v{22, 5.76, "ufo", {9, 5, 1}};

      std::string b;

      b = glz::write_beve(v).value_or("error");

      auto res = glz::read_beve<my_struct>(b);
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

void test_partial()
{
   expect(glz::name_v<glz::detail::member_tuple_t<some_struct>> ==
          R"(glz::tuple<int32_t,double,Color,std::string,std::array<uint64_t,3>,sub,std::map<std::string,int32_t>>)");

   some_struct s{};
   some_struct s2{};
   std::string buffer = R"({"i":2,"map":{"fish":5,"cake":2,"bear":3}})";
   expect(glz::read_json(s, buffer) == false);

   std::string out;
   static constexpr auto partial = glz::json_ptrs("/i", "/d", "/hello", "/sub/x", "/sub/y", "/map/fish", "/map/bear");

   static constexpr auto sorted = glz::sort_json_ptrs(partial);

   static constexpr auto groups = glz::group_json_ptrs<sorted>();

   static constexpr auto N = glz::tuple_size_v<decltype(groups)>;
   glz::for_each<N>([&]<auto I>() {
      const auto group = glz::get<I>(groups);
      std::cout << glz::get<0>(group) << ": ";
      for (auto& rest : glz::get<1>(group)) {
         std::cout << rest << ", ";
      }
      std::cout << '\n';
   });

   expect(!glz::write_beve<partial>(s, out));

   s2.i = 5;
   s2.hello = "text";
   s2.d = 5.5;
   s2.sub.x = 0.0;
   s2.sub.y = 20;
   expect(!glz::read_beve(s2, out));

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
   bool j{false};
};

template <>
struct glz::meta<includer_struct>
{
   using T = includer_struct;
   static constexpr auto value = object("include", glz::file_include{}, "str", &T::str, "i", &T::i, "j", &T::j);
};

static_assert(glz::is_includer<glz::includer<includer_struct>>);

void file_include_test()
{
   includer_struct obj{};

   expect(glz::write_file_beve(obj, "../alabastar.beve", std::string{}) == glz::error_code::none);

   obj.str = "";
   obj.i = 0;
   obj.j = true;

   expect(glz::read_file_beve(obj, "../alabastar.beve", std::string{}) == glz::error_code::none);

   expect(obj.str == "Hello") << obj.str;
   expect(obj.i == 55) << obj.i;
   expect(obj.j == false) << obj.j;
}

void container_types()
{
   using namespace ut;
   "vector int roundtrip"_test = [] {
      std::vector<int> vec(100);
      for (auto& item : vec) item = rand();
      std::string buffer{};
      std::vector<int> vec2{};
      expect(not glz::write_beve(vec, buffer));
      expect(!glz::read_beve(vec2, buffer));
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
      expect(not glz::write_beve(vec, buffer));
      expect(!glz::read_beve(vec2, buffer));
      expect(vec == vec2);
   };
   "vector double roundtrip"_test = [] {
      std::vector<double> vec(100);
      for (auto& item : vec) item = rand() / (1.0 + rand());
      std::string buffer{};
      std::vector<double> vec2{};
      expect(not glz::write_beve(vec, buffer));
      expect(!glz::read_beve(vec2, buffer));
      expect(vec == vec2);
   };
   "vector bool roundtrip"_test = [] {
      std::vector<bool> vec(100);
      for (auto&& item : vec) item = rand() / (1.0 + rand()) > 0.5;
      std::string buffer{};
      std::vector<bool> vec2{};
      expect(not glz::write_beve(vec, buffer));
      expect(!glz::read_beve(vec2, buffer));
      expect(vec == vec2);
   };
   "deque roundtrip"_test = [] {
      std::vector<int> deq(100);
      for (auto& item : deq) item = rand();
      std::string buffer{};
      std::vector<int> deq2{};
      expect(not glz::write_beve(deq, buffer));
      expect(!glz::read_beve(deq2, buffer));
      expect(deq == deq2);
   };
   "list roundtrip"_test = [] {
      std::list<int> lis(100);
      for (auto& item : lis) item = rand();
      std::string buffer{};
      std::list<int> lis2{};
      expect(not glz::write_beve(lis, buffer));
      expect(!glz::read_beve(lis2, buffer));
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
      expect(not glz::write_beve(map1, buffer));
      expect(!glz::read_beve(map2, buffer));
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
      expect(not glz::write_beve(map1, buffer));
      expect(!glz::read_beve(map2, buffer));
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
      expect(not glz::write_beve(map1, buffer));
      expect(!glz::read_beve(map2, buffer));
      for (auto& it : map1) {
         expect(map2[it.first] == it.second);
      }
   };
   "tuple roundtrip"_test = [] {
      auto tuple1 = std::make_tuple(3, 2.7, std::string("curry"));
      decltype(tuple1) tuple2{};
      std::string buffer{};
      expect(not glz::write_beve(tuple1, buffer));
      expect(!glz::read_beve(tuple2, buffer));
      expect(tuple1 == tuple2);
   };
   "pair roundtrip"_test = [] {
      auto pair = std::make_pair(std::string("water"), 5.2);
      decltype(pair) pair2{};
      std::string buffer{};
      expect(not glz::write_beve(pair, buffer));
      expect(!glz::read_beve(pair2, buffer));
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
      expect(not glz::write_beve(v, s));
      v.x = 0;

      expect(!glz::read_beve(v, s));
      expect(v.x == 5);
   };

   "lambda value"_test = [] {
      std::string s{};

      lambda_value_t v{};
      v.x = 5;
      expect(not glz::write_beve(v, s));
      v.x = 0;

      expect(!glz::read_beve(v, s));
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
   "std::byte buffer"_test = [] {
      TestMsg msg{};
      msg.id = 5;
      msg.val = "hello";
      std::vector<std::byte> buffer{};
      expect(not glz::write_beve(msg, buffer));

      buffer.emplace_back(static_cast<std::byte>('\0'));

      msg.id = 0;
      msg.val = "";

      expect(!glz::read_beve(msg, buffer));
      expect(msg.id == 5);
      expect(msg.val == "hello");
   };

   "uint8_t buffer"_test = [] {
      TestMsg msg{};
      msg.id = 5;
      msg.val = "hello";
      std::vector<uint8_t> buffer{};
      expect(not glz::write_beve(msg, buffer));

      buffer.emplace_back('\0');

      msg.id = 0;
      msg.val = "";

      expect(!glz::read_beve(msg, buffer));
      expect(msg.id == 5);
      expect(msg.val == "hello");
   };

   "std::string buffer"_test = [] {
      TestMsg msg{};
      msg.id = 5;
      msg.val = "hello";
      std::string buffer{};
      expect(not glz::write_beve(msg, buffer));

      msg.id = 0;
      msg.val = "";

      expect(!glz::read_beve(msg, buffer));
      expect(msg.id == 5);
      expect(msg.val == "hello");
   };

   "char8_t buffer"_test = [] {
      TestMsg msg{};
      msg.id = 5;
      msg.val = "hello";
      std::vector<char8_t> buffer{};
      expect(not glz::write_beve(msg, buffer));

      buffer.emplace_back('\0');

      msg.id = 0;
      msg.val = "";

      expect(!glz::read_beve(msg, buffer));
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
      expect(not glz::write_beve(s, b));

      s.x = false;
      s.z = false;

      expect(!glz::read_beve(s, b));

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
      expect(not glz::write_beve(f0, s));

      falcon1 f1{};
      expect(!glz::read_beve(f1, s));
      expect(f1.d == 3.14);
   };
};

suite complex_test = [] {
   "std::complex"_test = [] {
      std::complex<double> c{1.0, 0.5};
      std::string s{};
      expect(not glz::write_beve(c, s));

      c = {0.0, 0.0};
      expect(!glz::read_beve(c, s));
      expect(c.real() == 1.0);
      expect(c.imag() == 0.5);
   };

   "std::vector<std::complex<double>>"_test = [] {
      std::vector<std::complex<double>> vc = {{1.0, 0.5}, {2.0, 1.0}, {3.0, 1.5}};
      std::string s{};
      expect(not glz::write_beve(vc, s));

      vc.clear();
      expect(!glz::read_beve(vc, s));
      expect(vc[0] == std::complex{1.0, 0.5});
      expect(vc[1] == std::complex{2.0, 1.0});
      expect(vc[2] == std::complex{3.0, 1.5});
   };

   "std::vector<std::complex<float>>"_test = [] {
      std::vector<std::complex<float>> vc = {{1.0f, 0.5f}, {2.0f, 1.0f}, {3.0f, 1.5f}};
      std::string s{};
      expect(not glz::write_beve(vc, s));

      vc.clear();
      expect(!glz::read_beve(vc, s));
      expect(vc[0] == std::complex{1.0f, 0.5f});
      expect(vc[1] == std::complex{2.0f, 1.0f});
      expect(vc[2] == std::complex{3.0f, 1.5f});
   };
};

struct skipper
{
   int a = 4;
   std::string s = "Aha!";
};

template <>
struct glz::meta<skipper>
{
   using T = skipper;
   static constexpr auto value = object("a", &T::a, "pi", skip{}, "s", &T::s);
};

struct full
{
   int a = 10;
   double pi = 3.14;
   std::string s = "full";
};

template <>
struct glz::meta<full>
{
   using T = full;
   static constexpr auto value = object("a", &T::a, "pi", &T::pi, "s", &T::s);
};

struct nothing
{
   int a{};

   struct glaze
   {
      static constexpr auto value = glz::object("a", &nothing::a);
   };
};

suite skip_test = [] {
   "skip"_test = [] {
      full f{};
      std::string s{};
      expect(not glz::write_beve(f, s));

      skipper obj{};
      expect(!glz::read_beve(obj, s));
      expect(obj.a == 10);
      expect(obj.s == "full");
   };

   "no error on unknown keys"_test = [] {
      full f{};
      std::string s{};
      expect(not glz::write_beve(f, s));

      nothing obj{};
      expect(!glz::read<glz::opts{.format = glz::BEVE, .error_on_unknown_keys = false}>(obj, s));
   };
};

suite set_tests = [] {
   "unordered_set<string>"_test = [] {
      std::unordered_set<std::string> set{"one", "two", "three"};

      std::string s{};
      expect(not glz::write_beve(set, s));

      set.clear();

      expect(!glz::read_beve(set, s));
      expect(set.contains("one"));
      expect(set.contains("two"));
      expect(set.contains("three"));
   };

   "unordered_set<uint32_t>"_test = [] {
      std::unordered_set<uint32_t> set{0, 1, 2};

      std::string s{};
      expect(not glz::write_beve(set, s));

      set.clear();

      expect(!glz::read_beve(set, s));
      expect(set.contains(0));
      expect(set.contains(1));
      expect(set.contains(2));
   };

   "set<string>"_test = [] {
      std::set<std::string> set{"one", "two", "three"};

      std::string s{};
      expect(not glz::write_beve(set, s));

      set.clear();

      expect(!glz::read_beve(set, s));
      expect(set.contains("one"));
      expect(set.contains("two"));
      expect(set.contains("three"));
   };

   "set<uint32_t>"_test = [] {
      std::set<uint32_t> set{0, 1, 2};

      std::string s{};
      expect(not glz::write_beve(set, s));

      set.clear();

      expect(!glz::read_beve(set, s));
      expect(set.contains(0));
      expect(set.contains(1));
      expect(set.contains(2));
   };
};

suite bitset = [] {
   "bitset"_test = [] {
      std::bitset<8> b = 0b10101010;

      std::string s{};
      expect(not glz::write_beve(b, s));

      b.reset();
      expect(!glz::read_beve(b, s));
      expect(b == 0b10101010);
   };

   "bitset16"_test = [] {
      std::bitset<16> b = 0b10010010'00000010;

      std::string s{};
      expect(not glz::write_beve(b, s));

      b.reset();
      expect(!glz::read_beve(b, s));
      expect(b == 0b10010010'00000010);
   };
};

suite array_bool_tests = [] {
   "array_bool_13"_test = [] {
      std::array<bool, 13> arr = {true, false, true, true, false, false, true, false, true, false, true, true, false};

      std::string s{};
      expect(not glz::write_beve(arr, s));

      std::array<bool, 13> arr2{};
      expect(!glz::read_beve(arr2, s));
      expect(arr == arr2);
   };

   "array_bool_8"_test = [] {
      std::array<bool, 8> arr = {true, false, true, false, true, false, true, false};

      std::string s{};
      expect(not glz::write_beve(arr, s));

      std::array<bool, 8> arr2{};
      expect(!glz::read_beve(arr2, s));
      expect(arr == arr2);
   };

   "array_bool_1"_test = [] {
      std::array<bool, 1> arr = {true};

      std::string s{};
      expect(not glz::write_beve(arr, s));

      std::array<bool, 1> arr2{};
      expect(!glz::read_beve(arr2, s));
      expect(arr == arr2);
   };
};

struct nested_bool_array
{
   int id{};
   std::array<bool, 13> flags{};
   std::string name{};

   bool operator==(const nested_bool_array&) const = default;
};

suite nested_array_bool_tests = [] {
   "nested_array_bool"_test = [] {
      nested_bool_array obj{
         42, {true, false, true, true, false, false, true, false, true, false, true, true, false}, "test"};

      std::string s{};
      expect(not glz::write_beve(obj, s));

      nested_bool_array obj2{};
      expect(!glz::read_beve(obj2, s));
      expect(obj == obj2);
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
      expect(not glz::write_beve(obj, s));

      obj.i = 0;
      obj.d = 0;
      obj.hello = "";
      obj.arr = {};
      expect(!glz::read_beve(obj, s));

      expect(obj.i == 287);
      expect(obj.d == 3.14);
      expect(obj.hello == "Hello World");
      expect(obj.arr == std::array<uint64_t, 3>{1, 2, 3});
   };
};

struct header_t
{
   bool valid{};
   std::string description{};

   struct glaze
   {
      using T = header_t;
      static constexpr auto value = glz::object(&T::valid, &T::description);
   };
};

struct signal_t
{
   header_t header{};
   std::vector<double> v_f64;
   std::vector<uint8_t> v_u8;

   struct glaze
   {
      using T = signal_t;
      static constexpr auto value = glz::object(&T::header, &T::v_f64, &T::v_u8);
   };
};

suite signal_tests = [] {
   "signal"_test = [] {
      std::string s;
      signal_t obj{{true, "header description"}, {1.0, 2.0}, {1, 2, 3, 4, 5}};
      expect(not glz::write_beve(obj, s));

      obj = {};
      expect(!glz::read_beve(obj, s));

      expect(obj.header.valid == true);
      expect(obj.header.description == "header description");
      expect(obj.v_f64 == std::vector{1.0, 2.0});
      expect(obj.v_u8 == std::vector<uint8_t>{1, 2, 3, 4, 5});
   };
};

suite vector_tests = [] {
   "std::vector<uint8_t>"_test = [] {
      auto scoped = trace.scope("test std::vector<uint8_t>");
      std::string s;
      static constexpr auto n = 10000;
      std::vector<uint8_t> v(n);

      std::mt19937_64 gen{};

      for (auto i = 0; i < n; ++i) {
         v[i] = uint8_t(std::uniform_int_distribution<uint16_t>{0, 255}(gen));
      }

      auto copy = v;

      expect(not glz::write_beve(v, s));

      v.clear();

      expect(!glz::read_beve(v, s));

      expect(v == copy);
   };

   "std::vector<uint16_t>"_test = [] {
      auto scoped = trace.scope("test std::vector<uint16_t>");
      std::string s;
      static constexpr auto n = 10000;
      std::vector<uint16_t> v(n);

      std::mt19937_64 gen{};

      for (auto i = 0; i < n; ++i) {
         v[i] = std::uniform_int_distribution<uint16_t>{(std::numeric_limits<uint16_t>::min)(),
                                                        (std::numeric_limits<uint16_t>::max)()}(gen);
      }

      auto copy = v;

      expect(not glz::write_beve(v, s));

      v.clear();

      expect(!glz::read_beve(v, s));

      expect(v == copy);
   };

   "std::vector<float>"_test = [] {
      auto scoped = trace.async_scope("test std::vector<float>");
      std::string s;
      static constexpr auto n = 10000;
      std::vector<float> v(n);

      std::mt19937_64 gen{};

      for (auto i = 0; i < n; ++i) {
         v[i] = std::uniform_real_distribution<float>{(std::numeric_limits<float>::min)(),
                                                      (std::numeric_limits<float>::max)()}(gen);
      }

      auto copy = v;

      expect(not glz::write_beve(v, s));

      v.clear();

      expect(!glz::read_beve(v, s));

      expect(v == copy);
   };

   "std::vector<double>"_test = [] {
      auto scoped = trace.async_scope("test std::vector<double>");
      std::string s;
      static constexpr auto n = 10000;
      std::vector<double> v(n);

      std::mt19937_64 gen{};

      for (auto i = 0; i < n; ++i) {
         v[i] = std::uniform_real_distribution<double>{(std::numeric_limits<double>::min)(),
                                                       (std::numeric_limits<double>::max)()}(gen);
      }

      auto copy = v;

      expect(not glz::write_beve(v, s));

      v.clear();

      expect(!glz::read_beve(v, s));

      expect(v == copy);
   };
};

suite file_write_read_tests = [] {
   "file_write_read"_test = [] {
      std::string s;
      static constexpr auto n = 10000;
      std::vector<uint8_t> v(n);

      std::mt19937_64 gen{};

      for (auto i = 0; i < n; ++i) {
         v[i] = uint8_t(std::uniform_int_distribution<uint16_t>{0, 255}(gen));
      }

      auto copy = v;

      expect(!glz::write_file_beve(v, "file_read_write.beve", s));

      v.clear();

      expect(!glz::read_file_beve(v, "file_read_write.beve", s));

      expect(v == copy);
   };
};

struct something_t
{
   std::vector<double> data;

   struct glaze
   {
      using T = something_t;
      static constexpr auto value = glz::object(&T::data);
   };
};

suite glz_obj_tests = [] {
   "glz::obj"_test = [] {
      std::string s;
      std::vector<double> data;
      expect(not glz::write_beve(glz::obj{"data", data}, s));

      something_t obj;
      expect(!glz::read_beve(obj, s));
      expect(obj.data == data);
   };
};

struct reflectable_t
{
   int x{1};
   int y{2};
   int z{3};

   constexpr bool operator==(const reflectable_t&) const noexcept = default;
};

static_assert(glz::reflectable<reflectable_t>);

suite reflection_test = [] {
   "reflectable_t"_test = [] {
      std::string s;
      reflectable_t obj{};
      expect(not glz::write_beve(obj, s));

      reflectable_t compare{};
      expect(!glz::read_beve(compare, s));
      expect(compare == obj);
   };
};

struct my_example
{
   int i = 287;
   double d = 3.14;
   std::string hello = "Hello World";
   std::array<uint64_t, 3> arr = {1, 2, 3};
   std::map<std::string, int> map{{"one", 1}, {"two", 2}};

   bool operator==(const my_example& other) const noexcept = default;
};

suite example_reflection_test = [] {
   "example_reflection"_test = [] {
      std::string s;
      my_example obj{};
      expect(not glz::write_beve(obj, s));

      my_example compare{};
      compare.i = 0;
      compare.d = 0.0;
      compare.hello = "";
      compare.arr = {0, 0, 0};
      compare.map.clear();
      expect(!glz::read_beve(compare, s));
      expect(compare == obj);
   };
};

suite example_reflection_without_keys_test = [] {
   "example_reflection_without_keys"_test = [] {
      std::string without_keys;
      my_example obj{.i = 55, .d = 3.14, .hello = "happy"};
      constexpr glz::opts options{.format = glz::BEVE, .structs_as_arrays = true};
      expect(not glz::write<options>(obj, without_keys));

      std::string with_keys;
      expect(not glz::write_beve(obj, with_keys));

      expect(without_keys.find("hello") == std::string::npos);
      expect(with_keys.find("hello") != std::string::npos);
      expect(without_keys != with_keys);

      obj = {};
      expect(!glz::read<options>(obj, without_keys));

      expect(obj.i == 55);
      expect(obj.d == 3.14);
      expect(obj.hello == "happy");
   };

   "example_reflection_without_keys_function_wrappers"_test = [] {
      std::string without_keys;
      my_example obj{.i = 55, .d = 3.14, .hello = "happy"};
      expect(not glz::write_beve_untagged(obj, without_keys));

      std::string with_keys;
      expect(not glz::write_beve(obj, with_keys));

      expect(without_keys.find("hello") == std::string::npos);
      expect(with_keys.find("hello") != std::string::npos);
      expect(without_keys != with_keys);

      obj = {};
      expect(!glz::read_beve_untagged(obj, without_keys));

      expect(obj.i == 55);
      expect(obj.d == 3.14);
      expect(obj.hello == "happy");
   };

   "read_beve_untagged"_test = [] {
      my_example obj{.i = 42, .d = 2.718, .hello = "world"};
      auto encoded = glz::write_beve_untagged(obj);
      expect(encoded.has_value());

      my_example decoded{};
      auto ec = glz::read_beve_untagged(decoded, *encoded);
      expect(!ec);

      expect(decoded.i == 42);
      expect(decoded.d == 2.718);
      expect(decoded.hello == "world");
   };
};

suite my_struct_without_keys_test = [] {
   "my_struct_without_keys"_test = [] {
      std::string without_keys;
      my_struct obj{.i = 55, .d = 3.14, .hello = "happy"};
      constexpr glz::opts options{.format = glz::BEVE, .structs_as_arrays = true};
      expect(not glz::write<options>(obj, without_keys));

      std::string with_keys;
      expect(not glz::write_beve(obj, with_keys));

      expect(without_keys.find("hello") == std::string::npos);
      expect(with_keys.find("hello") != std::string::npos);
      expect(without_keys != with_keys);

      obj = {};
      expect(!glz::read<options>(obj, without_keys));

      expect(obj.i == 55);
      expect(obj.d == 3.14);
      expect(obj.hello == "happy");
   };
};

namespace variants
{
   struct A
   {
      uint8_t a{};

      auto operator<=>(const A&) const = default;
   };

   struct A1
   {
      std::map<uint8_t, uint64_t> a{};

      auto operator<=>(const A1&) const = default;
   };

   struct B
   {
      uint8_t b{};
      A1 a{};

      auto operator<=>(const B&) const = default;
   };

   struct C
   {
      bool is_a{};
      std::map<uint8_t, std::variant<A, B>> a{};
   };

   class D
   {
     public:
      C c{};
   };

   suite variants = [] {
      "variants"_test = [] {
         std::vector<uint8_t> out;
         D d{};
         expect(
            not glz::write<glz::opts{.format = glz::BEVE, .structs_as_arrays = true}>(d, out)); // testing compilation
      };
   };
}

struct empty_t
{
   struct glaze
   {
      using T = empty_t;
      static constexpr auto value = glz::object();
   };
};

suite empty_object_test = [] {
   "empty_object"_test = [] {
      std::string s;
      empty_t empty{};
      expect(not glz::write_beve(empty, s));

      empty_t obj;
      expect(!glz::read_beve(obj, s));
   };
};

enum class sub : uint8_t { START, END, UPDATE_ITEM, UPDATE_PRICE };

struct A
{
   sub b;

   struct glaze
   {
      using T = A;
      static constexpr auto value = glz::object("b", &T::b);
   };
};

suite sub_enum = [] {
   "sub_enum"_test = [] {
      A obj{.b = sub::END};
      std::string s{};
      expect(not glz::write_beve(obj, s));

      obj = {};
      expect(!glz::read_beve(obj, s));
      expect(obj.b == sub::END);
   };
};

suite glz_text_tests = [] {
   "glz_text"_test = [] {
      glz::text text = "Hello World";
      std::string out{};
      expect(not glz::write_beve(text, out));

      text.str.clear();
      expect(!glz::read_beve(text, out));
      expect(text.str == "Hello World");
   };
};

suite beve_custom_key_tests = [] {
   "map ModuleID"_test = [] { verify_map_roundtrip<ModuleID>(); };
   "map CastModuleID"_test = [] { verify_map_roundtrip<CastModuleID>(); };

   "unordered_map ModuleID"_test = [] { verify_unordered_map_roundtrip<ModuleID>(); };
   "unordered_map CastModuleID"_test = [] { verify_unordered_map_roundtrip<CastModuleID>(); };

   "no_header ModuleID"_test = [] { verify_no_header_raw_bytes<ModuleID>(); };
   "no_header CastModuleID"_test = [] { verify_no_header_raw_bytes<CastModuleID>(); };

   "vector pair ModuleID"_test = [] { verify_vector_pair_roundtrip<ModuleID>(); };
   "vector pair CastModuleID"_test = [] { verify_vector_pair_roundtrip<CastModuleID>(); };
};

suite beve_to_json_tests = [] {
   "beve_to_json bool"_test = [] {
      bool b = true;
      std::string buffer{};
      expect(not glz::write_beve(b, buffer));

      std::string json{};
      expect(!glz::beve_to_json(buffer, json));
      expect(json == R"(true)");
   };

   "beve_to_json float"_test = [] {
      float v = 3.14f;
      std::string buffer{};
      expect(not glz::write_beve(v, buffer));

      std::string json{};
      expect(!glz::beve_to_json(buffer, json));
      expect(json == R"(3.14)") << json;
      float res{};
      expect(!glz::read_json(res, json));
      expect(v == res);
   };

   "beve_to_json string"_test = [] {
      std::string v = "Hello World";
      std::string buffer{};
      expect(not glz::write_beve(v, buffer));

      std::string json{};
      expect(!glz::beve_to_json(buffer, json));
      expect(json == R"("Hello World")") << json;
   };

   "beve_to_json std::map"_test = [] {
      std::map<std::string, int> v = {{"first", 1}, {"second", 2}, {"third", 3}};
      std::string buffer{};
      expect(not glz::write_beve(v, buffer));

      std::string json{};
      expect(!glz::beve_to_json(buffer, json));
      expect(json == R"({"first":1,"second":2,"third":3})") << json;

      expect(!glz::beve_to_json<glz::opts{.prettify = true}>(buffer, json));
      expect(json == //
             R"({
   "first": 1,
   "second": 2,
   "third": 3
})") << json;
   };

   "beve_to_json std::vector<int32_t>"_test = [] {
      std::vector<int32_t> v = {1, 2, 3, 4, 5};
      std::string buffer{};
      expect(not glz::write_beve(v, buffer));

      std::string json{};
      expect(!glz::beve_to_json(buffer, json));
      expect(json == R"([1,2,3,4,5])") << json;
   };

   "beve_to_json std::vector<double>"_test = [] {
      std::vector<double> v = {1.0, 2.0, 3.0, 4.0, 5.0};
      std::string buffer{};
      expect(not glz::write_beve(v, buffer));

      std::string json{};
      expect(!glz::beve_to_json(buffer, json));
      expect(json == R"([1,2,3,4,5])") << json;
   };

   "beve_to_json std::vector<std::string>"_test = [] {
      std::vector<std::string> v = {"one", "two", "three"};
      std::string buffer{};
      expect(not glz::write_beve(v, buffer));

      std::string json{};
      expect(!glz::beve_to_json(buffer, json));
      expect(json == R"(["one","two","three"])") << json;
   };

   "beve_to_json std::tuple<int, std::string>"_test = [] {
      std::tuple<int, std::string> v = {99, "spiders"};
      std::string buffer{};
      expect(not glz::write_beve(v, buffer));

      std::string json{};
      expect(!glz::beve_to_json(buffer, json));
      expect(json == R"([99,"spiders"])") << json;
   };

   "beve_to_json std::variant<int, std::string>"_test = [] {
      std::variant<int, std::string> v = 99;
      std::string buffer{};
      expect(not glz::write_beve(v, buffer));

      std::string json{};
      expect(!glz::beve_to_json(buffer, json));
      expect(json == R"(99)") << json;
   };

   "beve_to_json std::variant<int, std::string> prettify"_test = [] {
      std::variant<int, std::string> v = 99;
      std::string buffer{};
      expect(not glz::write_beve(v, buffer));

      std::string json{};
      expect(!glz::beve_to_json<glz::opts{.prettify = true}>(buffer, json));
      expect(json == //
             R"(99)")
         << json;
   };

   "beve_to_json std::complex<float>"_test = [] {
      std::complex<float> v{1.f, 2.f};
      std::string buffer{};
      expect(not glz::write_beve(v, buffer));

      std::string json{};
      expect(!glz::beve_to_json(buffer, json));
      expect(json == R"([1,2])") << json;
   };

   "beve_to_json std::vector<std::complex<float>>"_test = [] {
      std::vector<std::complex<float>> v{{1.f, 2.f}, {2.f, 3.f}};
      std::string buffer{};
      expect(not glz::write_beve(v, buffer));

      std::string json{};
      expect(!glz::beve_to_json(buffer, json));
      expect(json == R"([[1,2],[2,3]])") << json;
   };
};

suite merge_tests = [] {
   "merge"_test = [] {
      my_struct v{};

      const auto bin = glz::write_beve(glz::merge{glz::obj{"a", v}, glz::obj{"c", "d"}}).value_or("error");

      std::string json{};
      expect(!glz::beve_to_json(bin, json));
      expect(json == R"({"a":{"i":287,"d":3.14,"hello":"Hello World","arr":[1,2,3],"include":""},"c":"d"})") << json;
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
   "std::filesystem::path"_test = [] {
      std::filesystem::path p{"./my_path"};
      std::string buffer = glz::write_beve(p).value_or("error");

      p = "./bogus";
      expect(!glz::read_beve(p, buffer));
      expect(p.string() == "./my_path");
   };

   "path_test_struct"_test = [] {
      path_test_struct obj{};
      std::string buffer = glz::write_beve(obj).value_or("error");

      obj.p.clear();
      expect(!glz::read_beve(obj, buffer));
      expect(obj.p == "./my_path");
   };
};

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
      expect(not glz::write_beve(arr, s));
      std::memset(arr, 0, 4 * sizeof(uint32_t));
      expect(arr[0] == 0);
      expect(!glz::read_beve(arr, s));
      expect(arr[0] == 1);
      expect(arr[1] == 2);
      expect(arr[2] == 3);
      expect(arr[3] == 4);
   };

   "const double c array"_test = [] {
      const double arr[4] = {1.1, 2.2, 3.3, 4.4};
      std::string s{};
      expect(not glz::write_beve(arr, s));
   };

   "double c array"_test = [] {
      double arr[4] = {1.1, 2.2, 3.3, 4.4};
      std::string s{};
      expect(not glz::write_beve(arr, s));
      std::memset(arr, 0, 4 * sizeof(double));
      expect(arr[0] == 0.0);
      expect(!glz::read_beve(arr, s));
      expect(arr[0] == 1.1);
      expect(arr[1] == 2.2);
      expect(arr[2] == 3.3);
      expect(arr[3] == 4.4);
   };

   "struct_c_arrays"_test = [] {
      struct_c_arrays obj{};
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

suite error_outputs = [] {
   "valid"_test = [] {
      std::string v = "Hello World";
      std::vector<std::byte> buffer{};
      expect(not glz::write_beve(v, buffer));
      buffer.emplace_back(std::byte('\0'));
      v.clear();
      auto ec = glz::read_beve(v, buffer);
      expect(ec == glz::error_code::none);
      [[maybe_unused]] auto err = glz::format_error(ec, buffer);
   };

   "invalid"_test = [] {
      std::string v = "Hello World";
      std::string buffer{};
      expect(not glz::write_beve(int{5}, buffer));

      auto ec = glz::read_beve(v, buffer);
      expect(ec != glz::error_code::none);
      buffer.clear();
      [[maybe_unused]] auto err = glz::format_error(ec, buffer);
      expect(err == "index 0: syntax_error") << err;
   };

   "invalid with buffer"_test = [] {
      std::string v = "Hello World";
      std::string buffer{};
      expect(not glz::write_beve(int{5}, buffer));

      auto ec = glz::read_beve(v, buffer);
      expect(ec != glz::error_code::none);
      [[maybe_unused]] auto err = glz::format_error(ec, buffer);
   };
};

struct partial_struct
{
   std::string string{};
   int32_t integer{};
};

struct full_struct
{
   std::string skip_me{};
   std::string string{};
   int32_t integer{};
   std::vector<int> more_data_to_ignore{};
};

struct Header
{
   std::string id{};
   std::string type{};
};

template <>
struct glz::meta<Header>
{
   static constexpr auto partial_read = true;
};

suite read_allocated_tests = [] {
   static constexpr glz::opts partial{.format = glz::BEVE, .partial_read = true};

   "partial_read tuple"_test = [] {
      std::tuple<std::string, int, std::string> input{"hello", 88, "a string we don't care about"};
      auto s = glz::write_beve(input).value_or("error");
      std::tuple<std::string, int> obj{};
      auto ec = glz::read<partial>(obj, s);
      expect(!ec) << glz::format_error(ec, s);
      expect(std::get<0>(obj) == "hello");
      expect(std::get<1>(obj) == 88);
   };

   "partial_read vector<int>"_test = [] {
      std::vector<int> input{1, 2, 3, 4, 5};
      auto s = glz::write_beve(input).value_or("error");
      std::vector<int> v(2);
      expect(!glz::read<partial>(v, s));
      expect(v.size() == 2);
      expect(v[0] == 1);
      expect(v[1] == 2);
   };

   "partial_read vector<string>"_test = [] {
      std::vector<std::string> input{"1", "2", "3", "4", "5"};
      auto s = glz::write_beve(input).value_or("error");
      std::vector<std::string> v(2);
      expect(!glz::read<partial>(v, s));
      expect(v.size() == 2);
      expect(v[0] == "1");
      expect(v[1] == "2");
   };

   "partial_read map"_test = [] {
      std::map<std::string, int> input{{"1", 1}, {"2", 2}, {"3", 3}};
      auto s = glz::write_beve(input).value_or("error");
      std::map<std::string, int> obj{{"2", 0}};
      expect(!glz::read<partial>(obj, s));
      expect(obj.size() == 1);
      expect(obj.at("2") = 2);
   };

   "partial_read partial_struct"_test = [] {
      full_struct input{"garbage", "ha!", 400, {1, 2, 3}};
      auto s = glz::write_beve(input).value_or("error");
      partial_struct obj{};
      expect(!glz::read<glz::opts{.format = glz::BEVE, .error_on_unknown_keys = false, .partial_read = true}>(obj, s));
      expect(obj.string == "ha!");
      expect(obj.integer == 400);
   };

   "partial_read"_test = [] {
      Header input{"51e2affb", "message_type"};
      auto buf = glz::write_beve(input).value_or("error");
      Header h{};
      expect(!glz::read_beve(h, buf));
      expect(h.id == "51e2affb");
      expect(h.type == "message_type");
   };

   "partial read unknown key 2"_test = [] {
      Header input{"51e2affb", "message_type"};
      auto buf = glz::write_beve(input).value_or("error");
      Header h{};
      expect(!glz::read<glz::opts{.format = glz::BEVE, .error_on_unknown_keys = false}>(h, buf));
      expect(h.id == "51e2affb");
      expect(h.type == "message_type");
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
   using T = hide_struct;
   static constexpr auto value = object(&T::i, //
                                        &T::d, //
                                        "hello", hide{&T::hello});
};

suite hide_tests = [] {
   "hide"_test = [] {
      hide_struct obj{};
      auto b = glz::write_beve(obj).value_or("error");
      expect(!glz::read_beve(obj, b));
   };
};

struct skip_fields
{
   std::string str = "Hello";
   int opt = 35;
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
      skip_fields data{};
      auto buffer = glz::write_beve(data).value_or("error");
      skip_obj obj{};
      expect(!glz::read_beve(obj, buffer));
   };
};

suite type_conversions = [] {
   "double -> float"_test = [] {
      constexpr double pi64 = std::numbers::pi_v<double>;
      auto b = glz::write_beve(pi64).value_or("error");
      float pi32{};
      expect(!glz::read_beve(pi32, b));
      expect(pi32 == std::numbers::pi_v<float>);
   };

   "float -> double"_test = [] {
      constexpr float pi32 = std::numbers::pi_v<float>;
      auto b = glz::write_beve(pi32).value_or("error");
      double pi64{};
      expect(!glz::read_beve(pi64, b));
      expect(pi64 == std::numbers::pi_v<float>);
   };

   "int8_t -> uint8_t"_test = [] {
      auto b = glz::write_beve(int8_t{45}).value_or("error");
      uint8_t i{};
      expect(!glz::read_beve(i, b));
      expect(i == 45);

      b = glz::write_beve(int8_t{-1}).value_or("error");
      expect(!glz::read_beve(i, b));
      expect(i == 255);
   };

   "int8_t -> int32_t"_test = [] {
      auto b = glz::write_beve(int8_t{127}).value_or("error");
      int32_t i{};
      expect(!glz::read_beve(i, b));
      expect(i == 127);
   };

   "vector<double> -> vector<float>"_test = [] {
      std::vector<double> input{1.1, 2.2, 3.3};
      auto b = glz::write_beve(input).value_or("error");
      std::vector<float> v{};
      expect(!glz::read_beve(v, b));
      expect(v == std::vector{1.1f, 2.2f, 3.3f});
   };

   "vector<float> -> vector<double>"_test = [] {
      std::vector<float> input{1.f, 2.f, 3.f};
      auto b = glz::write_beve(input).value_or("error");
      std::vector<double> v{};
      expect(!glz::read_beve(v, b));
      expect(v == std::vector{1.0, 2.0, 3.0});
   };

   "map<int32_t, double> -> map<uint32_t, float>"_test = [] {
      std::map<int32_t, double> input{{1, 1.1}, {2, 2.2}, {3, 3.3}};
      auto b = glz::write_beve(input).value_or("error");
      std::map<uint32_t, float> v{};
      expect(!glz::read_beve(v, b));
      expect(v == std::map<uint32_t, float>{{1, 1.1f}, {2, 2.2f}, {3, 3.3f}});
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
      volatile int i = 42;
      std::string s{};
      expect(not glz::write_beve(i, s));
      i = 0;
      expect(!glz::read_beve(i, s));
      expect(i == 42);

      volatile uint64_t u = 99;
      expect(not glz::write_beve(u, s));
      u = 0;
      expect(!glz::read_beve(u, s));
      expect(u == 99);
   };

   "basic volatile pointer"_test = [] {
      volatile int i = 42;
      volatile int* ptr = &i;
      std::string s{};
      expect(not glz::write_beve(ptr, s));

      i = 0;
      expect(!glz::read_beve(i, s));
      expect(*ptr == 42);
      expect(i == 42);
   };

   "volatile struct_for_volatile"_test = [] {
      volatile struct_for_volatile obj{{1, 2, 3, 4}, true, -7, 9.9, 12};
      std::string s{};
      expect(not glz::write_beve(obj, s));

      obj.a.fill(0);
      obj.b = false;
      obj.c = 0;
      obj.d = 0.0;
      obj.e = 0;

      expect(!glz::read_beve(obj, s));
      expect(obj.a == glz::volatile_array<uint16_t, 4>{1, 2, 3, 4});
      expect(obj.b == true);
      expect(obj.c == -7);
      expect(obj.d == 9.9);
      expect(obj.e == 12);
   };

   "volatile my_volatile_struct"_test = [] {
      volatile my_volatile_struct obj{{1, 2, 3, 4}, true, -7, 9.9, 12};
      std::string s{};
      expect(not glz::write_beve(obj, s));

      obj.a.fill(0);
      obj.b = false;
      obj.c = 0;
      obj.d = 0.0;
      obj.e = 0;

      expect(!glz::read_beve(obj, s));
      expect(obj.a == glz::volatile_array<uint16_t, 4>{1, 2, 3, 4});
      expect(obj.b == true);
      expect(obj.c == -7);
      expect(obj.d == 9.9);
      expect(obj.e == 12);
   };
};

suite generic_tests = [] {
   "generic"_test = [] {
      glz::generic json("Hello World");
      auto b = glz::write_beve(json).value_or("error");

      json = nullptr;
      expect(not glz::read_beve(json, b));
      expect(json.is_string());
      expect(json.get_string() == "Hello World");
   };

   "generic"_test = [] {
      glz::generic json{{"i", 42}};
      auto b = glz::write_beve(json).value_or("error");

      json = nullptr;
      expect(not glz::read_beve(json, b));
      expect(json.is_object());
      expect(json.get_object().size() == 1);
      expect(json["i"].get_number() == 42);
   };

   "generic"_test = [] {
      glz::generic json{{"str", "somewhere"}, {"arr", {1, 2, 3}}};
      auto b = glz::write_beve(json).value_or("error");

      json = nullptr;
      expect(not glz::read_beve(json, b));
      expect(json.is_object());
      expect(json.get_object().size() == 2);
      expect(json["str"].get_string() == "somewhere");
      expect(json["arr"].get_array().size() == 3);
   };

   "generic"_test = [] {
      glz::generic json{1, 2, 3};
      auto b = glz::write_beve(json).value_or("error");

      json = nullptr;
      expect(not glz::read_beve(json, b));
      expect(json.is_array());
      expect(json.get_array().size() == 3);
      expect(json[0].get_number() == 1);
   };
};

suite early_end = [] {
   using namespace ut;

   "early_end"_test = [] {
      Thing obj{};
      glz::generic json{};
      glz::skip skip_me{};
      std::string buffer_data = glz::write_beve(obj).value();
      std::string_view buffer = buffer_data;
      while (buffer.size() > 0) {
         buffer_data.pop_back();
         buffer = buffer_data;
         // This is mainly to check if all our end checks are in place.
         auto ec = glz::read_beve(obj, buffer);
         expect(ec);
         expect(ec.count <= buffer.size());
         ec = glz::read_beve(json, buffer);
         expect(ec);
         expect(ec.count <= buffer.size());
         ec = glz::read_beve(skip_me, buffer);
         expect(ec);
         expect(ec.count <= buffer.size());
      }
   };

   "early_end !null terminated"_test = [] {
      static constexpr glz::opts options{.format = glz::BEVE, .null_terminated = false};

      Thing obj{};
      glz::generic json{};
      glz::skip skip_me{};
      std::string buffer_data = glz::write_beve(obj).value();
      std::vector<char> temp{buffer_data.begin(), buffer_data.end()};
      std::string_view buffer{temp.data(), temp.data() + temp.size()};
      while (buffer.size() > 0) {
         temp.pop_back();
         buffer = {temp.data(), temp.data() + temp.size()};
         // This is mainly to check if all our end checks are in place.
         auto ec = glz::read<options>(obj, buffer);
         expect(ec);
         expect(ec.count <= buffer.size());
         ec = glz::read<options>(json, buffer);
         expect(ec);
         expect(ec.count <= buffer.size());
         ec = glz::read<options>(skip_me, buffer);
         expect(ec);
         expect(ec.count <= buffer.size());
      }
   };
};

struct empty_string_test_struct
{
   std::string empty_field = "";
   int num = 42;
};

suite empty_string_test = [] {
   "empty string at buffer boundary"_test = [] {
      // Test case for the issue where ix == b.size() and str.size() == 0
      // causes an assert inside std::vector::operator[]
      std::string empty_str = "";
      std::string buffer;
      expect(not glz::write_beve(empty_str, buffer));

      // Test reading back
      std::string result;
      expect(!glz::read_beve(result, buffer));
      expect(result == empty_str);
   };

   "empty string in struct"_test = [] {
      empty_string_test_struct obj;
      std::string buffer;
      expect(not glz::write_beve(obj, buffer));

      empty_string_test_struct result;
      expect(!glz::read_beve(result, buffer));
      expect(result.empty_field == "");
      expect(result.num == 42);
   };

   "multiple empty strings"_test = [] {
      std::vector<std::string> empty_strings = {"", "", ""};
      std::string buffer;
      expect(not glz::write_beve(empty_strings, buffer));

      std::vector<std::string> result;
      expect(!glz::read_beve(result, buffer));
      expect(result.size() == 3);
      expect(result[0] == "");
      expect(result[1] == "");
      expect(result[2] == "");
   };
};

suite past_fuzzing_issues = [] {
   "fuzz0"_test = [] {
      std::string_view base64 =
         "AwQEaWH//////////////////////////////////////////////////////////////////////////////////////////////////////"
         "////////////////////////////////////////////////////////////8A=";
      const auto input = glz::read_base64(base64);
      expect(glz::read_beve<my_struct>(input).error());
   };

   "fuzz1"_test = [] {
      std::string_view base64 = "A4gEaWHw8PDw8PDw8PDw8PDw8PDw8PDw8PDw8PDw8PDw8PDw";
      const auto input = glz::read_base64(base64);
      expect(glz::read_beve<my_struct>(input).error());
   };

   "fuzz2"_test = [] {
      std::string_view base64 = "A2AMYXJy3ANg/////////wpgDAxhcnI=";
      const auto input = glz::read_base64(base64);
      expect(glz::read_beve<my_struct>(input).error());
   };

   "fuzz3"_test = [] {
      std::string_view base64 =
         "AzoxKOUMYXJydCQkKOUMYXJydCQkJCQkJCQkJCQkJCQkJCQkJCQkJCQkJCQkJCQkJCQkJCQkJCQkJCQkJCQkJCQkJCQkJCQkJCQkJCQkJCQkJ"
         "CQkJCQkJCQkJCQkJCkA";
      const auto input = glz::read_base64(base64);
      expect(glz::read_beve<my_struct>(input).error());
   };

   "fuzz4"_test = [] {
      std::string_view base64 = "Zew=";
      const auto input = glz::read_base64(base64);
      std::string json{};
      expect(glz::beve_to_json(input, json));
   };

   "fuzz5"_test = [] {
      std::string_view base64 = "CDE=";
      const auto input = glz::read_base64(base64);
      std::string json{};
      expect(glz::beve_to_json(input, json));
   };

   "fuzz6"_test = [] {
      std::string_view base64 = "HsEmAH5L";
      const auto input = glz::read_base64(base64);
      expect(glz::read_beve<my_struct>(input).error());
      std::string json{};
      expect(glz::beve_to_json(input, json));
   };

   "fuzz7"_test = [] {
      std::string_view base64 = "VSYAAGUAPdJVPdI=";
      const auto input = glz::read_base64(base64);
      expect(glz::read_beve<my_struct>(input).error());
      std::string json{};
      expect(glz::beve_to_json(input, json));
   };

   "fuzz8"_test = [] {
      std::string_view base64 =
         "ERYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYAFgAAAgAWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYAFgIAABYWFhYWFhYWFhYWF"
         "hYWFhYWFhYAFgAAAgAWFhYWFhYWFhYWABYAABYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWABYAABYWFhYWFhYWFhYWFhYWFh"
         "YWFhYWFhYWABYAAAIAFhYWFhYWFhYWFhYWFhYWFhYWFgAWABYWFhYWFhYWFhYWFhYWFhYWFhYAFgAAFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhY"
         "WFhYWFhYWFgACABYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWABYAAAIAFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYW"
         "FhYWFhYWFhYWFhYWFhYWFgACABYWFhYWFhYWFhYWFhYWFhYCABYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWABYAABYWF"
         "hYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYAFgAAAgAWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFh"
         "YWFhYWFgAWAAACABYWFhYWFhYWFhYWFhYWFhYWFhYAFgAWFhYWFhYWFhYWFhYWFhYWFhYWABYAABYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhY"
         "WFhYWFhYAAgAWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFgAWAAACABYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYW"
         "FhYWFhYWFhYWFhYWFhYWFhYWFgAWABYWFhYWFhYWFhYWFhYWFhYWFhYAFgAAFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFgACABYWF"
         "hYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWABYAAAIAFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFh"
         "YWFhYWFgAWABYWFhYWFhYWFhYWFhYWFhYWFhYAFgAAFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFgACABYWFhYWFhYWFhYWFhYWFhY"
         "WFhYWFhYWFhYWFhYWABYAAAIAFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFgACABYWFhYWFhYWFhYW"
         "FhYWFhYWFhYWFhYWFhYWFhYWABYAABYAFgIWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYAFgAAFhYWFhYWFhYWFhYWFhYWA"
         "BYAABYAFgAWAhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFgAWAAAWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFh"
         "YWABYAABYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFgAWFhYWFhYWFgACABYWFhYWFhYWFhYWFhYWFhYCABYWFhYWFhYWFhYWFhYWFhYWFhYWFhY"
         "WFhYWFhYWFhYeFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYAFgACABYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYW"
         "FhYWFhYWFhYWFhYWFhYWFhYWFhYWFgAWAAAWFhYWFhYWFhYWFhYWFhYWFhYWABYAFhYWFhYWFhYWFhYWFhYWFhYWFgAWAAAWFhYWFhYWFhYWF"
         "hYWFhYAFgAAFgAWABYCFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWABYAABYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFh"
         "YWFhYWFhYAFgAAFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFgAWFhYWFhYWFgACABYWFhYWFhYWFhYWFhYWFhYCABYWFhYWFhYWFhYWFhY"
         "WFhYWFhYWFhYWFhYWFhYWFhYeFhYWFhYWABYAABYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYAFgAAAgAWFhYWFhYWFhYW"
         "FhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYAFgAAAgAWFhYWFhYWFhYWFhYWFhYWFhYWABYAFhYWFhYWFhYWFhYWFhYWF"
         "hYWFgAWAAAWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWABYAAAIAFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFh"
         "YWFgACABYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWABYAABYAFgIWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFgQAFhY"
         "AFgAAFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWAAIAFhYWFhYWFhYWFhYWFhYWAAIAFhYWFhYW"
         "FhYWFgAWAAACAAAAFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFgAWABYWFhYWFhYWFhYWF"
         "hYWFhYWFhYAFgAAFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFgACABYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWABYAAAIAFh"
         "YWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYAFgAAFgA"
         "WAhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFgAWAAAWFhYWFhYWFhYWFhYWFhYAFgAAFgAWABYCFhYWFhYWFhYWFhYWFhYW"
         "FhYWFhYWFhYWFhYWFhYWFhYWFhYWABYAABYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYAFgAAFhYWFhYWFhYWFhYWFhYWFhYWF"
         "hYWFhYWFhYWFgAWFhYWFhYWFgACABYWFhYWFhYWFhYWFhYWFhYCABYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYeFhYWFhYWABYAAB"
         "YWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYAFgAAAgAWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhY"
         "WFhYWFhYWFhYAFgAAAgAWFhYWFhYWFhYWFhYWFhYWFhYWABYAFhYWFhYWFhYWFhYWFhYWFhYWFgAWAAAWFhYWFhYWFhYWFhYWFhYWFhYWFhYW"
         "FhYWFhYWFhYWABYAAAIAFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFgACABYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWF"
         "hYWFhYWABYAABYAFgIWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFgQAFhYAFgAAFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFh"
         "YWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWAAIAFhYWFhYWFhYWFhYWFhYWAAIAFhYWFhYWFhYWFgABBwACAAAA";

      const auto input = glz::read_base64(base64);
      expect(glz::read_beve<my_struct>(input).error());
      std::string json{};
      expect(glz::beve_to_json(input, json));
   };

   auto test_base64 = [](std::string_view base64) {
      return [base64] {
         const auto input = glz::read_base64(base64);
         expect(glz::read_beve<my_struct>(input).error());
         std::string json{};
         expect(glz::beve_to_json(input, json));
      };
   };

   "fuzz9"_test = test_base64("A10sAA==");

   "fuzz10"_test = test_base64("A4wA");

   "fuzz11"_test = test_base64("AxQA");

   "fuzz12"_test = test_base64("AzwAaGho");

   "fuzz13"_test = test_base64("AzAAYQ==");

   "fuzz14"_test = test_base64("A5AAaGgAbg==");

   "fuzz15"_test = test_base64("AzEyAA==");

   "fuzz16"_test = [] {
      std::string_view base64 =
         "YAVNTU1NTU1NTU1NTU1NTU1NTUlNTTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1"
         "NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTUxMTBNTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU01"
         "NTU1NTExME1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1N"
         "TU1NTU1NTTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1"
         "NTU1NTVgNTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTVlADU1NTU1NTU1NTExME1NTU1NTU1N"
         "TU1NTU01NTU1NTU1NTU1NTU1NTU1NWA1NTU1NU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1N"
         "TU1NTU1NTU1NTU1NTU1NTU1NTU01NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1"
         "NTU1NTU1NTU1NTU1NTU1NTU1YDU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1"
         "NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTExME1NTU1NTU1NTU1NTU1NTU1N"
         "TU1NTU1NTU01NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1"
         "NTU1NTU1YDU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTUxMTBNTU1NTU1N"
         "TU1NTU1NTU1NTU1NTTU1NTU1MTEwTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1N"
         "TU1NTU1NTU1NTU1NTU1NTU01NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1"
         "NTU1NTU1NTU1NTU1NTU1YDU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTUx"
         "MTBNTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1"
         "NTU1NTU1MTEwTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1"
         "NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTVgNTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1"
         "NTU1NTU1NTU1NTU1NTU1NTExME1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU06TU1NTU1NTU1NTU1N"
         "TU1NTU1NTU1NTU1NTU1NTU1NTU01NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1"
         "NTU1NTU1NTU1NTU1NTU1NTVgNTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1"
         "NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1MTEwTU1NTU1NTU1NTU1NTU1NTU1N"
         "TU1NTU1NTTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1"
         "NTU1NTVgNTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTExME1NTU1NTU1N"
         "TU1NTU1NTU1NTU1NNTU1NTUxMTBNTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1N"
         "TU1NTU1NTU1NTU1NTU1NTTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1"
         "NTU1NTU1NTU1NTU1NTVgNTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTEx"
         "ME1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NNTU1NTUxMTBNTU1NTU1NTU1NTU1NTU1NTU1NTU1N"
         "TU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU01NTU1NTU1NTU1NTU1NTU1NTU1NTU1"
         "NTU1NTU1NTU1TU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NNTU1NTU1"
         "NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NWA1NTU1NTU1"
         "NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1MTEwTU1NTU1NTU1NTU1NTU1NTU1NTU1N"
         "TU1NTU1NNTU1NTUxMTBNTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1N"
         "TU1NTU1NTU1NTU1NTU01NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1"
         "NTU1NTU1NTU1NTU1YDU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTVgNTU1NTU1NTU1NTU1"
         "NTU1NTU1NTU1NTU1NTU1NTVlADU1NTU1NTU1NTExME1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU01"
         "NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1YDU1"
         "NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1YDU1NTU1"
         "TU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTTU1NTU1"
         "NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTVgNTU1NTU1"
         "NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1"
         "NTU1NTU1NTU1NTU1MTEwTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTTU1NTU1NTU1NTU1NTU1NTU1"
         "NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTVgNTU1NTU1NTU1NTU1NTU1NTU1NTU1"
         "NTU1NTU1NTU1NTU1NTU1NTU1NTExME1NTU1NTU1NTU1NTU1NTU1NTU1NNTU1NTUxMTBNTU1NTU1N"
         "TU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NNTU1NTU1NTU1NTU1"
         "NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NWA1NTU1NTU1NTU1NTU1NTU1"
         "NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1MTEwTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU01NTU1"
         "NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTExME1NTU1NTU1NTU1NTU1NTU1NTU1NTU1N"
         "TU01NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NWA1"
         "NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1MTEwTU1NTU1NTU1NTU1NTU1N"
         "TU1NTU1NTU1NTTpNTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTTU1NTU1NTU1NTU1NTU1"
         "NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTVgNTU1NTU1NTU1NTU1NTU1"
         "NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1"
         "NTU1MTEwTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1"
         "NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTVgNTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1"
         "NTU1NTU1NTU1NTExME1NTU1NTU1NTU1NTU1NTU1NTU1NNTU1NTUxMTBNTU1NTU1NTU1NTU1NTU1N"
         "TU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTTU1NTU1NTU1NTU1NTU1NTU1NTU1"
         "NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTVgNTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1"
         "NTU1NTU1NTU1NTU1NTU1NTExME1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTTU1NTU1MTEwTU1N"
         "TU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1"
         "NTU1NTU1NTU1NTU1NTU1NTU1NWA1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NWUANTU1NTU1"
         "NTU1MTEwTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1"
         "NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTVgNTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1NTU1"
         "NTU1NTU1NTU1NTU1NTExME1NNTUxMTBNTU1NTU1NTU1NTU1NTU1NTU1NTU1NJwA=";

      const auto input = glz::read_base64(base64);
      expect(glz::read_beve<my_struct>(input).error());
      std::string json{};
      auto ec = glz::beve_to_json(input, json);
      expect(ec == glz::error_code::exceeded_max_recursive_depth);
   };
};

struct custom_load_t
{
   std::vector<int> x{};
   std::vector<int> y{1, 2, 3};

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
      std::string s{};
      expect(not glz::write_beve(obj, s));
      expect(!glz::read_beve(obj, s));
      expect(obj.x[0] == 1);
      expect(obj.x[1] == 2);
      expect(obj.x[2] == 3);
   };
};

struct opts_concatenate : glz::opts
{
   bool concatenate = true;
};

suite pair_ranges_tests = [] {
   static constexpr opts_concatenate concatenate_off{{glz::BEVE}, false};

   "vector pair"_test = [] {
      std::vector<std::pair<int, int>> v{{1, 2}, {3, 4}};
      auto s = glz::write<concatenate_off>(v).value_or("error");
      std::string json{};
      expect(not glz::beve_to_json(s, json));
      expect(json == R"([{"1":2},{"3":4}])");
      std::vector<std::pair<int, int>> x;
      expect(!glz::read<concatenate_off>(x, s));
      expect(x == v);
   };
   "vector pair roundtrip"_test = [] {
      std::vector<std::pair<int, int>> v{{1, 2}, {3, 4}};
      auto s = glz::write_beve(v).value_or("error");
      std::string json{};
      expect(not glz::beve_to_json(s, json));
      expect(json == R"({"1":2,"3":4})");
      std::vector<std::pair<int, int>> x;
      expect(!glz::read_beve(x, s));
      expect(x == v);
   };
};

// Test for static variant tags with empty structs
namespace static_tag_test
{
   enum class MsgTypeEmpty { A, B };

   struct MsgAEmpty
   {
      static constexpr auto type = MsgTypeEmpty::A;
   };

   struct MsgBEmpty
   {
      static constexpr auto type = MsgTypeEmpty::B;
   };

   using MsgEmpty = std::variant<MsgAEmpty, MsgBEmpty>;

   enum class MsgType { A, B };

   struct MsgA
   {
      static constexpr auto type = MsgType::A;
      int value = 42;
   };

   struct MsgB
   {
      static constexpr auto type = MsgType::B;
      std::string text = "hello";
   };

   using Msg = std::variant<MsgA, MsgB>;
}

suite static_variant_tags = [] {
   "static variant tags with empty structs"_test = [] {
      using namespace static_tag_test;

      // Test untagged BEVE with empty structs having static tags
      {
         MsgEmpty original{MsgAEmpty{}};
         auto encoded = glz::write_beve_untagged(original);
         expect(encoded.has_value());

         auto decoded = glz::read_beve_untagged<MsgEmpty>(*encoded);
         expect(decoded.has_value());
         expect(decoded->index() == 0);
      }

      {
         MsgEmpty original{MsgBEmpty{}};
         auto encoded = glz::write_beve_untagged(original);
         expect(encoded.has_value());

         auto decoded = glz::read_beve_untagged<MsgEmpty>(*encoded);
         expect(decoded.has_value());
         expect(decoded->index() == 1);
      }
   };

   "static variant tags with non-empty structs"_test = [] {
      using namespace static_tag_test;

      // Test untagged BEVE with non-empty structs having static tags
      {
         Msg original{MsgA{}};
         auto encoded = glz::write_beve_untagged(original);
         expect(encoded.has_value());

         auto decoded = glz::read_beve_untagged<Msg>(*encoded);
         expect(decoded.has_value());
         expect(decoded->index() == 0);
         expect(std::get<0>(*decoded).value == 42);
      }

      {
         Msg original{MsgB{}};
         auto encoded = glz::write_beve_untagged(original);
         expect(encoded.has_value());

         auto decoded = glz::read_beve_untagged<Msg>(*encoded);
         expect(decoded.has_value());
         expect(decoded->index() == 1);
         expect(std::get<1>(*decoded).text == "hello");
      }
   };
};

suite explicit_string_view_support = [] {
   "write beve from explicit string_view"_test = [] {
      struct explicit_string_view_type
      {
         std::string storage{};

         explicit explicit_string_view_type(std::string_view s) : storage(s) {}

         explicit operator std::string_view() const noexcept { return storage; }
      };

      explicit_string_view_type value{std::string_view{"explicit"}};

      std::string buffer{};
      expect(not glz::write_beve(value, buffer));
      expect(not buffer.empty());

      std::string decoded{};
      expect(not glz::read_beve(decoded, buffer));
      expect(decoded == "explicit");
   };
};

struct MemberFunctionThingBeve
{
   std::string name{};
   auto get_description() const -> std::string { return "something"; }
};

namespace glz
{
   template <>
   struct meta<MemberFunctionThingBeve>
   {
      using T = MemberFunctionThingBeve;
      static constexpr auto value = object("name", &T::name, "description", &T::get_description);
   };
} // namespace glz

suite member_function_pointer_beve_serialization = [] {
   "member function pointer skipped in beve write"_test = [] {
      MemberFunctionThingBeve input{};
      input.name = "test_item";
      std::string buffer{};
      expect(not glz::write_beve(input, buffer));

      MemberFunctionThingBeve output{};
      expect(not glz::read_beve(output, buffer));
      expect(output.name == input.name);
   };

   "member function pointer opt-in write encodes description key"_test = [] {
      MemberFunctionThingBeve input{};
      input.name = "test_item";

      std::string buffer_default{};
      expect(not glz::write_beve(input, buffer_default));
      expect(buffer_default.find("description") == std::string::npos);

      struct opts_with_member_functions : glz::opts
      {
         bool write_member_functions = true;
      };

      std::string buffer_opt_in{};
      expect(not glz::write<glz::set_beve<opts_with_member_functions{}>()>(input, buffer_opt_in));
      expect(buffer_opt_in.find("description") != std::string::npos);
   };
};

// ===== Delimited BEVE tests =====

struct simple_obj
{
   int x{};
   std::string y{};
};

suite delimited_beve_tests = [] {
   "delimiter tag value"_test = [] {
      // Verify the delimiter tag is correct: extensions type (6) with subtype 0
      expect(glz::tag::delimiter == uint8_t(0x06));
   };

   "write_beve_delimiter"_test = [] {
      std::string buffer{};
      glz::write_beve_delimiter(buffer);
      expect(buffer.size() == size_t(1));
      expect(static_cast<uint8_t>(buffer[0]) == glz::tag::delimiter);
   };

   "write_beve_append single value"_test = [] {
      std::string buffer{};

      auto result1 = glz::write_beve_append(42, buffer);
      expect(!result1);
      expect(result1.count > size_t(0));
      const size_t first_size = buffer.size();

      auto result2 = glz::write_beve_append(std::string{"hello"}, buffer);
      expect(!result2);
      expect(result2.count > size_t(0));
      expect(buffer.size() > first_size);
   };

   "write_beve_append_with_delimiter"_test = [] {
      std::string buffer{};

      // Write first value without delimiter
      auto result1 = glz::write_beve_append(42, buffer);
      expect(!result1);
      const size_t first_size = buffer.size();

      // Write second value with delimiter
      auto result2 = glz::write_beve_append_with_delimiter(100, buffer);
      expect(!result2);
      expect(result2.count > size_t(0));

      // Check delimiter was written
      expect(static_cast<uint8_t>(buffer[first_size]) == glz::tag::delimiter);
   };

   "write_beve_delimited vector of ints"_test = [] {
      std::vector<int> values{1, 2, 3, 4, 5};
      std::string buffer{};

      auto ec = glz::write_beve_delimited(values, buffer);
      expect(!ec);
      expect(buffer.size() > size_t(0));

      // Verify round-trip works correctly (more robust than counting raw delimiter bytes)
      std::vector<int> result{};
      ec = glz::read_beve_delimited(result, buffer);
      expect(!ec);
      expect(result.size() == size_t(5));
      expect(result == values);
   };

   "write_beve_delimited returning string"_test = [] {
      std::vector<double> values{1.5, 2.5, 3.5};
      auto result = glz::write_beve_delimited(values);
      expect(result.has_value());
      expect(result->size() > size_t(0));
   };

   "read_beve_delimited vector of ints"_test = [] {
      // Write delimited values
      std::vector<int> input{10, 20, 30, 40};
      std::string buffer{};
      auto ec = glz::write_beve_delimited(input, buffer);
      expect(!ec);

      // Read them back
      std::vector<int> output{};
      ec = glz::read_beve_delimited(output, buffer);
      expect(!ec);
      expect(output.size() == size_t(4));
      expect(output == input);
   };

   "read_beve_delimited vector of strings"_test = [] {
      std::vector<std::string> input{"hello", "world", "test"};
      std::string buffer{};
      auto ec = glz::write_beve_delimited(input, buffer);
      expect(!ec);

      std::vector<std::string> output{};
      ec = glz::read_beve_delimited(output, buffer);
      expect(!ec);
      expect(output == input);
   };

   "read_beve_delimited vector of objects"_test = [] {
      std::vector<simple_obj> input{{1, "first"}, {2, "second"}, {3, "third"}};
      std::string buffer{};
      auto ec = glz::write_beve_delimited(input, buffer);
      expect(!ec);

      std::vector<simple_obj> output{};
      ec = glz::read_beve_delimited(output, buffer);
      expect(!ec);
      expect(output.size() == size_t(3));
      expect(output[0].x == 1);
      expect(output[0].y == "first");
      expect(output[1].x == 2);
      expect(output[2].x == 3);
   };

   "read_beve_delimited returning container"_test = [] {
      std::vector<int> input{100, 200, 300};
      auto buffer = glz::write_beve_delimited(input).value_or("");
      expect(!buffer.empty());

      auto result = glz::read_beve_delimited<std::vector<int>>(buffer);
      expect(result.has_value());
      expect(*result == input);
   };

   "read_beve_at with offset"_test = [] {
      std::string buffer{};

      // Write three values with delimiters
      (void)glz::write_beve_append(42, buffer);
      glz::write_beve_delimiter(buffer);
      size_t second_offset = buffer.size();
      (void)glz::write_beve_append(std::string{"hello"}, buffer);
      glz::write_beve_delimiter(buffer);
      size_t third_offset = buffer.size();
      (void)glz::write_beve_append(3.14, buffer);

      // Read at offset 0
      int val1{};
      auto result1 = glz::read_beve_at(val1, buffer, 0);
      expect(result1.has_value());
      expect(val1 == 42);

      // Read at second_offset (should skip delimiter)
      std::string val2{};
      auto result2 = glz::read_beve_at(val2, buffer, second_offset);
      expect(result2.has_value());
      expect(val2 == "hello");

      // Read at third_offset (should skip delimiter)
      double val3{};
      auto result3 = glz::read_beve_at(val3, buffer, third_offset);
      expect(result3.has_value());
      expect(std::abs(val3 - 3.14) < 0.001);
   };

   "empty buffer handling"_test = [] {
      std::string empty_buffer{};
      std::vector<int> output{};

      auto ec = glz::read_beve_delimited(output, empty_buffer);
      expect(!ec);
      expect(output.empty());
   };

   "trailing delimiter handling"_test = [] {
      // Create buffer with values followed by a trailing delimiter
      std::string buffer{};
      (void)glz::write_beve_append(42, buffer);
      glz::write_beve_delimiter(buffer);
      (void)glz::write_beve_append(100, buffer);
      glz::write_beve_delimiter(buffer); // trailing delimiter

      // read_beve_delimited should gracefully handle trailing delimiter
      std::vector<int> output{};
      auto ec = glz::read_beve_delimited(output, buffer);
      expect(!ec);
      expect(output.size() == size_t(2));
      expect(output[0] == 42);
      expect(output[1] == 100);

      // read_beve_at at trailing delimiter should return error (nothing to read)
      int value{};
      size_t trailing_offset = buffer.size() - 1; // points to trailing delimiter
      auto result = glz::read_beve_at(value, buffer, trailing_offset);
      expect(!result.has_value()); // should fail - no value after delimiter
   };

   "single value delimited"_test = [] {
      std::vector<int> input{42};
      std::string buffer{};
      auto ec = glz::write_beve_delimited(input, buffer);
      expect(!ec);

      // Verify single value round-trips correctly
      std::vector<int> output{};
      ec = glz::read_beve_delimited(output, buffer);
      expect(!ec);
      expect(output.size() == size_t(1));
      expect(output == input);
   };

   "manual append workflow"_test = [] {
      // Append multiple objects to a buffer and read them back

      std::string buffer{};

      // Append first object
      auto bytes1 = glz::write_beve_append(simple_obj{1, "first"}, buffer);
      expect(!bytes1);

      // Append delimiter and second object
      auto bytes2 = glz::write_beve_append_with_delimiter(simple_obj{2, "second"}, buffer);
      expect(!bytes2);

      // Append delimiter and third object
      auto bytes3 = glz::write_beve_append_with_delimiter(simple_obj{3, "third"}, buffer);
      expect(!bytes3);

      // Now read all objects back
      std::vector<simple_obj> results{};
      auto ec = glz::read_beve_delimited(results, buffer);
      expect(!ec);
      expect(results.size() == size_t(3));
      expect(results[0].x == 1);
      expect(results[0].y == "first");
      expect(results[1].x == 2);
      expect(results[1].y == "second");
      expect(results[2].x == 3);
      expect(results[2].y == "third");
   };

   "bytes consumed tracking"_test = [] {
      // Test that error_ctx.count tracks bytes consumed correctly
      int value = 42;
      std::string buffer{};
      auto ec = glz::write_beve(value, buffer);
      expect(!ec);

      int result{};
      ec = glz::read_beve(result, buffer);
      expect(!ec);
      expect(ec.count == buffer.size()) << "count should equal bytes consumed";
      expect(result == 42);
   };
};

// ============================================================================
// Tests for error_on_missing_keys
// ============================================================================

namespace error_on_missing_keys_tests
{
   struct DataV1
   {
      int hp = 0;
      bool is_alive = false;
      bool operator==(const DataV1&) const = default;
   };

   struct DataV2
   {
      int hp = 0;
      bool is_alive = false;
      int new_field = 0;
      bool operator==(const DataV2&) const = default;
   };

   struct DataWithOptional
   {
      int hp = 0;
      std::optional<int> optional_field;
      bool operator==(const DataWithOptional&) const = default;
   };

   struct DataWithNullablePtr
   {
      int hp = 0;
      std::unique_ptr<int> nullable_ptr;
   };

   struct NestedOuter
   {
      DataV1 inner;
      int outer_value = 0;
      bool operator==(const NestedOuter&) const = default;
   };

   struct NestedOuterV2
   {
      DataV2 inner;
      int outer_value = 0;
      int extra = 0;
      bool operator==(const NestedOuterV2&) const = default;
   };

   struct EmptyStruct
   {
      bool operator==(const EmptyStruct&) const = default;
   };

   struct DataMultipleFields
   {
      int a = 0;
      int b = 0;
      int c = 0;
      bool operator==(const DataMultipleFields&) const = default;
   };
}

template <>
struct glz::meta<error_on_missing_keys_tests::DataV1>
{
   using T = error_on_missing_keys_tests::DataV1;
   static constexpr auto value = object("hp", &T::hp, "is_alive", &T::is_alive);
};

template <>
struct glz::meta<error_on_missing_keys_tests::DataV2>
{
   using T = error_on_missing_keys_tests::DataV2;
   static constexpr auto value = object("hp", &T::hp, "is_alive", &T::is_alive, "new_field", &T::new_field);
};

template <>
struct glz::meta<error_on_missing_keys_tests::DataWithOptional>
{
   using T = error_on_missing_keys_tests::DataWithOptional;
   static constexpr auto value = object("hp", &T::hp, "optional_field", &T::optional_field);
};

template <>
struct glz::meta<error_on_missing_keys_tests::DataWithNullablePtr>
{
   using T = error_on_missing_keys_tests::DataWithNullablePtr;
   static constexpr auto value = object("hp", &T::hp, "nullable_ptr", &T::nullable_ptr);
};

template <>
struct glz::meta<error_on_missing_keys_tests::NestedOuter>
{
   using T = error_on_missing_keys_tests::NestedOuter;
   static constexpr auto value = object("inner", &T::inner, "outer_value", &T::outer_value);
};

template <>
struct glz::meta<error_on_missing_keys_tests::NestedOuterV2>
{
   using T = error_on_missing_keys_tests::NestedOuterV2;
   static constexpr auto value = object("inner", &T::inner, "outer_value", &T::outer_value, "extra", &T::extra);
};

template <>
struct glz::meta<error_on_missing_keys_tests::EmptyStruct>
{
   using T = error_on_missing_keys_tests::EmptyStruct;
   static constexpr auto value = object();
};

template <>
struct glz::meta<error_on_missing_keys_tests::DataMultipleFields>
{
   using T = error_on_missing_keys_tests::DataMultipleFields;
   static constexpr auto value = object("a", &T::a, "b", &T::b, "c", &T::c);
};

suite beve_error_on_missing_keys = [] {
   using namespace error_on_missing_keys_tests;

   "error_on_missing_keys=false allows missing keys"_test = [] {
      DataV1 v1{10, true};
      std::string buffer{};
      expect(not glz::write_beve(v1, buffer));

      DataV2 v2{};
      constexpr glz::opts opts = {.format = glz::BEVE, .error_on_missing_keys = false};
      auto ec = glz::read<opts>(v2, buffer);
      expect(!ec) << glz::format_error(ec, buffer);
      expect(v2.hp == 10);
      expect(v2.is_alive == true);
      expect(v2.new_field == 0); // Default value preserved
   };

   "error_on_missing_keys=true detects missing required key"_test = [] {
      DataV1 v1{10, true};
      std::string buffer{};
      expect(not glz::write_beve(v1, buffer));

      DataV2 v2{};
      constexpr glz::opts opts = {.format = glz::BEVE, .error_on_missing_keys = true};
      auto ec = glz::read<opts>(v2, buffer);
      expect(ec.ec == glz::error_code::missing_key) << "Expected missing_key error";
   };

   "error_on_missing_keys=true with complete data succeeds"_test = [] {
      DataV2 v2_orig{10, true, 42};
      std::string buffer{};
      expect(not glz::write_beve(v2_orig, buffer));

      DataV2 v2{};
      constexpr glz::opts opts = {.format = glz::BEVE, .error_on_missing_keys = true};
      auto ec = glz::read<opts>(v2, buffer);
      expect(!ec) << glz::format_error(ec, buffer);
      expect(v2 == v2_orig);
   };

   "error_on_missing_keys=true allows missing optional fields"_test = [] {
      // Write only hp (but optional_field is nullable so not required)
      DataV1 v1{10, true};
      std::string buffer{};
      expect(not glz::write_beve(v1, buffer));

      // Read into struct where optional_field exists but is nullable
      DataWithOptional v{};
      constexpr glz::opts opts = {.format = glz::BEVE, .error_on_unknown_keys = false, .error_on_missing_keys = true};
      auto ec = glz::read<opts>(v, buffer);
      // Should succeed because optional_field is nullable
      expect(!ec) << glz::format_error(ec, buffer);
      expect(v.hp == 10);
      expect(!v.optional_field.has_value());
   };

   "error_on_missing_keys=true allows missing unique_ptr fields"_test = [] {
      DataV1 v1{10, true};
      std::string buffer{};
      expect(not glz::write_beve(v1, buffer));

      DataWithNullablePtr v{};
      constexpr glz::opts opts = {.format = glz::BEVE, .error_on_unknown_keys = false, .error_on_missing_keys = true};
      auto ec = glz::read<opts>(v, buffer);
      // Should succeed because nullable_ptr is nullable
      expect(!ec) << glz::format_error(ec, buffer);
      expect(v.hp == 10);
      expect(v.nullable_ptr == nullptr);
   };

   "error_on_missing_keys with nested objects"_test = [] {
      NestedOuter outer{{5, true}, 100};
      std::string buffer{};
      expect(not glz::write_beve(outer, buffer));

      NestedOuterV2 outer_v2{};
      constexpr glz::opts opts = {.format = glz::BEVE, .error_on_missing_keys = true};
      auto ec = glz::read<opts>(outer_v2, buffer);
      // Should fail because extra field is missing AND inner.new_field is missing
      expect(ec.ec == glz::error_code::missing_key);
   };

   "error_on_missing_keys reports missing key in error message"_test = [] {
      DataV1 v1{10, true};
      std::string buffer{};
      expect(not glz::write_beve(v1, buffer));

      DataV2 v2{};
      constexpr glz::opts opts = {.format = glz::BEVE, .error_on_missing_keys = true};
      auto ec = glz::read<opts>(v2, buffer);
      expect(ec.ec == glz::error_code::missing_key);
      // The error message should contain the missing key name
      std::string error_msg = glz::format_error(ec, buffer);
      expect(error_msg.find("new_field") != std::string::npos)
         << "Error message should contain 'new_field': " << error_msg;
   };

   "error_on_missing_keys with multiple missing keys reports first"_test = [] {
      EmptyStruct empty{};
      std::string buffer{};
      constexpr glz::opts write_opts = {.format = glz::BEVE};
      expect(not glz::write<write_opts>(empty, buffer));

      DataMultipleFields multi{};
      constexpr glz::opts read_opts = {.format = glz::BEVE, .error_on_missing_keys = true};
      auto ec = glz::read<read_opts>(multi, buffer);
      expect(ec.ec == glz::error_code::missing_key);
   };
};

// ============================================================================
// Tests for skipping typed arrays (std::vector<bool>, std::vector<std::string>)
// ============================================================================

namespace skip_typed_array_tests
{
   struct WithBoolArray
   {
      int id = 0;
      std::vector<bool> flags;
      std::string name;
      bool operator==(const WithBoolArray&) const = default;
   };

   struct WithoutBoolArray
   {
      int id = 0;
      std::string name;
      bool operator==(const WithoutBoolArray&) const = default;
   };

   struct WithStringArray
   {
      int id = 0;
      std::vector<std::string> names;
      int count = 0;
      bool operator==(const WithStringArray&) const = default;
   };

   struct WithoutStringArray
   {
      int id = 0;
      int count = 0;
      bool operator==(const WithoutStringArray&) const = default;
   };

   struct WithIntArray
   {
      int id = 0;
      std::vector<int> values;
      std::string label;
      bool operator==(const WithIntArray&) const = default;
   };

   struct WithoutIntArray
   {
      int id = 0;
      std::string label;
      bool operator==(const WithoutIntArray&) const = default;
   };

   struct WithFloatArray
   {
      int id = 0;
      std::vector<float> values;
      std::string label;
      bool operator==(const WithFloatArray&) const = default;
   };

   struct ComplexStruct
   {
      int id = 0;
      std::vector<bool> bool_arr;
      std::vector<std::string> str_arr;
      std::vector<int> int_arr;
      std::string name;
      bool operator==(const ComplexStruct&) const = default;
   };

   struct SimpleStruct
   {
      int id = 0;
      std::string name;
      bool operator==(const SimpleStruct&) const = default;
   };
}

template <>
struct glz::meta<skip_typed_array_tests::WithBoolArray>
{
   using T = skip_typed_array_tests::WithBoolArray;
   static constexpr auto value = object("id", &T::id, "flags", &T::flags, "name", &T::name);
};

template <>
struct glz::meta<skip_typed_array_tests::WithoutBoolArray>
{
   using T = skip_typed_array_tests::WithoutBoolArray;
   static constexpr auto value = object("id", &T::id, "name", &T::name);
};

template <>
struct glz::meta<skip_typed_array_tests::WithStringArray>
{
   using T = skip_typed_array_tests::WithStringArray;
   static constexpr auto value = object("id", &T::id, "names", &T::names, "count", &T::count);
};

template <>
struct glz::meta<skip_typed_array_tests::WithoutStringArray>
{
   using T = skip_typed_array_tests::WithoutStringArray;
   static constexpr auto value = object("id", &T::id, "count", &T::count);
};

template <>
struct glz::meta<skip_typed_array_tests::WithIntArray>
{
   using T = skip_typed_array_tests::WithIntArray;
   static constexpr auto value = object("id", &T::id, "values", &T::values, "label", &T::label);
};

template <>
struct glz::meta<skip_typed_array_tests::WithoutIntArray>
{
   using T = skip_typed_array_tests::WithoutIntArray;
   static constexpr auto value = object("id", &T::id, "label", &T::label);
};

template <>
struct glz::meta<skip_typed_array_tests::WithFloatArray>
{
   using T = skip_typed_array_tests::WithFloatArray;
   static constexpr auto value = object("id", &T::id, "values", &T::values, "label", &T::label);
};

template <>
struct glz::meta<skip_typed_array_tests::ComplexStruct>
{
   using T = skip_typed_array_tests::ComplexStruct;
   static constexpr auto value =
      object("id", &T::id, "bool_arr", &T::bool_arr, "str_arr", &T::str_arr, "int_arr", &T::int_arr, "name", &T::name);
};

template <>
struct glz::meta<skip_typed_array_tests::SimpleStruct>
{
   using T = skip_typed_array_tests::SimpleStruct;
   static constexpr auto value = object("id", &T::id, "name", &T::name);
};

suite beve_skip_typed_arrays = [] {
   using namespace skip_typed_array_tests;

   "skip std::vector<bool> when reading unknown key"_test = [] {
      WithBoolArray src{42, {true, false, true, true, false}, "test_name"};
      std::string buffer{};
      expect(not glz::write_beve(src, buffer));

      WithoutBoolArray dst{};
      constexpr glz::opts opts = {.format = glz::BEVE, .error_on_unknown_keys = false};
      auto ec = glz::read<opts>(dst, buffer);
      expect(!ec) << glz::format_error(ec, buffer);
      expect(dst.id == 42);
      expect(dst.name == "test_name");
   };

   "skip std::vector<bool> with many elements"_test = [] {
      std::vector<bool> large_bool_vec(1000);
      for (size_t i = 0; i < large_bool_vec.size(); ++i) {
         large_bool_vec[i] = (i % 3 == 0);
      }
      WithBoolArray src{99, large_bool_vec, "large_test"};
      std::string buffer{};
      expect(not glz::write_beve(src, buffer));

      WithoutBoolArray dst{};
      constexpr glz::opts opts = {.format = glz::BEVE, .error_on_unknown_keys = false};
      auto ec = glz::read<opts>(dst, buffer);
      expect(!ec) << glz::format_error(ec, buffer);
      expect(dst.id == 99);
      expect(dst.name == "large_test");
   };

   "skip std::vector<std::string> when reading unknown key"_test = [] {
      WithStringArray src{42, {"hello", "world", "test"}, 100};
      std::string buffer{};
      expect(not glz::write_beve(src, buffer));

      WithoutStringArray dst{};
      constexpr glz::opts opts = {.format = glz::BEVE, .error_on_unknown_keys = false};
      auto ec = glz::read<opts>(dst, buffer);
      expect(!ec) << glz::format_error(ec, buffer);
      expect(dst.id == 42);
      expect(dst.count == 100);
   };

   "skip std::vector<std::string> with many elements"_test = [] {
      std::vector<std::string> large_str_vec;
      for (int i = 0; i < 100; ++i) {
         large_str_vec.push_back("string_" + std::to_string(i));
      }
      WithStringArray src{99, large_str_vec, 999};
      std::string buffer{};
      expect(not glz::write_beve(src, buffer));

      WithoutStringArray dst{};
      constexpr glz::opts opts = {.format = glz::BEVE, .error_on_unknown_keys = false};
      auto ec = glz::read<opts>(dst, buffer);
      expect(!ec) << glz::format_error(ec, buffer);
      expect(dst.id == 99);
      expect(dst.count == 999);
   };

   "skip std::vector<std::string> with empty strings"_test = [] {
      WithStringArray src{42, {"", "non-empty", "", ""}, 50};
      std::string buffer{};
      expect(not glz::write_beve(src, buffer));

      WithoutStringArray dst{};
      constexpr glz::opts opts = {.format = glz::BEVE, .error_on_unknown_keys = false};
      auto ec = glz::read<opts>(dst, buffer);
      expect(!ec) << glz::format_error(ec, buffer);
      expect(dst.id == 42);
      expect(dst.count == 50);
   };

   "skip std::vector<int> when reading unknown key"_test = [] {
      WithIntArray src{42, {1, 2, 3, 4, 5}, "label"};
      std::string buffer{};
      expect(not glz::write_beve(src, buffer));

      WithoutIntArray dst{};
      constexpr glz::opts opts = {.format = glz::BEVE, .error_on_unknown_keys = false};
      auto ec = glz::read<opts>(dst, buffer);
      expect(!ec) << glz::format_error(ec, buffer);
      expect(dst.id == 42);
      expect(dst.label == "label");
   };

   "skip std::vector<float> when reading unknown key"_test = [] {
      WithFloatArray src{42, {1.1f, 2.2f, 3.3f}, "float_label"};
      std::string buffer{};
      expect(not glz::write_beve(src, buffer));

      WithoutIntArray dst{};
      constexpr glz::opts opts = {.format = glz::BEVE, .error_on_unknown_keys = false};
      auto ec = glz::read<opts>(dst, buffer);
      expect(!ec) << glz::format_error(ec, buffer);
      expect(dst.id == 42);
      expect(dst.label == "float_label");
   };

   "skip multiple typed arrays"_test = [] {
      ComplexStruct src{42, {true, false}, {"a", "b", "c"}, {1, 2, 3, 4}, "complex"};
      std::string buffer{};
      expect(not glz::write_beve(src, buffer));

      SimpleStruct dst{};
      constexpr glz::opts opts = {.format = glz::BEVE, .error_on_unknown_keys = false};
      auto ec = glz::read<opts>(dst, buffer);
      expect(!ec) << glz::format_error(ec, buffer);
      expect(dst.id == 42);
      expect(dst.name == "complex");
   };

   "skip empty std::vector<bool>"_test = [] {
      WithBoolArray src{42, {}, "empty_bool"};
      std::string buffer{};
      expect(not glz::write_beve(src, buffer));

      WithoutBoolArray dst{};
      constexpr glz::opts opts = {.format = glz::BEVE, .error_on_unknown_keys = false};
      auto ec = glz::read<opts>(dst, buffer);
      expect(!ec) << glz::format_error(ec, buffer);
      expect(dst.id == 42);
      expect(dst.name == "empty_bool");
   };

   "skip empty std::vector<std::string>"_test = [] {
      WithStringArray src{42, {}, 100};
      std::string buffer{};
      expect(not glz::write_beve(src, buffer));

      WithoutStringArray dst{};
      constexpr glz::opts opts = {.format = glz::BEVE, .error_on_unknown_keys = false};
      auto ec = glz::read<opts>(dst, buffer);
      expect(!ec) << glz::format_error(ec, buffer);
      expect(dst.id == 42);
      expect(dst.count == 100);
   };

   "roundtrip with bool array preserved"_test = [] {
      WithBoolArray src{42, {true, false, true}, "roundtrip"};
      std::string buffer{};
      expect(not glz::write_beve(src, buffer));

      WithBoolArray dst{};
      auto ec = glz::read_beve(dst, buffer);
      expect(!ec) << glz::format_error(ec, buffer);
      expect(dst == src);
   };

   "roundtrip with string array preserved"_test = [] {
      WithStringArray src{42, {"a", "bb", "ccc"}, 100};
      std::string buffer{};
      expect(not glz::write_beve(src, buffer));

      WithStringArray dst{};
      auto ec = glz::read_beve(dst, buffer);
      expect(!ec) << glz::format_error(ec, buffer);
      expect(dst == src);
   };
};

// Bounded buffer overflow tests for BEVE format
namespace beve_bounded_buffer_tests
{
   struct simple_beve_obj
   {
      int x = 42;
      std::string name = "hello";
   };

   struct large_beve_obj
   {
      int x = 42;
      std::string long_name = "this is a very long string that definitely won't fit in a tiny buffer";
      std::vector<int> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
   };
}

suite beve_bounded_buffer_overflow_tests = [] {
   using namespace beve_bounded_buffer_tests;

   "beve write to std::array with sufficient space succeeds"_test = [] {
      simple_beve_obj obj{};
      std::array<char, 512> buffer{};

      auto result = glz::write_beve(obj, buffer);
      expect(not result) << "write should succeed with sufficient buffer";
      expect(result.count > 0) << "count should be non-zero";
      expect(result.count < buffer.size()) << "count should be less than buffer size";

      // Verify roundtrip
      simple_beve_obj decoded{};
      auto ec = glz::read_beve(decoded, std::string_view{buffer.data(), result.count});
      expect(!ec) << "read should succeed";
      expect(decoded.x == obj.x) << "x should match";
      expect(decoded.name == obj.name) << "name should match";
   };

   "beve write to std::array that is too small returns buffer_overflow"_test = [] {
      large_beve_obj obj{};
      std::array<char, 10> buffer{};

      auto result = glz::write_beve(obj, buffer);
      expect(result.ec == glz::error_code::buffer_overflow) << "should return buffer_overflow error";
   };

   "beve write to std::span with sufficient space succeeds"_test = [] {
      simple_beve_obj obj{};
      std::array<char, 512> storage{};
      std::span<char> buffer(storage);

      auto result = glz::write_beve(obj, buffer);
      expect(not result) << "write should succeed with sufficient buffer";
      expect(result.count > 0) << "count should be non-zero";
   };

   "beve write to std::span that is too small returns buffer_overflow"_test = [] {
      large_beve_obj obj{};
      std::array<char, 5> storage{};
      std::span<char> buffer(storage);

      auto result = glz::write_beve(obj, buffer);
      expect(result.ec == glz::error_code::buffer_overflow) << "should return buffer_overflow error";
   };

   "beve write array to bounded buffer works correctly"_test = [] {
      std::vector<int> arr{1, 2, 3, 4, 5};
      std::array<char, 512> buffer{};

      auto result = glz::write_beve(arr, buffer);
      expect(not result) << "write should succeed";
      expect(result.count > 0) << "count should be non-zero";

      std::vector<int> decoded{};
      auto ec = glz::read_beve(decoded, std::string_view{buffer.data(), result.count});
      expect(!ec) << "read should succeed";
      expect(decoded == arr) << "decoded array should match";
   };

   "beve write large array to small bounded buffer fails"_test = [] {
      std::vector<int> arr(100, 42);
      std::array<char, 8> buffer{};

      auto result = glz::write_beve(arr, buffer);
      expect(result.ec == glz::error_code::buffer_overflow) << "should return buffer_overflow for large array";
   };

   "beve resizable buffer still works as before"_test = [] {
      simple_beve_obj obj{};
      std::string buffer;

      auto result = glz::write_beve(obj, buffer);
      expect(not result) << "write to resizable buffer should succeed";
      expect(buffer.size() > 0) << "buffer should have data";
   };

   "beve nested struct to bounded buffer"_test = [] {
      my_struct obj{};
      obj.i = 100;
      obj.d = 3.14;
      obj.hello = "world";
      obj.arr = {1, 2, 3};
      std::array<char, 1024> buffer{};

      auto result = glz::write_beve(obj, buffer);
      expect(not result) << "write should succeed";

      my_struct decoded{};
      auto ec = glz::read_beve(decoded, std::string_view{buffer.data(), result.count});
      expect(!ec) << "read should succeed";
      expect(decoded.i == obj.i) << "i should match";
      expect(decoded.d == obj.d) << "d should match";
      expect(decoded.hello == obj.hello) << "hello should match";
   };

   "beve map to bounded buffer"_test = [] {
      std::map<std::string, int> obj{{"one", 1}, {"two", 2}, {"three", 3}};
      std::array<char, 512> buffer{};

      auto result = glz::write_beve(obj, buffer);
      expect(not result) << "write should succeed";

      std::map<std::string, int> decoded{};
      auto ec = glz::read_beve(decoded, std::string_view{buffer.data(), result.count});
      expect(!ec) << "read should succeed";
      expect(decoded == obj) << "decoded map should match";
   };
};

// Structs for DoS prevention tests
struct DoSTestInner
{
   std::string name;
   std::string value;
};

struct DoSTestOuter
{
   std::vector<DoSTestInner> items;
};

// Security tests for DoS prevention (Issues #2187 and #2190)
// These tests verify that malicious BEVE buffers with huge length headers
// are rejected before any memory allocation occurs.
suite dos_prevention = [] {
   "string memory bomb protection"_test = [] {
      // Create a valid BEVE buffer with a long string, then truncate it
      std::string original(1000, 'x'); // 1000 character string
      std::string valid_buffer;
      expect(not glz::write_beve(original, valid_buffer));

      // Truncate to just the header + length (claiming 1000 bytes but only a few bytes of data)
      // This ensures the length header claims more data than available
      std::string truncated_buffer = valid_buffer.substr(0, 4);

      std::string result;
      auto ec = glz::read_beve(result, truncated_buffer);

      // Should fail with unexpected_end, NOT crash with bad_alloc
      expect(bool(ec)) << "Should reject truncated string buffer";
   };

   "string array memory bomb protection"_test = [] {
      // Create a valid BEVE buffer with a few strings, then truncate it
      std::vector<std::string> original = {"a", "b", "c", "d", "e", "f", "g", "h", "i", "j"};
      std::string valid_buffer;
      expect(not glz::write_beve(original, valid_buffer));

      // Truncate to just the header + count (claiming 10 strings but only a few bytes)
      std::string truncated_buffer = valid_buffer.substr(0, 3);

      std::vector<std::string> result;
      auto ec = glz::read_beve(result, truncated_buffer);

      expect(ec.ec == glz::error_code::invalid_length) << "Should reject truncated string array";
   };

   "boolean array memory bomb protection"_test = [] {
      // Create valid buffer with many bools, then truncate
      std::vector<bool> original(100, true);
      std::string valid_buffer;
      expect(not glz::write_beve(original, valid_buffer));

      // Truncate to just header + count
      std::string truncated_buffer = valid_buffer.substr(0, 3);

      std::vector<bool> result;
      auto ec = glz::read_beve(result, truncated_buffer);

      expect(ec.ec == glz::error_code::invalid_length) << "Should reject truncated bool array";
   };

   "generic array memory bomb protection"_test = [] {
      // Create valid buffer with many elements, then truncate
      std::vector<glz::generic> original;
      for (int i = 0; i < 50; i++) {
         original.push_back(glz::generic{i});
      }
      std::string valid_buffer;
      expect(not glz::write_beve(original, valid_buffer));

      // Truncate to just header + count
      std::string truncated_buffer = valid_buffer.substr(0, 3);

      std::vector<glz::generic> result;
      auto ec = glz::read_beve(result, truncated_buffer);

      expect(ec.ec == glz::error_code::invalid_length) << "Should reject truncated generic array";
   };

   "numeric array memory bomb protection"_test = [] {
      // Create a valid BEVE buffer with many ints, then truncate it
      std::vector<int> original(100, 42); // 100 integers
      std::string valid_buffer;
      expect(not glz::write_beve(original, valid_buffer));

      // Truncate to just the header + count (claiming 100 ints but only a few bytes of data)
      std::string truncated_buffer = valid_buffer.substr(0, 4);

      std::vector<int> result;
      auto ec = glz::read_beve(result, truncated_buffer);

      // Should fail with unexpected_end, NOT crash with bad_alloc
      expect(bool(ec)) << "Should reject truncated numeric array buffer";
   };

   "nested struct with strings memory bomb protection"_test = [] {
      // Create valid buffer, then truncate
      DoSTestOuter original;
      for (int i = 0; i < 10; i++) {
         original.items.push_back({.name = "item" + std::to_string(i), .value = "value" + std::to_string(i)});
      }
      std::string valid_buffer;
      expect(not glz::write_beve(original, valid_buffer));

      // Truncate significantly
      std::string truncated = valid_buffer.substr(0, valid_buffer.size() / 4);

      DoSTestOuter result;
      auto ec = glz::read_beve(result, truncated);
      expect(bool(ec)) << "Should fail on truncated nested struct";
   };

   "map with huge key count protection"_test = [] {
      // Create valid map buffer, then truncate
      std::map<std::string, int> original = {{"one", 1}, {"two", 2}, {"three", 3}};
      std::string valid_buffer;
      expect(not glz::write_beve(original, valid_buffer));

      // Truncate to minimal data
      std::string truncated = valid_buffer.substr(0, 4);

      std::map<std::string, int> result;
      auto ec = glz::read_beve(result, truncated);
      expect(bool(ec)) << "Should fail on truncated map";
   };
};

// Custom opts for max_string_length and max_array_size tests
struct limited_string_opts : glz::opts
{
   uint32_t format = glz::BEVE;
   size_t max_string_length = 10;
};

struct limited_array_opts : glz::opts
{
   uint32_t format = glz::BEVE;
   size_t max_array_size = 5;
};

struct limited_both_opts : glz::opts
{
   uint32_t format = glz::BEVE;
   size_t max_string_length = 10;
   size_t max_array_size = 5;
};

// Tests for user-configurable allocation limits (Issue #2190)
suite allocation_limits = [] {
   "max_string_length rejects oversized strings"_test = [] {
      std::string long_string(100, 'x'); // 100 character string
      std::string buffer;
      expect(not glz::write_beve(long_string, buffer));

      // Try to read with a limit of 10 characters
      std::string result;
      auto ec = glz::read<limited_string_opts{}>(result, buffer);
      expect(ec.ec == glz::error_code::invalid_length) << "Should reject string exceeding max_string_length";
   };

   "max_string_length allows strings under limit"_test = [] {
      std::string short_string("hello"); // 5 characters, under 10 limit
      std::string buffer;
      expect(not glz::write_beve(short_string, buffer));

      std::string result;
      auto ec = glz::read<limited_string_opts{}>(result, buffer);
      expect(!ec) << "Should accept string under max_string_length";
      expect(result == short_string);
   };

   "max_array_size rejects oversized arrays"_test = [] {
      std::vector<int> large_array(100, 42); // 100 integers
      std::string buffer;
      expect(not glz::write_beve(large_array, buffer));

      // Try to read with a limit of 5 elements
      std::vector<int> result;
      auto ec = glz::read<limited_array_opts{}>(result, buffer);
      expect(ec.ec == glz::error_code::invalid_length) << "Should reject array exceeding max_array_size";
   };

   "max_array_size allows arrays under limit"_test = [] {
      std::vector<int> small_array = {1, 2, 3}; // 3 elements, under 5 limit
      std::string buffer;
      expect(not glz::write_beve(small_array, buffer));

      std::vector<int> result;
      auto ec = glz::read<limited_array_opts{}>(result, buffer);
      expect(!ec) << "Should accept array under max_array_size";
      expect(result == small_array);
   };

   "max_string_length works for string arrays"_test = [] {
      std::vector<std::string> strings = {"short", "hello", "world"};
      std::string buffer;
      expect(not glz::write_beve(strings, buffer));

      // All strings are under 10 chars, so should succeed
      std::vector<std::string> result;
      auto ec = glz::read<limited_string_opts{}>(result, buffer);
      expect(!ec) << "Should accept string array with all strings under limit";

      // Now try with a long string
      std::vector<std::string> long_strings = {"short", "this is a very long string indeed"};
      buffer.clear();
      expect(not glz::write_beve(long_strings, buffer));

      result.clear();
      ec = glz::read<limited_string_opts{}>(result, buffer);
      expect(ec.ec == glz::error_code::invalid_length) << "Should reject string array with string exceeding limit";
   };

   "max_array_size works for boolean arrays"_test = [] {
      std::vector<bool> large_bools(100, true); // 100 booleans
      std::string buffer;
      expect(not glz::write_beve(large_bools, buffer));

      std::vector<bool> result;
      auto ec = glz::read<limited_array_opts{}>(result, buffer);
      expect(ec.ec == glz::error_code::invalid_length) << "Should reject boolean array exceeding max_array_size";
   };

   "max_array_size works for generic arrays"_test = [] {
      std::vector<glz::generic> generics;
      for (int i = 0; i < 100; i++) {
         generics.push_back(glz::generic{i * 1.5});
      }
      std::string buffer;
      expect(not glz::write_beve(generics, buffer));

      std::vector<glz::generic> result;
      auto ec = glz::read<limited_array_opts{}>(result, buffer);
      expect(ec.ec == glz::error_code::invalid_length) << "Should reject generic array exceeding max_array_size";
   };

   "both limits work together"_test = [] {
      // Test that both limits can be used together
      std::vector<std::string> data = {"hi", "yo"}; // 2 elements, short strings - OK
      std::string buffer;
      expect(not glz::write_beve(data, buffer));

      std::vector<std::string> result;
      auto ec = glz::read<limited_both_opts{}>(result, buffer);
      expect(!ec) << "Should accept data under both limits";

      // Exceed array limit
      std::vector<std::string> many_strings(10, "hi"); // 10 elements, exceeds limit of 5
      buffer.clear();
      expect(not glz::write_beve(many_strings, buffer));

      result.clear();
      ec = glz::read<limited_both_opts{}>(result, buffer);
      expect(ec.ec == glz::error_code::invalid_length) << "Should reject when array size exceeds limit";

      // Exceed string limit
      std::vector<std::string> long_string_vec = {"hi", "this is way too long"};
      buffer.clear();
      expect(not glz::write_beve(long_string_vec, buffer));

      result.clear();
      ec = glz::read<limited_both_opts{}>(result, buffer);
      expect(ec.ec == glz::error_code::invalid_length) << "Should reject when string length exceeds limit";
   };

   "no limit by default"_test = [] {
      // With default opts (max_string_length = 0, max_array_size = 0), no limits apply
      std::string long_string(1000, 'x');
      std::string buffer;
      expect(not glz::write_beve(long_string, buffer));

      std::string result;
      auto ec = glz::read_beve(result, buffer);
      expect(!ec) << "Default opts should allow any string length";
      expect(result == long_string);

      std::vector<int> large_array(1000, 42);
      buffer.clear();
      expect(not glz::write_beve(large_array, buffer));

      std::vector<int> arr_result;
      ec = glz::read_beve(arr_result, buffer);
      expect(!ec) << "Default opts should allow any array size";
      expect(arr_result == large_array);
   };

   "max_map_size applies to std::map"_test = [] {
      std::map<std::string, int> large_map;
      for (int i = 0; i < 100; ++i) {
         large_map["key" + std::to_string(i)] = i;
      }
      std::string buffer;
      expect(not glz::write_beve(large_map, buffer));

      // Try to read with a limit of 50 entries
      struct map_limited_opts : glz::opts
      {
         uint32_t format = glz::BEVE;
         size_t max_map_size = 50;
      };

      std::map<std::string, int> result;
      auto ec = glz::read<map_limited_opts{}>(result, buffer);
      expect(ec.ec == glz::error_code::invalid_length) << "Should reject oversized map";
   };

   "max_map_size accepts valid std::map"_test = [] {
      std::map<std::string, int> small_map{{"a", 1}, {"b", 2}, {"c", 3}};
      std::string buffer;
      expect(not glz::write_beve(small_map, buffer));

      struct map_limited_opts : glz::opts
      {
         uint32_t format = glz::BEVE;
         size_t max_map_size = 50;
      };

      std::map<std::string, int> result;
      auto ec = glz::read<map_limited_opts{}>(result, buffer);
      expect(!ec) << "Should accept map within limit";
      expect(result == small_map);
   };

   "max_map_size applies to std::unordered_map"_test = [] {
      std::unordered_map<std::string, int> large_map;
      for (int i = 0; i < 100; ++i) {
         large_map["key" + std::to_string(i)] = i;
      }
      std::string buffer;
      expect(not glz::write_beve(large_map, buffer));

      struct map_limited_opts : glz::opts
      {
         uint32_t format = glz::BEVE;
         size_t max_map_size = 50;
      };

      std::unordered_map<std::string, int> result;
      auto ec = glz::read<map_limited_opts{}>(result, buffer);
      expect(ec.ec == glz::error_code::invalid_length) << "Should reject oversized unordered_map";
   };

   "max_array_size does not affect maps"_test = [] {
      // Verify that max_array_size doesn't limit maps (they use max_map_size)
      std::map<std::string, int> large_map;
      for (int i = 0; i < 100; ++i) {
         large_map["key" + std::to_string(i)] = i;
      }
      std::string buffer;
      expect(not glz::write_beve(large_map, buffer));

      // max_array_size = 50 should NOT affect maps
      struct array_limited_opts : glz::opts
      {
         uint32_t format = glz::BEVE;
         size_t max_array_size = 50;
      };

      std::map<std::string, int> result;
      auto ec = glz::read<array_limited_opts{}>(result, buffer);
      expect(!ec) << "max_array_size should not limit maps";
      expect(result.size() == 100);
   };

   "extended opts usage with max_array_size"_test = [] {
      std::vector<int> large_array(100, 42);
      std::string buffer;
      expect(not glz::write_beve(large_array, buffer));

      // Extend opts to add allocation limits
      struct array_limited_opts : glz::opts
      {
         uint32_t format = glz::BEVE;
         size_t max_array_size = 50;
      };

      std::vector<int> result;
      auto ec = glz::read<array_limited_opts{}>(result, buffer);
      expect(ec.ec == glz::error_code::invalid_length) << "Should reject using extended opts";
   };

   "extended opts usage with max_string_length"_test = [] {
      std::string long_string(100, 'x');
      std::string buffer;
      expect(not glz::write_beve(long_string, buffer));

      struct string_limited_opts : glz::opts
      {
         uint32_t format = glz::BEVE;
         size_t max_string_length = 50;
      };

      std::string result;
      auto ec = glz::read<string_limited_opts{}>(result, buffer);
      expect(ec.ec == glz::error_code::invalid_length) << "Should reject using extended opts";
   };
};

// Structs for max_length wrapper tests
struct MaxLengthStringStruct
{
   std::string name;
   std::string description;
};

template <>
struct glz::meta<MaxLengthStringStruct>
{
   using T = MaxLengthStringStruct;
   static constexpr auto value = object("name", glz::max_length<&T::name, 10>, // limit to 10 chars
                                        "description", &T::description // no limit
   );
};

struct MaxLengthArrayStruct
{
   std::vector<int> small_list;
   std::vector<int> big_list;
};

template <>
struct glz::meta<MaxLengthArrayStruct>
{
   using T = MaxLengthArrayStruct;
   static constexpr auto value = object("small_list", glz::max_length<&T::small_list, 5>, // limit to 5 elements
                                        "big_list", &T::big_list // no limit
   );
};

// Complex struct for testing generic array path
struct ComplexItem
{
   std::string name;
   int value;
   std::vector<double> data;
};

struct MaxLengthComplexArrayStruct
{
   std::vector<ComplexItem> items;
};

template <>
struct glz::meta<MaxLengthComplexArrayStruct>
{
   using T = MaxLengthComplexArrayStruct;
   static constexpr auto value = object("items", glz::max_length<&T::items, 3> // limit to 3 complex items
   );
};

// Tests for max_length wrapper (per-field limits)
suite max_length_wrapper = [] {
   "max_length wrapper limits string field"_test = [] {
      MaxLengthStringStruct original{.name = "hello", .description = "a long description"};
      std::string buffer;
      expect(not glz::write_beve(original, buffer));

      MaxLengthStringStruct result;
      auto ec = glz::read_beve(result, buffer);
      expect(!ec) << "Should accept strings under limit";
      expect(result.name == original.name);
      expect(result.description == original.description);
   };

   "max_length wrapper rejects oversized string field"_test = [] {
      MaxLengthStringStruct original{.name = "this name is way too long", .description = "ok"};
      std::string buffer;
      expect(not glz::write_beve(original, buffer));

      MaxLengthStringStruct result;
      auto ec = glz::read_beve(result, buffer);
      expect(ec.ec == glz::error_code::invalid_length) << "Should reject string field exceeding max_length";
   };

   "max_length wrapper allows unlimited field"_test = [] {
      MaxLengthStringStruct original{.name = "short", .description = std::string(1000, 'x')};
      std::string buffer;
      expect(not glz::write_beve(original, buffer));

      MaxLengthStringStruct result;
      auto ec = glz::read_beve(result, buffer);
      expect(!ec) << "Unlimited field should accept any length";
      expect(result.description == original.description);
   };

   "max_length wrapper limits array field"_test = [] {
      MaxLengthArrayStruct original{.small_list = {1, 2, 3}, .big_list = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}};
      std::string buffer;
      expect(not glz::write_beve(original, buffer));

      MaxLengthArrayStruct result;
      auto ec = glz::read_beve(result, buffer);
      expect(!ec) << "Should accept array under limit";
      expect(result.small_list == original.small_list);
      expect(result.big_list == original.big_list);
   };

   "max_length wrapper rejects oversized array field"_test = [] {
      MaxLengthArrayStruct original{.small_list = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}, .big_list = {}};
      std::string buffer;
      expect(not glz::write_beve(original, buffer));

      MaxLengthArrayStruct result;
      auto ec = glz::read_beve(result, buffer);
      expect(ec.ec == glz::error_code::invalid_length) << "Should reject array field exceeding max_length";
   };

   "max_length wrapper roundtrip preserves data"_test = [] {
      MaxLengthStringStruct original{.name = "test", .description = "desc"};
      std::string buffer;
      expect(not glz::write_beve(original, buffer));

      MaxLengthStringStruct result;
      auto ec = glz::read_beve(result, buffer);
      expect(!ec);
      expect(result.name == original.name);
      expect(result.description == original.description);
   };

   "max_length wrapper limits complex struct array"_test = [] {
      MaxLengthComplexArrayStruct original{
         .items = {{.name = "a", .value = 1, .data = {1.0, 2.0}}, {.name = "b", .value = 2, .data = {3.0}}}};
      std::string buffer;
      expect(not glz::write_beve(original, buffer));

      MaxLengthComplexArrayStruct result;
      auto ec = glz::read_beve(result, buffer);
      expect(!ec) << "Should accept complex array under limit";
      expect(result.items.size() == 2);
      expect(result.items[0].name == "a");
      expect(result.items[1].value == 2);
   };

   "max_length wrapper rejects oversized complex struct array"_test = [] {
      MaxLengthComplexArrayStruct original{.items = {{.name = "a", .value = 1, .data = {1.0}},
                                                     {.name = "b", .value = 2, .data = {2.0}},
                                                     {.name = "c", .value = 3, .data = {3.0}},
                                                     {.name = "d", .value = 4, .data = {4.0}},
                                                     {.name = "e", .value = 5, .data = {5.0}}}};
      std::string buffer;
      expect(not glz::write_beve(original, buffer));

      MaxLengthComplexArrayStruct result;
      auto ec = glz::read_beve(result, buffer);
      expect(ec.ec == glz::error_code::invalid_length) << "Should reject complex struct array exceeding max_length";
   };
};

int main()
{
   trace.begin("binary_test");
   write_tests();
   bench();
   test_partial();
   file_include_test();
   container_types();

   trace.end("binary_test");
   const auto ec = glz::write_file_json(trace, "binary_test.trace.json", std::string{});
   if (ec) {
      std::cerr << "trace output failed\n";
   }
   return 0;
}
