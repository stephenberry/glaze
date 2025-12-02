# WebSocket Client

The Glaze WebSocket client provides a simple and efficient way to connect to WebSocket servers with support for both secure (WSS) and insecure (WS) connections.

## Basic Usage

```cpp
#include "glaze/net/websocket_client.hpp"

int main() {
    glz::websocket_client client;

    // Setup event handlers
    client.on_open([]() {
        std::cout << "Connected to WebSocket server!" << std::endl;
    });

    client.on_message([](std::string_view message, glz::ws_opcode opcode) {
        if (opcode == glz::ws_opcode::text) {
            std::cout << "Received: " << message << std::endl;
        }
    });

    client.on_close([](glz::ws_close_code code, std::string_view reason) {
        std::cout << "Connection closed with code: " << static_cast<int>(code);
        if (!reason.empty()) {
            std::cout << ", reason: " << reason;
        }
        std::cout << std::endl;
    });

    client.on_error([](std::error_code ec) {
        std::cerr << "Error: " << ec.message() << std::endl;
    });

    // Connect to WebSocket server
    client.connect("ws://localhost:8080/ws");

    // Run the io_context (blocks until connection is closed)
    client.run();

    return 0;
}
```

## Features

- **Protocol Support**: Both `ws://` (WebSocket) and `wss://` (WebSocket Secure) protocols
- **Event-Driven**: Callback-based handlers for open, message, close, and error events
- **Automatic Handshake**: Handles WebSocket upgrade handshake automatically
- **Message Masking**: Automatically masks outgoing messages in client mode (per RFC 6455)
- **Secure Connections**: Built-in SSL/TLS support for WSS connections
- **Configurable**: Adjustable max message size and other options
- **Shared io_context**: Can share an ASIO io_context with other network operations

## Constructor

### Creating a WebSocket Client

```cpp
// Create with default io_context
glz::websocket_client client;

// Create with shared io_context
auto io_ctx = std::make_shared<asio::io_context>();
glz::websocket_client client(io_ctx);
```

When you provide a shared `io_context`, you can integrate the WebSocket client with other ASIO-based operations and control the execution model yourself.

## Methods

### connect()

Establishes a connection to a WebSocket server.

```cpp
void connect(std::string_view url);
```

The URL should be in the format:
- `ws://host:port/path` for insecure connections
- `wss://host:port/path` for secure (SSL/TLS) connections

**Example:**
```cpp
client.connect("ws://example.com:8080/chat");
client.connect("wss://secure-api.example.com/stream");
```

### send()

Sends a text message to the connected WebSocket server.

```cpp
void send(std::string_view message);
```

**Example:**
```cpp
client.send("Hello, server!");
client.send("{\"type\": \"message\", \"content\": \"Hello\"}");
```

### send_binary()

Sends a binary message to the connected WebSocket server.

```cpp
void send_binary(std::string_view message);
```

**Example:**
```cpp
// Send binary data
std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0xFF};
std::string binary_msg(reinterpret_cast<const char*>(data.data()), data.size());
client.send_binary(binary_msg);
```

### close()

Closes the WebSocket connection gracefully.

```cpp
void close();
```

**Example:**
```cpp
client.on_message([&client](std::string_view message, glz::ws_opcode opcode) {
    if (message == "goodbye") {
        client.close();
    }
});
```

### run()

Runs the internal `io_context`, blocking until it stops.

```cpp
void run();
```

This is a convenience method that calls `context()->run()`. If you're using a shared `io_context`, you may prefer to manage its execution yourself.

### context()

Returns a reference to the internal `io_context`.

```cpp
std::shared_ptr<asio::io_context>& context();
```

**Example:**
```cpp
// Stop the io_context
client.context()->stop();

// Check if stopped
if (client.context()->stopped()) {
    std::cout << "io_context has stopped" << std::endl;
}
```

### set_max_message_size()

Sets the maximum allowed message size in bytes.

```cpp
void set_max_message_size(size_t size);
```

Default is 16 MB (16 * 1024 * 1024 bytes).

**Example:**
```cpp
client.set_max_message_size(1024 * 1024 * 32); // 32 MB
```

## Event Handlers

Event handlers are callback functions that are invoked when specific events occur.

### on_open()

Called when the WebSocket connection is successfully established.

```cpp
void on_open(std::function<void()> handler);
```

**Example:**
```cpp
client.on_open([&client]() {
    std::cout << "Connection opened!" << std::endl;
    client.send("Hello from client!");
});
```

### on_message()

Called when a message is received from the server.

```cpp
void on_message(std::function<void(std::string_view, ws_opcode)> handler);
```

