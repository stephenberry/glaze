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

