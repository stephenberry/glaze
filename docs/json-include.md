# JSON Include System

When using JSON for configuration files it can be helpful to move object definitions into separate files. This reduces copying and the need to change inputs across multiple files.

Glaze provides a `glz::file_include` type that can be added to your struct (or glz::meta). The key may be anything, in this example we use choose `include` to mimic C++.

```c++
struct includer_struct {
  glz::file_include include{};
  std::string str = "Hello";
  int i = 55;
};
```

When this object is parsed, when the key `include` is encountered the associated file will be read into the local object.

```c++
includer_struct obj{};
std::string s = R"({"include": "./obj.json", "i": 100})";
glz::read_json(obj, s);
```

This will read the `./obj.json` file into the `obj` as it is parsed. Since glaze parses in sequence, the order in which includes are listed in the JSON file is the order in which they will be evaluated. The `file_include` will only be read into the local object, so includes can be deeply nested.

> Paths are always relative to the location of the previously loaded file. For nested includes this means the user only needs to consider the relative path to the file in which the include is written.

## Hostname Include

Similar to `glz::file_include`, glaze provides `glz::hostname_include`. This is used for host-specific configuration files.

In use:

```c++
std::string s = R"({"hostname_include": "../{}_config.json", "i": 100})";
auto ec = glz::read_json(obj, s);
```

Note how the provided path contains `{}`, which will be replaced with the hostname when the file is read. Other than this, the `hostname_include` behaves the same as `file_include`.
