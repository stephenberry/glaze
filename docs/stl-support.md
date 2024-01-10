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

See [Variant Handling](./docs/variant-handling.md) for more information.

## Nullable Types

- `std::unique_ptr`
- `std::shared_ptr`
- `std::optional`

Nullable types may be allocated by valid input or nullified by the `null` keyword.

## String Types

- `std::string`
- `std::string_view`
- `std::bitset`

```c++
std::bitset<8> b = 0b10101010;
// will be serialized as a string: "10101010"
```