The `ws_opcode` parameter indicates the message type:
- `glz::ws_opcode::text` - Text message
- `glz::ws_opcode::binary` - Binary message

**Example:**
```cpp
client.on_message([](std::string_view message, glz::ws_opcode opcode) {
    if (opcode == glz::ws_opcode::text) {
        std::cout << "Text message: " << message << std::endl;
    } else if (opcode == glz::ws_opcode::binary) {
        std::cout << "Binary message (" << message.size() << " bytes)" << std::endl;
    }
});
```

### on_close()

Called when the WebSocket connection is closed.

```cpp
void on_close(std::function<void(ws_close_code, std::string_view)> handler);
```

The handler receives:
- `ws_close_code` - The close code (e.g., normal closure, protocol error)
- `std::string_view` - The close reason (optional text explaining why the connection was closed)

**Example:**
```cpp
client.on_close([](glz::ws_close_code code, std::string_view reason) {
    std::cout << "Connection closed with code " << static_cast<int>(code);
    if (!reason.empty()) {
        std::cout << " (" << reason << ")";
    }
    std::cout << std::endl;
});
```

### on_error()

Called when an error occurs during connection or communication.

```cpp
void on_error(std::function<void(std::error_code)> handler);
```

**Example:**
```cpp
client.on_error([](std::error_code ec) {
    std::cerr << "WebSocket error: " << ec.message() << std::endl;
});
```

## Examples

### Echo Client

```cpp
#include "glaze/net/websocket_client.hpp"
#include <iostream>
#include <thread>

int main() {
    glz::websocket_client client;

    client.on_open([&client]() {
        std::cout << "Connected! Sending message..." << std::endl;
        client.send("Hello, WebSocket!");
    });

    client.on_message([&client](std::string_view message, glz::ws_opcode opcode) {
        if (opcode == glz::ws_opcode::text) {
            std::cout << "Echo received: " << message << std::endl;
            client.close();
        }
    });

    client.on_close([&client](glz::ws_close_code code, std::string_view reason) {
        std::cout << "Connection closed" << std::endl;
        client.context()->stop();
    });

    client.on_error([](std::error_code ec) {
        std::cerr << "Error: " << ec.message() << std::endl;
    });

    client.connect("ws://localhost:8080/ws");
    client.run();

    return 0;
}
```

### Chat Client

```cpp
#include "glaze/net/websocket_client.hpp"
#include <iostream>
#include <thread>
#include <atomic>
#include <string>

int main() {
    glz::websocket_client client;
    std::atomic<bool> connected{false};

    client.on_open([&connected]() {
        std::cout << "Connected to chat server!" << std::endl;
        connected = true;
    });

    client.on_message([](std::string_view message, glz::ws_opcode opcode) {
        if (opcode == glz::ws_opcode::text) {
            std::cout << "Chat: " << message << std::endl;
        }
    });

    client.on_close([&client](glz::ws_close_code code, std::string_view reason) {
        std::cout << "Disconnected from chat" << std::endl;
        client.context()->stop();
    });

    client.on_error([](std::error_code ec) {
        std::cerr << "Error: " << ec.message() << std::endl;
    });

    // Connect to chat server
    client.connect("ws://chat.example.com:8080/chat");

    // Run io_context in separate thread
    std::thread io_thread([&client]() {
        client.run();
    });

    // Wait for connection
    while (!connected) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Send messages from main thread
    std::string input;
    while (std::getline(std::cin, input)) {
        if (input == "/quit") {
            client.close();
            break;
        }
        client.send(input);
    }

    io_thread.join();
    return 0;
}
```

### Secure WebSocket Client (WSS)

```cpp
#include "glaze/net/websocket_client.hpp"
#include <iostream>

int main() {
    glz::websocket_client client;

    client.on_open([&client]() {
        std::cout << "Secure connection established!" << std::endl;
        client.send("Sending data over secure connection");
    });

    client.on_message([](std::string_view message, glz::ws_opcode opcode) {
        if (opcode == glz::ws_opcode::text) {
            std::cout << "Secure message: " << message << std::endl;
        }
    });

    client.on_error([](std::error_code ec) {
        std::cerr << "Error: " << ec.message() << std::endl;
    });

    // Connect using WSS protocol
    client.connect("wss://secure-api.example.com/data-stream");
    client.run();

    return 0;
}
```

### JSON Message Exchange

