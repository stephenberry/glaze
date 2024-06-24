# Directory Serialization and Deserialization

Glaze supports reading and writing from an entire directory of files into a map like object.

`#include "glaze/glaze.hpp"` or specifically include: 

- `#include "glaze/file/read_directory.hpp"` for read support
- `#include "glaze/file/write_directory.hpp"` for write support

The key to the map is the path to the individual file and the value is any C++ type.

The code below demonstrates writing out a map of two file paths and associated structs to two separate files. We then read the created directory into a default structure of the same form.

```c++
std::map<std::filesystem::path, my_struct> files{{"./dir/alpha.json", {}}, {"./dir/beta.json", {.i = 0}}};
expect(not glz::write_directory(files, "./dir"));

std::map<std::filesystem::path, my_struct> input{};
expect(not glz::read_directory(input, "./dir"));
expect(input.size() == 2);
expect(input.contains("./dir/alpha.json"));
expect(input.contains("./dir/beta.json"));
expect(input["./dir/beta.json"].i == 0);
```
