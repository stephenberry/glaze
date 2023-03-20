# Tagged Binary Messages (Crusher)

Glaze provides a tagged binary format to send and receive messages much like JSON, but with significantly improved performance and message size savings.

The binary specification is known as [Crusher](https://github.com/stephenberry/crusher).

Integers and integer keys are locally compressed for efficiency. Elements are byte aligned, but size headers uses bit packing where the benefits are greatest and performance costs are low.

Most classes use `std::memcpy` for maximum performance.

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

## Binary Arrays

Arrays of compile time known size, e.g. `std::array`, do not include the size (number of elements) with the message. This is to enable minimal binary size if required. Dynamic types, such as `std::vector`, include the number of elements. *This means that statically sized arrays and dynamically sized arrays cannot be intermixed across implementations.*

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

# 
