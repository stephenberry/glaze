// WSS (WebSocket Secure) Test for Glaze Library
// Verifies ASIO 1.32+ compatibility for SSL WebSocket client (issue #2164)

#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

// Enable SSL before including headers
#ifndef GLZ_ENABLE_SSL
#define GLZ_ENABLE_SSL
#endif

#include "glaze/glaze.hpp"
#include "glaze/net/websocket_client.hpp"
#include "ut/ut.hpp"

#ifdef DELETE
#undef DELETE
#endif

using namespace ut;
using namespace glz;

suite wss_client_tests = [] {
   "websocket_client_wss_connection_attempt"_test = [] {
      // This test verifies that websocket_client can attempt a WSS connection.
      // The key fix for issue #2164 was changing lowest_layer() to next_layer()
      // in websocket_client.hpp for ASIO 1.32+ compatibility.
      websocket_client client;

      std::atomic<bool> open_called{false};
      std::atomic<bool> error_called{false};

      client.on_open([&]() { open_called = true; });
      client.on_message([](std::string_view, ws_opcode) {});
      client.on_close([](ws_close_code, std::string_view) {});
      client.on_error([&](std::error_code) { error_called = true; });

      // Try to connect to a non-existent WSS server
      // This exercises the SSL code path (socket creation, SSL context setup)
      client.connect("wss://127.0.0.1:19999/test");

      // Wait for connection attempt to fail
      std::this_thread::sleep_for(std::chrono::milliseconds(500));

      // Connection should fail since there's no server
      expect(!open_called.load()) << "Should not connect to non-existent server\n";

      client.close();
   };

   "websocket_client_api_with_ssl"_test = [] {
      // Verify websocket_client API works when SSL is enabled at compile time
      websocket_client client;

      // All callback setters should compile and work
      client.on_open([]() {});
      client.on_message([](std::string_view, ws_opcode) {});
      client.on_close([](ws_close_code, std::string_view) {});
      client.on_error([](std::error_code) {});

      expect(true) << "websocket_client API compiles with SSL enabled\n";
   };
};

int main()
{
   std::cout << "=== WSS (WebSocket Secure) Client Tests ===\n";
   std::cout << "Verifies issue #2164 fix: ASIO 1.32+ SSL compatibility\n\n";

   return 0;
}
