# Partial Write

Glaze supports partial object writing in two modes:
1. **Whitelist (Include)**: Write only specified fields using `glz::json_ptrs`
2. **Exclude**: Permanently exclude fields from serialization using `glz::meta::excluded`

## Example Structure

```c++
struct animals_t
{
   std::string lion = "Lion";
   std::string tiger = "Tiger";
   std::string panda = "Panda";
};

struct zoo_t
{
   animals_t animals{};
   std::string name{"My Awesome Zoo"};
};
```

## Whitelist Mode (Include Only)

Write only the specified fields:

```c++
"partial write"_test = [] {
    static constexpr auto partial = glz::json_ptrs("/name", "/animals/tiger");

    zoo_t obj{};
    std::string s{};
    const auto ec = glz::write_json<partial>(obj, s);
    expect(!ec);
    expect(s == R"({"animals":{"tiger":"Tiger"},"name":"My Awesome Zoo"})") << s;
};
```

## Exclude Mode (Metadata-Based)

Permanently exclude fields by adding `excluded` to `glz::meta`. Excluded fields are removed from the type's reflection and will never be serialized or deserialized:

```c++
struct User
{
   std::string name = "Alice";
   std::string email = "alice@example.com";
   std::string password = "secret123";
};

template <>
struct glz::meta<User>
{
   static constexpr std::array excluded{"password"};  // Never serialize password
};
```

You can exclude fields from nested objects:

```c++
struct animals_exclude_panda_t
{
   std::string lion = "Lion";
   std::string tiger = "Tiger";
   std::string panda = "Panda";
};

template <>
struct glz::meta<animals_exclude_panda_t>
{
   static constexpr std::array excluded{"panda"};  // Exclude panda
};
```

Or exclude entire nested objects:

```c++
struct zoo_exclude_animals_t
{
   animals_t animals{};
   std::string name{"My Awesome Zoo"};
};

template <>
struct glz::meta<zoo_exclude_animals_t>
{
   static constexpr std::array excluded{"animals"};  // Exclude entire animals object
};
```

### Writing Excluded Types

No template parameters needed - exclusions are automatic:

```c++
"exclude write"_test = [] {
    User user{};
    std::string s{};
    const auto ec = glz::write_json(user, s);  // No template parameter!
    expect(!ec);
    // Writes all fields except password
    expect(s == R"({"name":"Alice","email":"alice@example.com"})") << s;
};

"exclude nested"_test = [] {
    animals_exclude_panda_t obj{};
    std::string s{};
    const auto ec = glz::write_json(obj, s);
    expect(!ec);
    // Writes all fields except panda
    expect(s == R"({"lion":"Lion","tiger":"Tiger"})") << s;
};

"exclude object"_test = [] {
    zoo_exclude_animals_t obj{};
    std::string s{};
    const auto ec = glz::write_json(obj, s);
    expect(!ec);
    // Writes all fields except the entire animals object
    expect(s == R"({"name":"My Awesome Zoo"})") << s;
};
```

## Use Cases

**Whitelist** is best when:
- You need different field subsets for different contexts
- Building minimal API responses dynamically
- Selecting specific data for display at runtime

**Exclude** is best when:
- Hiding sensitive data permanently (passwords, tokens, internal IDs)
- Fields should NEVER be serialized in any context
- Type-level guarantees that certain data won't leak
- Cleaner code without repetitive exclusion calls

## Key Differences

| Feature | Whitelist (`json_ptrs`) | Exclude (`glz::meta::excluded`) |
|---------|------------------------|----------------------------------|
| **Definition** | Call-site | Type-level |
| **Flexibility** | Different per call | Permanent |
| **Syntax** | `write_json<partial>(obj)` | `write_json(obj)` |
| **Use case** | Dynamic selection | Security/privacy |
| **Read/Write** | Write only | Both excluded |

## Alternative: Local `struct glaze`

You can also use a local `struct glaze` instead of `glz::meta` template specialization:

```c++
struct User
{
   std::string name = "Alice";
   std::string password = "secret";

   struct glaze
   {
      static constexpr std::array excluded{"password"};
   };
};
```

Both approaches work identically - use whichever fits your coding style better.
