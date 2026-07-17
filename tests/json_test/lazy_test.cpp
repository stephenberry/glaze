// Glaze Library
// For the license information refer to glaze.hpp

#include "glaze/json.hpp"
#include "ut/ut.hpp"

using namespace ut;

struct User
{
   std::string name{};
   int age{};
   bool active{};
};

struct Address
{
   std::string city{};
   std::string country{};
};

struct Person
{
   std::string name{};
   Address address{};
};

struct Item
{
   int id{};
   std::string value{};
};

struct MinimalUser
{
   std::string name{};
};

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
         expect(sizeof(glz::lazy_json_view<glz::opts{}>) == 48u)
            << "lazy_json_view should be 48 bytes on 64-bit, got " << sizeof(glz::lazy_json_view<glz::opts{}>);
         expect(sizeof(glz::lazy_json_view<glz::opts{.null_terminated = false}>) == 48u)
            << "lazy_json_view should be 48 bytes on 64-bit, got "
            << sizeof(glz::lazy_json_view<glz::opts{.null_terminated = false}>);
      }
      else {
         // lazy_json_view is 24 bytes: doc* (4) + data* (4) + parse_pos* (4) + key_ (8) + error (4)
         expect(sizeof(glz::lazy_json_view<glz::opts{}>) == 24u)
            << "lazy_json_view should be 24 bytes on 32-bit, got " << sizeof(glz::lazy_json_view<glz::opts{}>);
         expect(sizeof(glz::lazy_json_view<glz::opts{.null_terminated = false}>) == 24u)
            << "lazy_json_view should be 24 bytes on 32-bit, got "
            << sizeof(glz::lazy_json_view<glz::opts{.null_terminated = false}>);
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
      std::string buffer =
         R"({"quote":"\"","backslash":"\\","slash":"\/","backspace":"\b","formfeed":"\f","newline":"\n","return":"\r","tab":"\t"})";
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
      std::string buffer =
         R"({"key\nwith\nnewlines":"value1","key\twith\ttabs":"value2","key\"with\"quotes":"value3"})";
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
      std::string buffer =
         R"({"path":"C:\\Users\\test\\file.txt","json":"{\"nested\":\"value\"}","multi":"line1\nline2\ttabbed"})";
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

   // ============================================================================
   // indexed_lazy_view tests
   // ============================================================================

   "indexed_lazy_view_array_basic"_test = [] {
      std::string buffer = R"([1, 2, 3, 4, 5])";
      auto result = glz::lazy_json(buffer);
      expect(result.has_value());

      auto indexed = result->root().index();
      expect(indexed.is_array());
      expect(!indexed.is_object());
      expect(indexed.size() == 5u);
      expect(!indexed.empty());

      // O(1) random access
      expect(indexed[0].get<int64_t>().value() == 1);
      expect(indexed[2].get<int64_t>().value() == 3);
      expect(indexed[4].get<int64_t>().value() == 5);

      // Out of bounds
      expect(indexed[10].has_error());
   };

   "indexed_lazy_view_array_iteration"_test = [] {
      std::string buffer = R"([10, 20, 30, 40, 50])";
      auto result = glz::lazy_json(buffer);
      expect(result.has_value());

      auto indexed = result->root().index();
      int64_t sum = 0;
      size_t count = 0;
      for (auto& item : indexed) {
         auto val = item.get<int64_t>();
         if (val) sum += *val;
         ++count;
      }
      expect(sum == 150);
      expect(count == 5u);
   };

   "indexed_lazy_view_object_basic"_test = [] {
      std::string buffer = R"({"a":1,"b":2,"c":3})";
      auto result = glz::lazy_json(buffer);
      expect(result.has_value());

      auto indexed = result->root().index();
      expect(indexed.is_object());
      expect(!indexed.is_array());
      expect(indexed.size() == 3u);
      expect(!indexed.empty());

      // Random access by index
      expect(indexed[0].get<int64_t>().value() == 1);
      expect(indexed[0].key() == "a");
      expect(indexed[1].get<int64_t>().value() == 2);
      expect(indexed[1].key() == "b");
      expect(indexed[2].get<int64_t>().value() == 3);
      expect(indexed[2].key() == "c");

      // Key lookup (O(n) linear search)
      expect(indexed["a"].get<int64_t>().value() == 1);
      expect(indexed["b"].get<int64_t>().value() == 2);
      expect(indexed["c"].get<int64_t>().value() == 3);
      expect(indexed["missing"].has_error());

      // Contains
      expect(indexed.contains("a"));
      expect(indexed.contains("b"));
      expect(!indexed.contains("missing"));
   };

   "indexed_lazy_view_object_iteration"_test = [] {
      std::string buffer = R"({"x":10,"y":20,"z":30})";
      auto result = glz::lazy_json(buffer);
      expect(result.has_value());

      auto indexed = result->root().index();
      std::vector<std::pair<std::string_view, int64_t>> items;
      for (auto& item : indexed) {
         auto val = item.get<int64_t>();
         if (val) {
            items.emplace_back(item.key(), *val);
         }
      }

      expect(items.size() == 3u);
      expect(items[0].first == "x");
      expect(items[0].second == 10);
      expect(items[1].first == "y");
      expect(items[1].second == 20);
      expect(items[2].first == "z");
      expect(items[2].second == 30);
   };

   "indexed_lazy_view_nested_lazy"_test = [] {
      // Elements in indexed view should still be lazy for nested access
      std::string buffer = R"([{"id":1,"data":{"value":100}},{"id":2,"data":{"value":200}}])";
      auto result = glz::lazy_json(buffer);
      expect(result.has_value());

      auto indexed = result->root().index();
      expect(indexed.size() == 2u);

      // Nested access is still lazy
      expect(indexed[0]["id"].get<int64_t>().value() == 1);
      expect(indexed[0]["data"]["value"].get<int64_t>().value() == 100);
      expect(indexed[1]["id"].get<int64_t>().value() == 2);
      expect(indexed[1]["data"]["value"].get<int64_t>().value() == 200);
   };

   "indexed_lazy_view_empty"_test = [] {
      std::string buffer1 = R"([])";
      auto result1 = glz::lazy_json(buffer1);
      expect(result1.has_value());
      auto indexed1 = result1->root().index();
      expect(indexed1.empty());
      expect(indexed1.size() == 0u);

      std::string buffer2 = R"({})";
      auto result2 = glz::lazy_json(buffer2);
      expect(result2.has_value());
      auto indexed2 = result2->root().index();
      expect(indexed2.empty());
      expect(indexed2.size() == 0u);
   };

   "indexed_lazy_view_random_access_iterator"_test = [] {
      std::string buffer = R"([0, 1, 2, 3, 4, 5, 6, 7, 8, 9])";
      auto result = glz::lazy_json(buffer);
      expect(result.has_value());

      auto indexed = result->root().index();
      auto it = indexed.begin();

      // Test iterator arithmetic
      expect((*it).get<int64_t>().value() == 0);
      expect((*(it + 3)).get<int64_t>().value() == 3);
      expect((*(it + 9)).get<int64_t>().value() == 9);

      // Test operator[]
      expect(it[5].get<int64_t>().value() == 5);

      // Test distance
      auto end = indexed.end();
      expect(end - it == 10);

      // Test increment/decrement
      ++it;
      expect((*it).get<int64_t>().value() == 1);
      --it;
      expect((*it).get<int64_t>().value() == 0);

      // Test +=/-=
      it += 5;
      expect((*it).get<int64_t>().value() == 5);
      it -= 2;
      expect((*it).get<int64_t>().value() == 3);
   };

   "indexed_lazy_view_large_array"_test = [] {
      // Build a large array
      std::string buffer = "[";
      for (int i = 0; i < 1000; ++i) {
         if (i > 0) buffer += ",";
         buffer += std::to_string(i);
      }
      buffer += "]";

      auto result = glz::lazy_json(buffer);
      expect(result.has_value());

      auto indexed = result->root().index();
      expect(indexed.size() == 1000u);

      // O(1) random access to any element
      expect(indexed[0].get<int64_t>().value() == 0);
      expect(indexed[500].get<int64_t>().value() == 500);
      expect(indexed[999].get<int64_t>().value() == 999);

      // Iteration and sum
      int64_t sum = 0;
      for (auto& item : indexed) {
         auto val = item.get<int64_t>();
         if (val) sum += *val;
      }
      expect(sum == 499500); // Sum of 0..999
   };

   // ============================================================================
   // minified option tests
   // ============================================================================

   "lazy_json_minified_basic"_test = [] {
      // Minified JSON (no whitespace)
      std::string buffer = R"({"name":"John","age":30,"active":true})";
      constexpr auto opts = glz::opts{.minified = true};
      auto result = glz::lazy_json<opts>(buffer);
      expect(result.has_value());

      auto& doc = *result;
      expect(doc["name"].get<std::string>().value() == "John");
      expect(doc["age"].get<int64_t>().value() == 30);
      expect(doc["active"].get<bool>().value() == true);
   };

   "lazy_json_minified_nested"_test = [] {
      std::string buffer = R"({"user":{"profile":{"email":"test@example.com"}}})";
      constexpr auto opts = glz::opts{.minified = true};
      auto result = glz::lazy_json<opts>(buffer);
      expect(result.has_value());

      auto& doc = *result;
      expect(doc["user"]["profile"]["email"].get<std::string>().value() == "test@example.com");
   };

   "lazy_json_minified_array_iteration"_test = [] {
      std::string buffer = R"({"items":[{"id":1},{"id":2},{"id":3}]})";
      constexpr auto opts = glz::opts{.minified = true};
      auto result = glz::lazy_json<opts>(buffer);
      expect(result.has_value());

      auto& doc = *result;
      int64_t sum = 0;
      for (auto& item : doc["items"]) {
         auto id = item["id"].get<int64_t>();
         if (id) sum += *id;
      }
      expect(sum == 6);
   };

   "lazy_json_minified_indexed"_test = [] {
      std::string buffer = R"([0,1,2,3,4,5,6,7,8,9])";
      constexpr auto opts = glz::opts{.minified = true};
      auto result = glz::lazy_json<opts>(buffer);
      expect(result.has_value());

      auto indexed = result->root().index();
      expect(indexed.size() == 10u);
      expect(indexed[0].get<int64_t>().value() == 0);
      expect(indexed[5].get<int64_t>().value() == 5);
      expect(indexed[9].get<int64_t>().value() == 9);
   };

   // ============================================================================
   // raw_json() and struct deserialization tests
   // ============================================================================

   "lazy_json_raw_json_basic"_test = [] {
      std::string buffer = R"({"name":"Alice","age":30})";
      auto result = glz::lazy_json(buffer);
      expect(result.has_value());

      // raw_json() returns the entire JSON for a value
      auto raw = result->root().raw_json();
      expect(raw == R"({"name":"Alice","age":30})");

      // Can also get raw JSON for nested values
      auto name_raw = (*result)["name"].raw_json();
      expect(name_raw == R"("Alice")");

      auto age_raw = (*result)["age"].raw_json();
      expect(age_raw == "30");
   };

   "lazy_json_raw_json_nested"_test = [] {
      std::string buffer = R"({"user":{"profile":{"name":"Bob","email":"bob@test.com"}},"count":5})";
      auto result = glz::lazy_json(buffer);
      expect(result.has_value());

      // Get raw JSON for nested object
      auto user_raw = (*result)["user"].raw_json();
      expect(user_raw == R"({"profile":{"name":"Bob","email":"bob@test.com"}})");

      auto profile_raw = (*result)["user"]["profile"].raw_json();
      expect(profile_raw == R"({"name":"Bob","email":"bob@test.com"})");
   };

   "lazy_json_raw_json_array"_test = [] {
      std::string buffer = R"({"items":[1,2,3],"name":"test"})";
      auto result = glz::lazy_json(buffer);
      expect(result.has_value());

      auto items_raw = (*result)["items"].raw_json();
      expect(items_raw == "[1,2,3]");

      auto first_raw = (*result)["items"][0].raw_json();
      expect(first_raw == "1");
   };

   "lazy_json_deserialize_struct"_test = [] {
      std::string buffer = R"({
         "user": {"name": "Alice", "age": 30, "active": true},
         "metadata": {"version": 1}
      })";

      auto result = glz::lazy_json(buffer);
      expect(result.has_value());

      // Navigate lazily to "user", then deserialize into struct
      auto user_view = (*result)["user"];
      expect(!user_view.has_error());

      // Get raw JSON and deserialize
      auto user_json = user_view.raw_json();
      User user{};
      auto ec = glz::read_json(user, user_json);

      expect(ec == glz::error_code::none) << glz::format_error(ec, user_json);
      expect(user.name == "Alice");
      expect(user.age == 30);
      expect(user.active == true);
   };

   "lazy_json_deserialize_nested_struct"_test = [] {
      std::string buffer = R"({
         "people": [
            {"name": "Alice", "address": {"city": "New York", "country": "USA"}},
            {"name": "Bob", "address": {"city": "London", "country": "UK"}}
         ],
         "count": 2
      })";

      auto result = glz::lazy_json(buffer);
      expect(result.has_value());

      // Deserialize first person
      auto first_person_json = (*result)["people"][0].raw_json();
      Person alice{};
      auto ec1 = glz::read_json(alice, first_person_json);
      expect(ec1 == glz::error_code::none);
      expect(alice.name == "Alice");
      expect(alice.address.city == "New York");
      expect(alice.address.country == "USA");

      // Deserialize second person
      auto second_person_json = (*result)["people"][1].raw_json();
      Person bob{};
      auto ec2 = glz::read_json(bob, second_person_json);
      expect(ec2 == glz::error_code::none);
      expect(bob.name == "Bob");
      expect(bob.address.city == "London");
      expect(bob.address.country == "UK");
   };

   "lazy_json_deserialize_array_element"_test = [] {
      std::string buffer = R"({"items":[{"id":1,"value":"one"},{"id":2,"value":"two"},{"id":3,"value":"three"}]})";

      auto result = glz::lazy_json(buffer);
      expect(result.has_value());

      // Use indexed view for efficient random access, then deserialize specific elements
      auto items = (*result)["items"].index();
      expect(items.size() == 3u);

      // Deserialize middle element
      Item middle{};
      auto ec = glz::read_json(middle, items[1].raw_json());
      expect(ec == glz::error_code::none);
      expect(middle.id == 2);
      expect(middle.value == "two");
   };

   // ============================================================================
   // read_into<T>() tests - efficient single-pass deserialization
   // ============================================================================

   "lazy_json_read_into_basic"_test = [] {
      std::string buffer = R"({
         "user": {"name": "Alice", "age": 30, "active": true},
         "metadata": {"version": 1}
      })";

      auto result = glz::lazy_json(buffer);
      expect(result.has_value());

      // Navigate lazily to "user", then deserialize directly (single-pass)
      User user{};
      auto ec = (*result)["user"].read_into(user);

      expect(!ec) << glz::format_error(ec, buffer);
      expect(user.name == "Alice");
      expect(user.age == 30);
      expect(user.active == true);
   };

   "lazy_json_read_into_propagates_opts"_test = [] {
      std::string buffer = R"({
         "user": {"name": "Alice", "ignored": 42}
      })";

      auto result = glz::lazy_json<glz::opts{.error_on_unknown_keys = false}>(buffer);
      expect(result.has_value());

      MinimalUser user{};
      auto ec = (*result)["user"].read_into(user);

      expect(!ec) << glz::format_error(ec, buffer);
      expect(user.name == "Alice");
   };

   "lazy_json_read_into_partial_read_clears_error"_test = [] {
      std::string buffer = R"({
         "user": {"name": "Alice", "ignored": 42}
      })";

      auto result = glz::lazy_json<glz::opts{.partial_read = true}>(buffer);
      expect(result.has_value());

      MinimalUser user{};
      auto ec = (*result)["user"].read_into(user);

      expect(!ec) << glz::format_error(ec, buffer);
      expect(ec == glz::error_code::none);
      expect(user.name == "Alice");
   };

   "lazy_json_read_into_nested"_test = [] {
      std::string buffer = R"({
         "people": [
            {"name": "Alice", "address": {"city": "New York", "country": "USA"}},
            {"name": "Bob", "address": {"city": "London", "country": "UK"}}
         ]
      })";

      auto result = glz::lazy_json(buffer);
      expect(result.has_value());

      // Deserialize directly using read_into (more efficient than raw_json + read_json)
      Person alice{};
      auto ec1 = (*result)["people"][0].read_into(alice);
      expect(!ec1);
      expect(alice.name == "Alice");
      expect(alice.address.city == "New York");
      expect(alice.address.country == "USA");

      Person bob{};
      auto ec2 = (*result)["people"][1].read_into(bob);
      expect(!ec2);
      expect(bob.name == "Bob");
      expect(bob.address.city == "London");
      expect(bob.address.country == "UK");
   };

   "lazy_json_read_into_indexed"_test = [] {
      std::string buffer = R"({"items":[{"id":1,"value":"one"},{"id":2,"value":"two"},{"id":3,"value":"three"}]})";

      auto result = glz::lazy_json(buffer);
      expect(result.has_value());

      // Use indexed view for O(1) random access, then read_into for single-pass deserialize
      auto items = (*result)["items"].index();
      expect(items.size() == 3u);

      // Deserialize last element directly
      Item last{};
      auto ec = items[2].read_into(last);
      expect(!ec);
      expect(last.id == 3);
      expect(last.value == "three");
   };

   "lazy_json_read_into_primitive"_test = [] {
      std::string buffer = R"({"count":42,"ratio":3.14,"name":"test","active":true})";

      auto result = glz::lazy_json(buffer);
      expect(result.has_value());

      // read_into works with primitive types too
      int count{};
      auto ec1 = (*result)["count"].read_into(count);
      expect(!ec1);
      expect(count == 42);

      double ratio{};
      auto ec2 = (*result)["ratio"].read_into(ratio);
      expect(!ec2);
      expect(std::abs(ratio - 3.14) < 0.001);

      std::string name{};
      auto ec3 = (*result)["name"].read_into(name);
      expect(!ec3);
      expect(name == "test");

      bool active{};
      auto ec4 = (*result)["active"].read_into(active);
      expect(!ec4);
      expect(active == true);
   };

   "lazy_json_read_into_array"_test = [] {
      std::string buffer = R"({"data":[1,2,3,4,5]})";

      auto result = glz::lazy_json(buffer);
      expect(result.has_value());

      // read_into can deserialize arrays
      std::vector<int> data{};
      auto ec = (*result)["data"].read_into(data);
      expect(!ec);
      expect(data.size() == 5u);
      expect(data[0] == 1);
      expect(data[4] == 5);
   };

   "lazy_json_read_into_error_handling"_test = [] {
      std::string buffer = R"({"name":"test","age":30})";

      auto result = glz::lazy_json(buffer);
      expect(result.has_value());

      // read_into on missing key should return error
      User user{};
      auto missing_view = (*result)["missing"];
      expect(missing_view.has_error());

      auto ec = missing_view.read_into(user);
      expect(bool(ec)); // Should have error
   };

   "lazy_json_read_into_vs_raw_json"_test = [] {
      // Demonstrates that read_into produces same results as raw_json + read_json
      std::string buffer = R"({"user":{"name":"Test","age":25,"active":false}})";

      auto result = glz::lazy_json(buffer);
      expect(result.has_value());

      // Method 1: raw_json() + read_json() - two passes over data
      User user1{};
      auto raw = (*result)["user"].raw_json();
      auto ec1 = glz::read_json(user1, raw);
      expect(!ec1);

      // Method 2: read_into() - single pass (more efficient)
      User user2{};
      auto ec2 = (*result)["user"].read_into(user2);
      expect(!ec2);

      // Both methods produce identical results
      expect(user1.name == user2.name);
      expect(user1.age == user2.age);
      expect(user1.active == user2.active);
   };

   // ============================================================================
   // glz::read_json(value, lazy_json_view) overload tests
   // ============================================================================

   "lazy_json_read_json_overload_basic"_test = [] {
      std::string buffer = R"({
         "user": {"name": "Alice", "age": 30, "active": true},
         "metadata": {"version": 1}
      })";

      auto result = glz::lazy_json(buffer);
      expect(result.has_value());

      // Use glz::read_json directly with lazy_json_view
      User user{};
      auto ec = glz::read_json(user, (*result)["user"]);

      expect(!ec) << glz::format_error(ec, buffer);
      expect(user.name == "Alice");
      expect(user.age == 30);
      expect(user.active == true);
   };

   "lazy_json_read_json_overload_nested"_test = [] {
      std::string buffer = R"({
         "people": [
            {"name": "Alice", "address": {"city": "New York", "country": "USA"}},
            {"name": "Bob", "address": {"city": "London", "country": "UK"}}
         ]
      })";

      auto result = glz::lazy_json(buffer);
      expect(result.has_value());

      // glz::read_json works with nested lazy views
      Person alice{};
      auto ec1 = glz::read_json(alice, (*result)["people"][0]);
      expect(!ec1);
      expect(alice.name == "Alice");
      expect(alice.address.city == "New York");

      Person bob{};
      auto ec2 = glz::read_json(bob, (*result)["people"][1]);
      expect(!ec2);
      expect(bob.name == "Bob");
      expect(bob.address.city == "London");
   };

   "lazy_json_read_json_overload_indexed"_test = [] {
      std::string buffer = R"({"items":[{"id":1,"value":"one"},{"id":2,"value":"two"},{"id":3,"value":"three"}]})";

      auto result = glz::lazy_json(buffer);
      expect(result.has_value());

      auto items = (*result)["items"].index();

      // glz::read_json works with indexed views
      Item item{};
      auto ec = glz::read_json(item, items[1]);
      expect(!ec);
      expect(item.id == 2);
      expect(item.value == "two");
   };

   "lazy_json_read_json_overload_primitives"_test = [] {
      std::string buffer = R"({"count":42,"ratio":3.14,"name":"test","active":true})";

      auto result = glz::lazy_json(buffer);
      expect(result.has_value());

      // glz::read_json works with primitive types
      int count{};
      auto ec1 = glz::read_json(count, (*result)["count"]);
      expect(!ec1);
      expect(count == 42);

      double ratio{};
      auto ec2 = glz::read_json(ratio, (*result)["ratio"]);
      expect(!ec2);
      expect(std::abs(ratio - 3.14) < 0.001);

      std::string name{};
      auto ec3 = glz::read_json(name, (*result)["name"]);
      expect(!ec3);
      expect(name == "test");

      bool active{};
      auto ec4 = glz::read_json(active, (*result)["active"]);
      expect(!ec4);
      expect(active == true);
   };

   "lazy_json_read_json_overload_vector"_test = [] {
      std::string buffer = R"({"data":[1,2,3,4,5]})";

      auto result = glz::lazy_json(buffer);
      expect(result.has_value());

      // glz::read_json works with containers
      std::vector<int> data{};
      auto ec = glz::read_json(data, (*result)["data"]);
      expect(!ec);
      expect(data.size() == 5u);
      expect(data[0] == 1);
      expect(data[4] == 5);
   };

   "lazy_json_read_json_overload_error"_test = [] {
      std::string buffer = R"({"name":"test"})";

      auto result = glz::lazy_json(buffer);
      expect(result.has_value());

      // glz::read_json on error view returns error
      User user{};
      auto missing_view = (*result)["missing"];
      expect(missing_view.has_error());

      auto ec = glz::read_json(user, missing_view);
      expect(bool(ec)); // Should have error
   };
};

