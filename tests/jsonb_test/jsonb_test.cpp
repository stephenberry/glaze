// Glaze Library
// For the license information refer to glaze.hpp

#include "glaze/jsonb.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <expected>
#include <filesystem>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "glaze/exceptions/jsonb_exceptions.hpp"
#include "ut/ut.hpp"

using namespace ut;

static_assert(glz::write_supported<int, glz::JSONB>);
static_assert(glz::read_supported<int, glz::JSONB>);

// --- Tagged variant test types (used by suite tagged_variant_tests below) ---

struct put_op
{
   std::string path{};
   int value = 0;
   bool operator==(const put_op&) const = default;
};

struct delete_op
{
   std::string path{};
   bool recursive = false;
   bool operator==(const delete_op&) const = default;
};

using tagged_op = std::variant<put_op, delete_op>;

template <>
struct glz::meta<tagged_op>
{
   static constexpr std::string_view tag = "op";
   static constexpr auto ids = std::array{"put", "delete"};
};

// Integer-id variant (uses default ids = struct name? — no, integers must be explicit)
struct shape_circle
{
   double radius = 1.0;
   bool operator==(const shape_circle&) const = default;
};
struct shape_square
{
   double side = 1.0;
   bool operator==(const shape_square&) const = default;
};

using tagged_shape = std::variant<shape_circle, shape_square>;

template <>
struct glz::meta<tagged_shape>
{
   static constexpr std::string_view tag = "kind";
   static constexpr auto ids = std::array<int, 2>{1, 2};
};

// Variant with multiple object alternatives — only valid because it has a tag.
using tagged_multi_obj = std::variant<put_op, delete_op, shape_circle>;

template <>
struct glz::meta<tagged_multi_obj>
{
   static constexpr std::string_view tag = "t";
   static constexpr auto ids = std::array{"put", "del", "circle"};
};

struct simple_struct
{
   int i = 287;
   double d = 3.14;
   std::string hello = "Hello World";
   std::array<uint64_t, 3> arr = {1, 2, 3};
   bool operator==(const simple_struct&) const = default;
};

struct nested_struct
{
   std::string name = "root";
   simple_struct child{};
   std::vector<int> ids = {10, 20, 30};
   bool operator==(const nested_struct&) const = default;
};

// Header-level smoke test: round-trip a single scalar and verify raw bytes.
suite header_tests = [] {
   "null scalar"_test = [] {
      std::nullptr_t n;
      std::string buf;
      expect(not glz::write_jsonb(n, buf));
      expect(buf.size() == 1);
      expect(static_cast<uint8_t>(buf[0]) == 0x00); // type 0, size 0

      std::nullptr_t out;
      expect(not glz::read_jsonb(out, buf));
   };

   "boolean scalars"_test = [] {
      std::string buf;
      expect(not glz::write_jsonb(true, buf));
      expect(buf.size() == 1);
      expect(static_cast<uint8_t>(buf[0]) == 0x01); // type 1, size 0
      bool b = false;
      expect(not glz::read_jsonb(b, buf));
      expect(b);

      buf.clear();
      expect(not glz::write_jsonb(false, buf));
      expect(buf.size() == 1);
      expect(static_cast<uint8_t>(buf[0]) == 0x02);
      expect(not glz::read_jsonb(b, buf));
      expect(not b);
   };

   "small int round-trip"_test = [] {
      std::string buf;
      expect(not glz::write_jsonb(42, buf));
      // Expect: header byte with type=3, size=2 ('4','2'), then '4','2'
      expect(buf.size() == 3);
      expect(static_cast<uint8_t>(buf[0]) == ((2u << 4) | 3u));
      expect(buf[1] == '4' && buf[2] == '2');

      int out = 0;
      expect(not glz::read_jsonb(out, buf));
      expect(out == 42);
   };

   "large int round-trip"_test = [] {
      std::string buf;
      const int64_t v = -1234567890123LL;
      expect(not glz::write_jsonb(v, buf));
      int64_t out = 0;
      expect(not glz::read_jsonb(out, buf));
      expect(out == v);
   };

   "uint64 round-trip"_test = [] {
      std::string buf;
      const uint64_t v = 9876543210123456789ULL;
      expect(not glz::write_jsonb(v, buf));
      uint64_t out = 0;
      expect(not glz::read_jsonb(out, buf));
      expect(out == v);
   };

   "float round-trip"_test = [] {
      std::string buf;
      expect(not glz::write_jsonb(3.14, buf));
      double out = 0;
      expect(not glz::read_jsonb(out, buf));
      expect(out == 3.14);
   };

   "float special values as FLOAT5"_test = [] {
      const std::array<double, 3> specials{
         std::numeric_limits<double>::quiet_NaN(),
         std::numeric_limits<double>::infinity(),
         -std::numeric_limits<double>::infinity(),
      };
      for (double v : specials) {
         std::string buf;
         expect(not glz::write_jsonb(v, buf));
         double out = 0;
         expect(not glz::read_jsonb(out, buf));
         if (std::isnan(v)) {
            expect(std::isnan(out));
         }
         else {
            expect(out == v);
         }
      }
   };

   "string round-trip"_test = [] {
      std::string buf;
      const std::string s = "Hello";
      expect(not glz::write_jsonb(s, buf));
      // No bytes needing escape, so writer emits TEXT (type 7), inline size 5 =>
      // header (5<<4)|7 = 0x57.
      expect(static_cast<uint8_t>(buf[0]) == ((5u << 4) | 7u));
      std::string out;
      expect(not glz::read_jsonb(out, buf));
      expect(out == s);
   };

   "string requiring u8 size header"_test = [] {
      std::string buf;
      const std::string s = "Hello, world!";
      expect(not glz::write_jsonb(s, buf));
      // Size 13 > 11 inline limit, so header is 2 bytes: (12<<4)|7 = 0xC7 then 0x0D
      expect(static_cast<uint8_t>(buf[0]) == ((12u << 4) | 7u));
      expect(static_cast<uint8_t>(buf[1]) == 13u);
      std::string out;
      expect(not glz::read_jsonb(out, buf));
      expect(out == s);
   };

   "empty string round-trip"_test = [] {
      std::string buf;
      const std::string s;
      expect(not glz::write_jsonb(s, buf));
      std::string out = "junk";
      expect(not glz::read_jsonb(out, buf));
      expect(out.empty());
   };

   "long string triggers u8_follows header"_test = [] {
      std::string s(100, 'x');
      std::string buf;
      expect(not glz::write_jsonb(s, buf));
      // Header byte: size_nibble = 12 (u8_follows), type = 7 (TEXT)
      expect(static_cast<uint8_t>(buf[0]) == ((12u << 4) | 7u));
      expect(static_cast<uint8_t>(buf[1]) == 100u);
      std::string out;
      expect(not glz::read_jsonb(out, buf));
      expect(out == s);
   };

   "string with embedded quote written as TEXTRAW"_test = [] {
      std::string buf;
      const std::string s = "hi\"there";
      expect(not glz::write_jsonb(s, buf));
      // Contains '"' → needs JSON escape → TEXTRAW (type 10).
      expect(static_cast<uint8_t>(buf[0]) == ((8u << 4) | 10u));
      std::string out;
      expect(not glz::read_jsonb(out, buf));
      expect(out == s);
   };

   "string with embedded backslash written as TEXTRAW"_test = [] {
      std::string buf;
      const std::string s = "a\\b";
      expect(not glz::write_jsonb(s, buf));
      expect(static_cast<uint8_t>(buf[0]) == ((3u << 4) | 10u));
      std::string out;
      expect(not glz::read_jsonb(out, buf));
      expect(out == s);
   };

   "string with control char written as TEXTRAW"_test = [] {
      std::string buf;
      const std::string s = std::string("a") + std::string(1, '\n') + "b";
      expect(not glz::write_jsonb(s, buf));
      expect(static_cast<uint8_t>(buf[0]) == ((3u << 4) | 10u));
      std::string out;
      expect(not glz::read_jsonb(out, buf));
      expect(out == s);
   };

   "UTF-8 high-byte string written as TEXT"_test = [] {
      // Bytes >= 0x80 don't need JSON escaping, so a plain UTF-8 payload is TEXT.
      std::string buf;
      const std::string s = "caf\xC3\xA9"; // "café" in UTF-8
      expect(not glz::write_jsonb(s, buf));
      expect((static_cast<uint8_t>(buf[0]) & 0x0Fu) == 7u);
      std::string out;
      expect(not glz::read_jsonb(out, buf));
      expect(out == s);
   };

   "TEXT payload converts directly to JSON without scan"_test = [] {
      const std::string s = "Hello world";
      std::string blob;
      expect(not glz::write_jsonb(s, blob));
      auto json = glz::jsonb_to_json(blob);
      expect(json.has_value());
      expect(json.value() == "\"Hello world\"");
   };

   "TEXTRAW payload gets escape-converted to JSON"_test = [] {
      const std::string s = "line1\nline2";
      std::string blob;
      expect(not glz::write_jsonb(s, blob));
      // Sanity: this payload requires escaping so it's TEXTRAW.
      expect((static_cast<uint8_t>(blob[0]) & 0x0Fu) == 10u);
      auto json = glz::jsonb_to_json(blob);
      expect(json.has_value());
      expect(json.value() == "\"line1\\nline2\"");
   };
};

