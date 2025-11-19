#include <iostream>
#include <thread>
#include <future>
#include "glaze/net/http_server.hpp"
#include "glaze/net/websocket_connection.hpp"
#include "glaze/net/websocket_client.hpp"
#include "glaze/glaze.hpp"

using namespace glz;

void run_server_thread(std::promise<void>& started_promise, std::atomic<bool>& should_stop, uint16_t port) {
    http_server server;
    auto ws_server = std::make_shared<websocket_server<asio::ip::tcp::socket>>();

    ws_server->on_open([](auto /*conn*/, const request&) {
       std::cout << "[Server] Client connected\n";
    });

    ws_server->on_message([](auto conn, std::string_view message, ws_opcode opcode) {
       std::cout << "[Server] Received: " << message << "\n";
       if (opcode == ws_opcode::text) {
          conn->send_text(std::string("Echo: ") + std::string(message));
          std::cout << "[Server] Sent echo: Echo: " << message << "\n";
       }
    });
    
    ws_server->on_close([](auto /*conn*/) {
        std::cout << "[Server] Client disconnected\n";
    });
    
    ws_server->on_error([](auto /*conn*/, std::error_code ec) {
        std::cerr << "[Server] Error: " << ec.message() << "\n";
    });

    server.websocket("/ws", ws_server);
    
    try {
        server.bind(port);
        server.start(); 
        started_promise.set_value();
        std::cout << "[Server] Started on port " << port << "\n";

        while (!should_stop) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        server.stop();
        std::cout << "[Server] Stopped\n";
    } catch (const std::exception& e) {
        std::cerr << "[Server] Exception: " << e.what() << "\n";
        // If an exception occurs before set_value, ensure the main thread doesn't hang
        // Check if promise state is still not set to avoid double set
        try {
            started_promise.set_value(); 
        } catch (const std::future_error&) {
            // Already set, ignore
        }
    }
}

int main() {
    std::cout << "Starting WebSocket Client Test\n";

    uint16_t port = 8090;
    std::promise<void> server_started_promise;
    auto server_future = server_started_promise.get_future();
    std::atomic<bool> stop_server{false};

    // Start server in a thread
    std::thread server_thread(run_server_thread, std::ref(server_started_promise), std::ref(stop_server), port);

    // Wait for server to signal it has started
    server_future.wait();
    std::cout << "[Test] Server signaled start.\n";

    // Client Logic
    websocket_client client;
    std::promise<void> message_received_promise;
    auto message_future = message_received_promise.get_future();
    bool success = false;

    client.on_open([&client]() {
        std::cout << "[Client] Connected\n";
        client.send("Hello Glaze!");
        std::cout << "[Client] Sent: Hello Glaze!\n";
    });

    client.on_message([&](std::string_view message, ws_opcode) {
        std::cout << "[Client] Received: " << message << "\n";
        if (message == "Echo: Hello Glaze!") {
            success = true;
            message_received_promise.set_value();
            std::cout << "[Client] Initiating close...\n";
            client.close(); // Initiate close after receiving expected message
        }
    });
    
    client.on_error([&](std::error_code ec) {
        std::cerr << "[Client] Error: " << ec.message() << "\n";
        // Unblock future on error, but only if not already set by success path
        try {
            message_received_promise.set_value(); 
        } catch (const std::future_error&) {
            // Already set, ignore
        }
    });
    
    client.on_close([&](ws_close_code code, std::string_view reason) {
        std::cout << "[Client] Closed with code " << static_cast<uint16_t>(code) << " and reason: " << reason << "\n";
        client.ctx_->stop(); // Stop the client's io_context
    });

    std::string client_url = "ws://localhost:" + std::to_string(port) + "/ws";
    client.connect(client_url);
    
    std::cout << "[Client] Connecting to " << client_url << "\n";

    // Run client io_context in a separate thread
    std::thread client_thread([&client]() {
        client.ctx_->run();
        std::cout << "[Client] io_context stopped.\n";
    });

    // Wait for verification or timeout
    if (message_future.wait_for(std::chrono::seconds(5)) == std::future_status::ready) {
        std::cout << "[Test] Message received and verified.\n";
    } else {
        std::cerr << "[Test] Timed Out or Message Not Verified!\n";
        success = false;
        client.ctx_->stop(); // Ensure client thread stops if we timed out
    }

    // Join client thread
    if (client_thread.joinable()) {
        client_thread.join();
        std::cout << "[Test] Client thread joined.\n";
    }

    // Signal server to stop and join its thread
    stop_server = true;
    server_thread.join();
    std::cout << "[Test] Server thread joined.\n";

    return success ? 0 : 1;
}
