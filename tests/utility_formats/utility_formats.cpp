// Glaze Library
// For the license information refer to glaze.hpp

#include <ut/ut.hpp>

#include "glaze/base64/base64.hpp"
#include "glaze/util/progress_bar.hpp"

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

suite progress_bar_tests = [] {
   "progress bar 30%"_test = [] {
      glz::progress_bar bar{.width = 12, .completed = 3, .total = 10, .time_taken = 30.0};
      expect(bar.string() == "[===-------] 30% | ETA: 1m 10s | 3/10") << bar.string();
   };

   "progress bar 100%"_test = [] {
      glz::progress_bar bar{.width = 12, .completed = 10, .total = 10, .time_taken = 30.0};
      expect(bar.string() == "[==========] 100% | ETA: 0m 0s | 10/10") << bar.string();
   };
};

int main() { return 0; }
