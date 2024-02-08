# Ranges

Glaze has special support for standard ranges. You can create ranges/views of `std::pair` to write out concatenated JSON objects.

```c++
auto num_view =
            std::views::iota(-2, 3) | std::views::transform([](const auto i) { return std::pair(i, i * i); });
expect(glz::write_json(num_view) == glz::sv{R"({"-2":4,"-1":1,"0":0,"1":1,"2":4})"});

auto str_view = std::views::iota(-2, 3) |
                         std::views::transform([](const auto i) { return std::pair(i, std::to_string(i * i)); });
expect(glz::write_json(str_view) == glz::sv{R"({"-2":"4","-1":"1","0":"0","1":"1","2":"4"})"});
```

## Arrays of Arrays

If you want to write out a JSON array of two-element arrays, don't use `std::pair` as the value type. Instead, use `std::array`, `std::tuple`, or `glz::array`.
