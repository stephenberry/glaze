# Unknown Keys

Sometimes you don't know in advance all the keys of your objects. By default, glaze will error on unknown keys.
If you set the compile time option `error_on_unknown_keys = false`, the behavior will change to skip the unknown keys.

Furthermore it is possible to customize the handling of object unknown keys at read and/or write time by defining `unknown_read` and `unknown_write` members of meta class as pointer to member or pointer to method.

Note : `unknown_write` is using `glz::merge` internally so unknown keys will be written at the end of object.

## Example

```c++
struct my_struct
{
  std::string hello;
  std::string zzz;
  std::map<glz::sv, glz::raw_json> extra; // store key/raw_json in extra map

  // with methods
  // void my_unknown_read(const glz::sv& key, const glz::raw_json& value) { 
  //     extra[key] = value;
  // };
  // std::map<glz::sv, glz::raw_json> my_unknown_write() const {
  //    return extra;
  // }

};

template <>
struct glz::meta<my_struct> {
   using T = my_struct;
   static constexpr auto value = object(
      "hello", &T::hello,
      "zzz", &T::zzz
   );
   
   // with members
   static constexpr auto unknown_write{&T::extra};
   static constexpr auto unknown_read{&T::extra};
   // with methods
   // static constexpr auto unknown_write{&T::my_unknown_write};
   // static constexpr auto unknown_read{&T::my_unknown_read};

};

// usage
std::string buffer = R"({"hello":"Hello World!","lang":"en","zzz":"zzz"})";
my_struct s{};

// decodes and retains extra unknown fields (lang) in extra map
glz::context ctx{};
glz::read<glz::opts{.error_on_unknown_keys = false}>(s, buffer, ctx);

// update
s.hello = "Hi !"

// encodes output string
std::string out{};
glz::write_json(s, out);

// out ==  {"hello":"Hi !","zzz":"zzz","lang":"en"}
```

## Example 2 : Known extra type

If the type of extra keys is known, this would work

```c++
struct my_struct
{
  std::string infinite;
  int zero;
  std::map<glz::sv, int> numbers;
};

template <>
struct glz::meta<my_struct> {
   using T = my_struct;
   static constexpr auto value = object(
      "infinite", &T::infinite,
      "zero", &T::zero
   );
   
   static constexpr auto unknown_read{&T::extra};
};

// usage
// note extra keys (one, two) are of type int
std::string buffer = R"({"inf":"infinite","zero":0,"one":1,"two":2})";
my_struct s{};

// decodes and retains extra unknown fields (lang) in extra map
glz::context ctx{};
glz::read<glz::opts{.error_on_unknown_keys = false}>(s, buffer, ctx);
```


