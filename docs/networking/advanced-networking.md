# Advanced Networking

This guide covers advanced Glaze HTTP features including CORS support, WebSocket functionality, SSL/TLS encryption, and HTTP client capabilities.

## CORS (Cross-Origin Resource Sharing)

### Quick CORS Setup

```cpp
#include "glaze/net/http_server.hpp"

glz::http_server server;

// Enable CORS with default settings (allows all origins)
server.enable_cors();

// Good for development, allows all origins and methods
server.bind(8080).with_signals();
server.start();
server.wait_for_signal(); // Block until Ctrl+C
```

### Production CORS Configuration

```cpp
// Restrict CORS to specific origins
std::vector<std::string> allowed_origins = {
    "https://myapp.com",
    "https://api.myapp.com",
    "https://admin.myapp.com"
};

server.enable_cors(allowed_origins, true); // Allow credentials

// Or use detailed configuration
glz::cors_config config;
config.allowed_origins = {"https://myapp.com", "https://app.myapp.com"};
config.allowed_origins_validator = [](std::string_view origin) {
    return origin.ends_with(".partner.myapp.com");
};
config.allowed_methods = {"GET", "POST", "PUT", "DELETE"};
config.allowed_headers = {"Content-Type", "Authorization", "X-API-Key"};
config.exposed_headers = {"X-Total-Count", "X-Page-Info"};
config.allow_credentials = true;
config.max_age = 3600; // 1 hour preflight cache
config.options_success_status = 200; // Use legacy-friendly 200 instead of 204

server.enable_cors(config);
```

### Custom CORS Middleware

```cpp
// Create custom CORS handler
auto custom_cors = glz::create_cors_middleware({
    .allowed_origins = {"https://trusted-site.com"},
    .allowed_origins_validator = [](std::string_view origin) {
        return origin.ends_with(".trusted-site.com");
    },
    .allowed_methods = {"GET", "POST"},
    .allowed_headers = {"Content-Type", "Authorization"},
    .allow_credentials = true,
    .max_age = 86400 // 24 hours
});

server.use(custom_cors);
```

### CORS Testing Endpoint

```cpp
server.get("/test-cors", [](const glz::request& req, glz::response& res) {
    res.json({
        {"message", "CORS test endpoint"},
        {"origin", req.headers.count("Origin") ? req.headers.at("Origin") : "none"},
        {"method", glz::to_string(req.method)},
        {"timestamp", std::time(nullptr)}
    });
});
```

### Automatic Preflight Handling

The server builds the `Allow` header dynamically, and runs middleware (including CORS) before returning a response. If the origin is denied the preflight receives `403`; if the browser asks for a verb that is not implemented the server answers with `405` (after running middleware so diagnostics still flow); otherwise the CORS middleware fills in the appropriate `Access-Control-Allow-*` headers.

### Extended CORS Configuration

- **Dynamic origins** – `allowed_origins_validator` let you accept wildcard domains or perform per-request checks.
- **Custom preflight status** – override `options_success_status` if a client expects `200` instead of `204`.

## WebSocket Support

### Basic WebSocket Server

```cpp
#include "glaze/net/websocket_connection.hpp"

// Create WebSocket server
auto ws_server = std::make_shared<glz::websocket_server>();

// Handle new connections
ws_server->on_open([](std::shared_ptr<glz::websocket_connection> conn, const glz::request& req) {
    std::cout << "WebSocket connection opened from " << conn->remote_address() << std::endl;
    conn->send_text("Welcome to the WebSocket server!");
});

// Handle incoming messages
ws_server->on_message([](std::shared_ptr<glz::websocket_connection> conn, std::string_view message, glz::ws_opcode opcode) {
    if (opcode == glz::ws_opcode::text) {
        std::cout << "Received: " << message << std::endl;
        
        // Echo message back
        conn->send_text("Echo: " + std::string(message));
    }
});

// Handle connection close
ws_server->on_close([](std::shared_ptr<glz::websocket_connection> conn) {
    std::cout << "WebSocket connection closed" << std::endl;
});

// Handle errors
ws_server->on_error([](std::shared_ptr<glz::websocket_connection> conn, std::error_code ec) {
    std::cerr << "WebSocket error: " << ec.message() << std::endl;
});

// Mount WebSocket on HTTP server
glz::http_server server;
server.websocket("/ws", ws_server);

server.bind(8080).with_signals();
server.start();
server.wait_for_signal();
```

