// Glaze Library
// For the license information refer to glaze.ixx

import std;
import glaze;
import ut;

import glaze.util.string_literal;

using namespace ut;

using sv = glz::sv;

struct empty_object
{};

struct bool_object
{
   bool b{};
};

struct int_object
{
   int i{};
};

struct nullable_object
{
   std::optional<int> i{};
};

template <auto Opts>
inline void should_fail()
{
   "unclosed_array"_test = [] {
      constexpr sv s = R"(["Unclosed array")";
      {
         std::vector<std::string> v;
         expect(glz::read_json(v, s));
      }
      {
         std::deque<std::string> v;
         expect(glz::read_json(v, s));
      }
      {
         std::list<std::string> v;
         expect(glz::read_json(v, s));
      }
      {
         glz::generic obj{};
         expect(glz::read_json(obj, s));
      }
   };

   // A trailing comma with no following element, or a bare '[', must be rejected rather than
   // silently accepted as a complete array.
   "truncated array"_test = [] {
      for (const sv s : {sv{"["}, sv{"[ "}, sv{"[1,"}, sv{"[1, "}, sv{"[1,2,"}}) {
         {
            std::vector<int> v;
            expect(glz::read_json(v, s)) << s;
         }
         {
            std::deque<int> v;
            expect(glz::read_json(v, s)) << s;
         }
         {
            std::list<int> v;
            expect(glz::read_json(v, s)) << s;
         }
         {
            glz::generic obj{};
            expect(glz::read_json(obj, s)) << s;
         }
      }
   };

   "truncated nested array"_test = [] {
      for (const sv s : {sv{"[["}, sv{"[[1,"}, sv{"[[1],"}}) {
         std::vector<std::vector<int>> v;
         expect(glz::read_json(v, s)) << s;
      }
   };

   "unquoted_key"_test = [] {
      constexpr sv s = R"({unquoted_key: "keys must be quoted"})";
      {
         std::map<std::string, std::string> v;
         expect(glz::read_json(v, s));
      }
      {
         std::unordered_map<std::string, std::string> v;
         expect(glz::read_json(v, s));
      }
      {
         empty_object obj{};
         expect(glz::read_json(obj, s));
      }
      {
         glz::generic obj{};
         expect(glz::read_json(obj, s));
      }
   };

   "extra comma"_test = [] {
      constexpr sv s = R"(["extra comma",])";
      {
         std::vector<std::string> v;
         expect(glz::read_json(v, s));
      }
      {
         glz::generic obj{};
         expect(glz::read_json(obj, s));
      }
   };

   "double extra comma"_test = [] {
      constexpr sv s = R"(["double extra comma",,])";
      {
         std::vector<std::string> v;
         expect(glz::read_json(v, s));
      }
      {
         glz::generic obj{};
         expect(glz::read_json(obj, s));
      }
   };

   "missing value"_test = [] {
      constexpr sv s = R"([   , "<-- missing value"])";
      {
         std::vector<std::string> v;
         expect(glz::read_json(v, s));
      }
      {
         glz::generic obj{};
         expect(glz::read_json(obj, s));
      }
   };

   if constexpr (check_validate_trailing_whitespace(Opts)) {
      "comma after close"_test = [] {
         constexpr sv s = R"(["Comma after the close"],)";
         {
            std::vector<std::string> v;
            expect(glz::read<Opts>(v, s));
         }
      };

      "extra close"_test = [] {
         constexpr sv s = R"(["Extra close"]])";
         {
            std::vector<std::string> v;
            expect(glz::read<Opts>(v, s));
         }
      };

      "extra value after close"_test = [] {
         constexpr sv s = R"({"b": true} "misplaced quoted value")";
         {
            bool_object obj{};
            expect(glz::read<Opts>(obj, s));
         }
      };
   }

   "illegal expression"_test = [] {
      constexpr sv s = R"({"i": 1 + 2})";
      {
         int_object obj{};
         expect(glz::read_json(obj, s));
      }
      {
         glz::generic obj{};
         expect(glz::read_json(obj, s));
      }
   };

   "illegal invocation"_test = [] {
      constexpr sv s = R"({"i": alert()})";
      {
         int_object obj{};
         expect(glz::read_json(obj, s));
      }
      {
         glz::generic obj{};
         expect(glz::read_json(obj, s));
      }
   };

   "numbers cannot have leading zeroes"_test = [] {
      constexpr sv s = R"({"i": 013})";
      {
         int_object obj{};
         expect(glz::read_json(obj, s));
      }
      {
         glz::generic obj{};
         expect(glz::read_json(obj, s));
      }
   };

   "numbers cannot be hex"_test = [] {
      constexpr sv s = R"({"i": 0x14})";
      {
         int_object obj{};
         expect(glz::read_json(obj, s));
      }
      {
         glz::generic obj{};
         expect(glz::read_json(obj, s));
      }
   };

   "illegal backslash escape: \x15"_test = [] {
      constexpr sv s = R"(["Illegal backslash escape: \x15"])";
      {
         std::vector<std::string> v;
         expect(glz::read_json(v, s));
      }
      {
         glz::generic obj{};
         expect(glz::read_json(obj, s));
      }
   };

   "illegal control code short tail"_test = [] {
      constexpr sv s = "[\"ab\r\nd\"]";
      {
         std::vector<std::string> v;
         expect(glz::read_json(v, s));
      }
      {
         glz::generic obj{};
         expect(glz::read_json(obj, s));
      }
   };

   "illegal control code middle tail"_test = [] {
      constexpr sv s = "[\"ab\r\nd\",\"ab\"]";
      {
         std::vector<std::string> v;
         expect(glz::read_json(v, s));
      }
      {
         glz::generic obj{};
         expect(glz::read_json(obj, s));
      }
   };

   "illegal control code long tail"_test = [] {
      constexpr sv s = "[\"ab\r\nd\",\"abcdefghji\"]";
      {
         std::vector<std::string> v;
         expect(glz::read_json(v, s));
      }
      {
         glz::generic obj{};
         expect(glz::read_json(obj, s));
      }
   };

   "illegal control code u8string short tail"_test = [] {
      constexpr sv s = "[\"ab\r\nd\"]";
      {
         std::vector<std::u8string> v;
         expect(glz::read_json(v, s));
      }
   };

   "illegal control code u8string middle tail"_test = [] {
      constexpr sv s = "[\"ab\r\nd\",\"ab\"]";
      {
         std::vector<std::u8string> v;
         expect(glz::read_json(v, s));
      }
   };

   "illegal control code u8string long tail"_test = [] {
      constexpr sv s = "[\"ab\r\nd\",\"abcdefghji\"]";
      {
         std::vector<std::u8string> v;
         expect(glz::read_json(v, s));
      }
   };

   "illegal control code non-null-terminated"_test = [] {
      std::string input = "\"a\rb\"";
      std::string_view s{input.data(), input.size()};
      {
         std::string v;
         expect(glz::read<glz::opts{.null_terminated = false}>(v, s));
      }
      {
         std::u8string v;
         expect(glz::read<glz::opts{.null_terminated = false}>(v, s));
      }
   };

   "illegal backslash escape: \017"_test = [] {
      constexpr sv s = R"(["Illegal backslash escape: \017"])";
      {
         std::vector<std::string> v;
         expect(glz::read_json(v, s));
      }
      {
         glz::generic obj{};
         expect(glz::read_json(obj, s));
      }
   };

   "naked"_test = [] {
      constexpr sv s = R"([\naked])";
      {
         std::vector<std::string> v;
         expect(glz::read_json(v, s));
      }
      {
         glz::generic obj{};
         expect(glz::read_json(obj, s));
      }
   };

   "missing colon"_test = [] {
      constexpr sv s = R"({"i" null})";
      {
         nullable_object obj{};
         expect(glz::read_json(obj, s));
      }
      {
         glz::generic obj{};
         expect(glz::read_json(obj, s));
      }
   };

   "double colon"_test = [] {
      constexpr sv s = R"({"i":: null})";
      {
         nullable_object obj{};
         expect(glz::read_json(obj, s));
      }
      {
         glz::generic obj{};
         expect(glz::read_json(obj, s));
      }
   };

   "comma instead of colon"_test = [] {
      constexpr sv s = R"({"i", null})";
      {
         nullable_object obj{};
         expect(glz::read_json(obj, s));
      }
   };

   "colon instead of comma"_test = [] {
      constexpr sv s = R"(["Colon instead of comma": false])";
      {
         std::tuple<std::string, bool> v{};
         expect(glz::read_json(v, s));
      }
      {
         glz::generic obj{};
         expect(glz::read_json(obj, s));
      }
   };

   "bad value"_test = [] {
      constexpr sv s = R"(["Bad value", truth])";
      {
         std::tuple<std::string, bool> v{};
         expect(glz::read_json(v, s));
      }
      {
         glz::generic obj{};
         expect(glz::read_json(obj, s));
      }
   };

   "single quote"_test = [] {
      constexpr sv s = R"(['single quote'])";
      {
         std::vector<std::string> v;
         expect(glz::read_json(v, s));
      }
      {
         glz::generic obj{};
         expect(glz::read_json(obj, s));
      }
   };

   "0e"_test = [] {
      constexpr sv s = R"([0e])";
      {
         std::vector<double> v{};
         expect(glz::read_json(v, s));
      }
      {
         std::vector<float> v{};
         expect(glz::read_json(v, s));
      }
      {
         std::vector<int> v{};
         expect(glz::read_json(v, s));
      }
   };

   "0e+"_test = [] {
      constexpr sv s = R"([0e+])";
      {
         std::vector<double> v{};
         expect(glz::read_json(v, s));
      }
      {
         std::vector<float> v{};
         expect(glz::read_json(v, s));
      }
      {
         std::vector<int> v{};
         expect(glz::read_json(v, s));
      }
   };
}

