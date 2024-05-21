# Wrappers

Glaze provides a number of wrappers that indicate at compile time how a value should be read and/or written. These wrappers allow you to modify the read/write behavior of a type without affecting your C++ class.

Example:

Say we have the following class, but we want to read and write the number as a JSON string.

```c++
struct foo {
  int x = 5;
};
```

We use a wrapper in our `glz::meta` to indicate this string handling.

```c++
template <>
struct glz::meta<foo> {
  using T = foo;
  static constexpr auto value = object("x", quoted_num<&T::x>);
};
```

Now our input and output for `foo` will look like:

```json
{ "x": "5" }
```

> Because wrappers are known at compile time it means this behavior can be highly optimized and does not add much overhead to the default behavior in Glaze.

## Available Wrappers

```c++
glz::quoted_num<&T::x> // reads a number as a string and writes it as a string
glz::quoted<&T::x> // reads a value as a string and unescapes, to avoid the user having to parse twice
glz::number<&T::x> // reads a string as a number and writes the string as a number
glz::invoke<&T::func> // invokes a std::function or member function with n-arguments as an array input
glz::custom<&T::read, &T::write> // calls custom read and write std::functions or member functions
glz::manage<&T::x, &T::read_x, &T::write_x> // calls read_x() after reading x and calls write_x() before writing x
glz::raw<&T::x> // write out string like types without quotes
glz::raw_string<&T::string> // do not decode/encode escaped characters for strings (improves read/write performance)

glz::write_float32<&T::x> // writes out numbers with a maximum precision of float32_t
glz::write_float64<&T::x> // writes out numbers with a maximum precision of float64_t
glz::write_float_full<&T::x> // writes out numbers with full precision (turns off higher level float precision wrappers)
```

