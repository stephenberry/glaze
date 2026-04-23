// Glaze Library
// For the license information refer to glaze.hpp

#include "glaze/bson.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <variant>
#include <vector>

#include "ut/ut.hpp"

using namespace ut;

namespace bson_test
{
   // --- helpers -------------------------------------------------------------

   // Compare a written BSON buffer against an expected byte sequence.
   inline bool bytes_equal(const std::string& got, std::initializer_list<uint8_t> want)
   {
      if (got.size() != want.size()) return false;
      size_t i = 0;
      for (auto w : want) {
         if (static_cast<uint8_t>(got[i++]) != w) return false;
      }
      return true;
   }

   template <class T>
   inline void expect_roundtrip_equal(const T& original)
   {
      auto encoded = glz::write_bson(original);
      if (!encoded) {
         expect(false) << "write_bson failed: " << glz::format_error(encoded.error());
         return;
      }
      T decoded{};
      auto ec = glz::read_bson(decoded, std::string_view{encoded.value()});
      if (ec) {
         expect(false) << "read_bson failed: " << glz::format_error(ec, encoded.value());
         return;
      }
      expect(decoded == original);
   }

   // --- test types (file-scope so reflection can see them) -----------------

   struct hello_t
   {
      std::string hello{};
      bool operator==(const hello_t&) const = default;
   };

   struct i32_s
   {
      int32_t x{};
      bool operator==(const i32_s&) const = default;
   };

   struct i64_s
   {
      int64_t x{};
      bool operator==(const i64_s&) const = default;
   };

   struct u32_s
   {
      uint32_t x{};
      bool operator==(const u32_s&) const = default;
   };

   struct u64_s
   {
      uint64_t x{};
      bool operator==(const u64_s&) const = default;
   };

   struct u8_s
   {
      uint8_t x{};
      bool operator==(const u8_s&) const = default;
   };

   struct u16_s
   {
      uint16_t x{};
      bool operator==(const u16_s&) const = default;
   };

   struct i8_s
   {
      int8_t x{};
      bool operator==(const i8_s&) const = default;
   };

   struct bool_s
   {
      bool b{};
      bool operator==(const bool_s&) const = default;
   };

   struct double_s
   {
      double d{};
      bool operator==(const double_s&) const = default;
   };

   struct array_s
   {
      std::vector<int32_t> a{};
      bool operator==(const array_s&) const = default;
   };

   struct fixed_array_s
   {
      std::array<int32_t, 3> a{};
      bool operator==(const fixed_array_s&) const = default;
   };

   struct optional_s
   {
      std::optional<int32_t> o{};
      bool operator==(const optional_s&) const = default;
   };

   struct inner_s
   {
      int32_t x{};
      bool operator==(const inner_s&) const = default;
   };

   struct outer_s
   {
      inner_s inner{};
      bool operator==(const outer_s&) const = default;
   };

   struct uuid_s
   {
      glz::uuid u{};
      bool operator==(const uuid_s&) const = default;
   };

   struct time_s
   {
      std::chrono::system_clock::time_point t{};
      bool operator==(const time_s&) const = default;
   };

   struct oid_s
   {
      glz::bson::object_id id{};
      bool operator==(const oid_s&) const = default;
   };

   struct person_s
   {
      std::string name{};
      int32_t age{};
      bool operator==(const person_s&) const = default;
   };

   struct team_s
   {
      std::vector<person_s> members{};
      bool operator==(const team_s&) const = default;
   };

   struct big_s
   {
      int32_t a{};
      int32_t b{};
      int32_t c{};
   };

   struct small_s
   {
      int32_t b{};
   };

   struct helpers_s
   {
      glz::bson::datetime when{};
      glz::bson::timestamp ts{};
      glz::bson::regex re{};
      glz::bson::javascript js{};
      glz::bson::decimal128 d128{};
      glz::bson::min_key mn{};
      glz::bson::max_key mx{};
      glz::bson::binary<std::vector<std::byte>> blob{};

      bool operator==(const helpers_s& other) const
      {
         return when == other.when && ts == other.ts && re == other.re && js == other.js && d128 == other.d128 &&
                mn == other.mn && mx == other.mx && blob == other.blob;
      }
   };

   // Variant types -----------------------------------------------------------

   struct int_or_string_s
   {
      std::variant<int32_t, std::string> v{};
      bool operator==(const int_or_string_s&) const = default;
   };

   struct maybe_int_s
   {
      std::variant<std::monostate, int32_t> v{};
      bool operator==(const maybe_int_s&) const = default;
   };

   struct num_s
   {
      std::variant<int32_t, double> v{};
      bool operator==(const num_s&) const = default;
   };

   struct scalar_or_array_s
   {
      std::variant<int32_t, std::vector<int32_t>> v{};
      bool operator==(const scalar_or_array_s&) const = default;
   };

   struct id_or_time_s
   {
      std::variant<glz::bson::object_id, glz::bson::datetime> v{};
      bool operator==(const id_or_time_s& other) const { return v == other.v; }
   };

   // --- Single-helper wrapper structs for byte-exact assertions -------------

   struct oid_only_s
   {
      glz::bson::object_id id{};
      bool operator==(const oid_only_s&) const = default;
   };

   struct regex_only_s
   {
      glz::bson::regex r{};
      bool operator==(const regex_only_s&) const = default;
   };

   struct js_only_s
   {
      glz::bson::javascript j{};
      bool operator==(const js_only_s&) const = default;
   };

   struct dec128_only_s
   {
      glz::bson::decimal128 d{};
      bool operator==(const dec128_only_s&) const = default;
   };

   struct min_only_s
   {
      glz::bson::min_key mn{};
      bool operator==(const min_only_s&) const = default;
   };

   struct max_only_s
   {
      glz::bson::max_key mx{};
      bool operator==(const max_only_s&) const = default;
   };

   struct bin_only_s
   {
      glz::bson::binary<std::vector<std::byte>> b{};
      bool operator==(const bin_only_s&) const = default;
   };

   // --- Types exercising monostate, nesting, required keys ------------------

   struct monostate_field_s
   {
      std::monostate m{};
      int32_t x{};

      bool operator==(const monostate_field_s& other) const { return x == other.x; }
   };

   struct nested_vec_s
   {
      std::vector<std::vector<int32_t>> rows{};
      bool operator==(const nested_vec_s&) const = default;
   };

   struct nested_opt_s
   {
      std::optional<std::optional<int32_t>> oo{};
      bool operator==(const nested_opt_s&) const = default;
   };

   struct required_s
   {
      int32_t id{};
      std::optional<int32_t> opt{};
      bool operator==(const required_s&) const = default;
   };

   // --- Helpers for bson_to_json tests (namespace-scope so reflection works) -

   struct prim_doc_s
   {
      int32_t a{};
      int64_t b{};
      double c{};
      bool d{};
      std::string e{};
   };

   struct dt_field_s
   {
      glz::bson::datetime when{};
   };

   struct ts_field_s
   {
      glz::bson::timestamp ts{};
   };

   struct deep_s
   {
      std::optional<std::string> a{};
      bool operator==(const deep_s&) const = default;
   };

   // --- Types exercising glz::meta-annotated (non-aggregate) reflection ------

   struct meta_renamed_s
   {
      int32_t id{};
      std::string label{};
      bool operator==(const meta_renamed_s&) const = default;
   };

   struct meta_nested_s
   {
      meta_renamed_s child{};
      double value{};
      bool operator==(const meta_nested_s&) const = default;
   };

} // namespace bson_test

