# Wrappers

Glaze provides a number of wrappers that indicate at compile time how a value should be read and/or written. These wrappers allow you to modify the read/write behavior of a type without affecting your C++ class.

## Available Wrappers

```c++
glz::append_arrays<&T::x> // When reading into an array that is appendable, the new data will be appended rather than overwrite
glz::bools_as_numbers<&T::x> // Read and write booleans as numbers
glz::cast<&T::x, CastType> // Casts a value to and from the CastType, which is parsed/serialized
glz::quoted_num<&T::x> // Read and write numbers as strings
glz::quoted<&T::x> // Read a value as a string and unescape, to avoid the user having to parse twice
glz::number<&T::x> // Read a string as a number and writes the string as a number
glz::raw<&T::x> // Write out string like types without quotes
glz::raw_string<&T::string> // Do not decode/encode escaped characters for strings (improves read/write performance)
glz::escaped<&T::string> // Opposite of glz::raw_string, it turns off this behavior

glz::read_constraint<&T::x, constraint_function, "Message"> // Applies a constraint function when reading
  
glz::partial_read<&T::x> // Reads into only existing fields and elements and then exits without parsing the rest of the input

glz::invoke<&T::func> // Invoke a std::function, lambda, or member function with n-arguments as an array input
  
glz::write_float32<&T::x> // Writes out numbers with a maximum precision of float32_t
glz::write_float64<&T::x> // Writes out numbers with a maximum precision of float64_t
glz::write_float_full<&T::x> // Writes out numbers with full precision (turns off higher level float precision wrappers)

glz::custom<&T::read, &T::write> // Calls custom read and write std::functions, lambdas, or member functions
glz::manage<&T::x, &T::read_x, &T::write_x> // Calls read_x() after reading x and calls write_x() before writing x
glz::as_array<&T::member> // Treat a reflected/member-annotated type as a positional array for read and write
```

## Associated glz::opts

`glz::opts` is the compile time options struct passed to most of Glaze functions to configure read/write behavior. Often wrappers are associated with compile time options and can also be set via `glz::opts`. For example, the `glz::quoted_num` wrapper is associated with the `quoted_num` boolean in `glz::opts`.

## append_arrays

When reading into an array that is appendable, the new data will be appended rather than overwrite

Associated option: `glz::opts{.append_arrays = true};`

```c++
struct append_obj
{
   std::vector<std::string> names{};
   std::vector<std::array<int, 2>> arrays{};
};

template <>
struct glz::meta<append_obj>
{
   using T = append_obj;
   static constexpr auto value = object("names", append_arrays<&T::names>, "arrays", append_arrays<&T::arrays>);
};
```

In use:

```c++
append_obj obj{};
expect(not glz::read_json(obj, R"({"names":["Bob"],"arrays":[[0,0]]})"));
expect(obj.names == std::vector<std::string>{"Bob"});
expect(obj.arrays == std::vector<std::array<int, 2>>{{0,0}});

expect(not glz::read_json(obj, R"({"names":["Liz"],"arrays":[[1,1]]})"));
expect(obj.names == std::vector<std::string>{"Bob", "Liz"});
expect(obj.arrays == std::vector<std::array<int, 2>>{{0,0},{1,1}});
```

## bools_as_numbers

Read and write booleans as numbers

Associated option: `glz::opts{.bools_as_numbers = true};`

```c++
struct bools_as_numbers_struct
{
   bool a{};
   bool b{};
   bool c{};
   bool d{};
   
   struct glaze {
      using T = bools_as_numbers_struct;
      static constexpr auto value = glz::object("a", glz::bools_as_numbers<&T::a>, "b", glz::bools_as_numbers<&T::b>, &T::c, &T::d);
   };
};
```

In use:

```c++
std::string s = R"({"a":1,"b":0,"c":true,"d":false})";
bools_as_numbers_struct obj{};
expect(!glz::read_json(obj, s));
expect(obj.a == true);
expect(obj.b == false);
expect(glz::write_json(obj) == s);
```

### bools_as_numbers from glz::opts

You don't have to use wrappers if you want the global behavior to handle booleans as numbers.

```c++
std::string s = R"([1,0,1,0])";
std::array<bool, 4> obj{};
constexpr glz::opts opts{.bools_as_numbers = true};
expect(!glz::read<opts>(obj, s));
expect(glz::write<opts>(obj) == s);
```

