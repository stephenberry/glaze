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

## Missing Keys

An included file is a fragment of the object that names it: some of the object's keys come from the include and the rest come from the including document. With `error_on_missing_keys` the check is therefore made by the including object over the union of both documents, so the example above reads without error even though neither document lists every key on its own. Keys that appear in neither document are still reported, as are missing keys of objects nested inside an included file.

## Hostname Include

Similar to `glz::file_include`, glaze provides `glz::hostname_include`. This is used for host-specific configuration files.

> You must explicitly include the header file `#include "glaze/file/hostname_include.hpp"`

In use:

```c++
std::string s = R"({"hostname_include": "../{}_config.json", "i": 100})";
auto ec = glz::read_json(obj, s);
```

Note how the provided path contains `{}`, which will be replaced with the hostname when the file is read. Other than this, the `hostname_include` behaves the same as `file_include`.

## glz::raw_or_file

`glz::raw_or_file` is somewhat like a `glz::file_include`, but also handles serialization. The purpose is to allow JSON files to be optionally referred to within higher level configuration files, but once loaded, the files behave as glz::raw_json, where the format does not need to be understood until deserialized.

> Note that his approach only allows a single layer of includes. It does not allow infinitely deep includes like `glz::file_include` and `glz::hostname_include` support.

**How glz::raw_or_file behaves:**
If the input is a valid file path within a string, the entire file will be read into a std::string within the glz::raw_or_file member. The internal buffer does not escape characters, as it is handled like glz::raw_json.
If the input is not a valid file path or not a string, the value is simply validated and stored like glz::raw_json
When serialized, the value is dumped like glz::raw_json, which means it will look like the input file was copied and pasted into the higher level document.

Benefits of this approach include:

- The layout for the file loaded into glz::raw_or_file does not need to be known by the program. As long as it is valid JSON it can be passed along.
- The serialized output after loading the file looks like a combined file, which is easier to read and format than an escaped string version of the file. It is also more efficient because escaping/unescaping doesn't need to be performed, only validation.
