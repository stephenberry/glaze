# Struct Registration Macros

Glaze supports reflection for aggregate initializable structs, and for member object pointers. So, these macros no longer save typing and have much less flexibility. They will be deprecated in the future.

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

Instead of these macros don't write anything :)
