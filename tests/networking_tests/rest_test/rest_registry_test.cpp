// Glaze Library
// For the license information refer to glaze.hpp

#include <fstream>
#include <iostream>

#include "glaze/glaze.hpp"
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

struct ErrorResponse
{
   std::string error{};
};

// Define the UserService that will be exposed via REST
struct UserService
{
   // State
   std::unordered_map<int, User> users = {{1, {1, "John Doe", "john@example.com"}},
                                          {2, {2, "Jane Smith", "jane@example.com"}}};
   int next_id = 3;

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
      // In a real app, you'd want proper error handling
      // For simplicity, we'll return an empty user
      return User{};
   }

   // Create a new user
   User createUser(User&& user)
   {
      user.id = next_id++;
      users[user.id] = user;
      return user;
   }
};

template <>
struct glz::meta<UserService>
{
   using T = UserService;
   static constexpr auto value = object(&T::getAllUsers, &T::getUserById, &T::createUser);
};

int main()
{
   glz::http_server server;

   UserService userService;

   glz::registry<glz::opts{}, glz::REST> registry;

   registry.on(userService);

   server.mount("/api", registry.endpoints);

   // Serve the frontend files
   server.get("/", [](const glz::request& /*req*/, glz::response& res) {
      std::string_view html = R"(
      <!DOCTYPE html>
      <html lang="en">
      <head>
      <meta charset="UTF-8">
      <meta name="viewport" content="width=device-width, initial-scale=1.0">
      <title>Glaze REST API Demo</title>
      <style>
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            margin: 0;
            padding: 20px;
            background-color: #f5f5f5;
            color: #333;
        }
        .container {
            max-width: 800px;
            margin: 0 auto;
            background-color: white;
            border-radius: 8px;
            padding: 20px;
            box-shadow: 0 2px 10px rgba(0, 0, 0, 0.1);
        }
        h1 {
            color: #2c3e50;
            margin-top: 0;
        }
        h2 {
            color: #3498db;
            margin-top: 30px;
        }
        .card {
            border: 1px solid #ddd;
            border-radius: 4px;
            padding: 15px;
            margin-bottom: 15px;
            transition: transform 0.2s;
        }
        .card:hover {
            transform: translateY(-2px);
            box-shadow: 0 4px 8px rgba(0, 0, 0, 0.1);
        }
        .form-group {
            margin-bottom: 15px;
        }
        label {
            display: block;
            margin-bottom: 5px;
            font-weight: bold;
        }
        input[type="text"],
        input[type="email"] {
            width: 100%;
            padding: 8px;
            border: 1px solid #ddd;
            border-radius: 4px;
            box-sizing: border-box;
        }
        button {
            background-color: #3498db;
            color: white;
            border: none;
            padding: 10px 15px;
            border-radius: 4px;
            cursor: pointer;
            transition: background-color 0.2s;
        }
        button:hover {
            background-color: #2980b9;
        }
        .user-id {
            font-weight: bold;
            color: #7f8c8d;
        }
        .error {
            color: #e74c3c;
            margin-top: 10px;
        }
        .success {
            color: #27ae60;
            margin-top: 10px;
        }
        #loading {
            text-align: center;
            margin: 20px 0;
            display: none;
        }
        .hidden {
            display: none;
        }
      </style>
      </head>
      <body>
      <div class="container">
        <h1>Glaze REST API Demo</h1>
        <p>A simple demonstration of the Glaze REST API functionality.</p>
        
        <h2>All Users</h2>
        <div id="usersList"></div>
        <div id="loading">Loading...</div>
        
        <h2>Get User by ID</h2>
        <div class="form-group">
            <label for="userId">User ID:</label>
            <input type="text" id="userId" placeholder="Enter user ID">
        </div>
        <button id="getUser">Get User</button>
        <div id="userResult" class="hidden card"></div>
        <div id="userError" class="error hidden"></div>
        
        <h2>Add New User</h2>
        <div class="form-group">
            <label for="userName">Name:</label>
            <input type="text" id="userName" placeholder="Enter name">
        </div>
        <div class="form-group">
            <label for="userEmail">Email:</label>
            <input type="email" id="userEmail" placeholder="Enter email">
        </div>
        <button id="addUser">Add User</button>
        <div id="addSuccess" class="success hidden">User added successfully!</div>
        <div id="addError" class="error hidden"></div>
      </div>
      
      <script>
        // Fetch all users
        async function fetchUsers() {
            document.getElementById('loading').style.display = 'block';
            document.getElementById('usersList').innerHTML = '';
            
            try {
                const response = await fetch('/api/getAllUsers');
                const users = await response.json();
                
                document.getElementById('loading').style.display = 'none';
                
                if (users.length === 0) {
                    document.getElementById('usersList').innerHTML = '<p>No users found</p>';
                    return;
                }
                
                users.forEach(user => {
                    const userCard = document.createElement('div');
                    userCard.className = 'card';
                    userCard.innerHTML = `
                        <p><span class="user-id">ID: ${user.id}</span></p>
                        <p><strong>Name:</strong> ${user.name}</p>
                        <p><strong>Email:</strong> ${user.email}</p>
                    `;
                    document.getElementById('usersList').appendChild(userCard);
                });
            } catch (error) {
                document.getElementById('loading').style.display = 'none';
                document.getElementById('usersList').innerHTML = `<p class="error">Error loading users: ${error.message}</p>`;
            }
        }
        
        // Get user by ID
        document.getElementById('getUser').addEventListener('click', async () => {
            const userId = document.getElementById('userId').value;
            if (!userId) {
                document.getElementById('userError').textContent = 'Please enter a user ID';
                document.getElementById('userError').classList.remove('hidden');
                document.getElementById('userResult').classList.add('hidden');
                return;
            }
            
            try {
                // Using POST to getUserById with a JSON body
                const response = await fetch('/api/getUserById', {
                    method: 'POST',
                    headers: {
                        'Content-Type': 'application/json'
                    },
                    body: JSON.stringify({ id: parseInt(userId) })
                });
                
                const data = await response.json();
                
                // Check if we got a valid user (has an id)
                if (!data.id) {
                    document.getElementById('userError').textContent = 'User not found';
                    document.getElementById('userError').classList.remove('hidden');
                    document.getElementById('userResult').classList.add('hidden');
                    return;
                }
                
                document.getElementById('userError').classList.add('hidden');
                document.getElementById('userResult').classList.remove('hidden');
                document.getElementById('userResult').innerHTML = `
                    <p><span class="user-id">ID: ${data.id}</span></p>
                    <p><strong>Name:</strong> ${data.name}</p>
                    <p><strong>Email:</strong> ${data.email}</p>
                `;
            } catch (error) {
                document.getElementById('userError').textContent = `Error: ${error.message}`;
                document.getElementById('userError').classList.remove('hidden');
                document.getElementById('userResult').classList.add('hidden');
            }
        });
        
        // Add new user
        document.getElementById('addUser').addEventListener('click', async () => {
            const name = document.getElementById('userName').value;
            const email = document.getElementById('userEmail').value;
            
            if (!name || !email) {
                document.getElementById('addError').textContent = 'Please fill in all fields';
                document.getElementById('addError').classList.remove('hidden');
                document.getElementById('addSuccess').classList.add('hidden');
                return;
            }
            
            try {
                // Using createUser endpoint
                const response = await fetch('/api/createUser', {
                    method: 'POST',
                    headers: {
                        'Content-Type': 'application/json'
                    },
                    body: JSON.stringify({ name, email })
                });
                
                const data = await response.json();
                
                document.getElementById('addError').classList.add('hidden');
                document.getElementById('addSuccess').classList.remove('hidden');
                document.getElementById('userName').value = '';
                document.getElementById('userEmail').value = '';
                
                // Refresh the users list
                fetchUsers();
            } catch (error) {
                document.getElementById('addError').textContent = `Error: ${error.message}`;
                document.getElementById('addError').classList.remove('hidden');
                document.getElementById('addSuccess').classList.add('hidden');
            }
        });
        
        // Initialize
        document.addEventListener('DOMContentLoaded', () => {
            fetchUsers();
        });
      </script>
      </body>
      </html>
      )";

      res.content_type("text/html").body(html);
   });

   // Start the server
   server.bind("127.0.0.1", 8080).with_signals(); // Enable signal handling for graceful shutdown

   std::cout << "Server listening on http://127.0.0.1:8080\n";
   std::cout << "Press Ctrl+C to gracefully shut down the server\n";

   server.start();

   // Wait for shutdown signal (blocks until server stops)
   server.wait_for_signal();

   std::cout << "Server shut down successfully\n";
   return 0;
}