template <auto Opts>
inline void should_pass()
{
   "bool_object"_test = [] {
      constexpr sv s = R"({"b": true})";
      {
         bool_object obj{};
         expect(!glz::read_json(obj, s));
         expect(obj.b == true);
      }
   };

   "int_object"_test = [] {
      constexpr sv s = R"({"i": 55})";
      {
         int_object obj{};
         expect(!glz::read_json(obj, s));
         expect(obj.i == 55);
      }
   };
}

struct opts_validate_trailing_whitespace : glz::opts
{
   bool validate_trailing_whitespace = true;
};

// RFC 8259 section 8.1 requires JSON text to be encoded in UTF-8. Read input is always someone
// else's data, so this is validated unconditionally rather than behind an option.
suite utf8_validation = [] {
   // Each of these is malformed UTF-8 in a distinct way.
   static const std::pair<sv, std::string> malformed[]{
      {"lone continuation byte", std::string("[\"a\x80!b\"]")},
      {"overlong encoding of NUL", std::string("[\"a\xC0\x80!b\"]")},
      {"truncated 3 byte sequence", std::string("[\"a\xE2\x82!b\"]")},
      {"invalid start byte", std::string("[\"a\xFF!b\"]")},
      {"encoded UTF-16 surrogate half", std::string("[\"a\xED\xA0\x80!b\"]")},
      {"code point above U+10FFFF", std::string("[\"a\xF5\x80\x80\x80!b\"]")},
   };

   "malformed utf8 rejected"_test = [] {
      for (const auto& [name, s] : malformed) {
         std::vector<std::string> v;
         const auto ec = glz::read_json(v, s);
         expect(ec == glz::error_code::invalid_utf8) << name;
      }
   };

   "malformed utf8 rejected by validate_json"_test = [] {
      for (const auto& [name, s] : malformed) {
         expect(bool(glz::validate_json(s))) << name;
      }
   };

   // Reading into glz::generic must be just as strict as reading into concrete types.
   "malformed utf8 rejected for generic"_test = [] {
      for (const auto& [name, s] : malformed) {
         glz::generic obj{};
         expect(glz::read_json(obj, s) == glz::error_code::invalid_utf8) << name;
      }
   };

   "well formed utf8 accepted"_test = [] {
      // Euro sign, 2 byte, and 4 byte (emoji) sequences.
      for (const sv s : {sv{"[\"a\xE2\x82\xAC"
                            "b\"]"},
                         sv{"[\"a\xC3\xA9"
                            "b\"]"},
                         sv{"[\"a\xF0\x9F\x98\x80"
                            "b\"]"}}) {
         {
            std::vector<std::string> v;
            expect(!glz::read_json(v, s)) << s;
         }
         expect(!glz::validate_json(s)) << s;
      }
   };

   // The scan loops take different paths for short strings and for the 8 byte SWAR bulk loop, so a
   // bad byte must be caught at either length.
   "malformed utf8 in a long string"_test = [] {
      const std::string s = "[\"" + std::string(64, 'a') + "\xFF" + std::string(64, 'b') + "\"]";
      std::vector<std::string> v;
      expect(glz::read_json(v, s) == glz::error_code::invalid_utf8);
   };

   "malformed utf8 in an object key"_test = [] {
      const std::string s = "{\"k\xFF!\":1}";
      {
         std::map<std::string, int> m;
         expect(glz::read_json(m, s) == glz::error_code::invalid_utf8);
      }
      expect(bool(glz::validate_json(s)));
   };

   "malformed utf8 for other string targets"_test = [] {
      const std::string s = "[\"a\xFF!b\"]";
      {
         std::vector<std::string_view> v;
         expect(glz::read_json(v, s) == glz::error_code::invalid_utf8);
      }
      {
         std::vector<std::u8string> v;
         expect(glz::read_json(v, s) == glz::error_code::invalid_utf8);
      }
      {
         glz::generic obj{};
         expect(glz::read_json(obj, s) == glz::error_code::invalid_utf8);
      }
   };

   // \uXXXX escapes are validated separately from raw byte validation; both must reject. A lone
   // surrogate escape is pure ASCII on the wire, so only the escape decoder can catch it.
   "unpaired surrogate escapes rejected"_test = [] {
      constexpr sv s = R"(["a\ud800b"])";
      {
         std::vector<std::string> v;
         expect(glz::read_json(v, s));
      }
      expect(bool(glz::validate_json(s)));
   };

   // Escaped high code points decode to multi-byte UTF-8 and must still be accepted.
   "valid surrogate pair escapes accepted"_test = [] {
      constexpr sv s = R"(["a😀b"])";
      std::vector<std::string> v;
      expect(!glz::read_json(v, s));
      expect(v.size() == 1u);
      expect(v[0] ==
             "a\xF0\x9F\x98\x80"
             "b");
   };
};

