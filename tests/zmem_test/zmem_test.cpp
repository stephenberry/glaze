// ZMEM Format Test Suite
// Tests the ZMEM binary serialization format implementation for Glaze

#include <cstring>
#include <vector>
#include <string>
#include <map>

#include "glaze/zmem.hpp"
#include <ut/ut.hpp>

using namespace ut;

// ============================================================================
// Test Structs
// ============================================================================

// Fixed struct - trivially copyable (zero overhead serialization)
struct Point {
   float x;
   float y;
};
static_assert(sizeof(Point) == 8);

struct Vec3 {
   float x;
   float y;
   float z;
};
static_assert(sizeof(Vec3) == 12);

struct Color {
   uint8_t r;
   uint8_t g;
   uint8_t b;
   uint8_t a;
};
static_assert(sizeof(Color) == 4);

// Fixed struct with padding
struct Mixed {
   uint8_t a;
   // padding
   uint32_t b;
   uint8_t c;
   // padding
   uint16_t d;
};
static_assert(sizeof(Mixed) == 12);

// Fixed struct with fixed array
struct Matrix2x2 {
   float data[4];
};
static_assert(sizeof(Matrix2x2) == 16);

// Variable struct - has vector field
struct Entity {
   uint64_t id;
   std::vector<float> weights;
};

// Variable struct with string
struct LogEntry {
   uint64_t timestamp;
   std::string message;
};

// Nested fixed struct
struct Transform {
   Vec3 position;
   Vec3 rotation;
   Vec3 scale;
};
static_assert(sizeof(Transform) == 36);

// Variable struct for map value testing
struct MapVariableValue {
   std::string name;
   std::vector<int32_t> values;
};

// ============================================================================
// Test Suites
// ============================================================================

suite primitives_tests = [] {
   "integer_roundtrip"_test = [] {
      std::string buffer;
      uint64_t value = 0x123456789ABCDEF0ULL;
      auto err = glz::write_zmem(value, buffer);
      expect(!err);
      expect(buffer.size() == sizeof(uint64_t));

      uint64_t result = 0;
      err = glz::read_zmem(result, buffer);
      expect(!err);
      expect(result == value);
   };

   "float_roundtrip"_test = [] {
      std::string buffer;
      float value = 3.14159f;
      auto err = glz::write_zmem(value, buffer);
      expect(!err);
      expect(buffer.size() == sizeof(float));

      float result = 0.0f;
      err = glz::read_zmem(result, buffer);
      expect(!err);
      expect(result == value);
   };

   "double_roundtrip"_test = [] {
      std::string buffer;
      double value = 2.718281828459045;
      auto err = glz::write_zmem(value, buffer);
      expect(!err);
      expect(buffer.size() == sizeof(double));

      double result = 0.0;
      err = glz::read_zmem(result, buffer);
      expect(!err);
      expect(result == value);
   };

   "bool_true_roundtrip"_test = [] {
      std::string buffer;
      bool value = true;
      auto err = glz::write_zmem(value, buffer);
      expect(!err);

      bool result = false;
      err = glz::read_zmem(result, buffer);
      expect(!err);
      expect(result == value);
   };

   "bool_false_roundtrip"_test = [] {
      std::string buffer;
      bool value = false;
      auto err = glz::write_zmem(value, buffer);
      expect(!err);

      bool result = true;
      err = glz::read_zmem(result, buffer);
      expect(!err);
      expect(result == value);
   };

   "signed_integers"_test = [] {
      std::string buffer;
      int32_t value = -42;
      auto err = glz::write_zmem(value, buffer);
      expect(!err);

      int32_t result = 0;
      err = glz::read_zmem(result, buffer);
      expect(!err);
      expect(result == value);
   };
};

