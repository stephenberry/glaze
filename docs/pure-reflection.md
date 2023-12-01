# Pure Reflection in Clang

Version 1.7.0 added pure reflection for aggregate, constexpr initializable structs in Clang.

For the Clang compiler only, Glaze will reflect your structs. No need to write any `glz::meta` structures or use any macros. The reflection is hidden from the user and computed at compile time.

- You can still write a `glz::meta` to customize your serialization, which will override the default reflection.
- This approach only works for types that are constexpr constructible, but this includes `std::string` and `std::vector` in newer versions of Clang.
- The `glz::meta` approach is still the most optimized. There is some work to be done to make the automatic reflection just as fast.
- This approach currently uses Clang's `__builtin_dump_struct`, but we will be able to invisibly move to standard C++ reflection whenever it becomes available.

> IMPORTANT:
>
> When writing custom serializers specializing `to_json` or `from_json`, your concepts for custom types might not take precedence over the reflection engine (when you haven't written a `glz::meta` for your type). The reflection engine tries to discern that no specialization occurs for your type, but this is not always possible.
>
> To solve this problem, you can add `static constexpr auto reflect = false;` to your class. Or, you can add a `glz::meta` for your type.
