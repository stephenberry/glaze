# Volatile Support

Glaze supports the `volatile` qualifier, which is especially useful when interacting with hardware registers.

Glaze also provides `glz::volatile_array` in `glaze/hardware/volatile_array.hpp`. This class provides the same API as `std::array`, but with volatile memory and methods.

Example struct definition using automatic reflection:

```c++
struct my_struct
{
   glz::volatile_array<uint16_t, 4> a{};
   bool b{};
   int32_t c{};
   double d{};
   uint32_t e{};
};
```

Example unit tests:

```c++
volatile my_struct obj{{1, 2, 3, 4}, true, -7, 9.9, 12};
std::string s{};
glz::write_json(obj, s);
expect(s == R"({"a":[1,2,3,4],"b":true,"c":-7,"d":9.9,"e":12})") << s;

obj.a.fill(0);
obj.b = false;
obj.c = 0;
obj.d = 0.0;
obj.e = 0;

expect(!glz::read_json(obj, s));
expect(obj.a == glz::volatile_array<uint16_t, 4>{1, 2, 3, 4});
expect(obj.b == true);
expect(obj.c == -7);
expect(obj.d == 9.9);
expect(obj.e == 12);
```


## Arrays of class elements

The array elements may themselves be class types, which is the usual shape of a register block built from N identical sub-blocks:

```c++
struct channel_t
{
   uint32_t control{};
   uint32_t status{};
};

struct block_t
{
   uint32_t enable{};
   glz::volatile_array<channel_t, 4> channels{};
};
```

```c++
volatile block_t block{};
block.enable = 1;
block.channels[0].control = 7;

std::string s{};
glz::write_json(block, s);
// {"enable":1,"channels":[{"control":7,"status":0}, ...]}

expect(!glz::read_json(block, s));
expect(glz::get<volatile uint32_t>(block, "/channels/0/control") == uint32_t(7));
```

These arrays are traversed by index rather than by iterator: `volatile_array<T, N>::begin()` returns a `volatile T*`, which is not a `std::input_iterator` when `T` is a class type, because `std::indirectly_readable` requires `T` be constructible from `volatile T&` and an implicit copy constructor takes `const T&`. Scalar elements copy through a built-in conversion instead, so they keep taking the ordinary iterable path and their encoding is unchanged.

A plain C array of class type inside a `volatile` struct works the same way:

```c++
struct block_t
{
   channel_t channels[4]{};
};

template <>
struct glz::meta<block_t>
{
   static constexpr auto value = object(&block_t::channels);
};
```

Note the explicit `glz::meta`: pure reflection cannot handle an aggregate containing a C array, with or without `volatile`, because the member-count probe counts the array's elements individually. Use `glz::volatile_array` (or `std::array`) to keep reflection working.
