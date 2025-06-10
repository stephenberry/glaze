// Unit Tests for Glaze HTTP Examples
// Tests examples from the Glaze HTTP/REST Documentation
// See: http-examples.md

#include "ut/ut.hpp"
#include "glaze/glaze.hpp"
#include "glaze/net/http_router.hpp"
#include "glaze/net/http_server.hpp"
#include "glaze/rpc/registry.hpp"

#include <mutex>
#include <random>
#include <set>

using namespace ut;

// Test structures from Basic REST API Server example
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

// UserAPI class from Basic REST API Server example (examples.md - Basic REST API Server)
class UserAPI {
    std::unordered_map<int, User> users_;
    int next_id_ = 1;
    
public:
    UserAPI() {
        users_[1] = {1, "Alice Johnson", "alice@example.com", "2024-01-01T10:00:00Z"};
        users_[2] = {2, "Bob Smith", "bob@example.com", "2024-01-01T11:00:00Z"};
        next_id_ = 3;
    }
    
    std::vector<User> get_all_users() const {
        std::vector<User> user_list;
        for (const auto& [id, user] : users_) {
            user_list.push_back(user);
        }
        return user_list;
    }
    
    std::optional<User> get_user_by_id(int id) const {
        auto it = users_.find(id);
        if (it != users_.end()) {
            return it->second;
        }
        return std::nullopt;
    }
    
    User create_user(const CreateUserRequest& request) {
        User new_user;
        new_user.id = next_id_++;
        new_user.name = request.name;
        new_user.email = request.email;
        new_user.created_at = "2024-01-01T10:00:00Z"; // Fixed for testing
        
        users_[new_user.id] = new_user;
        return new_user;
    }
    
    bool update_user(int id, const UpdateUserRequest& request) {
        auto it = users_.find(id);
        if (it == users_.end()) {
            return false;
        }
        
        if (!request.name.empty()) {
            it->second.name = request.name;
        }
        if (!request.email.empty()) {
            it->second.email = request.email;
        }
        
        return true;
    }
    
    bool delete_user(int id) {
        auto it = users_.find(id);
        if (it != users_.end()) {
            users_.erase(it);
            return true;
        }
        return false;
    }
    
    size_t user_count() const { return users_.size(); }
};

// Task structures from Auto-Generated REST API example
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

struct TaskSearchRequest {
    std::string query{};
    bool completed_only{false};
    int limit{10};
};

// TaskService class from Auto-Generated REST API example (examples.md - Auto-Generated REST API)
class TaskService {
    std::vector<Task> tasks_;
    int next_id_ = 1;
    
public:
    TaskService() {
        tasks_ = {
            {1, "Learn Glaze", "Study the Glaze HTTP library", false, "2024-01-01T10:00:00Z", "2024-01-15T00:00:00Z"},
            {2, "Write documentation", "Create API documentation", false, "2024-01-02T10:00:00Z", "2024-01-20T00:00:00Z"}
        };
        next_id_ = 3;
    }
    
    std::vector<Task> getAllTasks() { return tasks_; }
    
    Task getTaskById(int id) {
        auto it = std::find_if(tasks_.begin(), tasks_.end(),
            [id](const Task& task) { return task.id == id; });
        
        if (it != tasks_.end()) {
            return *it;
        }
        
        throw std::runtime_error("Task not found");
    }
    
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
        task.created_at = "2024-01-01T10:00:00Z"; // Fixed for testing
        
        tasks_.push_back(task);
        return task;
    }
    
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
            
            if (int(results.size()) >= request.limit) {
                break;
            }
        }
        
        return results;
    }
    
    std::vector<Task> getCompletedTasks() {
        std::vector<Task> completed;
        
        std::copy_if(tasks_.begin(), tasks_.end(), std::back_inserter(completed),
            [](const Task& task) { return task.completed; });
        
        return completed;
    }
    
    size_t task_count() const { return tasks_.size(); }
};

// Chat structures from WebSocket Chat Server example
struct ChatMessage {
    std::string username;
    std::string message;
    std::string timestamp;
    std::string type = "message";
};

