# Reflection in Glaze

Glaze provides compile time reflection utilities for C++.

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