// ============================================================================
// Dynamic key serialization using glz::custom with string_view and lazy_json
// Issue #2230: Using member value as JSON key name
// ============================================================================

namespace dynamic_key_test
{
   struct TaskA
   {
      std::string name = "task_a";
      int val{};
   };

   struct TaskB
   {
      std::string name = "task_b";
      int val{};
   };

   struct TaskC
   {
      std::string name = "task_c";
      int val{};
   };

   struct TopLevel
   {
      TaskA a;
      TaskB b;
      TaskC c;
   };
}

template <>
struct glz::meta<dynamic_key_test::TopLevel>
{
   using T = dynamic_key_test::TopLevel;

   // Helper struct for the value portion (excludes name since it becomes the key)
   struct TaskVal
   {
      int val;
   };

   // Write using string_view keys to avoid intermediate string allocations
   // The string_views point to the struct's own name members, which outlive serialization
   static constexpr auto write_custom = [](const T& self) {
      return std::vector<std::pair<std::string_view, TaskVal>>{
         {self.a.name, {self.a.val}}, {self.b.name, {self.b.val}}, {self.c.name, {self.c.val}}};
   };

   // Read using raw_json_view (no allocation) + lazy_json for efficient iteration
   static constexpr auto read_custom = [](T& self, const glz::raw_json_view& raw) {
      auto result = glz::lazy_json(raw.str);
      if (result) {
         for (auto item : result->root()) {
            auto key = item.key(); // std::string_view - no allocation
            auto val = item["val"].get<int>();
            if (val) {
               if (key == self.a.name)
                  self.a.val = *val;
               else if (key == self.b.name)
                  self.b.val = *val;
               else if (key == self.c.name)
                  self.c.val = *val;
            }
         }
      }
   };

