#include <atomic>
#include <chrono>
#include <thread>

#include "glaze/net/http_server.hpp"
#include "glaze/net/websocket_client.hpp"
#include "ut/ut.hpp"

using namespace ut;
using namespace glz;

// Helper to run a basic echo server (copied from websocket_client_test.cpp)
void run_echo_server(std::atomic<bool>& server_ready, std::atomic<bool>& should_stop,
                     std::atomic<uint16_t>& selected_port)
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
      server.bind("127.0.0.1", 0);
      selected_port = server.port();
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
      std::atomic<bool> server_ready{false};
      std::atomic<bool> stop_server{false};
      std::atomic<uint16_t> port{0};

      std::thread server_thread(run_echo_server, std::ref(server_ready), std::ref(stop_server), std::ref(port));

      while (!server_ready) std::this_thread::yield();

      auto io_ctx = std::make_shared<asio::io_context>();
      auto work_guard = asio::make_work_guard(*io_ctx); // Keep run() alive

      std::thread io_thread([io_ctx]() { io_ctx->run(); });

      {
         // Scope for Client A
         auto client_a = std::make_unique<websocket_client>(io_ctx);
         std::atomic<bool> connected_a{false};
         client_a->on_open([&connected_a]() { connected_a = true; });
         client_a->connect("ws://127.0.0.1:" + std::to_string(port.load()) + "/ws");

         int retries = 0;
         while (!connected_a && retries++ < 500) std::this_thread::sleep_for(std::chrono::milliseconds(10));
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

      client_b->connect("ws://127.0.0.1:" + std::to_string(port.load()) + "/ws");

      int connect_retries = 0;
      while (!connected_b && connect_retries++ < 500) std::this_thread::sleep_for(std::chrono::milliseconds(10));

      int message_retries = 0;
      while (!msg_received_b && message_retries++ < 500) std::this_thread::sleep_for(std::chrono::milliseconds(10));

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
