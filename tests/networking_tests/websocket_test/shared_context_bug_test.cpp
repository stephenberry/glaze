#include <atomic>
#include <thread>

#include "glaze/net/http_server.hpp"
#include "glaze/net/websocket_client.hpp"
#include "ut/ut.hpp"

using namespace ut;
using namespace glz;

// Helper to run a basic echo server (copied from websocket_client_test.cpp)
void run_echo_server(std::atomic<bool>& server_ready, std::atomic<bool>& should_stop, uint16_t port)
{
   http_server server;
   auto ws_server = std::make_shared<websocket_server>();

   ws_server->on_open([](auto /*conn*/, const request&) {});
   ws_server->on_message([](auto conn, std::string_view message, ws_opcode opcode) {
      if (opcode == ws_opcode::text) {
         conn->send_text(std::string("Echo: ") + std::string(message));
      }
   });
   ws_server->on_close([](auto /*conn*/, ws_close_code /*code*/, std::string_view /*reason*/) {});
   ws_server->on_error([](auto /*conn*/, std::error_code /*ec*/) {});

   server.websocket("/ws", ws_server);

   try {
      server.bind(port);
      server.start();
      server_ready = true;

      while (!should_stop) {
         std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }
      server.stop();
   }
   catch (const std::exception& e) {
      std::cerr << "Server Exception: " << e.what() << "\n";
      server_ready = true;
   }
}

suite shared_context_test = [] {
   "shared_context_survival_test"_test = [] {
      uint16_t port = 8120;
      std::atomic<bool> server_ready{false};
      std::atomic<bool> stop_server{false};

      std::thread server_thread(run_echo_server, std::ref(server_ready), std::ref(stop_server), port);

      while (!server_ready) std::this_thread::yield();

      auto io_ctx = std::make_shared<asio::io_context>();
      auto work_guard = asio::make_work_guard(*io_ctx); // Keep run() alive

      std::thread io_thread([io_ctx]() { io_ctx->run(); });

      {
         // Scope for Client A
         auto client_a = std::make_unique<websocket_client>(io_ctx);
         std::atomic<bool> connected_a{false};
         client_a->on_open([&connected_a]() { connected_a = true; });
         client_a->connect("ws://localhost:" + std::to_string(port) + "/ws");

         int retries = 0;
         while (!connected_a && retries++ < 100) std::this_thread::sleep_for(std::chrono::milliseconds(10));
         expect(connected_a.load()) << "Client A failed to connect";

         // Client A goes out of scope here, its destructor runs
      }

      // Client B should still work
      auto client_b = std::make_unique<websocket_client>(io_ctx);
      std::atomic<bool> connected_b{false};
      std::atomic<bool> msg_received_b{false};

      client_b->on_open([&]() {
         connected_b = true;
         client_b->send("Test");
      });

      client_b->on_message([&](std::string_view msg, ws_opcode) {
         if (msg == "Echo: Test") msg_received_b = true;
      });

      client_b->connect("ws://localhost:" + std::to_string(port) + "/ws");

      // Give it some time
      std::this_thread::sleep_for(std::chrono::milliseconds(500));

      // If io_ctx was stopped by Client A, Client B will not connect or receive messages
      bool ctx_stopped = io_ctx->stopped();

      expect(!ctx_stopped) << "IO Context was stopped by Client A destructor!";
      expect(connected_b.load()) << "Client B failed to connect (Context stopped?)";
      expect(msg_received_b.load()) << "Client B failed to receive message";

      work_guard.reset();
      io_ctx->stop();
      if (io_thread.joinable()) io_thread.join();

      stop_server = true;
      server_thread.join();
   };
};

int main() {}