// Simplified ChatRoom for testing (examples.md - WebSocket Chat Server)
class ChatRoom {
    std::set<int> connection_ids_; // Simplified for testing
    std::vector<ChatMessage> message_history_;
    mutable std::mutex history_mutex_;
    int next_connection_id_ = 1;
    
public:
    int add_connection(const std::string& username) {
        int conn_id = next_connection_id_++;
        connection_ids_.insert(conn_id);
        
        // Broadcast join message
        ChatMessage join_msg;
        join_msg.username = "System";
        join_msg.message = username + " joined the chat";
        join_msg.timestamp = "2024-01-01T10:00:00Z";
        join_msg.type = "join";
        
        broadcast_message(join_msg);
        return conn_id;
    }
    
    void remove_connection(int conn_id, const std::string& username) {
        connection_ids_.erase(conn_id);
        
        ChatMessage leave_msg;
        leave_msg.username = "System";
        leave_msg.message = username + " left the chat";
        leave_msg.timestamp = "2024-01-01T10:00:00Z";
        leave_msg.type = "leave";
        
        broadcast_message(leave_msg);
    }
    
    void handle_message(const std::string& username, const std::string& message) {
        ChatMessage chat_msg;
        chat_msg.username = username;
        chat_msg.message = message;
        chat_msg.timestamp = "2024-01-01T10:00:00Z";
        chat_msg.type = "message";
        
        broadcast_message(chat_msg);
    }
    
    void broadcast_message(const ChatMessage& message) {
        std::lock_guard<std::mutex> lock(history_mutex_);
        message_history_.push_back(message);
        
        // Keep only last 100 messages
        if (message_history_.size() > 100) {
            message_history_.erase(message_history_.begin());
        }
    }
    
    size_t get_user_count() const { return connection_ids_.size(); }
    
    std::vector<ChatMessage> get_message_history() const {
        std::lock_guard<std::mutex> lock(history_mutex_);
        return message_history_;
    }
};

// Authentication structures from Simple Authentication Server example
struct LoginRequest {
    std::string username;
    std::string password;
};

struct LoginResponse {
    std::string token;
    std::string username;
    int expires_in;
};

struct AuthUser {
    int id;
    std::string username;
    std::string email;
    std::string role;
};

// SimpleAuthService from Simple Authentication Server example (examples.md - Simple Authentication Server)
class SimpleAuthService {
    std::unordered_map<std::string, AuthUser> users_;
    std::unordered_map<std::string, std::string> active_tokens_;
    
public:
    SimpleAuthService() {
        users_["admin"] = {1, "admin", "admin@example.com", "admin"};
        users_["user"] = {2, "user", "user@example.com", "user"};
    }
    
    std::optional<LoginResponse> login(const LoginRequest& request) {
        if ((request.username == "admin" && request.password == "admin123") ||
            (request.username == "user" && request.password == "user123")) {
            
            std::string token = generate_token();
            active_tokens_[token] = request.username;
            
            return LoginResponse{token, request.username, 3600};
        }
        
        return std::nullopt;
    }
    