suite container_tests = [] {
   "vector of int"_test = [] {
      std::vector<int> v = {1, 2, 3, 4, 5};
      std::string buf;
      expect(not glz::write_jsonb(v, buf));
      // Top byte: type=11 (array), size_nibble=15 (u64_follows, always-9 strategy)
      expect(glz::jsonb::get_type(static_cast<uint8_t>(buf[0])) == glz::jsonb::type::array);
      std::vector<int> out;
      expect(not glz::read_jsonb(out, buf));
      expect(out == v);
   };

   "empty vector"_test = [] {
      std::vector<int> v;
      std::string buf;
      expect(not glz::write_jsonb(v, buf));
      std::vector<int> out = {99};
      expect(not glz::read_jsonb(out, buf));
      expect(out.empty());
   };

   "nested vectors"_test = [] {
      std::vector<std::vector<int>> v = {{1, 2}, {3, 4, 5}, {}};
      std::string buf;
      expect(not glz::write_jsonb(v, buf));
      std::vector<std::vector<int>> out;
      expect(not glz::read_jsonb(out, buf));
      expect(out == v);
   };

   "std::array"_test = [] {
      std::array<int, 3> a = {7, 8, 9};
      std::string buf;
      expect(not glz::write_jsonb(a, buf));
      std::array<int, 3> out{};
      expect(not glz::read_jsonb(out, buf));
      expect(out == a);
   };

   "std::tuple"_test = [] {
      std::tuple<int, std::string, double> t{42, "hi", 2.5};
      std::string buf;
      expect(not glz::write_jsonb(t, buf));
      std::tuple<int, std::string, double> out{};
      expect(not glz::read_jsonb(out, buf));
      expect(out == t);
   };

   "std::map<string, int>"_test = [] {
      std::map<std::string, int> m{{"a", 1}, {"b", 2}, {"c", 3}};
      std::string buf;
      expect(not glz::write_jsonb(m, buf));
      std::map<std::string, int> out;
      expect(not glz::read_jsonb(out, buf));
      expect(out == m);
   };

   "reflected struct (no meta)"_test = [] {
      simple_struct s{};
      std::string buf;
      expect(not glz::write_jsonb(s, buf));
      simple_struct out{};
      out.i = 0;
      out.d = 0;
      out.hello.clear();
      out.arr = {0, 0, 0};
      expect(not glz::read_jsonb(out, buf));
      expect(out == s);
   };

   "nested reflected struct"_test = [] {
      nested_struct s{};
      s.name = "parent";
      s.child.i = 99;
      s.ids = {100, 200, 300, 400};
      std::string buf;
      expect(not glz::write_jsonb(s, buf));
      nested_struct out{};
      expect(not glz::read_jsonb(out, buf));
      expect(out == s);
   };

   "optional present"_test = [] {
      std::optional<int> o = 7;
      std::string buf;
      expect(not glz::write_jsonb(o, buf));
      std::optional<int> out;
      expect(not glz::read_jsonb(out, buf));
      expect(out.has_value() && *out == 7);
   };

   "optional null"_test = [] {
      std::optional<int> o;
      std::string buf;
      expect(not glz::write_jsonb(o, buf));
      std::optional<int> out = 42;
      expect(not glz::read_jsonb(out, buf));
      expect(not out.has_value());
   };

   "variant round-trip"_test = [] {
      using V = std::variant<int, std::string, double>;
      V v = std::string("hello");
      std::string buf;
      expect(not glz::write_jsonb(v, buf));
      V out;
      expect(not glz::read_jsonb(out, buf));
      expect(std::holds_alternative<std::string>(out));
      expect(std::get<std::string>(out) == "hello");
   };

   "variant writes bare alternative (no [index, value] wrapper)"_test = [] {
      using V = std::variant<int, std::string, double>;
      V v = std::string("hi");
      std::string variant_buf;
      expect(not glz::write_jsonb(v, variant_buf));

      // The variant blob must be byte-identical to the blob you'd get writing the
      // active alternative directly — no array wrapper, no index prefix.
      std::string direct_buf;
      expect(not glz::write_jsonb(std::string("hi"), direct_buf));
      expect(variant_buf == direct_buf);
   };

   "variant deduces int vs double from JSONB type code"_test = [] {
      // JSONB's INT and FLOAT type codes are distinct, so this variant auto-deduces
      // even though Glaze's JSON convention can't distinguish numeric alternatives.
      using V = std::variant<int64_t, double, std::string, bool>;

      V v = int64_t{42};
      std::string buf;
      expect(not glz::write_jsonb(v, buf));
      V out;
      expect(not glz::read_jsonb(out, buf));
      expect(std::holds_alternative<int64_t>(out));
      expect(std::get<int64_t>(out) == 42);

      v = 3.14;
      buf.clear();
      expect(not glz::write_jsonb(v, buf));
      expect(not glz::read_jsonb(out, buf));
      expect(std::holds_alternative<double>(out));
      expect(std::get<double>(out) == 3.14);

      v = true;
      buf.clear();
      expect(not glz::write_jsonb(v, buf));
      expect(not glz::read_jsonb(out, buf));
      expect(std::holds_alternative<bool>(out));
      expect(std::get<bool>(out) == true);
   };

   "variant with null alternative round-trips through NULL element"_test = [] {
      using V = std::variant<std::nullptr_t, int, std::string>;
      V v = nullptr;
      std::string buf;
      expect(not glz::write_jsonb(v, buf));
      V out = 42;
      expect(not glz::read_jsonb(out, buf));
      expect(std::holds_alternative<std::nullptr_t>(out));
   };
};

suite tagged_variant_tests = [] {
   "tagged variant: string id round-trip (first alternative)"_test = [] {
      tagged_op v = put_op{"/users/42", 7};
      std::string buf;
      expect(not glz::write_jsonb(v, buf));

      tagged_op out{};
      expect(not glz::read_jsonb(out, buf));
      expect(std::holds_alternative<put_op>(out));
      expect(std::get<put_op>(out) == std::get<put_op>(v));
   };

   "tagged variant: string id round-trip (second alternative)"_test = [] {
      tagged_op v = delete_op{"/sessions", true};
      std::string buf;
      expect(not glz::write_jsonb(v, buf));

      tagged_op out{};
      expect(not glz::read_jsonb(out, buf));
      expect(std::holds_alternative<delete_op>(out));
      expect(std::get<delete_op>(out) == std::get<delete_op>(v));
   };

   "tagged variant: written as OBJECT not bare alternative"_test = [] {
      // The blob must be an OBJECT (type 12), not the alternative's own OBJECT-shape
      // wrapped bare — i.e., it has the wrapping tag in addition to the struct fields.
      tagged_op v = put_op{"/x", 1};
      std::string buf;
      expect(not glz::write_jsonb(v, buf));
      expect((static_cast<uint8_t>(buf[0]) & 0x0Fu) == 12u); // OBJECT type code

      // And it should NOT be byte-identical to writing the alternative directly,
      // because the tagged write injects the tag pair.
      std::string direct;
      expect(not glz::write_jsonb(put_op{"/x", 1}, direct));
      expect(buf != direct);
      expect(buf.size() > direct.size());
   };

   "tagged variant: integer id round-trip"_test = [] {
      tagged_shape v = shape_square{2.5};
      std::string buf;
      expect(not glz::write_jsonb(v, buf));

      tagged_shape out{};
      expect(not glz::read_jsonb(out, buf));
      expect(std::holds_alternative<shape_square>(out));
      expect(std::get<shape_square>(out).side == 2.5);
   };

   "tagged variant: multiple object alternatives disambiguated by tag"_test = [] {
      // Without a tag this would fail variant_jsonb_auto_deducible (3 object alts).
      tagged_multi_obj v = shape_circle{3.0};
      std::string buf;
      expect(not glz::write_jsonb(v, buf));

      tagged_multi_obj out = put_op{"/foo", 1};
      expect(not glz::read_jsonb(out, buf));
      expect(std::holds_alternative<shape_circle>(out));
      expect(std::get<shape_circle>(out).radius == 3.0);
   };

   "tagged variant: vector of mixed alternatives round-trips"_test = [] {
      std::vector<tagged_op> ops{
         put_op{"/a", 1},
         delete_op{"/b", true},
         put_op{"/c", 99},
         delete_op{"/d", false},
      };
      std::string buf;
      expect(not glz::write_jsonb(ops, buf));

      std::vector<tagged_op> out;
      expect(not glz::read_jsonb(out, buf));
      expect(out.size() == ops.size());
      expect(std::get<put_op>(out[0]).path == "/a");
      expect(std::get<delete_op>(out[1]).path == "/b");
      expect(std::get<put_op>(out[2]).value == 99);
      expect(std::get<delete_op>(out[3]).recursive == false);
   };

   "tagged variant: unknown id rejected"_test = [] {
      // Build a blob by hand: OBJECT with tag "op" → "bogus", forcing
      // variant_id_to_index to fail.
      // Simplest: write a put_op blob, then we just confirm a malformed tag value path.
      tagged_op v = put_op{};
      std::string buf;
      expect(not glz::write_jsonb(v, buf));

      // Find and replace the tag value "put" with "xxx" (same length) to force unknown.
      const auto put_pos = buf.find("put");
      expect(put_pos != std::string::npos);
      buf[put_pos] = 'x';
      buf[put_pos + 1] = 'x';
      buf[put_pos + 2] = 'x';

      tagged_op out{};
      const auto ec = glz::read_jsonb(out, buf);
      expect(bool(ec));
      expect(ec.ec == glz::error_code::no_matching_variant_type);
   };
};

suite error_tests = [] {
   "reserved type code rejected"_test = [] {
      // Construct a minimal invalid blob: one header byte with type=13 (reserved).
      std::string buf;
      buf.push_back(static_cast<char>((0u << 4) | 13u));
      int out = 0;
      auto ec = glz::read_jsonb(out, buf);
      expect(bool(ec));
   };

   "truncated payload rejected"_test = [] {
      // Header says payload size 5 for TEXTRAW, but the buffer has only 2 bytes of payload.
      std::string buf;
      buf.push_back(static_cast<char>((5u << 4) | 10u)); // textraw, size 5
      buf.append("ab");
      std::string out;
      auto ec = glz::read_jsonb(out, buf);
      expect(bool(ec));
   };

   "wrong type for expected scalar"_test = [] {
      // Write a string, try to read as int.
      std::string buf;
      const std::string s = "abc";
      expect(not glz::write_jsonb(s, buf));
      int out = 0;
      auto ec = glz::read_jsonb(out, buf);
      expect(bool(ec));
   };
};

