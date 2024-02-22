# Pure Reflection

Glaze supports pure reflection for [aggregate initializable](https://en.cppreference.com/w/cpp/language/aggregate_initialization) structs in Clang, MSVC, and GCC.

> Aggregate initializable means:
>
> - no user-declared constructors
> - no user-declared or inherited constructors
> - no private or protected direct non-static data members
> - no [virtual base classes](https://en.cppreference.com/w/cpp/language/derived_class#Virtual_base_classes)

There's no need to write any `glz::meta` structures or use any macros. The reflection is hidden from
the user and computed at compile time.

- You can still write a `glz::meta` to customize your serialization, which will override the default reflection.

> CUSTOMIZATION NOTE:
>
> When writing custom serializers specializing `to_json` or `from_json`, your concepts for custom types might not take precedence over the reflection engine (when you haven't written a `glz::meta` for your type). The reflection engine tries to discern that no specialization occurs for your type, but this is not always possible.
>
> To solve this problem, you can add `static constexpr auto glaze_reflect = false;` to your class. Or, you can add a `glz::meta` for your type.

## Mixing glz::meta reflection with pure-reflected structs

If you have an struct with constructors you may need to use `glz::meta` to define the reflection. But, now your type might break the counting of the numbers of members when included in another struct.

To fix this, add a constructor for your struct to enable the proper counting: `my_struct(glz::make_reflectable) {}`

Example:

```c++
struct V2
{
   float x{};
   float y{};

   V2(glz::make_reflectable) {}
   V2() = default;

   V2(V2&&) = default;
   V2(const float* arr) : x(arr[0]), y(arr[1]) {}
};

template <>
struct glz::meta<V2>
{
   static constexpr auto value = object(&V2::x, &V2::y);
};

struct V2Wrapper
{
   V2 x{}; // reflection wouldn't work except for adding `V2(glz::make_reflectable) {}`
};
```

