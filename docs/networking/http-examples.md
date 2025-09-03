# Examples

This page provides working examples of Glaze HTTP functionality that you can use as templates for your own projects.

## Basic REST API Server

Complete REST API server with CRUD operations:

```cpp
#include "glaze/net/http_server.hpp"
#include "glaze/glaze.hpp"
#include <iostream>
#include <unordered_map>
#include <vector>

struct User {
    int id{};
    std::string name{};
    std::string email{};
    std::string created_at{};
};

struct CreateUserRequest {
    std::string name{};
    std::string email{};
};

struct UpdateUserRequest {
    std::string name{};
    std::string email{};
};

struct ErrorResponse {
    std::string error{};
    std::string message{};
};

class UserAPI {
    std::unordered_map<int, User> users_;
    int next_id_ = 1;
    
public:
    UserAPI() {
        // Add some initial data
        users_[1] = {1, "Alice Johnson", "alice@example.com", "2024-01-01T10:00:00Z"};
        users_[2] = {2, "Bob Smith", "bob@example.com", "2024-01-01T11:00:00Z"};
        next_id_ = 3;
    }
    
    void setup_routes(glz::http_server& server) {
        // GET /api/users - List all users
        server.get("/api/users", [this](const glz::request& req, glz::response& res) {
            std::vector<User> user_list;
            for (const auto& [id, user] : users_) {
                user_list.push_back(user);
            }
            res.json(user_list);
        });
        
        // GET /api/users/:id - Get user by ID
        server.get("/api/users/:id", [this](const glz::request& req, glz::response& res) {
            try {
                int id = std::stoi(req.params.at("id"));
                auto it = users_.find(id);
                
                if (it != users_.end()) {
                    res.json(it->second);
                } else {
                    res.status(404).json(ErrorResponse{"not_found", "User not found"});
                }
            } catch (const std::exception&) {
                res.status(400).json(ErrorResponse{"invalid_id", "Invalid user ID"});
            }
        });
        
        // POST /api/users - Create new user
        server.post("/api/users", [this](const glz::request& req, glz::response& res) {
            CreateUserRequest create_req;
            auto ec = glz::read_json(create_req, req.body);
            
            if (ec) {
                res.status(400).json(ErrorResponse{"invalid_json", "Invalid request body"});
                return;
            }
            
            if (create_req.name.empty() || create_req.email.empty()) {
                res.status(400).json(ErrorResponse{"validation_error", "Name and email are required"});
                return;
            }
            
            User new_user;
            new_user.id = next_id_++;
            new_user.name = create_req.name;
            new_user.email = create_req.email;
            new_user.created_at = get_current_timestamp();
            
            users_[new_user.id] = new_user;
            res.status(201).json(new_user);
        });
        
        // PUT /api/users/:id - Update user
        server.put("/api/users/:id", [this](const glz::request& req, glz::response& res) {
            try {
                int id = std::stoi(req.params.at("id"));
                auto it = users_.find(id);
                
                if (it == users_.end()) {
                    res.status(404).json(ErrorResponse{"not_found", "User not found"});
                    return;
                }
                
                UpdateUserRequest update_req;
                auto ec = glz::read_json(update_req, req.body);
                
                if (ec) {
                    res.status(400).json(ErrorResponse{"invalid_json", "Invalid request body"});
                    return;
                }
                
                if (!update_req.name.empty()) {
                    it->second.name = update_req.name;
                }
                
                if (!update_req.email.empty()) {
                    it->second.email = update_req.email;
                }
                
                res.json(it->second);
            } catch (const std::exception&) {
                res.status(400).json(ErrorResponse{"invalid_id", "Invalid user ID"});
            }
        });
        
        // DELETE /api/users/:id - Delete user
        server.del("/api/users/:id", [this](const glz::request& req, glz::response& res) {
            try {
                int id = std::stoi(req.params.at("id"));
                auto it = users_.find(id);
                
                if (it != users_.end()) {
                    users_.erase(it);
                    res.status(204); // No content
                } else {
                    res.status(404).json(ErrorResponse{"not_found", "User not found"});
                }
            } catch (const std::exception&) {
                res.status(400).json(ErrorResponse{"invalid_id", "Invalid user ID"});
            }
        });
    }
    
private:
    std::string get_current_timestamp() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
        return ss.str();
    }
};

int main() {
    glz::http_server server;
    UserAPI user_api;
    
    // Enable CORS for frontend development
    server.enable_cors();
    
    // Setup API routes
    user_api.setup_routes(server);
    
    // Health check endpoint
    server.get("/health", [](const glz::request&, glz::response& res) {
        res.json({{"status", "healthy"}, {"timestamp", std::time(nullptr)}});
    });
    
    std::cout << "Starting server on http://localhost:8080" << std::endl;
    std::cout << "Press Ctrl+C to gracefully shut down the server" << std::endl;
    
    server.bind(8080)
          .with_signals();  // Enable signal handling for graceful shutdown
    
    server.start();
    
    // Wait for shutdown signal (blocks until server stops)
    server.wait_for_signal();
    
    std::cout << "Server shut down successfully" << std::endl;
    return 0;
}
```

