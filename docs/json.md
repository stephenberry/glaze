# JSON Handling in Glaze

Glaze is one of the fastest JSON libraries in the world, offering automatic reflection and direct memory serialization/deserialization with incredible performance. Glaze makes JSON handling in C++ simple, fast, and type-safe. Start with basic automatic reflection and gradually adopt advanced features as your needs grow!

### Basic Example

With Glaze, your structs automatically get JSON serialization without any metadata:

```cpp
#include "glaze/glaze.hpp"

struct Person {
    std::string name = "John Doe";
    int age = 30;
    std::vector<std::string> hobbies = {"reading", "gaming"};
};

int main() {
    Person person{};
    
    // Write to JSON
    std::string buffer{};
    auto ec = glz::write_json(person, buffer);
    if (!ec) {
        // buffer now contains: {"name":"John Doe","age":30,"hobbies":["reading","gaming"]}
    }
    
    // Read from JSON
    std::string json_data = R"({"name":"Jane","age":25,"hobbies":["hiking","cooking"]})";
    Person jane{};
    ec = glz::read_json(jane, json_data);
    if (!ec) {
        // jane.name is now "Jane", jane.age is 25, etc.
    }
    
    return 0;
}
```

### Alternative API Styles

Glaze offers multiple ways to read and write JSON:

**Writing JSON:**
```cpp
Person person{};

// Primary method: Write into existing buffer
std::string buffer{};
auto ec = glz::write_json(person, buffer);
if (!ec) {
    // buffer contains the JSON string
}

// Alternative: Function returns std::expected<std::string, glz::error_ctx>
std::string json = glz::write_json(person).value_or("error");
```

**Reading JSON:**
```cpp
std::string json_data = R"({"name":"Alice","age":28})";

// Primary method: Read into existing object
Person person{};
auto ec = glz::read_json(person, json_data);
if (!ec) {
    // person is now populated from json_data
}

// Alternative: Function returns std::expected<T, glz::error_ctx>
auto result = glz::read_json<Person>(json_data);
if (result) {
    const Person& person = result.value();
}
```

### File I/O

```cpp
Person person{};

// Read from file
auto ec = glz::read_file_json(person, "./person.json", std::string{});
if (!ec) {
    // person is now populated from the file
}

// Write to file  
auto ec = glz::write_file_json(person, "./person.json", std::string{});
if (!ec) {
    // person data has been written to the file
}
```

## Automatic Reflection

Glaze uses compile-time reflection to automatically serialize your structs. No macros, no manual registration - it just works!

**Supported out of the box:**
- All aggregate-initializable structs
- Standard library containers
- Primitive types
- Nested objects

```cpp
struct Address {
    std::string street;
    std::string city;
    int zip_code;
};

struct Employee {
    std::string name;
    int employee_id;
    Address address;
    std::optional<std::string> department;
    std::map<std::string, int> skills_rating;
};

// This struct automatically works with JSON - no additional code needed!
```

## Manual Metadata (When Needed)

For non-aggregate structs, private members, or custom behavior, you can provide explicit metadata:

```cpp
struct CustomStruct {
private:
    int value;
    std::string name;
    
    friend struct glz::meta<CustomStruct>; // friend only needed to access private members
    
public:
    CustomStruct(int v, const std::string& n) : value(v), name(n) {}
};

template <>
struct glz::meta<CustomStruct> {
    using T = CustomStruct;
    static constexpr auto value = glz::object(
        &T::value, // will use the key "value" automatically
        &T::name // will use the key "name" automatically
    );
};
```

### Custom Field Names

You can override the default field names:

```cpp
template <>
struct glz::meta<Person> {
    using T = Person;
    static constexpr auto value = glz::object(
        "full_name", &T::name,
        "years_old", &T::age,
        "interests", &T::hobbies
    );
};

// JSON output: {"full_name":"John","years_old":30,"interests":["reading"]}
```

## Error Handling

Glaze provides comprehensive error handling with detailed error messages:

```cpp
std::string invalid_json = R"({"name":"John", "age":})"; // Missing value

Person person{};
auto ec = glz::read_json(person, invalid_json);
if (ec) {
    // Get a detailed error message
    std::string error_msg = glz::format_error(ec, invalid_json);
    std::cout << error_msg << '\n';
}
```

### Input Buffer Requirements

For optimal performance, use null-terminated buffers (like `std::string`):

```cpp
// Recommended - null terminated
std::string json_data = "...";
Person person{};
auto ec = glz::read_json(person, json_data);

// If you must use non-null terminated buffers
constexpr auto options = glz::opts{.null_terminated = false};
auto ec = glz::read<options>(person, buffer);
```

## Type Support

### Containers and Arrays

All standard C++ containers are supported, and if your containers match C++ standard APIs they will work as well:

```cpp
std::vector<int> numbers = {1, 2, 3, 4, 5};
std::array<std::string, 3> colors = {"red", "green", "blue"};
std::set<int> unique_numbers = {1, 2, 3};
std::deque<double> values = {1.1, 2.2, 3.3};

// All serialize to JSON arrays: [1,2,3,4,5], ["red","green","blue"], etc.
```