template <>
struct glz::meta<bson_test::meta_renamed_s>
{
   using T = bson_test::meta_renamed_s;
   static constexpr auto value = glz::object("identifier", &T::id, "display_name", &T::label);
};

template <>
struct glz::meta<bson_test::meta_nested_s>
{
   using T = bson_test::meta_nested_s;
   static constexpr auto value = glz::object("child", &T::child, "v", &T::value);
};

using namespace bson_test;

namespace
{
   suite bson_interop_tests = [] {
      "spec-canonical-hello-world"_test = [] {
         // {"hello": "world"} — the canonical example from bsonspec.org.
         static constexpr std::initializer_list<uint8_t> expected = {
            0x16, 0x00, 0x00, 0x00,                 //
            0x02, 'h', 'e', 'l', 'l', 'o', 0x00,    //
            0x06, 0x00, 0x00, 0x00,                 //
            'w', 'o', 'r', 'l', 'd', 0x00,          //
            0x00                                    //
         };
         hello_t h{"world"};
         auto w = glz::write_bson(h);
         expect(w.has_value());
         expect(bytes_equal(w.value(), expected));
      };

      "int32-bytes"_test = [] {
         i32_s v{42};
         auto w = glz::write_bson(v);
         expect(w.has_value());
         expect(bytes_equal(w.value(),
                            {0x0C, 0x00, 0x00, 0x00, 0x10, 'x', 0x00, 0x2A, 0x00, 0x00, 0x00, 0x00}));
      };

      "int64-bytes"_test = [] {
         i64_s v{1000000000000LL};
         auto w = glz::write_bson(v);
         expect(w.has_value());
         expect(bytes_equal(w.value(), {0x10, 0x00, 0x00, 0x00, 0x12, 'x', 0x00, 0x00, 0x10, 0xA5, 0xD4, 0xE8,
                                        0x00, 0x00, 0x00, 0x00}));
      };

      "double-bytes"_test = [] {
         double_s v{1.5};
         auto w = glz::write_bson(v);
         expect(w.has_value());
         // 1.5 as IEEE 754 double → 0x3FF8000000000000, little-endian.
         expect(bytes_equal(w.value(), {0x10, 0x00, 0x00, 0x00, 0x01, 'd', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                        0x00, 0xF8, 0x3F, 0x00}));
      };

      "bool-bytes"_test = [] {
         bool_s v{true};
         auto w = glz::write_bson(v);
         expect(w.has_value());
         expect(bytes_equal(w.value(), {0x09, 0x00, 0x00, 0x00, 0x08, 'b', 0x00, 0x01, 0x00}));
      };

      "array-bytes"_test = [] {
         array_s v{{1, 2}};
         auto w = glz::write_bson(v);
         expect(w.has_value());
         expect(bytes_equal(w.value(), {0x1B, 0x00, 0x00, 0x00,                                   //
                                        0x04, 'a', 0x00,                                          //
                                        0x13, 0x00, 0x00, 0x00,                                   //
                                        0x10, '0', 0x00, 0x01, 0x00, 0x00, 0x00,                  //
                                        0x10, '1', 0x00, 0x02, 0x00, 0x00, 0x00,                  //
                                        0x00,                                                     //
                                        0x00}));
      };

      "optional-full-bytes"_test = [] {
         optional_s v{42};
         auto w = glz::write_bson(v);
         expect(w.has_value());
         expect(bytes_equal(w.value(),
                            {0x0C, 0x00, 0x00, 0x00, 0x10, 'o', 0x00, 0x2A, 0x00, 0x00, 0x00, 0x00}));
      };

      "optional-empty-skips-key"_test = [] {
         // Default skip_null_members=true, so absent optional leaves only the
         // empty 5-byte document.
         optional_s v{};
         auto w = glz::write_bson(v);
         expect(w.has_value());
         expect(bytes_equal(w.value(), {0x05, 0x00, 0x00, 0x00, 0x00}));
      };

      "uuid-binary-subtype-04"_test = [] {
         uuid_s v{};
         expect(glz::parse_uuid("00112233-4455-6677-8899-aabbccddeeff", v.u));
         auto w = glz::write_bson(v);
         expect(w.has_value());
         expect(bytes_equal(w.value(), {0x1D, 0x00, 0x00, 0x00,                                      //
                                        0x05, 'u', 0x00,                                             //
                                        0x10, 0x00, 0x00, 0x00, 0x04,                                //
                                        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99,  //
                                        0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,                          //
                                        0x00}));
      };

      "uuid-legacy-subtype-03-rejected"_test = [] {
         // Subtype 0x03 is ambiguous — Java, C#, and Python drivers all laid
         // the bytes out differently. Reading without the flavor would silently
         // scramble the UUID, so Glaze rejects it outright.
         static constexpr uint8_t buf[] = {0x1D, 0x00, 0x00, 0x00, //
                                           0x05, 'u', 0x00,        //
                                           0x10, 0x00, 0x00, 0x00, 0x03, //
                                           0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99,
                                           0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,  //
                                           0x00};
         uuid_s r{};
         auto ec = glz::read_bson(r, std::string_view{reinterpret_cast<const char*>(buf), sizeof(buf)});
         expect(bool(ec));
         expect(ec.ec == glz::error_code::syntax_error);
      };

      "object-id-bytes"_test = [] {
         oid_only_s v{};
         for (uint8_t i = 0; i < 12; ++i) v.id.bytes[i] = i;
         auto w = glz::write_bson(v);
         expect(w.has_value());
         expect(bytes_equal(w.value(), {0x15, 0x00, 0x00, 0x00,                                             //
                                        0x07, 'i', 'd', 0x00,                                              //
                                        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,  //
                                        0x0B,                                                              //
                                        0x00}));
      };

      "regex-bytes"_test = [] {
         regex_only_s v{};
         v.r.pattern = "abc";
         v.r.options = "i";
         auto w = glz::write_bson(v);
         expect(w.has_value());
         // pattern cstring then options cstring.
         expect(bytes_equal(w.value(), {0x0E, 0x00, 0x00, 0x00,        //
                                        0x0B, 'r', 0x00,               //
                                        'a', 'b', 'c', 0x00, 'i', 0x00, //
                                        0x00}));
      };

      "regex-options-sorted"_test = [] {
         // BSON spec requires regex options stored in alphabetical order.
         // Input "mi" must be emitted as "im" on the wire.
         regex_only_s v{};
         v.r.pattern = "a";
         v.r.options = "mi";
         auto w = glz::write_bson(v);
         expect(w.has_value());
         expect(bytes_equal(w.value(), {0x0D, 0x00, 0x00, 0x00,              //
                                        0x0B, 'r', 0x00,                     //
                                        'a', 0x00, 'i', 'm', 0x00,           //
                                        0x00}));
      };

      "javascript-bytes"_test = [] {
         js_only_s v{};
         v.j.code = "x";
         auto w = glz::write_bson(v);
         expect(w.has_value());
         // int32 length (bytes+1 for null) then code then null.
         expect(bytes_equal(w.value(), {0x0E, 0x00, 0x00, 0x00,              //
                                        0x0D, 'j', 0x00,                     //
                                        0x02, 0x00, 0x00, 0x00, 'x', 0x00,   //
                                        0x00}));
      };

      "decimal128-bytes"_test = [] {
         dec128_only_s v{};
         // Opaque 16-byte payload: 0x01 ascending, to prove byte order is preserved.
         for (uint8_t i = 0; i < 16; ++i) v.d.bytes[i] = uint8_t(0x01 + i);
         auto w = glz::write_bson(v);
         expect(w.has_value());
         // Doc length = 4 (len) + 3 (tag+"d"+NUL) + 16 (value) + 1 (term) = 24 = 0x18.
         expect(bytes_equal(w.value(), {0x18, 0x00, 0x00, 0x00,                                           //
                                        0x13, 'd', 0x00,                                                  //
                                        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, //
                                        0x0C, 0x0D, 0x0E, 0x0F, 0x10,                                     //
                                        0x00}));
      };

      "min-key-bytes"_test = [] {
         min_only_s v{};
         auto w = glz::write_bson(v);
         expect(w.has_value());
         // Doc length = 4 + 1 (tag) + 2 ("mn") + 1 (NUL) + 0 (no value) + 1 (term) = 9.
         expect(bytes_equal(w.value(), {0x09, 0x00, 0x00, 0x00, 0xFF, 'm', 'n', 0x00, 0x00}));
      };

      "max-key-bytes"_test = [] {
         max_only_s v{};
         auto w = glz::write_bson(v);
         expect(w.has_value());
         // Same framing as min_key with tag 0x7F.
         expect(bytes_equal(w.value(), {0x09, 0x00, 0x00, 0x00, 0x7F, 'm', 'x', 0x00, 0x00}));
      };

      "binary-generic-subtype-bytes"_test = [] {
         bin_only_s v{};
         v.b.subtype = glz::bson::binary_subtype::generic;
         v.b.data = {std::byte{'a'}, std::byte{'b'}, std::byte{'c'}};
         auto w = glz::write_bson(v);
         expect(w.has_value());
         // length 3, subtype 0x00, payload "abc".
         expect(bytes_equal(w.value(), {0x10, 0x00, 0x00, 0x00,                         //
                                        0x05, 'b', 0x00,                                //
                                        0x03, 0x00, 0x00, 0x00, 0x00, 'a', 'b', 'c',    //
                                        0x00}));
      };

      "binary-old-subtype-02-bytes"_test = [] {
         // Subtype 0x02 is spec-required to wrap the payload in a redundant
         // inner int32 length: outer = 4 + N, then subtype, then int32(N),
         // then N payload bytes.
         bin_only_s v{};
         v.b.subtype = glz::bson::binary_subtype::binary_old;
         v.b.data = {std::byte{'a'}, std::byte{'b'}, std::byte{'c'}};
         auto w = glz::write_bson(v);
         expect(w.has_value());
         // Doc length = 4 (outer len field) + 3 (tag+"b"+NUL) + 4 (outer len)
         // + 1 (subtype) + 4 (inner len) + 3 (payload) + 1 (term) = 20 = 0x14.
         expect(bytes_equal(w.value(), {0x14, 0x00, 0x00, 0x00,              //
                                        0x05, 'b', 0x00,                     //
                                        0x07, 0x00, 0x00, 0x00,              //
                                        0x02,                                //
                                        0x03, 0x00, 0x00, 0x00,              //
                                        'a', 'b', 'c',                       //
                                        0x00}));
      };
   };