```cpp
#include "glaze/net/websocket_client.hpp"
#include "glaze/glaze.hpp"
#include <iostream>

struct Message {
    std::string type;
    std::string content;
    int64_t timestamp;
};

template <>
struct glz::meta<Message> {
    using T = Message;
    static constexpr auto value = object(
        "type", &T::type,
        "content", &T::content,
        "timestamp", &T::timestamp
    );
};

int main() {
    glz::websocket_client client;

    client.on_open([&client]() {
        Message msg{"greeting", "Hello from Glaze!", std::time(nullptr)};
        std::string json = glz::write_json(msg).value_or("");
        client.send(json);
    });

    client.on_message([](std::string_view message, glz::ws_opcode opcode) {
        if (opcode == glz::ws_opcode::text) {
            auto msg = glz::read_json<Message>(message);
            if (msg) {
                std::cout << "Message type: " << msg->type << std::endl;
                std::cout << "Content: " << msg->content << std::endl;
                std::cout << "Timestamp: " << msg->timestamp << std::endl;
            }
        }
    });

    client.on_error([](std::error_code ec) {
        std::cerr << "Error: " << ec.message() << std::endl;
    });

    client.connect("ws://api.example.com/messages");
    client.run();

    return 0;
}
```

### Binary Message Exchange

```cpp
#include "glaze/net/websocket_client.hpp"
#include <iostream>
#include <vector>

int main() {
    glz::websocket_client client;

    client.on_open([&client]() {
        std::cout << "Connected! Sending binary data..." << std::endl;

        // Create binary data (e.g., image, file, or protocol buffer)
        std::vector<uint8_t> binary_data = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A}; // PNG header
        std::string binary_msg(reinterpret_cast<const char*>(binary_data.data()),
                               binary_data.size());

        client.send_binary(binary_msg);
    });

    client.on_message([&client](std::string_view message, glz::ws_opcode opcode) {
        if (opcode == glz::ws_opcode::binary) {
            std::cout << "Received binary message (" << message.size() << " bytes)" << std::endl;

            // Process binary data
            const uint8_t* data = reinterpret_cast<const uint8_t*>(message.data());
            for (size_t i = 0; i < std::min(size_t(16), message.size()); ++i) {
                printf("%02X ", data[i]);
            }
            std::cout << std::endl;

            client.close();
        }
    });

    client.on_close([&client](glz::ws_close_code code, std::string_view reason) {
        std::cout << "Connection closed" << std::endl;
        client.context()->stop();
    });

    client.on_error([](std::error_code ec) {
        std::cerr << "Error: " << ec.message() << std::endl;
    });

    client.connect("ws://localhost:8080/ws");
    client.run();

    return 0;
}
```

### Multiple Clients with Shared io_context

```cpp
#include "glaze/net/websocket_client.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <memory>

int main() {
    // Create shared io_context
    auto io_ctx = std::make_shared<asio::io_context>();

    // Create multiple clients sharing the same io_context
    // Use unique_ptr to avoid memory issues when vector resizes
    std::vector<std::unique_ptr<glz::websocket_client>> clients;

    for (int i = 0; i < 5; ++i) {
        clients.push_back(std::make_unique<glz::websocket_client>(io_ctx));

        // Get raw pointer to avoid capturing reference to vector element
        auto* client_ptr = clients.back().get();
        int client_id = i;

        client_ptr->on_open([client_ptr, client_id]() {
            std::cout << "Client " << client_id << " connected" << std::endl;
            client_ptr->send("Hello from client " + std::to_string(client_id));
        });

        client_ptr->on_message([client_id](std::string_view message, glz::ws_opcode opcode) {
            std::cout << "Client " << client_id << " received: " << message << std::endl;
        });

        client_ptr->on_error([client_id](std::error_code ec) {
            std::cerr << "Client " << client_id << " error: " << ec.message() << std::endl;
        });

        client_ptr->connect("ws://localhost:8080/ws");
    }

    // Run the shared io_context in a thread pool
    std::vector<std::thread> threads;
    for (size_t i = 0; i < std::thread::hardware_concurrency(); ++i) {
        threads.emplace_back([io_ctx]() {
            io_ctx->run();
        });
    }

    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }

    return 0;
}
```

## SSL/TLS Support (WSS)

To use secure WebSocket connections (WSS), ensure that Glaze is built with SSL support enabled (`GLZ_ENABLE_SSL`).

### Requirements

- OpenSSL or compatible SSL library
- Build Glaze with `-DGLZ_ENABLE_SSL=ON`

### Usage

Simply use the `wss://` protocol in the URL:

```cpp
client.connect("wss://secure-server.com/socket");
```

The client automatically:
- Creates an SSL context with TLS 1.2 client configuration
- Sets default certificate verification paths
- Configures SNI (Server Name Indication) for the target host
- Performs SSL/TLS handshake before the WebSocket upgrade

### Custom SSL Configuration

For advanced SSL configuration, you would need to modify the client's SSL context. This is currently handled automatically, but future versions may expose more configuration options.