## Auto-Generated REST API

Using the REST registry to automatically generate endpoints:

```cpp
#include "glaze/rpc/registry.hpp"
#include "glaze/net/http_server.hpp"
#include <vector>
#include <algorithm>

struct Task {
    int id{};
    std::string title{};
    std::string description{};
    bool completed{false};
    std::string created_at{};
    std::string due_date{};
};

struct CreateTaskRequest {
    std::string title{};
    std::string description{};
    std::string due_date{};
};

struct UpdateTaskRequest {
    int id{};
    std::string title{};
    std::string description{};
    bool completed{};
    std::string due_date{};
};

struct TaskSearchRequest {
    std::string query{};
    bool completed_only{false};
    int limit{10};
};

class TaskService {
    std::vector<Task> tasks_;
    int next_id_ = 1;
    
public:
    TaskService() {
        // Add sample tasks
        tasks_ = {
            {1, "Learn Glaze", "Study the Glaze HTTP library", false, "2024-01-01T10:00:00Z", "2024-01-15T00:00:00Z"},
            {2, "Write documentation", "Create API documentation", false, "2024-01-02T10:00:00Z", "2024-01-20T00:00:00Z"},
            {3, "Deploy to production", "Deploy the new API", false, "2024-01-03T10:00:00Z", "2024-01-25T00:00:00Z"}
        };
        next_id_ = 4;
    }
    
    // Auto-generated endpoints:
    
    // GET /api/getAllTasks
    std::vector<Task> getAllTasks() {
        return tasks_;
    }
    
    // POST /api/getTaskById (with JSON body: {"id": 123})
    Task getTaskById(int id) {
        auto it = std::find_if(tasks_.begin(), tasks_.end(),
            [id](const Task& task) { return task.id == id; });
        
        if (it != tasks_.end()) {
            return *it;
        }
        
        throw std::runtime_error("Task not found");
    }
    
    // POST /api/createTask
    Task createTask(const CreateTaskRequest& request) {
        if (request.title.empty()) {
            throw std::invalid_argument("Task title is required");
        }
        
        Task task;
        task.id = next_id_++;
        task.title = request.title;
        task.description = request.description;
        task.due_date = request.due_date;
        task.completed = false;
        task.created_at = get_current_timestamp();
        
        tasks_.push_back(task);
        return task;
    }
    
    // POST /api/updateTask
    Task updateTask(const UpdateTaskRequest& request) {
        auto it = std::find_if(tasks_.begin(), tasks_.end(),
            [&request](const Task& task) { return task.id == request.id; });
        
        if (it == tasks_.end()) {
            throw std::runtime_error("Task not found");
        }
        
        if (!request.title.empty()) {
            it->title = request.title;
        }
        
        if (!request.description.empty()) {
            it->description = request.description;
        }
        
        if (!request.due_date.empty()) {
            it->due_date = request.due_date;
        }
        
        it->completed = request.completed;
        
        return *it;
    }
    
    // POST /api/deleteTask
    void deleteTask(int id) {
        auto it = std::find_if(tasks_.begin(), tasks_.end(),
            [id](const Task& task) { return task.id == id; });
        
        if (it != tasks_.end()) {
            tasks_.erase(it);
        } else {
            throw std::runtime_error("Task not found");
        }
    }
    
    // POST /api/searchTasks
    std::vector<Task> searchTasks(const TaskSearchRequest& request) {
        std::vector<Task> results;
        
        for (const auto& task : tasks_) {
            bool matches = true;
            
            if (!request.query.empty()) {
                matches = task.title.find(request.query) != std::string::npos ||
                         task.description.find(request.query) != std::string::npos;
            }
            
            if (request.completed_only && !task.completed) {
                matches = false;
            }
            
            if (matches) {
                results.push_back(task);
            }
            
            if (results.size() >= request.limit) {
                break;
            }
        }
        
        return results;
    }
    
    // GET /api/getCompletedTasks
    std::vector<Task> getCompletedTasks() {
        std::vector<Task> completed;
        
        std::copy_if(tasks_.begin(), tasks_.end(), std::back_inserter(completed),
            [](const Task& task) { return task.completed; });
        
        return completed;
    }
    
private:
    std::string get_current_timestamp() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
        return ss.str();
    }
};

// Register the service with Glaze for reflection
template <>
struct glz::meta<TaskService> {
    using T = TaskService;
    static constexpr auto value = object(
        &T::getAllTasks,
        &T::getTaskById,
        &T::createTask,
        &T::updateTask,
        &T::deleteTask,
        &T::searchTasks,
        &T::getCompletedTasks
    );
};

int main() {
    glz::http_server server;
    TaskService taskService;
    
    // Create REST registry
    glz::registry<glz::opts{}, glz::REST> registry;
    
    // Register the service - automatically generates all endpoints
    registry.on(taskService);
    
    // Mount the auto-generated endpoints
    server.mount("/api", registry.endpoints);
    
    // Enable CORS
    server.enable_cors();
    
    // Add some additional manual endpoints
    server.get("/", [](const glz::request&, glz::response& res) {
        res.content_type("text/html").body(R"(
            <h1>Task API</h1>
            <p>Auto-generated REST API for task management</p>
            <h2>Endpoints:</h2>
            <ul>
                <li>GET /api/getAllTasks</li>
                <li>POST /api/getTaskById</li>
                <li>POST /api/createTask</li>
                <li>POST /api/updateTask</li>
                <li>POST /api/deleteTask</li>
                <li>POST /api/searchTasks</li>
                <li>GET /api/getCompletedTasks</li>
            </ul>
        )");
    });
    
    std::cout << "Task API server running on http://localhost:8080" << std::endl;
    std::cout << "Press Ctrl+C to gracefully shut down the server" << std::endl;
    
    server.bind(8080)
          .with_signals();  // Enable signal handling for graceful shutdown
    
    server.start();
    
    // Wait for shutdown signal (blocks until server stops)
    server.wait_for_signal();
    
    std::cout << "Server shut down successfully" << std::endl;
    return 0;
}
```

