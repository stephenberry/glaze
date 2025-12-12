// Unit tests for std::u8string support (both as buffer and value types)
#include <string>

#include "glaze/glaze.hpp"
#include "ut/ut.hpp"

using namespace ut;

// Test struct with basic types for buffer tests
struct TestMsg
{
   int id{};
   std::string val{};
};

// Test struct containing std::u8string as a value member
struct U8StringValue
{
   int id{};
   std::u8string text{};
};

suite u8string_buffer_tests = [] {
   "u8string buffer write"_test = [] {
      TestMsg msg{};
      msg.id = 42;
      msg.val = "hello world";
      std::u8string buffer{};
      expect(not glz::write_json(msg, buffer));

      // Verify content by converting to regular string for comparison
      std::string_view sv(reinterpret_cast<const char*>(buffer.data()), buffer.size());
      expect(sv == R"({"id":42,"val":"hello world"})") << sv;
   };

   "u8string buffer roundtrip"_test = [] {
      TestMsg msg{};
      msg.id = 123;
      msg.val = "test string";
      std::u8string buffer{};
      expect(not glz::write_json(msg, buffer));

      buffer.push_back(u8'\0');

      TestMsg msg2{};
      expect(not glz::read_json(msg2, buffer));
      expect(msg2.id == 123);
      expect(msg2.val == "test string");
   };

   "u8string buffer with unicode"_test = [] {
      TestMsg msg{};
      msg.id = 1;
      msg.val = "こんにちは"; // Japanese hello
      std::u8string buffer{};
      expect(not glz::write_json(msg, buffer));

      buffer.push_back(u8'\0');

      TestMsg msg2{};
      expect(not glz::read_json(msg2, buffer));
      expect(msg2.id == 1);
      expect(msg2.val == "こんにちは");
   };

   "u8string buffer with escapes"_test = [] {
      TestMsg msg{};
      msg.id = 7;
      msg.val = "line1\nline2\ttab";
      std::u8string buffer{};
      expect(not glz::write_json(msg, buffer));

      buffer.push_back(u8'\0');

      TestMsg msg2{};
      expect(not glz::read_json(msg2, buffer));
      expect(msg2.id == 7);
      expect(msg2.val == "line1\nline2\ttab");
   };

   "u8string buffer with special characters"_test = [] {
      TestMsg msg{};
      msg.id = 99;
      msg.val = R"(quotes: "hello", backslash: \)";
      std::u8string buffer{};
      expect(not glz::write_json(msg, buffer));

      buffer.push_back(u8'\0');

      TestMsg msg2{};
      expect(not glz::read_json(msg2, buffer));
      expect(msg2.id == 99);
      expect(msg2.val == R"(quotes: "hello", backslash: \)");
   };

   "u8string buffer empty object"_test = [] {
      glz::generic obj{};
      std::u8string buffer{};
      expect(not glz::write_json(obj, buffer));

      std::string_view sv(reinterpret_cast<const char*>(buffer.data()), buffer.size());
      expect(sv == "null") << sv;
   };

   "u8string buffer nested objects"_test = [] {
      struct Nested
      {
         int a{};
         std::string b{};
      };
      struct Outer
      {
         Nested inner{};
         std::vector<int> nums{};
      };

      Outer obj{};
      obj.inner.a = 10;
      obj.inner.b = "nested";
      obj.nums = {1, 2, 3};

      std::u8string buffer{};
      expect(not glz::write_json(obj, buffer));

      buffer.push_back(u8'\0');

      Outer obj2{};
      expect(not glz::read_json(obj2, buffer));
      expect(obj2.inner.a == 10);
      expect(obj2.inner.b == "nested");
      expect(obj2.nums == std::vector{1, 2, 3});
   };

   "u8string buffer large data"_test = [] {
      struct LargeData
      {
         std::vector<int> data{};
      };

      LargeData obj{};
      obj.data.resize(1000);
      for (int i = 0; i < 1000; ++i) {
         obj.data[i] = i;
      }

      std::u8string buffer{};
      expect(not glz::write_json(obj, buffer));

      buffer.push_back(u8'\0');

      LargeData obj2{};
      expect(not glz::read_json(obj2, buffer));
      expect(obj2.data.size() == 1000);
      expect(obj2.data[0] == 0);
      expect(obj2.data[999] == 999);
   };
};

