# HTTP Router

The Glaze HTTP router provides efficient path matching using a radix tree data structure, supporting static routes, parameterized routes, wildcards, and parameter validation.

## Overview

The router supports:
- **Static routes** - Exact path matching
- **Parameter routes** - Dynamic path segments with `:param`
- **Wildcard routes** - Catch-all segments with `*param`  
- **Parameter constraints** - Pattern validation for parameters
- **Middleware** - Cross-cutting request/response processing
- **Efficient matching** - O(log n) performance using radix tree

## Basic Routing

### Static Routes

```cpp
glz::http_router router;

// Simple static routes
router.get("/", home_handler);
router.get("/about", about_handler);
router.get("/contact", contact_handler);

// Nested static routes
router.get("/api/v1/status", status_handler);
router.get("/api/v1/health", health_handler);
```

### HTTP Methods

```cpp
// Standard HTTP methods
router.get("/users", get_users);           // GET
router.post("/users", create_user);        // POST
router.put("/users/:id", update_user);     // PUT
router.del("/users/:id", delete_user);     // DELETE
router.patch("/users/:id", patch_user);    // PATCH

// Generic route method
router.route(glz::http_method::HEAD, "/users", head_users);
router.route(glz::http_method::OPTIONS, "/users", options_users);
```

## Parameter Routes

### Path Parameters

```cpp
// Single parameter
router.get("/users/:id", [](const glz::request& req, glz::response& res) {
    std::string user_id = req.params.at("id");
    res.json(get_user(user_id));
});

// Multiple parameters
router.get("/users/:user_id/posts/:post_id", 
    [](const glz::request& req, glz::response& res) {
        std::string user_id = req.params.at("user_id");
        std::string post_id = req.params.at("post_id");
        res.json(get_user_post(user_id, post_id));
    });

// Mixed static and parameter segments
router.get("/api/v1/users/:id/profile", profile_handler);
```

### Parameter Constraints

Glaze provides a `validation` function, which allows users to implement high performance validation logic using regex libraries or custom parsing.

Examples:

```cpp
// Numeric constraint
glz::param_constraint numeric{
    .description = "Must be a positive integer",
    .validation = [](std::string_view value) {
        if (value.empty()) return false;
        for (char c : value) {
            if (!std::isdigit(c)) return false;
        }
        return true;
    }
};

router.get("/users/:id", user_handler, {{"id", numeric}});

// UUID constraint using regex
glz::param_constraint uuid{
    .description = "Must be a valid UUID",
    .validation = [](std::string_view value) {
        std::regex uuid_regex(R"([0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12})");
        return std::regex_match(std::string(value), uuid_regex);
    }
};

router.get("/sessions/:session_id", session_handler, {{"session_id", uuid}});

// Alphanumeric constraint
glz::param_constraint username{
    .description = "Username: 3-20 alphanumeric characters or underscore",
    .validation = [](std::string_view value) {
        if (value.size() < 3 || value.size() > 20) return false;
        for (char c : value) {
            if (!std::isalnum(c) && c != '_') return false;
        }
        return true;
    }
};

router.get("/profile/:username", profile_handler, {{"username", username}});
```

### Advanced Validation Functions

Validation functions provide flexible parameter validation:

```cpp
// File extension check
glz::param_constraint any_extension{
    .description = "Text files only",
    .validation = [](std::string_view value) {
        return value.ends_with(".txt");
    }
};

// Hex color code validation
glz::param_constraint hex_color{
    .description = "Valid hex color code",
    .validation = [](std::string_view value) {
        if (value.size() != 7 || value[0] != '#') return false;
        for (size_t i = 1; i < value.size(); ++i) {
            if (!std::isxdigit(value[i])) return false;
        }
        return true;
    }
};

// Exact match validation
glz::param_constraint exact_match{
    .description = "Must be exactly 'admin'",
    .validation = [](std::string_view value) {
        return value == "admin";
    }
};

// Year range validation
glz::param_constraint year{
    .description = "4-digit year starting with 2",
    .validation = [](std::string_view value) {
        if (value.size() != 4 || value[0] != '2') return false;
        for (char c : value) {
            if (!std::isdigit(c)) return false;
        }
        return true;
    }
};
```

