# Struct Registration Macros

Since version 1.8.1 reflection for member object pointers has been supported. So, these macros no longer save much typing and have much less flexibility. They will be deprecated in the future.

**Macros must be explicitly included via: `#include "glaze/core/macros.hpp"`**

- GLZ_META is for external registration
- GLZ_LOCAL_META is for internal registration

```c++
struct macro_t {
   double x = 5.0;
   std::string y = "yay!";
   int z = 55;
};

GLZ_META(macro_t, x, y, z);

struct local_macro_t {
   double x = 5.0;
   std::string y = "yay!";
   int z = 55;
   
   GLZ_LOCAL_META(local_macro_t, x, y, z);
};
```

Instead of these macros, write:

```c++
struct macro_t {
   double x = 5.0;
   std::string y = "yay!";
   int z = 55;
};

template <>
struct glz::meta<macro_t> {
  using T = macro_t;
  static constexpr auto value = object(&T::x, &T::y, &T::z);
};

struct local_macro_t {
   double x = 5.0;
   std::string y = "yay!";
   int z = 55;
   
   struct glaze {
     using T = local_macro_t;
     static constexpr auto value = glz::object(&T::x, &T::y, &T::z);
   };
};
```

> Yes, this is more typing, but it allows far more flexibility and avoids the numerous problems with macros.