## cast

`glz::cast` is a simple wrapper that will serialize and deserialize the cast type rather than underlying type. This enables the user to parse JSON for a floating point value into an integer, or perform similar `static_cast` behaviors.

```c++
struct cast_obj
{
   int integer{};
};

template <>
struct glz::meta<cast_obj>
{
   using T = cast_obj;
   static constexpr auto value = object("integer", cast<&T::integer, double>);
};
```

In use:

```c++
cast_obj obj{};
std::string buffer = R"({"integer":5.7})";
expect(not glz::read_json(obj, buffer));
expect(obj.integer == 5);
```

When a cast-backed type is used as a key in a map (for example `std::map<MyId, T>`), BEVE now recognises the wrapper and emits the same header as the cast target. This means strong-ID wrappers can be reused across JSON, TOML, and BEVE without specialising custom read/write logic.

## quoted_num

Read and write numbers as strings.

Associated option: `glz::opts{.quoted_num = true};`

```c++
struct foo {
  int x{};
};

template <>
struct glz::meta<foo> {
  using T = foo;
  static constexpr auto value = object("x", quoted_num<&T::x>);
};
```

In use:

```c++
std::string input = R"({ "x": "5" })";
foo obj{};
expect(!glz::read_json(obj, input));
expect(glz::write_json(obj) == R"({ "x": "5" })");
```

> `quoted_num` is more efficient than `quoted` for numbers.

## quoted

When reading, first reads a value as a string, which unescapes, and then reads the value normally. When writing, will first write the value as a string and then write the string to produce escapes.

> `glz::quoted` is useful for storing escaped JSON inside of a higher level JSON object.

```c++
struct client_state
{
   uint64_t id{};
   std::map<std::string, std::vector<std::string>> layouts{};
};

template <>
struct glz::meta<client_state>
{
   using T = client_state;
   static constexpr auto value = object("id", &T::id, "layouts", quoted<&T::layouts>);
};
```

In use:

```c++
client_state obj{};
std::string input = R"({
"id": 4848,
"layouts": "{\"first layout\": [ \"inner1\", \"inner2\" ] }"
})";
expect(!glz::read_json(obj, input));
expect(obj.id == 4848);
expect(obj.layouts.at("first layout") == std::vector<std::string>{"inner1", "inner2"});

std::string out{};
glz::write_json(obj, out);
expect(out == R"({"id":4848,"layouts":"{\"first layout\":[\"inner1\",\"inner2\"]}"})");
```

## as_array

Convert a positional JSON array into an existing struct while writing back out as an array. Use `glz::as_array<&T::member>` when declaring the member in `glz::object`. Handy when a service sends compact arrays but your C++ type is a struct in memory.

```c++
struct Person_details
{
   std::string_view name;
   std::string_view surname;
   std::string_view city;
   std::string_view street;
};

struct Person
{
   int id{};
   Person_details person{};
};

template <>
struct glz::meta<Person>
{
   using T = Person;
   static constexpr auto value = glz::object(
      "id", &T::id,
      "person", glz::as_array<&T::person>
   );
};

std::string payload = R"({
   "id": 1,
   "person": ["Joe", "Doe", "London", "Chamber St"]
})";

Person p{};
expect(!glz::read_json(p, payload));
expect(p.person.city == "London");

auto written = glz::write_json(p).value();
expect(written ==
       R"({"id":1,"person":["Joe","Doe","London","Chamber St"]})"
);
```

## number

Read JSON numbers into strings and write strings as JSON numbers.

Associated option: `glz::opts{.number = true};`

```c++
struct numbers_as_strings
{
   std::string x{};
   std::string y{};
};

template <>
struct glz::meta<numbers_as_strings>
{
   using T = numbers_as_strings;
   static constexpr auto value = object("x", glz::number<&T::x>, "y", glz::number<&T::y>);
};
```

In use:

```c++
std::string input = R"({"x":555,"y":3.14})";
numbers_as_strings obj{};
expect(!glz::read_json(obj, input));
expect(obj.x == "555");
expect(obj.y == "3.14");

std::string output;
glz::write_json(obj, output);
expect(input == output);
```

## raw