    std::optional<AuthUser> validate_token(const std::string& token) {
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
    
    size_t active_token_count() const { return active_tokens_.size(); }
    
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

// Microservice structures from Microservice Template example
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

// ProductService from Microservice Template example (examples.md - Microservice Template)
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
    
    size_t product_count() const { return products_.size(); }
};

// HealthChecker from Microservice Template example
class HealthChecker {
public:
    HealthStatus get_health_status(const MetricsData& metrics) {
        HealthStatus status;
        status.version = "1.0.0";
        status.status = "healthy";
        
        auto now = std::chrono::steady_clock::now();
        status.uptime_seconds = std::chrono::duration_cast<std::chrono::seconds>(
            now - metrics.start_time).count();
        
        // Check dependencies (mocked for testing)
        status.dependencies.database = true;
        status.dependencies.external_api = true;
        status.dependencies.redis = true;
        
        // Calculate metrics
        int total_requests = metrics.total_requests.load();
        status.metrics.total_requests = total_requests;
        status.metrics.error_rate = total_requests > 0 ? 
            (metrics.error_count.load() * 100 / total_requests) : 0;
        status.metrics.avg_response_time = total_requests > 0 ?
            (metrics.total_response_time.load() / total_requests) : 0.0;
        
        return status;
    }
};

// Basic HTTP Router tests
suite http_router_tests = [] {
    "router_creation"_test = [] {
        glz::http_router router;
        expect(true) << "HTTP router should be created successfully\n";
    };
    
    "route_registration"_test = [] {
        glz::http_router router;
        
        router.get("/test", [](const glz::request&, glz::response& res) {
            res.body("test response");
        });
        
        auto [handler, params] = router.match(glz::http_method::GET, "/test");
        expect(handler != nullptr) << "Route should be registered and matchable\n";
        expect(params.empty()) << "Static route should have no parameters\n";
    };
    
    "parameter_routes"_test = [] {
        glz::http_router router;
        
        router.get("/users/:id", [](const glz::request&, glz::response&) {});
        
        auto [handler, params] = router.match(glz::http_method::GET, "/users/123");
        expect(handler != nullptr) << "Parameter route should match\n";
        expect(params.size() == 1) << "Should extract one parameter\n";
        expect(params.at("id") == "123") << "Parameter value should be extracted correctly\n";
    };
};

// Basic REST API Server tests (examples.md - Basic REST API Server)
suite basic_rest_api_tests = [] {
    "user_api_initialization"_test = [] {
        UserAPI api;
        expect(api.user_count() == 2) << "API should initialize with 2 users\n";
        
        auto users = api.get_all_users();
        expect(users.size() == 2) << "Should return 2 initial users\n";
        expect(users[0].name == "Alice Johnson" || users[1].name == "Alice Johnson") 
            << "Should contain Alice Johnson\n";
    };
    
    "user_creation"_test = [] {
        UserAPI api;
        
        CreateUserRequest request{"John Doe", "john@example.com"};
        auto new_user = api.create_user(request);
        
        expect(new_user.id > 0) << "New user should have valid ID\n";
        expect(new_user.name == "John Doe") << "User name should match request\n";
        expect(new_user.email == "john@example.com") << "User email should match request\n";
        expect(api.user_count() == 3) << "User count should increase\n";
    };
    
    "user_retrieval"_test = [] {
        UserAPI api;
        
        auto user = api.get_user_by_id(1);
        expect(user.has_value()) << "Should find existing user\n";
        expect(user->id == 1) << "Should return correct user ID\n";
        
        auto missing_user = api.get_user_by_id(999);
        expect(!missing_user.has_value()) << "Should return nullopt for missing user\n";
    };
    
    "user_update"_test = [] {
        UserAPI api;
        
        UpdateUserRequest update{"Alice Smith", "alice.smith@example.com"};
        bool updated = api.update_user(1, update);
        
        expect(updated) << "Should successfully update existing user\n";
        
        auto user = api.get_user_by_id(1);
        expect(user->name == "Alice Smith") << "Name should be updated\n";
        expect(user->email == "alice.smith@example.com") << "Email should be updated\n";
    };
    
    "user_deletion"_test = [] {
        UserAPI api;
        
        bool deleted = api.delete_user(1);
        expect(deleted) << "Should successfully delete existing user\n";
        expect(api.user_count() == 1) << "User count should decrease\n";
        
        auto user = api.get_user_by_id(1);
        expect(!user.has_value()) << "Deleted user should not be found\n";
    };
};

// Auto-Generated REST API tests (examples.md - Auto-Generated REST API)
suite auto_generated_rest_tests = [] {
    "task_service_initialization"_test = [] {
        TaskService service;
        expect(service.task_count() == 2) << "Service should initialize with 2 tasks\n";
        
        auto tasks = service.getAllTasks();
        expect(tasks.size() == 2) << "Should return 2 initial tasks\n";
        expect(tasks[0].title == "Learn Glaze" || tasks[1].title == "Learn Glaze")
            << "Should contain 'Learn Glaze' task\n";
    };
    
    "task_creation"_test = [] {
        TaskService service;
        
        CreateTaskRequest request{"Test Task", "Description", "2024-02-01T00:00:00Z"};
        auto new_task = service.createTask(request);
        
        expect(new_task.id > 0) << "New task should have valid ID\n";
        expect(new_task.title == "Test Task") << "Task title should match request\n";
        expect(!new_task.completed) << "New task should not be completed\n";
        expect(service.task_count() == 3) << "Task count should increase\n";
    };
    
    "task_creation_validation"_test = [] {
        TaskService service;
        
        CreateTaskRequest empty_request{"", "", ""};
        expect(throws([&] {
            service.createTask(empty_request);
        })) << "Should throw exception for empty title\n";
    };
    
    "task_retrieval"_test = [] {
        TaskService service;
        
        auto task = service.getTaskById(1);
        expect(task.id == 1) << "Should return correct task\n";
        
        expect(throws([&] {
            service.getTaskById(999);
        })) << "Should throw exception for missing task\n";
    };
    
    "task_search"_test = [] {
        TaskService service;
        
        TaskSearchRequest search{"Glaze", false, 10};
        auto results = service.searchTasks(search);
        
        expect(results.size() > 0) << "Should find tasks matching 'Glaze'\n";
        expect(results[0].title.find("Glaze") != std::string::npos ||
               results[0].description.find("Glaze") != std::string::npos)
            << "Results should contain search term\n";
    };
    
    "completed_tasks_filter"_test = [] {
        TaskService service;
        
        auto completed = service.getCompletedTasks();
        expect(completed.empty()) << "Should have no completed tasks initially\n";
    };
};

// WebSocket Chat Server tests (examples.md - WebSocket Chat Server)
suite websocket_chat_tests = [] {
    "chat_room_initialization"_test = [] {
        ChatRoom room;
        expect(room.get_user_count() == 0) << "Chat room should start empty\n";
        expect(room.get_message_history().empty()) << "Message history should start empty\n";
    };
    
    "user_connection"_test = [] {
        ChatRoom room;
        
        int conn_id = room.add_connection("Alice");
        expect(conn_id > 0) << "Should return valid connection ID\n";
        expect(room.get_user_count() == 1) << "User count should increase\n";
        
        auto history = room.get_message_history();
        expect(history.size() == 1) << "Should have join message in history\n";
        expect(history[0].type == "join") << "Should be a join message\n";
        expect(history[0].message.find("Alice") != std::string::npos)
            << "Join message should contain username\n";
    };
    
    "message_handling"_test = [] {
        ChatRoom room;
        
        room.add_connection("Alice");
        room.handle_message("Alice", "Hello, world!");
        
        auto history = room.get_message_history();
        expect(history.size() == 2) << "Should have join and message\n";
        expect(history[1].type == "message") << "Should be a message type\n";
        expect(history[1].username == "Alice") << "Should have correct username\n";
        expect(history[1].message == "Hello, world!") << "Should have correct message\n";
    };
    
    "user_disconnection"_test = [] {
        ChatRoom room;
        
        int conn_id = room.add_connection("Alice");
        room.remove_connection(conn_id, "Alice");
        
        expect(room.get_user_count() == 0) << "User count should decrease\n";
        
        auto history = room.get_message_history();
        expect(history.size() == 2) << "Should have join and leave messages\n";
        expect(history[1].type == "leave") << "Should be a leave message\n";
    };
    
    "multiple_users"_test = [] {
        ChatRoom room;
        
        [[maybe_unused]] int alice_id = room.add_connection("Alice");
        [[maybe_unused]] int bob_id = room.add_connection("Bob");
        
        expect(room.get_user_count() == 2) << "Should have 2 users\n";
        
        room.handle_message("Alice", "Hi Bob!");
        room.handle_message("Bob", "Hi Alice!");
        
        auto history = room.get_message_history();
        expect(history.size() == 4) << "Should have 2 joins + 2 messages\n";
    };
};

// Simple Authentication Server tests (examples.md - Simple Authentication Server)
suite authentication_tests = [] {
    "auth_service_initialization"_test = [] {
        SimpleAuthService auth;
        expect(auth.active_token_count() == 0) << "Should start with no active tokens\n";
    };
    
    "successful_login"_test = [] {
        SimpleAuthService auth;
        
        LoginRequest valid_request{"admin", "admin123"};
        auto response = auth.login(valid_request);
        
        expect(response.has_value()) << "Should return login response for valid credentials\n";
        expect(response->username == "admin") << "Should return correct username\n";
        expect(!response->token.empty()) << "Should return non-empty token\n";
        expect(response->expires_in == 3600) << "Should return correct expiration time\n";
        expect(auth.active_token_count() == 1) << "Should track active token\n";
    };
    
    "failed_login"_test = [] {
        SimpleAuthService auth;
        
        LoginRequest invalid_request{"admin", "wrongpassword"};
        auto response = auth.login(invalid_request);
        
        expect(!response.has_value()) << "Should return nullopt for invalid credentials\n";
        expect(auth.active_token_count() == 0) << "Should not create token for failed login\n";
    };
    
    "token_validation"_test = [] {
        SimpleAuthService auth;
        
        LoginRequest request{"user", "user123"};
        auto login_response = auth.login(request);
        
        auto user = auth.validate_token(login_response->token);
        expect(user.has_value()) << "Should validate valid token\n";
        expect(user->username == "user") << "Should return correct user info\n";
        expect(user->role == "user") << "Should return correct user role\n";
    };
    
    "invalid_token_validation"_test = [] {
        SimpleAuthService auth;
        
        auto user = auth.validate_token("invalid_token");
        expect(!user.has_value()) << "Should reject invalid token\n";
    };
    
    "logout"_test = [] {
        SimpleAuthService auth;
        
        LoginRequest request{"admin", "admin123"};
        auto login_response = auth.login(request);
        
        auth.logout(login_response->token);
        expect(auth.active_token_count() == 0) << "Should remove token on logout\n";
        
        auto user = auth.validate_token(login_response->token);
        expect(!user.has_value()) << "Should invalidate token after logout\n";
    };
    
    "multiple_tokens"_test = [] {
        SimpleAuthService auth;
        
        LoginRequest admin_request{"admin", "admin123"};
        LoginRequest user_request{"user", "user123"};
        
        auto admin_response = auth.login(admin_request);
        auto user_response = auth.login(user_request);
        
        expect(auth.active_token_count() == 2) << "Should track multiple tokens\n";
        
        auto admin_user = auth.validate_token(admin_response->token);
        auto user_user = auth.validate_token(user_response->token);
        
        expect(admin_user->role == "admin") << "Should validate admin token\n";
        expect(user_user->role == "user") << "Should validate user token\n";
    };
};

// Microservice Template tests (examples.md - Microservice Template)
suite microservice_tests = [] {
    "product_service_initialization"_test = [] {
        ProductService service;
        expect(service.product_count() == 3) << "Service should initialize with 3 products\n";
        
        auto products = service.getAllProducts();
        expect(products.size() == 3) << "Should return 3 initial products\n";
        expect(products[0].category == "Electronics") << "Products should be in Electronics category\n";
    };
    
    "product_retrieval"_test = [] {
        ProductService service;
        
        auto product = service.getProductById(1);
        expect(product.id == 1) << "Should return correct product\n";
        expect(product.name == "Laptop") << "Should return correct product name\n";
        expect(product.available) << "Product should be available\n";
        
        expect(throws([&] {
            service.getProductById(999);
        })) << "Should throw exception for missing product\n";
    };
    
    "products_by_category"_test = [] {
        ProductService service;
        
        auto electronics = service.getProductsByCategory("Electronics");
        expect(electronics.size() == 3) << "Should return all electronics products\n";
        
        auto nonexistent = service.getProductsByCategory("Books");
        expect(nonexistent.empty()) << "Should return empty vector for nonexistent category\n";
    };
    
    "metrics_tracking"_test = [] {
        MetricsData metrics;
        
        metrics.total_requests += 10;
        metrics.error_count += 2;
        metrics.total_response_time += 150.5;
        
        expect(metrics.total_requests.load() == 10) << "Should track total requests\n";
        expect(metrics.error_count.load() == 2) << "Should track error count\n";
        expect(metrics.total_response_time.load() == 150.5) << "Should track response time\n";
    };
    
    "health_status_generation"_test = [] {
        MetricsData metrics;
        HealthChecker checker;
        
        metrics.total_requests = 100;
        metrics.error_count = 5;
        metrics.total_response_time = 250.0;
        
        auto status = checker.get_health_status(metrics);
        
        expect(status.status == "healthy") << "Should report healthy status\n";
        expect(status.version == "1.0.0") << "Should report correct version\n";
        expect(status.metrics.total_requests == 100) << "Should report correct request count\n";
        expect(status.metrics.error_rate == 5) << "Should calculate correct error rate\n";
        expect(status.metrics.avg_response_time == 2.5) << "Should calculate correct avg response time\n";
        expect(status.dependencies.database) << "Should report database as healthy\n";
    };
};

// JSON serialization tests for all structures
suite json_serialization_tests = [] {
    "user_json_serialization"_test = [] {
        User user{1, "John Doe", "john@example.com", "2024-01-01T10:00:00Z"};
        
        std::string json;
        auto ec = glz::write_json(user, json);
        expect(!ec) << "Should serialize User to JSON without error\n";
        expect(!json.empty()) << "Should produce non-empty JSON\n";
        
        User deserialized;
        ec = glz::read_json(deserialized, json);
        expect(!ec) << "Should deserialize User from JSON without error\n";
        expect(deserialized.id == user.id) << "Should preserve user ID\n";
        expect(deserialized.name == user.name) << "Should preserve user name\n";
    };
    
    "task_json_serialization"_test = [] {
        Task task{1, "Test Task", "Description", false, "2024-01-01T10:00:00Z", "2024-01-15T00:00:00Z"};
        
        std::string json;
        auto ec = glz::write_json(task, json);
        expect(!ec) << "Should serialize Task to JSON without error\n";
        
        Task deserialized;
        ec = glz::read_json(deserialized, json);
        expect(!ec) << "Should deserialize Task from JSON without error\n";
        expect(deserialized.title == task.title) << "Should preserve task title\n";
        expect(deserialized.completed == task.completed) << "Should preserve completion status\n";
    };
    
    "chat_message_json_serialization"_test = [] {
        ChatMessage msg{"Alice", "Hello world", "2024-01-01T10:00:00Z", "message"};
        
        std::string json;
        auto ec = glz::write_json(msg, json);
        expect(!ec) << "Should serialize ChatMessage to JSON without error\n";
        
        ChatMessage deserialized;
        ec = glz::read_json(deserialized, json);
        expect(!ec) << "Should deserialize ChatMessage from JSON without error\n";
        expect(deserialized.username == msg.username) << "Should preserve username\n";
        expect(deserialized.message == msg.message) << "Should preserve message\n";
    };
    
    "error_response_json_serialization"_test = [] {
        ErrorResponse error{"validation_error", "Invalid input data"};
        
        std::string json;
        auto ec = glz::write_json(error, json);
        expect(!ec) << "Should serialize ErrorResponse to JSON without error\n";
        
        ErrorResponse deserialized;
        ec = glz::read_json(deserialized, json);
        expect(!ec) << "Should deserialize ErrorResponse from JSON without error\n";
        expect(deserialized.error == error.error) << "Should preserve error type\n";
        expect(deserialized.message == error.message) << "Should preserve error message\n";
    };
};

// Performance and thread safety tests
suite performance_tests = [] {
    "concurrent_user_operations"_test = [] {
        UserAPI api;
        std::atomic<int> operations_completed{0};
        
        std::vector<std::thread> threads;
        for (int i = 0; i < 5; ++i) {
            threads.emplace_back([&api, &operations_completed, i]() {
                CreateUserRequest request{"User" + std::to_string(i), "user" + std::to_string(i) + "@test.com"};
                api.create_user(request);
                operations_completed++;
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        expect(operations_completed.load() == 5) << "All concurrent operations should complete\n";
        expect(api.user_count() >= 5) << "Should have created at least 5 new users\n";
    };
    
    "chat_room_concurrent_messages"_test = [] {
        ChatRoom room;
        room.add_connection("Alice");
        room.add_connection("Bob");
        
        std::atomic<int> messages_sent{0};
        
        std::vector<std::thread> threads;
        for (int i = 0; i < 10; ++i) {
            threads.emplace_back([&room, &messages_sent, i]() {
                room.handle_message("User", "Message " + std::to_string(i));
                messages_sent++;
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        expect(messages_sent.load() == 10) << "All messages should be sent\n";
        auto history = room.get_message_history();
        expect(history.size() >= 10) << "All messages should be in history\n";
    };
    
    "metrics_atomic_operations"_test = [] {
        MetricsData metrics;
        std::atomic<int> operations_completed{0};
        
        std::vector<std::thread> threads;
        for (int i = 0; i < 100; ++i) {
            threads.emplace_back([&metrics, &operations_completed]() {
                metrics.total_requests++;
                metrics.error_count++;
                metrics.total_response_time += 1.0;
                operations_completed++;
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        expect(operations_completed.load() == 100) << "All operations should complete\n";
        expect(metrics.total_requests.load() == 100) << "Request count should be correct\n";
        expect(metrics.error_count.load() == 100) << "Error count should be correct\n";
        expect(metrics.total_response_time.load() == 100.0) << "Response time should be correct\n";
    };
};

// Integration tests combining multiple components
suite integration_tests = [] {
    "user_api_with_auth"_test = [] {
        UserAPI user_api;
        SimpleAuthService auth;
        
        // Login as admin
        LoginRequest login{"admin", "admin123"};
        auto login_response = auth.login(login);
        expect(login_response.has_value()) << "Admin should be able to login\n";
        
        // Validate token
        auto user = auth.validate_token(login_response->token);
        expect(user.has_value()) << "Token should be valid\n";
        expect(user->role == "admin") << "Should have admin role\n";
        
        // Create user with valid auth
        if (user->role == "admin") {
            CreateUserRequest create_req{"New User", "newuser@test.com"};
            auto new_user = user_api.create_user(create_req);
            expect(new_user.id > 0) << "Admin should be able to create users\n";
        }
    };
    
    "task_service_with_chat_notifications"_test = [] {
        TaskService tasks;
        ChatRoom chat;
        
        // Add a user to chat
        [[maybe_unused]] int conn_id = chat.add_connection("TaskBot");
        
        // Create a task and notify chat
        CreateTaskRequest request{"Important Task", "This needs attention", ""};
        auto task = tasks.createTask(request);
        
        chat.handle_message("TaskBot", "New task created: " + task.title);
        
        auto history = chat.get_message_history();
        expect(history.size() >= 2) << "Should have join and task notification messages\n";
        
        bool found_task_message = false;
        for (const auto& msg : history) {
            if (msg.message.find("Important Task") != std::string::npos) {
                found_task_message = true;
                break;
            }
        }
        expect(found_task_message) << "Should find task notification in chat history\n";
    };
    
    "microservice_health_with_metrics"_test = [] {
        ProductService products;
        MetricsData metrics;
        HealthChecker checker;
        
        // Simulate some requests
        metrics.total_requests = 50;
        metrics.error_count = 2;
        metrics.total_response_time = 125.0;
        
        // Get health status
        auto health = checker.get_health_status(metrics);
        
        expect(health.status == "healthy") << "Should report healthy status\n";
        expect(health.metrics.total_requests == 50) << "Should report correct metrics\n";
        expect(health.metrics.error_rate == 4) << "Should calculate 4% error rate\n";
        
        // Verify products are accessible
        auto all_products = products.getAllProducts();
        expect(all_products.size() == 3) << "Products should be accessible when healthy\n";
    };
};

int main() {
    std::cout << "Testing implementations from http_examples.md documentation\n\n";
    return 0;
}
