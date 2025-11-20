#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

#include "glaze/glaze.hpp"
#include "glaze/net/http_server.hpp"
#include "glaze/net/websocket_client.hpp"
#include "glaze/net/websocket_connection.hpp"
#include "ut/ut.hpp"

using namespace ut;
using namespace glz;

template <typename Predicate>
bool wait_for_condition(Predicate pred, std::chrono::milliseconds timeout = std::chrono::milliseconds(5000))
{
   auto start = std::chrono::steady_clock::now();
   while (!pred()) {
      if (std::chrono::steady_clock::now() - start > timeout) {
         return false;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
   }
   return true;
}

void run_server_thread(std::atomic<bool>& server_ready, std::atomic<bool>& should_stop, uint16_t port)
{
   http_server server;
   auto ws_server = std::make_shared<websocket_server<asio::ip::tcp::socket>>();

   ws_server->on_open([](auto /*conn*/, const request&) {});

   ws_server->on_message([](auto conn, std::string_view message, ws_opcode opcode) {
      if (opcode == ws_opcode::text) {
         conn->send_text(std::string("Echo: ") + std::string(message));
      }
   });

   ws_server->on_close([](auto /*conn*/) {});

   ws_server->on_error(
      [](auto /*conn*/, std::error_code ec) { std::cerr << "Server Error: " << ec.message() << "\n"; });

   server.websocket("/ws", ws_server);

   try {
      server.bind(port);
      server.start();
      server_ready = true;

      while (!should_stop) {
         std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
      server.stop();
   }
   catch (const std::exception& e) {
      std::cerr << "Server Exception: " << e.what() << "\n";
      server_ready = true; // Unblock main thread if failed
   }
}

suite websocket_client_tests = [] {
   "echo_test"_test = [] {
      uint16_t port = 8091; // Use a different port to avoid conflicts
      std::atomic<bool> server_ready{false};
      std::atomic<bool> stop_server{false};

      // Start server in a thread
      std::thread server_thread(run_server_thread, std::ref(server_ready), std::ref(stop_server), port);

      // Wait for server to signal it has started
      expect(wait_for_condition([&] { return server_ready.load(); })) << "Server failed to start";

      // Client Logic
      websocket_client client;
      std::atomic<bool> message_received{false};
      std::atomic<bool> connection_closed{false};

      client.on_open([&client]() { client.send("Hello Glaze!"); });

      client.on_message([&](std::string_view message, ws_opcode) {
         if (message == "Echo: Hello Glaze!") {
            message_received = true;
            client.close();
         }
      });

      client.on_error([&](std::error_code ec) { std::cerr << "Client Error: " << ec.message() << "\n"; });

      client.on_close([&](ws_close_code, std::string_view) {
         connection_closed = true;
         client.ctx_->stop(); // Stop the client's io_context
      });

      std::string client_url = "ws://localhost:" + std::to_string(port) + "/ws";
      client.connect(client_url);

      // Run client io_context in a separate thread
      std::thread client_thread([&client]() { client.ctx_->run(); });

      // Wait for verification
      expect(wait_for_condition([&] { return message_received.load(); })) << "Message was not received/echoed";
      expect(wait_for_condition([&] { return connection_closed.load(); })) << "Connection was not closed gracefully";

      // Stop client thread if still running
      if (!client.ctx_->stopped()) {
         client.ctx_->stop();
      }

      if (client_thread.joinable()) {
         client_thread.join();
      }

      stop_server = true;
      server_thread.join();
   };
};

int main() {}