## Wildcard Routes

### Catch-All Parameters

```cpp
// Static file serving
router.get("/static/*path", [](const glz::request& req, glz::response& res) {
    std::string file_path = req.params.at("path");
    // file_path contains everything after /static/
    serve_file("public/" + file_path, res);
});

// API versioning catch-all
router.get("/api/*version", [](const glz::request& req, glz::response& res) {
    std::string version = req.params.at("version");
    // Handle all API versions dynamically
    handle_api_request(version, req, res);
});
```

### Wildcard Constraints

```cpp
// Constrain wildcard content
glz::param_constraint safe_path{
    .description = "Safe file path",
    .validation = [](std::string_view value) {
        for (char c : value) {
            if (!(std::isalnum(c) || c == '/' || c == '.' || c == '_' || c == '-')) return false;
        }
        return true;
    }
};

router.get("/files/*path", file_handler, {{"path", safe_path}});
```

## Route Priority

The router matches routes in priority order:

1. **Static routes** (highest priority)
2. **Parameter routes** 
3. **Wildcard routes** (lowest priority)

```cpp
// These routes are checked in priority order:
router.get("/users/admin", admin_handler);        // 1. Static (exact match)
router.get("/users/:id", user_handler);           // 2. Parameter
router.get("/users/*action", user_action);        // 3. Wildcard

// Request "/users/admin" matches admin_handler
// Request "/users/123" matches user_handler  
// Request "/users/edit/profile" matches user_action
```

## Middleware

### Global Middleware

```cpp
// Logging middleware
router.use([](const glz::request& req, glz::response& res) {
    auto start = std::chrono::high_resolution_clock::now();
    
    // Log request
    std::cout << glz::to_string(req.method) << " " << req.target 
              << " from " << req.remote_ip << std::endl;
});

// Authentication middleware
router.use([](const glz::request& req, glz::response& res) {
    if (requires_auth(req.target)) {
        auto auth_header = req.headers.find("Authorization");
        if (auth_header == req.headers.end()) {
            res.status(401).json({{"error", "Authentication required"}});
            return;
        }
        
        if (!validate_token(auth_header->second)) {
            res.status(403).json({{"error", "Invalid token"}});
            return;
        }
    }
});
```

### Middleware Execution Order

```cpp
// Middleware executes in registration order
router.use(logging_middleware);       // 1. First
router.use(auth_middleware);          // 2. Second  
router.use(rate_limit_middleware);    // 3. Third

// Then route handler executes
router.get("/api/data", data_handler); // 4. Finally
```

### Conditional Middleware

```cpp
// Apply middleware only to specific paths
router.use([](const glz::request& req, glz::response& res) {
    if (req.target.starts_with("/admin/")) {
        // Admin-only middleware
        if (!is_admin_user(req)) {
            res.status(403).json({{"error", "Admin access required"}});
            return;
        }
    }
});
```

## Route Groups

### Manual Grouping

```cpp
// API v1 routes
void setup_api_v1(glz::http_router& router) {
    router.get("/api/v1/users", get_users_v1);
    router.post("/api/v1/users", create_user_v1);
    router.get("/api/v1/users/:id", get_user_v1);
}

// API v2 routes  
void setup_api_v2(glz::http_router& router) {
    router.get("/api/v2/users", get_users_v2);
    router.post("/api/v2/users", create_user_v2);
    router.get("/api/v2/users/:id", get_user_v2);
}

// Main router setup
glz::http_router router;
setup_api_v1(router);
setup_api_v2(router);
```

### Sub-router Mounting

```cpp
// Create specialized routers
glz::http_router api_router;
api_router.get("/users", get_users);
api_router.post("/users", create_user);

glz::http_router admin_router;
admin_router.get("/dashboard", admin_dashboard);
admin_router.get("/settings", admin_settings);

// Mount on main server
glz::http_server server;
server.mount("/api", api_router);
server.mount("/admin", admin_router);

// Results in routes:
// GET /api/users
// POST /api/users
// GET /admin/dashboard  
// GET /admin/settings
```

