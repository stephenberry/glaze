// Glaze Library
// For the license information refer to glaze.hpp

#define UT_RUN_TIME_ONLY

#include "glaze/mustache/mustache.hpp"

#include "glaze/glaze.hpp"
#include "glaze/mustache/stencilcount.hpp"
#include "ut/ut.hpp"

using namespace ut;

struct person
{
   std::string first_name{};
   std::string last_name{};
   uint32_t age{};
   bool hungry{};
};

struct person_template
{
   static constexpr auto glaze_mustache = R"({{first_name}} | {{last_name}} | {{age}})";
   std::string first_name{};
   std::string last_name{};
   uint32_t age{};
   bool hungry{};
};

suite mustache_tests = [] {
   "person"_test = [] {
      std::string_view layout = R"({{first_name}} {{last_name}} {{age}})";

      person p{"Henry", "Foster", 34};
      auto result = glz::mustache(p, layout).value_or("error");
      expect(result == "Henry Foster 34") << result;
   };

   "person"_test = [] {
      std::string_view layout = R"({{first_name}} {{last_name}}, age: {{age}})";

      person p{"Henry", "Foster", 34};
      auto result = glz::mustache(p, layout).value_or("error");
      expect(result == "Henry Foster, age: 34") << result;
   };

   "person"_test = [] {
      std::string_view layout = R"({{first_name}} {{last}}, age: {{age}})";

      person p{"Henry", "Foster", 34};
      auto result = glz::mustache(p, layout);
      expect(not result.has_value());
      expect(result.error() == glz::error_code::unknown_key);
   };

   "person_template"_test = [] {
      person_template p{"Henry", "Foster", 34};
      auto result = glz::mustache(p).value_or("error");
      expect(result == "Henry | Foster | 34") << result;
   };
};

suite stencilcount_tests = [] {
   "basic docstencil"_test = [] {
      std::string_view layout = R"(# About
## {{+}} {{first_name}} {{last_name}}
{{++}} {{first_name}} is {{age}} years old.

## {{+}} Hobbies
{{++}} Outdoor
{{+++}} Running
{{+++}} Hiking
{{+++}} Camping
{{++}} Indoor
{{+++}} Board Games
{{+++}} Cooking

## {{+}} Education
{{++}} College
{{+++}} Math
{{+++}} English
)";

      person p{"Henry", "Foster", 34};
      auto result = glz::stencilcount(p, layout).value_or("error");
      expect(result == R"(# About
## 1. Henry Foster
1.1 Henry is 34 years old.

## 2. Hobbies
2.1 Outdoor
2.1.1 Running
2.1.2 Hiking
2.1.3 Camping
2.1 Indoor
2.1.1 Board Games
2.1.2 Cooking

## 3. Education
3.1 College
3.1.1 Math
3.1.2 English
)") << result;
   };
};

int main() { return 0; }