### Chat Room Example

```cpp
class ChatRoom {
    std::set<std::shared_ptr<glz::websocket_connection>> connections_;
    std::mutex connections_mutex_;
    
public:
    void add_connection(std::shared_ptr<glz::websocket_connection> conn) {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        connections_.insert(conn);
    }
    
    void remove_connection(std::shared_ptr<glz::websocket_connection> conn) {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        connections_.erase(conn);
    }
    
    void broadcast_message(const std::string& message) {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        
        auto it = connections_.begin();
        while (it != connections_.end()) {
            auto conn = *it;
            try {
                conn->send_text(message);
                ++it;
            } catch (...) {
                // Remove broken connections
                it = connections_.erase(it);
            }
        }
    }
};

// Setup chat WebSocket
ChatRoom chat_room;
auto chat_server = std::make_shared<glz::websocket_server>();

chat_server->on_open([&chat_room](auto conn, const glz::request& req) {
    chat_room.add_connection(conn);
    chat_room.broadcast_message("User joined the chat");
});

chat_server->on_message([&chat_room](auto conn, std::string_view message, glz::ws_opcode opcode) {
    if (opcode == glz::ws_opcode::text) {
        chat_room.broadcast_message(std::string(message));
    }
});

chat_server->on_close([&chat_room](auto conn) {
    chat_room.remove_connection(conn);
    chat_room.broadcast_message("User left the chat");
});

server.websocket("/chat", chat_server);
```

### WebSocket Authentication

```cpp
auto secure_ws = std::make_shared<glz::websocket_server>();

// Validate connections before upgrade
secure_ws->on_validate([](const glz::request& req) -> bool {
    auto auth_header = req.headers.find("Authorization");
    if (auth_header == req.headers.end()) {
        return false;
    }
    
    return validate_jwt_token(auth_header->second);
});

secure_ws->on_open([](auto conn, const glz::request& req) {
    // Store user info from validated token
    auto user_data = extract_user_from_token(req.headers.at("Authorization"));
    conn->set_user_data(std::make_shared<UserData>(user_data));
    
    conn->send_text("Authenticated successfully");
});

server.websocket("/secure-ws", secure_ws);
```

## SSL/TLS Support

### HTTPS Server Setup

```cpp
#include "glaze/net/http_server.hpp"

// Create HTTPS server (EnableTLS template parameter)
glz::https_server server;  // or glz::http_server<true>

// Load SSL certificates
server.load_certificate("server-cert.pem", "server-key.pem");

// Configure SSL options
server.set_ssl_verify_mode(0); // No client certificate verification

// Optional: Require client certificates
// server.set_ssl_verify_mode(SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT);

// Setup routes as normal
server.get("/secure-data", [](const glz::request& req, glz::response& res) {
    res.json({
        {"secure", true},
        {"client_ip", req.remote_ip},
        {"timestamp", std::time(nullptr)}
    });
});

server.bind(8443).with_signals(); // Standard HTTPS port
server.start();
server.wait_for_signal();
```

---

### Mixed HTTP/HTTPS Server

```cpp
// Run both HTTP and HTTPS on different ports
void run_dual_servers() {
    // HTTP server
    glz::http_server http_server;
    http_server.get("/health", health_check_handler);
    http_server.get("/redirect", [](const glz::request& req, glz::response& res) {
        res.status(301).header("Location", "https://localhost:8443" + req.target);
    });

    // HTTPS server
    glz::https_server https_server;
    https_server.load_certificate("cert.pem", "key.pem");
    setup_secure_routes(https_server);

    // Start both servers
    auto http_future = std::async([&]() {
        http_server.bind(8080);
        http_server.start();
    });

    auto https_future = std::async([&]() {
        https_server.bind(8443);
        https_server.start();
    });

    // Wait for shutdown signal
    wait_for_shutdown();

    http_server.stop();
    https_server.stop();

    http_future.wait();
    https_future.wait();
}
```

### Multiple Servers with Shared io_context

You can run multiple servers on the same io_context for better resource efficiency:

