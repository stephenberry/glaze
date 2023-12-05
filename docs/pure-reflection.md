# Pure Reflection

Version 1.9.0 adds pure reflection for aggregate initializable structs in Clang, MSVC, and GCC.

There's no need to write any `glz::meta` structures or use any macros. The reflection is hidden from
the user and computed at compile time, available with the inclusion of
`glaze/reflection/reflect.hpp`.

- You can still write a `glz::meta` to customize your serialization, which will override the default reflection.
- The `glz::meta` approach is still the most optimized. There is some work to be done to make the automatic reflection just as fast.

> IMPORTANT:
>
> When writing custom serializers specializing `to_json` or `from_json`, your concepts for custom types might not take precedence over the reflection engine (when you haven't written a `glz::meta` for your type). The reflection engine tries to discern that no specialization occurs for your type, but this is not always possible.
>
> To solve this problem, you can add `static constexpr auto reflect = false;` to your class. Or, you can add a `glz::meta` for your type.
