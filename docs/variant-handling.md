# Variant Handling

Glaze has full support for `std::variant` when writing, and comprehensive read support with automatic type deduction. When reading JSON into a variant, Glaze can automatically determine the correct type based on the JSON structure, field presence, or explicit type tags. 

## Basic Types

Types can be auto-deduced if the variant contains at most one type matching each of the fundamental JSON types of [string, number, object, array, boolean]. For auto-deduction to work:
- `std::variant<double, std::string>` ✅ (one number type, one string type)
- `std::variant<double, float>` ❌ (two number types - requires explicit handling)
- `std::variant<bool, std::string, double>` ✅ (one of each type)

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

### Multiple Types of Same Category

Glaze can handle variants with multiple types of the same JSON category (e.g., multiple array types or multiple number types). When auto-deduction isn't possible, Glaze will try each alternative in order until one successfully parses:

```c++
// Multiple array types - now supported!
using NestedArrayVariant = std::variant<std::vector<double>, std::vector<std::vector<double>>>;

NestedArrayVariant var;
glz::read_json(var, "[1.0, 2.0, 3.0]");  // → std::vector<double>
glz::read_json(var, "[[1.0, 2.0], [3.0, 4.0]]");  // → std::vector<std::vector<double>>
```

This works for any combination of types:
```c++
// Multiple number types
using NumberVariant = std::variant<int, float, double>;
NumberVariant n;
glz::read_json(n, "42");     // Tries int first, succeeds
glz::read_json(n, "3.14");   // Tries int (fails), then float (succeeds)
```

## Object Types

Glaze provides multiple strategies for deducing which variant type to use when reading JSON objects:

### Auto Deduction with Reflectable Structs

Simple aggregate structs can be used in variants without requiring explicit `glz::meta` specializations. Glaze automatically uses reflection to deduce the type based on field names:

```c++
// No glz::meta needed for these simple structs!
struct Book {
   std::string title;
   std::string author;
   int pages;
};

struct Movie {
   std::string director;
   int duration;
   float rating;
};

struct Song {
   std::string artist;
   std::string album;
   int year;
};

// Automatic deduction based on unique field names
using MediaVariant = std::variant<Book, Movie, Song>;

MediaVariant media;
glz::read_json(media, R"({"title":"1984","author":"Orwell","pages":328})");  // → Book
glz::read_json(media, R"({"director":"Nolan","duration":148,"rating":8.8})"); // → Movie
glz::read_json(media, R"({"artist":"Beatles","album":"Abbey Road","year":1969})"); // → Song

// Even partial fields work for deduction
glz::read_json(media, R"({"title":"Partial Book"})");  // → Book (unique field)
glz::read_json(media, R"({"director":"Unknown"})");    // → Movie (unique field)
```

### Auto Deduction with Meta Objects
For more complex types or when you need custom serialization, you can still use explicit `glz::meta` specializations. Objects can be auto deduced when they have unique key combinations that distinguish them from other types in the variant.
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
   static constexpr auto value = object(&T::x, &T::y);
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
   static constexpr auto value = object(&T::y, &T::z);
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
   static constexpr auto value = object(&T::x, &T::z);
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
   static constexpr auto value = object(&T::data);
};

struct delete_action
{
   std::string data{};
};

