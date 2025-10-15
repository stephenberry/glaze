# Binary Format (BEVE)

Glaze provides a binary format to send and receive messages like JSON, but with significantly improved performance and message size savings.

The binary specification is known as [BEVE](https://github.com/beve-org/beve).

**Write BEVE**

```c++
my_struct s{};
std::vector<std::byte> buffer{};
glz::write_beve(s, buffer);
```

**Read BEVE**

```c++
my_struct s{};
glz::read_beve(s, buffer);
```

> [!NOTE]
>
> Reading binary is safe for invalid input and does not require null terminated buffers.

## Untagged Binary

By default Glaze will handle structs as tagged objects, meaning that keys will be written/read. However, structs can be written/read without tags by using the option `structs_as_arrays` or the functions `glz::write_beve_untagged` and `glz::read_beve_untagged`.

## BEVE to JSON Conversion

`glaze/binary/beve_to_json.hpp` provides `glz::beve_to_json`, which directly converts a buffer of BEVE data to a buffer of JSON data.

### Member Function Pointers

Objects that expose member function pointers through `glz::meta` are skipped by the BEVE writer by default. This mirrors JSON/TOML behaviour and avoids emitting unusable callable placeholders in binary payloads.

If you want the key present, use `write_member_functions = true`.

## Custom Map Keys

BEVE can serialize map-like containers whose key types expose a value through Glaze metadata. This allows “strong ID” wrappers to keep a user-defined type while the binary payload stores the underlying numeric representation.

```c++
struct ModuleID {
   uint64_t value{};
   auto operator<=>(const ModuleID&) const = default;
};

template <>
struct glz::meta<ModuleID> {
   static constexpr auto value = &ModuleID::value;
};

std::map<ModuleID, std::string> modules{{ModuleID{42}, "life"}, {ModuleID{9001}, "power"}};

std::string beve{};
glz::write_beve(modules, beve);
```

Glaze inspects the metadata, reuses the underlying `uint64_t`, and emits the numeric BEVE map header so the payload decodes as a regular number key. The same behaviour works for `std::unordered_map` and concatenated ranges such as `std::vector<std::pair<ModuleID, T>>`.

If you prefer to keep a custom conversion in your metadata, `glz::cast` works as well:

```c++
template <>
struct glz::meta<ModuleID> {
   static constexpr auto value = glz::cast<&ModuleID::value, uint64_t>;
};
```

## Partial Objects

It is sometimes desirable to write out only a portion of an object. This is permitted via an array of JSON pointers, which indicate which parts of the object should be written out.

```c++
static constexpr auto partial = glz::json_ptrs("/i",
                                               "/d",
                                               "/sub/x",
                                               "/sub/y");
std::vector<std::byte> out;
glz::write_beve<partial>(s, out);
```
