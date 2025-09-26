# Nullable Types

Glaze provides concepts to support nullable types, including raw pointers, smart pointers, and optional types. In order to serialize and parse your custom nullable type it should conform to the `nullable_t` concept:

```c++
template <class T>
concept nullable_t = !meta_value_t<T> && !str_t<T> && requires(T t) {
   bool(t);
   {
      *t
   };
};
```

## Raw Pointers

Raw pointers (`T*`) are treated as nullable types in Glaze. They automatically conform to the `nullable_t` concept and have the following behavior:

### JSON Serialization

When serializing standalone pointers:
- **Null pointers** serialize to `null`
- **Non-null pointers** serialize to the pointed-to value

```c++
int* ptr = nullptr;
std::string json;
glz::write_json(ptr, json);  // Results in: "null"

int value = 42;
ptr = &value;
glz::write_json(ptr, json);  // Results in: "42"
```

### Struct Members

When pointers are members of structs, their behavior depends on the `skip_null_members` option:

#### With `skip_null_members = true` (default)

Null pointer members are omitted from the JSON output:

```c++
struct my_struct {
    int* ptr{};
    double value{3.14};
};

my_struct obj;
std::string json;
glz::write_json(obj, json);  // Results in: {"value":3.14}

int x = 10;
obj.ptr = &x;
glz::write_json(obj, json);  // Results in: {"ptr":10,"value":3.14}
```

#### With `skip_null_members = false`

Null pointer members are written as `null`:

```c++
constexpr auto opts = glz::opts{.skip_null_members = false};
my_struct obj;
std::string json;
glz::write<opts>(obj, json);  // Results in: {"ptr":null,"value":3.14}
```

### Automatic Reflection Support

Raw pointers work seamlessly with Glaze's automatic reflection (pure reflection) - no explicit `glz::meta` specialization is required:

```c++
struct example {
    int* int_ptr{};
    std::string* str_ptr{};
    double value{};
};
// No glz::meta needed - automatic reflection handles pointer members correctly
```

### Behavior Consistency

Raw pointers behave consistently with other nullable types like `std::optional` and `std::shared_ptr`:
- They respect the `skip_null_members` option
- Null values can be omitted or written as `null`
- Non-null values serialize to their pointed-to data

> [!WARNING]
>
> When using raw pointers in structs, ensure the pointed-to data remains valid during serialization. Glaze does not manage the lifetime of pointed-to objects.

> [!NOTE]
>
> If you want to read into a nullable type then you must have a `.reset()` method or your type must be assignable from empty braces: `value = {}`. This allows Glaze to reset the value if `"null"` is parsed.

## Nullable Value Types

In some cases a type may be like a `std::optional`, but also require an `operator bool()` that converts to the internal boolean. In these cases you can instead conform to the `nullable_value_t` concept:

```c++
// For optional like types that cannot overload `operator bool()`
template <class T>
concept nullable_value_t = !meta_value_t<T> && requires(T t) {
   t.value();
   {
      t.has_value()
   } -> std::convertible_to<bool>;
};
```
