// Glaze REST Demo Server

#include <filesystem>
#include <fstream>
#include <iostream>

#include "glaze/glaze.hpp"
#include "glaze/net/http_server.hpp"
#include "glaze/rpc/registry.hpp"

struct User
{
   int id{};
   std::string name{};
   std::string email{};
   std::string avatar{};
};

struct UserIdRequest
{
   int id{};
};

struct UserCreateRequest
{
   std::string name{};
   std::string email{};
   std::string avatar{};
};

struct UserUpdateRequest
{
   int id{};
   std::string name{};
   std::string email{};
   std::string avatar{};
};

struct DeleteResponse
{
   bool success{};
   std::string message{};
};

struct PostCreateRequest
{
   std::string title{};
   std::string body{};
   std::string author{};
};

struct Post
{
   int id{};
   std::string title{};
   std::string body{};
   std::string author{};
   std::string createdAt{};
};

// User service with CRUD operations
struct UserService
{
   std::unordered_map<int, User> users = {{1, {1, "Alice Johnson", "alice@example.com", "👩‍💼"}},
                                          {2, {2, "Bob Smith", "bob@example.com", "👨‍💻"}},
                                          {3, {3, "Carol Davis", "carol@example.com", "👩‍🎨"}}};
   int next_id = 4;

   // Get all users
   std::vector<User> getAllUsers()
   {
      std::vector<User> user_list;
      for (const auto& [id, user] : users) {
         user_list.push_back(user);
      }
      return user_list;
   }

   // Get user by ID
   User getUserById(const UserIdRequest& request)
   {
      auto it = users.find(request.id);
      if (it != users.end()) {
         return it->second;
      }
      return User{}; // Return empty user if not found
   }

   // Create a new user
   User createUser(const UserCreateRequest& request)
   {
      User user;
      user.id = next_id++;
      user.name = request.name;
      user.email = request.email;
      user.avatar = request.avatar.empty() ? "👤" : request.avatar;
      users[user.id] = user;
      return user;
   }

   // Update existing user
   User updateUser(const UserUpdateRequest& request)
   {
      auto it = users.find(request.id);
      if (it != users.end()) {
         it->second.name = request.name;
         it->second.email = request.email;
         if (!request.avatar.empty()) {
            it->second.avatar = request.avatar;
         }
         return it->second;
      }
      return User{}; // Return empty user if not found
   }

   // Delete user
   DeleteResponse deleteUser(const UserIdRequest& request)
   {
      auto it = users.find(request.id);
      if (it != users.end()) {
         users.erase(it);
         return {true, "User deleted successfully"};
      }
      return {false, "User not found"};
   }
};

// Simple blog post service for more complex demo
struct PostService
{
   std::unordered_map<int, Post> posts = {
      {1,
       {1, "Welcome to Glaze", "This is a demo of Mithril with a Glaze C++ backend.", "Alice Johnson",
        "2025-05-27T10:00:00Z"}},
      {2,
       {2, "Building REST APIs", "Learn how to build REST APIs with Glaze library.", "Bob Smith",
        "2025-05-27T11:00:00Z"}}};
   int next_id = 3;

   std::vector<Post> getAllPosts()
   {
      std::vector<Post> post_list;
      for (const auto& [id, post] : posts) {
         post_list.push_back(post);
      }
      return post_list;
   }

   Post createPost(const PostCreateRequest& request)
   {
      Post post;
      post.id = next_id++;
      post.title = request.title;
      post.body = request.body;
      post.author = request.author;
      post.createdAt = std::to_string(std::time(nullptr)); // Simple timestamp
      posts[post.id] = post;
      return post;
   }
};

// Glaze metadata for reflection
template <>
struct glz::meta<UserService>
{
   using T = UserService;
   static constexpr auto value =
      object(&T::getAllUsers, &T::getUserById, &T::createUser, &T::updateUser, &T::deleteUser);
};

template <>
struct glz::meta<PostService>
{
   using T = PostService;
   static constexpr auto value = object(&T::getAllPosts, &T::createPost);
};

std::string read_file(const std::string& path)
{
   std::string full_path = std::string{SOURCE_DIR} + "/" + path;
   std::ifstream file(full_path, std::ios::ate | std::ios::binary);

   if (!file.is_open()) {
      std::cerr << "Failed to open " << full_path << ", current directory: " << std::filesystem::current_path().string()
                << "\n";
      return "";
   }
   // Get the file size from the current position (since we used std::ios::ate)
   std::streamsize size = file.tellg();
   file.seekg(0, std::ios::beg);

   std::string buffer;
   buffer.resize(size);
   if (file.read(buffer.data(), size)) {
      return buffer;
   }
   return "";
}

// Helper function to check if file exists
bool file_exists(const std::string& path)
{
   std::string full_path = std::string{SOURCE_DIR} + "/" + path;
   return std::filesystem::exists(full_path);
}

// Helper function to get MIME type
std::string get_mime_type(const std::string& path)
{
   std::filesystem::path p(path);
   std::string ext = p.extension().string();

   if (ext == ".html") return "text/html";
   if (ext == ".js") return "application/javascript";
   if (ext == ".css") return "text/css";
   if (ext == ".json") return "application/json";
   if (ext == ".png") return "image/png";
   if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
   if (ext == ".gif") return "image/gif";
   if (ext == ".svg") return "image/svg+xml";

   return "text/plain";
}

int main()
{
   glz::http_server server;

   // Create service instances
   UserService userService;
   PostService postService;

   // Create REST registry
   glz::registry<glz::opts{}, glz::REST> registry;

   // Register services
   registry.on(userService);
   registry.on(postService);

   // OPTION 1: Enable CORS with default settings (allow all origins - good for development)
   server.enable_cors();

   // OPTION 2: Enable CORS with custom configuration
   /*
    glz::cors_config cors_config;
    cors_config.allowed_origins = {"http://localhost:3000", "https://myapp.com"};
    cors_config.allowed_methods = {"GET", "POST", "PUT", "DELETE"};
    cors_config.allowed_headers = {"Content-Type", "Authorization", "X-API-Key"};
    cors_config.allow_credentials = true;
    cors_config.max_age = 3600; // 1 hour
    server.enable_cors(cors_config);
    */

   // OPTION 3: Enable CORS for specific origins (good for production)
   /*
    server.enable_cors({
    "https://myapp.com",
    "https://api.myapp.com"
    }, true); // allow credentials
    */

   // Mount API endpoints
   server.mount("/api", registry.endpoints);

   // Serve static files
   server.get("/", [](const glz::request& /*req*/, glz::response& res) {
      std::string html = read_file("index.html");
      if (html.empty()) {
         res.status(404).body("index.html not found");
      }
      else {
         res.content_type("text/html").body(html);
      }
   });

   // Example of a custom endpoint that returns CORS headers
   server.get("/test-cors", [](const glz::request& req, glz::response& res) {
      // The CORS middleware will automatically add the appropriate headers
      res.json({{"message", "CORS test endpoint"},
                {"origin", req.headers.count("origin") ? req.headers.at("origin") : "none"},
                {"method", glz::to_string(req.method)}});
   });

   // Start the server
   server.bind("127.0.0.1", 8080).with_signals(); // Enable signal handling for graceful shutdown

   std::cout << "Glaze Demo Server running on http://127.0.0.1:8080\n";
   std::cout << "Press Ctrl+C to gracefully shut down the server\n\n";

   server.start();

   // Wait for shutdown signal (blocks until server stops)
   server.wait_for_signal();

   std::cout << "Server shut down successfully\n";
   return 0;
}