Write out string like types without quotes.

> Useful for when a string is already in JSON format and doesn't need to be quoted.

Associated option: `glz::opts{.raw = true};`

```c++
struct raw_struct
{
   std::string str{};
};

template <>
struct glz::meta<raw_struct>
{
   using T = raw_struct;
   static constexpr auto value = object("str", glz::raw<&T::str>);
};
```

In use:

```c++
suite raw_test = [] {
  raw_struct obj{.str = R"("Hello")"};
  // quotes would have been escaped if str were not wrapped with raw
  expect(glz::write_json(obj) == R"({"str":"Hello"})");
};
```

## raw_string

Do not decode/encode escaped characters for strings (improves read/write performance).

> If your code does not care about decoding escaped characters or you know your input will never have escaped characters, this wrapper makes reading/writing that string faster.

Associated option: `glz::opts{.raw_string = true};`

```c++
struct raw_stuff
{
   std::string a{};
   std::string b{};
   std::string c{};

   struct glaze
   {
      using T = raw_stuff;
      static constexpr auto value = glz::object(&T::a, &T::b, &T::c);
   };
};

struct raw_stuff_wrapper
{
   raw_stuff data{};

   struct glaze
   {
      using T = raw_stuff_wrapper;
      static constexpr auto value{glz::raw_string<&T::data>};
   };
};
```

In use:

```c++
raw_stuff_wrapper obj{};
std::string buffer = R"({"a":"Hello\nWorld","b":"Hello World","c":"\tHello\bWorld"})";

expect(!glz::read_json(obj, buffer));
expect(obj.data.a == R"(Hello\nWorld)");
expect(obj.data.b == R"(Hello World)");
expect(obj.data.c == R"(\tHello\bWorld)");

buffer.clear();
glz::write_json(obj, buffer);
expect(buffer == R"({"a":"Hello\nWorld","b":"Hello World","c":"\tHello\bWorld"})");
```

## escaped

The `glz::escaped` wrapper turns off the effects of `glz::raw_string`.

```c++
struct raw_stuff_escaped
{
   raw_stuff data{};

   struct glaze
   {
      using T = raw_stuff_escaped;
      static constexpr auto value{glz::escaped<&T::data>};
   };
};
```

In use:

```c++
raw_stuff_escaped obj{};
std::string buffer = R"({"a":"Hello\nWorld"})";

expect(!glz::read_json(obj, buffer));
expect(obj.data.a ==
       R"(Hello
World)");

buffer.clear();
glz::write_json(obj, buffer);
expect(buffer == R"({"a":"Hello\nWorld","b":"","c":""})");
```

## read_constraint

Enables complex constraints to be defined within a `glz::meta` or using member functions. Parsing is short circuited upon violating a constraint and a nicely formatted error can be produced with a custom error message.

### Field order and optional members

Object members are visited in the order that the JSON input supplies them. This is an intentional design choice so
that input streams do not have to be re-ordered to match the declaration order. Because of this, a
`read_constraint` may only rely on fields that have already appeared in the JSON payload. If you need to validate the
final state of the entire object, use a `self_constraint` as shown below—those run after every field has been read.

Optional members are parsed lazily: if the JSON payload does not contain the key, the member is left untouched and the
corresponding `read_constraint` is not evaluated. This guarantees that absent optional data does not trigger
constraints. Keep in mind that reusing the same C++ object across multiple reads will retain the previous value for any
field that is omitted in later payloads, so reset or re-initialize the instance when you expect fresh state.

```c++
struct constrained_object
{
   int age{};
   std::string name{};
};

template <>
struct glz::meta<constrained_object>
{
   using T = constrained_object;
   static constexpr auto limit_age = [](const T&, int age) { return (age >= 0 && age <= 120); };

   static constexpr auto limit_name = [](const T&, const std::string& name) { return name.size() <= 8; };

   static constexpr auto value = object("age", read_constraint<&T::age, limit_age, "Age out of range">, //
                                        "name", read_constraint<&T::name, limit_name, "Name is too long">);
};
```

### Object level validation

To validate combinations of fields after the object has been fully deserialized, provide a single
`self_constraint` entry. This constraint runs once after all object members have been populated and can therefore
reason about the final state.

