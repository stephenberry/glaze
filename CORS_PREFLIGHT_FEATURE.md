# CORS Preflight Feature Implementation

## Overview
Added automatic OPTIONS route generation for CORS preflight request handling in Glaze HTTP server.

## Problem Solved
Previously, when CORS was enabled, browsers would send preflight OPTIONS requests that resulted in 404 errors because route matching occurred before middleware execution. This prevented cross-origin requests from working properly.

## Solution
Implemented automatic generation of OPTIONS routes when CORS is enabled, ensuring preflight requests receive proper 204 No Content responses with appropriate CORS headers.

## Key Changes

### 1. HTTP Router (`include/glaze/net/http_router.hpp`)
- Added `options()` and `options_async()` methods for registering OPTIONS routes
- Made `direct_routes` public to enable access from http_server

### 2. HTTP Server (`include/glaze/net/http_server.hpp`)
- Added CORS state tracking (cors_enabled_, cors_config_, cors_preflight_handler_)
- Implemented `create_preflight_handler()` to generate preflight responses
- Added `auto_generate_options_routes()` to create OPTIONS routes for existing paths
- Modified `enable_cors()` to trigger automatic OPTIONS route generation
- Updated route registration methods to auto-add OPTIONS when CORS is enabled
- Added `get_router()` accessor for testing purposes

### 3. Tests (`tests/networking_tests/cors_preflight_test/`)
- Created comprehensive test suite with 9 test cases
- All tests passing, no regressions in existing tests

### 4. Documentation (`docs/networking/cors.md`)
- Added comprehensive CORS documentation
- Updated http-server.md and http-router.md with CORS references
- Added to mkdocs.yml navigation

## Usage Example

```cpp
glz::http_server server;

// Enable CORS - automatically creates OPTIONS routes
server.enable_cors();

// These routes automatically get OPTIONS handlers
server.get("/api/users", get_users_handler);
server.post("/api/users", create_user_handler);
// OPTIONS /api/users is automatically available
```

## Benefits
- Eliminates 404 errors for CORS preflight requests
- Zero configuration required for basic CORS support
- Maintains backward compatibility
- Follows industry best practices (similar to ASP.NET Core, FastAPI, Spring Boot)
- Allows custom OPTIONS handlers when needed

## Migration
No migration required. Existing code continues to work. To benefit from automatic preflight handling, simply ensure `enable_cors()` is called before registering routes.