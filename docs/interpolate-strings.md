# String Interpolation

> [!WARNING]
>
> glz::interpolate is still somewhat experimental. The API may not be as stable as other parts of Glaze.

Glaze supports string interpolation support for serializing structs.

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
  auto result = glz::interpolate(p, layout).value_or("error");
  expect(result == "Henry Foster 34");
};
```

