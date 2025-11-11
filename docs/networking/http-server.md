# HTTP Server

The Glaze HTTP server provides a high-performance, async HTTP server implementation using ASIO.

## Basic Usage

### Creating a Server

```cpp
#include "glaze/net/http_server.hpp"
#include <iostream>

glz::http_server server;

// Configure routes
server.get("/hello", [](const glz::request& /*req*/, glz::response& res) {
    res.body("Hello, World!");
});

// Note: start() is non-blocking; block main until shutdown
server.bind("127.0.0.1", 8080)
      .with_signals(); // handle Ctrl+C (SIGINT)

std::cout << "Server running on http://127.0.0.1:8080\n";
std::cout << "Press Ctrl+C to stop\n";

server.start();
server.wait_for_signal();
```

### HTTPS Server

```cpp
#include "glaze/net/http_server.hpp"

// Create HTTPS server (template parameter enables TLS)
glz::https_server server;  // or glz::http_server<true>

// Load SSL certificates
server.load_certificate("cert.pem", "key.pem");
server.set_ssl_verify_mode(0); // No client verification

server.get("/secure", [](const glz::request& req, glz::response& res) {
    res.json({{"secure", true}, {"message", "This is HTTPS!"}});
});

server.bind(8443);
server.start();
```

## Server Configuration

### Binding and Ports

```cpp
// Bind to specific address and port
server.bind("127.0.0.1", 8080);

// Bind to all interfaces
server.bind("0.0.0.0", 8080);

// Bind to port only (defaults to all interfaces)
server.bind(8080);

// IPv6 support
server.bind("::1", 8080);  // localhost IPv6
server.bind("::", 8080);   // all IPv6 interfaces
```

### Thread Pool Configuration

```cpp
// Start with specific number of threads
server.start(4);  // 4 worker threads

// Start with hardware concurrency (default)
server.start();   // Uses std::thread::hardware_concurrency()
```

### Error Handling

```cpp
server.on_error([](std::error_code ec, std::source_location loc) {
    std::fprintf(stderr, "Server error at %s:%d: %s\n", 
                loc.file_name(), loc.line(), ec.message().c_str());
});
```

## Route Registration

### Basic Routes

```cpp
// GET route
server.get("/users", [](const glz::request& req, glz::response& res) {
    res.json(get_all_users());
});

// POST route
server.post("/users", [](const glz::request& req, glz::response& res) {
    User user;
    if (auto ec = glz::read_json(user, req.body)) {
        res.status(400).body("Invalid JSON");
        return;
    }
    create_user(user);
    res.status(201).json(user);
});

// Multiple HTTP methods
server.route(glz::http_method::PUT, "/users/:id", handler);
server.route(glz::http_method::DELETE, "/users/:id", handler);
```

### Route Parameters

```cpp
// Path parameters with :param syntax
server.get("/users/:id", [](const glz::request& req, glz::response& res) {
    int user_id = std::stoi(req.params.at("id"));
    res.json(get_user_by_id(user_id));
});

// Multiple parameters
server.get("/users/:user_id/posts/:post_id", 
    [](const glz::request& req, glz::response& res) {
        int user_id = std::stoi(req.params.at("user_id"));
        int post_id = std::stoi(req.params.at("post_id"));
        res.json(get_user_post(user_id, post_id));
    });

// Wildcard parameters with *param syntax
server.get("/files/*path", [](const glz::request& req, glz::response& res) {
    std::string file_path = req.params.at("path");
    serve_static_file(file_path, res);
});
```

### Parameter Constraints

```cpp
// Numeric ID constraint
glz::param_constraint id_constraint{
    .description = "User ID must be numeric",
    .validation = [](std::string_view value) {
        if (value.empty()) return false;
        for (char c : value) {
            if (!std::isdigit(c)) return false;
        }
        return true;
    }
};

server.get("/users/:id", handler, {{"id", id_constraint}});

// Email constraint
glz::param_constraint email_constraint{
    .description = "Valid email address required",
    .validation = [](std::string_view value) {
        std::regex email_regex(R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})");
        return std::regex_match(std::string(value), email_regex);
    }
};
```

## Response Handling

### Response Builder

```cpp
server.get("/api/data", [](const glz::request& req, glz::response& res) {
    // Set status code
    res.status(200);
    
    // Set headers
    res.header("X-Custom-Header", "value");
    res.content_type("application/json");
    
    // Set body
    res.body("{\"message\": \"Hello\"}");
    
    // Method chaining
    res.status(201)
       .header("Location", "/api/data/123")
       .json({{"id", 123}, {"created", true}});
});
```

### JSON Responses

```cpp
// Automatic JSON serialization
struct User {
    int id;
    std::string name;
    std::string email;
};

server.get("/users/:id", [](const glz::request& req, glz::response& res) {
    User user = get_user(std::stoi(req.params.at("id")));
    res.json(user);  // Automatically serializes to JSON
});

// Using glz::opts for custom serialization
server.get("/users/:id/detailed", [](const glz::request& req, glz::response& res) {
    User user = get_user(std::stoi(req.params.at("id")));
    res.body<glz::opts{.prettify = true}>(user);  // Pretty-printed JSON
});
```

