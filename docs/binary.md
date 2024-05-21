# Binary Format (BEVE)

Glaze provides a binary format to send and receive messages like JSON, but with significantly improved performance and message size savings.

The binary specification is known as [BEVE](https://github.com/stephenberry/beve).

**Write Binary**

```c++
my_struct s{};
std::vector<std::byte> buffer{};
glz::write_binary(s, buffer);
```

**Read Binary**

```c++
my_struct s{};
glz::read_binary(s, buffer);
```

> [!WARNING]
>
> Reading binary has few checks for valid input. This is intentional for maximum performance, as safety can be achieved through commonly used mechanisms.
>
>  Binary format errors may occur if data is incorrectly written, corrupted, or maliciously manipulated.
>
> - Do not write binary by hand, to ensure valid formatting.
> - Use protocols like TCP or other checksum methods to ensure data is not corrupted.
> - Use proper cryptographic solutions where malicious attacks are possible.
>
> Glaze does include some validation for binary input, but this should be seen as a final line of defense.

> [!IMPORTANT]
>
> Glaze will be adding a fully checked binary (BEVE) parsing option, but this does not currently exist. So, do not use BEVE for open APIs where users could send corrupt/invalid input with the current codebase.

## Untagged Binary

By default Glaze will handle structs as tagged objects, meaning that keys will be written/read. However, structs can be written/read without tags by using the option `structs_as_arrays` or the functions `glz::write_binary_untagged` and `glz::read_binary_untagged`.

## BEVE to JSON Conversion

`glaze/binary/beve_to_json.hpp` provides `glz::beve_to_json`, which directly converts a buffer of BEVE data to a buffer of JSON data.

## Partial Objects

It is sometimes desirable to write out only a portion of an object. This is permitted via an array of JSON pointers, which indicate which parts of the object should be written out.

```c++
static constexpr auto partial = glz::json_ptrs("/i",
                                               "/d",
                                               "/sub/x",
                                               "/sub/y");
std::vector<std::byte> out;
glz::write_binary<partial>(s, out);
```