## Async Handlers

### Async Route Handlers

```cpp
// Async handlers return std::future<void>
router.get_async("/slow-data", [](const glz::request& req, glz::response& res) -> std::future<void> {
    return std::async([&req, &res]() {
        // Simulate slow operation
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        auto data = fetch_slow_data();
        res.json(data);
    });
});

// Convert regular handler to async
router.route_async(glz::http_method::POST, "/async-upload", 
    [](const glz::request& req, glz::response& res) -> std::future<void> {
        return std::async([&]() {
            process_large_upload(req.body);
            res.status(204);  // No content
        });
    });
```

## Route Debugging

### Tree Visualization

```cpp
glz::http_router router;
// ... add routes ...

// Print the routing tree structure
router.print_tree();

/* Output example:
Radix Tree Structure:
Node[api, endpoint=false, children=1, full_path=/api]
  Node[PARAM:version, endpoint=false, children=0+param, full_path=/api/:version]
    Node[users, endpoint=true, children=0, full_path=/api/:version/users]
      Handlers: GET POST 
      Constraints for GET:
        version: v[0-9]+ (API version like v1, v2)
*/
```

### Route Testing

```cpp
// Test route matching
auto [handler, params] = router.match(glz::http_method::GET, "/api/v1/users");

if (handler) {
    std::cout << "Route matched!" << std::endl;
    for (const auto& [key, value] : params) {
        std::cout << key << " = " << value << std::endl;
    }
} else {
    std::cout << "No route matched" << std::endl;
}
```

## Performance Optimization

### Route Organization

```cpp
// Place most frequently accessed routes first
router.get("/", home_handler);           // High traffic
router.get("/api/health", health_check); // Health checks

// Group related routes together
router.get("/api/users", get_users);
router.get("/api/users/:id", get_user);
router.post("/api/users", create_user);

// Place wildcard routes last
router.get("/static/*path", static_files); // Catch-all
```

### Direct Route Optimization

```cpp
// The router automatically optimizes non-parameterized routes
// These are stored in a direct lookup table for O(1) access:

router.get("/api/status", status_handler);     // O(1) lookup
router.get("/api/health", health_handler);     // O(1) lookup

// Parameterized routes use radix tree for O(log n) lookup:
router.get("/api/users/:id", user_handler);    // O(log n) lookup
```

## Error Handling

### Route Handler Errors

```cpp
router.get("/api/data", [](const glz::request& req, glz::response& res) {
    try {
        auto data = get_data_that_might_throw();
        res.json(data);
    } catch (const database_error& e) {
        res.status(503).json({{"error", "Database unavailable"}});
    } catch (const validation_error& e) {
        res.status(400).json({{"error", e.what()}});
    } catch (const std::exception& e) {
        res.status(500).json({{"error", "Internal server error"}});
    }
});
```

### Route Conflicts

```cpp
// The router detects and prevents route conflicts:
router.get("/users/:id", user_handler);
router.get("/users/:user_id", another_handler); // Error: parameter name conflict

// This would throw: 
// std::runtime_error("Route conflict: different parameter names at same position")
```

## Best Practices

### Parameter Validation

```cpp
// Always validate parameters from untrusted input
router.get("/users/:id", [](const glz::request& req, glz::response& res) {
    auto id_str = req.params.at("id");
    
    try {
        int id = std::stoi(id_str);
        if (id <= 0) {
            res.status(400).json({{"error", "User ID must be positive"}});
            return;
        }
        
        auto user = get_user(id);
        res.json(user);
    } catch (const std::invalid_argument&) {
        res.status(400).json({{"error", "Invalid user ID format"}});
    }
});
```

### Resource-based Routing

```cpp
// Follow RESTful conventions
router.get("/users", get_users);           // GET /users - list
router.post("/users", create_user);        // POST /users - create
router.get("/users/:id", get_user);        // GET /users/123 - read
router.put("/users/:id", update_user);     // PUT /users/123 - update
router.del("/users/:id", delete_user);     // DELETE /users/123 - delete

// Nested resources
router.get("/users/:id/posts", get_user_posts);
router.post("/users/:id/posts", create_user_post);
```