template <>
struct glz::meta<delete_action>
{
   using T = delete_action;
   static constexpr auto value = object(&T::data);
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

### Tagged Variants with Embedded Tags

Variant tags may be embedded within the structs themselves, making the discriminator accessible at runtime without using `std::visit`. This provides cleaner, more maintainable code:

```c++
// String-based embedded tags
struct CreateAction {
   std::string action{"CREATE"};  // Embedded tag field
   std::string resource;
   std::map<std::string, std::string> attributes;
};

struct UpdateAction {
   std::string action{"UPDATE"};  // Embedded tag field
   std::string id;
   std::map<std::string, std::string> changes;
};

struct DeleteAction {
   std::string action{"DELETE"};  // Embedded tag field
   std::string id;
};

using ActionVariant = std::variant<CreateAction, UpdateAction, DeleteAction>;

template <>
struct glz::meta<ActionVariant> {
   static constexpr std::string_view tag = "action";  // Field name that serves as tag
   static constexpr auto ids = std::array{"CREATE", "UPDATE", "DELETE"};
};

// Writing - no double tagging, single "action" field
ActionVariant v = UpdateAction{"UPDATE", "123", {{"status", "active"}}};
auto json = glz::write_json(v);
// Result: {"action":"UPDATE","id":"123","changes":{"status":"active"}}

// Reading - tag field is populated automatically
ActionVariant v2;
glz::read_json(v2, json.value());
if (std::holds_alternative<UpdateAction>(v2)) {
   auto& update = std::get<UpdateAction>(v2);
   // Direct access to discriminator without std::visit!
   assert(update.action == "UPDATE");  
}
```

#### Enum-based Embedded Tags

For type safety, you can use enums as embedded tags:

```c++
enum class OperationType { GET, POST, PUT, DELETE };

// Define string mapping for the enum
template <>
struct glz::meta<OperationType> {
   using enum OperationType;
   static constexpr auto value = enumerate(GET, POST, PUT, DELETE);
};

struct GetRequest {
   OperationType operation{OperationType::GET};  // Embedded enum tag
   std::string path;
   std::map<std::string, std::string> params;
};

struct PostRequest {
   OperationType operation{OperationType::POST};  // Embedded enum tag
   std::string path;
   std::string body;
};

using RequestVariant = std::variant<GetRequest, PostRequest>;

template <>
struct glz::meta<RequestVariant> {
   static constexpr std::string_view tag = "operation";
   static constexpr auto ids = std::array{"GET", "POST"};
};

// The enum is serialized as a string in JSON
RequestVariant req = PostRequest{OperationType::POST, "/api/users", R"({"name":"Alice"})"};
auto json = glz::write_json(req);
// Result: {"operation":"POST","path":"/api/users","body":"{\"name\":\"Alice\"}"}
```

Benefits of embedded tags:
- **Runtime access**: The tag value is directly accessible without `std::visit`
- **No duplication**: JSON contains the tag field only once
- **Type safety**: Using enums provides compile-time checking
- **Cleaner code**: Keeps discriminator with the data it describes

### Tagged Variants with Reflectable Structs

You can also use tagged variants with simple reflectable structs without explicit `glz::meta` for the struct types:

```c++
// Simple structs - no glz::meta needed!
struct Person {
   std::string name;
   int age;
};

struct Animal {
   std::string species;
   float weight;
};

struct Vehicle {
   std::string model;
   int wheels;
};

using EntityVariant = std::variant<Person, Animal, Vehicle>;

// Only the variant needs meta for tagging
template <>
struct glz::meta<EntityVariant> {
   static constexpr std::string_view tag = "type";
   static constexpr auto ids = std::array{"person", "animal", "vehicle"};
};

// Writing includes the type tag
EntityVariant e = Person{"Alice", 30};
auto json = glz::write_json(e);
// Result: {"type":"person","name":"Alice","age":30}

// Reading uses the tag for type selection
EntityVariant e2;
glz::read_json(e2, R"({"type":"animal","species":"Lion","weight":190.5})");
// e2 now holds Animal{"Lion", 190.5f}

// Tag/field validation: If a tag value doesn't match the fields, an error is reported
EntityVariant e3;
auto error = glz::read_json(e3, R"({"species":"Lion","type":"person","weight":190.5})");
// Error: no_matching_variant_type (tag says "person" but fields match Animal)
```

### Default Variant Types (Catch-All Handler)

When working with tagged variants, you may encounter unknown tag values that aren't in your predefined list. Glaze supports a default/catch-all variant type by making the `ids` array shorter than the number of variant alternatives. The first unlabeled type (without a corresponding ID) becomes the default handler for unknown tags.

```c++
struct CreateAction {
   std::string action{"CREATE"};  // Embedded tag field
   std::string resource;
   std::map<std::string, std::string> attributes;
};

struct UpdateAction {
   std::string action{"UPDATE"};  // Embedded tag field
   std::string id;
   std::map<std::string, std::string> changes;
};

// Default handler for unknown action types
struct UnknownAction {
   std::string action;  // Will be populated with the actual tag value
   std::optional<std::string> id;
   std::optional<std::string> resource;
   std::optional<std::string> target;
   // Can add more optional fields to handle various unknown formats
};

using ActionVariant = std::variant<CreateAction, UpdateAction, UnknownAction>;

template <>
struct glz::meta<ActionVariant> {
   static constexpr std::string_view tag = "action";
   // Note: Only 2 IDs for 3 variant types - UnknownAction becomes the default
   static constexpr auto ids = std::array{"CREATE", "UPDATE"};
};

// Known actions work as expected
std::string_view json = R"({"action":"CREATE","resource":"user","attributes":{"name":"Alice"}})";
ActionVariant av;
glz::read_json(av, json);
// av holds CreateAction with action == "CREATE"

// Unknown actions route to the default type
json = R"({"action":"DELETE","id":"123","target":"resource"})";
glz::read_json(av, json);
// av holds UnknownAction with action == "DELETE"
if (std::holds_alternative<UnknownAction>(av)) {
   auto& unknown = std::get<UnknownAction>(av);
   std::cout << "Unknown action: " << unknown.action << std::endl;  // Prints: DELETE
}
```

This feature is particularly useful for:
- **Forward compatibility**: Handle future action types without breaking existing code
- **API versioning**: Gracefully handle newer message types from updated clients
- **Error recovery**: Capture and log unknown operations instead of failing
- **Plugin systems**: Allow extensions to define custom actions

The default type works with both string and numeric tags:

```c++
struct TypeA {
   int type{1};
   std::string data;
};

struct TypeB {
   int type{2};
   double value;
};

struct TypeDefault {
   int type;  // Will contain the actual numeric tag value
   std::optional<std::string> data;
   std::optional<double> value;
};

using NumericVariant = std::variant<TypeA, TypeB, TypeDefault>;

template <>
struct glz::meta<NumericVariant> {
   static constexpr std::string_view tag = "type";
   static constexpr auto ids = std::array{1, 2};  // TypeDefault handles all other values
};

// Type 99 is unknown, routes to TypeDefault
std::string_view json = R"({"type":99,"data":"unknown"})";
NumericVariant nv;
glz::read_json(nv, json);
// nv holds TypeDefault with type == 99
```

**Important notes:**
- The default type must be the last type in the variant that doesn't have a corresponding ID
- The tag field in the default type will be populated with the actual tag value from the JSON
- Only the first unlabeled type serves as the default (you can only have one default handler)

## Variants with Smart Pointers

Glaze fully supports `std::unique_ptr` and `std::shared_ptr` as variant alternatives, including both auto-deduction and tagged variants. This is particularly useful for polymorphic types, factory patterns, and managing object ownership in variant containers.

### Auto-Deduction with Smart Pointers

Smart pointers work seamlessly with auto-deduction based on field names:

```c++
struct Cat {
   std::string name;
   int lives;
};

struct Dog {
   std::string name;
   std::string breed;
};

// Variant of smart pointers - auto-deduction works!
using PetVariant = std::variant<std::unique_ptr<Cat>, std::unique_ptr<Dog>>;

PetVariant pet;
glz::read_json(pet, R"({"name":"Fluffy","lives":9})");
// pet holds std::unique_ptr<Cat>

glz::read_json(pet, R"({"name":"Rover","breed":"Labrador"})");
// pet holds std::unique_ptr<Dog>
```

### Tagged Variants with Smart Pointers

Tagged variants also work with smart pointers, providing explicit type control:

```c++
struct SiteDiagnostic {
   std::string message;
   int severity;
};

struct DerivedSite {
   std::string message;
   int severity;
   std::string additional_info;
};

using DiagnosticVariant = std::variant<
   std::unique_ptr<SiteDiagnostic>,
   std::unique_ptr<DerivedSite>
>;

template <>
struct glz::meta<DiagnosticVariant> {
   static constexpr std::string_view tag = "type";
   static constexpr auto ids = std::array{"SiteDiagnostic", "DerivedSite"};
};

// Writing includes the type tag
DiagnosticVariant diag = std::make_unique<SiteDiagnostic>(
   SiteDiagnostic{"Error occurred", 3}
);
auto json = glz::write_json(diag);
// Result: {"type":"SiteDiagnostic","message":"Error occurred","severity":3}

// Reading uses the tag for type selection
DiagnosticVariant parsed;
glz::read_json(parsed, R"({"type":"DerivedSite","message":"Warning","severity":2,"additional_info":"Details"})");
// parsed holds std::unique_ptr<DerivedSite>
```

**Important notes:**
- Smart pointers are automatically allocated during deserialization
- Null smart pointers during serialization will result in an error
- Both auto-deduction and tagged variants work with smart pointers
- Mixed variants (e.g., `std::variant<Cat, std::unique_ptr<Dog>>`) are also supported

## BEVE to JSON

BEVE uses the variant index to denote the type in a variant. When calling `glz::beve_to_json`, variants will be written in JSON with `"index"` and `"value"` keys. The index indicates the type, which would correspond to a `std::variant` `index()` method.

```json
{
  "index": 1,
  "value": "my value"
}
```

> BEVE conversion to JSON does not support `string` tags, to simplify the specification and avoid bifurcation of variant handling. Using the index is more efficient in binary and more directly translated to `std::variant`.