## Error Handling

Errors are reported through the `on_error` callback. Common error scenarios:

### Connection Errors

```cpp
client.on_error([](std::error_code ec) {
    if (ec == std::errc::connection_refused) {
        std::cerr << "Server is not running or refusing connections" << std::endl;
    } else if (ec == std::errc::timed_out) {
        std::cerr << "Connection timed out" << std::endl;
    } else if (ec == std::errc::address_not_available) {
        std::cerr << "Invalid server address" << std::endl;
    } else {
        std::cerr << "Connection error: " << ec.message() << std::endl;
    }
});
```

### Protocol Errors

```cpp
client.on_error([](std::error_code ec) {
    if (ec == std::errc::protocol_error) {
        std::cerr << "WebSocket protocol error (handshake failed)" << std::endl;
    } else if (ec == std::errc::protocol_not_supported) {
        std::cerr << "WSS not supported (build without SSL)" << std::endl;
    }
});
```

## Client Lifetime

The `websocket_client` uses a safe callback lifetime model:

- **Destruction stops callbacks**: When a client is destroyed, pending async callbacks exit silently rather than firing with errors. This prevents use-after-free bugs when callback captures reference local variables.

- **Shared context safety**: When multiple clients share an `io_context`, destroying one client does not stop the shared context. The context is only stopped if the destroyed client was the sole owner.

```cpp
// Safe pattern - local variables won't dangle
void safe_example() {
    std::string local_data = "important";
    glz::websocket_client client;

    client.on_message([&local_data](auto msg, auto) {
        std::cout << local_data;  // Safe: callback won't fire after function returns
    });

    client.connect("ws://...");
    // When function returns and client is destroyed,
    // callbacks exit silently - no dangling reference
}
```

If you need to know when operations complete, wait before destroying:

```cpp
std::atomic<bool> closed{false};
client.on_close([&](auto, auto) { closed = true; });
client.close();
while (!closed) { std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
// Now safe to let client go out of scope
```

## Best Practices

### 1. Always Set Error Handler

Always provide an error handler to catch connection and protocol errors:

```cpp
client.on_error([](std::error_code ec) {
    std::cerr << "Error: " << ec.message() << std::endl;
});
```

### 2. Handle Connection Lifecycle

Properly manage the connection lifecycle with all event handlers:

```cpp
std::atomic<bool> is_connected{false};

client.on_open([&is_connected]() {
    is_connected = true;
});

client.on_close([&is_connected, &client](glz::ws_close_code code, std::string_view reason) {
    is_connected = false;
    client.context()->stop();
});
```

### 3. Thread Safety

The `send()` and `close()` methods are thread-safe (protected by an internal mutex). You can safely call them from different threads:

```cpp
// Thread 1: Receiving messages
client.on_message([](std::string_view message, glz::ws_opcode opcode) {
    process_message(message);
});

// Thread 2: Sending messages
std::thread sender([&client]() {
    while (running) {
        client.send(get_next_message());
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
});
```

### 4. Message Size Limits

Configure appropriate message size limits based on your use case:

```cpp
// For small control messages
client.set_max_message_size(64 * 1024); // 64 KB

// For large data transfers
client.set_max_message_size(100 * 1024 * 1024); // 100 MB
```

### 5. Graceful Shutdown

Always close connections gracefully before exiting:

```cpp
// Register signal handler
std::atomic<bool> should_exit{false};
signal(SIGINT, [](int) { should_exit = true; });

client.on_open([&client, &should_exit]() {
    while (!should_exit) {
        // Do work...
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    client.close();
});
```

## Performance Notes

- The client uses ASIO for asynchronous I/O operations
- Message masking (required for client-to-server messages) adds minimal overhead
- Connection pooling is not applicable for WebSocket clients (persistent connections)
- For maximum throughput, consider using a shared `io_context` with multiple worker threads
- The default max message size (16 MB) provides good performance for most use cases

## Comparison with WebSocket Server

| Feature | WebSocket Client | WebSocket Server |
|---------|-----------------|------------------|
| Protocol | `ws://` or `wss://` | Mounted on HTTP server |
| Connection Mode | Initiates connections | Accepts connections |
| Message Masking | Required (automatic) | Not used (server mode) |
| Multiple Connections | One per client instance | Multiple per server |
| Handshake | Initiates upgrade | Responds to upgrade |

## See Also

- [HTTP Client](http-client.md) - HTTP client with connection pooling
- [WebSocket Server](advanced-networking.md#websocket-support) - WebSocket server functionality
- [Advanced Networking](advanced-networking.md) - CORS, SSL/TLS, and middleware
