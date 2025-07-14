#include "glaze/glaze.hpp"
#include "glaze/net/http_server.hpp"

// main.cpp
int main()
{
   glz::http_server server{};

   // Define a route with a description and tags
   server.get(
      "/users/:id",
      [](const glz::request& req, glz::response& res) {
         res.body("Fetching profile for user ID: " + req.params.at("id"));
      },
      {.description = "Get a user by ID"});

   // Define another route for context
   server.post(
      "/users",
      [](const glz::request&, glz::response& res) {
         res.status(201).body("User created");
      }
   );

   // Enable the OpenAPI specification endpoint
   server.enable_openapi_spec(
      "/openapi.json", // The path for the spec
      "My User API",   // The title of the API
      "1.0.0"          // The version of the API
   );

   // Start the server
   server.bind("127.0.0.1", 8080);
   server.start();
   std::cout << "Server listening on http://127.0.0.1:8080" << std::endl;
   std::cin.get();

   return 0;
}