suite u8string_value_tests = [] {
   "u8string value write"_test = [] {
      U8StringValue obj{};
      obj.id = 1;
      obj.text = u8"hello";

      std::string buffer{};
      expect(not glz::write_json(obj, buffer));
      expect(buffer == R"({"id":1,"text":"hello"})") << buffer;
   };

   "u8string value roundtrip"_test = [] {
      U8StringValue obj{};
      obj.id = 42;
      obj.text = u8"test string";

      std::string buffer{};
      expect(not glz::write_json(obj, buffer));

      U8StringValue obj2{};
      expect(not glz::read_json(obj2, buffer));
      expect(obj2.id == 42);
      expect(obj2.text == u8"test string");
   };

   "u8string value unicode"_test = [] {
      U8StringValue obj{};
      obj.id = 100;
      obj.text = u8"日本語テスト"; // Japanese test

      std::string buffer{};
      expect(not glz::write_json(obj, buffer));

      U8StringValue obj2{};
      expect(not glz::read_json(obj2, buffer));
      expect(obj2.id == 100);
      expect(obj2.text == u8"日本語テスト");
   };

   "u8string value emoji"_test = [] {
      U8StringValue obj{};
      obj.id = 200;
      obj.text = u8"Hello 🌍🚀✨";

      std::string buffer{};
      expect(not glz::write_json(obj, buffer));

      U8StringValue obj2{};
      expect(not glz::read_json(obj2, buffer));
      expect(obj2.id == 200);
      expect(obj2.text == u8"Hello 🌍🚀✨");
   };

   "u8string value empty"_test = [] {
      U8StringValue obj{};
      obj.id = 0;
      obj.text = u8"";

      std::string buffer{};
      expect(not glz::write_json(obj, buffer));
      expect(buffer == R"({"id":0,"text":""})") << buffer;

      U8StringValue obj2{};
      obj2.text = u8"should be cleared";
      expect(not glz::read_json(obj2, buffer));
      expect(obj2.text.empty());
   };

   "u8string value with escapes"_test = [] {
      U8StringValue obj{};
      obj.id = 300;
      obj.text = u8"line1\nline2\ttab\\backslash\"quote";

      std::string buffer{};
      expect(not glz::write_json(obj, buffer));

      U8StringValue obj2{};
      expect(not glz::read_json(obj2, buffer));
      expect(obj2.id == 300);
      expect(obj2.text == u8"line1\nline2\ttab\\backslash\"quote");
   };

   "u8string value mixed struct"_test = [] {
      struct MixedStrings
      {
         std::string regular{};
         std::u8string utf8{};
         int num{};
      };

      MixedStrings obj{};
      obj.regular = "regular string";
      obj.utf8 = u8"utf8 string 日本語";
      obj.num = 42;

      std::string buffer{};
      expect(not glz::write_json(obj, buffer));

      MixedStrings obj2{};
      expect(not glz::read_json(obj2, buffer));
      expect(obj2.regular == "regular string");
      expect(obj2.utf8 == u8"utf8 string 日本語");
      expect(obj2.num == 42);
   };

   "u8string value in vector"_test = [] {
      struct VectorOfU8
      {
         std::vector<std::u8string> strings{};
      };

      VectorOfU8 obj{};
      obj.strings = {u8"first", u8"second", u8"third"};

      std::string buffer{};
      expect(not glz::write_json(obj, buffer));

      VectorOfU8 obj2{};
      expect(not glz::read_json(obj2, buffer));
      expect(obj2.strings.size() == 3);
      expect(obj2.strings[0] == u8"first");
      expect(obj2.strings[1] == u8"second");
      expect(obj2.strings[2] == u8"third");
   };

   "u8string value as map key"_test = [] {
      struct MapWithU8Key
      {
         std::map<std::u8string, int> data{};
      };

      MapWithU8Key obj{};
      obj.data[u8"key1"] = 10;
      obj.data[u8"key2"] = 20;

      std::string buffer{};
      expect(not glz::write_json(obj, buffer));

      MapWithU8Key obj2{};
      expect(not glz::read_json(obj2, buffer));
      expect(obj2.data.size() == 2);
      expect(obj2.data[u8"key1"] == 10);
      expect(obj2.data[u8"key2"] == 20);
   };

   "u8string value as map value"_test = [] {
      struct MapWithU8Value
      {
         std::map<std::string, std::u8string> data{};
      };

      MapWithU8Value obj{};
      obj.data["key1"] = u8"value1";
      obj.data["key2"] = u8"value2 日本語";

      std::string buffer{};
      expect(not glz::write_json(obj, buffer));

      MapWithU8Value obj2{};
      expect(not glz::read_json(obj2, buffer));
      expect(obj2.data.size() == 2);
      expect(obj2.data["key1"] == u8"value1");
      expect(obj2.data["key2"] == u8"value2 日本語");
   };
};

suite u8string_combined_tests = [] {
   "u8string buffer and value combined"_test = [] {
      U8StringValue obj{};
      obj.id = 999;
      obj.text = u8"combined test 🎉";

      std::u8string buffer{};
      expect(not glz::write_json(obj, buffer));

      buffer.push_back(u8'\0');

      U8StringValue obj2{};
      expect(not glz::read_json(obj2, buffer));
      expect(obj2.id == 999);
      expect(obj2.text == u8"combined test 🎉");
   };

   "u8string buffer with u8string values in nested struct"_test = [] {
      struct Inner
      {
         std::u8string name{};
      };
      struct Outer
      {
         Inner inner{};
         std::u8string title{};
      };

      Outer obj{};
      obj.inner.name = u8"inner name";
      obj.title = u8"outer title";

      std::u8string buffer{};
      expect(not glz::write_json(obj, buffer));

      buffer.push_back(u8'\0');

      Outer obj2{};
      expect(not glz::read_json(obj2, buffer));
      expect(obj2.inner.name == u8"inner name");
      expect(obj2.title == u8"outer title");
   };
};

suite u8string_view_tests = [] {
   "u8string_view read"_test = [] {
      std::string json = R"("hello world")";
      std::u8string_view value{};
      expect(not glz::read_json(value, json));
      expect(value == u8"hello world");
   };

   "u8string_view write"_test = [] {
      std::u8string_view value = u8"test string";
      std::string buffer{};
      expect(not glz::write_json(value, buffer));
      expect(buffer == R"("test string")");
   };

   "u8string_view unicode"_test = [] {
      std::string json = R"("日本語テスト")";
      std::u8string_view value{};
      expect(not glz::read_json(value, json));
      // The view points into the json buffer (raw string, no escape processing)
      expect(value == u8"日本語テスト");
   };

   "u8string_view in struct"_test = [] {
      // Note: u8string_view in structs only works when the view can point to the source buffer
      std::u8string_view text = u8"static text";
      std::string buffer{};
      expect(not glz::write_json(text, buffer));
      expect(buffer == R"("static text")");
   };
};

int main() { return 0; }
