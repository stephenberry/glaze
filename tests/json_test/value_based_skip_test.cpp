// Test for value-based skip functionality
// Related to GitHub issue #1994

#include <string>

#include "glaze/glaze.hpp"
#include "ut/ut.hpp"

using namespace ut;

struct MyJson
{
   std::string name = "John";
   int age = 12;
   std::string city = "New York";
};

template <>
struct glz::meta<MyJson>
{
   template <class T>
   static constexpr bool skip_if(T&& value, std::string_view key, const glz::meta_context&)
   {
      using V = std::decay_t<T>;
      if constexpr (std::same_as<V, std::string>) {
         return key == "name" && value == "John";
      }
      else if constexpr (std::same_as<V, int>) {
         return key == "age" && value == 12;
      }
      return false;
   }
};

suite value_based_skip_tests = [] {
   "skip_default_values"_test = [] {
      MyJson obj{};
      obj.name = "John"; // Default value
      obj.age = 12; // Default value
      obj.city = "Boston"; // Non-default value

      std::string buffer{};
      auto ec = glz::write_json(obj, buffer);
      expect(!ec);

      // Should only contain city since name and age have default values
      expect(buffer == R"({"city":"Boston"})") << buffer;
   };

   "include_non_default_values"_test = [] {
      MyJson obj{};
      obj.name = "Jane"; // Non-default value
      obj.age = 25; // Non-default value
      obj.city = "Seattle";

      std::string buffer{};
      auto ec = glz::write_json(obj, buffer);
      expect(!ec);

      // Should contain all fields since none have default values
      expect(buffer == R"({"name":"Jane","age":25,"city":"Seattle"})") << buffer;
   };

   "mixed_default_and_non_default"_test = [] {
      MyJson obj{};
      obj.name = "John"; // Default value - should be skipped
      obj.age = 30; // Non-default value - should be included
      obj.city = "LA";

      std::string buffer{};
      auto ec = glz::write_json(obj, buffer);
      expect(!ec);

      // Should not contain name (default), but should contain age and city
      expect(buffer == R"({"age":30,"city":"LA"})") << buffer;
   };
};

// Test combining skip and skip_if
struct CombinedSkip
{
   std::string id{};
   std::string secret{};
   int count = 0;
};

template <>
struct glz::meta<CombinedSkip>
{
   // Compile-time skip: always exclude secret
   static constexpr bool skip(const std::string_view key, const glz::meta_context&) { return key == "secret"; }

   // Runtime skip: exclude count when it's 0
   template <class T>
   static constexpr bool skip_if(T&& value, std::string_view key, const glz::meta_context&)
   {
      using V = std::decay_t<T>;
      if constexpr (std::same_as<V, int>) {
         return key == "count" && value == 0;
      }
      return false;
   }
};

suite combined_skip_tests = [] {
   "skip_and_skip_if_together"_test = [] {
      CombinedSkip obj1{"123", "my_secret", 0};
      std::string buffer1{};
      auto ec = glz::write_json(obj1, buffer1);
      expect(!ec);
      // secret skipped by skip(), count skipped by skip_if()
      expect(buffer1 == R"({"id":"123"})") << buffer1;

      CombinedSkip obj2{"456", "another_secret", 5};
      std::string buffer2{};
      ec = glz::write_json(obj2, buffer2);
      expect(!ec);
      // secret skipped by skip(), count included (non-zero)
      expect(buffer2 == R"({"id":"456","count":5})") << buffer2;
   };
};

int main() { return 0; }
