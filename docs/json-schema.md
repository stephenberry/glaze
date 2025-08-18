# JSON Schema

JSON Schema can automaticly be generated for serializable named types exposed via the meta system.

```c++
std::string schema = glz::write_json_schema<my_struct>();
```

This can be used for autocomplete, linting, and validation of user input/config files in editors like VS Code that support JSON Schema.

![autocomplete example](https://user-images.githubusercontent.com/9817348/199346159-8b127c7b-a9ac-49fe-b86d-71350f0e1b10.png)

![linting example](https://user-images.githubusercontent.com/9817348/199347118-ef7e9f74-ed20-4ff5-892a-f70ff1df23b5.png)

## Registering JSON Schema Metadata

By default `glz::write_json_schema` will write out your fields and types, but additional field may also be registered by specializing `glz::json_schema` or adding a `glaze_json_schema` to your struct.

Example:

```c++
struct meta_schema_t
{
   int x{};
   std::string file_name{};
   bool is_valid{};
};

template <>
struct glz::json_schema<meta_schema_t>
{
   schema x{.description = "x is a special integer", .minimum = 1};
   schema file_name{.description = "provide a file name to load"};
   schema is_valid{.description = "for validation"};
};
```

The `glz::json_schema` struct allows additional metadata like `minimum`, `maximum`, etc. to be specified for your fields.

Or you can define `glaze_json_schema` in your struct in the same manner:

```c++
struct local_schema_t
{
   int x{};
   std::string file_name{};
   bool is_valid{};

   struct glaze_json_schema
   {
      glz::schema x{.description = "x is a special integer", .minimum = 1};
      glz::schema file_name{.description = "provide a file name to load"};
      glz::schema is_valid{.description = "for validation"};
   };
};
```

## Required Fields

Glaze can automatically mark fields as required in the generated JSON schema based on their nullability and compile options.

### Automatic Required Fields

By default, when using the `error_on_missing_keys` option, non-nullable fields will be marked as required in the schema:

```c++
struct user_config
{
   std::string username{};  // required (non-nullable)
   std::optional<int> age{}; // optional (nullable)
   int id{};                 // required (non-nullable)
};

// Generate schema with required fields
auto schema = glz::write_json_schema<user_config, glz::opts{.error_on_missing_keys = true}>();
```

This will generate a schema with `"required": ["username", "id"]`.

### Custom Required Field Logic

You can override the default behavior by providing a custom `requires_key` function in your type's `glz::meta` specialization:

```c++
struct api_request
{
   std::string endpoint{};
   std::string api_key{};
   std::optional<std::string> optional_param{};
   std::string reserved_field{};  // internal use only
};

template <>
struct glz::meta<api_request>
{
   // Custom logic to determine which fields are required
   static constexpr bool requires_key(std::string_view key, bool is_nullable)
   {
      // Don't require fields starting with "reserved"
      if (key.starts_with("reserved")) {
         return false;
      }
      // All non-nullable fields are required
      return !is_nullable;
   }
};
```

The `requires_key` function receives:
- `key`: The name of the field being checked
- `is_nullable`: Whether the field type is nullable (e.g., `std::optional`, pointers)

This allows fine-grained control over which fields are marked as required in the generated JSON schema, useful for:
- Excluding internal/reserved fields from requirements
- Making certain nullable fields required based on business logic
- Implementing complex validation rules