```cpp
#include "glaze/net/http_server.hpp"
#include <asio/io_context.hpp>
#include <memory>
#include <thread>

void run_multiple_servers_shared_context() {
    // Create a shared io_context
    auto io_ctx = std::make_shared<asio::io_context>();

    // Create HTTP server on shared context
    glz::http_server http_server(io_ctx);
    http_server.get("/api/v1", [](const glz::request&, glz::response& res) {
        res.json({{"version", "1.0"}});
    });
    http_server.bind(8080);
    http_server.start(0); // Don't create worker threads

    // Create HTTPS server on same shared context
    glz::https_server https_server(io_ctx);
    https_server.load_certificate("cert.pem", "key.pem");
    https_server.get("/api/v2", [](const glz::request&, glz::response& res) {
        res.json({{"version", "2.0"}, {"secure", true}});
    });
    https_server.bind(8443);
    https_server.start(0); // Don't create worker threads

    // Run the shared io_context in a thread pool
    std::vector<std::thread> threads;
    const size_t num_threads = std::thread::hardware_concurrency();
    threads.reserve(num_threads);

    for (size_t i = 0; i < num_threads; ++i) {
        threads.emplace_back([io_ctx]() {
            io_ctx->run();
        });
    }

    std::cout << "HTTP server running on port 8080\n";
    std::cout << "HTTPS server running on port 8443\n";
    std::cout << "Both servers sharing " << num_threads << " worker threads\n";

    // Wait for shutdown...
    wait_for_shutdown();

    // Stop both servers and the io_context
    http_server.stop();
    https_server.stop();
    io_ctx->stop();

    // Wait for all threads to finish
    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}
```

This approach is useful when you need to:
- Run multiple servers (HTTP, HTTPS, WebSocket) on the same thread pool
- Minimize resource usage by sharing threads between services
- Centralize io_context management for better control
- Integrate multiple servers into an existing ASIO-based application

## HTTP Client

### Basic HTTP Requests

```cpp
#include "glaze/net/http_client.hpp"

glz::http_client client;

// GET request
auto response = client.get("https://api.example.com/users");
if (response && response->status_code == 200) {
    std::cout << "Response: " << response->response_body << std::endl;
}

// POST request with JSON
User new_user{0, "John Doe", "john@example.com"};
auto create_response = client.post_json("https://api.example.com/users", new_user);

// POST with custom headers
std::unordered_map<std::string, std::string> headers = {
    {"Authorization", "Bearer " + token},
    {"Content-Type", "application/json"}
};

auto auth_response = client.post("https://api.example.com/data", 
                                json_data, headers);
```

### Async HTTP Requests

```cpp
// Async GET
auto future_response = client.get_async("https://api.example.com/slow-endpoint");

// Do other work while request is in progress
perform_other_tasks();

// Get result when ready
auto response = future_response.get();
if (response && response->status_code == 200) {
    process_response(response->response_body);
}

// Multiple concurrent requests
std::vector<std::future<std::expected<glz::response, std::error_code>>> futures;

for (int i = 0; i < 10; ++i) {
    std::string url = "https://api.example.com/data/" + std::to_string(i);
    futures.push_back(client.get_async(url));
}

// Collect all results
for (auto& future : futures) {
    auto response = future.get();
    if (response) {
        process_response(*response);
    }
}
```

## Request/Response Middleware

### Custom Middleware Development

```cpp
// Timing middleware
auto timing_middleware = [](const glz::request& req, glz::response& res) {
    auto start = std::chrono::high_resolution_clock::now();
    
    // Store start time in response for later use
    res.header("X-Request-Start", std::to_string(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            start.time_since_epoch()).count()));
};

// Response time middleware (would need to be applied after handler)
auto response_time_middleware = [](const glz::request& req, glz::response& res) {
    auto start_header = res.response_headers.find("X-Request-Start");
    if (start_header != res.response_headers.end()) {
        auto start_ms = std::stoll(start_header->second);
        auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count();
        
        res.header("X-Response-Time", std::to_string(now_ms - start_ms) + "ms");
    }
};

server.use(timing_middleware);
```

### Security Middleware