suite textj_tests = [] {
   "read TEXTJ with \\n escape"_test = [] {
      // Build: type=8 (TEXTJ), payload "a\\nb" (4 bytes), header = 0x48 (size_nibble=4)
      std::string buf;
      buf.push_back(static_cast<char>((4u << 4) | 8u));
      buf.push_back('a');
      buf.push_back('\\');
      buf.push_back('n');
      buf.push_back('b');
      std::string out;
      expect(not glz::read_jsonb(out, buf));
      expect(out == std::string("a\nb"));
   };

   "read TEXTJ with unicode escape"_test = [] {
      // Build: type=8 (TEXTJ), payload "\\u00e9" (6 bytes)
      std::string buf;
      buf.push_back(static_cast<char>((6u << 4) | 8u));
      buf += "\\u00e9";
      std::string out;
      expect(not glz::read_jsonb(out, buf));
      // UTF-8 encoding of U+00E9 = 0xC3 0xA9
      expect(out.size() == 2);
      expect(static_cast<uint8_t>(out[0]) == 0xC3);
      expect(static_cast<uint8_t>(out[1]) == 0xA9);
   };
};

suite spec_tests = [] {
   // Hand-built blobs consistent with https://sqlite.org/jsonb.html §3.

   "parse hand-built null"_test = [] {
      std::string buf{static_cast<char>(0x00)};
      std::nullptr_t out;
      expect(not glz::read_jsonb(out, buf));
   };

   "parse hand-built true / false"_test = [] {
      bool b = false;
      std::string t{static_cast<char>(0x01)};
      expect(not glz::read_jsonb(b, t));
      expect(b);

      std::string f{static_cast<char>(0x02)};
      expect(not glz::read_jsonb(b, f));
      expect(not b);
   };

   "parse hand-built INT '123'"_test = [] {
      // type=3 (INT), size=3, payload "123"
      std::string buf;
      buf.push_back(static_cast<char>((3u << 4) | 3u));
      buf += "123";
      int out = 0;
      expect(not glz::read_jsonb(out, buf));
      expect(out == 123);
   };

   "parse hand-built FLOAT '3.14'"_test = [] {
      std::string buf;
      buf.push_back(static_cast<char>((4u << 4) | 5u)); // type=5 (FLOAT), size=4
      buf += "3.14";
      double out = 0;
      expect(not glz::read_jsonb(out, buf));
      expect(out == 3.14);
   };

   "parse hand-built empty ARRAY"_test = [] {
      std::string buf{static_cast<char>((0u << 4) | 11u)}; // type=11, size=0
      std::vector<int> out{1, 2, 3};
      expect(not glz::read_jsonb(out, buf));
      expect(out.empty());
   };

   "parse hand-built ARRAY [1,2]"_test = [] {
      // ARRAY payload = two INT elements, each 2 bytes: [0x13,'1'] [0x13,'2']
      std::string buf;
      buf.push_back(static_cast<char>((4u << 4) | 11u)); // array, size 4
      buf.push_back(static_cast<char>((1u << 4) | 3u)); // INT, size 1
      buf.push_back('1');
      buf.push_back(static_cast<char>((1u << 4) | 3u));
      buf.push_back('2');
      std::vector<int> out;
      expect(not glz::read_jsonb(out, buf));
      expect(out.size() == 2);
      expect(out[0] == 1 && out[1] == 2);
   };

   "parse hand-built OBJECT {\"k\":1}"_test = [] {
      // OBJECT payload: TEXT key "k" then INT 1 — total 2 + 2 = 4 bytes.
      std::string buf;
      buf.push_back(static_cast<char>((4u << 4) | 12u)); // object, size 4
      buf.push_back(static_cast<char>((1u << 4) | 7u)); // TEXT, size 1
      buf.push_back('k');
      buf.push_back(static_cast<char>((1u << 4) | 3u)); // INT, size 1
      buf.push_back('1');
      std::map<std::string, int> out;
      expect(not glz::read_jsonb(out, buf));
      expect(out.size() == 1);
      expect(out["k"] == 1);
   };

   "parse INT5 hex payload"_test = [] {
      std::string buf;
      buf.push_back(static_cast<char>((4u << 4) | 4u)); // INT5, size 4
      buf += "0xFF";
      int out = 0;
      expect(not glz::read_jsonb(out, buf));
      expect(out == 255);
   };

   "parse INT5 negative hex payload"_test = [] {
      // JSON5 permits an optional leading sign before the 0x prefix. SQLite produces
      // payloads like "-0x1A"; the reader used to reject these while jsonb_to_json handled
      // them, so the two paths disagreed.
      std::string buf;
      buf.push_back(static_cast<char>((5u << 4) | 4u)); // INT5, size 5
      buf += "-0x1A";
      int out = 0;
      expect(not glz::read_jsonb(out, buf));
      expect(out == -26);
   };

   "parse INT5 positive-signed hex payload"_test = [] {
      std::string buf;
      buf.push_back(static_cast<char>((5u << 4) | 4u)); // INT5, size 5
      buf += "+0xFF";
      int out = 0;
      expect(not glz::read_jsonb(out, buf));
      expect(out == 255);
   };

   "parse INT5 negative hex into unsigned rejected"_test = [] {
      std::string buf;
      buf.push_back(static_cast<char>((5u << 4) | 4u)); // INT5, size 5
      buf += "-0x1A";
      unsigned out = 0;
      auto ec = glz::read_jsonb(out, buf);
      expect(bool(ec));
   };

   "parse INT5 negative hex edge case (INT_MIN-adjacent)"_test = [] {
      // -0x80000000 is exactly int32_t min; the two's-complement unsigned-magnitude trick
      // must handle this without UB.
      std::string buf;
      const std::string payload = "-0x80000000";
      buf.push_back(static_cast<char>((payload.size() << 4) | 4u));
      buf += payload;
      int32_t out = 0;
      expect(not glz::read_jsonb(out, buf));
      expect(out == (std::numeric_limits<int32_t>::min)());
   };

   "INT decimal overflow into narrower signed target rejected"_test = [] {
      // "1000" fits a long long but not int8_t — must be parse_number_failure,
      // not a silent wrap to -24.
      std::string buf;
      const std::string payload = "1000";
      buf.push_back(static_cast<char>((payload.size() << 4) | 3u)); // INT, size 4
      buf += payload;
      int8_t out = 0;
      const auto ec = glz::read_jsonb(out, buf);
      expect(bool(ec));
      expect(ec.ec == glz::error_code::parse_number_failure);
   };

   "INT decimal overflow into narrower unsigned target rejected"_test = [] {
      // "300" > uint8_t max (255) — must reject rather than wrap to 44.
      std::string buf;
      const std::string payload = "300";
      buf.push_back(static_cast<char>((payload.size() << 4) | 3u)); // INT, size 3
      buf += payload;
      uint8_t out = 0;
      const auto ec = glz::read_jsonb(out, buf);
      expect(bool(ec));
      expect(ec.ec == glz::error_code::parse_number_failure);
   };

   "INT negative into unsigned target rejected"_test = [] {
      std::string buf;
      const std::string payload = "-1";
      buf.push_back(static_cast<char>((payload.size() << 4) | 3u)); // INT, size 2
      buf += payload;
      uint32_t out = 0;
      const auto ec = glz::read_jsonb(out, buf);
      expect(bool(ec));
      expect(ec.ec == glz::error_code::parse_number_failure);
   };

   "INT signed underflow into narrower target rejected"_test = [] {
      // "-200" fits long long but < int8_t min (-128).
      std::string buf;
      const std::string payload = "-200";
      buf.push_back(static_cast<char>((payload.size() << 4) | 3u)); // INT, size 4
      buf += payload;
      int8_t out = 0;
      const auto ec = glz::read_jsonb(out, buf);
      expect(bool(ec));
      expect(ec.ec == glz::error_code::parse_number_failure);
   };

   "INT decimal boundary fits narrower target"_test = [] {
      // int8_t max (127) must still read cleanly.
      std::string buf;
      const std::string payload = "127";
      buf.push_back(static_cast<char>((payload.size() << 4) | 3u)); // INT, size 3
      buf += payload;
      int8_t out = 0;
      expect(not glz::read_jsonb(out, buf));
      expect(out == 127);
   };

   "INT5 hex overflow into narrower signed target rejected"_test = [] {
      // 0x1FF = 511; does not fit int8_t.
      std::string buf;
      const std::string payload = "0x1FF";
      buf.push_back(static_cast<char>((payload.size() << 4) | 4u)); // INT5, size 5
      buf += payload;
      int8_t out = 0;
      const auto ec = glz::read_jsonb(out, buf);
      expect(bool(ec));
      expect(ec.ec == glz::error_code::parse_number_failure);
   };

   "INT5 hex overflow into narrower unsigned target rejected"_test = [] {
      // 0x1FF = 511; does not fit uint8_t.
      std::string buf;
      const std::string payload = "0x1FF";
      buf.push_back(static_cast<char>((payload.size() << 4) | 4u)); // INT5, size 5
      buf += payload;
      uint8_t out = 0;
      const auto ec = glz::read_jsonb(out, buf);
      expect(bool(ec));
      expect(ec.ec == glz::error_code::parse_number_failure);
   };

   "INT5 negative hex past int32 min rejected"_test = [] {
      // -0x80000001 is one less than int32_t min.
      std::string buf;
      const std::string payload = "-0x80000001";
      buf.push_back(static_cast<char>((payload.size() << 4) | 4u)); // INT5, size 11
      buf += payload;
      int32_t out = 0;
      const auto ec = glz::read_jsonb(out, buf);
      expect(bool(ec));
      expect(ec.ec == glz::error_code::parse_number_failure);
   };

   "INT5 hex boundary fits signed target"_test = [] {
      // 0x7F is exactly int8_t max.
      std::string buf;
      const std::string payload = "0x7F";
      buf.push_back(static_cast<char>((payload.size() << 4) | 4u)); // INT5, size 4
      buf += payload;
      int8_t out = 0;
      expect(not glz::read_jsonb(out, buf));
      expect(out == 127);
   };

   "NULL with non-zero payload tolerated (forward compatibility)"_test = [] {
      // Spec: "Legacy implementations that see an element type of 0 with a non-zero
      // payload size should continue to interpret that element as 'null' for
      // compatibility." Build a NULL with a 3-byte payload of arbitrary bytes.
      std::string buf;
      buf.push_back(static_cast<char>((3u << 4) | 0u)); // NULL, size 3
      buf += "abc";
      std::nullptr_t out;
      expect(not glz::read_jsonb(out, buf));
   };

   "TRUE with non-zero payload tolerated"_test = [] {
      std::string buf;
      buf.push_back(static_cast<char>((2u << 4) | 1u)); // TRUE, size 2
      buf += "XY";
      bool out = false;
      expect(not glz::read_jsonb(out, buf));
      expect(out == true);
   };

   "FALSE with non-zero payload tolerated"_test = [] {
      std::string buf;
      buf.push_back(static_cast<char>((2u << 4) | 2u)); // FALSE, size 2
      buf += "ZZ";
      bool out = true;
      expect(not glz::read_jsonb(out, buf));
      expect(out == false);
   };

   "optional<int> with non-zero-payload null tolerated"_test = [] {
      std::string buf;
      buf.push_back(static_cast<char>((2u << 4) | 0u)); // NULL, size 2
      buf += "??";
      std::optional<int> out = 42;
      expect(not glz::read_jsonb(out, buf));
      expect(!out.has_value());
   };

   "jsonb_to_json renders NULL/TRUE/FALSE with non-zero payload"_test = [] {
      std::string buf;
      buf.push_back(static_cast<char>((3u << 4) | 0u));
      buf += "xxx";
      auto j = glz::jsonb_to_json(buf);
      expect(j.has_value());
      expect(j.value() == "null");

      buf.clear();
      buf.push_back(static_cast<char>((1u << 4) | 1u));
      buf += "t";
      j = glz::jsonb_to_json(buf);
      expect(j.has_value());
      expect(j.value() == "true");

      buf.clear();
      buf.push_back(static_cast<char>((2u << 4) | 2u));
      buf += "f!";
      j = glz::jsonb_to_json(buf);
      expect(j.has_value());
      expect(j.value() == "false");
   };

   "deeply nested array rejected at depth cap"_test = [] {
      // Build an array-of-array-of-array... 1000 levels deep. Each level is a 9-byte
      // u64_follows ARRAY header whose payload contains exactly one nested element.
      // Without a recursion cap, this would stack-overflow the reader.
      const int levels = 1000;
      std::string buf;
      // Payload sizes grow from innermost outward; the innermost element is an empty
      // array (9 bytes: header-with-size-0).
      std::vector<std::string> frames;
      {
         std::string leaf;
         leaf.resize(9);
         leaf[0] = static_cast<char>((15u << 4) | 11u); // ARRAY, u64_follows, size 0
         for (int i = 1; i < 9; ++i) leaf[i] = 0;
         frames.push_back(std::move(leaf));
      }
      for (int i = 1; i < levels; ++i) {
         std::string wrapper;
         wrapper.resize(9);
         const uint64_t payload_size = frames.back().size();
         wrapper[0] = static_cast<char>((15u << 4) | 11u);
         uint64_t v = payload_size;
         if constexpr (std::endian::native == std::endian::little) v = std::byteswap(v);
         std::memcpy(&wrapper[1], &v, 8);
         wrapper += frames.back();
         frames.push_back(std::move(wrapper));
      }
      buf = frames.back();

      // Reader: glz::generic handles arbitrary nesting, so depth_guard is the stopping
      // condition (not a type-shape mismatch).
      glz::generic out;
      auto ec = glz::read_jsonb(out, buf);
      expect(bool(ec));
      expect(ec.ec == glz::error_code::exceeded_max_recursive_depth);

      // Converter: same input, exceeds depth cap.
      auto j = glz::jsonb_to_json(buf);
      expect(!j.has_value());
      expect(j.error().ec == glz::error_code::exceeded_max_recursive_depth);
   };

   "reasonably-nested blob within depth cap succeeds"_test = [] {
      // Ten levels of nesting is comfortably under the 256 cap.
      std::vector<std::vector<std::vector<std::vector<std::vector<int>>>>> v{{{{{1, 2, 3}}}}};
      std::string buf;
      expect(not glz::write_jsonb(v, buf));
      decltype(v) out;
      expect(not glz::read_jsonb(out, buf));
      expect(out == v);
      // Converter also succeeds.
      auto j = glz::jsonb_to_json(buf);
      expect(j.has_value());
   };

   "reserved type codes 13/14/15 rejected"_test = [] {
      for (uint8_t code : {uint8_t(13), uint8_t(14), uint8_t(15)}) {
         std::string buf;
         buf.push_back(static_cast<char>((0u << 4) | code));
         int out = 0;
         auto ec = glz::read_jsonb(out, buf);
         expect(bool(ec));
      }
   };
};

