# Mustache Format

> [!WARNING]
>
> Glaze Mustache support is still limited and experimental. The API may not be as stable as other formats in Glaze.
>
> Also, html characters are currently not escaped to avoid injection attacks.

Glaze supports basic mustache support for serializing structs.

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
  std::string_view layout = R"({{first_name}} {{last_name}} {{age}})";

  person p{"Henry", "Foster", 34};
  auto result = glz::mustache(p, layout).value_or("error");
  expect(result == "Henry Foster 34");
};
```

## Local Mustache Template

Glaze also supports the template as a static constexpr string.

```c++
struct person_template
{
   static constexpr auto glaze_mustache = R"({{first_name}} | {{last_name}} | {{age}})";
   std::string first_name{};
   std::string last_name{};
   uint32_t age{};
   bool hungry{};
};

"person_template"_test = [] {
  person_template p{"Henry", "Foster", 34};
  auto result = glz::mustache(p).value_or("error");
  expect(result == "Henry | Foster | 34");
};
```

