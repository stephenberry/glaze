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