```c++
struct cross_constrained
{
   int age{};
   std::string name{};
};

template <>
struct glz::meta<cross_constrained>
{
   using T = cross_constrained;

   static constexpr auto combined = [](const T& v) {
      return ((v.name.starts_with('A') && v.age > 10) || v.age > 5);
   };

   static constexpr auto value = object(&T::age, &T::name);
   static constexpr auto self_constraint = glz::self_constraint<combined, "Age/name combination invalid">;
};
```

You can perform more elaborate business logic as well, such as validating that user credentials are consistent and
secure:

```c++
struct registration_request
{
   std::string username{};
   std::string password{};
   std::string confirm_password{};
   std::optional<std::string> email{};
};

template <>
struct glz::meta<registration_request>
{
   using T = registration_request;

   static constexpr auto strong_credentials = [](const T& value) {
      const bool strong_length = value.password.size() >= 12;
      const bool matches = value.password == value.confirm_password;
      const bool has_username = !value.username.empty();
      return strong_length && matches && has_username;
   };

   static constexpr auto value = object(
      &T::username,
      &T::password,
      &T::confirm_password,
      &T::email);

   static constexpr auto self_constraint = glz::self_constraint<strong_credentials,
      "Password must be at least 12 characters and match confirmation">;
};
```

If a self constraint fails, deserialization stops and `glz::error_code::constraint_violated` is reported with the
associated message.

When it is important that object memory remains valid after every individual assignment—for example, when other code
observes the partially constructed object during parsing—prefer `read_constraint` on the specific members. Those
constraints fire before the member is written, so the in-memory representation never stores an invalid value. In
contrast, `self_constraint` runs after fields are populated, so it can detect issues that span multiple members but the
object may hold the problematic data until the constraint handler reports an error.

## partial_read

Reads into existing object and array elements and then exits without parsing the rest of the input. More documentation concerning `partial_read` can be found in the [Partial Read documentation](./partial-read.md).

> `partial_read` is useful when parsing header information before deciding how to decode the rest of a document. Or, when you only care about the first few elements of an array.

## invoke

Invoke a std::function or member function with n-arguments as an array input.

```c++
struct invoke_struct
{
   int y{};
   std::function<void(int x)> square{};
   void add_one() { ++y; }

   // MSVC requires this constructor for 'this' to be captured
   invoke_struct()
   {
      square = [&](int x) { y = x * x; };
   }
};

template <>
struct glz::meta<invoke_struct>
{
   using T = invoke_struct;
   static constexpr auto value = object("square", invoke<&T::square>, "add_one", invoke<&T::add_one>);
};
```

In use:

```c++
std::string s = R"(
{
	"square":[5],
	"add_one":[]
})";
invoke_struct obj{};
expect(!glz::read_json(obj, s));
expect(obj.y == 26); // 5 * 5 + 1
};
```

## write_float32

Writes out numbers with a maximum precision of `float32_t`.

```c++
struct write_precision_t
{
   double pi = std::numbers::pi_v<double>;

   struct glaze
   {
      using T = write_precision_t;
      static constexpr auto value = glz::object("pi", glz::write_float32<&T::pi>);
   };
};
```

> [!IMPORTANT]
>
> The `glz::float_precision float_max_write_precision` is not a core option in `glz::opts`. You must create an options structure that adds this field to enable float precision control. The example below shows this user defined options struct that inherits from `glz::opts`.

In use:

```c++
struct float_opts : glz::opts {
   glz::float_precision float_max_write_precision{};
};

write_precision_t obj{};
std::string json_float = glz::write<float_opts{}>(obj);
expect(json_float == R"({"pi":3.1415927})") << json_float;
```

## write_float64

Writes out numbers with a maximum precision of `float64_t`.

## write_float_full

Writes out numbers with full precision  (turns off higher level float precision wrappers).

## Associated glz::opts for float precision

```c++
enum struct float_precision : uint8_t { full, float32 = 4, float64 = 8, float128 = 16 };
```

glz::opts

```c++
// The maximum precision type used for writing floats, higher precision floats will be cast down to this precision
float_precision float_max_write_precision{};
```

## custom

Calls custom read and write std::functions, lambdas, or member functions.

