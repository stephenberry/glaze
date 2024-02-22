# Struct Registration Macros (REMOVED)

GLZ_META, GLZ_LOCAL_META, and associated macros have been removed.

> If you desperately want them, you can go to an older version of Glaze (v2.1.6 and older) and copy the macros and add them to your codebase. But, we are moving away from macros in the API for the sake of C++20 modules and better code overall.

Glaze supports reflection for aggregate initializable structs, and for member object pointers. So,
these macros no longer save typing and have much less flexibility. More reflection is coming as well.
