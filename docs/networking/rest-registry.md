# REST Registry

The Glaze REST Registry automatically generates REST API endpoints from C++ classes using reflection. This eliminates boilerplate code and ensures consistency between your C++ domain models and REST APIs.

## Overview

The REST Registry:
- **Auto-generates** REST endpoints from C++ classes
- **Maps methods** to appropriate HTTP verbs (GET/POST/PUT/DELETE)  
- **Handles JSON** serialization/deserialization automatically
- **Validates input** using Glaze's JSON validation
- **Supports** complex nested objects and collections
- **Provides** type-safe API generation

> [!NOTE]
>
> Glaze's REST registry is actually format agnostic and can also be used with binary formats other than JSON.

## Quick Start

### Basic Service Registration

```cpp
#include "glaze/rpc/registry.hpp"
#include "glaze/net/http_server.hpp"

struct UserService {
    std::vector<User> getAllUsers() { 
        return users; 
    }
    
    User getUserById(int id) {
        // Find and return user
    }
    
    User createUser(const User& user) {
        // Create and return new user
    }
    
private:
    std::vector<User> users;
};

int main() {
    glz::http_server server;
    UserService userService;
    
    // Create REST registry
    glz::registry<glz::opts{}, glz::REST> registry;
    
    // Register service - auto-generates endpoints
    registry.on(userService);
    
    // Mount generated endpoints
    server.mount("/api", registry.endpoints);
    
    server.bind(8080).with_signals();
    server.start();
    server.wait_for_signal();
}
```

This automatically creates:
- `GET /api/getAllUsers` → `getAllUsers()`
- `POST /api/getUserById` → `getUserById(int)`  
- `POST /api/createUser` → `createUser(const User&)`

## Service Definition

### Method Types

The registry supports different method signatures:

```cpp
struct BookService {
    // No parameters - GET endpoint
    std::vector<Book> getAllBooks() {
        return books;
    }
    
    // Single parameter - POST endpoint with JSON body
    Book getBookById(int id) {
        return find_book(id);
    }
    
    // Object parameter - POST endpoint with object body
    Book createBook(const Book& book) {
        books.push_back(book);
        return book;
    }
    
    // Update method - PUT endpoint  
    Book updateBook(const BookUpdate& update) {
        auto& book = find_book(update.id);
        book.title = update.title;
        book.author = update.author;
        return book;
    }
    
    // Void return - POST endpoint, 204 No Content response
    void deleteBook(int id) {
        remove_book(id);
    }
    
private:
    std::vector<Book> books;
};
```

### Request/Response Objects

```cpp
// Domain objects for API
struct Book {
    int id;
    std::string title;
    std::string author;
    std::string isbn;
    int year;
};

struct BookUpdate {
    int id;
    std::string title;
    std::string author;
};

struct BookSearchRequest {
    std::string query;
    int limit = 10;
    int offset = 0;
};

struct BookSearchResponse {
    std::vector<Book> books;
    int total_count;
    bool has_more;
};
```

## HTTP Mapping

### Automatic Method Mapping

The registry maps C++ methods to HTTP verbs based on patterns:

| Method Pattern | HTTP Verb | Description |
|---------------|-----------|-------------|
| `get*()` | GET | Read operations, no parameters |
| `find*()` | GET | Read operations, no parameters |
| `list*()` | GET | Read operations, no parameters |
| `*()` with params | POST | Operations with input data |
| `create*()` | POST | Create operations |
| `update*()` | PUT | Update operations |
| `delete*()` | DELETE | Delete operations |

### Custom Mapping

```cpp
struct CustomService {
    // These all become POST endpoints due to parameters
    UserStats getUserStats(const StatsRequest& req) { /*...*/ }
    SearchResults searchUsers(const SearchQuery& query) { /*...*/ }
    ValidationResult validateUser(const User& user) { /*...*/ }
    
    // These become GET endpoints (no parameters)
    std::vector<User> getAllActiveUsers() { /*...*/ }
    ServerStatus getServerStatus() { /*...*/ }
    std::string getVersion() { /*...*/ }
};
```

## Advanced Features

### Error Handling

```cpp
struct SafeUserService {
    User getUserById(int id) {
        if (id <= 0) {
            throw std::invalid_argument("Invalid user ID");
        }
        
        auto it = users.find(id);
        if (it == users.end()) {
            throw std::runtime_error("User not found");
        }
        
        return it->second;
    }
    
    // Registry automatically converts exceptions to HTTP errors:
    // std::invalid_argument -> 400 Bad Request
    // std::runtime_error -> 500 Internal Server Error
    
private:
    std::unordered_map<int, User> users;
};
```

## Multiple Services

### Service Composition

```cpp
int main() {
    glz::http_server server;
    
    // Multiple service instances
    UserService userService;
    ProductService productService;
    OrderService orderService;
    
    // Single registry for all services
    glz::registry<glz::opts{}, glz::REST> registry;
    
    // Register all services
    registry.on(userService);
    registry.on(productService);  
    registry.on(orderService);
    
    // All endpoints available under /api
    server.mount("/api", registry.endpoints);
    
    server.bind(8080).with_signals();
    server.start();
    server.wait_for_signal();
}
```

## Customization

### Custom Serialization Options

```cpp
// Pretty JSON formatting
constexpr auto pretty_opts = glz::opts{.prettify = true};
glz::registry<pretty_opts, glz::REST> registry;
```

## API Documentation Generation

### Endpoint Discovery

```cpp
// Add endpoint listing for API discovery
server.get("/api", [&registry](const glz::request&, glz::response& res) {
    std::vector<std::string> endpoints;
    
    for (const auto& [path, methods] : registry.endpoints.routes) {
        for (const auto& [method, handler] : methods) {
            endpoints.push_back(glz::to_string(method) + " " + path);
        }
    }
    
    res.json({{"endpoints", endpoints}});
});
```
