// Glaze Library
// For the license information refer to glaze.hpp

// Complete example showing Glaze HTTP server with WebSocket support

#include <iostream>
#include <mutex>
#include <set>

#include "glaze/net/http_server.hpp"
#include "glaze/net/websocket_connection.hpp"

using namespace glz;

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

int main()
{
   std::cout << "Starting Glaze HTTP + WebSocket Server\n";
   std::cout << "=====================================\n";

   // Show which SHA-1 implementation is being used
#if defined(GLZ_ENABLE_OPENSSL) && defined(GLZ_HAS_OPENSSL)
   std::cout << "✅ Using OpenSSL for WebSocket handshake\n";
#else
   std::cout << "⚠️ Using fallback SHA-1 implementation\n";
#endif

   // Create HTTP server
   http_server server;

   // Create WebSocket server
   auto ws_server = std::make_shared<websocket_server>();

   // Thread-safe storage for connected clients
   std::set<std::shared_ptr<websocket_connection>> clients;
   std::mutex clients_mutex;

   // WebSocket event handlers
   ws_server->on_validate([](const request& req) -> bool {
      // Optional: Add authentication/validation logic here
      std::cout << "📋 Validating WebSocket connection from: " << req.remote_ip << std::endl;
      return true; // Accept all connections for this example
   });

   ws_server->on_open([&clients, &clients_mutex](std::shared_ptr<websocket_connection> conn, const request&) {
      std::lock_guard<std::mutex> lock(clients_mutex);
      clients.insert(conn);

      std::cout << "🔗 WebSocket opened: " << conn->remote_address() << " (Total clients: " << clients.size() << ")"
                << std::endl;

      // Send welcome message
      conn->send_text("Welcome! You are connected to the Glaze WebSocket server.");

      // Notify other clients about the new connection
      std::string join_msg = "User from " + conn->remote_address() + " joined the chat";
      for (auto& client : clients) {
         if (client != conn) {
            client->send_text("📢 " + join_msg);
         }
      }
   });

   ws_server->on_message([&clients, &clients_mutex](std::shared_ptr<websocket_connection> conn,
                                                    std::string_view message, ws_opcode opcode) {
      std::cout << "💬 Message from " << conn->remote_address() << ": " << message << std::endl;

      if (opcode == ws_opcode::text) {
         std::string msg(message);

         // Handle special commands
         if (msg == "/ping") {
            conn->send_ping("server-ping");
            conn->send_text("🏓 Ping sent!");
            return;
         }

         if (msg == "/clients") {
            std::lock_guard<std::mutex> lock(clients_mutex);
            conn->send_text("👥 Connected clients: " + std::to_string(clients.size()));
            return;
         }

         if (msg.starts_with("/echo ")) {
            std::string echo_msg = msg.substr(6);
            conn->send_text("🔄 Echo: " + echo_msg);
            return;
         }

         // Broadcast message to all clients
         std::lock_guard<std::mutex> lock(clients_mutex);
         std::string broadcast_msg = "[" + conn->remote_address() + "]: " + msg;
         for (auto& client : clients) {
            client->send_text(broadcast_msg);
         }
      }
      else if (opcode == ws_opcode::binary) {
         std::cout << "📦 Binary message received (" << message.size() << " bytes)" << std::endl;
         conn->send_binary("Binary echo: " + std::string(message));
      }
   });

   ws_server->on_close([&clients, &clients_mutex](std::shared_ptr<websocket_connection> conn) {
      std::lock_guard<std::mutex> lock(clients_mutex);
      clients.erase(conn);

      std::cout << "❌ WebSocket closed: " << conn->remote_address() << " (Remaining clients: " << clients.size() << ")"
                << std::endl;

      // Notify remaining clients
      std::string leave_msg = "User from " + conn->remote_address() + " left the chat";
      for (auto& client : clients) {
         client->send_text("📢 " + leave_msg);
      }
   });

   ws_server->on_error([](std::shared_ptr<websocket_connection> conn, std::error_code ec) {
      std::cout << "🚨 WebSocket error for " << conn->remote_address() << ": " << ec.message() << std::endl;
   });

   // Register WebSocket endpoint
   server.websocket("/ws", ws_server);

   // HTTP Routes
   server.get("/", [](const request&, response& res) { res.content_type("text/html").body(read_file("index.html")); });

   // API endpoints
   server.get("/api/status", [&clients, &clients_mutex](const request&, response& res) {
      std::lock_guard<std::mutex> lock(clients_mutex);
      res.json({{"server", "Glaze WebSocket + HTTP Server"},
                {"websocket_clients", clients.size()},
                {"implementation",
#if defined(GLZ_ENABLE_OPENSSL) && defined(GLZ_HAS_OPENSSL)
                 "OpenSSL"
#else
            "fallback_sha1"
#endif
                },
                {"status", "running"}});
   });

   server.get("/api/broadcast", [&clients, &clients_mutex](const request& req, response& res) {
      auto it = req.params.find("message");
      if (it == req.params.end()) {
         res.status(400).json({{"error", "Missing message parameter"}});
         return;
      }

      std::lock_guard<std::mutex> lock(clients_mutex);
      std::string broadcast_msg = "📢 Server broadcast: " + it->second;

      for (auto& client : clients) {
         client->send_text(broadcast_msg);
      }

      res.json({{"message", "Broadcast sent"}, {"recipients", clients.size()}});
   });

   // Enable CORS for browser access
   server.enable_cors();

   try {
      // Start server
      server.bind(8080).with_signals(); // Enable signal handling for graceful shutdown

      std::cout << "\nServer running on http://localhost:8080\n";
      std::cout << "WebSocket endpoint: ws://localhost:8080/ws\n";
      std::cout << "Web interface: http://localhost:8080\n";
      std::cout << "Status API: http://localhost:8080/api/status\n";
      std::cout << "\nPress Ctrl+C to gracefully shut down the server\n\n";

      server.start();

      // Wait for shutdown signal (blocks until server stops)
      server.wait_for_signal();

      std::cout << "👋 Server stopped gracefully\n";
   }
   catch (const std::exception& e) {
      std::cerr << "❌ Server error: " << e.what() << std::endl;
      return 1;
   }

   return 0;
}