### Error Responses

```cpp
server.get("/users/:id", [](const glz::request& req, glz::response& res) {
    try {
        int id = std::stoi(req.params.at("id"));
        User user = get_user(id);
        
        if (user.id == 0) {
            res.status(404).json({{"error", "User not found"}});
            return;
        }
        
        res.json(user);
    } catch (const std::exception& e) {
        res.status(400).json({{"error", "Invalid user ID"}});
    }
});
```

## Middleware

### Adding Middleware

```cpp
// Logging middleware
server.use([](const glz::request& req, glz::response& res) {
    std::cout << glz::to_string(req.method) << " " << req.target << std::endl;
});

// Authentication middleware
server.use([](const glz::request& req, glz::response& res) {
    // Note: Header names are case-insensitive (RFC 7230)
    if (req.headers.find("authorization") == req.headers.end()) {
        res.status(401).json({{"error", "Authorization required"}});
        return;
    }
    // Validate token...
});

// CORS middleware (built-in)
server.enable_cors();
```

### Custom Middleware

```cpp
// Rate limiting middleware
auto rate_limiter = [](const glz::request& req, glz::response& res) {
    static std::unordered_map<std::string, int> request_counts;
    
    auto& count = request_counts[req.remote_ip];
    if (++count > 100) {  // 100 requests per IP
        res.status(429).json({{"error", "Rate limit exceeded"}});
        return;
    }
};

server.use(rate_limiter);
```

### Wrapping Middleware

Wrapping middleware executes around handlers, allowing code execution both before and after the handler completes.

```cpp
// Timing and metrics middleware
server.wrap([](const glz::request& req, glz::response& res, const auto& next) {
    auto start = std::chrono::steady_clock::now();

    next(); // Execute next middleware or handler

    auto duration = std::chrono::steady_clock::now() - start;
    std::cout << req.target << " completed in "
              << std::chrono::duration_cast<std::chrono::milliseconds>(duration).count()
              << "ms\n";
});
```

#### ⚠️ Safety Requirements

**The `next()` handler MUST be called synchronously.** The `next` parameter is intentionally non-copyable and non-movable to prevent accidental storage and asynchronous invocation, which would cause dangling references.

✅ **Correct - Synchronous execution:**
```cpp
server.wrap([](const glz::request& req, glz::response& res, const auto& next) {
    // Do work before
    next(); // Call synchronously
    // Do work after
});
```

❌ **Incorrect - Will not compile:**
```cpp
server.wrap([](const glz::request& req, glz::response& res, const auto& next) {
    // ✗ COMPILE ERROR: next is non-copyable
    std::thread([next]() {
        next();
    }).detach();
});
```

For asynchronous operations, complete them **before** or **after** calling `next()`:
```cpp
server.wrap([](const glz::request& req, glz::response& res, const auto& next) {
    // Async work BEFORE handler
    auto data = fetch_data_async().get();

    next(); // Synchronous handler execution

    // Fire-and-forget async work AFTER (using copied values only)
    std::async([status = res.status_code]() {
        log_to_remote(status);
    });
});
```

#### Use Cases

**Request/Response Timing:**
```cpp
struct Metrics {
    std::atomic<uint64_t> total_requests{0};
    std::atomic<uint64_t> total_responses{0};
    std::atomic<double> response_time_sum{0.0};
};

Metrics metrics;

server.wrap([&metrics](const glz::request&, glz::response& res, const auto& next) {
    metrics.total_requests++;

    auto start = std::chrono::steady_clock::now();
    next();
    auto duration = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();

    metrics.response_time_sum += duration;
    metrics.total_responses++;
});
```

**Error Handling:**
```cpp
server.wrap([](const glz::request&, glz::response& res, const auto& next) {
    try {
        next();
    } catch (const std::exception& e) {
        res.status(500).json({{"error", "Internal server error"}});
        log_error(e.what());
    }
});
```

**Response Transformation:**
```cpp
server.wrap([](const glz::request&, glz::response& res, const auto& next) {
    next();

    // Add security headers after handler completes
    res.header("X-Content-Type-Options", "nosniff");
    res.header("X-Frame-Options", "DENY");
    res.header("X-XSS-Protection", "1; mode=block");
});
```

**Logging with Full Context:**
```cpp
server.wrap([](const glz::request& req, glz::response& res, const auto& next) {
    auto start = std::chrono::steady_clock::now();

    next();

    auto duration = std::chrono::steady_clock::now() - start;
    log_request(req.method, req.target, res.status_code, duration);
});
```

#### Execution Order

Multiple wrapping middleware execute in the order they are registered:

```cpp
server.wrap([](const glz::request&, glz::response&, const auto& next) {
    std::cout << "Middleware 1 before\n";
    next();
    std::cout << "Middleware 1 after\n";
});

server.wrap([](const glz::request&, glz::response&, const auto& next) {
    std::cout << "Middleware 2 before\n";
    next();
    std::cout << "Middleware 2 after\n";
});

// Output order:
// Middleware 1 before
// Middleware 2 before
// Handler executes
// Middleware 2 after
// Middleware 1 after
```

