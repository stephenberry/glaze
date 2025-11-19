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

    ws_server->on_open([](auto /*conn*/, const request&) {});

    ws_server->on_message([](auto conn, std::string_view message, ws_opcode opcode) {
       if (opcode == ws_opcode::text) {
          conn->send_text(std::string("Echo: ") + std::string(message));
       }
    });
    
    ws_server->on_close([](auto /*conn*/) {});
    
    ws_server->on_error([](auto /*conn*/, std::error_code ec) {
        std::cerr << "Server Error: " << ec.message() << "\n";
    });

    server.websocket("/ws", ws_server);
    
    try {
        server.bind(port);
        server.start(); 
        started_promise.set_value();

        while (!should_stop) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        server.stop();
    } catch (const std::exception& e) {
        std::cerr << "Server Exception: " << e.what() << "\n";
        try {
            started_promise.set_value(); 
        } catch (const std::future_error&) {}
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

    // Client Logic
    websocket_client client;
    std::promise<void> message_received_promise;
    auto message_future = message_received_promise.get_future();
    bool success = false;

    client.on_open([&client]() {
        client.send("Hello Glaze!");
    });

    client.on_message([&](std::string_view message, ws_opcode) {
        if (message == "Echo: Hello Glaze!") {
            success = true;
            message_received_promise.set_value();
            client.close();
        }
    });
    
    client.on_error([&](std::error_code ec) {
        std::cerr << "Client Error: " << ec.message() << "\n";
        try {
            message_received_promise.set_value(); 
        } catch (const std::future_error&) {}
    });
    
    client.on_close([&](ws_close_code, std::string_view) {
        client.ctx_->stop(); // Stop the client's io_context
    });

    std::string client_url = "ws://localhost:" + std::to_string(port) + "/ws";
    client.connect(client_url);
    
    // Run client io_context in a separate thread
    std::thread client_thread([&client]() {
        client.ctx_->run();
    });

    // Wait for verification or timeout
    if (message_future.wait_for(std::chrono::seconds(5)) == std::future_status::ready) {
        std::cout << "Test Passed!\n";
    } else {
        std::cerr << "Test Timed Out or Failed!\n";
        success = false;
        client.ctx_->stop();
    }

    if (client_thread.joinable()) {
        client_thread.join();
    }

    stop_server = true;
    server_thread.join();

    return success ? 0 : 1;
}
