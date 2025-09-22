# CORS Preflight Request Handling in Glaze

## Executive Summary

The Glaze HTTP server currently returns 404 errors for CORS preflight (OPTIONS) requests because route matching occurs before middleware execution. This document outlines a professional solution that automatically generates OPTIONS routes when CORS is enabled, aligning with industry best practices.

## The Problem

### Current Behavior
When a browser makes a cross-origin request with certain characteristics (custom headers, non-simple methods, etc.), it first sends a preflight OPTIONS request. In Glaze:

1. **Request arrives**: `OPTIONS /api/users`
2. **Route matching** (http_server.hpp:1146): No OPTIONS route exists
3. **404 Response** (http_server.hpp:1153): Server returns "Not Found"
4. **Middleware never runs**: CORS middleware can't handle the preflight

### Root Cause
```cpp
// Current execution flow in process_request():
auto [handle, params] = root_router.match(method, target);  // Line 1146

if (!handle) {
    send_error_response(socket, 404, "Not Found");          // Line 1153
    return;  // Middleware never gets called!
}

// Middleware only runs if a route was found
for (const auto& middleware : root_router.middlewares) {
    middleware(request, response);                          // Line 1163
}
```

## The Professional Solution: Automatic OPTIONS Route Generation

### Design Principles
- **Consistency**: All HTTP methods follow the same request pipeline
- **Transparency**: OPTIONS routes are explicit and visible in the routing table
- **Performance**: Leverages existing route optimization strategies
- **Flexibility**: Allows per-route CORS configurations and custom OPTIONS handlers

### Implementation Strategy

#### 1. Add OPTIONS Method Support to http_router
```cpp
// In http_router.hpp
inline http_router& options(std::string_view path, handler handle, 
                           const route_spec& spec = {}) {
    return route(http_method::OPTIONS, path, std::move(handle), spec);
}

inline http_router& options_async(std::string_view path, async_handler handle,
                                 const route_spec& spec = {}) {
    return route_async(http_method::OPTIONS, path, std::move(handle), spec);
}
```

#### 2. Enhanced CORS Enablement in http_server
```cpp
class http_server {
private:
    bool cors_enabled_ = false;
    cors_config cors_config_;
    
    // Handler for auto-generated OPTIONS routes
    handler cors_preflight_handler_;
    
    void create_preflight_handler() {
        cors_preflight_handler_ = [this](const request& req, response& res) {
            // Apply CORS headers using stored config
            auto cors_middleware = create_cors_middleware(cors_config_);
            cors_middleware(req, res);
            
            // Preflight always returns 204 No Content if successful
            if (res.status_code == 200) {
                res.status(204);
            }
        };
    }
    
    void auto_generate_options_routes() {
        std::unordered_set<std::string> paths_with_options;
        
        // Collect paths that already have OPTIONS handlers
        for (const auto& [path, methods] : root_router.routes) {
            if (methods.contains(http_method::OPTIONS)) {
                paths_with_options.insert(path);
            }
        }
        
        // Add OPTIONS routes for paths that don't have them
        for (const auto& [path, methods] : root_router.routes) {
            if (!paths_with_options.contains(path) && !methods.empty()) {
                root_router.options(path, cors_preflight_handler_);
            }
        }
        
        // Also handle direct routes (optimization structure)
        for (const auto& [path, method_handlers] : root_router.direct_routes) {
            if (!method_handlers.contains(http_method::OPTIONS)) {
                root_router.options(path, cors_preflight_handler_);
            }
        }
    }
    
public:
    http_server& enable_cors(const cors_config& config = {}) {
        cors_enabled_ = true;
        cors_config_ = config;
        
        // Create the preflight handler
        create_preflight_handler();
        
        // Add CORS middleware for actual requests
        root_router.use(create_cors_middleware(config));
        
        // Generate OPTIONS routes for existing paths
        auto_generate_options_routes();
        
        return *this;
    }
    
    // Override route methods to auto-add OPTIONS when CORS is enabled
    template<typename Handler>
    http_server& get(std::string_view path, Handler&& handler) {
        root_router.get(path, std::forward<Handler>(handler));
        
        if (cors_enabled_ && !has_options_route(path)) {
            root_router.options(path, cors_preflight_handler_);
        }
        
        return *this;
    }
    
    // Similar overrides for post(), put(), delete(), patch()
};
```

#### 3. Smart Route Registration
```cpp
// When new routes are added after CORS is enabled
http_server& route(http_method method, std::string_view path, handler handle) {
    root_router.route(method, path, std::move(handle));
    
    // Auto-add OPTIONS route if CORS is enabled and no OPTIONS exists
    if (cors_enabled_ && method != http_method::OPTIONS) {
        if (!has_options_route(path)) {
            root_router.options(path, cors_preflight_handler_);
        }
    }
    
    return *this;
}
```

