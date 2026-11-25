// Glaze Library
// For the license information refer to glaze.hpp

#include <map>
#include <string>
#include <string_view>

#include "glaze/beve.hpp"
#include "glaze/bson.hpp"
#include "glaze/cbor.hpp"
#include "glaze/glaze.hpp"
#include "glaze/jsonb.hpp"
#include "glaze/msgpack.hpp"
#include "glaze/toml.hpp"
#include "glaze/yaml.hpp"
#include "ut/ut.hpp"

using namespace ut;
using namespace std::string_literals;

namespace
{
   // Reaches glz::str_t through an implicit `operator const char*` rather than through a
   // string_view conversion. Writing by way of that conversion stops at the first embedded null
   // and ignores the type's own size(), so the type's bounds have to win.
   struct custom_string
   {
      using value_type = char;
      using size_type = size_t;

      std::string val{};

      operator const char*() const { return val.c_str(); }
      const char* data() const { return val.data(); }
      size_t size() const { return val.size(); }

      void assign(const char* v, size_t n) { val.assign(v, n); }
      void resize(size_t n) { val.resize(n); }

      auto operator<=>(const custom_string& other) const { return val <=> other.val; }
      bool operator==(const custom_string&) const = default;
   };

   static_assert(glz::string_t<custom_string>);

   // Three bytes, the middle one a null. Truncating at the null leaves just "a".
   const auto payload = "a\0b"s;

   struct wrapper
   {
      custom_string str{};
   };

   // Same shape and field name, but backed by a std::string whose bounds and string_view
   // conversion agree. Every format must encode the two identically.
   struct reference_wrapper
   {
      std::string str{};
   };

   template <auto Format, class T>
   std::string write_bytes(const T& value)
   {
      std::string buffer{};
      const auto ec = glz::write<glz::opts{.format = Format}>(value, buffer);
      expect(not ec) << "write failed: " << glz::format_error(ec, buffer);
      return buffer;
   }

   // The invariant under test, checked per format: a type reporting three bytes must encode
   // exactly like a std::string holding those same three bytes.
   template <auto Format>
   void expect_matches_std_string(std::string_view label)
   {
      const auto actual = write_bytes<Format>(wrapper{custom_string{payload}});
      const auto expected = write_bytes<Format>(reference_wrapper{payload});
      expect(actual == expected) << label << " truncated the payload: wrote " << actual.size() << " bytes, expected "
                                 << expected.size();
   }
}

template <>
struct glz::meta<wrapper>
{
   static constexpr auto value = object("str", &wrapper::str);
};

template <>
struct glz::meta<reference_wrapper>
{
   static constexpr auto value = object("str", &reference_wrapper::str);
};

suite embedded_null_tests = [] {
   "struct members keep embedded nulls"_test = [] {
      expect_matches_std_string<glz::JSON>("json");
      expect_matches_std_string<glz::BEVE>("beve");
      expect_matches_std_string<glz::CBOR>("cbor");
      expect_matches_std_string<glz::BSON>("bson");
      expect_matches_std_string<glz::JSONB>("jsonb");
      expect_matches_std_string<glz::MSGPACK>("msgpack");
      expect_matches_std_string<glz::TOML>("toml");
      expect_matches_std_string<glz::YAML>("yaml");
   };

   "top level values keep embedded nulls"_test = [] {
      // Pinned byte for byte so a regression names the wrong encoding rather than only its length.
      expect(write_bytes<glz::MSGPACK>(custom_string{payload}) == "\xa3"s + payload);
      expect(write_bytes<glz::CBOR>(custom_string{payload}) == "\x63"s + payload);

      // Glaze writes control characters unescaped (see issue #812), so JSON carries the raw null
      // exactly as it does for a std::string of the same contents.
      expect(write_bytes<glz::JSON>(custom_string{payload}) == "\"a\0b\""s);
      expect(write_bytes<glz::JSON>(custom_string{payload}) == write_bytes<glz::JSON>(payload));
   };

   "map keys keep embedded nulls"_test = [] {
      // Keys take a separate write path from values in several formats.
      const std::map<custom_string, int> actual{{custom_string{payload}, 1}};
      const std::map<std::string, int> expected{{payload, 1}};

      expect(write_bytes<glz::JSON>(actual) == write_bytes<glz::JSON>(expected));
      expect(write_bytes<glz::JSON>(actual) == "{\"a\0b\":1}"s);
      expect(write_bytes<glz::BEVE>(actual) == write_bytes<glz::BEVE>(expected));
   };
};

int main() { return 0; }