suite fixed_struct_tests = [] {
   "point_zero_overhead"_test = [] {
      std::string buffer;
      Point p{1.0f, 2.0f};
      auto err = glz::write_zmem(p, buffer);
      expect(!err);
      expect(buffer.size() == sizeof(Point)) << "Fixed struct should have zero overhead";

      Point result{};
      err = glz::read_zmem(result, buffer);
      expect(!err);
      expect(result.x == p.x);
      expect(result.y == p.y);
   };

   "vec3_roundtrip"_test = [] {
      std::string buffer;
      Vec3 v{1.0f, 2.0f, 3.0f};
      auto err = glz::write_zmem(v, buffer);
      expect(!err);
      expect(buffer.size() == glz::zmem::padded_size_8(sizeof(Vec3)));  // 12 -> 16 bytes

      Vec3 result{};
      err = glz::read_zmem(result, buffer);
      expect(!err);
      expect(result.x == v.x);
      expect(result.y == v.y);
      expect(result.z == v.z);
   };

   "mixed_with_padding"_test = [] {
      std::string buffer;
      Mixed m{1, 2, 3, 4};
      auto err = glz::write_zmem(m, buffer);
      expect(!err);
      expect(buffer.size() == glz::zmem::padded_size_8(sizeof(Mixed)));  // 12 -> 16 bytes

      Mixed result{};
      err = glz::read_zmem(result, buffer);
      expect(!err);
      expect(result.a == m.a);
      expect(result.b == m.b);
      expect(result.c == m.c);
      expect(result.d == m.d);
   };

   "nested_fixed_struct"_test = [] {
      std::string buffer;
      Transform t{{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
      auto err = glz::write_zmem(t, buffer);
      expect(!err);
      expect(buffer.size() == glz::zmem::padded_size_8(sizeof(Transform)));  // 36 -> 40 bytes

      Transform result{};
      err = glz::read_zmem(result, buffer);
      expect(!err);
      expect(result.position.x == t.position.x);
      expect(result.rotation.y == t.rotation.y);
      expect(result.scale.z == t.scale.z);
   };

   "color_struct"_test = [] {
      std::string buffer;
      Color c{255, 128, 64, 32};
      auto err = glz::write_zmem(c, buffer);
      expect(!err);
      expect(buffer.size() == glz::zmem::padded_size_8(sizeof(Color)));  // 4 -> 8 bytes

      Color result{};
      err = glz::read_zmem(result, buffer);
      expect(!err);
      expect(result.r == c.r);
      expect(result.g == c.g);
      expect(result.b == c.b);
      expect(result.a == c.a);
   };
};

suite fixed_array_tests = [] {
   "c_array_float"_test = [] {
      std::string buffer;
      float arr[4] = {1.0f, 2.0f, 3.0f, 4.0f};
      auto err = glz::write_zmem(arr, buffer);
      expect(!err);
      expect(buffer.size() == sizeof(arr));

      float result[4] = {};
      err = glz::read_zmem(result, buffer);
      expect(!err);
      for (int i = 0; i < 4; ++i) {
         expect(result[i] == arr[i]);
      }
   };

   "std_array_int"_test = [] {
      std::string buffer;
      std::array<int32_t, 3> arr = {10, 20, 30};
      auto err = glz::write_zmem(arr, buffer);
      expect(!err);
      expect(buffer.size() == sizeof(arr));

      std::array<int32_t, 3> result = {};
      err = glz::read_zmem(result, buffer);
      expect(!err);
      expect(result == arr);
   };

   "struct_with_array"_test = [] {
      std::string buffer;
      Matrix2x2 m{{1, 2, 3, 4}};
      auto err = glz::write_zmem(m, buffer);
      expect(!err);
      expect(buffer.size() == sizeof(Matrix2x2));

      Matrix2x2 result{};
      err = glz::read_zmem(result, buffer);
      expect(!err);
      for (int i = 0; i < 4; ++i) {
         expect(result.data[i] == m.data[i]);
      }
   };
};

suite vector_tests = [] {
   "vector_of_floats"_test = [] {
      std::string buffer;
      std::vector<float> v = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
      auto err = glz::write_zmem(v, buffer);
      expect(!err);
      expect(buffer.size() == 8 + v.size() * sizeof(float)) << "count + data";

      std::vector<float> result;
      err = glz::read_zmem(result, buffer);
      expect(!err);
      expect(result == v);
   };

   "empty_vector"_test = [] {
      std::string buffer;
      std::vector<int32_t> v;
      auto err = glz::write_zmem(v, buffer);
      expect(!err);
      expect(buffer.size() == 8) << "Just count (0)";

      std::vector<int32_t> result;
      err = glz::read_zmem(result, buffer);
      expect(!err);
      expect(result.empty());
   };

   "vector_of_fixed_structs"_test = [] {
      std::string buffer;
      std::vector<Point> v = {{1, 2}, {3, 4}, {5, 6}};
      auto err = glz::write_zmem(v, buffer);
      expect(!err);

      std::vector<Point> result;
      err = glz::read_zmem(result, buffer);
      expect(!err);
      expect(result.size() == v.size());
      for (size_t i = 0; i < v.size(); ++i) {
         expect(result[i].x == v[i].x);
         expect(result[i].y == v[i].y);
      }
   };

   "large_vector"_test = [] {
      std::string buffer;
      std::vector<uint64_t> v(10000);
      for (size_t i = 0; i < v.size(); ++i) {
         v[i] = i * 42;
      }
      auto err = glz::write_zmem(v, buffer);
      expect(!err);

      std::vector<uint64_t> result;
      err = glz::read_zmem(result, buffer);
      expect(!err);
      expect(result == v);
   };
};

suite string_tests = [] {
   "string_roundtrip"_test = [] {
      std::string buffer;
      std::string s = "Hello, ZMEM!";
      auto err = glz::write_zmem(s, buffer);
      expect(!err);
      expect(buffer.size() == 8 + s.size()) << "length + data";

      std::string result;
      err = glz::read_zmem(result, buffer);
      expect(!err);
      expect(result == s);
   };

   "empty_string"_test = [] {
      std::string buffer;
      std::string s;
      auto err = glz::write_zmem(s, buffer);
      expect(!err);
      expect(buffer.size() == 8) << "Just length (0)";

      std::string result = "not empty";
      err = glz::read_zmem(result, buffer);
      expect(!err);
      expect(result.empty());
   };

   "unicode_string"_test = [] {
      std::string buffer;
      std::string s = "Hello, \xe4\xb8\x96\xe7\x95\x8c! \xf0\x9f\x8c\x8d"; // UTF-8 encoded: "Hello, 世界! 🌍"
      auto err = glz::write_zmem(s, buffer);
      expect(!err);

      std::string result;
      err = glz::read_zmem(result, buffer);
      expect(!err);
      expect(result == s);
   };
};

suite optional_tests = [] {
   "zmem_optional_present"_test = [] {
      std::string buffer;
      glz::zmem::optional<uint32_t> opt(42);
      auto err = glz::write_zmem(opt, buffer);
      expect(!err);
      expect(buffer.size() == sizeof(glz::zmem::optional<uint32_t>));

      glz::zmem::optional<uint32_t> result;
      err = glz::read_zmem(result, buffer);
      expect(!err);
      expect(result.has_value());
      expect(*result == 42u);
   };

   "zmem_optional_absent"_test = [] {
      std::string buffer;
      glz::zmem::optional<uint64_t> opt;
      auto err = glz::write_zmem(opt, buffer);
      expect(!err);

      glz::zmem::optional<uint64_t> result(123);
      err = glz::read_zmem(result, buffer);
      expect(!err);
      expect(!result.has_value());
   };

   "std_optional_present"_test = [] {
      std::string buffer;
      std::optional<float> opt = 3.14f;
      auto err = glz::write_zmem(opt, buffer);
      expect(!err);

      std::optional<float> result;
      err = glz::read_zmem(result, buffer);
      expect(!err);
      expect(result.has_value());
      expect(*result == 3.14f);
   };

   "std_optional_absent"_test = [] {
      std::string buffer;
      std::optional<double> opt;
      auto err = glz::write_zmem(opt, buffer);
      expect(!err);

      std::optional<double> result = 999.0;
      err = glz::read_zmem(result, buffer);
      expect(!err);
      expect(!result.has_value());
   };
};

suite map_tests = [] {
   "map_fixed_values"_test = [] {
      std::string buffer;
      std::map<int32_t, float> m = {{1, 1.0f}, {2, 2.0f}, {3, 3.0f}};
      auto err = glz::write_zmem(m, buffer);
      expect(!err);

      std::map<int32_t, float> result;
      err = glz::read_zmem(result, buffer);
      expect(!err);
      expect(result == m);
   };

   "empty_map"_test = [] {
      std::string buffer;
      std::map<uint64_t, uint64_t> m;
      auto err = glz::write_zmem(m, buffer);
      expect(!err);

      std::map<uint64_t, uint64_t> result;
      err = glz::read_zmem(result, buffer);
      expect(!err);
      expect(result.empty());
   };

   "map_maintains_order"_test = [] {
      std::string buffer;
      std::map<int32_t, int32_t> m = {{3, 30}, {1, 10}, {2, 20}};
      auto err = glz::write_zmem(m, buffer);
      expect(!err);

      std::map<int32_t, int32_t> result;
      err = glz::read_zmem(result, buffer);
      expect(!err);
      expect(result == m);

      // Verify order
      auto it = result.begin();
      expect(it->first == 1);
      ++it;
      expect(it->first == 2);
      ++it;
      expect(it->first == 3);
   };

   "map_variable_values_string"_test = [] {
      // Map with string values (variable type)
      std::string buffer;
      std::map<int32_t, std::string> m = {
         {1, "hello"},
         {2, "world"},
         {3, "test"}
      };
      auto err = glz::write_zmem(m, buffer);
      expect(!err);

      std::map<int32_t, std::string> result;
      err = glz::read_zmem(result, buffer);
      expect(!err);
      expect(result == m);
      expect(result[1] == "hello");
      expect(result[2] == "world");
      expect(result[3] == "test");
   };

   "map_variable_values_vector"_test = [] {
      // Map with vector values (variable type)
      std::string buffer;
      std::map<int32_t, std::vector<int32_t>> m = {
         {1, {10, 20, 30}},
         {2, {40, 50}},
         {3, {60, 70, 80, 90}}
      };
      auto err = glz::write_zmem(m, buffer);
      expect(!err);

      std::map<int32_t, std::vector<int32_t>> result;
      err = glz::read_zmem(result, buffer);
      expect(!err);
      expect(result == m);
      expect(result[1] == std::vector<int32_t>{10, 20, 30});
      expect(result[2] == std::vector<int32_t>{40, 50});
      expect(result[3] == std::vector<int32_t>{60, 70, 80, 90});
   };

   "map_variable_empty"_test = [] {
      // Empty map with variable value type
      std::string buffer;
      std::map<int32_t, std::string> m;
      auto err = glz::write_zmem(m, buffer);
      expect(!err);

      std::map<int32_t, std::string> result;
      err = glz::read_zmem(result, buffer);
      expect(!err);
      expect(result.empty());
   };

   "map_variable_single_entry"_test = [] {
      // Single entry map with variable value
      std::string buffer;
      std::map<uint64_t, std::string> m = {{42, "answer to everything"}};
      auto err = glz::write_zmem(m, buffer);
      expect(!err);

      std::map<uint64_t, std::string> result;
      err = glz::read_zmem(result, buffer);
      expect(!err);
      expect(result == m);
      expect(result[42] == "answer to everything");
   };

   "map_variable_nested_struct"_test = [] {
      // Map with struct values containing vectors
      std::string buffer;
      std::map<int32_t, MapVariableValue> m = {
         {1, {"first", {1, 2, 3}}},
         {2, {"second", {4, 5}}},
         {3, {"third", {}}}
      };
      auto err = glz::write_zmem(m, buffer);
      expect(!err);

      std::map<int32_t, MapVariableValue> result;
      err = glz::read_zmem(result, buffer);
      expect(!err);
      expect(result.size() == 3u);
      expect(result[1].name == "first");
      expect(result[1].values == std::vector<int32_t>{1, 2, 3});
      expect(result[2].name == "second");
      expect(result[2].values == std::vector<int32_t>{4, 5});
      expect(result[3].name == "third");
      expect(result[3].values.empty());
   };
};

suite layout_tests = [] {
   "optional_layout_u8"_test = [] {
      expect(sizeof(glz::zmem::optional<uint8_t>) == 2u);
   };

   "optional_layout_u16"_test = [] {
      expect(sizeof(glz::zmem::optional<uint16_t>) == 4u);
   };

   "optional_layout_u32"_test = [] {
      expect(sizeof(glz::zmem::optional<uint32_t>) == 8u);
   };

   "optional_layout_u64"_test = [] {
      expect(sizeof(glz::zmem::optional<uint64_t>) == 16u);
   };

   "optional_alignment_u32"_test = [] {
      expect(alignof(glz::zmem::optional<uint32_t>) == 4u);
   };

   "optional_alignment_u64"_test = [] {
      expect(alignof(glz::zmem::optional<uint64_t>) == 8u);
   };

   "vector_ref_size"_test = [] {
      expect(sizeof(glz::zmem::vector_ref) == 16u);
   };

   "string_ref_size"_test = [] {
      expect(sizeof(glz::zmem::string_ref) == 16u);
   };
};

suite wire_format_tests = [] {
   "fixed_struct_exact_bytes"_test = [] {
      Point p{1.0f, 2.0f};
      std::string buffer;
      auto err = glz::write_zmem(p, buffer);
      expect(!err);

      // Should be identical to memcpy
      expect(buffer.size() == sizeof(Point));
      Point direct;
      std::memcpy(&direct, buffer.data(), sizeof(Point));
      expect(direct.x == p.x);
      expect(direct.y == p.y);
   };

   "vector_header_format"_test = [] {
      std::vector<uint32_t> v = {1, 2, 3};
      std::string buffer;
      auto err = glz::write_zmem(v, buffer);
      expect(!err);

      // First 8 bytes should be count
      uint64_t count;
      std::memcpy(&count, buffer.data(), 8);
      glz::zmem::byteswap_le(count);
      expect(count == 3u);

      // Total size: 8 (count) + 3 * 4 (elements)
      expect(buffer.size() == 8 + 3 * sizeof(uint32_t));
   };

   "little_endian_encoding"_test = [] {
      uint32_t value = 0x12345678;
      std::string buffer;
      auto err = glz::write_zmem(value, buffer);
      expect(!err);

      // Little-endian: least significant byte first
      expect(static_cast<uint8_t>(buffer[0]) == 0x78u);
      expect(static_cast<uint8_t>(buffer[1]) == 0x56u);
      expect(static_cast<uint8_t>(buffer[2]) == 0x34u);
      expect(static_cast<uint8_t>(buffer[3]) == 0x12u);
   };
};

// ============================================================================
// glaze_object_t Tests (types with glz::meta metadata)
// ============================================================================

// Fixed struct with explicit glaze metadata (not reflectable)
struct MetaPoint {
   float x_coord;
   float y_coord;
};

template <>
struct glz::meta<MetaPoint> {
   using T = MetaPoint;
   static constexpr auto value = object(
      "x", &T::x_coord,
      "y", &T::y_coord
   );
};

// Variable struct with glaze metadata
struct MetaEntity {
   uint64_t entity_id;
   std::string entity_name;
   std::vector<int32_t> entity_tags;
};

template <>
struct glz::meta<MetaEntity> {
   using T = MetaEntity;
   static constexpr auto value = object(
      "id", &T::entity_id,
      "name", &T::entity_name,
      "tags", &T::entity_tags
   );
};

// Nested glaze_object_t types
struct MetaTransform {
   MetaPoint position;
   float rotation;
   float scale;
};

template <>
struct glz::meta<MetaTransform> {
   using T = MetaTransform;
   static constexpr auto value = object(
      "pos", &T::position,
      "rot", &T::rotation,
      "scale", &T::scale
   );
};

suite glaze_object_tests = [] {
   "meta_fixed_struct_roundtrip"_test = [] {
      std::string buffer;
      MetaPoint p{3.14f, 2.71f};
      auto err = glz::write_zmem(p, buffer);
      expect(!err);
      // Fixed glaze_object_t should have zero overhead (same as reflectable)
      expect(buffer.size() == sizeof(MetaPoint)) << "Fixed meta struct should have zero overhead";

      MetaPoint result{};
      err = glz::read_zmem(result, buffer);
      expect(!err);
      expect(result.x_coord == p.x_coord);
      expect(result.y_coord == p.y_coord);
   };

   "meta_variable_struct_roundtrip"_test = [] {
      std::string buffer;
      MetaEntity entity{42, "TestEntity", {1, 2, 3, 4, 5}};
      auto err = glz::write_zmem(entity, buffer);
      expect(!err);

      MetaEntity result{};
      err = glz::read_zmem(result, buffer);
      expect(!err);
      expect(result.entity_id == entity.entity_id);
      expect(result.entity_name == entity.entity_name);
      expect(result.entity_tags == entity.entity_tags);
   };

   "meta_nested_struct_roundtrip"_test = [] {
      std::string buffer;
      MetaTransform xform{{1.0f, 2.0f}, 45.0f, 1.5f};
      auto err = glz::write_zmem(xform, buffer);
      expect(!err);

      MetaTransform result{};
      err = glz::read_zmem(result, buffer);
      expect(!err);
      expect(result.position.x_coord == xform.position.x_coord);
      expect(result.position.y_coord == xform.position.y_coord);
      expect(result.rotation == xform.rotation);
      expect(result.scale == xform.scale);
   };

   "meta_size_computation"_test = [] {
      MetaEntity entity{100, "HelloWorld", {10, 20, 30}};

      // Compute expected size and verify size_zmem
      const size_t computed_size = glz::size_zmem(entity);

      std::string buffer;
      auto err = glz::write_zmem(entity, buffer);
      expect(!err);
      expect(buffer.size() == computed_size) << "size_zmem should match actual serialized size";
   };

   "meta_preallocated_write"_test = [] {
      MetaEntity entity{999, "PreallocTest", {100, 200, 300, 400}};

      std::string buffer;
      auto err = glz::write_zmem_preallocated(entity, buffer);
      expect(!err);

      MetaEntity result{};
      err = glz::read_zmem(result, buffer);
      expect(!err);
      expect(result.entity_id == entity.entity_id);
      expect(result.entity_name == entity.entity_name);
      expect(result.entity_tags == entity.entity_tags);
   };
};

// ============================================================================
// lazy_zmem Tests (zero-copy views)
// ============================================================================

// Variable struct for lazy view testing
struct Person {
   uint64_t id;
   std::string name;
   std::vector<int32_t> scores;
};

// Nested variable struct
struct Team {
   std::string team_name;
   std::vector<float> ratings;
};

struct Organization {
   uint64_t org_id;
   Team team;
   std::string description;
};

suite lazy_zmem_tests = [] {
   "lazy_fixed_struct_as_fixed"_test = [] {
      Point p{3.14f, 2.71f};
      std::string buffer;
      auto err = glz::write_zmem(p, buffer);
      expect(!err);

      auto view = glz::lazy_zmem<Point>(buffer);
      expect(view.valid());
      expect(view.size() == buffer.size());

      const Point& ref = view.as_fixed();
      expect(ref.x == p.x);
      expect(ref.y == p.y);
   };

   "lazy_fixed_struct_get_fields"_test = [] {
      Point p{1.5f, 2.5f};
      std::string buffer;
      auto err = glz::write_zmem(p, buffer);
      expect(!err);

      auto view = glz::lazy_zmem<Point>(buffer);
      expect(view.get<0>() == p.x);
      expect(view.get<1>() == p.y);
   };

   "lazy_fixed_nested_struct"_test = [] {
      Transform t{{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
      std::string buffer;
      auto err = glz::write_zmem(t, buffer);
      expect(!err);

      auto view = glz::lazy_zmem<Transform>(buffer);
      const Transform& ref = view.as_fixed();
      expect(ref.position.x == t.position.x);
      expect(ref.rotation.y == t.rotation.y);
      expect(ref.scale.z == t.scale.z);
   };

   "lazy_variable_struct_string"_test = [] {
      LogEntry entry{12345, "Hello, lazy world!"};
      std::string buffer;
      auto err = glz::write_zmem(entry, buffer);
      expect(!err);

      auto view = glz::lazy_zmem<LogEntry>(buffer);
      expect(view.valid());

      // Get timestamp (fixed field)
      uint64_t ts = view.get<0>();
      expect(ts == entry.timestamp);

      // Get message as string_view (zero-copy)
      std::string_view msg = view.get<1>();
      expect(msg == entry.message);
   };

   "lazy_variable_struct_vector"_test = [] {
      Entity entity{42, {1.0f, 2.0f, 3.0f, 4.0f, 5.0f}};
      std::string buffer;
      auto err = glz::write_zmem(entity, buffer);
      expect(!err);

      auto view = glz::lazy_zmem<Entity>(buffer);
      expect(view.valid());

      // Get id (fixed field)
      uint64_t id = view.get<0>();
      expect(id == entity.id);

      // Get weights as span (zero-copy)
      std::span<const float> weights = view.get<1>();
      expect(weights.size() == entity.weights.size());
      for (size_t i = 0; i < weights.size(); ++i) {
         expect(weights[i] == entity.weights[i]);
      }
   };

   "lazy_variable_struct_multiple_fields"_test = [] {
      Person person{100, "Alice", {95, 87, 92, 88}};
      std::string buffer;
      auto err = glz::write_zmem(person, buffer);
      expect(!err);

      auto view = glz::lazy_zmem<Person>(buffer);

      uint64_t id = view.get<0>();
      expect(id == person.id);

      std::string_view name = view.get<1>();
      expect(name == person.name);

      std::span<const int32_t> scores = view.get<2>();
      expect(scores.size() == person.scores.size());
      expect(scores[0] == 95);
      expect(scores[3] == 88);
   };

   "lazy_empty_string"_test = [] {
      LogEntry entry{999, ""};
      std::string buffer;
      auto err = glz::write_zmem(entry, buffer);
      expect(!err);

      auto view = glz::lazy_zmem<LogEntry>(buffer);
      std::string_view msg = view.get<1>();
      expect(msg.empty());
   };

   "lazy_empty_vector"_test = [] {
      Entity entity{123, {}};
      std::string buffer;
      auto err = glz::write_zmem(entity, buffer);
      expect(!err);

      auto view = glz::lazy_zmem<Entity>(buffer);
      std::span<const float> weights = view.get<1>();
      expect(weights.empty());
   };

   "lazy_nested_variable_struct"_test = [] {
      Organization org{
         42,
         {"Engineering", {4.5f, 4.8f, 4.2f}},
         "Software development team"
      };
      std::string buffer;
      auto err = glz::write_zmem(org, buffer);
      expect(!err);

      auto view = glz::lazy_zmem<Organization>(buffer);

      uint64_t org_id = view.get<0>();
      expect(org_id == org.org_id);

      // Get nested Team as a lazy view
      auto team_view = view.get<1>();
      std::string_view team_name = team_view.get<0>();
      expect(team_name == org.team.team_name);

      std::span<const float> ratings = team_view.get<1>();
      expect(ratings.size() == org.team.ratings.size());
      expect(ratings[0] == 4.5f);

      std::string_view desc = view.get<2>();
      expect(desc == org.description);
   };

   "lazy_from_raw_pointer"_test = [] {
      Point p{10.0f, 20.0f};
      std::string buffer;
      auto err = glz::write_zmem(p, buffer);
      expect(!err);

      auto view = glz::lazy_zmem<Point>(buffer.data(), buffer.size());
      expect(view.valid());
      expect(view.as_fixed().x == p.x);
   };

   "lazy_from_void_pointer"_test = [] {
      Point p{30.0f, 40.0f};
      std::string buffer;
      auto err = glz::write_zmem(p, buffer);
      expect(!err);

      const void* data = buffer.data();
      auto view = glz::lazy_zmem<Point>(data, buffer.size());
      expect(view.valid());
      expect(view.as_fixed().y == p.y);
   };

   "lazy_meta_fixed_struct"_test = [] {
      MetaPoint p{1.0f, 2.0f};
      std::string buffer;
      auto err = glz::write_zmem(p, buffer);
      expect(!err);

      auto view = glz::lazy_zmem<MetaPoint>(buffer);
      const MetaPoint& ref = view.as_fixed();
      expect(ref.x_coord == p.x_coord);
      expect(ref.y_coord == p.y_coord);
   };

   "lazy_meta_variable_struct"_test = [] {
      MetaEntity entity{777, "LazyMeta", {10, 20, 30}};
      std::string buffer;
      auto err = glz::write_zmem(entity, buffer);
      expect(!err);

      auto view = glz::lazy_zmem<MetaEntity>(buffer);

      uint64_t id = view.get<0>();
      expect(id == entity.entity_id);

      std::string_view name = view.get<1>();
      expect(name == entity.entity_name);

      std::span<const int32_t> tags = view.get<2>();
      expect(tags.size() == entity.entity_tags.size());
      expect(tags[0] == 10);
      expect(tags[2] == 30);
   };

   "lazy_long_string"_test = [] {
      std::string long_message(10000, 'X');
      LogEntry entry{1, long_message};
      std::string buffer;
      auto err = glz::write_zmem(entry, buffer);
      expect(!err);

      auto view = glz::lazy_zmem<LogEntry>(buffer);
      std::string_view msg = view.get<1>();
      expect(msg.size() == 10000u);
      expect(msg == long_message);
   };

   "lazy_large_vector"_test = [] {
      Entity entity{1, {}};
      entity.weights.resize(10000);
      for (size_t i = 0; i < entity.weights.size(); ++i) {
         entity.weights[i] = static_cast<float>(i);
      }
      std::string buffer;
      auto err = glz::write_zmem(entity, buffer);
      expect(!err);

      auto view = glz::lazy_zmem<Entity>(buffer);
      std::span<const float> weights = view.get<1>();
      expect(weights.size() == 10000u);
      expect(weights[0] == 0.0f);
      expect(weights[9999] == 9999.0f);
   };
};

int main() {}
