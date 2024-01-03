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

## Untagged Binary

By default Glaze will handle structs as tagged objects, meaning that keys will be written/read. However, structs can be written/read without tags by using the option `structs_as_arrays` or the functions `glz::write_binary_untagged` and `glz::read_binary_untagged`.

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
