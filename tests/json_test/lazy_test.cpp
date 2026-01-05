// Glaze Library
// For the license information refer to glaze.hpp

#include "glaze/json.hpp"
#include "ut/ut.hpp"

using namespace ut;

suite lazy_json_tests = [] {
   "lazy_json_read_basic"_test = [] {
      std::string buffer = R"({"name":"John","age":30,"active":true,"balance":123.45})";
      auto result = glz::lazy_json(buffer);
      expect(result.has_value()) << "Failed to parse JSON";

      auto& doc = *result;
      expect(doc.is_object());
      expect(doc.root().size() == 4u);

      // Access values lazily
      expect(doc["name"].is_string());
      expect(doc["name"].get<std::string>().value() == "John");

      expect(doc["age"].is_number());
      expect(doc["age"].get<int64_t>().value() == 30);

      expect(doc["active"].is_boolean());
      expect(doc["active"].get<bool>().value() == true);

      expect(doc["balance"].is_number());
      expect(std::abs(doc["balance"].get<double>().value() - 123.45) < 0.001);
   };

   "lazy_json_read_array"_test = [] {
      std::string buffer = R"([1, 2, 3, "hello", true, null])";
      auto result = glz::lazy_json(buffer);
      expect(result.has_value());

      auto& doc = *result;
      expect(doc.is_array());
      expect(doc.root().size() == 6u);

      expect(doc[0].get<int64_t>().value() == 1);
      expect(doc[1].get<int64_t>().value() == 2);
      expect(doc[2].get<int64_t>().value() == 3);
      expect(doc[3].get<std::string>().value() == "hello");
      expect(doc[4].get<bool>().value() == true);
      expect(doc[5].is_null());
   };

   "lazy_json_nested"_test = [] {
      std::string buffer = R"({"person":{"name":"Alice","friends":["Bob","Charlie"]},"count":2})";
      auto result = glz::lazy_json(buffer);
      expect(result.has_value());

      auto& doc = *result;
      expect(doc["person"]["name"].get<std::string>().value() == "Alice");
      expect(doc["person"]["friends"][0].get<std::string>().value() == "Bob");
      expect(doc["person"]["friends"][1].get<std::string>().value() == "Charlie");
      expect(doc["count"].get<int64_t>().value() == 2);
   };

   "lazy_json_escaped_string"_test = [] {
      std::string buffer = R"({"message":"Hello\nWorld","path":"C:\\Users\\test"})";
      auto result = glz::lazy_json(buffer);
      expect(result.has_value());

      auto& doc = *result;
      expect(doc["message"].get<std::string>().value() == "Hello\nWorld");
      expect(doc["path"].get<std::string>().value() == "C:\\Users\\test");
   };

   "lazy_json_unicode_escape"_test = [] {
      std::string buffer = R"({"emoji":"\u0048\u0065\u006c\u006c\u006f"})";
      auto result = glz::lazy_json(buffer);
      expect(result.has_value());

      auto& doc = *result;
      expect(doc["emoji"].get<std::string>().value() == "Hello");
   };

   "lazy_json_write"_test = [] {
      std::string buffer = R"({"x":1,"y":2})";
      auto result = glz::lazy_json(buffer);
      expect(result.has_value());

      std::string output;
      auto ec = glz::write_json(result->root(), output);
      expect(ec == glz::error_code::none);
      expect(output == buffer) << output;
   };

   "lazy_json_contains"_test = [] {
      std::string buffer = R"({"a":1,"b":2})";
      auto result = glz::lazy_json(buffer);
      expect(result.has_value());

      auto& doc = *result;
      expect(doc.root().contains("a"));
      expect(doc.root().contains("b"));
      expect(!doc.root().contains("c"));
   };

   "lazy_json_empty_object"_test = [] {
      std::string buffer = R"({})";
      auto result = glz::lazy_json(buffer);
      expect(result.has_value());

      auto& doc = *result;
      expect(doc.is_object());
      expect(doc.root().empty());
      expect(doc.root().size() == 0u);
   };

   "lazy_json_empty_array"_test = [] {
      std::string buffer = R"([])";
      auto result = glz::lazy_json(buffer);
      expect(result.has_value());

      auto& doc = *result;
      expect(doc.is_array());
      expect(doc.root().empty());
      expect(doc.root().size() == 0u);
   };

   "lazy_json_null"_test = [] {
      std::string buffer = R"(null)";
      auto result = glz::lazy_json(buffer);
      expect(result.has_value());

      auto& doc = *result;
      expect(doc.is_null());
      expect(doc.root().empty());
   };

   "lazy_json_number_types"_test = [] {
      std::string buffer = R"({"int":42,"float":3.14,"negative":-100,"big":9007199254740993})";
      auto result = glz::lazy_json(buffer);
      expect(result.has_value());

      auto& doc = *result;
      expect(doc["int"].get<int32_t>().value() == 42);
      expect(doc["int"].get<uint32_t>().value() == 42u);
      expect(doc["int"].get<int64_t>().value() == 42);
      expect(doc["int"].get<double>().value() == 42.0);

      expect(std::abs(doc["float"].get<double>().value() - 3.14) < 0.001);
      expect(doc["negative"].get<int64_t>().value() == -100);

      // Big integers
      expect(doc["big"].get<int64_t>().value() == 9007199254740993);
   };

   "lazy_json_raw_string_view"_test = [] {
      std::string buffer = R"({"simple":"hello","escaped":"hello\\world"})";
      auto result = glz::lazy_json(buffer);
      expect(result.has_value());

      auto& doc = *result;

      // For simple strings without escapes, string_view works
      auto simple_sv = doc["simple"].get<std::string_view>();
      expect(simple_sv.has_value());
      expect(simple_sv.value() == "hello");

      // For escaped strings, string_view returns raw (with escapes)
      auto escaped_sv = doc["escaped"].get<std::string_view>();
      expect(escaped_sv.has_value());
      expect(escaped_sv.value() == "hello\\\\world"); // Raw contains double backslash

      // Use get<std::string>() for proper unescaping
      expect(doc["escaped"].get<std::string>().value() == "hello\\world");
   };

   "lazy_json_explicit_bool"_test = [] {
      std::string buffer = R"({"exists":true})";
      auto result = glz::lazy_json(buffer);
      expect(result.has_value());

      // explicit bool conversion - true if not null
      expect(static_cast<bool>(*result));
      expect(static_cast<bool>((*result)["exists"]));

      std::string null_buffer = R"(null)";
      auto null_result = glz::lazy_json(null_buffer);
      expect(null_result.has_value());
      expect(!static_cast<bool>(*null_result)); // null is false
   };

   "lazy_json_wrong_type_error"_test = [] {
      std::string buffer = R"({"str":"hello","num":42})";
      auto result = glz::lazy_json(buffer);
      expect(result.has_value());

      auto& doc = *result;

      // Trying to get string as number should fail
      auto num_result = doc["str"].get<int64_t>();
      expect(!num_result.has_value());
      expect(num_result.error().ec == glz::error_code::get_wrong_type);

      // Trying to get number as string should fail
      auto str_result = doc["num"].get<std::string>();
      expect(!str_result.has_value());
      expect(str_result.error().ec == glz::error_code::get_wrong_type);
   };

   "lazy_json_float"_test = [] {
      std::string buffer = R"({"value":2.5})";
      auto result = glz::lazy_json(buffer);
      expect(result.has_value());

      auto& doc = *result;
      auto float_result = doc["value"].get<float>();
      expect(float_result.has_value());
      expect(std::abs(float_result.value() - 2.5f) < 0.001f);
   };

   "lazy_json_large_array"_test = [] {
      std::string buffer = R"([0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19])";
      auto result = glz::lazy_json(buffer);
      expect(result.has_value());

      auto& doc = *result;
      expect(doc.root().size() == 20u);
      for (size_t i = 0; i < 20; ++i) {
         expect(doc[i].get<int64_t>().value() == static_cast<int64_t>(i));
      }
   };

   "lazy_json_struct_size"_test = [] {
      // Sizes differ between 32-bit and 64-bit systems
      if constexpr (sizeof(void*) == 8) {
         // lazy_json_view is 48 bytes: doc* (8) + data* (8) + parse_pos* (8) + key_ (16) + error (4) + padding (4)
         expect(sizeof(glz::lazy_json_view<glz::opts{}>) == 48u) << "lazy_json_view should be 48 bytes on 64-bit, got " << sizeof(glz::lazy_json_view<glz::opts{}>);
         expect(sizeof(glz::lazy_json_view<glz::opts{.null_terminated = false}>) == 48u) << "lazy_json_view should be 48 bytes on 64-bit, got " << sizeof(glz::lazy_json_view<glz::opts{.null_terminated = false}>);
      }
      else {
         // lazy_json_view is 24 bytes: doc* (4) + data* (4) + parse_pos* (4) + key_ (8) + error (4)
         expect(sizeof(glz::lazy_json_view<glz::opts{}>) == 24u) << "lazy_json_view should be 24 bytes on 32-bit, got " << sizeof(glz::lazy_json_view<glz::opts{}>);
         expect(sizeof(glz::lazy_json_view<glz::opts{.null_terminated = false}>) == 24u) << "lazy_json_view should be 24 bytes on 32-bit, got " << sizeof(glz::lazy_json_view<glz::opts{.null_terminated = false}>);
      }
   };

   "lazy_json_progressive_scanning"_test = [] {
      // Test that progressive scanning works for sequential key access
      std::string buffer = R"({"a":1,"b":2,"c":3,"d":4,"e":5})";
      auto result = glz::lazy_json(buffer);
      expect(result.has_value());

      auto& doc = *result;

      // Sequential access should use progressive scanning
      expect(doc["a"].get<int64_t>().value() == 1);
      expect(doc["b"].get<int64_t>().value() == 2);
      expect(doc["c"].get<int64_t>().value() == 3);
      expect(doc["d"].get<int64_t>().value() == 4);
      expect(doc["e"].get<int64_t>().value() == 5);

      // Accessing earlier key should still work (wrap-around)
      expect(doc["a"].get<int64_t>().value() == 1);
      expect(doc["c"].get<int64_t>().value() == 3);

      // Non-existent key should return error
      auto missing = doc["z"];
      expect(missing.has_error());
   };

   "lazy_json_reset_parse_pos"_test = [] {
      std::string buffer = R"({"x":10,"y":20,"z":30})";
      auto result = glz::lazy_json(buffer);
      expect(result.has_value());

      auto& doc = *result;

      // Access z first (advances parse_pos to end)
      expect(doc["z"].get<int64_t>().value() == 30);

      // Access x (should wrap around)
      expect(doc["x"].get<int64_t>().value() == 10);

      // Reset and access again
      doc.reset_parse_pos();
      expect(doc["y"].get<int64_t>().value() == 20);
   };

   "lazy_json_unicode_direct_utf8"_test = [] {
      // Direct UTF-8 encoded characters in JSON
      std::string buffer = R"({"greeting":"こんにちは","emoji":"🎉","mixed":"Hello 世界!"})";
      auto result = glz::lazy_json(buffer);
      expect(result.has_value());

      auto& doc = *result;
      expect(doc["greeting"].get<std::string>().value() == "こんにちは");
      expect(doc["emoji"].get<std::string>().value() == "🎉");
      expect(doc["mixed"].get<std::string>().value() == "Hello 世界!");

      // string_view returns raw UTF-8 bytes
      expect(doc["greeting"].get<std::string_view>().value() == "こんにちは");
   };

   "lazy_json_unicode_escape_sequences"_test = [] {
      // Unicode escape sequences
      std::string buffer = R"({"jp":"\u3053\u3093\u306B\u3061\u306F","euro":"\u20AC100"})";
      auto result = glz::lazy_json(buffer);
      expect(result.has_value());

      auto& doc = *result;
      expect(doc["jp"].get<std::string>().value() == "こんにちは");
      expect(doc["euro"].get<std::string>().value() == "€100");
   };

   "lazy_json_surrogate_pairs"_test = [] {
      // Surrogate pairs for characters outside BMP (emoji, etc.)
      // 😀 = U+1F600 = \uD83D\uDE00 (surrogate pair)
      // 🎉 = U+1F389 = \uD83C\uDF89
      // 𝄞 = U+1D11E = \uD834\uDD1E (musical G clef)
      std::string buffer = R"({"smile":"\uD83D\uDE00","party":"\uD83C\uDF89","clef":"\uD834\uDD1E"})";
      auto result = glz::lazy_json(buffer);
      expect(result.has_value());

      auto& doc = *result;
      expect(doc["smile"].get<std::string>().value() == "😀");
      expect(doc["party"].get<std::string>().value() == "🎉");
      expect(doc["clef"].get<std::string>().value() == "𝄞");
   };

   "lazy_json_all_escape_sequences"_test = [] {
      // All JSON escape sequences: \" \\ \/ \b \f \n \r \t
      std::string buffer = R"({"quote":"\"","backslash":"\\","slash":"\/","backspace":"\b","formfeed":"\f","newline":"\n","return":"\r","tab":"\t"})";
      auto result = glz::lazy_json(buffer);
      expect(result.has_value());

      auto& doc = *result;
      expect(doc["quote"].get<std::string>().value() == "\"");
      expect(doc["backslash"].get<std::string>().value() == "\\");
      expect(doc["slash"].get<std::string>().value() == "/");
      expect(doc["backspace"].get<std::string>().value() == "\b");
      expect(doc["formfeed"].get<std::string>().value() == "\f");
      expect(doc["newline"].get<std::string>().value() == "\n");
      expect(doc["return"].get<std::string>().value() == "\r");
      expect(doc["tab"].get<std::string>().value() == "\t");
   };

   "lazy_json_escaped_keys"_test = [] {
      // Keys with escape sequences - lazy_json matches raw JSON keys
      std::string buffer = R"({"key\nwith\nnewlines":"value1","key\twith\ttabs":"value2","key\"with\"quotes":"value3"})";
      auto result = glz::lazy_json(buffer);
      expect(result.has_value());

      auto& doc = *result;
      // lazy_json uses raw key matching - must provide the escaped form as it appears in JSON
