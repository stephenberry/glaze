# Variant Handling

Glaze has full support for `std::variant` when writing, and comprehensive read support with automatic type deduction. When reading JSON into a variant, Glaze can automatically determine the correct type based on the JSON structure, field presence, or explicit type tags. 

## Basic Types

Types can be auto-deduced if the variant contains at most one type matching each of the fundamental JSON types of [string, number, object, array boolean] or multiple object types. `std::variant<double, std::string>` could be auto deduced but `std::variant<double, float>` cannot be.

Write example:
```c++
std::variant<double, std::string> d = "not_a_fish";
auto s = glz::write_json(d);
expect(s == R"("not_a_fish")");
```

Read example:
```c++
std::variant<int32_t, double> x = 44;
glz::read_json(x, "33");
expect(std::get<int32_t>(x) == 33);
```

## Object Types

Glaze provides multiple strategies for deducing which variant type to use when reading JSON objects:

### Auto Deduction with Unique Keys
Objects can be auto deduced when they have unique key combinations that distinguish them from other types in the variant.
```c++
struct xy_t
{
   int x{};
   int y{};
};

template <>
struct glz::meta<xy_t>
{
   using T = xy_t;
   static constexpr auto value = object("x", &T::x, "y", &T::y);
};

struct yz_t
{
   int y{};
   int z{};
};

template <>
struct glz::meta<yz_t>
{
   using T = yz_t;
   static constexpr auto value = object("y", &T::y, "z", &T::z);
};

struct xz_t
{
   int x{};
   int z{};
};

template <>
struct glz::meta<xz_t>
{
   using T = xz_t;
   static constexpr auto value = object("x", &T::x, "z", &T::z);
};

suite metaobject_variant_auto_deduction = [] {
   "metaobject_variant_auto_deduction"_test = [] {
      std::variant<xy_t, yz_t, xz_t> var{};

      std::string b = R"({"y":1,"z":2})";
      expect(glz::read_json(var, b) == glz::error_code::none);
      expect(std::holds_alternative<yz_t>(var));
      expect(std::get<yz_t>(var).y == 1);
      expect(std::get<yz_t>(var).z == 2);

      b = R"({"x":5,"y":7})";
      expect(glz::read_json(var, b) == glz::error_code::none);
      expect(std::holds_alternative<xy_t>(var));
      expect(std::get<xy_t>(var).x == 5);
      expect(std::get<xy_t>(var).y == 7);

      b = R"({"z":3,"x":4})";
      expect(glz::read_json(var, b) == glz::error_code::none);
      expect(std::holds_alternative<xz_t>(var));
      expect(std::get<xz_t>(var).z == 3);
      expect(std::get<xz_t>(var).x == 4);
   };
};
```

In the example above, each type has a unique combination of keys (xy, yz, xz), making deduction straightforward based on which keys are present in the JSON.

### Ambiguous Variant Resolution

When multiple object types in a variant could match the input JSON (all required fields are present), Glaze automatically resolves the ambiguity by selecting the type with the **fewest fields**. This "smallest object wins" strategy ensures the most specific type is chosen.

**Example:**
```cpp
struct SwitchBlock {
   int value{};  // 1 field
};

struct PDataBlock {
   std::string p_id{};  // 2 fields  
   int value{};
};

using var_t = std::variant<SwitchBlock, PDataBlock>;

// JSON: {"value": 42}
// Both types could match, but SwitchBlock is chosen (1 field < 2 fields)
var_t v1;
glz::read_json(v1, R"({"value": 42})");
// v1 holds SwitchBlock

// JSON: {"p_id": "test", "value": 99}  
// Only PDataBlock matches (has all required fields)
var_t v2;
glz::read_json(v2, R"({"p_id": "test", "value": 99})");
// v2 holds PDataBlock
```

This resolution applies when:
- The variant contains 2 or more object types
- Multiple types have all their fields present in the JSON
- The type with the minimum field count is automatically selected

Common use cases include:
- **Progressive detail levels**: Types that represent the same concept with increasing levels of detail
- **Optional field patterns**: Where simpler types are subsets of more complex types
- **API versioning**: Where newer types extend older ones with additional fields

**Complete Example with Multiple Levels:**
```cpp
struct PersonBasic {
   std::string name{};
};

struct PersonWithAge {
   std::string name{};
   int age{};
};

struct PersonFull {
   std::string name{};
   int age{};
   double height{};
};

using person_variant = std::variant<PersonBasic, PersonWithAge, PersonFull>;

// Automatically selects the most specific type based on fields present:
person_variant p1;
glz::read_json(p1, R"({"name": "Alice"})");           // → PersonBasic
glz::read_json(p1, R"({"name": "Bob", "age": 30})");  // → PersonWithAge  
glz::read_json(p1, R"({"name": "Charlie", "age": 25, "height": 175.5})"); // → PersonFull
```

### Deduction of Tagged Object Types
If you don't want auto deduction, need explicit type control, or need to deduce the type based on the value associated with a key, Glaze supports custom tags. This is particularly useful when:
- You want explicit control over type selection regardless of field presence
- The automatic deduction might be ambiguous or unpredictable
- You need to support types with identical field structures

```c++
struct put_action
{
   std::map<std::string, int> data{};
};

template <>
struct glz::meta<put_action>
{
   using T = put_action;
   static constexpr auto value = object("data", &T::data);
};

struct delete_action
{
   std::string data{};
};

template <>
struct glz::meta<delete_action>
{
   using T = delete_action;
   static constexpr auto value = object("data", &T::data);
};

using tagged_variant = std::variant<put_action, delete_action>;

template <>
struct glz::meta<tagged_variant>
{
   static constexpr std::string_view tag = "action";
   static constexpr auto ids = std::array{"PUT", "DELETE"}; //Defaults to glz::name_v of the type if ids is not supplied
};

suite tagged_variant_tests = [] {
   "tagged_variant_write_tests"_test = [] {
      tagged_variant var = delete_action{{"the_internet"}};
      std::string s{};
      glz::write_json(var, s);
      expect(s == R"({"action":"DELETE","data":"the_internet"})");
   };
   
   "tagged_variant_read_tests"_test = [] {
      tagged_variant var{};
      expect(glz::read_json(var, R"({"action":"DELETE","data":"the_internet"})") == glz::error_code::none);
      expect(std::get<delete_action>(var).data == "the_internet");
   };
};
```

## BEVE to JSON

BEVE uses the variant index to denote the type in a variant. When calling `glz::beve_to_json`, variants will be written in JSON with `"index"` and `"value"` keys. The index indicates the type, which would correspond to a `std::variant` `index()` method.

```json
{
  "index": 1,
  "value": "my value"
}
```

> BEVE conversion to JSON does not support `string` tags, to simplify the specification and avoid bifurcation of variant handling. Using the index is more efficient in binary and more directly translated to `std::variant`.