## Usage Examples

### Basic CORS Setup
```cpp
glz::http_server server;

// Enable CORS - automatically creates OPTIONS routes
server.enable_cors();

// These routes automatically get OPTIONS handlers
server.get("/api/users", get_users_handler);
server.post("/api/users", create_user_handler);
// OPTIONS /api/users is automatically available
```

### Custom CORS Configuration
```cpp
glz::cors_config config;
config.allowed_origins = {"https://app.example.com", "https://admin.example.com"};
config.allowed_methods = {"GET", "POST", "PUT", "DELETE"};
config.allowed_headers = {"Content-Type", "Authorization", "X-API-Key"};
config.allow_credentials = true;
config.max_age = 3600;

server.enable_cors(config);
```

### Override Specific OPTIONS Handler
```cpp
// Enable default CORS
server.enable_cors();

// Most routes use automatic OPTIONS
server.get("/api/public", public_handler);

// But this endpoint needs custom OPTIONS handling
server.options("/api/special", [](const request& req, response& res) {
    res.header("Access-Control-Allow-Origin", "https://special.example.com")
       .header("Access-Control-Allow-Methods", "GET, POST")
       .header("X-Custom-Header", "special-value")
       .status(204);
});
```

## Benefits of This Approach

### 1. **Architectural Integrity**
- Maintains consistent request flow: Route → Middleware → Handler
- No special cases in core request processing
- OPTIONS is a first-class HTTP method

### 2. **Performance**
- Leverages existing route optimizations (O(1) for direct routes)
- No middleware execution for invalid paths
- Efficient route matching using radix tree

### 3. **Security**
- Middleware doesn't run for non-existent routes
- Clear audit trail of allowed OPTIONS endpoints
- Can apply authentication to specific OPTIONS routes if needed

### 4. **Developer Experience**
- OPTIONS routes visible in route listings
- Easy to test and debug
- Can override specific OPTIONS handlers
- Supports route-specific CORS policies

### 5. **Industry Alignment**
Follows patterns used by professional frameworks:
- **ASP.NET Core**: Auto-generates OPTIONS endpoints
- **FastAPI**: Creates implicit OPTIONS routes
- **Spring Boot**: Generates OPTIONS mappings
- **Express.js**: Supports explicit OPTIONS route definition

## Migration Guide

### For Existing Code
```cpp
// Before: Manual OPTIONS routes required
server.get("/api/users", handler);
server.options("/api/users", cors_handler);  // Had to add manually

// After: Automatic with CORS enabled
server.enable_cors();
server.get("/api/users", handler);  // OPTIONS route created automatically
```

### For Complex CORS Scenarios
```cpp
// Different CORS policies per path
server.enable_cors(default_config);

// Override for specific API section
auto api_v2_cors = create_cors_middleware(api_v2_config);
server.options("/api/v2/*", [api_v2_cors](const request& req, response& res) {
    api_v2_cors(req, res);
    res.status(204);
});
```

## Testing Considerations

### Verify OPTIONS Routes Exist
```cpp
// Test that OPTIONS routes are created
TEST_CASE("CORS enables OPTIONS routes") {
    glz::http_server server;
    server.enable_cors();
    server.get("/api/test", handler);
    
    // Should be able to handle OPTIONS request
    auto response = test_client.options("/api/test");
    REQUIRE(response.status == 204);
    REQUIRE(response.headers.contains("Access-Control-Allow-Methods"));
}
```

### Test Preflight Sequences
```cpp
TEST_CASE("Full CORS preflight flow") {
    // 1. Preflight request
    auto preflight = test_client.options("/api/users")
        .header("Origin", "https://example.com")
        .header("Access-Control-Request-Method", "POST")
        .send();
    
    REQUIRE(preflight.status == 204);
    REQUIRE(preflight.header("Access-Control-Allow-Origin") == "https://example.com");
    
    // 2. Actual request
    auto actual = test_client.post("/api/users")
        .header("Origin", "https://example.com")
        .json({{"name", "test"}})
        .send();
    
    REQUIRE(actual.status == 201);
}
```

## Performance Impact

- **Memory**: Minimal - one additional route entry per unique path
- **CPU**: No impact - same O(1) or O(log n) route matching
- **Network**: Reduces failed requests, improving overall efficiency

## Conclusion

This solution provides a robust, professional approach to CORS preflight handling that:
- Maintains architectural consistency
- Follows industry best practices
- Provides excellent performance
- Offers maximum flexibility
- Ensures backward compatibility

The automatic OPTIONS route generation ensures that CORS "just works" while maintaining the ability to customize behavior when needed. This approach has been proven successful in production frameworks and represents the current industry standard for handling CORS preflight requests.