#if GLZ_HAS_CONSTEXPR_STRING
      expect(doc[glz::escape_unicode<"key\nwith\nnewlines">].get<std::string>().value() == "value1");
      expect(doc[glz::escape_unicode<"key\twith\ttabs">].get<std::string>().value() == "value2");
      expect(doc[glz::escape_unicode<"key\"with\"quotes">].get<std::string>().value() == "value3");
#else
      expect(doc["key\\nwith\\nnewlines"].get<std::string>().value() == "value1");
      expect(doc["key\\twith\\ttabs"].get<std::string>().value() == "value2");
      expect(doc["key\\\"with\\\"quotes"].get<std::string>().value() == "value3");
#endif
   };

   "lazy_json_unicode_keys"_test = [] {
      // Unicode in keys (direct UTF-8)
      std::string buffer = R"({"日本語キー":"japanese","émoji🎉":"with_emoji","Ключ":"russian"})";
      auto result = glz::lazy_json(buffer);
      expect(result.has_value());

      auto& doc = *result;
      expect(doc["日本語キー"].get<std::string>().value() == "japanese");
      expect(doc["émoji🎉"].get<std::string>().value() == "with_emoji");
      expect(doc["Ключ"].get<std::string>().value() == "russian");
   };

   "lazy_json_unicode_escape_keys"_test = [] {
      // Unicode escape sequences in keys - lazy_json matches raw JSON keys
      // Note: \u0048\u0065\u006C\u006C\u006F spells "Hello" in unicode escapes
      // Note: \u20AC is the Euro sign €
      std::string buffer = R"({"\u0048\u0065\u006C\u006C\u006F":"world","\u20AC":"euro_sign"})";
      auto result = glz::lazy_json(buffer);
      expect(result.has_value());

      auto& doc = *result;
      // lazy_json uses raw key matching - must match the literal escape sequences as they appear in JSON
      // Note: escape_unicode only escapes non-ASCII, so "Hello" stays as-is but "€" becomes \u20AC
      expect(doc["\\u0048\\u0065\\u006C\\u006C\\u006F"].get<std::string>().value() == "world");
#if GLZ_HAS_CONSTEXPR_STRING
      expect(doc[glz::escape_unicode<"€">].get<std::string>().value() == "euro_sign");
#else
      expect(doc["\\u20AC"].get<std::string>().value() == "euro_sign");
#endif
   };

   "lazy_json_complex_escapes"_test = [] {
      // Complex combinations of escapes
      std::string buffer = R"({"path":"C:\\Users\\test\\file.txt","json":"{\"nested\":\"value\"}","multi":"line1\nline2\ttabbed"})";
      auto result = glz::lazy_json(buffer);
      expect(result.has_value());

      auto& doc = *result;
      expect(doc["path"].get<std::string>().value() == "C:\\Users\\test\\file.txt");
      expect(doc["json"].get<std::string>().value() == "{\"nested\":\"value\"}");
      expect(doc["multi"].get<std::string>().value() == "line1\nline2\ttabbed");
   };

   "lazy_json_nested_unicode"_test = [] {
      // Unicode in nested structures
      std::string buffer = R"({"user":{"名前":"田中","city":"東京"},"tags":["日本","🇯🇵","test"]})";
      auto result = glz::lazy_json(buffer);
      expect(result.has_value());

      auto& doc = *result;
      expect(doc["user"]["名前"].get<std::string>().value() == "田中");
      expect(doc["user"]["city"].get<std::string>().value() == "東京");
      expect(doc["tags"][0].get<std::string>().value() == "日本");
      expect(doc["tags"][1].get<std::string>().value() == "🇯🇵");
      expect(doc["tags"][2].get<std::string>().value() == "test");
   };
};

int main() { return 0; }
