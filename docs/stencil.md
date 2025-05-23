# Stencil (String Interpolation)

> [!WARNING]
>
> glz::stencil is still somewhat experimental. The API may not be as stable as other parts of Glaze.

Glaze provides string interpolation for C++ structs. The `stencil` feature provides a templating mechanism for formatting structured data into strings. Inspired by the Mustache templating language, it enables the generation of dynamic output by combining predefined templates with C++ structs.

Example:

```c++
struct person
{
   std::string first_name{};
   std::string last_name{};
   uint32_t age{};
   bool hungry{};
};

"person"_test = [] {
  std::string_view layout = R"({{first_name}} | {{last_name}} = {{age}})";

  person p{"Henry", "Foster", 34};
  auto result = glz::stencil(layout, p);
  expect(result == "Henry | Foster = 34");
};

"section_with_inner_placeholders"_test = [] {
  std::string_view layout =
     R"({{first_name}} {{last_name}} {{#employed}}Status: Employed, Age: {{age}}{{/employed}})";

  person p{"Carol", "Davis", 30, true, true};
  auto result = glz::stencil(layout, p);
  expect(result == "Carol Davis Status: Employed, Age: 30");
};

"inverted_section_true"_test = [] {
  std::string_view layout = R"({{first_name}} {{last_name}} {{^hungry}}I'm not hungry{{/hungry}})";

  person p{"Henry", "Foster", 34, false}; // hungry is false
  auto result = glz::stencil(layout, p);
  expect(result == "Henry Foster I'm not hungry");
};
```

> [!NOTE]
>
> `result` in these examples is a `std::expected<std::string, glz::error_ctx>`. Like most functions in Glaze (e.g. `glz::write_json`) you can also pass in your own string buffer as the last argument, in which case the return type is `glz::error_ctx`.

## Specification

- `{{key}}` 
  - Represents an interpolated field, to be replaced.
- `{{#boolean_key}} SECTION {{/boolean_key}}`
  - Activates the SECTION if the boolean field is true.
- `{{^boolean_key}} SECTION {{/boolean_key}}`
  - Activates the SECTION if the boolean field is false.
- `{{! Here is a comment}}`
  - Comments are skipped when interpolating.

## Escaped HTML

Double braces `{{}}` will perform HTML escaping to prevent XSS (cross-site scripting) attacks. Triple braces `{{{}}}` insert content **without escaping**.

> [!TIP]
>
> If you aren't using stencils for HTML then you can use the compile time option `escape_html = false` to avoid escaping HTML even for double braces `{{}}`. This will improve performance.

## Why Not Full Mustache Specification?

At some point Glaze may introduce a `glz::mustache` function with full support. But, `glz::stencil` attempts to provide a more efficient solution by limiting the specification.
