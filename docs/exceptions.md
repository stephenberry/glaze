# Glaze Exceptions

If C++ exceptions are being used in your codebase, it can be cleaner to call functions that throw.

Glaze provides a header `glaze_exceptions.hpp`, which provides an API to use Glaze with exceptions. This header will be disabled if `-fno-exceptions` is specified.

> Glaze functions that throw exceptions use the namespace `glz::ex`

```c++
std::string buffer{};
glz::ex::read_json(obj, buffer); // will throw on a read error
```