```cpp
// Security headers middleware
auto security_middleware = [](const glz::request& req, glz::response& res) {
    res.header("X-Content-Type-Options", "nosniff");
    res.header("X-Frame-Options", "DENY");
    res.header("X-XSS-Protection", "1; mode=block");
    res.header("Strict-Transport-Security", "max-age=31536000; includeSubDomains");
    res.header("Content-Security-Policy", "default-src 'self'");
};

// Rate limiting middleware
class RateLimiter {
    std::unordered_map<std::string, std::vector<std::chrono::steady_clock::time_point>> requests_;
    std::mutex mutex_;
    const int max_requests_ = 100;
    const std::chrono::minutes window_{1};
    
public:
    glz::handler create_middleware() {
        return [this](const glz::request& req, glz::response& res) {
            std::lock_guard<std::mutex> lock(mutex_);
            
            auto now = std::chrono::steady_clock::now();
            auto& client_requests = requests_[req.remote_ip];
            
            // Remove old requests outside the window
            client_requests.erase(
                std::remove_if(client_requests.begin(), client_requests.end(),
                    [now, this](const auto& time) {
                        return now - time > window_;
                    }),
                client_requests.end());
            
            // Check rate limit
            if (client_requests.size() >= max_requests_) {
                res.status(429).json({{"error", "Rate limit exceeded"}});
                return;
            }
            
            client_requests.push_back(now);
        };
    }
};

RateLimiter rate_limiter;
server.use(security_middleware);
server.use(rate_limiter.create_middleware());
```

## Performance Optimization

### Connection Pooling

```cpp
// The HTTP client automatically manages connections
class APIClient {
    glz::http_client client_;  // Reuse same client instance
    
public:
    // Multiple requests reuse connections automatically
    std::vector<User> get_all_users() {
        auto response = client_.get("https://api.example.com/users");
        // Connection is pooled for reuse
        
        if (response && response->status_code == 200) {
            return glz::read_json<std::vector<User>>(response->response_body).value_or(std::vector<User>{});
        }
        return {};
    }
    
    User get_user(int id) {
        auto response = client_.get("https://api.example.com/users/" + std::to_string(id));
        // Reuses pooled connection if available
        
        if (response && response->status_code == 200) {
            return glz::read_json<User>(response->response_body).value_or(User{});
        }
        return {};
    }
};
```

### Async Processing

```cpp
// Process requests asynchronously for better throughput
server.get("/heavy-work", [](const glz::request& req, glz::response& res) {
    // Offload heavy work to thread pool
    auto future = std::async([&]() {
        auto result = perform_heavy_computation();
        res.json(result);
    });
    
    // Response will be sent when async work completes
    future.wait();
});

// Or use the built-in async handlers
server.get_async("/async-work", [](const glz::request& req, glz::response& res) -> std::future<void> {
    return std::async([&req, &res]() {
        auto data = fetch_data_from_external_api();
        res.json(data);
    });
});
```

## Error Handling and Logging

### Structured Error Responses

```cpp
struct ErrorResponse {
    std::string error;
    std::string message;
    int code;
    std::string timestamp;
};

auto error_handler = [](const std::exception& e, glz::response& res) {
    ErrorResponse error;
    error.timestamp = get_iso_timestamp();
    
    if (auto validation_error = dynamic_cast<const ValidationException*>(&e)) {
        error.error = "validation_error";
        error.message = validation_error->what();
        error.code = 1001;
        res.status(400);
    } else if (auto not_found = dynamic_cast<const NotFoundException*>(&e)) {
        error.error = "not_found";
        error.message = not_found->what();
        error.code = 1002;
        res.status(404);
    } else {
        error.error = "internal_error";
        error.message = "An unexpected error occurred";
        error.code = 1000;
        res.status(500);
    }
    
    res.json(error);
};
```

### Request Logging

```cpp
// Structured logging middleware
auto logging_middleware = [](const glz::request& req, glz::response& res) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Log request
    std::cout << "REQUEST: " << glz::to_string(req.method) << " " << req.target
              << " from " << req.remote_ip << ":" << req.remote_port << std::endl;
    
    // Store start time for response logging
    res.header("X-Start-Time", std::to_string(
        std::chrono::duration_cast<std::chrono::microseconds>(
            start_time.time_since_epoch()).count()));
};

server.use(logging_middleware);
```
