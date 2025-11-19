#include <iostream>
#include <thread>
#include <future>
#include "glaze/net/http_server.hpp"
#include "glaze/net/websocket_connection.hpp"
#include "glaze/net/websocket_client.hpp"
#include "glaze/glaze.hpp"

using namespace glz;

void run_server(std::promise<void>& started_promise, std::atomic<bool>& should_stop, uint16_t port) {
   http_server server;
   auto ws_server = std::make_shared<websocket_server>();

   ws_server->on_open([](std::shared_ptr<websocket_connection> conn, const request&) {
      std::cout << "Server: Client connected\n";
   });

   ws_server->on_message([](std::shared_ptr<websocket_connection> conn, std::string_view message, ws_opcode opcode) {
      if (opcode == ws_opcode::text) {
         std::cout << "Server received: " << message << "\n";
         conn->send_text(std::string("Echo: ") + std::string(message));
      }
   });

   server.websocket("/ws", ws_server);
   
   try {
      server.bind(port);
      server.start(); // This runs in a separate thread managed by http_server usually? 
      // Wait, http_server.start() usually spawns threads or runs.
      // Looking at http_server.hpp: run() blocks, start() just sets up?
      // No, http_server::run() blocks.
      // But http_server::start() in websocket_server.cpp calls server.start() then wait_for_signal().
      
      started_promise.set_value();
      
      while (!should_stop) {
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
      server.stop();
   } catch (const std::exception& e) {
       std::cerr << "Server error: " << e.what() << "\n";
       // If we failed to start, set value to unblock main thread (it will likely crash or fail connection)
       // started_promise.set_value(); 
   }
}

int main() {
    std::cout << "Starting WebSocket Client Test\n";

    uint16_t port = 8090;
    std::promise<void> server_started;
    auto server_future = server_started.get_future();
    std::atomic<bool> stop_server{false};

    // Start server in a thread
    std::thread server_thread([&]() {
        http_server server;
        auto ws_server = std::make_shared<websocket_server>();

        ws_server->on_open([](std::shared_ptr<websocket_connection> conn, const request&) {
           // std::cout << "Server: Client connected\n";
        });

        ws_server->on_message([](std::shared_ptr<websocket_connection> conn, std::string_view message, ws_opcode) {
           // std::cout << "Server received: " << message << "\n";
           conn->send_text(std::string("Echo: ") + std::string(message));
        });

        server.websocket("/ws", ws_server);
        server.bind(port);
        
        // http_server.start() is non-blocking?
        // Let's check http_server implementation.
        // asio_server::run() blocks if run_on_main_thread is true.
        // http_server uses asio_server internally.
        // The `websocket_server.cpp` example calls server.start() then server.wait_for_signal().
        // This suggests start() is non-blocking (runs in background threads).
        
        server.start(); 
        server_started.set_value();

        while (!stop_server) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        server.stop();
    });

    server_future.wait();
    std::cout << "Server started on port " << port << "\n";

    // Client Logic
    websocket_client client;
    std::promise<void> message_received;
    auto message_future = message_received.get_future();
    bool success = false;

    client.on_open([&client]() {
        std::cout << "Client: Connected\n";
        client.send("Hello Glaze!");
    });

    client.on_message([&](std::string_view message, ws_opcode) {
        std::cout << "Client received: " << message << "\n";
        if (message == "Echo: Hello Glaze!") {
            success = true;
            message_received.set_value();
            client.close();
        }
    });
    
    client.on_error([](std::error_code ec) {
        std::cerr << "Client Error: " << ec.message() << "\n";
    });
    
    client.on_close([&](ws_close_code, std::string_view) {
        std::cout << "Client: Closed\n";
        client.ctx_->stop(); // Stop the client loop
    });

    client.connect("ws://localhost:" + std::to_string(port) + "/ws");
    
    // Run client loop
    client.run();

    // Wait for verification
    if (message_future.wait_for(std::chrono::seconds(2)) == std::future_status::ready) {
        std::cout << "Test Passed!\n";
    } else {
        std::cerr << "Test Timed Out or Failed!\n";
        success = false;
    }

    stop_server = true;
    server_thread.join();

    return success ? 0 : 1;
}
