// Glaze Library
// For the license information refer to glaze.hpp

// Tests for WebSocket client lifetime safety (issue #2409)
//
// These tests verify that the websocket_client can be safely destroyed at any
// point during the connection lifecycle without causing crashes (access violations
// on Windows IOCP, segfaults on other platforms).
//
// The root cause of #2409: cancel_all() only called force_close() on the
// websocket_connection, but during the connection/handshake phase the connection
// doesn't exist yet (it's still monostate). The raw sockets (tcp_socket_,
// ssl_socket_) were destroyed while async operations (async_connect,
// async_handshake) were still pending, causing IOCP completions to reference
// freed memory.

#include "glaze/net/websocket_client.hpp"

#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>

#include "glaze/net/http_server.hpp"
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

// Helper to run a basic echo server
void run_echo_server(std::atomic<bool>& server_ready, std::atomic<bool>& should_stop, uint16_t port)
{
   http_server server;
   auto ws_server = std::make_shared<websocket_server>();

   ws_server->on_open([](auto, const request&) {});
   ws_server->on_message([](auto conn, std::string_view message, ws_opcode opcode) {
      if (opcode == ws_opcode::text) {
         std::string echo_msg = "Echo: ";
         echo_msg.append(message);
         conn->send_text(echo_msg);
      }
      else if (opcode == ws_opcode::binary) {
         conn->send_binary(message);
      }
   });
   ws_server->on_close([](auto, ws_close_code, std::string_view) {});
   ws_server->on_error([](auto, std::error_code) {});

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

// Helper to run a server that sends a message immediately on open
void run_immediate_send_server(std::atomic<bool>& server_ready, std::atomic<bool>& should_stop, uint16_t port)
{
   http_server server;
   auto ws_server = std::make_shared<websocket_server>();

   ws_server->on_open([](auto conn, const request&) {
      // Send a message immediately upon connection
      conn->send_text("welcome");
   });
   ws_server->on_message([](auto, std::string_view, ws_opcode) {});
   ws_server->on_close([](auto, ws_close_code, std::string_view) {});
   ws_server->on_error([](auto, std::error_code) {});

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

// ============================================================================
// Tests for issue #2409: safe destruction during connection lifecycle
// ============================================================================

suite websocket_client_lifetime_tests = [] {
   // -----------------------------------------------------------------------
   // Test: Destroying client while DNS resolve is in progress
   // Before the fix, the socket could be destroyed while async_connect or
   // async_resolve was pending, causing use-after-free on IOCP.
   // -----------------------------------------------------------------------
   "destroy_during_resolve"_test = [] {
      for (int i = 0; i < 10; ++i) {
         // Use a non-routable address so resolve might take time
         // but mainly this tests the cleanup path in cancel_all()
         websocket_client client;
         client.on_error([](std::error_code) {});
         client.on_close([](ws_close_code, std::string_view) {});

         client.connect("ws://localhost:19876/ws");

         // Immediately destroy without calling run() - the async_resolve
         // is queued but not yet started. cancel_all() must safely cancel
         // the resolver and clean up the socket.
      }
      // If we get here without crashing, the test passes.
      expect(true) << "Destroying client during resolve should not crash";
   };

   // -----------------------------------------------------------------------
   // Test: Destroying client during async_connect
   // Start connecting to a real server, then destroy the client while
   // async_connect may be in progress.
   // -----------------------------------------------------------------------
   "destroy_during_connect"_test = [] {
      uint16_t port = 18401;
      std::atomic<bool> server_ready{false};
      std::atomic<bool> stop_server{false};

      std::thread server_thread(run_echo_server, std::ref(server_ready), std::ref(stop_server), port);
      expect(wait_for_condition([&] { return server_ready.load(); })) << "Server failed to start";

      for (int i = 0; i < 20; ++i) {
         websocket_client client;
         client.on_error([](std::error_code) {});
         client.on_close([](ws_close_code, std::string_view) {});

         std::string url = "ws://localhost:" + std::to_string(port) + "/ws";
         client.connect(url);

         // Run the io_context very briefly to potentially get into the
         // connect phase, then destroy the client.
         client.context()->run_for(std::chrono::milliseconds(1));

         // Client is destroyed here. cancel_all() must properly cancel
         // the pending async_connect and drain IOCP completions.
      }

      stop_server = true;
      server_thread.join();

      expect(true) << "Destroying client during connect should not crash";
   };

   // -----------------------------------------------------------------------
   // Test: Destroying client during HTTP handshake
   // Let the TCP connection succeed but destroy during the WebSocket
   // HTTP upgrade handshake.
   // -----------------------------------------------------------------------
   "destroy_during_handshake"_test = [] {
      uint16_t port = 18402;
      std::atomic<bool> server_ready{false};
      std::atomic<bool> stop_server{false};

      std::thread server_thread(run_echo_server, std::ref(server_ready), std::ref(stop_server), port);
      expect(wait_for_condition([&] { return server_ready.load(); })) << "Server failed to start";

      for (int i = 0; i < 20; ++i) {
         websocket_client client;
         client.on_error([](std::error_code) {});
         client.on_close([](ws_close_code, std::string_view) {});

         std::string url = "ws://localhost:" + std::to_string(port) + "/ws";
         client.connect(url);

         // Run long enough for the TCP connect to succeed but potentially
         // not long enough for the full HTTP handshake.
         client.context()->run_for(std::chrono::milliseconds(5));

         // Client is destroyed here during handshake.
      }

      stop_server = true;
      server_thread.join();

      expect(true) << "Destroying client during handshake should not crash";
   };

   // -----------------------------------------------------------------------
   // Test: Destroying client during active WebSocket communication
   // Establish a full connection, start reading, then destroy.
   // -----------------------------------------------------------------------
   "destroy_during_active_connection"_test = [] {
      uint16_t port = 18403;
      std::atomic<bool> server_ready{false};
      std::atomic<bool> stop_server{false};

      std::thread server_thread(run_echo_server, std::ref(server_ready), std::ref(stop_server), port);
      expect(wait_for_condition([&] { return server_ready.load(); })) << "Server failed to start";

      for (int i = 0; i < 10; ++i) {
         std::atomic<bool> connected{false};

         websocket_client client;
         client.on_open([&connected]() { connected = true; });
         client.on_message([](std::string_view, ws_opcode) {});
         client.on_error([](std::error_code) {});
         client.on_close([](ws_close_code, std::string_view) {});

         std::string url = "ws://localhost:" + std::to_string(port) + "/ws";
         client.connect(url);

         // Run until connected or timeout
         std::thread client_thread([&client]() { client.context()->run(); });

         // Wait a bit for the connection to establish
         wait_for_condition([&] { return connected.load(); }, std::chrono::milliseconds(2000));

         // Stop and destroy while the connection is active (async_read_some pending)
         client.context()->stop();
         client_thread.join();

         // Client destructor runs here with an active connection
      }

      stop_server = true;
      server_thread.join();

      expect(true) << "Destroying client during active connection should not crash";
   };

   // -----------------------------------------------------------------------
   // Test: Rapid connect/destroy cycles
   // Stress test: rapidly create, connect, and destroy clients.
   // This maximizes the chance of hitting IOCP timing windows.
   // -----------------------------------------------------------------------
   "rapid_connect_destroy_cycles"_test = [] {
      uint16_t port = 18404;
      std::atomic<bool> server_ready{false};
      std::atomic<bool> stop_server{false};

      std::thread server_thread(run_echo_server, std::ref(server_ready), std::ref(stop_server), port);
      expect(wait_for_condition([&] { return server_ready.load(); })) << "Server failed to start";

      // Do many rapid connect/destroy cycles
      for (int i = 0; i < 50; ++i) {
         auto client = std::make_unique<websocket_client>();
         client->on_error([](std::error_code) {});
         client->on_close([](ws_close_code, std::string_view) {});

         std::string url = "ws://localhost:" + std::to_string(port) + "/ws";
         client->connect(url);

         // Vary the timing to hit different phases of the connection
         if (i % 3 == 0) {
            // Destroy immediately (during resolve)
            client.reset();
         }
         else if (i % 3 == 1) {
            // Run briefly then destroy (during connect/handshake)
            client->context()->run_for(std::chrono::milliseconds(2));
            client.reset();
         }
         else {
            // Run longer then destroy (during handshake or active read)
            client->context()->run_for(std::chrono::milliseconds(10));
            client.reset();
         }
      }

      stop_server = true;
      server_thread.join();

      expect(true) << "Rapid connect/destroy cycles should not crash";
   };

   // -----------------------------------------------------------------------
   // Test: Destroying client that failed to connect
   // Verify clean shutdown when connection was refused.
   // -----------------------------------------------------------------------
   "destroy_after_connection_failure"_test = [] {
      for (int i = 0; i < 10; ++i) {
         websocket_client client;
         std::atomic<bool> error_received{false};

         client.on_error([&](std::error_code) { error_received = true; });
         client.on_close([](ws_close_code, std::string_view) {});

         // Connect to a port with no server
         client.connect("ws://localhost:19877/ws");

         // Run until the error is received or timeout
         std::thread client_thread([&client]() { client.context()->run(); });

         wait_for_condition([&] { return error_received.load(); }, std::chrono::milliseconds(3000));

         if (!client.context()->stopped()) {
            client.context()->stop();
         }
         client_thread.join();

         // Client destructor runs here - must clean up properly
      }

      expect(true) << "Destroying client after connection failure should not crash";
   };

   // -----------------------------------------------------------------------
   // Test: Multiple clients sharing context, destroyed independently
   // -----------------------------------------------------------------------
   "shared_context_destroy_order"_test = [] {
      uint16_t port = 18405;
      std::atomic<bool> server_ready{false};
      std::atomic<bool> stop_server{false};

      std::thread server_thread(run_echo_server, std::ref(server_ready), std::ref(stop_server), port);
      expect(wait_for_condition([&] { return server_ready.load(); })) << "Server failed to start";

      auto shared_ctx = std::make_shared<asio::io_context>();

      {
         websocket_client client1(shared_ctx);
         websocket_client client2(shared_ctx);

         client1.on_error([](std::error_code) {});
         client1.on_close([](ws_close_code, std::string_view) {});
         client2.on_error([](std::error_code) {});
         client2.on_close([](ws_close_code, std::string_view) {});

         std::string url = "ws://localhost:" + std::to_string(port) + "/ws";
         client1.connect(url);
         client2.connect(url);

         shared_ctx->run_for(std::chrono::milliseconds(10));

         // client2 destroyed first, then client1 - both must clean up safely
      }

      // shared_ctx is still alive here
      expect(true) << "Shared context clients destroyed in order should not crash";

      stop_server = true;
      server_thread.join();
   };
};

// ============================================================================
// Test for initial data handler ordering fix
// ============================================================================

suite websocket_initial_data_tests = [] {
   // -----------------------------------------------------------------------
   // Test: Server sends message immediately on open
   // Before the fix, handlers were set AFTER set_initial_data(), so if the
   // server sent data that arrived in the same TCP segment as the HTTP
   // upgrade response, the on_message handler would miss it.
   // -----------------------------------------------------------------------
   "immediate_server_message_received"_test = [] {
      uint16_t port = 18406;
      std::atomic<bool> server_ready{false};
      std::atomic<bool> stop_server{false};

      std::thread server_thread(run_immediate_send_server, std::ref(server_ready), std::ref(stop_server), port);
      expect(wait_for_condition([&] { return server_ready.load(); })) << "Server failed to start";

      websocket_client client;
      std::atomic<bool> message_received{false};
      std::string received_msg;
      std::mutex msg_mutex;

      client.on_open([]() {});

      client.on_message([&](std::string_view message, ws_opcode) {
         std::lock_guard<std::mutex> lock(msg_mutex);
         received_msg = std::string(message);
         message_received = true;
      });

      client.on_error([](std::error_code ec) {
         std::cerr << "Client error: " << ec.message() << "\n";
      });

      client.on_close([&](ws_close_code, std::string_view) { client.context()->stop(); });

      std::string url = "ws://localhost:" + std::to_string(port) + "/ws";
      client.connect(url);

      std::thread client_thread([&client]() { client.context()->run(); });

      bool got_message = wait_for_condition([&] { return message_received.load(); }, std::chrono::milliseconds(3000));

      // Close and clean up
      if (got_message) {
         client.close();
      }

      if (!client.context()->stopped()) {
         client.context()->stop();
      }
      client_thread.join();

      expect(got_message) << "Should receive message sent immediately by server";

      {
         std::lock_guard<std::mutex> lock(msg_mutex);
         expect(received_msg == "welcome") << "Message content should be 'welcome'";
      }

      stop_server = true;
      server_thread.join();
   };
};

int main() {}
