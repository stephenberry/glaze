# Pure Reflection

Since v1.9.0 Glaze supports pure reflection for [aggregate initializable](https://en.cppreference.com/w/cpp/language/aggregate_initialization) structs in Clang, MSVC, and GCC.

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
