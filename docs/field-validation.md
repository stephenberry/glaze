# Field Validation

Glaze provides field validation capabilities through compile-time options and customization points.

## Required Fields with `error_on_missing_keys`

By default, Glaze allows JSON objects to have missing fields when parsing. However, you can enable strict validation using the `error_on_missing_keys` option:

```c++
struct config {
   std::string host;
   int port;
   std::optional<std::string> description;
};

std::string json = R"({"host":"localhost"})";  // Missing 'port'
config obj;

// Without error_on_missing_keys (default)
auto ec = glz::read_json(obj, json);  // Success, port gets default value

// With error_on_missing_keys
ec = glz::read<glz::opts{.error_on_missing_keys = true}>(obj, json);  // Error: missing_key
```

### Default Behavior

When `error_on_missing_keys = true`, field requirements are determined automatically:

- **Non-nullable types** (e.g., `int`, `std::string`, `double`) are **required**
- **Nullable types** (e.g., `std::optional<T>`, `std::unique_ptr<T>`, `std::shared_ptr<T>`, `T*`) are **optional**

```c++
struct user {
   std::string username;           // Required
   int id;                          // Required
   std::optional<std::string> bio;  // Optional
};

// Valid: optional field can be missing
std::string json = R"({"username":"alice","id":42})";
user u;
auto ec = glz::read<glz::opts{.error_on_missing_keys = true}>(u, json);  // Success

// Invalid: required field missing
json = R"({"username":"alice"})";  // Missing 'id'
ec = glz::read<glz::opts{.error_on_missing_keys = true}>(u, json);  // Error: missing_key
```

## Custom Field Requirements with `requires_key`

For fine-grained control over which fields are required, you can provide a `requires_key` function in your type's `glz::meta` specialization. This allows you to:

- Make non-nullable fields optional without wrapping them in `std::optional`
- Exclude internal/reserved fields from validation
- Implement complex business logic for field requirements

### Basic Usage

```c++
struct api_request {
   std::string endpoint;      // Required
   std::string api_key;       // Required
   int timeout;               // Want this to be optional
   std::string _internal_id;  // Internal field, should not be required
};

template <>
struct glz::meta<api_request> {
   static constexpr bool requires_key(std::string_view key, bool is_nullable) {
      // Make timeout optional even though it's not nullable
      if (key == "timeout") {
         return false;
      }

      // Don't require internal fields
      if (key.starts_with("_")) {
         return false;
      }

      // Default: non-nullable fields are required
      return !is_nullable;
   }
};

// Now parsing works with optional timeout
std::string json = R"({"endpoint":"/users","api_key":"secret123"})";
api_request req;
auto ec = glz::read<glz::opts{.error_on_missing_keys = true}>(req, json);  // Success!
// req.timeout will have its default value (0)
```

### Function Signature

```c++
static constexpr bool requires_key(std::string_view key, bool is_nullable)
```

**Parameters:**
- `key`: The name of the field being checked
- `is_nullable`: Whether the field's type is nullable (e.g., `std::optional`, pointers)

**Returns:**
- `true` if the field is required (parsing will fail if it's missing)
- `false` if the field is optional (parsing will succeed even if it's missing)

### Advanced Examples

#### Conditional Requirements

```c++
struct form_data {
   std::string name;
   std::string email;
   std::string phone;
   std::string address;
};

template <>
struct glz::meta<form_data> {
   // Require only name and at least one contact method (email or phone)
   // This is a simplified example - actual implementation would need runtime validation
   static constexpr bool requires_key(std::string_view key, bool is_nullable) {
      if (key == "name") return true;  // Always required
      // For this compile-time function, we can only mark fields as optional or required
      // Runtime validation would be needed for "at least one of" requirements
      return false;
   }
};
```

#### Reserved/Internal Fields

```c++
struct database_record {
   int id;
   std::string name;
   int reserved_field1;  // Reserved for future use
   int reserved_field2;  // Reserved for future use
};

template <>
struct glz::meta<database_record> {
   static constexpr bool requires_key(std::string_view key, bool is_nullable) {
      // Don't require fields starting with "reserved"
      if (key.starts_with("reserved")) {
         return false;
      }
      return !is_nullable;
   }
};
```

#### Working with Nullable Types

The `requires_key` function works seamlessly with nullable types:

```c++
struct mixed_requirements {
   int id;                          // Required (non-nullable, no special rule)
   std::optional<int> optional_id;  // Optional (nullable type)
   int flexible_field;              // Made optional via requires_key
};

template <>
struct glz::meta<mixed_requirements> {
   static constexpr bool requires_key(std::string_view key, bool is_nullable) {
      // Make flexible_field optional
      if (key == "flexible_field") {
         return false;
      }
      // Default behavior: nullable types are optional, others are required
      return !is_nullable;
   }
};

// Valid: both optional_id and flexible_field can be missing
std::string json = R"({"id":42})";
mixed_requirements obj;
auto ec = glz::read<glz::opts{.error_on_missing_keys = true}>(obj, json);  // Success
```

## Use Cases

### 1. Backward Compatibility

When adding new fields to an existing API, make them optional without changing their types:

```c++
struct api_response_v2 {
   int status;
   std::string message;
   int new_field_added_in_v2;  // New field, should be optional for v1 clients
};

template <>
struct glz::meta<api_response_v2> {
   static constexpr bool requires_key(std::string_view key, bool is_nullable) {
      if (key == "new_field_added_in_v2") {
         return false;
      }
      return !is_nullable;
   }
};
```

### 2. Configuration Files

Allow optional configuration values without cluttering code with `std::optional`:

```c++
struct app_config {
   std::string database_url;  // Required
   int max_connections;       // Optional, has good default
   int timeout_ms;            // Optional, has good default
};

template <>
struct glz::meta<app_config> {
   static constexpr bool requires_key(std::string_view key, bool is_nullable) {
      if (key == "database_url") return true;
      return false;  // Everything else is optional
   }
};
```

### 3. Partial Updates

When receiving partial update payloads, only require the ID field:

```c++
struct user_update {
   int user_id;              // Always required
   std::string name;         // Optional - only update if provided
   std::string email;        // Optional - only update if provided
   std::string password;     // Optional - only update if provided
};

template <>
struct glz::meta<user_update> {
   static constexpr bool requires_key(std::string_view key, bool is_nullable) {
      return key == "user_id";  // Only ID is required
   }
};
```

## Relationship with JSON Schema

The `requires_key` customization point affects both:

1. **Parsing/Validation**: Determines which fields are validated during `glz::read` with `error_on_missing_keys = true`
2. **JSON Schema Generation**: Determines which fields appear in the `"required"` array when using `glz::write_json_schema`

This ensures consistency between your validation logic and your schema documentation.

## Best Practices

1. **Use `std::optional` when field truly represents optional data** - Reserve `requires_key` for cases where you need non-nullable types to be optional
2. **Document your `requires_key` logic** - Future maintainers should understand why certain fields are optional
3. **Keep validation simple** - Complex interdependent requirements may be better suited for runtime validation after parsing
4. **Consider backward compatibility** - Making a previously-optional field required is a breaking change
5. **Test both paths** - Ensure your code handles both the presence and absence of optional fields

## See Also

- [Options](options.md) - Compile-time options including `error_on_missing_keys`
- [JSON Schema](json-schema.md) - Generating JSON schemas from C++ types
- [Nullable Types](nullable-types.md) - Working with optional and pointer types
