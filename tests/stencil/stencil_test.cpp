// Glaze Library
// For the license information refer to glaze.hpp

#include "glaze/glaze.hpp"
#include "ut/ut.hpp"

using namespace ut;

struct person
{
   std::string first_name{};
   std::string last_name{};
   uint32_t age{};
   bool hungry{};
   bool employed{};
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
   
   // **Regular Section Tests (#)**

   "section_true"_test = [] {
      std::string_view layout = R"({{first_name}} {{last_name}} {{#employed}}Employed{{/employed}})";

      person p{"Alice", "Johnson", 28, true, true}; // employed is true
      auto result = glz::stencil(layout, p).value_or("error");
      expect(result == "Alice Johnson Employed") << result;
   };

   "section_false"_test = [] {
      std::string_view layout = R"({{first_name}} {{last_name}} {{#employed}}Employed{{/employed}})";

      person p{"Bob", "Smith", 45, false, false}; // employed is false
      auto result = glz::stencil(layout, p).value_or("error");
      expect(result == "Bob Smith ") << result; // The section should be skipped
   };

   "section_with_inner_placeholders"_test = [] {
      std::string_view layout = R"({{first_name}} {{last_name}} {{#employed}}Status: Employed, Age: {{age}}{{/employed}})";

      person p{"Carol", "Davis", 30, true, true};
      auto result = glz::stencil(layout, p).value_or("error");
      expect(result == "Carol Davis Status: Employed, Age: 30") << result;
   };

   "section_with_extra_text"_test = [] {
      std::string_view layout = R"({{first_name}} {{last_name}} {{#employed}}Employed{{/employed}}. Welcome!)";

      person p{"Dave", "Miller", 40, true, true};
      auto result = glz::stencil(layout, p).value_or("error");
      expect(result == "Dave Miller Employed. Welcome!") << result;
   };

   "section_with_extra_text_skipped"_test = [] {
      std::string_view layout = R"({{first_name}} {{last_name}} {{#employed}}Employed{{/employed}}. Welcome!)";

      person p{"Eve", "Wilson", 22, true, false}; // employed is false
      auto result = glz::stencil(layout, p).value_or("error");
      expect(result == "Eve Wilson . Welcome!") << result;
   };

   "nested_sections"_test = [] {
      std::string_view layout = R"({{first_name}} {{last_name}} {{#employed}}Status: Employed {{#hungry}}and Hungry{{/hungry}}{{/employed}})";

      person p1{"Frank", "Taylor", 50, true, true}; // employed is true, hungry is true
      auto result1 = glz::stencil(layout, p1);
      expect(result1 == "Frank Taylor Status: Employed and Hungry");

      person p2{"Grace", "Anderson", 0, false, true}; // employed is true, hungry is false
      auto result2 = glz::stencil(layout, p2);
      expect(result2 == "Grace Anderson Status: Employed ") << result2.value();
   };

   "section_unknown_key"_test = [] {
      std::string_view layout = R"({{first_name}} {{last_name}} {{#unknown}}Should not appear{{/unknown}})";

      person p{"Henry", "Foster", 34, false, true};
      auto result = glz::stencil(layout, p);
      expect(not result.has_value());
      expect(result.error() == glz::error_code::unknown_key);
   };

   "section_mismatched_closing_tag"_test = [] {
      std::string_view layout = R"({{first_name}} {{last_name}} {{#employed}}Employed{{/employment}})"; // Mismatched closing tag

      person p{"Ivy", "Thomas", 29, false, true};
      auto result = glz::stencil(layout, p);
      expect(not result.has_value());
      expect(result.error() == glz::error_code::unexpected_end);
   };
   
   // **Inverted Section Tests**

   "inverted_section_true"_test = [] {
      std::string_view layout = R"({{first_name}} {{last_name}} {{^hungry}}I'm not hungry{{/hungry}})";

      person p{"Henry", "Foster", 34, false}; // hungry is false
      auto result = glz::stencil(layout, p).value_or("error");
      expect(result == "Henry Foster I'm not hungry") << result;
   };

   "inverted_section_false"_test = [] {
      std::string_view layout = R"({{first_name}} {{last_name}} {{^hungry}}I'm not hungry{{/hungry}})";

      person p{"Henry", "Foster", 34, true}; // hungry is true
      auto result = glz::stencil(layout, p).value_or("error");
      expect(result == "Henry Foster ") << result; // The inverted section should be skipped
   };

   "inverted_section_with_extra_text_true"_test = [] {
      std::string_view layout = R"({{first_name}} {{last_name}} {{^hungry}}I'm not hungry{{/hungry}}. Have a nice day!)";

      person p{"Henry", "Foster", 34, false}; // hungry is false
      auto result = glz::stencil(layout, p).value_or("error");
      expect(result == "Henry Foster I'm not hungry. Have a nice day!") << result;
   };

   "inverted_section_with_extra_text_false"_test = [] {
      std::string_view layout = R"({{first_name}} {{last_name}} {{^hungry}}I'm not hungry{{/hungry}}. Have a nice day!)";

      person p{"Henry", "Foster", 34, true}; // hungry is true
      auto result = glz::stencil(layout, p).value_or("error");
      expect(result == "Henry Foster . Have a nice day!") << result;
   };

   "nested_inverted_section"_test = [] {
       std::string_view layout = R"({{first_name}} {{last_name}} {{^hungry}}I'm not hungry {{^employed}}and not employed{{/employed}}{{/hungry}})";

       person p1{"Henry", "Foster", 34, false, false};
       auto result1 = glz::stencil(layout, p1).value_or("error");
       expect(result1 == "Henry Foster I'm not hungry and not employed") << result1;

       person p2{"Henry", "Foster", 34, false, true};
       auto result2 = glz::stencil(layout, p2).value_or("error");
       expect(result2 == "Henry Foster I'm not hungry ") << result2;

       person p3{"Henry", "Foster", 34, true, false};
       std::string_view layout_skip = R"({{first_name}} {{last_name}} {{^hungry}}I'm not hungry {{^employed}}and not employed{{/employed}}{{/hungry}})";
       auto result3 = glz::stencil(layout_skip, p3).value_or("error");
       expect(result3 == "Henry Foster ") << result3;
   };

   "inverted_section_unknown_key"_test = [] {
      std::string_view layout = R"({{first_name}} {{last_name}} {{^unknown}}Should not appear{{/unknown}})";

      person p{"Henry", "Foster", 34, false};
      auto result = glz::stencil(layout, p);
      expect(not result.has_value());
      expect(result.error() == glz::error_code::unknown_key);
   };

   "inverted_section_mismatched_closing_tag"_test = [] {
      std::string_view layout = R"({{first_name}} {{last_name}} {{^hungry}}I'm not hungry{{/hunger}})"; // Mismatched closing tag

      person p{"Henry", "Foster", 34, false};
      auto result = glz::stencil(layout, p);
      expect(not result.has_value());
      expect(result.error() == glz::error_code::unexpected_end);
   };
};

#include "glaze/stencil/stencilcount.hpp"

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