## Mounting Sub-routers

```cpp
// Create sub-router for API v1
glz::http_router api_v1;
api_v1.get("/users", get_users_handler);
api_v1.post("/users", create_user_handler);

// Create sub-router for API v2  
glz::http_router api_v2;
api_v2.get("/users", get_users_v2_handler);

// Mount sub-routers
server.mount("/api/v1", api_v1);
server.mount("/api/v2", api_v2);

// Now routes are available at:
// GET /api/v1/users
// POST /api/v1/users  
// GET /api/v2/users
```

## Static File Serving

```cpp
// Serve static files from directory
server.get("/static/*path", [](const glz::request& req, glz::response& res) {
    std::string file_path = "public/" + req.params.at("path");
    
    std::ifstream file(file_path);
    if (!file.is_open()) {
        res.status(404).body("File not found");
        return;
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    
    // Set appropriate content type
    if (file_path.ends_with(".html")) {
        res.content_type("text/html");
    } else if (file_path.ends_with(".css")) {
        res.content_type("text/css");
    } else if (file_path.ends_with(".js")) {
        res.content_type("application/javascript");
    }
    
    res.body(content);
});
```

## Server Lifecycle

### Starting and Stopping

```cpp
// Simple server with signal handling
int main() {
    glz::http_server server;
    
    // Configure routes
    server.get("/api/hello", [](const glz::request&, glz::response& res) {
        res.json({{"message", "Hello, World!"}});
    });
    
    // Bind and enable signal handling
    server.bind(8080)
          .with_signals();
    
    // Start server
    server.start();
    
    // Wait for shutdown signal
    server.wait_for_signal();
    
    return 0;
}
```

For integration into larger applications:

```cpp
class APIServer {
    glz::http_server server_;
    
public:
    bool start(uint16_t port) {
        try {
            configure_routes();
            server_.bind(port)
                   .with_signals();  // Enable signal handling
            
            // Start server (non-blocking)
            server_.start();
            
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Failed to start server: " << e.what() << std::endl;
            return false;
        }
    }
    
    void wait_for_shutdown() {
        server_.wait_for_signal();
    }
    
    void stop() {
        server_.stop();
    }
    
private:
    void configure_routes() {
        server_.get("/health", [](const glz::request&, glz::response& res) {
            res.json({{"status", "healthy"}});
        });
    }
};
```

### Graceful Shutdown

```cpp
// Built-in signal handling (recommended approach)
int main() {
    glz::http_server server;
    
    // Configure routes
    setup_routes(server);
    
    // Enable signal handling for graceful shutdown (handles SIGINT/SIGTERM)
    server.bind(8080)
          .with_signals();
    
    std::cout << "Server running on http://localhost:8080" << std::endl;
    std::cout << "Press Ctrl+C to gracefully shut down the server" << std::endl;
    
    // Start server
    server.start();
    
    // Wait for shutdown signal (blocks until server stops)
    server.wait_for_signal();
    
    std::cout << "Server shut down successfully" << std::endl;
    return 0;
}
```

For more control, you can still implement custom signal handling:

```cpp
#include <csignal>

std::atomic<bool> running{true};
glz::http_server server;

void signal_handler(int signal) {
    std::cout << "Received signal " << signal << ", shutting down..." << std::endl;
    running = false;
    server.stop();
}

int main() {
    // Register signal handlers
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    // Configure server
    setup_routes(server);
    server.bind(8080);
    
    // Start server
    std::future<void> server_future = std::async(std::launch::async, [&]() {
        server.start();
    });
    
    // Wait for shutdown signal
    while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    server_future.wait();
    return 0;
}
```

## Production Configuration

### Performance Tuning

```cpp
// Optimize for production
server.bind("0.0.0.0", 8080);

// Use multiple threads (typically CPU cores)
int num_threads = std::thread::hardware_concurrency();
server.start(num_threads);

// Configure error handling for production
server.on_error([](std::error_code ec, std::source_location loc) {
    // Log to file/service instead of stderr
    if (should_log_error(ec)) {
        log_error(ec, loc);
    }
});
```

### Health Checks

```cpp
// Health check endpoint
server.get("/health", [](const glz::request&, glz::response& res) {
    // Check database connection, external services, etc.
    bool healthy = check_database() && check_external_apis();
    
    if (healthy) {
        res.json({
            {"status", "healthy"},
            {"timestamp", get_timestamp()},
            {"version", VERSION}
        });
    } else {
        res.status(503).json({
            {"status", "unhealthy"},
            {"timestamp", get_timestamp()}
        });
    }
});

// Readiness check for Kubernetes
server.get("/ready", [](const glz::request&, glz::response& res) {
    if (is_ready_to_serve()) {
        res.json({{"status", "ready"}});
    } else {
        res.status(503).json({{"status", "not ready"}});
    }
});
```
