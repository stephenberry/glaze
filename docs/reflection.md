# Reflection in Glaze

Glaze provides compile time reflection utilities for C++.

Most aggregate structs require no metadata at all, but if you later decide a couple of keys should be
renamed or exposed under additional aliases you can layer those tweaks on with
[`glz::meta<T>::modify`](modify-reflection.md). Pure reflection continues to handle the untouched members while
the `modify` entries supply the bespoke names.

```c++
struct T
{
  int a{};
  std::string str{};
  std::array<float, 3> arr{};
};

static_assert(glz::refl<T>::size == 3);
static_assert(glz::refl<T>::keys[0] == "a");
```

## Reflection Concepts

### `reflectable<T>`

The `reflectable<T>` concept identifies aggregate types that can be automatically reflected without requiring a `glz::meta` specialization:

```c++
struct SimpleStruct {
    int x;
    double y;
};

static_assert(glz::reflectable<SimpleStruct>); // true - aggregate type
```

### `has_reflect<T>` 

The `has_reflect<T>` concept detects whether `glz::reflect<T>` can be instantiated for a given type. It automatically captures **all** types that have a valid `reflect<T>` specialization, including:
- Types satisfying `reflectable<T>` (aggregate types)
- Types with `glz::meta` specializations (`glaze_object_t`, `glaze_array_t`, `glaze_enum_t`, etc.)
- Readable map types like `std::map` and `std::unordered_map`
- Any future types that get `reflect<T>` specializations

```c++
// Aggregate struct - both reflectable and has_reflect
struct Aggregate {
    int value;
};

// Struct with glz::meta - NOT reflectable but has_reflect
struct WithMeta {
    int x;
    double y;
};

template <>
struct glz::meta<WithMeta> {
    using T = WithMeta;
    static constexpr auto value = glz::object(&T::x, &T::y);
};

static_assert(glz::reflectable<Aggregate>);  // true
static_assert(glz::has_reflect<Aggregate>);  // true

static_assert(!glz::reflectable<WithMeta>);  // false - has meta specialization
static_assert(glz::has_reflect<WithMeta>);   // true - can use reflect<T>

// Both can safely use reflect<T>
constexpr auto aggregate_size = glz::reflect<Aggregate>::size;  // Works!
constexpr auto meta_size = glz::reflect<WithMeta>::size;        // Works!
```

### When to use each concept

- Use `reflectable<T>` when you specifically need aggregate types without `glz::meta` specializations
- Use `has_reflect<T>` when you want to check if `glz::reflect<T>` can be safely called
- Use `has_reflect<T>` in generic code that needs to work with any type that supports reflection

The `has_reflect<T>` concept is implemented by checking if `reflect<T>` can be instantiated, making it automatically stay in sync with all `reflect<T>` specializations

```c++
// Generic function that works with any reflectable type
template<glz::has_reflect T>
void print_field_count(const T& obj) {
    std::cout << "Type has " << glz::reflect<T>::size << " fields\n";
}

// Works with both aggregate types and types with glz::meta
print_field_count(Aggregate{});  // Works
print_field_count(WithMeta{});   // Also works
```

## glz::convert_struct

The `glz::convert_struct` function show a simple application of Glaze reflection at work. It allows two different structs with the same member names to convert from one to the other. If any of the fields don't have matching names, a compile time error will be generated.

```c++
struct a_type
{
   float fluff = 1.1f;
   int goo = 1;
   std::string stub = "a";
};

struct b_type
{
   float fluff = 2.2f;
   int goo = 2;
   std::string stub = "b";
};

struct c_type
{
   std::optional<float> fluff = 3.3f;
   std::optional<int> goo = 3;
   std::optional<std::string> stub = "c";
};

suite convert_tests = [] {
   "convert a to b"_test = [] {
      a_type in{};
      b_type out{};

      glz::convert_struct(in, out);

      expect(out.fluff == 1.1f);
      expect(out.goo == 1);
      expect(out.stub == "a");
   };

   "convert a to c"_test = [] {
      a_type in{};
      c_type out{};

      glz::convert_struct(in, out);

      expect(out.fluff.value() == 1.1f);
      expect(out.goo.value() == 1);
      expect(out.stub.value() == "a");
   };

   "convert c to a"_test = [] {
      c_type in{};
      a_type out{};

      glz::convert_struct(in, out);

      expect(out.fluff == 3.3f);
      expect(out.goo == 3);
      expect(out.stub == "c");
   };
};
```
