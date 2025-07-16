#include <chrono>
#include <thread>

#include "glaze/glaze.hpp"
#include "glaze/net/http_client.hpp"
#include "glaze/net/http_server.hpp"
#include "glaze/rpc/registry.hpp"
#include "ut/ut.hpp"

using namespace ut;

struct User
{
   int id{};
   std::string name{};
   std::string email{};
};

struct UserIdRequest
{
   int id{};
};

struct CreateUserRequest
{
   std::string name{};
   std::string email{};
};

struct ErrorResponse
{
   std::string error{};
   int code{};
};

// Define the UserService that will be exposed via REST
struct UserService
{
   // State
   std::unordered_map<int, User> users = {{1, {1, "Alice", "alice@example.com"}},
                                          {2, {2, "Bob", "bob@example.com"}},
                                          {3, {3, "Charlie", "charlie@example.com"}}};
   int next_id = 4;

   // Methods that will be exposed as REST endpoints

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
      // Return empty user if not found
      return User{};
   }

   // Create a new user
   User createUser(CreateUserRequest&& request)
   {
      User user{next_id++, request.name, request.email};
      users[user.id] = user;
      return user;
   }

   // Delete a user
   bool deleteUser(const UserIdRequest& request)
   {
      auto it = users.find(request.id);
      if (it != users.end()) {
         users.erase(it);
         return true;
      }
      return false;
   }
};

template <>
struct glz::meta<UserService>
{
   using T = UserService;
   static constexpr auto value = object(&T::getAllUsers, &T::getUserById, &T::createUser, &T::deleteUser);
};

int main()
{
   "openapi_test"_test = [] {
      glz::http_server server{};

      UserService userService;

      // Create a REST registry
      glz::registry<glz::opts{}, glz::REST> registry;

      // Register the UserService with the registry using server.on pattern
      registry.on(userService);

      // Mount the registry endpoints to the server
      server.mount("/api", registry.endpoints);

      // Add some custom GET endpoints
      server.get("/health", [](const glz::request&, glz::response& res) {
         res.content_type("application/json").body(R"({"status": "healthy", "timestamp": "2025-01-01T00:00:00Z"})");
      });

      server.get("/version", [](const glz::request&, glz::response& res) {
         res.content_type("application/json")
            .body(R"({"version": "1.0.0", "service": "user-management", "build": "dev"})");
      });

      // Add some custom PUT endpoints
      server.put("/settings", [](const glz::request&, glz::response& res) {
         res.content_type("application/json").body(R"({"message": "Settings updated successfully"})");
      });

      server.put("/config/database", [](const glz::request&, glz::response& res) {
         res.content_type("application/json").body(R"({"message": "Database configuration updated", "applied": true})");
      });

      // Add a custom POST endpoint as well
      server.post("/auth/login", [](const glz::request&, glz::response& res) {
         res.content_type("application/json").body(R"({"token": "abc123", "expires_in": 3600, "user_id": 1})");
      });

      // Enable the OpenAPI specification endpoint
      server.enable_openapi_spec("/openapi.json", // The path for the spec
                                 "User Management API", // The title of the API
                                 "1.0.0" // The version of the API
      );

      // Test that the registry has registered the endpoints
      expect(registry.endpoints.routes.size() > 0);

      // Basic validation that endpoints were registered
      // The registry should have registered endpoints for:
      // - GET /getAllUsers
      // - POST /getUserById
      // - POST /createUser
      // - POST /deleteUser

      std::cout << "OpenAPI test completed successfully!" << std::endl;
      std::cout << "Registry has " << registry.endpoints.routes.size() << " endpoints registered" << std::endl;

      // Print registered endpoints for inspection
      for (const auto& [path, methods] : registry.endpoints.routes) {
         for (const auto& [method, entry] : methods) {
            std::cout << "  - " << glz::to_string(method) << " " << path << std::endl;
         }
      }

      // Start the server in a separate thread
      std::thread server_thread([&]() {
         server.bind("127.0.0.1", 8080);
         server.start();
      });

      // Give the server time to start
      std::this_thread::sleep_for(std::chrono::milliseconds(100));

      // Make HTTP request to get OpenAPI spec
      try {
         glz::http_client client{};
         auto response_result = client.get("http://127.0.0.1:8080/openapi.json");

         if (response_result.has_value()) {
            const auto& response = response_result.value();

            std::cout << "\n" << std::string(80, '=') << std::endl;
            std::cout << "OpenAPI Specification from /openapi.json:" << std::endl;
            std::cout << std::string(80, '=') << std::endl;
            std::cout << glz::prettify_json(response.response_body) << std::endl;
            std::cout << std::string(80, '=') << std::endl;

            // Basic validation that we got a valid OpenAPI response
            expect(response.status_code == 200);
            expect(response.response_body.find("openapi") != std::string::npos);
            expect(response.response_body.find("User Management API") != std::string::npos);
            expect(response.response_body.find("paths") != std::string::npos);
         }
         else {
            std::cout << "HTTP request failed with error code: " << response_result.error().value() << std::endl;
            expect(false) << "Failed to get OpenAPI spec";
         }
      }
      catch (const std::exception& e) {
         std::cout << "Error making HTTP request: " << e.what() << std::endl;
         expect(false) << "Failed to get OpenAPI spec";
      }

      // Stop the server
      server.stop();
      if (server_thread.joinable()) {
         server_thread.join();
      }
   };

   return 0;
}