```c++
struct custom_encoding
{
   uint64_t x{};
   std::string y{};
   std::array<uint32_t, 3> z{};

   void read_x(const std::string& s) { x = std::stoi(s); }

   uint64_t write_x() { return x; }

   void read_y(const std::string& s) { y = "hello" + s; }

   auto& write_z()
   {
      z[0] = 5;
      return z;
   }
};

template <>
struct glz::meta<custom_encoding>
{
   using T = custom_encoding;
   static constexpr auto value = object("x", custom<&T::read_x, &T::write_x>, //
                                        "y", custom<&T::read_y, &T::y>, //
                                        "z", custom<&T::z, &T::write_z>);
};
```

In use:

```c++
"custom_reading"_test = [] {
  custom_encoding obj{};
  std::string s = R"({"x":"3","y":"world","z":[1,2,3]})";
  expect(!glz::read_json(obj, s));
  expect(obj.x == 3);
  expect(obj.y == "helloworld");
  expect(obj.z == std::array<uint32_t, 3>{1, 2, 3});
};

"custom_writing"_test = [] {
  custom_encoding obj{};
  std::string s = R"({"x":"3","y":"world","z":[1,2,3]})";
  expect(!glz::read_json(obj, s));
  std::string out{};
  glz::write_json(obj, out);
  expect(out == R"({"x":3,"y":"helloworld","z":[5,2,3]})");
};
```

### Another custom example

Showing use of constexpr lambdas for customization.

```c++
struct custom_buffer_input
{
   std::string str{};
};

template <>
struct glz::meta<custom_buffer_input>
{
   static constexpr auto read_x = [](custom_buffer_input& s, const std::string& input) { s.str = input; };
   static constexpr auto write_x = [](auto& s) -> auto& { return s.str; };
   static constexpr auto value = glz::object("str", glz::custom<read_x, write_x>);
};
```

In use:

```c++
std::string s = R"({"str":"Hello!"})";
custom_buffer_input obj{};
expect(!glz::read_json(obj, s));
expect(obj.str == "Hello!");
s.clear();
glz::write_json(obj, s);
expect(s == R"({"str":"Hello!"})");
expect(obj.str == "Hello!");
```

> [!NOTE]
>
> With read lambdas like `[](custom_buffer_input& s, const std::string& input)`, both types must be concrete (cannot use `auto`), otherwise you'll get a compilation error noting this. The reason is that Glaze must be able to determine what type to decode into before passing the decoded value to `input`.

## manage

Calls a read function after reading and calls a write function before writing.

> `glz::manage` is useful for transforming state from a user facing format into a more complex or esoteric internal format.

```c++
struct manage_x
{
   std::vector<int> x{};
   std::vector<int> y{};

   bool read_x()
   {
      y = x;
      return true;
   }

   bool write_x()
   {
      x = y;
      return true;
   }
};

template <>
struct glz::meta<manage_x>
{
   using T = manage_x;
   static constexpr auto value = object("x", manage<&T::x, &T::read_x, &T::write_x>);
};
```

In use:

```c++
manage_x obj{};
std::string s = R"({"x":[1,2,3]})";
expect(!glz::read_json(obj, s));
expect(obj.y[0] == 1);
expect(obj.y[1] == 2);
obj.x.clear();
s.clear();
glz::write_json(obj, s);
expect(s == R"({"x":[1,2,3]})");
expect(obj.x[0] == 1);
expect(obj.x[1] == 2);
```

### Another manage example

```c++
struct manage_x_lambda
{
   std::vector<int> x{};
   std::vector<int> y{};
};

template <>
struct glz::meta<manage_x_lambda>
{
   using T = manage_x_lambda;
   static constexpr auto read_x = [](auto& s) {
      s.y = s.x;
      return true;
   };
   static constexpr auto write_x = [](auto& s) {
      s.x = s.y;
      return true;
   };
   [[maybe_unused]] static constexpr auto value = object("x", manage<&T::x, read_x, write_x>);
};
```

In use:

```c++
manage_x_lambda obj{};
std::string s = R"({"x":[1,2,3]})";
expect(!glz::read_json(obj, s));
expect(obj.y[0] == 1);
expect(obj.y[1] == 2);
obj.x.clear();
s.clear();
glz::write_json(obj, s);
expect(s == R"({"x":[1,2,3]})");
expect(obj.x[0] == 1);
expect(obj.x[1] == 2);
```
