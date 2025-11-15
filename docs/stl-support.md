# Standard Template Library Support

Glaze uses C++20 concepts to not only support the standard template library, but also other libraries or custom types that conform to the same interface.

## Array Types

Array types logically convert to JSON array values. Concepts are used to allow various containers and even user containers if they match standard library interfaces.

- `std::tuple`
- `std::array`
- `std::vector`
- `std::deque`
- `std::list`
- `std::forward_list`
- `std::span`
- `std::set`
- `std::unordered_set`

## Object Types

- `glz::object` (compile time mixed types)
- `std::map`
- `std::unordered_map`

## Variants

- `std::variant`

Variants support auto-deduction and tagged types. Smart pointers (`std::unique_ptr`, `std::shared_ptr`) are fully supported as variant alternatives, including with tagged variants.

See [Variant Handling](./variant-handling.md) for more information.

## Nullable Types

- `std::unique_ptr`
- `std::shared_ptr`
- `std::optional`
- Raw pointers (`T*`)

Nullable types may be allocated by valid input or nullified by the `null` keyword. Smart pointers can also be used as variant alternatives (e.g., `std::variant<std::unique_ptr<A>, std::unique_ptr<B>>`), see [Variant Handling](./variant-handling.md) for details.

> [!NOTE]
> Raw pointers work with automatic reflection and respect the `skip_null_members` option. See [Nullable Types](./nullable-types.md) for detailed information about pointer handling.

## String Types

- `std::string`
- `std::string_view`
- `std::filesystem::path`
  - Note that `std::filesystem::path` does not work with pure reflection on GCC and MSVC (requires a glz::meta)

- `std::bitset`

```c++
std::bitset<8> b = 0b10101010;
// will be serialized as a string: "10101010"
```

## std::expected

`std::expected` is supported where the expected value is treated as it would typically be handled in JSON, and the unexpected value is treated as a JSON object with the unexpected value referred to by an `"unexpected"` key.

Example of unexpected:

```json
{"unexpected": "my error string"}
```