## WebSocket Chat Server

Real-time chat server with WebSocket support:

```cpp
#include "glaze/net/http_server.hpp"
#include "glaze/net/websocket_connection.hpp"
#include <set>
#include <mutex>
#include <memory>

struct ChatMessage {
    std::string username;
    std::string message;
    std::string timestamp;
    std::string type = "message"; // "message", "join", "leave"
};

class ChatRoom {
    std::set<std::shared_ptr<glz::websocket_connection>> connections_;
    mutable std::mutex connections_mutex_;
    std::vector<ChatMessage> message_history_;
    std::mutex history_mutex_;
    
public:
    void add_connection(std::shared_ptr<glz::websocket_connection> conn, const std::string& username) {
        {
            std::lock_guard<std::mutex> lock(connections_mutex_);
            connections_.insert(conn);
        }
        
        // Store username in connection user data
        conn->set_user_data(std::make_shared<std::string>(username));
        
        // Send chat history to new user
        {
            std::lock_guard<std::mutex> lock(history_mutex_);
            for (const auto& msg : message_history_) {
                std::string json_msg;
                glz::write_json(msg, json_msg);
                conn->send_text(json_msg);
            }
        }
        
        // Broadcast join message
        ChatMessage join_msg;
        join_msg.username = "System";
        join_msg.message = username + " joined the chat";
        join_msg.timestamp = get_current_timestamp();
        join_msg.type = "join";
        
        broadcast_message(join_msg);
    }
    
    void remove_connection(std::shared_ptr<glz::websocket_connection> conn) {
        std::string username = "Unknown";
        
        // Get username from user data
        if (auto user_data = conn->get_user_data()) {
            if (auto username_ptr = std::static_pointer_cast<std::string>(user_data)) {
                username = *username_ptr;
            }
        }
        
        {
            std::lock_guard<std::mutex> lock(connections_mutex_);
            connections_.erase(conn);
        }
        
        // Broadcast leave message
        ChatMessage leave_msg;
        leave_msg.username = "System";
        leave_msg.message = username + " left the chat";
        leave_msg.timestamp = get_current_timestamp();
        leave_msg.type = "leave";
        
        broadcast_message(leave_msg);
    }
    
    void handle_message(std::shared_ptr<glz::websocket_connection> conn, const std::string& message) {
        std::string username = "Anonymous";
        
        // Get username from connection
        if (auto user_data = conn->get_user_data()) {
            if (auto username_ptr = std::static_pointer_cast<std::string>(user_data)) {
                username = *username_ptr;
            }
        }
        
        // Create chat message
        ChatMessage chat_msg;
        chat_msg.username = username;
        chat_msg.message = message;
        chat_msg.timestamp = get_current_timestamp();
        chat_msg.type = "message";
        
        broadcast_message(chat_msg);
    }
    
    void broadcast_message(const ChatMessage& message) {
        // Add to history
        {
            std::lock_guard<std::mutex> lock(history_mutex_);
            message_history_.push_back(message);
            
            // Keep only last 100 messages
            if (message_history_.size() > 100) {
                message_history_.erase(message_history_.begin());
            }
        }
        
        // Serialize message
        std::string json_msg;
        glz::write_json(message, json_msg);
        
        // Broadcast to all connections
        std::lock_guard<std::mutex> lock(connections_mutex_);
        auto it = connections_.begin();
        while (it != connections_.end()) {
            auto conn = *it;
            try {
                conn->send_text(json_msg);
                ++it;
            } catch (...) {
                // Remove broken connections
                it = connections_.erase(it);
            }
        }
    }
    
    int get_user_count() const {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        return connections_.size();
    }
    
private:
    std::string get_current_timestamp() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
        return ss.str();
    }
};

int main() {
    glz::http_server server;
    ChatRoom chat_room;
    
    // Create WebSocket server
    auto ws_server = std::make_shared<glz::websocket_server>();
    
    // Handle new connections
    ws_server->on_open([&chat_room](std::shared_ptr<glz::websocket_connection> conn, const glz::request& req) {
        // Extract username from query parameters or headers
        std::string username = "User" + std::to_string(std::rand() % 1000);
        
        // In a real app, you'd extract this from authentication
        auto auth_header = req.headers.find("x-username");
        if (auth_header != req.headers.end()) {
            username = auth_header->second;
        }
        
        std::cout << "WebSocket connection from " << conn->remote_address() << " (username: " << username << ")" << std::endl;
        
        chat_room.add_connection(conn, username);
        
        // Send welcome message
        conn->send_text(R"({"username":"System","message":"Welcome to the chat!","type":"system"})");
    });
    
    // Handle incoming messages
    ws_server->on_message([&chat_room](std::shared_ptr<glz::websocket_connection> conn, std::string_view message, glz::ws_opcode opcode) {
        if (opcode == glz::ws_opcode::text) {
            chat_room.handle_message(conn, std::string(message));
        }
    });
    
    // Handle connection close
    ws_server->on_close([&chat_room](std::shared_ptr<glz::websocket_connection> conn) {
        chat_room.remove_connection(conn);
    });
    
    // Handle errors
    ws_server->on_error([](std::shared_ptr<glz::websocket_connection> conn, std::error_code ec) {
        std::cerr << "WebSocket error: " << ec.message() << std::endl;
    });
    
    // Mount WebSocket
    server.websocket("/ws", ws_server);
    
    // HTTP endpoints
    server.get("/", [](const glz::request&, glz::response& res) {
        res.content_type("text/html").body(R"(
<!DOCTYPE html>
<html>
<head>
    <title>WebSocket Chat</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        #messages { border: 1px solid #ccc; height: 400px; overflow-y: scroll; padding: 10px; margin-bottom: 10px; }
        #messageInput { width: 70%; padding: 5px; }
        #sendButton { padding: 5px 10px; }
        .message { margin-bottom: 5px; }
        .system { color: #666; font-style: italic; }
        .join { color: #008000; }
        .leave { color: #ff0000; }
    </style>
</head>
<body>
    <h1>WebSocket Chat</h1>
    <div id="messages"></div>
    <input type="text" id="messageInput" placeholder="Type your message...">
    <button id="sendButton">Send</button>
    
    <script>
        const ws = new WebSocket('ws://localhost:8080/ws');
        const messages = document.getElementById('messages');
        const messageInput = document.getElementById('messageInput');
        const sendButton = document.getElementById('sendButton');
        
        ws.onopen = function() {
            addMessage('Connected to chat server', 'system');
        };
        
        ws.onmessage = function(event) {
            const msg = JSON.parse(event.data);
            addMessage(msg.username + ': ' + msg.message, msg.type);
        };
        
        ws.onclose = function() {
            addMessage('Disconnected from chat server', 'system');
        };
        
        function addMessage(text, type) {
            const div = document.createElement('div');
            div.className = 'message ' + type;
            div.textContent = new Date().toLocaleTimeString() + ' - ' + text;
            messages.appendChild(div);
            messages.scrollTop = messages.scrollHeight;
        }
        
        function sendMessage() {
            const message = messageInput.value.trim();
            if (message && ws.readyState === WebSocket.OPEN) {
                ws.send(message);
                messageInput.value = '';
            }
        }
        
        sendButton.onclick = sendMessage;
        messageInput.onkeypress = function(e) {
            if (e.key === 'Enter') {
                sendMessage();
            }
        };
    </script>
</body>
</html>
        )");
    });
    
    // Chat stats endpoint
    server.get("/api/stats", [&chat_room](const glz::request&, glz::response& res) {
        res.json({
            {"users_online", chat_room.get_user_count()},
            {"server_time", std::time(nullptr)}
        });
    });
    
    server.enable_cors();
    
    std::cout << "Chat server running on http://localhost:8080" << std::endl;
    std::cout << "WebSocket endpoint: ws://localhost:8080/ws" << std::endl;
    std::cout << "Press Ctrl+C to gracefully shut down the server" << std::endl;
    
    server.bind(8080)
          .with_signals();  // Enable signal handling for graceful shutdown
    
    server.start();
    
    // Wait for shutdown signal (blocks until server stops)
    server.wait_for_signal();
    
    std::cout << "Server shut down successfully" << std::endl;
    return 0;
}
```