### Maps and Objects

```cpp
std::map<std::string, int> scores = {{"Alice", 95}, {"Bob", 87}};
std::unordered_map<std::string, std::string> config = {{"host", "localhost"}};

// Serialize to JSON objects: {"Alice":95,"Bob":87}
```

### Optional and Nullable Types

```cpp
struct OptionalData {
    std::optional<std::string> nickname;
    std::unique_ptr<int> priority;
    std::shared_ptr<std::vector<int>> tags;
};

// null values are written as JSON null, valid values are serialized normally
```

### Enums

Enums can be serialized as integers (default) or strings:

```cpp
enum class Status { Active, Inactive, Pending };

// Default: serialize as integers (0, 1, 2)
Status status = Status::Active;

// For string serialization, add metadata:
template <>
struct glz::meta<Status> {
    using enum Status;
    static constexpr auto value = glz::enumerate(
        Active,
        Inactive, 
        Pending
    );
};
// Now serializes as: "Active", "Inactive", "Pending"
```

### Variants

```cpp
std::variant<int, std::string, double> data = "hello";
// Glaze automatically handles variant serialization
```

## Advanced Features

### JSON with Comments (JSONC)

Read JSON files with comments:

```cpp
std::string jsonc_data = R"({
    "name": "John", // Person's name
    /* Multi-line comment
       about age */
    "age": 30
})";

Person person{};
auto ec = glz::read_jsonc(person, jsonc_data);
// Or: glz::read<glz::opts{.comments = true}>(person, jsonc_data);
```

### Pretty Printing

```cpp
Person person{};

// Write prettified JSON directly
std::string buffer{};
auto ec = glz::write<glz::opts{.prettify = true}>(person, buffer);

// Or prettify existing JSON
std::string minified = R"({"name":"John","age":30})";
std::string pretty = glz::prettify_json(minified);
```

Output:
```json
{
   "name": "John",
   "age": 30
}
```

### Minification

```cpp
// Write minified JSON (default behavior)
Person person{};
std::string buffer{};
auto ec = glz::write_json(person, buffer); // Already minified by default

// Minify existing JSON text
std::string pretty_json = "..."; // JSON with whitespace
std::string minified = glz::minify_json(pretty_json);

// For reading minified JSON (performance optimization)
auto ec = glz::read<glz::opts{.minified = true}>(person, minified_buffer);
```

### Custom Serialization

For complex custom behavior, use `glz::custom`:

```cpp
struct TimestampObject {
    std::chrono::system_clock::time_point timestamp;
    
    void read_timestamp(const std::string& iso_string) {
        // Parse ISO 8601 string to time_point
    }
    
    std::string write_timestamp() const {
        // Convert time_point to ISO 8601 string
    }
};

template <>
struct glz::meta<TimestampObject> {
    using T = TimestampObject;
    static constexpr auto value = glz::object(
        "timestamp", glz::custom<&T::read_timestamp, &T::write_timestamp>
    );
};
```

### Boolean Flags

Represent multiple boolean flags as a JSON array:

```cpp
struct Permissions {
    bool read = true;
    bool write = false;
    bool execute = true;
};

template <>
struct glz::meta<Permissions> {
    using T = Permissions;
    static constexpr auto value = glz::flags(
        "read", &T::read,
        "write", &T::write,
        "execute", &T::execute
    );
};

// JSON output: ["read", "execute"]
```

### Object Merging

Combine multiple objects into a single JSON object:

```cpp
glz::obj metadata{"version", "1.0", "author", "John"};
std::map<std::string, int> scores = {{"test1", 95}, {"test2", 87}};

auto merged = glz::merge{metadata, scores};
std::string buffer{};
auto ec = glz::write_json(merged, buffer);
// Results in: {"version":"1.0","author":"John","test1":95,"test2":87}
```

## Performance Features

### Direct Memory Access

Glaze reads and writes directly to/from object memory, avoiding intermediate representations:

```cpp
// No DOM tree, no temporary objects - direct serialization
LargeObject large_object{};
std::string buffer{};
auto ec = glz::write_json(large_object, buffer);
```

### Compile-Time Optimizations

- Perfect hash tables for object keys
- Compile-time reflection eliminates runtime overhead
- SIMD and SWAR optimizations where possible

## JSON Conformance

Glaze is fully RFC 8259 compliant with UTF-8 validation. By default, it uses two optimizations:

- `validate_skipped = false`: Faster parsing by not fully validating skipped values (does not affect resulting C++ objects)
- `validate_trailing_whitespace = false`: Stops parsing after the target object

For strict compliance, enable these options:

```cpp
struct strict_opts : glz::opts {
    bool validate_skipped = true;
    bool validate_trailing_whitespace = true;
};

auto ec = glz::read<strict_opts>(obj, json_data);
```
