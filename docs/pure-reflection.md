# Pure Reflection

Glaze supports pure reflection for [aggregate initializable](https://en.cppreference.com/w/cpp/language/aggregate_initialization) structs in Clang, MSVC, and GCC.

> Aggregate initializable means:
>
> - no user-declared constructors
> - no user-declared or inherited constructors
> - no private or protected direct non-static data members
> - no [virtual base classes](https://en.cppreference.com/w/cpp/language/derived_class#Virtual_base_classes)

There's no need to write any `glz::meta` structures or use any macros. The reflection is hidden from
the user and computed at compile time. If you eventually need to rename just a couple of fields or add
an alias while keeping everything else automatic, specialize `glz::meta<T>` with a
[`modify`](modify-reflection.md) object. The existing members stay under pure reflection; only the keys you
mention are altered or appended.

Types that support pure reflection satisfy the `glz::reflectable<T>` concept. To check if any type (including those with `glz::meta` specializations) can use the reflection API, use the `glz::has_reflect<T>` concept.

- You can still write a full `glz::meta` to customize your serialization, which will override the default reflection entirely.

## Treating reflected structs as positional arrays

Some JSON use cases employ arrays even though the target type is a struct. Wrap a member with `glz::as_array<&T::member>` to map between the positional representation and your reflected struct.

```c++
struct Person_details
{
   std::string_view name;
   std::string_view surname;
   std::string_view city;
   std::string_view street;
};

struct Person
{
   int id{};
   Person_details person{};
};

template <>
struct glz::meta<Person>
{
   using T = Person;
   static constexpr auto value = glz::object(
      "id", &T::id,
      "person", glz::as_array<&T::person> // consume a JSON array as Person_details
   );
};

std::string_view payload = R"({
   "id": 1,
   "person": ["Joe", "Doe", "London", "Chamber St"]
})";

Person p{};
if (auto ec = glz::read_json(p, payload); ec) {
   throw std::runtime_error(glz::format_error(ec, payload));
}

auto out = glz::write_json(p).value();
// out == R"({"id":1,"person":["Joe","Doe","London","Chamber St"]})"
```

`glz::as_array` relies on the wrapped type's reflection (pure reflection or an explicit `glz::meta`) to map each array element by index. Serialization uses that same order to emit an array, so you get positional reads and writes while the C++ type stays a struct. The wrapper works for JSON, BEVE, CSV, TOML, and any other Glaze format.

> CUSTOMIZATION NOTE:
>
> When writing custom serializers specializing `to<JSON>` or `from<JSON>`, your concepts for custom types might not take precedence over the reflection engine (when you haven't written a `glz::meta` for your type). The reflection engine tries to discern that no specialization occurs for your type, but this is not always possible.
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
