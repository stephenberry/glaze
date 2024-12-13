// Glaze Library
// For the license information refer to glaze.hpp

#include "glaze/stencil/stencil.hpp"

#include "glaze/glaze.hpp"
#include "glaze/stencil/stencilcount.hpp"
#include "ut/ut.hpp"

using namespace ut;

struct person
{
   std::string first_name{};
   std::string last_name{};
   uint32_t age{};
   bool hungry{};
};

suite mustache_tests = [] {
   "person"_test = [] {
      std::string_view layout = R"({{first_name}} {{last_name}} {{age}})";

      person p{"Henry", "Foster", 34};
      auto result = glz::stencil(layout, p).value_or("error");
      expect(result == "Henry Foster 34") << result;
   };

   "person"_test = [] {
      std::string_view layout = R"({{first_name}} {{last_name}}, age: {{age}})";

      person p{"Henry", "Foster", 34};
      auto result = glz::stencil(layout, p).value_or("error");
      expect(result == "Henry Foster, age: 34") << result;
   };

   "person"_test = [] {
      std::string_view layout = R"({{first_name}} {{last}}, age: {{age}})";

      person p{"Henry", "Foster", 34};
      auto result = glz::stencil(layout, p);
      expect(not result.has_value());
      expect(result.error() == glz::error_code::unknown_key);
   };

   "comment"_test = [] {
      std::string_view layout = R"({{first_name}} {{! This is a comment }}{{last_name}})";

      person p{"Henry", "Foster", 34};
      auto result = glz::stencil(layout, p).value_or("error");
      expect(result == "Henry Foster") << result;
   };

   "unsupported section"_test = [] {
      std::string_view layout = R"({{#hungry}}I am hungry{{/hungry}})";

      person p{"Henry", "Foster", 34, true};
      auto result = glz::stencil(layout, p);
      expect(not result.has_value());
      expect(result.error() == glz::error_code::feature_not_supported);
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
      auto result = glz::stencilcount(layout, p).value_or("error");
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