   suite bson_roundtrip_tests = [] {
      "roundtrip-primitives"_test = [] {
         expect_roundtrip_equal(hello_t{"world"});
         expect_roundtrip_equal(i32_s{42});
         expect_roundtrip_equal(i32_s{-2000000000});
         expect_roundtrip_equal(i64_s{1000000000000LL});
         expect_roundtrip_equal(i64_s{-9000000000000LL});
         expect_roundtrip_equal(bool_s{true});
         expect_roundtrip_equal(bool_s{false});
         expect_roundtrip_equal(double_s{3.14159});
         expect_roundtrip_equal(double_s{-2.5e10});
      };

      "roundtrip-containers"_test = [] {
         expect_roundtrip_equal(array_s{{}});
         expect_roundtrip_equal(array_s{{1, 2, 3, 4, 5}});
         expect_roundtrip_equal(outer_s{{7}});
         expect_roundtrip_equal(team_s{{{"alice", 30}, {"bob", 25}}});
      };

      "roundtrip-optional"_test = [] {
         expect_roundtrip_equal(optional_s{42});
         expect_roundtrip_equal(optional_s{-1});
         // optional-empty: read leaves default, which matches original.
         expect_roundtrip_equal(optional_s{});
      };

      "roundtrip-uuid"_test = [] {
         uuid_s v{};
         expect(glz::parse_uuid("550e8400-e29b-41d4-a716-446655440000", v.u));
         expect_roundtrip_equal(v);
      };

      "roundtrip-chrono"_test = [] {
         using namespace std::chrono;
         time_s v{system_clock::time_point{milliseconds{1700000000000LL}}};
         expect_roundtrip_equal(v);
      };

      "roundtrip-object_id"_test = [] {
         oid_s v{};
         for (int i = 0; i < 12; ++i) v.id.bytes[i] = static_cast<uint8_t>(i * 17);
         expect_roundtrip_equal(v);
      };

      "roundtrip-map"_test = [] {
         std::map<std::string, int32_t> m{{"alpha", 1}, {"beta", 2}, {"gamma", 3}};
         auto w = glz::write_bson(m);
         expect(w.has_value());
         std::map<std::string, int32_t> r{};
         auto ec = glz::read_bson(r, std::string_view{w.value()});
         expect(not ec);
         expect(r == m);
      };

      "map-duplicate-keys-last-writer-wins"_test = [] {
         // BSON does not forbid duplicate keys on the wire. Glaze's policy is
         // last-writer-wins (subsequent occurrences overwrite the prior value).
         // Hand-authored doc: {"k": 1, "k": 2}. Reader must yield {k: 2}.
         static constexpr uint8_t dup[] = {
            0x13, 0x00, 0x00, 0x00,                  // doc length = 4+7+7+1 = 19
            0x10, 'k', 0x00, 0x01, 0x00, 0x00, 0x00, //
            0x10, 'k', 0x00, 0x02, 0x00, 0x00, 0x00, //
            0x00};
         std::map<std::string, int32_t> r{};
         auto ec = glz::read_bson(r, std::string_view{reinterpret_cast<const char*>(dup), sizeof(dup)});
         expect(not ec);
         expect(r.size() == 1);
         expect(r.at("k") == 2);
      };

      "roundtrip-helpers"_test = [] {
         helpers_s v{};
         v.when.ms_since_epoch = 1700000000000LL;
         v.ts.increment = 7;
         v.ts.seconds = 1700000000;
         v.re.pattern = "^hello";
         v.re.options = "i";
         v.js.code = "function(x){ return x+1; }";
         for (int i = 0; i < 16; ++i) v.d128.bytes[i] = static_cast<uint8_t>(i);
         v.blob.data = {std::byte{0x01}, std::byte{0x02}, std::byte{0x03}, std::byte{0x04}};
         v.blob.subtype = glz::bson::binary_subtype::generic;
         expect_roundtrip_equal(v);
      };

      "roundtrip-binary-old-subtype-02"_test = [] {
         bin_only_s v{};
         v.b.subtype = glz::bson::binary_subtype::binary_old;
         v.b.data = {std::byte{0xDE}, std::byte{0xAD}, std::byte{0xBE}, std::byte{0xEF}};
         expect_roundtrip_equal(v);
      };

      "roundtrip-binary-function-subtype-01"_test = [] {
         // Subtype 0x01 (function) is framed identically to generic bytes on
         // the wire; the distinction is semantic only.
         bin_only_s v{};
         v.b.subtype = glz::bson::binary_subtype::function;
         v.b.data = {std::byte{'f'}, std::byte{'n'}, std::byte{0x00}, std::byte{0x42}};
         expect_roundtrip_equal(v);
      };

      "roundtrip-fixed-array"_test = [] {
         // std::array is fixed-capacity: not emplace_backable, not resizable.
         // The reader must dispatch to the indexed-fill else-branch.
         expect_roundtrip_equal(fixed_array_s{{10, 20, 30}});
      };

      "roundtrip-empty-string"_test = [] {
         // Edge case: BSON string with length prefix 1 (just the terminator).
         expect_roundtrip_equal(hello_t{""});
      };

      "roundtrip-top-level-fixed-array"_test = [] {
         // Top-level std::array must dispatch to the array reader even though
         // BSON's top-level container is always a "document" on the wire.
         std::array<int32_t, 3> src{11, 22, 33};
         auto w = glz::write_bson(src);
         expect(w.has_value());
         std::array<int32_t, 3> dst{};
         auto ec = glz::read_bson(dst, std::string_view{w.value()});
         expect(not ec);
         expect(dst == src);
      };

      "roundtrip-top-level-vector"_test = [] {
         std::vector<int32_t> src{7, 8, 9, 10};
         auto w = glz::write_bson(src);
         expect(w.has_value());
         std::vector<int32_t> dst{};
         auto ec = glz::read_bson(dst, std::string_view{w.value()});
         expect(not ec);
         expect(dst == src);
      };

      "fixed-array-wire-overflow-rejected"_test = [] {
         // Wire has 4 elements, target array has capacity 3 — must fail with
         // exceeded_static_array_size.
         array_s src{{1, 2, 3, 4}};
         auto w = glz::write_bson(src);
         expect(w.has_value());
         fixed_array_s dst{};
         auto ec = glz::read_bson(dst, std::string_view{w.value()});
         expect(ec.ec == glz::error_code::exceeded_static_array_size);
      };

      "read-binary-old-from-external-producer"_test = [] {
         // Spec-compliant 0x02 wire bytes hand-authored (what a MongoDB driver
         // would emit): outer_len = 4 + N, subtype 0x02, inner int32(N),
         // then N payload bytes.
         static constexpr uint8_t compliant[] = {
            0x14, 0x00, 0x00, 0x00,  // doc length
            0x05, 'b', 0x00,         // tag + key
            0x07, 0x00, 0x00, 0x00,  // outer_len = 4 + 3 = 7
            0x02,                    // subtype 0x02
            0x03, 0x00, 0x00, 0x00,  // inner_len = 3
            'x', 'y', 'z',           // payload
            0x00};
         bin_only_s dst{};
         auto ec = glz::read_bson(dst, std::string_view{reinterpret_cast<const char*>(compliant), sizeof(compliant)});
         expect(not ec);
         expect(dst.b.subtype == glz::bson::binary_subtype::binary_old);
         expect(dst.b.data.size() == 3);
         expect(dst.b.data[0] == std::byte{'x'});
         expect(dst.b.data[1] == std::byte{'y'});
         expect(dst.b.data[2] == std::byte{'z'});
      };

      "read-binary-old-inner-outer-mismatch-rejected"_test = [] {
         // Malformed 0x02: outer_len says 7 but inner_len says 5, so they
         // disagree. Reader must reject with syntax_error.
         static constexpr uint8_t bad[] = {
            0x14, 0x00, 0x00, 0x00,  //
            0x05, 'b', 0x00,         //
            0x07, 0x00, 0x00, 0x00,  // outer_len = 7 (implies inner = 3)
            0x02,                    //
            0x05, 0x00, 0x00, 0x00,  // inner_len = 5 (mismatch)
            'x', 'y', 'z',           //
            0x00};
         bin_only_s dst{};
         auto ec = glz::read_bson(dst, std::string_view{reinterpret_cast<const char*>(bad), sizeof(bad)});
         expect(ec.ec == glz::error_code::syntax_error);
      };
   };