   static constexpr auto value = glz::custom<read_custom, write_custom>;
};

suite dynamic_key_custom_tests = [] {
   using namespace dynamic_key_test;

   "dynamic_key_write"_test = [] {
      TopLevel top;
      top.a.val = 10;
      top.b.val = 20;
      top.c.val = 30;

      std::string json{};
      auto ec = glz::write_json(top, json);
      expect(not ec) << glz::format_error(ec, json);

      // Verify the output uses the name field values as keys
      expect(json == R"({"task_a":{"val":10},"task_b":{"val":20},"task_c":{"val":30}})") << json;
   };

   "dynamic_key_read"_test = [] {
      TopLevel top;
      std::string json = R"({"task_a":{"val":100},"task_b":{"val":200},"task_c":{"val":300}})";

      auto ec = glz::read_json(top, json);
      expect(not ec) << glz::format_error(ec, json);

      expect(top.a.val == 100);
      expect(top.b.val == 200);
      expect(top.c.val == 300);
   };

   "dynamic_key_roundtrip"_test = [] {
      TopLevel original;
      original.a.val = 42;
      original.b.val = 84;
      original.c.val = 126;

      // Write
      std::string json{};
      auto write_ec = glz::write_json(original, json);
      expect(not write_ec) << glz::format_error(write_ec, json);

      // Read into new object
      TopLevel parsed;
      auto read_ec = glz::read_json(parsed, json);
      expect(not read_ec) << glz::format_error(read_ec, json);

      // Verify values match
      expect(parsed.a.val == original.a.val);
      expect(parsed.b.val == original.b.val);
      expect(parsed.c.val == original.c.val);
   };

   "dynamic_key_partial_read"_test = [] {
      TopLevel top;
      top.a.val = 1;
      top.b.val = 2;
      top.c.val = 3;

      // JSON with only some keys present
      std::string json = R"({"task_b":{"val":999}})";

      auto ec = glz::read_json(top, json);
      expect(not ec) << glz::format_error(ec, json);

      // Only task_b should be updated
      expect(top.a.val == 1); // unchanged
      expect(top.b.val == 999); // updated
      expect(top.c.val == 3); // unchanged
   };

   "dynamic_key_different_order"_test = [] {
      TopLevel top;

      // JSON with keys in different order than struct definition
      std::string json = R"({"task_c":{"val":3},"task_a":{"val":1},"task_b":{"val":2}})";

      auto ec = glz::read_json(top, json);
      expect(not ec) << glz::format_error(ec, json);

      expect(top.a.val == 1);
      expect(top.b.val == 2);
      expect(top.c.val == 3);
   };

   // Truncated, non-null-terminated input must not read past the buffer. Each buffer below is
   // sized exactly to its bytes (a std::vector<char>, no trailing '\0'), so any read at
   // data() + size() lands in unallocated memory and aborts under the sanitizer CI builds. The
   // lazy scanner previously walked one byte past end whenever a value/element/key began at
   // end-of-input (a value missing after ':', a truncated literal, an unclosed key, or a
   // trailing comma), and it also handed back views positioned at end.
   "lazy_json_truncated_missing_value"_test = [] {
      const std::vector<char> buf{'{', '"', 'a', '"', ':'};
      const std::string_view sv{buf.data(), buf.size()};
      auto result = glz::lazy_json<glz::opts{.null_terminated = false}>(sv);
      expect(result.has_value());
      auto& doc = *result;
      expect(doc["x"].has_error()); // non-matching key drives skip_value_lazy at end
      expect(doc["a"].has_error()); // matching key whose value is truncated
   };

   "lazy_json_truncated_literal"_test = [] {
      const std::vector<char> buf{'{', '"', 'a', '"', ':', 't'};
      const std::string_view sv{buf.data(), buf.size()};
      auto result = glz::lazy_json<glz::opts{.null_terminated = false}>(sv);
      expect(result.has_value());
      expect((*result)["x"].has_error()); // skip over a truncated 'true' literal

      const std::vector<char> lit{'t'};
      const std::string_view lit_sv{lit.data(), lit.size()};
      auto lit_doc = glz::lazy_json<glz::opts{.null_terminated = false}>(lit_sv);
      expect(lit_doc.has_value());
      expect((*lit_doc).root().raw_json().size() == lit.size()); // span clamped to the buffer
   };

   "lazy_json_truncated_unclosed_key"_test = [] {
      const std::vector<char> buf{'{', '"', 'a'};
      const std::string_view sv{buf.data(), buf.size()};
      auto result = glz::lazy_json<glz::opts{.null_terminated = false}>(sv);
      expect(result.has_value());
      auto& doc = *result;
      expect(doc["a"].has_error());
      (void)doc.root().index(); // index() scans the key at end
      (void)doc.root().size();
   };

   "lazy_json_truncated_trailing_comma"_test = [] {
      const std::vector<char> buf{'{', '"', 'a', '"', ':', '1', ','};
      const std::string_view sv{buf.data(), buf.size()};
      auto result = glz::lazy_json<glz::opts{.null_terminated = false}>(sv);
      expect(result.has_value());
      auto& doc = *result;
      expect(doc["z"].has_error());
      (void)doc.root().index(); // the next key parse begins at end after the comma
      size_t count = 0;
      for (auto element : doc.root()) {
         (void)element;
         ++count;
      }
      expect(count == 1u); // only the "a" element is complete
   };

   "lazy_json_truncated_iterate"_test = [] {
      const std::vector<char> buf{'{', '"', 'a', '"', ':'};
      const std::string_view sv{buf.data(), buf.size()};
      auto result = glz::lazy_json<glz::opts{.null_terminated = false}>(sv);
      expect(result.has_value());
      size_t count = 0;
      for (auto element : (*result).root()) {
         (void)element;
         ++count;
      }
      expect(count == 0u); // the truncated element is not yielded
   };

   // size(), index().size(), and iteration must agree on a truncated object. size() used to
   // count the incomplete "a": pair (returning 1) while index() and iteration reported 0.
   "lazy_json_truncated_size_consistency"_test = [] {
      const std::vector<char> buf{'{', '"', 'a', '"', ':'};
      const std::string_view sv{buf.data(), buf.size()};
      auto result = glz::lazy_json<glz::opts{.null_terminated = false}>(sv);
      expect(result.has_value());
      auto& root = (*result).root();
      const size_t by_size = root.size();
      const size_t by_index = root.index().size();
      size_t by_iter = 0;
      for (auto element : root) {
         (void)element;
         ++by_iter;
      }
      expect(by_size == 0u);
      expect(by_index == 0u);
      expect(by_iter == 0u);
      expect(by_size == by_index);
      expect(by_size == by_iter);
   };

   // Iterating a malformed null-terminated document (an unclosed container, valid to feed with
   // the default null_terminated option since std::string supplies the '\0') must terminate via
   // json_end_ rather than spinning forever on the end position.
   "lazy_json_unterminated_iterate_null_terminated"_test = [] {
      const std::string json = R"({"a":1)"; // no closing brace
      auto result = glz::lazy_json<glz::opts{}>(json);
      expect(result.has_value());
      size_t count = 0;
      for (auto element : (*result).root()) {
         (void)element;
         if (++count > 1000) break; // guard: a regression here would loop forever
      }
      expect(count == 1u); // only the complete "a" element is yielded, then iteration stops
   };

   // null_terminated only promises a '\0' sentinel exists somewhere the parser may rely on, not
   // that it sits at json_end_. Here the message is the first 7 bytes ("{\"a\":1,"), but the
   // backing storage continues with more JSON and the '\0' is far past json_end_. Iteration must
   // stop at json_end_ (yielding only the complete "a" element), not run on into the trailing
   // bytes toward the distant '\0'.
   "lazy_json_null_terminated_end_before_sentinel"_test = [] {
      const char storage[] = R"({"a":1,"b":2})"; // '\0' at storage[13], well past the view below
      const std::string_view sv{storage, 7}; // "{\"a\":1,"
      auto result = glz::lazy_json<glz::opts{}>(sv); // null_terminated = true (default)
      expect(result.has_value());
      size_t count = 0;
      std::string_view last_key{};
      for (auto element : (*result).root()) {
         last_key = element.key();
         if (++count > 10) break; // guard: a '\0'-based stop would iterate into "b" and beyond
      }
      expect(count == 1u); // "b" lies past json_end_, so it must not be yielded
      expect(last_key == "a");
   };

   // A successful lookup primes parse_pos_, so a following lookup runs the progressive-scan branch.
   // If that second lookup matches a key whose value is truncated at end-of-input, it must report
   // the error and still leave the view usable: a later lookup of an earlier key must recover.
   "lazy_json_truncated_match_recovers_after_progressive_scan"_test = [] {
      // "a" has a complete value; "b" is matched but its value begins exactly at end-of-input.
      const std::vector<char> buf{'{', '"', 'a', '"', ':', '1', ',', '"', 'b', '"', ':'};
      const std::string_view sv{buf.data(), buf.size()};
      auto result = glz::lazy_json<glz::opts{.null_terminated = false}>(sv);
      expect(result.has_value());
      auto& doc = *result;

      auto a1 = doc["a"]; // primes parse_pos_ to the value of "a"
      expect(!a1.has_error());
      expect(a1.template get<int64_t>().value() == 1);

      expect(doc["b"].has_error()); // progressive scan matches "b"; its value is truncated

      auto a2 = doc["a"]; // must still resolve after the truncated match
      expect(!a2.has_error());
      expect(a2.template get<int64_t>().value() == 1);
   };

   // Malformed *null-terminated* input (an unclosed container) must stay bounded by json_end_ in
   // the accessors too, not just iteration. Each backing store below is a heap std::vector holding
   // the message bytes followed by a single '\0' at json_end_, with the view sized to exclude the
   // '\0'; any read past that sentinel lands outside the allocation and aborts under the sanitizer
   // CI builds. Before the fix, size() walked skip_string_fast past the sentinel (OOB read),
   // index() recorded elements at end forever (OOM), operator[](key) spun forever, and
   // operator[](index) handed back a view positioned at end.
   "lazy_json_null_terminated_unclosed_object_size"_test = [] {
      std::vector<char> storage{'{', '"', 'a', '"', ':', '1', '\0'}; // '\0' at index 6 == json_end_
      const std::string_view sv{storage.data(), 6}; // "{\"a\":1"
      auto result = glz::lazy_json<glz::opts{}>(sv); // null_terminated = true
      expect(result.has_value());
      expect((*result).root().size() == 1u); // one complete member, then bounded by json_end_
   };

   "lazy_json_null_terminated_unclosed_object_index"_test = [] {
      std::vector<char> storage{'{', '"', 'a', '"', ':', '1', '\0'};
      const std::string_view sv{storage.data(), 6};
      auto result = glz::lazy_json<glz::opts{}>(sv);
      expect(result.has_value());
      auto idx = (*result).root().index();
      expect(idx.size() == 1u); // loop stops at json_end_ instead of recording end forever
      expect(idx["a"].template get<int64_t>().value() == 1);
   };

   "lazy_json_null_terminated_unclosed_object_lookup"_test = [] {
      std::vector<char> storage{'{', '"', 'a', '"', ':', '1', '\0'};
      const std::string_view sv{storage.data(), 6};
      auto result = glz::lazy_json<glz::opts{}>(sv);
      expect(result.has_value());
      auto& root = (*result).root();
      expect(!root["a"].has_error()); // present key resolves
      expect(root["a"].template get<int64_t>().value() == 1);
      expect(root["missing"].has_error()); // absent key on an unclosed object: not_found, no spin
   };

   "lazy_json_null_terminated_unclosed_array_index"_test = [] {
      std::vector<char> storage{'[', '1', ',', '2', '\0'}; // "[1,2" then '\0' at json_end_
      const std::string_view sv{storage.data(), 4};
      auto result = glz::lazy_json<glz::opts{}>(sv);
      expect(result.has_value());
      auto& root = (*result).root();
      expect(root[0].template get<int64_t>().value() == 1);
      expect(root[1].template get<int64_t>().value() == 2);
      expect(root[2].has_error()); // past the end: an error, not a bogus view positioned at end
   };

   // operator[](key) runs its value-truncation guard in null-terminated mode too. Without it, a
   // matched key whose value begins at json_end_ hands back a bogus non-error view positioned at
   // end; for a std::string that view's data_ points at the '\0', so ASan would NOT catch the
   // regression. This test pins the behavior via has_error() instead.
   "lazy_json_null_terminated_truncated_match_lookup"_test = [] {
      // {"a":1,"b":  -- "a" is complete, "b" is matched but its value begins at json_end_.
      std::vector<char> storage{'{', '"', 'a', '"', ':', '1', ',', '"', 'b', '"', ':', '\0'};
      const std::string_view sv{storage.data(), 11}; // excludes the trailing '\0'
      auto result = glz::lazy_json<glz::opts{}>(sv); // null_terminated = true
      expect(result.has_value());
      auto& root = *result;
      expect(root["a"].template get<int64_t>().value() == 1); // complete member resolves
      expect(root["b"].has_error()); // matched, value truncated at end: error, not a view at end
   };

   // empty() must agree with size()==0 for a clearly-empty (bare, unclosed) null-terminated
   // container. Previously it inspected only the first byte after the brace and reported non-empty.
   "lazy_json_null_terminated_unclosed_empty"_test = [] {
      std::vector<char> obj{'{', '\0'};
      std::vector<char> arr{'[', '\0'};
      auto o = glz::lazy_json<glz::opts{}>(std::string_view{obj.data(), 1});
      auto a = glz::lazy_json<glz::opts{}>(std::string_view{arr.data(), 1});
      expect(o.has_value());
      expect(a.has_value());
      expect((*o).root().empty()); // '{' with nothing inside is empty
      expect((*o).root().size() == 0u);
      expect((*a).root().empty()); // '[' with nothing inside is empty
      expect((*a).root().size() == 0u);
   };

   // An unrecognized byte in value position (not a string, literal, container, or number - e.g.
   // '@') makes skip_value_lazy's number branch consume nothing. Before the fix that left the scan
   // position unmoved, so every value-skipping loop (size/index/operator[]/iteration) spun forever
   // on this malformed input. skip_value_lazy now guarantees forward progress by treating the byte
   // as a one-byte scalar, so the accessors terminate. The backing stores below are heap vectors
   // sized to exclude any '\0', so the sanitizer CI builds abort on any read past json_end_ - the
   // force-advance must never step over the end.
   "lazy_json_invalid_value_byte_array"_test = [] {
      const std::vector<char> closed{'[', '@', ']'}; // garbage element, closed
      const std::vector<char> open{'[', '@'}; // garbage element, unclosed
      const std::string_view sv_closed{closed.data(), closed.size()};
      const std::string_view sv_open{open.data(), open.size()};

      auto rc = glz::lazy_json<glz::opts{.null_terminated = false}>(sv_closed);
      auto ro = glz::lazy_json<glz::opts{.null_terminated = false}>(sv_open);
      expect(rc.has_value());
      expect(ro.has_value());

      // Each of these calls spun forever before the fix; reaching the assertions proves termination.
      expect((*rc).root().size() == 1u); // '@' counted as a lone scalar, then ']'
      expect((*rc).root().index().size() == 1u);
      expect((*rc).root()[1].has_error()); // only index 0 exists; index 1 is past the end

      expect((*ro).root().size() == 1u); // unclosed: bounded by json_end_ after the one scalar
      expect((*ro).root().index().size() == 1u);
   };

   // operator[](key) is the case the report called out: an invalid byte in value position hung the
   // key search. {"a":@} matches "a" (its value is the garbage byte) and must resolve an absent key
   // to not_found instead of spinning. {"a":1@ (a complete value followed by trailing garbage and
   // no close) must likewise terminate. Non-null-terminated exact-sized stores catch any OOB.
   "lazy_json_invalid_value_byte_object_lookup"_test = [] {
      const std::vector<char> garbage_val{'{', '"', 'a', '"', ':', '@', '}'}; // {"a":@}
      const std::vector<char> trailing{'{', '"', 'a', '"', ':', '1', '@'}; // {"a":1@  (unclosed)
      const std::string_view sv_g{garbage_val.data(), garbage_val.size()};
      const std::string_view sv_t{trailing.data(), trailing.size()};

      auto rg = glz::lazy_json<glz::opts{.null_terminated = false}>(sv_g);
      auto rt = glz::lazy_json<glz::opts{.null_terminated = false}>(sv_t);
      expect(rg.has_value());
      expect(rt.has_value());

      expect(!(*rg).root()["a"].has_error()); // "a" matches (value is the garbage byte)
      expect((*rg).root()["missing"].has_error()); // absent key: not_found, no spin

      expect((*rt).root()["a"].template get<int64_t>().value() == 1); // "a" resolves to 1
      expect((*rt).root()["missing"].has_error()); // scan past trailing garbage terminates
   };

   // Iteration must also terminate across an invalid value byte. A counter guard converts a
   // regression from a true hang into a visible wrong count.
   "lazy_json_invalid_value_byte_iteration"_test = [] {
      const std::vector<char> arr{'[', '1', ',', '@', ',', '2', ']'}; // [1,@,2]
      const std::string_view sv{arr.data(), arr.size()};
      auto result = glz::lazy_json<glz::opts{.null_terminated = false}>(sv);
      expect(result.has_value());
      size_t count = 0;
      for (auto element : (*result).root()) {
         (void)element;
         if (++count > 100) break; // guard: a regression here would loop forever
      }
      expect(count == 3u); // 1, the '@' scalar, and 2
   };

   // The same invalid-byte guarantee under the default null_terminated option: the garbage byte
   // sits strictly before the '\0', which lands at json_end_. The scan must advance across the
   // garbage and stop at json_end_, not spin and not walk past the sentinel.
   "lazy_json_invalid_value_byte_null_terminated"_test = [] {
      std::vector<char> storage{'[', '@', '\0'}; // '\0' at index 2 == json_end_
      const std::string_view sv{storage.data(), 2}; // "[@"
      auto result = glz::lazy_json<glz::opts{}>(sv); // null_terminated = true
      expect(result.has_value());
      expect((*result).root().size() == 1u); // '@' scalar, then bounded by json_end_
      expect((*result).root().index().size() == 1u);
      expect((*result).root()[1].has_error()); // nothing past the lone scalar
   };
};

int main() { return 0; }