suite converter_tests = [] {
   "jsonb_to_json scalars"_test = [] {
      std::string buf;
      expect(not glz::write_jsonb(42, buf));
      auto j = glz::jsonb_to_json(buf);
      expect(j.has_value());
      expect(j.value() == "42");

      buf.clear();
      expect(not glz::write_jsonb(true, buf));
      j = glz::jsonb_to_json(buf);
      expect(j.value() == "true");

      buf.clear();
      expect(not glz::write_jsonb(nullptr, buf));
      j = glz::jsonb_to_json(buf);
      expect(j.value() == "null");
   };

   "jsonb_to_json string escapes control chars"_test = [] {
      std::string buf;
      const std::string s = "a\nb\"c";
      expect(not glz::write_jsonb(s, buf));
      auto j = glz::jsonb_to_json(buf);
      expect(j.has_value());
      // Expect: "a\nb\"c" with backslash-escaped \n and \"
      expect(j.value() == std::string(R"("a\nb\"c")"));
   };

   "jsonb_to_json array"_test = [] {
      std::vector<int> v{1, 2, 3};
      std::string buf;
      expect(not glz::write_jsonb(v, buf));
      auto j = glz::jsonb_to_json(buf);
      expect(j.has_value());
      expect(j.value() == "[1,2,3]");
   };

   "jsonb_to_json object"_test = [] {
      std::map<std::string, int> m{{"a", 1}, {"b", 2}};
      std::string buf;
      expect(not glz::write_jsonb(m, buf));
      auto j = glz::jsonb_to_json(buf);
      expect(j.has_value());
      // std::map iterates in key order — deterministic.
      expect(j.value() == R"({"a":1,"b":2})");
   };

   "jsonb_to_json reflected struct"_test = [] {
      simple_struct s{};
      std::string buf;
      expect(not glz::write_jsonb(s, buf));
      auto j = glz::jsonb_to_json(buf);
      expect(j.has_value());
      // Reflected order: i, d, hello, arr
      expect(j.value() == R"({"i":287,"d":3.14,"hello":"Hello World","arr":[1,2,3]})");
   };

   "jsonb_to_json handles INT5 hex"_test = [] {
      std::string buf;
      buf.push_back(static_cast<char>((4u << 4) | 4u)); // INT5, size 4
      buf += "0xFF";
      auto j = glz::jsonb_to_json(buf);
      expect(j.has_value());
      expect(j.value() == "255");
   };

   "jsonb_to_json maps NaN/Inf to null"_test = [] {
      std::string buf;
      expect(not glz::write_jsonb(std::numeric_limits<double>::quiet_NaN(), buf));
      auto j = glz::jsonb_to_json(buf);
      expect(j.has_value());
      expect(j.value() == "null");

      buf.clear();
      expect(not glz::write_jsonb(std::numeric_limits<double>::infinity(), buf));
      j = glz::jsonb_to_json(buf);
      expect(j.value() == "null");
   };

   "jsonb_to_json TEXTJ passthrough"_test = [] {
      // Hand-built TEXTJ payload containing a \u escape: "a\uD83D\uDE00"
      const std::string payload = "a\\uD83D\\uDE00";
      std::string buf;
      buf.push_back(static_cast<char>((12u << 4) | 8u)); // TEXTJ, size_nibble=u8_follows
      buf.push_back(static_cast<char>(payload.size()));
      buf += payload;
      auto j = glz::jsonb_to_json(buf);
      expect(j.has_value());
      // Expected: "a\uD83D\uDE00"
      expect(j.value() == std::string("\"") + payload + "\"");
   };

   "jsonb_to_json TEXT5 with \\x produces strict JSON"_test = [] {
      // TEXT5 payload "A\x20B" decodes to "A B" (space). Raw pass-through would emit
      // "A\x20B" which is invalid JSON.
      const std::string payload = "A\\x20B";
      std::string buf;
      buf.push_back(static_cast<char>((payload.size() << 4) | 9u)); // TEXT5
      buf += payload;
      auto j = glz::jsonb_to_json(buf);
      expect(j.has_value());
      expect(j.value() == R"("A B")");
   };

   "jsonb_to_json TEXT5 with \\' produces strict JSON"_test = [] {
      // TEXT5 payload "it\\'s" decodes to "it's" — raw pass-through would emit \' which
      // is invalid JSON.
      const std::string payload = "it\\'s";
      std::string buf;
      buf.push_back(static_cast<char>((payload.size() << 4) | 9u));
      buf += payload;
      auto j = glz::jsonb_to_json(buf);
      expect(j.has_value());
      expect(j.value() == R"("it's")");
   };

   "jsonb_to_json TEXT5 with \\v produces strict JSON"_test = [] {
      // TEXT5 payload "x\\vy" decodes to "x\vy" (vertical tab) — the JSON writer will
      // escape control chars per RFC 8259.
      const std::string payload = "x\\vy";
      std::string buf;
      buf.push_back(static_cast<char>((payload.size() << 4) | 9u));
      buf += payload;
      auto j = glz::jsonb_to_json(buf);
      expect(j.has_value());
      // The JSON writer escapes vertical tab as \u000B (or similar).
      expect(j.value().find("\\v") == std::string::npos); // no JSON5 escape in output
      expect(j.value().front() == '"' && j.value().back() == '"');
   };

   "jsonb_to_json TEXT5 with line continuation produces strict JSON"_test = [] {
      // TEXT5 payload "ab\\\nc" — the \<LF> line continuation decodes to nothing, so
      // the output is "abc".
      std::string payload;
      payload.append("ab");
      payload.push_back('\\');
      payload.push_back('\n');
      payload.append("c");
      std::string buf;
      buf.push_back(static_cast<char>((payload.size() << 4) | 9u));
      buf += payload;
      auto j = glz::jsonb_to_json(buf);
      expect(j.has_value());
      expect(j.value() == R"("abc")");
   };
};

suite large_container_tests = [] {
   "vector with u16 size header"_test = [] {
      // A vector whose serialized payload > 255 bytes → forces u16_follows container header on read path
      // (write uses always-9 strategy, reader just sees payload_size ≥ 256).
      std::vector<int> v;
      for (int i = 0; i < 200; ++i) v.push_back(i);
      std::string buf;
      expect(not glz::write_jsonb(v, buf));
      std::vector<int> out;
      expect(not glz::read_jsonb(out, buf));
      expect(out == v);
   };

   "deeply nested structure"_test = [] {
      using Node = std::vector<std::vector<std::vector<int>>>;
      Node root;
      for (int i = 0; i < 3; ++i) {
         std::vector<std::vector<int>> mid;
         for (int j = 0; j < 3; ++j) {
            mid.push_back({i, j, i * j});
         }
         root.push_back(std::move(mid));
      }
      std::string buf;
      expect(not glz::write_jsonb(root, buf));
      Node out;
      expect(not glz::read_jsonb(out, buf));
      expect(out == root);
   };

   "unicode in string round-trip"_test = [] {
      // UTF-8 encoded: "héllo · 世界 · 🌍"
      std::string s = "h\xC3\xA9llo \xC2\xB7 \xE4\xB8\x96\xE7\x95\x8C \xC2\xB7 \xF0\x9F\x8C\x8D";
      std::string buf;
      expect(not glz::write_jsonb(s, buf));
      std::string out;
      expect(not glz::read_jsonb(out, buf));
      expect(out == s);
   };
};

struct many_fields
{
   int alpha{};
   int beta{};
   int gamma{};
   int delta{};
   int epsilon{};
   int zeta{};
   int eta{};
   int theta{};
   int iota{};
   int kappa{};
   bool operator==(const many_fields&) const = default;
};

suite hash_lookup_tests = [] {
   "reflected struct with many fields"_test = [] {
      many_fields mf{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
      std::string buf;
      expect(not glz::write_jsonb(mf, buf));
      many_fields out{};
      expect(not glz::read_jsonb(out, buf));
      expect(out == mf);
   };

   "unknown keys are skipped by default"_test = [] {
      // Object with an extra "unknown" field between known ones.
      // Build by hand: {"alpha":1, "unknown":"ignoreme", "beta":2}
      std::string buf;
      // Reserve 9-byte object header
      buf.resize(9);
      const size_t payload_start = buf.size();

      auto push_textraw = [&](std::string_view s) {
         const size_t n = s.size();
         if (n <= 11) {
            buf.push_back(static_cast<char>((n << 4) | 10u));
         }
         else {
            buf.push_back(static_cast<char>((12u << 4) | 10u));
            buf.push_back(static_cast<char>(n));
         }
         buf.append(s.data(), s.size());
      };
      auto push_int = [&](int v) {
         std::string digits = std::to_string(v);
         const size_t n = digits.size();
         buf.push_back(static_cast<char>((n << 4) | 3u));
         buf.append(digits);
      };

      push_textraw("alpha");
      push_int(1);
      push_textraw("unknown");
      push_textraw("ignoreme");
      push_textraw("beta");
      push_int(2);

      const uint64_t payload_size = buf.size() - payload_start;
      // Patch 9-byte header: (0xFx | 0x0C) size_code u64_follows, type object (12)
      buf[0] = static_cast<char>((15u << 4) | 12u);
      uint64_t v = payload_size;
      if constexpr (std::endian::native == std::endian::little) {
         v = std::byteswap(v);
      }
      std::memcpy(&buf[1], &v, 8);

      many_fields out{};
      constexpr glz::opts opts{.format = glz::JSONB, .error_on_unknown_keys = false};
      expect(not glz::read<opts>(out, buf));
      expect(out.alpha == 1);
      expect(out.beta == 2);
      expect(out.gamma == 0); // unchanged default
   };

   "unknown key errors by default"_test = [] {
      // Same hand-built buffer as above, but confirm the default opts error on unknown keys.
      std::string buf;
      buf.resize(9);
      const size_t payload_start = buf.size();
      auto push_textraw = [&](std::string_view s) {
         const size_t n = s.size();
         if (n <= 11)
            buf.push_back(static_cast<char>((n << 4) | 10u));
         else {
            buf.push_back(static_cast<char>((12u << 4) | 10u));
            buf.push_back(static_cast<char>(n));
         }
         buf.append(s.data(), s.size());
      };
      auto push_int = [&](int v) {
         std::string digits = std::to_string(v);
         buf.push_back(static_cast<char>((digits.size() << 4) | 3u));
         buf.append(digits);
      };
      push_textraw("alpha");
      push_int(1);
      push_textraw("unknown");
      push_textraw("ignoreme");
      const uint64_t payload_size = buf.size() - payload_start;
      buf[0] = static_cast<char>((15u << 4) | 12u);
      uint64_t v = payload_size;
      if constexpr (std::endian::native == std::endian::little) v = std::byteswap(v);
      std::memcpy(&buf[1], &v, 8);

      many_fields out{};
      auto ec = glz::read_jsonb(out, buf);
      expect(bool(ec));
      expect(ec.ec == glz::error_code::unknown_key);
   };

   "TEXTJ key matches reflected field"_test = [] {
      // Hand-built object with TEXTJ key "alp\u0068a" (which decodes to "alpha") and value 42.
      std::string buf;
      buf.resize(9);
      const size_t payload_start = buf.size();

      // Key: TEXTJ, payload "alp\u0068a" (9 bytes)
      const std::string textj_key = "alp\\u0068a";
      buf.push_back(static_cast<char>((12u << 4) | 8u)); // TEXTJ, u8_follows
      buf.push_back(static_cast<char>(textj_key.size()));
      buf.append(textj_key);

      // Value: INT "42"
      buf.push_back(static_cast<char>((2u << 4) | 3u));
      buf.append("42");

      const uint64_t payload_size = buf.size() - payload_start;
      buf[0] = static_cast<char>((15u << 4) | 12u);
      uint64_t v = payload_size;
      if constexpr (std::endian::native == std::endian::little) {
         v = std::byteswap(v);
      }
      std::memcpy(&buf[1], &v, 8);

      many_fields out{};
      expect(not glz::read_jsonb(out, buf));
      expect(out.alpha == 42);
   };
};

suite generic_tests = [] {
   "generic scalar round-trips"_test = [] {
      {
         glz::generic g = nullptr;
         std::string buf;
         expect(not glz::write_jsonb(g, buf));
         glz::generic out;
         expect(not glz::read_jsonb(out, buf));
         expect(out.is_null());
      }
      {
         glz::generic g = true;
         std::string buf;
         expect(not glz::write_jsonb(g, buf));
         glz::generic out;
         expect(not glz::read_jsonb(out, buf));
         expect(out.is_boolean());
         expect(out.get<bool>() == true);
      }
      {
         glz::generic g = "hello";
         std::string buf;
         expect(not glz::write_jsonb(g, buf));
         glz::generic out;
         expect(not glz::read_jsonb(out, buf));
         expect(out.is_string());
         expect(out.get<std::string>() == "hello");
      }
   };

   "generic number — default f64 mode"_test = [] {
      glz::generic g = 42.5;
      std::string buf;
      expect(not glz::write_jsonb(g, buf));
      // Header byte: type should be FLOAT (5) since generic<f64> always stores doubles.
      expect(glz::jsonb::get_type(static_cast<uint8_t>(buf[0])) == glz::jsonb::type::float_);
      glz::generic out;
      expect(not glz::read_jsonb(out, buf));
      expect(out.get<double>() == 42.5);
   };

   "generic_i64 emits INT for integer values"_test = [] {
      glz::generic_i64 g;
      g.data = int64_t{42};
      std::string buf;
      expect(not glz::write_jsonb(g, buf));
      expect(glz::jsonb::get_type(static_cast<uint8_t>(buf[0])) == glz::jsonb::type::int_);
      glz::generic_i64 out;
      expect(not glz::read_jsonb(out, buf));
      expect(out.is_int64());
      expect(out.get<int64_t>() == 42);
   };

   "generic_u64 preserves large unsigned"_test = [] {
      glz::generic_u64 g;
      g.data = uint64_t{9876543210123456789ULL};
      std::string buf;
      expect(not glz::write_jsonb(g, buf));
      glz::generic_u64 out;
      expect(not glz::read_jsonb(out, buf));
      expect(out.is_uint64());
      expect(out.get<uint64_t>() == 9876543210123456789ULL);
   };

   "generic array round-trip"_test = [] {
      glz::generic g;
      glz::generic::array_t arr;
      arr.emplace_back();
      arr.back().data = 1.0;
      arr.emplace_back();
      arr.back().data = std::string("two");
      arr.emplace_back();
      arr.back().data = true;
      g.data = std::move(arr);

      std::string buf;
      expect(not glz::write_jsonb(g, buf));
      expect(glz::jsonb::get_type(static_cast<uint8_t>(buf[0])) == glz::jsonb::type::array);
      glz::generic out;
      expect(not glz::read_jsonb(out, buf));
      expect(out.is_array());
      const auto& a = out.get_array();
      expect(a.size() == 3);
      expect(a[0].get<double>() == 1.0);
      expect(a[1].get<std::string>() == "two");
      expect(a[2].get<bool>() == true);
   };

   "generic object round-trip"_test = [] {
      glz::generic g;
      glz::generic::object_t obj;
      glz::generic a;
      a.data = 1.0;
      glz::generic b;
      b.data = std::string("s");
      obj.insert(std::make_pair(std::string("a"), std::move(a)));
      obj.insert(std::make_pair(std::string("b"), std::move(b)));
      g.data = std::move(obj);

      std::string buf;
      expect(not glz::write_jsonb(g, buf));
      expect(glz::jsonb::get_type(static_cast<uint8_t>(buf[0])) == glz::jsonb::type::object);
      glz::generic out;
      expect(not glz::read_jsonb(out, buf));
      expect(out.is_object());
      const auto& o = out.get_object();
      expect(o.size() == 2);
   };

   "generic round-trips through JSON parse"_test = [] {
      glz::generic g;
      expect(not glz::read_json(g, R"({"x":1.5,"y":[true,null,"z"]})"));
      std::string buf;
      expect(not glz::write_jsonb(g, buf));
      glz::generic out;
      expect(not glz::read_jsonb(out, buf));
      // Dump back to JSON and compare after normalizing key order.
      std::string dumped;
      expect(not glz::write_json(out, dumped));
      expect(dumped == R"({"x":1.5,"y":[true,null,"z"]})");
   };
};

suite integer_boundary_tests = [] {
   "int8_t extremes"_test = [] {
      for (int8_t v : {(std::numeric_limits<int8_t>::min)(), int8_t{-1}, int8_t{0}, int8_t{1},
                       (std::numeric_limits<int8_t>::max)()}) {
         std::string buf;
         expect(not glz::write_jsonb(v, buf));
         int8_t out = 0;
         expect(not glz::read_jsonb(out, buf));
         expect(out == v);
      }
   };

   "int32_t extremes"_test = [] {
      for (int32_t v :
           {(std::numeric_limits<int32_t>::min)(), int32_t{-1}, int32_t{0}, (std::numeric_limits<int32_t>::max)()}) {
         std::string buf;
         expect(not glz::write_jsonb(v, buf));
         int32_t out = 0;
         expect(not glz::read_jsonb(out, buf));
         expect(out == v);
      }
   };

   "int64_t extremes"_test = [] {
      for (int64_t v : {(std::numeric_limits<int64_t>::min)(), int64_t{-1}, int64_t{0}, int64_t{1},
                        (std::numeric_limits<int64_t>::max)()}) {
         std::string buf;
         expect(not glz::write_jsonb(v, buf));
         int64_t out = 0;
         expect(not glz::read_jsonb(out, buf));
         expect(out == v);
      }
   };

   "uint64_t extremes"_test = [] {
      for (uint64_t v :
           {uint64_t{0}, uint64_t{1}, uint64_t{255}, uint64_t{65535}, (std::numeric_limits<uint64_t>::max)()}) {
         std::string buf;
         expect(not glz::write_jsonb(v, buf));
         uint64_t out = 0;
         expect(not glz::read_jsonb(out, buf));
         expect(out == v);
      }
   };
};

suite float_edge_tests = [] {
   "negative zero preserved"_test = [] {
      double v = -0.0;
      std::string buf;
      expect(not glz::write_jsonb(v, buf));
      double out = 0;
      expect(not glz::read_jsonb(out, buf));
      // Check bit-pattern equality (so -0 != +0 even though -0 == +0 is true)
      uint64_t vb, ob;
      std::memcpy(&vb, &v, 8);
      std::memcpy(&ob, &out, 8);
      expect(vb == ob);
   };

   "smallest subnormal"_test = [] {
      double v = std::numeric_limits<double>::denorm_min();
      std::string buf;
      expect(not glz::write_jsonb(v, buf));
      double out = 0;
      expect(not glz::read_jsonb(out, buf));
      expect(out == v);
   };

   "largest finite"_test = [] {
      double v = (std::numeric_limits<double>::max)();
      std::string buf;
      expect(not glz::write_jsonb(v, buf));
      double out = 0;
      expect(not glz::read_jsonb(out, buf));
      expect(out == v);
   };

   "float precision round-trip"_test = [] {
      // Value that exercises full double precision.
      double v = 1.0 + std::numeric_limits<double>::epsilon();
      std::string buf;
      expect(not glz::write_jsonb(v, buf));
      double out = 0;
      expect(not glz::read_jsonb(out, buf));
      expect(out == v);
   };
};

suite robustness_tests = [] {
   "empty buffer"_test = [] {
      std::string buf;
      int out = 0;
      auto ec = glz::read_jsonb(out, buf);
      expect(bool(ec));
      // The top-level read driver reports no_read_input for empty buffers before
      // reaching our read<JSONB> path.
      expect(ec.ec == glz::error_code::no_read_input);
   };

   "truncated multi-byte header (u16 size_code, missing size bytes)"_test = [] {
      std::string buf;
      // TEXTRAW (type 10) + size_nibble=13 (u16_follows), but no size bytes.
      buf.push_back(static_cast<char>((13u << 4) | 10u));
      std::string out;
      auto ec = glz::read_jsonb(out, buf);
      expect(bool(ec));
   };

   "truncated payload"_test = [] {
      std::string buf;
      buf.push_back(static_cast<char>((5u << 4) | 10u)); // TEXTRAW, size=5 inline
      buf.append("abc"); // only 3 bytes of payload instead of 5
      std::string out;
      auto ec = glz::read_jsonb(out, buf);
      expect(bool(ec));
   };

   "extra bytes after value rejected at top level"_test = [] {
      // Per the SQLite JSONB spec: "The element must exactly fill the BLOB." Trailing
      // bytes after a valid top-level element must be rejected.
      std::string buf;
      expect(not glz::write_jsonb(42, buf));
      const auto original_size = buf.size();
      buf.append(3, '\0');
      int out = 0;
      auto ec = glz::read_jsonb(out, buf);
      expect(bool(ec));
      expect(ec.ec == glz::error_code::syntax_error);
      // ec.count should report how many bytes of the input were successfully parsed
      // (the valid element), so diagnostic tools can point at the trailing garbage.
      expect(ec.count == original_size);
   };

   "exact-fill check: single-byte valid blob accepted"_test = [] {
      // Sanity: a minimal 1-byte blob (NULL = 0x00) where count == buffer.size() succeeds.
      std::string buf{static_cast<char>(0x00)};
      std::nullptr_t out;
      expect(not glz::read_jsonb(out, buf));
   };

   "exact-fill check: trailing byte after container rejected"_test = [] {
      std::vector<int> v{1, 2, 3};
      std::string buf;
      expect(not glz::write_jsonb(v, buf));
      buf.push_back('X');
      std::vector<int> out;
      auto ec = glz::read_jsonb(out, buf);
      expect(bool(ec));
      expect(ec.ec == glz::error_code::syntax_error);
   };

   "container with mismatched payload size"_test = [] {
      // Array claiming size 10 but containing just 2 bytes of elements.
      std::string buf;
      buf.push_back(static_cast<char>((10u << 4) | 11u)); // array, size=10
      // Two INT "1" elements = 4 bytes only.
      buf.push_back(static_cast<char>((1u << 4) | 3u));
      buf.push_back('1');
      buf.push_back(static_cast<char>((1u << 4) | 3u));
      buf.push_back('1');
      buf.append(6, '\0'); // pad up to 10 bytes of "payload" so size check passes

      std::vector<int> out;
      auto ec = glz::read_jsonb(out, buf);
      // Reader should either error (bogus interior bytes) or stop at the reported size.
      expect(bool(ec));
   };

   "non-string key in object rejected"_test = [] {
      // Object with a numeric key (type INT, not a TEXT variant) — invalid per spec.
      std::string buf;
      buf.resize(9); // 9-byte object header
      const size_t payload_start = buf.size();

      buf.push_back(static_cast<char>((1u << 4) | 3u)); // INT key "1"
      buf.push_back('1');
      buf.push_back(static_cast<char>((1u << 4) | 3u)); // INT value "1"
      buf.push_back('1');

      const uint64_t payload_size = buf.size() - payload_start;
      buf[0] = static_cast<char>((15u << 4) | 12u); // object, u64_follows
      uint64_t v = payload_size;
      if constexpr (std::endian::native == std::endian::little) v = std::byteswap(v);
      std::memcpy(&buf[1], &v, 8);

      std::map<std::string, int> out;
      auto ec = glz::read_jsonb(out, buf);
      expect(bool(ec));
   };

   "invalid INT5 payload with wrong hex digits"_test = [] {
      std::string buf;
      buf.push_back(static_cast<char>((5u << 4) | 4u)); // INT5, size 5
      buf.append("0xZZZ");
      int out = 0;
      auto ec = glz::read_jsonb(out, buf);
      expect(bool(ec));
   };

   "INT with non-numeric payload"_test = [] {
      std::string buf;
      buf.push_back(static_cast<char>((3u << 4) | 3u)); // INT, size 3
      buf.append("abc");
      int out = 0;
      auto ec = glz::read_jsonb(out, buf);
      expect(bool(ec));
   };
};

suite file_io_tests = [] {
   "read_file / write_file round-trip"_test = [] {
      const auto path = std::filesystem::temp_directory_path() / "glaze_jsonb_roundtrip.bin";
      const std::string pstr = path.string();

      simple_struct s{};
      s.hello = "file io test";
      s.i = 777;
      s.d = -0.5;
      s.arr = {100, 200, 300};

      std::string write_buf;
      auto write_ec = glz::write_file_jsonb(s, pstr, write_buf);
      expect(!bool(write_ec));

      simple_struct out{};
      std::string read_buf;
      auto read_ec = glz::read_file_jsonb(out, pstr, read_buf);
      expect(!bool(read_ec));
      expect(out == s);

      std::error_code ignored;
      std::filesystem::remove(path, ignored);
   };
};

suite expected_tests = [] {
   "expected<int, string> write success"_test = [] {
      std::expected<int, std::string> v = 42;
      std::string buf;
      expect(not glz::write_jsonb(v, buf));
      // Should be INT "42" directly — not wrapped.
      expect(buf.size() == 3);
      expect(glz::jsonb::get_type(static_cast<uint8_t>(buf[0])) == glz::jsonb::type::int_);
   };

   "expected<int, string> write error"_test = [] {
      std::expected<int, std::string> v = std::unexpected(std::string("bad"));
      std::string buf;
      expect(not glz::write_jsonb(v, buf));
      // Should be an OBJECT with one key "unexpected" → "bad"
      expect(glz::jsonb::get_type(static_cast<uint8_t>(buf[0])) == glz::jsonb::type::object);
   };

   "expected<int, string> read success round-trip"_test = [] {
      std::expected<int, std::string> v = 123;
      std::string buf;
      expect(not glz::write_jsonb(v, buf));
      std::expected<int, std::string> out;
      expect(not glz::read_jsonb(out, buf));
      expect(out.has_value());
      expect(*out == 123);
   };

   "expected<int, string> read error round-trip"_test = [] {
      std::expected<int, std::string> v = std::unexpected(std::string("oops"));
      std::string buf;
      expect(not glz::write_jsonb(v, buf));
      std::expected<int, std::string> out = 0;
      expect(not glz::read_jsonb(out, buf));
      expect(not out.has_value());
      expect(out.error() == "oops");
   };
};

struct with_optionals
{
   int id{};
   std::optional<std::string> nickname{};
   std::unique_ptr<int> score{};
};

struct with_defaults
{
   // skip_default_members compares against the zero/empty value, not the declared
   // initializer — so these are all zero-initialized defaults.
   int a{};
   int b{};
   std::string s{};
   std::vector<int> v{};
};

struct required_fields_struct
{
   std::string name;
   int age{};
   std::optional<std::string> nickname;
};

suite skip_null_members_tests = [] {
   "skip_null_members=true omits null optional + unique_ptr"_test = [] {
      with_optionals v{.id = 5};
      std::string buf;
      constexpr glz::opts O{.format = glz::JSONB, .skip_null_members = true};
      expect(not glz::write<O>(v, buf));

      // Convert to JSON to inspect structure.
      auto j = glz::jsonb_to_json(buf);
      expect(j.has_value());
      expect(j.value() == R"({"id":5})");
   };

   "skip_null_members=false (explicit) emits nulls"_test = [] {
      with_optionals v{.id = 5};
      std::string buf;
      constexpr glz::opts O{.format = glz::JSONB, .skip_null_members = false};
      expect(not glz::write<O>(v, buf));
      auto j = glz::jsonb_to_json(buf);
      expect(j.has_value());
      expect(j.value() == R"({"id":5,"nickname":null,"score":null})");
   };

   "skip_null_members with set optional keeps it"_test = [] {
      with_optionals v{.id = 5, .nickname = "Al"};
      std::string buf;
      constexpr glz::opts O{.format = glz::JSONB, .skip_null_members = true};
      expect(not glz::write<O>(v, buf));
      auto j = glz::jsonb_to_json(buf);
      expect(j.has_value());
      expect(j.value() == R"({"id":5,"nickname":"Al"})");
   };
};

suite skip_default_members_tests = [] {
   // check_skip_default_members is off by default; define a custom opts struct that enables it.
   struct opts_with_default_skip : glz::opts
   {
      bool skip_default_members = true;
   };

   "skip_default_members omits zero/empty-valued fields"_test = [] {
      with_defaults v{}; // all zero/empty
      std::string buf;
      constexpr opts_with_default_skip O{{.format = glz::JSONB}};
      expect(not glz::write<O>(v, buf));
      auto j = glz::jsonb_to_json(buf);
      expect(j.has_value());
      expect(j.value() == "{}");
   };

   "skip_default_members keeps non-zero fields"_test = [] {
      with_defaults v{};
      v.b = 99;
      v.s = "changed";
      v.v = {1};
      std::string buf;
      constexpr opts_with_default_skip O{{.format = glz::JSONB}};
      expect(not glz::write<O>(v, buf));
      auto j = glz::jsonb_to_json(buf);
      expect(j.has_value());
      expect(j.value() == R"({"b":99,"s":"changed","v":[1]})");
   };
};

suite error_on_missing_keys_tests = [] {
   "missing required field errors"_test = [] {
      // Build JSONB for {"age":30} — "name" is missing and required (non-nullable).
      std::string buf;
      buf.resize(9);
      const size_t payload_start = buf.size();
      auto push_textraw = [&](std::string_view s) {
         buf.push_back(static_cast<char>((s.size() << 4) | 10u));
         buf.append(s.data(), s.size());
      };
      push_textraw("age");
      buf.push_back(static_cast<char>((2u << 4) | 3u));
      buf.append("30");
      const uint64_t payload_size = buf.size() - payload_start;
      buf[0] = static_cast<char>((15u << 4) | 12u);
      uint64_t v = payload_size;
      if constexpr (std::endian::native == std::endian::little) v = std::byteswap(v);
      std::memcpy(&buf[1], &v, 8);

      required_fields_struct out{};
      constexpr glz::opts O{.format = glz::JSONB, .error_on_missing_keys = true};
      auto ec = glz::read<O>(out, buf);
      expect(bool(ec));
      expect(ec.ec == glz::error_code::missing_key);
      expect(ec.custom_error_message == "name"); // the first missing required key
   };

   "all required fields present → no error"_test = [] {
      required_fields_struct src{"Bob", 40, std::nullopt};
      std::string buf;
      expect(not glz::write_jsonb(src, buf));
      required_fields_struct out{};
      constexpr glz::opts O{.format = glz::JSONB, .error_on_missing_keys = true};
      expect(not glz::read<O>(out, buf));
      expect(out.name == "Bob");
      expect(out.age == 40);
   };

   "missing nullable field is allowed"_test = [] {
      required_fields_struct src{"Eve", 22, std::nullopt};
      std::string buf;
      // Write with skip_null_members to exclude the nullable "nickname" field entirely.
      constexpr glz::opts write_opts{.format = glz::JSONB, .skip_null_members = true};
      expect(not glz::write<write_opts>(src, buf));

      required_fields_struct out{};
      constexpr glz::opts read_opts{.format = glz::JSONB, .error_on_missing_keys = true};
      expect(not glz::read<read_opts>(out, buf));
      expect(out.name == "Eve");
      expect(!out.nickname.has_value());
   };

   "error_on_missing_keys=false (default) tolerates missing required field"_test = [] {
      // Same buffer as the first test above — name missing.
      std::string buf;
      buf.resize(9);
      const size_t payload_start = buf.size();
      buf.push_back(static_cast<char>((3u << 4) | 10u));
      buf.append("age");
      buf.push_back(static_cast<char>((2u << 4) | 3u));
      buf.append("30");
      const uint64_t payload_size = buf.size() - payload_start;
      buf[0] = static_cast<char>((15u << 4) | 12u);
      uint64_t v = payload_size;
      if constexpr (std::endian::native == std::endian::little) v = std::byteswap(v);
      std::memcpy(&buf[1], &v, 8);

      required_fields_struct out{};
      expect(not glz::read_jsonb(out, buf));
      expect(out.age == 30);
      expect(out.name.empty()); // default
   };
};

// ---------- Realistic schemas ----------

struct address
{
   std::string street;
   std::string city;
   std::string country;
   std::optional<std::string> postal_code;
   bool operator==(const address&) const = default;
};

struct user_profile
{
   uint64_t id{};
   std::string email;
   std::string display_name;
   std::optional<address> home_address;
   std::vector<std::string> tags;
   std::map<std::string, std::string> preferences;
   bool active = true;
   bool operator==(const user_profile&) const = default;
};

struct api_page_meta
{
   int total{};
   int page{};
   int page_size{};
   std::optional<std::string> next_cursor;
   bool operator==(const api_page_meta&) const = default;
};

struct api_response
{
   std::vector<user_profile> items;
   api_page_meta meta;
   bool operator==(const api_response&) const = default;
};

struct logging_config
{
   std::string level = "info";
   std::optional<std::string> file;
   bool console = true;
   bool operator==(const logging_config&) const = default;
};

struct db_config
{
   std::string host;
   int port{};
   std::optional<std::string> username;
   std::optional<std::string> password;
   bool operator==(const db_config&) const = default;
};

struct app_config
{
   std::string name;
   std::string version;
   logging_config logging;
   db_config db;
   std::vector<std::string> features;
   std::map<std::string, std::string> env;
   bool operator==(const app_config&) const = default;
};

struct line_item
{
   std::string product_id;
   std::string name;
   int quantity{};
   double unit_price{};
   std::optional<double> discount_rate;
   bool operator==(const line_item&) const = default;
};

struct shopping_cart
{
   std::string cart_id;
   std::string user_id;
   std::vector<line_item> items;
   double subtotal{};
   double tax{};
   double total{};
   std::optional<std::string> coupon_code;
   bool operator==(const shopping_cart&) const = default;
};

struct event_log_entry
{
   int64_t timestamp_ms{};
   std::string event_type;
   std::string source;
   std::variant<std::string, double, int64_t, bool> value;
   std::map<std::string, std::string> attributes;
};

// Measurement data — heavy on numeric arrays (scientific / time-series style)
struct series
{
   std::string name;
   std::string unit;
   std::vector<double> values;
   std::vector<int64_t> timestamps;
   bool operator==(const series&) const = default;
};

struct dataset
{
   std::string id;
   std::string description;
   std::vector<series> signals;
   std::map<std::string, std::string> metadata;
   bool operator==(const dataset&) const = default;
};

suite realistic_scenarios = [] {
   "user profile round-trip"_test = [] {
      user_profile u{};
      u.id = 12345;
      u.email = "alice@example.com";
      u.display_name = "Alice Q. Developer";
      u.home_address = address{"742 Evergreen Terrace", "Springfield", "US", std::string{"49007"}};
      u.tags = {"admin", "beta-tester", "vip"};
      u.preferences = {{"theme", "dark"}, {"locale", "en-US"}, {"tz", "America/Chicago"}};
      u.active = true;

      std::string buf;
      expect(not glz::write_jsonb(u, buf));
      user_profile out{};
      expect(not glz::read_jsonb(out, buf));
      expect(out == u);
   };

   "user profile with null optional address and skip_null_members"_test = [] {
      user_profile u{};
      u.id = 7;
      u.email = "bob@example.com";
      u.display_name = "Bob";
      // home_address left nullopt
      u.tags = {};
      u.preferences = {};

      std::string buf;
      constexpr glz::opts O{.format = glz::JSONB, .skip_null_members = true};
      expect(not glz::write<O>(u, buf));

      // Confirm the null home_address did not get serialized — converted JSON must not contain "home_address".
      auto j = glz::jsonb_to_json(buf);
      expect(j.has_value());
      expect(j.value().find("home_address") == std::string::npos);

      // Round-trip should still work.
      user_profile out{};
      expect(not glz::read_jsonb(out, buf));
      expect(out.id == 7);
      expect(!out.home_address.has_value());
   };

   "paginated api response round-trip"_test = [] {
      api_response r{};
      for (int i = 0; i < 10; ++i) {
         user_profile u{};
         u.id = static_cast<uint64_t>(1000 + i);
         u.email = "u" + std::to_string(i) + "@test";
         u.display_name = "User" + std::to_string(i);
         u.tags = {"t-" + std::to_string(i)};
         r.items.push_back(std::move(u));
      }
      r.meta.total = 137;
      r.meta.page = 1;
      r.meta.page_size = 10;
      r.meta.next_cursor = std::string{"eyJvZmZzZXQiOjEwfQ=="};

      std::string buf;
      expect(not glz::write_jsonb(r, buf));
      api_response out{};
      expect(not glz::read_jsonb(out, buf));
      expect(out == r);
      expect(out.items.size() == 10);
      expect(out.meta.next_cursor.value() == "eyJvZmZzZXQiOjEwfQ==");
   };

   "app config with skip_null_members"_test = [] {
      app_config c{};
      c.name = "glaze-service";
      c.version = "1.2.3";
      c.logging.level = "debug";
      c.logging.file = std::string{"/var/log/glaze.log"};
      c.db.host = "localhost";
      c.db.port = 5432;
      // db.username and db.password left null deliberately
      c.features = {"feature-a", "feature-b"};
      c.env = {{"ENV", "prod"}, {"REGION", "us-east-1"}};

      std::string buf;
      constexpr glz::opts O{.format = glz::JSONB, .skip_null_members = true};
      expect(not glz::write<O>(c, buf));

      auto j = glz::jsonb_to_json(buf);
      expect(j.has_value());
      // Passwords must not leak into output when null.
      expect(j.value().find("password") == std::string::npos);
      expect(j.value().find("username") == std::string::npos);

      // Round-trip: reload with skip_null_members inherits defaults for missing optionals.
      app_config out{};
      expect(not glz::read_jsonb(out, buf));
      expect(out.name == c.name);
      expect(out.db.host == "localhost");
      expect(out.db.port == 5432);
      expect(!out.db.username.has_value());
      expect(!out.db.password.has_value());
      expect(out.features == c.features);
      expect(out.env == c.env);
   };

   "shopping cart numerical precision"_test = [] {
      shopping_cart cart{};
      cart.cart_id = "cart-42";
      cart.user_id = "user-99";
      cart.items = {
         {"SKU-001", "Widget", 3, 9.99, std::nullopt},
         {"SKU-002", "Gadget", 1, 29.95, std::optional<double>{0.10}},
         {"SKU-003", "Doohickey", 2, 4.50, std::nullopt},
      };
      cart.subtotal = 68.86;
      cart.tax = 5.51;
      cart.total = 74.37;
      cart.coupon_code = std::string{"SAVE10"};

      std::string buf;
      expect(not glz::write_jsonb(cart, buf));
      shopping_cart out{};
      expect(not glz::read_jsonb(out, buf));
      expect(out == cart);
      expect(out.items[1].discount_rate.value() == 0.10);
   };

   "event log with variant payload"_test = [] {
      // A homogeneous vector of entries, each with a variant-typed value.
      std::vector<event_log_entry> log;
      {
         event_log_entry e{};
         e.timestamp_ms = 1713300000000LL;
         e.event_type = "login";
         e.source = "auth-svc";
         e.value = std::string{"ok"};
         e.attributes = {{"ip", "10.0.0.5"}};
         log.push_back(e);
      }
      {
         event_log_entry e{};
         e.timestamp_ms = 1713300001000LL;
         e.event_type = "latency";
         e.source = "api-svc";
         e.value = 42.7;
         e.attributes = {{"endpoint", "/users"}};
         log.push_back(e);
      }
      {
         event_log_entry e{};
         e.timestamp_ms = 1713300002000LL;
         e.event_type = "count";
         e.source = "worker";
         e.value = int64_t{1337};
         log.push_back(e);
      }
      {
         event_log_entry e{};
         e.timestamp_ms = 1713300003000LL;
         e.event_type = "healthy";
         e.source = "probe";
         e.value = true;
         log.push_back(e);
      }

      std::string buf;
      expect(not glz::write_jsonb(log, buf));
      std::vector<event_log_entry> out;
      expect(not glz::read_jsonb(out, buf));
      expect(out.size() == log.size());
      expect(std::get<std::string>(out[0].value) == "ok");
      expect(std::get<double>(out[1].value) == 42.7);
      expect(std::get<int64_t>(out[2].value) == 1337);
      expect(std::get<bool>(out[3].value) == true);
      expect(out[0].attributes.at("ip") == "10.0.0.5");
   };

   "scientific dataset with heavy numeric arrays"_test = [] {
      dataset d{};
      d.id = "run-2026-04-16-001";
      d.description = "voltage sweep";
      d.metadata = {{"operator", "K. Tesla"}, {"bench", "lab-3"}};

      series v{}, t{};
      v.name = "voltage";
      v.unit = "V";
      t.name = "current";
      t.unit = "A";
      for (int i = 0; i < 500; ++i) {
         v.values.push_back(0.01 * i);
         v.timestamps.push_back(1'000'000 + i * 1000);
         t.values.push_back(std::sin(0.01 * i));
         t.timestamps.push_back(1'000'000 + i * 1000);
      }
      d.signals = {std::move(v), std::move(t)};

      std::string buf;
      expect(not glz::write_jsonb(d, buf));
      dataset out{};
      expect(not glz::read_jsonb(out, buf));
      expect(out == d);
   };

   "round-trip stability: write-read-write produces identical bytes"_test = [] {
      // Byte-level stability matters when blobs are used as keys / hashed / signed.
      user_profile u{};
      u.id = 42;
      u.email = "stable@example.com";
      u.display_name = "Stability";
      u.home_address = address{"1 Main", "Anywhere", "US", std::nullopt};
      u.tags = {"alpha", "beta", "gamma"};
      u.preferences = {{"k1", "v1"}, {"k2", "v2"}};

      std::string first;
      expect(not glz::write_jsonb(u, first));

      user_profile parsed{};
      expect(not glz::read_jsonb(parsed, first));

      std::string second;
      expect(not glz::write_jsonb(parsed, second));

      expect(first == second);
   };

   "large map of profiles"_test = [] {
      // 200 keys exercises a u16-size-header object payload.
      std::map<std::string, user_profile> users;
      for (int i = 0; i < 200; ++i) {
         user_profile u{};
         u.id = static_cast<uint64_t>(i);
         u.email = "user" + std::to_string(i) + "@example.com";
         u.display_name = "User " + std::to_string(i);
         u.tags = {"t"};
         users.emplace("key_" + std::to_string(i), std::move(u));
      }

      std::string buf;
      expect(not glz::write_jsonb(users, buf));
      std::map<std::string, user_profile> out;
      expect(not glz::read_jsonb(out, buf));
      expect(out.size() == 200);
      expect(out["key_123"].email == "user123@example.com");
   };

   "nested optional optional"_test = [] {
      // std::optional<std::optional<int>> — unusual but should round-trip.
      std::optional<std::optional<int>> v = std::optional<int>{5};
      std::string buf;
      expect(not glz::write_jsonb(v, buf));
      std::optional<std::optional<int>> out;
      expect(not glz::read_jsonb(out, buf));
      expect(out.has_value());
      expect(out->has_value());
      expect(**out == 5);
   };

   "vector of variant with mixed types"_test = [] {
      using V = std::variant<int, std::string, double, bool>;
      std::vector<V> v = {V{42}, V{std::string{"hello"}}, V{3.14}, V{true}, V{-7}};
      std::string buf;
      expect(not glz::write_jsonb(v, buf));
      std::vector<V> out;
      expect(not glz::read_jsonb(out, buf));
      expect(out.size() == v.size());
      expect(std::get<int>(out[0]) == 42);
      expect(std::get<std::string>(out[1]) == "hello");
      expect(std::get<double>(out[2]) == 3.14);
      expect(std::get<bool>(out[3]) == true);
      expect(std::get<int>(out[4]) == -7);
   };
};

#if __cpp_exceptions
suite exception_api_tests = [] {
   "ex::write_jsonb and ex::read_jsonb round-trip"_test = [] {
      auto buf = glz::ex::write_jsonb(std::vector<int>{1, 2, 3});
      auto out = glz::ex::read_jsonb<std::vector<int>>(buf);
      expect(out == std::vector<int>{1, 2, 3});
   };

   "ex::read_jsonb throws on malformed input"_test = [] {
      std::string buf;
      buf.push_back(static_cast<char>(0x00 | 13u)); // reserved type
      int out = 0;
      bool threw = false;
      try {
         glz::ex::read_jsonb(out, buf);
      }
      catch (const std::runtime_error&) {
         threw = true;
      }
      expect(threw);
   };

   "ex::jsonb_to_json"_test = [] {
      auto buf = glz::ex::write_jsonb(std::map<std::string, int>{{"a", 1}, {"b", 2}});
      const auto json = glz::ex::jsonb_to_json(buf);
      expect(json == R"({"a":1,"b":2})");
   };
};
#endif

int main() { return 0; }
