// Glaze Library
// For the license information refer to glaze.hpp

#include "glaze/jsonb.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "ut/ut.hpp"

using namespace ut;

static_assert(glz::write_supported<int, glz::JSONB>);
static_assert(glz::read_supported<int, glz::JSONB>);

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
      // TEXTRAW (type 10), inline size 5 => header (5<<4)|10 = 0x5A
      expect(static_cast<uint8_t>(buf[0]) == ((5u << 4) | 10u));
      std::string out;
      expect(not glz::read_jsonb(out, buf));
      expect(out == s);
   };

   "string requiring u8 size header"_test = [] {
      std::string buf;
      const std::string s = "Hello, world!";
      expect(not glz::write_jsonb(s, buf));
      // Size 13 > 11 inline limit, so header is 2 bytes: (12<<4)|10 = 0xCA then 0x0D
      expect(static_cast<uint8_t>(buf[0]) == ((12u << 4) | 10u));
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
      // Header byte: size_nibble = 12 (u8_follows), type = 10 (TEXTRAW)
      expect(static_cast<uint8_t>(buf[0]) == ((12u << 4) | 10u));
      expect(static_cast<uint8_t>(buf[1]) == 100u);
      std::string out;
      expect(not glz::read_jsonb(out, buf));
      expect(out == s);
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
      std::string s =
         "h\xC3\xA9llo \xC2\xB7 \xE4\xB8\x96\xE7\x95\x8C \xC2\xB7 \xF0\x9F\x8C\x8D";
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
         if (n <= 11) buf.push_back(static_cast<char>((n << 4) | 10u));
         else { buf.push_back(static_cast<char>((12u << 4) | 10u)); buf.push_back(static_cast<char>(n)); }
         buf.append(s.data(), s.size());
      };
      auto push_int = [&](int v) {
         std::string digits = std::to_string(v);
         buf.push_back(static_cast<char>((digits.size() << 4) | 3u));
         buf.append(digits);
      };
      push_textraw("alpha"); push_int(1);
      push_textraw("unknown"); push_textraw("ignoreme");
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
      arr.emplace_back(); arr.back().data = 1.0;
      arr.emplace_back(); arr.back().data = std::string("two");
      arr.emplace_back(); arr.back().data = true;
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
      glz::generic a; a.data = 1.0;
      glz::generic b; b.data = std::string("s");
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

int main() { return 0; }
