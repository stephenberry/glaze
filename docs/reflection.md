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