## Simple Authentication Server

Basic authentication server using simple token validation (no external dependencies):

```cpp
#include "glaze/net/http_server.hpp"
#include <random>

struct LoginRequest {
    std::string username;
    std::string password;
};

struct LoginResponse {
    std::string token;
    std::string username;
    int expires_in;
};

struct User {
    int id;
    std::string username;
    std::string email;
    std::string role;
};

class SimpleAuthService {
    std::unordered_map<std::string, User> users_;
    std::unordered_map<std::string, std::string> active_tokens_; // token -> username
    
public:
    SimpleAuthService() {
        // Add some test users
        users_["admin"] = {1, "admin", "admin@example.com", "admin"};
        users_["user"] = {2, "user", "user@example.com", "user"};
    }
    
    std::optional<LoginResponse> login(const LoginRequest& request) {
        // Simple password validation (use proper hashing in production)
        if ((request.username == "admin" && request.password == "admin123") ||
            (request.username == "user" && request.password == "user123")) {
            
            std::string token = generate_token();
            active_tokens_[token] = request.username;
            
            return LoginResponse{token, request.username, 3600}; // 1 hour
        }
        
        return std::nullopt;
    }
    
    std::optional<User> validate_token(const std::string& token) {
        auto it = active_tokens_.find(token);
        if (it != active_tokens_.end()) {
            auto user_it = users_.find(it->second);
            if (user_it != users_.end()) {
                return user_it->second;
            }
        }
        return std::nullopt;
    }
    
    void logout(const std::string& token) {
        active_tokens_.erase(token);
    }
    
private:
    std::string generate_token() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(0, 15);
        
        std::string token = "tok_";
        for (int i = 0; i < 32; ++i) {
            token += "0123456789abcdef"[dis(gen)];
        }
        return token;
    }
};

// Authentication middleware
auto create_simple_auth_middleware(SimpleAuthService& auth_service) {
    return [&auth_service](const glz::request& req, glz::response& res) {
        // Skip auth for login endpoint and public endpoints
        if (req.target == "/api/login" || req.target == "/health" || req.target == "/") {
            return;
        }
        
        auto auth_header = req.headers.find("authorization");
        if (auth_header == req.headers.end()) {
            res.status(401).json({{"error", "Authorization header required"}});
            return;
        }
        
        std::string auth_value = auth_header->second;
        if (!auth_value.starts_with("Bearer ")) {
            res.status(401).json({{"error", "Bearer token required"}});
            return;
        }
        
        std::string token = auth_value.substr(7); // Remove "Bearer "
        auto user = auth_service.validate_token(token);
        
        if (!user) {
            res.status(401).json({{"error", "Invalid or expired token"}});
            return;
        }
        
        // Store user info in response headers for use by handlers
        res.header("X-User-ID", std::to_string(user->id));
        res.header("X-Username", user->username);
        res.header("X-User-Role", user->role);
    };
}

int main() {
    glz::http_server server;
    SimpleAuthService auth_service;
    
    // Add authentication middleware
    server.use(create_simple_auth_middleware(auth_service));
    
    // Enable CORS
    server.enable_cors();
    
    // Public endpoints
    server.post("/api/login", [&auth_service](const glz::request& req, glz::response& res) {
        LoginRequest login_req;
        auto ec = glz::read_json(login_req, req.body);
        
        if (ec) {
            res.status(400).json({{"error", "Invalid request body"}});
            return;
        }
        
        auto login_response = auth_service.login(login_req);
        if (login_response) {
            res.json(*login_response);
        } else {
            res.status(401).json({{"error", "Invalid credentials"}});
        }
    });
    
    server.get("/health", [](const glz::request&, glz::response& res) {
        res.json({{"status", "healthy"}});
    });
    
    // Protected endpoints
    server.get("/api/profile", [](const glz::request& req, glz::response& res) {
        // User info is available from auth middleware
        std::string username = res.response_headers["X-Username"];
        std::string role = res.response_headers["X-User-Role"];
        
        res.json({
            {"username", username},
            {"role", role},
            {"message", "This is your protected profile"}
        });
    });
    
    server.get("/api/admin", [](const glz::request& req, glz::response& res) {
        std::string role = res.response_headers["X-User-Role"];
        
        if (role != "admin") {
            res.status(403).json({{"error", "Admin access required"}});
            return;
        }
        
        res.json({
            {"message", "Admin-only data"},
            {"users_count", 42},
            {"server_stats", "All systems operational"}
        });
    });
    
    std::cout << "Authentication server running on http://localhost:8080" << std::endl;
    std::cout << "Test credentials: admin/admin123 or user/user123" << std::endl;
    std::cout << "Press Ctrl+C to gracefully shut down the server" << std::endl;
    
    server.bind(8080)
          .with_signals();  // Enable signal handling for graceful shutdown
    
    server.start();
    
    // Wait for shutdown signal (blocks until server stops)
    server.wait_for_signal();
    
    std::cout << "Server shut down successfully" << std::endl;
    return 0;
}
```