   suite bson_coercion_tests = [] {
      "int64-wire-to-int32-target-in-range"_test = [] {
         i64_s src{42};
         auto w = glz::write_bson(src);
         expect(w.has_value());
         i32_s dst{};
         auto ec = glz::read_bson(dst, std::string_view{w.value()});
         expect(not ec);
         expect(dst.x == 42);
      };

      "int64-wire-to-int32-target-out-of-range"_test = [] {
         i64_s src{int64_t{1} << 40};
         auto w = glz::write_bson(src);
         expect(w.has_value());
         i32_s dst{};
         auto ec = glz::read_bson(dst, std::string_view{w.value()});
         expect(ec.ec == glz::error_code::parse_number_failure);
      };

      "int32-wire-to-int64-target"_test = [] {
         i32_s src{-5};
         auto w = glz::write_bson(src);
         expect(w.has_value());
         i64_s dst{};
         auto ec = glz::read_bson(dst, std::string_view{w.value()});
         expect(not ec);
         expect(dst.x == -5);
      };

      "int32-wire-to-uint32-target-non-negative"_test = [] {
         // External producer emits int32; target is uint32_t. Any non-negative
         // int32 fits and must not trip the range check.
         i32_s src{42};
         auto w = glz::write_bson(src);
         expect(w.has_value());
         u32_s dst{};
         auto ec = glz::read_bson(dst, std::string_view{w.value()});
         expect(not ec);
         expect(dst.x == 42u);
      };

      "int32-wire-to-uint32-target-negative-rejected"_test = [] {
         i32_s src{-1};
         auto w = glz::write_bson(src);
         expect(w.has_value());
         u32_s dst{};
         auto ec = glz::read_bson(dst, std::string_view{w.value()});
         expect(ec.ec == glz::error_code::parse_number_failure);
      };

      "int32-wire-to-uint64-target-non-negative"_test = [] {
         i32_s src{42};
         auto w = glz::write_bson(src);
         expect(w.has_value());
         u64_s dst{};
         auto ec = glz::read_bson(dst, std::string_view{w.value()});
         expect(not ec);
         expect(dst.x == 42u);
      };

      "int32-wire-to-uint64-target-negative-rejected"_test = [] {
         // Was silently producing UINT64_MAX before the fix.
         i32_s src{-1};
         auto w = glz::write_bson(src);
         expect(w.has_value());
         u64_s dst{};
         auto ec = glz::read_bson(dst, std::string_view{w.value()});
         expect(ec.ec == glz::error_code::parse_number_failure);
      };

      "int32-wire-to-uint16-target-in-range"_test = [] {
         i32_s src{1000};
         auto w = glz::write_bson(src);
         expect(w.has_value());
         u16_s dst{};
         auto ec = glz::read_bson(dst, std::string_view{w.value()});
         expect(not ec);
         expect(dst.x == 1000u);
      };

      "int32-wire-to-uint16-target-out-of-range"_test = [] {
         i32_s src{100000};
         auto w = glz::write_bson(src);
         expect(w.has_value());
         u16_s dst{};
         auto ec = glz::read_bson(dst, std::string_view{w.value()});
         expect(ec.ec == glz::error_code::parse_number_failure);
      };

      "int32-wire-to-uint16-target-negative-rejected"_test = [] {
         i32_s src{-1};
         auto w = glz::write_bson(src);
         expect(w.has_value());
         u16_s dst{};
         auto ec = glz::read_bson(dst, std::string_view{w.value()});
         expect(ec.ec == glz::error_code::parse_number_failure);
      };

      "int32-wire-to-uint8-target-in-range"_test = [] {
         i32_s src{200};
         auto w = glz::write_bson(src);
         expect(w.has_value());
         u8_s dst{};
         auto ec = glz::read_bson(dst, std::string_view{w.value()});
         expect(not ec);
         expect(dst.x == 200u);
      };

      "int32-wire-to-uint8-target-out-of-range"_test = [] {
         i32_s src{300};
         auto w = glz::write_bson(src);
         expect(w.has_value());
         u8_s dst{};
         auto ec = glz::read_bson(dst, std::string_view{w.value()});
         expect(ec.ec == glz::error_code::parse_number_failure);
      };

      "int32-wire-to-uint8-target-negative-rejected"_test = [] {
         i32_s src{-1};
         auto w = glz::write_bson(src);
         expect(w.has_value());
         u8_s dst{};
         auto ec = glz::read_bson(dst, std::string_view{w.value()});
         expect(ec.ec == glz::error_code::parse_number_failure);
      };

      "int64-wire-to-int8-target-out-of-range"_test = [] {
         i64_s src{1000};
         auto w = glz::write_bson(src);
         expect(w.has_value());
         i8_s dst{};
         auto ec = glz::read_bson(dst, std::string_view{w.value()});
         expect(ec.ec == glz::error_code::parse_number_failure);
      };

      "int64-wire-to-uint32-target-out-of-range"_test = [] {
         // Value above UINT32_MAX.
         i64_s src{int64_t{1} << 40};
         auto w = glz::write_bson(src);
         expect(w.has_value());
         u32_s dst{};
         auto ec = glz::read_bson(dst, std::string_view{w.value()});
         expect(ec.ec == glz::error_code::parse_number_failure);
      };

      "int64-wire-to-uint32-target-negative-rejected"_test = [] {
         i64_s src{-1};
         auto w = glz::write_bson(src);
         expect(w.has_value());
         u32_s dst{};
         auto ec = glz::read_bson(dst, std::string_view{w.value()});
         expect(ec.ec == glz::error_code::parse_number_failure);
      };

      "uint64-write-above-int64-max-rejected"_test = [] {
         // BSON's int64 wire type is signed; uint64_t values above INT64_MAX
         // cannot be represented and must fail the write path with
         // invalid_length (see write.hpp uint64_t range check).
         u64_s src{static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) + 1};
         auto w = glz::write_bson(src);
         expect(not w.has_value());
         expect(w.error().ec == glz::error_code::invalid_length);
      };