struct opts_validate_skipped : glz::opts
{
   bool error_on_unknown_keys = false;
   bool validate_skipped = true;
};

// RFC 8259 section 6: number = [ minus ] int [ frac ] [ exp ], where int = zero / (digit1-9 *DIGIT).
// The exponent may follow the integer part directly, so "0e4" is valid despite having no fractional
// part. The validating skip path used to reject a leading zero followed by anything but '.'.
suite number_grammar = [] {
   static constexpr sv valid[]{"0",     "-0",   "1",    "-1",    "123",  "0.0",    "-0.0",
                               "1.5",   "0.5",  "-0.5", "1e4",   "1E4",  "1e+4",   "1e-4",
                               "0e4",   "0E4",  "0e+4", "0e-4",  "-0e4", "-0E+4",  "0e0",
                               "-0e-0", "0e00", "1e00", "0.0e4", "10e4", "1.5e10", "123456789012345678901234567890"};

   static constexpr sv invalid[]{"00",  "01",  "00e4", "-00",  "-01",   "0x1",   "0X1",  ".5",  "5.",
                                 "0.",  "-.5", "+1",   "1e",   "1e+",   "1e-",   "0e",   "0E",  "0e+",
                                 "0e-", "-",   "1..2", "0..1", "1.2.3", "0e4e5", "0ee4", "1_0", "0b1"};

   // Every context that routes through the validating number scanner.
   auto check = [](const sv num, const bool is_valid) {
      const std::string scalar{num};
      const std::string array = "[" + scalar + "]";
      const std::string object = R"({"a":)" + scalar + "}";
      const std::string skipped = R"({"unknown":)" + scalar + R"(,"i":1})";

      expect(bool(glz::validate_json(scalar)) != is_valid) << "scalar " << scalar;
      expect(bool(glz::validate_json(array)) != is_valid) << "array " << array;
      expect(bool(glz::validate_json(object)) != is_valid) << "object " << object;

      // A skipped unknown value is scanned by the same routine when validate_skipped is on.
      int_object obj{};
      expect(bool(glz::read<opts_validate_skipped{}>(obj, skipped)) != is_valid) << "skipped " << skipped;

      // Validation must agree with the real number parser. The array form is used because a bare
      // scalar read stops at the first non-number byte and ignores the rest by default.
      std::vector<double> v{};
      expect(bool(glz::read_json(v, array)) != is_valid) << "parse " << array;
   };

   "valid numbers"_test = [check] {
      for (const auto num : valid) check(num, true);
   };

   "invalid numbers"_test = [check] {
      for (const auto num : invalid) check(num, false);
   };

   // The scanner is also used on buffers without a trailing null, where it must stay inside `end`.
   "exponent at end of non-null-terminated buffer"_test = [] {
      static constexpr char accepted[]{'[', '0', 'e', '4', ']'};
      expect(!glz::validate_json(sv{accepted, sizeof(accepted)}));

      static constexpr char truncated[]{'[', '0', 'e', ']'};
      expect(bool(glz::validate_json(sv{truncated, sizeof(truncated)})));
   };
};

suite json_conformance = [] {
   "error_on_unknown_keys = true"_test = [] {
      should_fail<glz::opts{}>();
      should_pass<glz::opts{}>();
   };

   "error_on_unknown_keys = false"_test = [] {
      should_fail<glz::opts{.error_on_unknown_keys = false}>();
      should_pass<glz::opts{.error_on_unknown_keys = false}>();
   };

   "validate_trailing_whitespace = true"_test = [] {
      should_fail<opts_validate_trailing_whitespace{}>();
      should_pass<opts_validate_trailing_whitespace{}>();
   };
};

int main() { return 0; }
