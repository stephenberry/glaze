# CORS (Cross-Origin Resource Sharing)

Glaze provides comprehensive CORS support with automatic preflight request handling, making it easy to build APIs that can be safely accessed from web browsers.

## Quick Start

The simplest way to enable CORS is to use the default configuration:

```cpp
#include "glaze/net/http_server.hpp"

glz::http_server server;

// Enable CORS with default settings (allows all origins)
server.enable_cors();

// Register your routes - OPTIONS routes are automatically created
server.get("/api/users", [](const glz::request& req, glz::response& res) {
    res.json({{"users", get_users()}});
});

server.post("/api/users", [](const glz::request& req, glz::response& res) {
    res.status(201).json({{"created", true}});
});

// The server now automatically handles:
// - OPTIONS /api/users (preflight requests)
// - CORS headers on GET and POST responses
```

## Automatic Preflight Handling

When CORS is enabled, Glaze automatically generates OPTIONS routes for all your endpoints. This eliminates the common "404 Not Found" errors for preflight requests.

### How It Works

1. **Browser sends preflight**: For certain cross-origin requests, browsers first send an OPTIONS request
2. **Automatic route matching**: Glaze automatically creates OPTIONS handlers for your routes
3. **Proper response**: Returns 204 No Content with appropriate CORS headers
4. **Actual request proceeds**: Browser then sends the real GET, POST, etc. request

### Example Flow

```cpp
glz::http_server server;

// Enable CORS first
server.enable_cors();

// Add routes - OPTIONS routes are auto-generated
server.get("/api/data", get_data_handler);
server.post("/api/data", post_data_handler);
server.put("/api/data/:id", update_data_handler);

// These OPTIONS routes are automatically available:
// - OPTIONS /api/data
// - OPTIONS /api/data/:id
```

## Configuration Options

### Basic CORS (Development)

For development environments where you want to allow all origins:

```cpp
// Allow all origins (default)
server.enable_cors();
```

### Custom Configuration

For more control over CORS behavior:

```cpp
glz::cors_config config;
config.allowed_origins = {"https://app.example.com", "https://admin.example.com"};
config.allowed_methods = {"GET", "POST", "PUT", "DELETE", "OPTIONS"};
config.allowed_headers = {"Content-Type", "Authorization", "X-API-Key"};
config.exposed_headers = {"X-Total-Count", "X-Page-Number"};
config.allow_credentials = true;
config.max_age = 3600; // Cache preflight for 1 hour

server.enable_cors(config);
```

### Restrictive CORS (Production)

For production environments with specific allowed origins:

```cpp
// Only allow specific origins
std::vector<std::string> allowed_origins = {
    "https://app.example.com",
    "https://admin.example.com"
};

// Second parameter enables credentials
server.enable_cors(allowed_origins, true);
```

## Configuration Reference

The `cors_config` struct supports the following options:

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `allowed_origins` | `vector<string>` | `{"*"}` | List of allowed origins or "*" for all |
| `allowed_methods` | `vector<string>` | Common methods | HTTP methods to allow |
| `allowed_headers` | `vector<string>` | Common headers | Headers clients can send |
| `exposed_headers` | `vector<string>` | Empty | Headers exposed to client code |
| `allow_credentials` | `bool` | `false` | Allow cookies/auth headers |
| `max_age` | `int` | `86400` | Preflight cache duration (seconds) |

## Advanced Usage

### Custom OPTIONS Handlers

While Glaze automatically generates OPTIONS routes, you can override specific endpoints with custom handlers:

```cpp
glz::http_server server;

// Enable CORS with default handlers
server.enable_cors();

// Most routes use auto-generated OPTIONS
server.get("/api/public", public_handler);

// But this endpoint needs custom OPTIONS handling
server.options("/api/special", [](const glz::request& req, glz::response& res) {
    res.status(204)
       .header("Access-Control-Allow-Origin", "https://special.example.com")
       .header("Access-Control-Allow-Methods", "GET, POST")
       .header("X-Custom-Header", "special-value");
});

server.get("/api/special", special_handler);
```

### Dynamic CORS Policies

You can implement dynamic CORS policies based on the request:

```cpp
// Create middleware for dynamic CORS
auto dynamic_cors = [](const glz::request& req, glz::response& res) {
    auto origin_it = req.headers.find("origin");
    if (origin_it == req.headers.end()) return;
    
    const std::string& origin = origin_it->second;
    
    // Check origin against database or configuration
    if (is_origin_allowed(origin)) {
        res.header("Access-Control-Allow-Origin", origin)
           .header("Access-Control-Allow-Credentials", "true");
    }
};

server.use(dynamic_cors);
```

### Per-Route CORS Configuration

For different CORS policies on different routes:

```cpp
glz::http_server server;

// Default CORS for most routes
glz::cors_config default_config;
default_config.allowed_origins = {"https://app.example.com"};
server.enable_cors(default_config);

// API v2 needs different CORS settings
server.options("/api/v2/*", [](const glz::request& req, glz::response& res) {
    // Custom CORS for API v2
    res.status(204)
       .header("Access-Control-Allow-Origin", "*")
       .header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, PATCH");
});
```

### Handling Complex Preflight Requests

For APIs that accept complex requests with custom headers:

```cpp
glz::cors_config config;

// Allow all standard and custom headers
config.allowed_headers = {
    "Content-Type",
    "Authorization", 
    "X-API-Key",
    "X-Request-ID",
    "X-Client-Version"
};

// Expose response headers to client JavaScript
config.exposed_headers = {
    "X-Total-Count",
    "X-Page-Number",
    "X-Rate-Limit-Remaining"
};

// Allow credentials for authenticated requests
config.allow_credentials = true;

// Cache preflight responses for 1 hour
config.max_age = 3600;

server.enable_cors(config);
```

