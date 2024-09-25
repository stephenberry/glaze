// Glaze Library
// For the license information refer to glaze.hpp

#define UT_RUN_TIME_ONLY

#include <ut/ut.hpp>

#include "glaze/base64/base64.hpp"

using namespace ut;

suite base64_read_tests = [] {
   "hello world"_test = [] {
      std::string_view b64 = "aGVsbG8gd29ybGQ=";
      const auto decoded = glz::read_base64(b64);
      expect(decoded == "hello world");
   };

   "{\"key\":42}"_test = [] {
      std::string_view b64 = "eyJrZXkiOjQyfQ==";
      const auto decoded = glz::read_base64(b64);
      expect(decoded == "{\"key\":42}");
   };
};

suite base64_roundtrip_tests = [] {
   "hello world"_test = [] {
      std::string_view str = "Hello World";
      const auto b64 = glz::write_base64(str);
      const auto decoded = glz::read_base64(b64);
      expect(decoded == str);
   };

   "{\"key\":42}"_test = [] {
      std::string_view str = "{\"key\":42}";
      const auto b64 = glz::write_base64(str);
      const auto decoded = glz::read_base64(b64);
      expect(decoded == str);
   };
};

int main() { return 0; }
