# Stencil (String Interpolation)

> [!WARNING]
>
> glz::stencil is still somewhat experimental. The API may not be as stable as other parts of Glaze.

Glaze supports string interpolation for serializing structs using `glz::stencil`.

```c++
struct person
{
   std::string first_name{};
   std::string last_name{};
   uint32_t age{};
   bool hungry{};
   bool employed{};
};

"person"_test = [] {
  std::string_view layout = R"({{first_name}} {{last_name}}, age: {{age}})";
  
  person p{"Henry", "Foster", 34};
  auto result = glz::stencil(layout, p);
  expect(result == "Henry Foster, age: 34");
};

"comment"_test = [] {
  std::string_view layout = R"({{first_name}} {{! This is a comment }}{{last_name}})";

  person p{"Henry", "Foster", 34};
  auto result = glz::stencil(layout, p).value_or("error");
  expect(result == "Henry Foster") << result;
};

"section_true"_test = [] {
  std::string_view layout = R"({{first_name}} {{last_name}} {{#employed}}Employed{{/employed}})";

  person p{"Alice", "Johnson", 28, true, true}; // employed is true
  auto result = glz::stencil(layout, p).value_or("error");
  expect(result == "Alice Johnson Employed") << result;
};

"section_with_inner_placeholders"_test = [] {
  std::string_view layout = R"({{first_name}} {{last_name}} {{#employed}}Status: Employed, Age: {{age}}{{/employed}})";

  person p{"Carol", "Davis", 30, true, true};
  auto result = glz::stencil(layout, p).value_or("error");
  expect(result == "Carol Davis Status: Employed, Age: 30") << result;
};

"inverted_section_true"_test = [] {
  std::string_view layout = R"({{first_name}} {{last_name}} {{^hungry}}I'm not hungry{{/hungry}})";

  person p{"Henry", "Foster", 34, false}; // hungry is false
  auto result = glz::stencil(layout, p).value_or("error");
  expect(result == "Henry Foster I'm not hungry") << result;
};
```

## Specification

- `{{field}}` 
  - Represents an interpolated field, to be replaced
- `{{#boolean}} section {{/boolean}}`
  - Activates the section if the boolean field is true
- `{{^boolean}} section {{/boolean}}`
  - Activates the section if the boolean field is false