## Working with Different Request Types

### Simple Requests

These requests don't trigger preflight:
- GET, HEAD, POST with simple headers
- Content-Type: application/x-www-form-urlencoded, multipart/form-data, or text/plain

```cpp
// Simple requests work without preflight
server.get("/api/public-data", [](const glz::request& req, glz::response& res) {
    res.json({{"data", "public"}});
});
```

### Preflighted Requests

These requests require preflight OPTIONS:
- PUT, DELETE, PATCH methods
- POST with Content-Type: application/json
- Any request with custom headers

```cpp
// These routes will have auto-generated OPTIONS handlers
server.put("/api/resource/:id", update_handler);
server.delete("/api/resource/:id", delete_handler);
server.patch("/api/resource/:id", patch_handler);
```

## Troubleshooting

### Common Issues

**1. CORS errors despite `enable_cors()`**
- Ensure CORS is enabled before registering routes
- Check that the origin is in the allowed list

**2. Preflight requests returning 404**
- Update to the latest version of Glaze
- Verify OPTIONS routes are being generated

**3. Credentials not working**
- Set `allow_credentials = true` in config
- Ensure origin is explicitly allowed (not "*")

**4. Headers not accessible in JavaScript**
- Add headers to `exposed_headers` in config

### Testing CORS

Test preflight requests with curl:

```bash
curl -X OPTIONS http://localhost:8080/api/users \
  -H "Origin: https://example.com" \
  -H "Access-Control-Request-Method: POST" \
  -H "Access-Control-Request-Headers: Content-Type, Authorization" \
  -v
```

Expected response:
```
HTTP/1.1 204 No Content
Access-Control-Allow-Origin: https://example.com
Access-Control-Allow-Methods: GET, POST, OPTIONS
Access-Control-Allow-Headers: Content-Type, Authorization
Access-Control-Max-Age: 86400
```

## Security Considerations

### Best Practices

1. **Never use wildcard origins in production**
   ```cpp
   // DON'T do this in production
   server.enable_cors();  // Allows all origins
   
   // DO this instead
   server.enable_cors({"https://app.example.com"}, true);
   ```

2. **Validate origins dynamically if needed**
   ```cpp
   auto validate_origin = [](const std::string& origin) {
       // Check against database or configuration
       return allowed_origins_db.contains(origin);
   };
   ```

3. **Limit allowed methods and headers**
   ```cpp
   config.allowed_methods = {"GET", "POST"};  // Only what you need
   config.allowed_headers = {"Content-Type"};  // Minimum required
   ```

4. **Use credentials carefully**
   - Only enable when necessary
   - Never use with wildcard origins
   - Implement proper authentication

### CORS and Authentication

When using authentication with CORS:

```cpp
glz::cors_config config;
config.allowed_origins = {"https://app.example.com"};
config.allow_credentials = true;  // Required for cookies/auth
config.allowed_headers = {"Content-Type", "Authorization"};

server.enable_cors(config);

// Authentication middleware
server.use([](const glz::request& req, glz::response& res) {
    // CORS headers are already set
    // Perform authentication here
    if (!is_authenticated(req)) {
        res.status(401).json({{"error", "Unauthorized"}});
    }
});
```

## Example: Complete CORS Setup

Here's a complete example of a production-ready CORS configuration:

```cpp
#include "glaze/net/http_server.hpp"
#include "glaze/net/cors.hpp"

int main() {
    glz::http_server server;
    
    // Configure CORS for production
    glz::cors_config cors_config;
    cors_config.allowed_origins = {
        "https://app.example.com",
        "https://admin.example.com"
    };
    cors_config.allowed_methods = {"GET", "POST", "PUT", "DELETE", "OPTIONS"};
    cors_config.allowed_headers = {
        "Content-Type",
        "Authorization",
        "X-API-Key",
        "X-Request-ID"
    };
    cors_config.exposed_headers = {
        "X-Total-Count",
        "X-RateLimit-Remaining"
    };
    cors_config.allow_credentials = true;
    cors_config.max_age = 3600;
    
    // Enable CORS before registering routes
    server.enable_cors(cors_config);
    
    // Register API routes - OPTIONS routes are auto-generated
    server.get("/api/users", [](const glz::request& req, glz::response& res) {
        res.json(get_users());
    });
    
    server.post("/api/users", [](const glz::request& req, glz::response& res) {
        User user;
        if (auto ec = glz::read_json(user, req.body)) {
            res.status(400).json({{"error", "Invalid JSON"}});
            return;
        }
        auto created = create_user(user);
        res.status(201).json(created);
    });
    
    server.put("/api/users/:id", [](const glz::request& req, glz::response& res) {
        auto id = req.params.at("id");
        User user;
        if (auto ec = glz::read_json(user, req.body)) {
            res.status(400).json({{"error", "Invalid JSON"}});
            return;
        }
        update_user(id, user);
        res.json(user);
    });
    
    server.del("/api/users/:id", [](const glz::request& req, glz::response& res) {
        auto id = req.params.at("id");
        delete_user(id);
        res.status(204);
    });
    
    // Start server
    server.bind("0.0.0.0", 8080);
    server.start();
    
    return 0;
}
```

## See Also

- [HTTP Server](http-server.md) - General HTTP server documentation
- [HTTP Router](http-router.md) - Routing and middleware details
- [REST Registry](rest-registry.md) - REST API patterns
- [MDN CORS Documentation](https://developer.mozilla.org/en-US/docs/Web/HTTP/CORS) - Browser CORS reference