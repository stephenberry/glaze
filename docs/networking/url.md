# URL Utilities

Glaze provides URL encoding/decoding utilities for parsing query strings, form data, and URL path segments. These utilities follow the [WHATWG URL Standard](https://url.spec.whatwg.org/) for `application/x-www-form-urlencoded` parsing.

## Header

```cpp
#include "glaze/net/url.hpp"
```

The URL utilities are also available when including `glaze/net/http.hpp` or `glaze/net/http_router.hpp`.

## URL Decoding

### Basic Usage

```cpp
// Decode percent-encoded strings
std::string decoded = glz::url_decode("hello%20world");  // "hello world"
std::string path = glz::url_decode("path%2Fto%2Ffile");  // "path/to/file"

// Plus signs are decoded as spaces (form encoding)
std::string query = glz::url_decode("search+term");  // "search term"
```

### Path Segments

`+` means a space only in query strings and form bodies. In a URL path it is a literal plus (RFC 3986), so decode path segments with `url_decode_path`, which handles `%XX` escapes and leaves `+` alone. The HTTP router uses it for `:param` and `*wildcard` captures.

```cpp
std::string segment = glz::url_decode_path("a+b%20c");  // "a+b c"
```

To build a path segment, encode it with `url_encode_path_segment` (see [URL Encoding](#url-encoding)) rather than `url_encode`, which writes a space as `+`.

### Supported Encodings

| Encoded | Decoded |
|---------|---------|
| `%20` | space |
| `%2F` | `/` |
| `%3D` | `=` |
| `%26` | `&` |
| `%3F` | `?` |
| `%23` | `#` |
| `+` | space (`url_decode` only; `url_decode_path` keeps it) |

### Buffer Reuse (Zero-Allocation)

For high-performance scenarios, pass a reusable buffer to avoid allocations:

```cpp
std::string buffer;
buffer.reserve(1024);  // Pre-allocate once

// Decode multiple strings without allocation (if buffer has capacity)
glz::url_decode("hello%20world", buffer);
std::cout << buffer << std::endl;  // "hello world"

glz::url_decode("foo%2Fbar", buffer);
std::cout << buffer << std::endl;  // "foo/bar"
```

## URL Encoding

### Basic Usage

```cpp
// Encode strings for safe use in URLs
std::string encoded = glz::url_encode("hello world");     // "hello+world"
std::string path = glz::url_encode("path/to/file");       // "path%2Fto%2Ffile"
std::string special = glz::url_encode("a=b&c=d");         // "a%3Db%26c%3Dd"
```

### Encoding Rules

- **Unreserved characters** pass through unchanged: `A-Z`, `a-z`, `0-9`, `-`, `.`, `_`, `~`
- **Space** is encoded as `+` (per `application/x-www-form-urlencoded`)
- **All other characters** are percent-encoded (`%XX`)

### Path Segments

`url_encode` is for query strings and form bodies. For a URL path segment use `url_encode_path_segment`, which writes a space as `%20` (a `+` in a path is a literal plus) and percent-encodes everything except unreserved characters, `/` and `+` included, so the value stays one segment. Encode each segment separately and join them with `/`. `url_decode_path` reverses it.

```cpp
std::string url = "http://localhost:8080/files/" + glz::url_encode_path_segment("Q3 report+notes.txt");
// "http://localhost:8080/files/Q3%20report%2Bnotes.txt"; a "/files/:name" route captures "Q3 report+notes.txt"
```

### Building Query Strings

```cpp
// Build a safe query string
std::string query = "q=" + glz::url_encode(user_search) +
                    "&category=" + glz::url_encode(category);

// Example: user_search = "C++ templates", category = "programming/advanced"
// Result: "q=C%2B%2B+templates&category=programming%2Fadvanced"
```

### Roundtrip Encoding

Encode and decode are symmetric:

```cpp
std::string original = "Hello World! Special: /=&?#";
std::string encoded = glz::url_encode(original);
std::string decoded = glz::url_decode(encoded);

// decoded == original
```

### Buffer Reuse (Zero-Allocation)

```cpp
std::string buffer;
buffer.reserve(1024);

glz::url_encode("hello world", buffer);
std::cout << buffer << std::endl;  // "hello+world"

glz::url_encode("a/b", buffer);
std::cout << buffer << std::endl;  // "a%2Fb"
```

## Parsing URL-Encoded Data

The `parse_urlencoded` function parses `key=value&key2=value2` format used in:
- URL query strings (`?limit=10&offset=20`)
- Form POST bodies (`application/x-www-form-urlencoded`)

### Basic Usage

```cpp
// Parse query string
auto params = glz::parse_urlencoded("limit=10&offset=20&sort=name");

std::cout << params["limit"];   // "10"
std::cout << params["offset"];  // "20"
std::cout << params["sort"];    // "name"
```

### Automatic Decoding

Keys and values are automatically URL-decoded:

```cpp
auto params = glz::parse_urlencoded("name=John%20Doe&city=New+York");

std::cout << params["name"];  // "John Doe"
std::cout << params["city"];  // "New York"
```

### Edge Cases

```cpp
// Empty value
auto p1 = glz::parse_urlencoded("key=");
// p1["key"] == ""

// Key without value
auto p2 = glz::parse_urlencoded("flag");
// p2["flag"] == ""

// Duplicate keys (last value wins)
auto p3 = glz::parse_urlencoded("a=1&a=2&a=3");
// p3["a"] == "3"

// Empty string
auto p4 = glz::parse_urlencoded("");
// p4 is empty

// Empty keys are skipped
auto p5 = glz::parse_urlencoded("=value&a=1");
// p5.size() == 1, p5["a"] == "1"
```

### Buffer Reuse (Zero-Allocation)

For server applications processing many requests, reuse buffers to minimize allocations:

```cpp
// Reusable buffers - allocate once, reuse for all requests
std::unordered_map<std::string, std::string> params;
std::string key_buffer;
std::string value_buffer;

// Pre-reserve capacity
key_buffer.reserve(64);
value_buffer.reserve(256);

// Process multiple query strings without allocation
for (const auto& query : incoming_queries) {
    glz::parse_urlencoded(query, params, key_buffer, value_buffer);

    // Process params...
    handle_request(params);
}
```

There's also a two-argument overload that manages key/value buffers internally:

```cpp
std::unordered_map<std::string, std::string> params;

glz::parse_urlencoded("foo=bar&baz=qux", params);
// params is populated, internal buffers used for decoding
```

## Splitting URL Targets

The `split_target` function separates a URL path from its query string:

```cpp
auto [path, query] = glz::split_target("/api/users?limit=10&offset=20");
// path  == "/api/users"
// query == "limit=10&offset=20"

auto [path2, query2] = glz::split_target("/api/users");
// path2  == "/api/users"
// query2 == ""  (empty)
```

This is useful when you need to process the path and query string separately:

```cpp
std::string_view target = "/search?q=hello%20world&page=1";

auto [path, query_string] = glz::split_target(target);
auto params = glz::parse_urlencoded(query_string);

std::cout << "Path: " << path << std::endl;           // "/search"
std::cout << "Query: " << params["q"] << std::endl;   // "hello world"
std::cout << "Page: " << params["page"] << std::endl; // "1"
```

## Integration with HTTP Server

When using the Glaze HTTP server, query parameters are automatically parsed and available in the request object:

```cpp
server.get("/api/users", [](const glz::request& req, glz::response& res) {
    // Query parameters are automatically parsed
    // For request: GET /api/users?limit=10&offset=20

    if (auto it = req.query.find("limit"); it != req.query.end()) {
        int limit = std::stoi(it->second);  // 10
    }

    if (auto it = req.query.find("offset"); it != req.query.end()) {
        int offset = std::stoi(it->second);  // 20
    }

    // req.path contains just the path without query string
    // req.path == "/api/users"

    // req.target contains the full URL
    // req.target == "/api/users?limit=10&offset=20"
});
```

## Parsing Form POST Data

For `application/x-www-form-urlencoded` POST requests, use `parse_urlencoded` on the request body:

```cpp
server.post("/login", [](const glz::request& req, glz::response& res) {
    // Check content type
    // A media type can carry parameters like "; charset=utf-8", so match on the prefix.
    auto ct = req.headers.first_value("content-type");
    if (!ct || !ct->starts_with("application/x-www-form-urlencoded")) {
        res.status(415).json({{"error", "Unsupported content type"}});
        return;
    }

    // Parse form data from body
    auto form = glz::parse_urlencoded(req.body);

    std::string username = form["username"];
    std::string password = form["password"];

    // Authenticate...
});
```

## API Reference

### `url_decode`

```cpp
// Returns decoded string (allocates)
[[nodiscard]] std::string url_decode(std::string_view input);

// Writes to buffer (can avoid allocation if buffer has capacity)
void url_decode(std::string_view input, std::string& output);
```

### `url_decode_path`

```cpp
// Like url_decode, but keeps '+' as a literal plus (for path segments)
[[nodiscard]] std::string url_decode_path(std::string_view input);
void url_decode_path(std::string_view input, std::string& output);
```

### `url_encode`

```cpp
// Returns encoded string (allocates)
[[nodiscard]] std::string url_encode(std::string_view input);

// Writes to buffer (can avoid allocation if buffer has capacity)
void url_encode(std::string_view input, std::string& output);
```

### `url_encode_path_segment`

```cpp
// Percent-encodes one path segment: space as %20, '/' as %2F
[[nodiscard]] std::string url_encode_path_segment(std::string_view input);
void url_encode_path_segment(std::string_view input, std::string& output);
```

### `parse_urlencoded`

```cpp
// Returns new map (allocates)
[[nodiscard]] std::unordered_map<std::string, std::string>
    parse_urlencoded(std::string_view query_string);

// Writes to provided map (reuses map capacity)
void parse_urlencoded(std::string_view query_string,
                      std::unordered_map<std::string, std::string>& output);

// Full buffer control (minimal allocations)
void parse_urlencoded(std::string_view query_string,
                      std::unordered_map<std::string, std::string>& output,
                      std::string& key_buffer,
                      std::string& value_buffer);
```

### `split_target`

```cpp
struct target_components {
    std::string_view path{};
    std::string_view query_string{};
};

constexpr target_components split_target(std::string_view target) noexcept;
```

### `hex_char_to_int`

```cpp
// Convert hex character to integer (0-15), returns -1 for invalid input
constexpr int hex_char_to_int(char c) noexcept;
```