      "uint64-write-at-int64-max-ok"_test = [] {
         // Boundary: INT64_MAX itself must write successfully.
         u64_s src{static_cast<uint64_t>(std::numeric_limits<int64_t>::max())};
         auto w = glz::write_bson(src);
         expect(w.has_value());
         u64_s dst{};
         auto ec = glz::read_bson(dst, std::string_view{w.value()});
         expect(not ec);
         expect(dst.x == src.x);
      };
   };

   suite bson_unknown_key_tests = [] {
      "strict-default-rejects-unknown-keys"_test = [] {
         big_s big{1, 2, 3};
         auto w = glz::write_bson(big);
         expect(w.has_value());
         small_s dst{};
         auto ec = glz::read_bson(dst, std::string_view{w.value()});
         expect(ec.ec == glz::error_code::unknown_key);
      };

      "permissive-skips-unknown-keys"_test = [] {
         big_s big{1, 2, 3};
         auto w = glz::write_bson(big);
         expect(w.has_value());
         small_s dst{};
         constexpr auto permissive = glz::opts{.error_on_unknown_keys = false};
         auto ec = glz::read_bson<permissive>(dst, std::string_view{w.value()});
         expect(not ec);
         expect(dst.b == 2);
      };
   };

   suite bson_spec_parse_tests = [] {
      "parse-canonical-hello-world"_test = [] {
         static constexpr uint8_t hw[] = {
            0x16, 0x00, 0x00, 0x00,              //
            0x02, 'h', 'e', 'l', 'l', 'o', 0x00, //
            0x06, 0x00, 0x00, 0x00,              //
            'w', 'o', 'r', 'l', 'd', 0x00,       //
            0x00                                 //
         };
         std::string_view buf(reinterpret_cast<const char*>(hw), sizeof(hw));
         hello_t h{};
         auto ec = glz::read_bson(h, buf);
         expect(not ec);
         expect(h.hello == "world");
      };

      "empty-document"_test = [] {
         // {} — smallest legal document: just 4-byte length + terminator.
         static constexpr uint8_t empty_doc[] = {0x05, 0x00, 0x00, 0x00, 0x00};
         std::string_view buf(reinterpret_cast<const char*>(empty_doc), sizeof(empty_doc));
         hello_t h{"prefilled"};
         constexpr auto permissive = glz::opts{.error_on_unknown_keys = false};
         auto ec = glz::read_bson<permissive>(h, buf);
         expect(not ec);
         // no "hello" key present → field stays as it was
         expect(h.hello == "prefilled");
      };

      "negative-length-prefix-rejected"_test = [] {
         // Length field is 0xFFFFFFFF (−1 when read as int32). Must be rejected.
         static constexpr uint8_t bad[] = {0xFF, 0xFF, 0xFF, 0xFF, 0x00};
         std::string_view buf(reinterpret_cast<const char*>(bad), sizeof(bad));
         hello_t h{};
         auto ec = glz::read_bson(h, buf);
         expect(bool(ec));
      };

      "truncated-document"_test = [] {
         // Length says 22 but buffer only has 10 bytes.
         static constexpr uint8_t bad[] = {0x16, 0x00, 0x00, 0x00, 0x02, 'h', 'i', 0x00, 0x01, 0x00};
         std::string_view buf(reinterpret_cast<const char*>(bad), sizeof(bad));
         hello_t h{};
         auto ec = glz::read_bson(h, buf);
         expect(ec.ec == glz::error_code::unexpected_end);
      };

      "trailing-bytes-rejected"_test = [] {
         // Valid {"hello":"world"} document followed by a stray byte. The header
         // length covers only the first 22 bytes; anything past that is garbage.
         static constexpr uint8_t doc_plus_garbage[] = {
            0x16, 0x00, 0x00, 0x00,              //
            0x02, 'h', 'e', 'l', 'l', 'o', 0x00, //
            0x06, 0x00, 0x00, 0x00,              //
            'w', 'o', 'r', 'l', 'd', 0x00,       //
            0x00,                                //
            0xFF                                 // trailing garbage
         };
         std::string_view buf(reinterpret_cast<const char*>(doc_plus_garbage), sizeof(doc_plus_garbage));
         hello_t h{};
         auto ec = glz::read_bson(h, buf);
         expect(ec.ec == glz::error_code::syntax_error);
      };

      "exact-fill-accepted"_test = [] {
         // Same document without trailing bytes must still succeed.
         static constexpr uint8_t doc[] = {
            0x16, 0x00, 0x00, 0x00,              //
            0x02, 'h', 'e', 'l', 'l', 'o', 0x00, //
            0x06, 0x00, 0x00, 0x00,              //
            'w', 'o', 'r', 'l', 'd', 0x00,       //
            0x00                                 //
         };
         std::string_view buf(reinterpret_cast<const char*>(doc), sizeof(doc));
         hello_t h{};
         auto ec = glz::read_bson(h, buf);
         expect(not ec);
         expect(h.hello == "world");
      };
   };

   suite bson_write_api_tests = [] {
      "fixed-buffer-write"_test = [] {
         // Bounded buffers require at least 512 bytes to account for the write
         // padding reservation (see glaze/core/buffer_traits.hpp).
         std::array<char, 512> buf{};
         hello_t h{"world"};
         auto ec = glz::write_bson(h, buf);
         expect(not ec);
         expect(ec.count == 22);
      };

      "vector-byte-buffer-write"_test = [] {
         std::vector<std::byte> buf{};
         hello_t h{"world"};
         auto ec = glz::write_bson(h, buf);
         expect(not ec);
         expect(buf.size() == 22);
      };
   };

   suite bson_variant_tests = [] {
      "variant-int-or-string-roundtrip-int"_test = [] {
         expect_roundtrip_equal(int_or_string_s{42});
      };

      "variant-int-or-string-roundtrip-string"_test = [] {
         expect_roundtrip_equal(int_or_string_s{std::string{"hello"}});
      };

      "variant-monostate-roundtrip-absent"_test = [] {
         // Active alternative is std::monostate → null on the wire.
         expect_roundtrip_equal(maybe_int_s{});
      };

      "variant-monostate-roundtrip-int"_test = [] {
         maybe_int_s v{};
         v.v = int32_t{7};
         expect_roundtrip_equal(v);
      };

      "variant-monostate-writes-null-byte"_test = [] {
         // {v: null} — doc length 8 | 0x0A 'v' 0x00 | 0x00 (no value bytes for null).
         maybe_int_s v{};
         auto w = glz::write_bson(v);
         expect(w.has_value());
         expect(bytes_equal(w.value(),
                            {0x08, 0x00, 0x00, 0x00,  //
                             0x0A, 'v', 0x00,         //
                             0x00}));
      };

      "variant-num-int-and-double"_test = [] {
         // Variant with distinct int and float categories: both round-trip.
         num_s a{int32_t{11}};
         expect_roundtrip_equal(a);
         num_s b{3.14};
         expect_roundtrip_equal(b);
      };

      "variant-scalar-or-array"_test = [] {
         scalar_or_array_s a{int32_t{5}};
         expect_roundtrip_equal(a);
         scalar_or_array_s b{std::vector<int32_t>{1, 2, 3}};
         expect_roundtrip_equal(b);
      };

      "variant-helper-types"_test = [] {
         id_or_time_s a{};
         glz::bson::object_id oid{};
         for (int i = 0; i < 12; ++i) oid.bytes[i] = static_cast<uint8_t>(i);
         a.v = oid;
         expect_roundtrip_equal(a);

         id_or_time_s b{};
         b.v = glz::bson::datetime{1700000000000LL};
         expect_roundtrip_equal(b);
      };

      "variant-read-unknown-tag-errors"_test = [] {
         // Forge a document with a variant field holding a double (0x01),
         // but the variant has no float alternative → no_matching_variant_type.
         // int_or_string_s has int32 + string only. Manual bytes: length | 01 v 00 | double | 00.
         static constexpr uint8_t bad[] = {
            0x10, 0x00, 0x00, 0x00,                                              //
            0x01, 'v', 0x00,                                                     //
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF8, 0x3F,                      //
            0x00                                                                 //
         };
         std::string_view buf(reinterpret_cast<const char*>(bad), sizeof(bad));
         int_or_string_s v{};
         auto ec = glz::read_bson(v, buf);
         expect(ec.ec == glz::error_code::no_matching_variant_type);
      };
   };

   suite bson_map_skip_null_tests = [] {
      "map-skips-empty-optional-by-default"_test = [] {
         std::map<std::string, std::optional<int32_t>> in{{"a", 1}, {"b", std::nullopt}, {"c", 3}};
         std::vector<std::byte> buf{};
         auto wec = glz::write_bson(in, buf);
         expect(not wec);

         std::map<std::string, std::optional<int32_t>> out{};
         auto rec = glz::read_bson(out, buf);
         expect(not rec);
         expect(out.size() == 2);
         expect(out.count("b") == 0);
         expect(out.at("a") == 1);
         expect(out.at("c") == 3);
      };

      "map-preserves-nulls-when-skip-disabled"_test = [] {
         constexpr auto preserve = glz::opts{.skip_null_members = false};
         std::map<std::string, std::optional<int32_t>> in{{"a", 1}, {"b", std::nullopt}};
         std::vector<std::byte> buf{};
         auto wec = glz::write_bson<preserve>(in, buf);
         expect(not wec);

         std::map<std::string, std::optional<int32_t>> out{};
         auto rec = glz::read_bson<preserve>(out, buf);
         expect(not rec);
         expect(out.size() == 2);
         expect(out.at("a") == 1);
         expect(!out.at("b").has_value());
      };
   };

   // ===========================================================================
   // Deprecated element types: present on the wire, the reader must skip them
   // cleanly when the receiving type has no such key.
   // ===========================================================================
   suite bson_deprecated_type_skip_tests = [] {
      "skip-undefined"_test = [] {
         constexpr auto permissive = glz::opts{.error_on_unknown_keys = false};
         // {"u": undefined} — type tag 0x06 carries no value bytes.
         static constexpr uint8_t bytes[] = {0x08, 0x00, 0x00, 0x00, 0x06, 'u', 0x00, 0x00};
         std::string_view buf(reinterpret_cast<const char*>(bytes), sizeof(bytes));
         hello_t h{"pre"};
         auto ec = glz::read_bson<permissive>(h, buf);
         expect(not ec);
         expect(h.hello == "pre");
      };

      "skip-symbol"_test = [] {
         constexpr auto permissive = glz::opts{.error_on_unknown_keys = false};
         // {"s": symbol("x")} — symbol is length-prefixed UTF-8 like a string.
         static constexpr uint8_t bytes[] = {
            0x0E, 0x00, 0x00, 0x00,               //
            0x0E, 's', 0x00,                      //
            0x02, 0x00, 0x00, 0x00, 'x', 0x00,    //
            0x00                                  //
         };
         std::string_view buf(reinterpret_cast<const char*>(bytes), sizeof(bytes));
         hello_t h{"pre"};
         auto ec = glz::read_bson<permissive>(h, buf);
         expect(not ec);
         expect(h.hello == "pre");
      };

      "skip-db-pointer"_test = [] {
         constexpr auto permissive = glz::opts{.error_on_unknown_keys = false};
         // {"p": DBPointer("x", <12 zero bytes>)} — ns string + 12-byte object id.
         static constexpr uint8_t bytes[] = {
            0x1A, 0x00, 0x00, 0x00,                                                   //
            0x0C, 'p', 0x00,                                                          //
            0x02, 0x00, 0x00, 0x00, 'x', 0x00,                                        //
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   //
            0x00                                                                      //
         };
         std::string_view buf(reinterpret_cast<const char*>(bytes), sizeof(bytes));
         hello_t h{"pre"};
         auto ec = glz::read_bson<permissive>(h, buf);
         expect(not ec);
         expect(h.hello == "pre");
      };

      "skip-code-with-scope"_test = [] {
         constexpr auto permissive = glz::opts{.error_on_unknown_keys = false};
         // {"c": CodeWithScope("x", {})} — int32 total-len | string | document.
         static constexpr uint8_t bytes[] = {
            0x17, 0x00, 0x00, 0x00,                //
            0x0F, 'c', 0x00,                       //
            0x0F, 0x00, 0x00, 0x00,                // inner total length = 15
            0x02, 0x00, 0x00, 0x00, 'x', 0x00,     // string "x"
            0x05, 0x00, 0x00, 0x00, 0x00,          // empty scope document
            0x00                                   //
         };
         std::string_view buf(reinterpret_cast<const char*>(bytes), sizeof(bytes));
         hello_t h{"pre"};
         auto ec = glz::read_bson<permissive>(h, buf);
         expect(not ec);
         expect(h.hello == "pre");
      };
   };

   // ===========================================================================
   // File I/O round-trip
   // ===========================================================================
   suite bson_file_io_tests = [] {
      "file-roundtrip"_test = [] {
         namespace fs = std::filesystem;
         const auto path = (fs::temp_directory_path() / "glaze_bson_file_roundtrip.bson").string();

         person_s out_person{"Ada", 36};
         std::string write_buf;
         const auto wec = glz::write_file_bson(out_person, path, write_buf);
         expect(wec == glz::error_code::none);

         person_s in_person{};
         std::string read_buf;
         const auto rec = glz::read_file_bson(in_person, path, read_buf);
         expect(not rec);
         expect(in_person == out_person);

         std::error_code ignore{};
         fs::remove(path, ignore);
      };
   };

   // ===========================================================================
   // error_on_missing_keys — required (non-nullable) fields must be present.
   // ===========================================================================
   suite bson_missing_keys_tests = [] {
      "missing-required-field-errors"_test = [] {
         constexpr auto strict = glz::opts{.error_on_missing_keys = true};
         // {"opt": 7} — the required "id" field is absent.
         static constexpr uint8_t bytes[] = {
            0x0E, 0x00, 0x00, 0x00,                                   //
            0x10, 'o', 'p', 't', 0x00, 0x07, 0x00, 0x00, 0x00,        //
            0x00                                                      //
         };
         std::string_view buf(reinterpret_cast<const char*>(bytes), sizeof(bytes));
         required_s v{};
         auto ec = glz::read_bson<strict>(v, buf);
         expect(ec.ec == glz::error_code::missing_key);
      };

      "missing-optional-field-ok"_test = [] {
         constexpr auto strict = glz::opts{.error_on_missing_keys = true};
         // {"id": 42} — "opt" is optional and absent, strict mode tolerates it.
         static constexpr uint8_t bytes[] = {
            0x0D, 0x00, 0x00, 0x00,                                   //
            0x10, 'i', 'd', 0x00, 0x2A, 0x00, 0x00, 0x00,             //
            0x00                                                      //
         };
         std::string_view buf(reinterpret_cast<const char*>(bytes), sizeof(bytes));
         required_s v{};
         auto ec = glz::read_bson<strict>(v, buf);
         expect(not ec);
         expect(v.id == 42);
         expect(not v.opt.has_value());
      };
   };

   // ===========================================================================
   // Nested containers / optionals + struct-field monostate
   // ===========================================================================
   suite bson_nested_and_monostate_tests = [] {
      "nested-vectors-roundtrip"_test = [] {
         expect_roundtrip_equal(nested_vec_s{{{1, 2, 3}, {}, {4, 5}}});
      };

      "nested-optional-present-roundtrip"_test = [] {
         // Outer present, inner present.
         expect_roundtrip_equal(nested_opt_s{std::optional<int32_t>{123}});
      };

      "monostate-struct-field-roundtrip"_test = [] {
         // The monostate writes as a null element and round-trips — the
         // reader dispatches to from<BSON, always_null_t T>.
         monostate_field_s in{};
         in.x = 99;
         std::vector<std::byte> buf{};
         expect(not glz::write_bson(in, buf));
         monostate_field_s out{};
         expect(not glz::read_bson(out, buf));
         expect(out.x == 99);
      };
   };

   // ===========================================================================
   // glz::meta-annotated (non-aggregate) struct reflection.
   // Exercises the reflect<T>::keys path where keys come from the meta
   // specialization rather than aggregate member-name extraction.
   // ===========================================================================
   suite bson_meta_annotated_tests = [] {
      "meta-renamed-keys-roundtrip"_test = [] {
         expect_roundtrip_equal(meta_renamed_s{7, "alpha"});
      };

      "meta-renamed-keys-emit-mapped-names"_test = [] {
         // Wire keys must come from the glz::meta declaration, not the
         // C++ member names.
         meta_renamed_s src{1, "x"};
         auto w = glz::write_bson(src);
         expect(w.has_value());
         const auto& buf = w.value();
         expect(buf.find("identifier") != std::string::npos);
         expect(buf.find("display_name") != std::string::npos);
      };

      "meta-nested-struct-roundtrip"_test = [] {
         // Nested glz::meta struct — inner and outer both use mapped keys.
         expect_roundtrip_equal(meta_nested_s{meta_renamed_s{42, "beta"}, 3.14});
      };
   };

   // ===========================================================================
   // Large-array key fast path (small_array_keys table covers 0..1023).
   // ===========================================================================
   suite bson_large_array_tests = [] {
      "array-1024-elements-roundtrips"_test = [] {
         // Uses array_s (defined at namespace scope) so reflection works; 1500
         // entries exercise both the precomputed key table (0..1023) and the
         // std::to_chars fallback (1024+).
         array_s v{};
         v.a.resize(1500);
         for (size_t i = 0; i < v.a.size(); ++i) v.a[i] = static_cast<int32_t>(i);
         expect_roundtrip_equal(v);
      };
   };

   // ===========================================================================
   // Missing-terminator error carries a specific custom_error_message.
   // ===========================================================================
   suite bson_terminator_error_tests = [] {
      "missing-terminator-has-clear-message"_test = [] {
         // Doc claims length 12, but byte 11 (where the 0x00 terminator should
         // sit) is 0xFF — so after reading the int32 element the reader
         // encounters 0xFF as a tag and fails trying to read a key. The
         // resulting unexpected_end is decorated with a specific message so
         // users don't have to guess whether it's a truncation or a framing
         // bug.
         static constexpr uint8_t bad[] = {
            0x0C, 0x00, 0x00, 0x00,                              //
            0x10, 'x', 0x00, 0x2A, 0x00, 0x00, 0x00,             //
            0xFF                                                 //
         };
         std::string_view buf(reinterpret_cast<const char*>(bad), sizeof(bad));
         i32_s v{};
         auto ec = glz::read_bson(v, buf);
         expect(ec.ec == glz::error_code::unexpected_end);
         expect(ec.custom_error_message == "missing document terminator");
      };

      "early-terminator-has-clear-message"_test = [] {
         // Doc claims length 13 but the 0x00 terminator appears at byte 11 (one
         // byte early); byte 12 is an unexpected 0x7F.
         static constexpr uint8_t bad[] = {
            0x0D, 0x00, 0x00, 0x00,                              //
            0x10, 'x', 0x00, 0x2A, 0x00, 0x00, 0x00,             //
            0x00,                                                //
            0x7F                                                 //
         };
         std::string_view buf(reinterpret_cast<const char*>(bad), sizeof(bad));
         i32_s v{};
         auto ec = glz::read_bson(v, buf);
         expect(ec.ec == glz::error_code::syntax_error);
         expect(ec.custom_error_message == "document terminator before end of declared length");
      };
   };

   // ===========================================================================
   // bson_to_json converter.
   // ===========================================================================
   suite bson_to_json_tests = [] {
      "convert-primitives"_test = [] {
         prim_doc_s v{.a = 1, .b = 9000000000LL, .c = 1.5, .d = true, .e = "hi"};
         auto bson = glz::write_bson(v);
         expect(bson.has_value());
         auto json = glz::bson_to_json(bson.value());
         expect(json.has_value());
         expect(json.value() == R"({"a":1,"b":9000000000,"c":1.5,"d":true,"e":"hi"})");
      };

      "convert-nested-document"_test = [] {
         outer_s v{inner_s{42}};
         auto bson = glz::write_bson(v);
         auto json = glz::bson_to_json(bson.value());
         expect(json.has_value());
         expect(json.value() == R"({"inner":{"x":42}})");
      };

      "convert-array"_test = [] {
         array_s v{{10, 20, 30}};
         auto bson = glz::write_bson(v);
         auto json = glz::bson_to_json(bson.value());
         expect(json.has_value());
         expect(json.value() == R"({"a":[10,20,30]})");
      };

      "convert-null-and-optional"_test = [] {
         // skip_null_members=false so the optional's null survives to JSON.
         constexpr auto preserve = glz::opts{.skip_null_members = false};
         optional_s v{};
         std::vector<std::byte> buf;
         expect(not glz::write_bson<preserve>(v, buf));
         auto json = glz::bson_to_json(buf);
         expect(json.has_value());
         expect(json.value() == R"({"o":null})");
      };

      "convert-object-id-extended-json"_test = [] {
         oid_only_s v{};
         for (uint8_t i = 0; i < 12; ++i) v.id.bytes[i] = i;
         auto bson = glz::write_bson(v);
         auto json = glz::bson_to_json(bson.value());
         expect(json.has_value());
         expect(json.value() == R"({"id":{"$oid":"000102030405060708090a0b"}})");
      };

      "convert-datetime-extended-json"_test = [] {
         dt_field_s v{};
         v.when.ms_since_epoch = 1700000000000LL;
         auto bson = glz::write_bson(v);
         auto json = glz::bson_to_json(bson.value());
         expect(json.has_value());
         expect(json.value() == R"({"when":{"$date":{"$numberLong":"1700000000000"}}})");
      };

      "convert-binary-base64"_test = [] {
         bin_only_s v{};
         v.b.subtype = glz::bson::binary_subtype::generic;
         v.b.data = {std::byte{'f'}, std::byte{'o'}, std::byte{'o'}};
         auto bson = glz::write_bson(v);
         auto json = glz::bson_to_json(bson.value());
         expect(json.has_value());
         // "foo" base64 = "Zm9v", subtype 0x00 → "00".
         expect(json.value() == R"({"b":{"$binary":{"base64":"Zm9v","subType":"00"}}})");
      };

      "convert-binary-old-subtype-02-strips-inner-length"_test = [] {
         // 0x02 payloads include a redundant inner int32 length before the
         // real bytes. The converter must strip it so the base64 field reflects
         // only the data.
         bin_only_s v{};
         v.b.subtype = glz::bson::binary_subtype::binary_old;
         v.b.data = {std::byte{'f'}, std::byte{'o'}, std::byte{'o'}};
         auto bson = glz::write_bson(v);
         auto json = glz::bson_to_json(bson.value());
         expect(json.has_value());
         expect(json.value() == R"({"b":{"$binary":{"base64":"Zm9v","subType":"02"}}})");
      };

      "convert-regex-extended-json"_test = [] {
         regex_only_s v{};
         v.r.pattern = "abc";
         v.r.options = "i";
         auto bson = glz::write_bson(v);
         auto json = glz::bson_to_json(bson.value());
         expect(json.has_value());
         expect(json.value() == R"({"r":{"$regularExpression":{"pattern":"abc","options":"i"}}})");
      };

      "convert-javascript-extended-json"_test = [] {
         js_only_s v{};
         v.j.code = "f()";
         auto bson = glz::write_bson(v);
         auto json = glz::bson_to_json(bson.value());
         expect(json.has_value());
         expect(json.value() == R"X({"j":{"$code":"f()"}})X");
      };

      "convert-timestamp-extended-json"_test = [] {
         ts_field_s v{};
         v.ts.seconds = 100;
         v.ts.increment = 3;
         auto bson = glz::write_bson(v);
         auto json = glz::bson_to_json(bson.value());
         expect(json.has_value());
         expect(json.value() == R"({"ts":{"$timestamp":{"t":100,"i":3}}})");
      };

      "convert-min-and-max-key"_test = [] {
         min_only_s v{};
         auto json = glz::bson_to_json(glz::write_bson(v).value());
         expect(json.has_value());
         expect(json.value() == R"({"mn":{"$minKey":1}})");

         max_only_s w{};
         auto json2 = glz::bson_to_json(glz::write_bson(w).value());
         expect(json2.has_value());
         expect(json2.value() == R"({"mx":{"$maxKey":1}})");
      };

      "convert-decimal128-hex-fallback"_test = [] {
         dec128_only_s v{};
         for (uint8_t i = 0; i < 16; ++i) v.d.bytes[i] = i;
         auto bson = glz::write_bson(v);
         auto json = glz::bson_to_json(bson.value());
         expect(json.has_value());
         expect(json.value() == R"({"d":{"decimal128Hex":"000102030405060708090a0b0c0d0e0f"}})");
      };

      "convert-rejects-truncated-input"_test = [] {
         // Declared length 22 but buffer only 10 bytes.
         static constexpr uint8_t bad[] = {0x16, 0x00, 0x00, 0x00, 0x02, 'h', 'i', 0x00, 0x01, 0x00};
         std::string_view buf(reinterpret_cast<const char*>(bad), sizeof(bad));
         auto json = glz::bson_to_json(buf);
         expect(not json.has_value());
      };

      "convert-rejects-trailing-bytes"_test = [] {
         // Well-formed {"hello":"world"} document (22 bytes) + 1 trailing
         // garbage byte. Must be rejected symmetrically with read_bson.
         static constexpr uint8_t with_trailer[] = {
            0x16, 0x00, 0x00, 0x00,              //
            0x02, 'h', 'e', 'l', 'l', 'o', 0x00, //
            0x06, 0x00, 0x00, 0x00,              //
            'w', 'o', 'r', 'l', 'd', 0x00,       //
            0x00,                                //
            0xFF};
         std::string_view buf(reinterpret_cast<const char*>(with_trailer), sizeof(with_trailer));
         auto json = glz::bson_to_json(buf);
         expect(not json.has_value());
         expect(json.error().ec == glz::error_code::syntax_error);
      };

      "convert-enforces-depth-limit"_test = [] {
         // Same deeply-nested build as in bson_depth_limit_tests — converter
         // walks the same document tree so the guard must trip here too.
         // Reference the real limit so the test self-adjusts if it changes.
         constexpr size_t depth = glz::max_recursive_depth_limit + 1;
         auto append_le_i32 = [&](std::string& dst, int32_t v) {
            char b[4];
            for (int i = 0; i < 4; ++i) b[i] = char((v >> (8 * i)) & 0xFF);
            dst.append(b, 4);
         };
         std::string inner;
         append_le_i32(inner, 5);
         inner.push_back('\0');
         for (size_t i = 0; i < depth; ++i) {
            std::string next;
            const auto inner_size = static_cast<int32_t>(inner.size());
            append_le_i32(next, 4 + 1 + 2 + inner_size + 1);
            next.push_back('\x03');
            next.push_back('a');
            next.push_back('\0');
            next.append(inner);
            next.push_back('\0');
            inner = std::move(next);
         }
         auto json = glz::bson_to_json(inner);
         expect(not json.has_value());
         expect(json.error().ec == glz::error_code::exceeded_max_recursive_depth);
      };
   };
}

int main() { return 0; }