## Microservice Template

Production-ready microservice template with monitoring and health checks:

```cpp
#include "glaze/net/http_server.hpp"
#include "glaze/rpc/registry.hpp"
#include <atomic>
#include <chrono>
#include <fstream>
#include <thread>

struct HealthStatus {
    std::string status;
    std::string version;
    int uptime_seconds;
    struct {
        bool database;
        bool external_api;
        bool redis;
    } dependencies;
    struct {
        int total_requests;
        int error_rate;
        double avg_response_time;
    } metrics;
};

struct MetricsData {
    std::atomic<int> total_requests{0};
    std::atomic<int> error_count{0};
    std::atomic<double> total_response_time{0.0};
    std::chrono::steady_clock::time_point start_time;
    
    MetricsData() : start_time(std::chrono::steady_clock::now()) {}
};

class ProductService {
    struct Product {
        int id;
        std::string name;
        std::string description;
        double price;
        std::string category;
        bool available;
    };
    
    std::vector<Product> products_;
    int next_id_ = 1;
    
public:
    ProductService() {
        // Load sample data
        products_ = {
            {1, "Laptop", "High-performance laptop", 999.99, "Electronics", true},
            {2, "Mouse", "Wireless mouse", 29.99, "Electronics", true},
            {3, "Keyboard", "Mechanical keyboard", 129.99, "Electronics", true}
        };
        next_id_ = 4;
    }
    
    std::vector<Product> getAllProducts() { return products_; }
    
    Product getProductById(int id) {
        auto it = std::find_if(products_.begin(), products_.end(),
            [id](const Product& p) { return p.id == id; });
        
        if (it != products_.end()) {
            return *it;
        }
        
        throw std::runtime_error("Product not found");
    }
    
    std::vector<Product> getProductsByCategory(const std::string& category) {
        std::vector<Product> result;
        std::copy_if(products_.begin(), products_.end(), std::back_inserter(result),
            [&category](const Product& p) { return p.category == category; });
        return result;
    }
};

template <>
struct glz::meta<ProductService> {
    using T = ProductService;
    static constexpr auto value = object(
        &T::getAllProducts,
        &T::getProductById,
        &T::getProductsByCategory
    );
};

// Middleware for metrics collection
auto create_metrics_middleware(MetricsData& metrics) {
    return [&metrics](const glz::request& req, glz::response& res) {
        auto start_time = std::chrono::high_resolution_clock::now();
        
        metrics.total_requests++;
        
        // Store start time for response time calculation
        res.header("X-Request-Start", std::to_string(
            std::chrono::duration_cast<std::chrono::microseconds>(
                start_time.time_since_epoch()).count()));
    };
}

// Health check implementation
class HealthChecker {
public:
    HealthStatus get_health_status(const MetricsData& metrics) {
        HealthStatus status;
        status.version = "1.0.0";
        status.status = "healthy";
        
        auto now = std::chrono::steady_clock::now();
        status.uptime_seconds = std::chrono::duration_cast<std::chrono::seconds>(
            now - metrics.start_time).count();
        
        // Check dependencies (simulate)
        status.dependencies.database = check_database();
        status.dependencies.external_api = check_external_api();
        status.dependencies.redis = check_redis();
        
        // Calculate metrics
        int total_requests = metrics.total_requests.load();
        status.metrics.total_requests = total_requests;
        status.metrics.error_rate = total_requests > 0 ? 
            (metrics.error_count.load() * 100 / total_requests) : 0;
        status.metrics.avg_response_time = total_requests > 0 ?
            (metrics.total_response_time.load() / total_requests) : 0.0;
        
        // Determine overall health
        if (!status.dependencies.database || 
            !status.dependencies.external_api ||
            status.metrics.error_rate > 5) {
            status.status = "unhealthy";
        }
        
        return status;
    }
    
private:
    bool check_database() {
        // Simulate database health check
        return true;
    }
    
    bool check_external_api() {
        // Simulate external API health check
        return std::rand() % 10 > 1; // 90% healthy
    }
    
    bool check_redis() {
        // Simulate Redis health check
        return true;
    }
};

// Configuration loader
struct Config {
    int port = 8080;
    std::string log_level = "info";
    bool enable_cors = true;
    std::string database_url = "localhost:5432";
    std::string redis_url = "localhost:6379";
    
    static Config load_from_env() {
        Config config;
        
        if (const char* port_env = std::getenv("PORT")) {
            config.port = std::atoi(port_env);
        }
        
        if (const char* log_level = std::getenv("LOG_LEVEL")) {
            config.log_level = log_level;
        }
        
        if (const char* db_url = std::getenv("DATABASE_URL")) {
            config.database_url = db_url;
        }
        
        return config;
    }
};

int main() {
    // Load configuration
    Config config = Config::load_from_env();
    
    std::cout << "Starting Product Microservice v1.0.0" << std::endl;
    std::cout << "Port: " << config.port << std::endl;
    std::cout << "Log Level: " << config.log_level << std::endl;
    
    glz::http_server server;
    ProductService productService;
    MetricsData metrics;
    HealthChecker health_checker;
    
    // Add metrics middleware
    server.use(create_metrics_middleware(metrics));
    
    // Error handling middleware
    server.use([&metrics](const glz::request& req, glz::response& res) {
        if (res.status_code >= 400) {
            metrics.error_count++;
        }
    });
    
    // Create REST registry
    glz::registry<glz::opts{}, glz::REST> registry;
    registry.on(productService);
    
    // Mount API endpoints
    server.mount("/api/v1", registry.endpoints);
    
    // Health endpoints
    server.get("/health", [&health_checker, &metrics](const glz::request&, glz::response& res) {
        auto health = health_checker.get_health_status(metrics);
        
        if (health.status == "healthy") {
            res.status(200);
        } else {
            res.status(503);
        }
        
        res.json(health);
    });
    
    server.get("/health/ready", [](const glz::request&, glz::response& res) {
        // Readiness check - are we ready to receive traffic?
        res.json({{"status", "ready"}});
    });
    
    server.get("/health/live", [](const glz::request&, glz::response& res) {
        // Liveness check - is the service still running?
        res.json({{"status", "alive"}});
    });
    
    // Metrics endpoint
    server.get("/metrics", [&metrics](const glz::request&, glz::response& res) {
        res.content_type("text/plain").body(
            "# HELP http_requests_total Total HTTP requests\n"
            "# TYPE http_requests_total counter\n"
            "http_requests_total " + std::to_string(metrics.total_requests.load()) + "\n"
            "# HELP http_errors_total Total HTTP errors\n"
            "# TYPE http_errors_total counter\n"
            "http_errors_total " + std::to_string(metrics.error_count.load()) + "\n"
        );
    });
    
    // Info endpoint
    server.get("/info", [&config](const glz::request&, glz::response& res) {
        res.json({
            {"service", "product-service"},
            {"version", "1.0.0"},
            {"environment", config.log_level},
            {"build_time", __DATE__ " " __TIME__}
        });
    });
    
    // Enable CORS if configured
    if (config.enable_cors) {
        server.enable_cors();
    }
    
    try {
        std::cout << "Product Microservice listening on port " << config.port << std::endl;
        std::cout << "Health check: http://localhost:" << config.port << "/health" << std::endl;
        std::cout << "API docs: http://localhost:" << config.port << "/api/v1" << std::endl;
        std::cout << "Press Ctrl+C to gracefully shut down the server" << std::endl;
        
        server.bind(config.port)
              .with_signals();  // Enable built-in signal handling for graceful shutdown
        
        server.start();
        
        // Wait for shutdown signal (blocks until server stops)
        server.wait_for_signal();
        
        std::cout << "Product Microservice shut down successfully" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Failed to start server: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
```
