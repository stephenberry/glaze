// Glaze Library
// For the license information refer to glaze.hpp

// Unit tests for WebSocket close frame and error handling

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

#include "glaze/net/http_server.hpp"
#include "glaze/net/websocket_connection.hpp"
#include "ut/ut.hpp"

using namespace ut;
using namespace glz;

// Helper to wait for a condition with timeout
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

suite websocket_close_frame_tests = [] {
   "close_frame_is_sent"_test = [] {
      std::atomic<bool> close_frame_received{false};
      std::atomic<bool> on_close_called{false};
      std::atomic<bool> server_ready{false};

      // Create WebSocket server
      auto ws_server = std::make_shared<websocket_server>();

      ws_server->on_open([](std::shared_ptr<websocket_connection> conn, const request&) {
         // Server initiates close - this should send a close frame
         conn->close(ws_close_code::normal, "Test close");
      });

      ws_server->on_close([&](std::shared_ptr<websocket_connection>) { on_close_called = true; });

      // Create HTTP server
      http_server server;
      server.websocket("/ws", ws_server);

      std::thread server_thread([&]() {
         server.bind(18081);
         server_ready = true;
         server.start();
      });

      // Wait for server to be ready
      expect(wait_for_condition([&] { return server_ready.load(); })) << "Server should start";
      std::this_thread::sleep_for(std::chrono::milliseconds(100));

      // Create a raw TCP client to verify close frame is actually sent
      asio::io_context io_context;
      asio::ip::tcp::socket socket(io_context);
      asio::ip::tcp::resolver resolver(io_context);
      auto endpoints = resolver.resolve("127.0.0.1", "18081");

      asio::connect(socket, endpoints);

      // Send WebSocket handshake
      std::string handshake = "GET /ws HTTP/1.1\r\n"
                              "Host: localhost:18081\r\n"
                              "Upgrade: websocket\r\n"
                              "Connection: Upgrade\r\n"
                              "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                              "Sec-WebSocket-Version: 13\r\n\r\n";

      asio::write(socket, asio::buffer(handshake));

      // Read handshake response
      std::array<char, 1024> response_buffer;
      size_t response_len = socket.read_some(asio::buffer(response_buffer));

      // Verify handshake succeeded
      std::string response(response_buffer.data(), response_len);
      expect(response.find("101 Switching Protocols") != std::string::npos) << "Handshake should succeed";

      // Give the server time to send the close frame
      std::this_thread::sleep_for(std::chrono::milliseconds(500));

      // Read WebSocket frames - should receive close frame
      std::array<uint8_t, 1024> frame_buffer;
      std::error_code ec;
      size_t frame_len = socket.read_some(asio::buffer(frame_buffer), ec);

      if (frame_len >= 2) {
         uint8_t opcode = frame_buffer[0] & 0x0F;
         bool is_close_frame = (opcode == 0x08); // Close opcode

         if (is_close_frame) {
            close_frame_received = true;
         }
      }

      socket.close();
      server.stop();
      server_thread.join();

      expect(close_frame_received.load()) << "Close frame should be received by client";
      expect(on_close_called.load()) << "on_close callback should be called";

      // Allow time for port to be released before next test
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
   };

   "close_frame_with_reason"_test = [] {
      std::atomic<bool> close_frame_received{false};
      std::atomic<uint16_t> received_close_code{0};
      std::string received_reason;
      std::atomic<bool> server_ready{false};
      std::mutex reason_mutex;

      // Create WebSocket server
      auto ws_server = std::make_shared<websocket_server>();

      ws_server->on_open([](std::shared_ptr<websocket_connection> conn, const request&) {
         // Close with specific code and reason
         conn->close(ws_close_code::going_away, "Server shutdown");
      });

      // Create HTTP server
      http_server server;
      server.websocket("/ws", ws_server);

      std::thread server_thread([&]() {
         server.bind(18082);
         server_ready = true;
         server.start();
      });

      // Wait for server to be ready
      expect(wait_for_condition([&] { return server_ready.load(); })) << "Server should start";
      std::this_thread::sleep_for(std::chrono::milliseconds(100));

      // Create a raw TCP client
      asio::io_context io_context;
      asio::ip::tcp::socket socket(io_context);
      asio::ip::tcp::resolver resolver(io_context);
      auto endpoints = resolver.resolve("127.0.0.1", "18082");

      asio::connect(socket, endpoints);

      // Send WebSocket handshake
      std::string handshake = "GET /ws HTTP/1.1\r\n"
                              "Host: localhost:18082\r\n"
                              "Upgrade: websocket\r\n"
                              "Connection: Upgrade\r\n"
                              "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                              "Sec-WebSocket-Version: 13\r\n\r\n";

      asio::write(socket, asio::buffer(handshake));

      // Read handshake response
      std::array<char, 1024> response_buffer;
      socket.read_some(asio::buffer(response_buffer));

      // Read close frame
      std::array<uint8_t, 1024> frame_buffer;
      std::error_code ec;
      size_t frame_len = socket.read_some(asio::buffer(frame_buffer), ec);

      if (frame_len >= 2) {
         uint8_t opcode = frame_buffer[0] & 0x0F;
         bool is_close_frame = (opcode == 0x08);

         if (is_close_frame) {
            close_frame_received = true;

            // Parse close code and reason
            uint8_t payload_len = frame_buffer[1] & 0x7F;
            size_t header_size = 2;

            if (payload_len >= 2) {
               uint16_t code = (frame_buffer[header_size] << 8) | frame_buffer[header_size + 1];
               received_close_code = code;

               if (payload_len > 2) {
                  std::lock_guard<std::mutex> lock(reason_mutex);
                  received_reason = std::string(reinterpret_cast<char*>(&frame_buffer[header_size + 2]),
                                                 payload_len - 2);
               }
            }
         }
      }

      socket.close();
      server.stop();
      server_thread.join();

      expect(close_frame_received.load()) << "Close frame should be received";
      expect(received_close_code.load() == 1001) << "Close code should be 1001 (going_away)";

      std::lock_guard<std::mutex> lock(reason_mutex);
      expect(received_reason == "Server shutdown") << "Close reason should match";

      // Allow time for port to be released before next test
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
   };
};

suite websocket_error_handling_tests = [] {
   "on_close_called_after_read_error"_test = [] {
      std::atomic<bool> on_error_called{false};
      std::atomic<bool> on_close_called{false};
      std::atomic<bool> server_ready{false};
      std::shared_ptr<websocket_connection> server_conn;
      std::mutex conn_mutex;

      // Create WebSocket server
      auto ws_server = std::make_shared<websocket_server>();

      ws_server->on_open([&](std::shared_ptr<websocket_connection> conn, const request&) {
         std::lock_guard<std::mutex> lock(conn_mutex);
         server_conn = conn;
      });

      ws_server->on_error([&](std::shared_ptr<websocket_connection>, std::error_code) { on_error_called = true; });

      ws_server->on_close([&](std::shared_ptr<websocket_connection>) { on_close_called = true; });

      // Create HTTP server
      http_server server;
      server.websocket("/ws", ws_server);

      std::thread server_thread([&]() {
         server.bind(18083);
         server_ready = true;
         server.start();
      });

      // Wait for server to be ready
      expect(wait_for_condition([&] { return server_ready.load(); })) << "Server should start";
      std::this_thread::sleep_for(std::chrono::milliseconds(100));

      // Create client and establish connection
      asio::io_context io_context;
      asio::ip::tcp::socket socket(io_context);
      asio::ip::tcp::resolver resolver(io_context);
      auto endpoints = resolver.resolve("127.0.0.1", "18083");

      asio::connect(socket, endpoints);

      // Send WebSocket handshake
      std::string handshake = "GET /ws HTTP/1.1\r\n"
                              "Host: localhost:18083\r\n"
                              "Upgrade: websocket\r\n"
                              "Connection: Upgrade\r\n"
                              "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                              "Sec-WebSocket-Version: 13\r\n\r\n";

      asio::write(socket, asio::buffer(handshake));

      // Read handshake response
      std::array<char, 1024> response_buffer;
      socket.read_some(asio::buffer(response_buffer));

      // Wait for connection to be established
      expect(wait_for_condition([&] {
         std::lock_guard<std::mutex> lock(conn_mutex);
         return server_conn != nullptr;
      })) << "Connection should be established";

      // Abruptly close the socket without proper WebSocket close handshake
      // This should trigger a read error on the server side
      socket.close();

      // Wait for error and close callbacks to be called
      expect(wait_for_condition([&] { return on_error_called.load(); })) << "on_error should be called";
      expect(wait_for_condition([&] { return on_close_called.load(); })) << "on_close should be called after error";

      server.stop();
      server_thread.join();

      // Allow time for port to be released before next test
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
   };

   "on_close_called_after_handshake_error"_test = [] {
      std::atomic<bool> on_error_called{false};
      std::atomic<bool> on_close_called{false};
      std::atomic<bool> server_ready{false};

      // Create WebSocket server
      auto ws_server = std::make_shared<websocket_server>();

      ws_server->on_error([&](std::shared_ptr<websocket_connection>, std::error_code) { on_error_called = true; });

      ws_server->on_close([&](std::shared_ptr<websocket_connection>) { on_close_called = true; });

      // Create HTTP server
      http_server server;
      server.websocket("/ws", ws_server);

      std::thread server_thread([&]() {
         server.bind(18084);
         server_ready = true;
         server.start();
      });

      // Wait for server to be ready
      expect(wait_for_condition([&] { return server_ready.load(); })) << "Server should start";
      std::this_thread::sleep_for(std::chrono::milliseconds(100));

      // Create client
      asio::io_context io_context;
      asio::ip::tcp::socket socket(io_context);
      asio::ip::tcp::resolver resolver(io_context);
      auto endpoints = resolver.resolve("127.0.0.1", "18084");

      asio::connect(socket, endpoints);

      // Send WebSocket handshake
      std::string handshake = "GET /ws HTTP/1.1\r\n"
                              "Host: localhost:18084\r\n"
                              "Upgrade: websocket\r\n"
                              "Connection: Upgrade\r\n"
                              "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                              "Sec-WebSocket-Version: 13\r\n\r\n";

      asio::write(socket, asio::buffer(handshake));

      // Start reading response but close socket immediately
      // This simulates a connection error during handshake
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      socket.close();

      // Wait for close callback - should be called even for handshake errors
      expect(wait_for_condition([&] { return on_close_called.load(); })) << "on_close should be called after handshake error";

      server.stop();
      server_thread.join();

      // Allow time for port to be released before next test
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
   };

   "multiple_closes_safe"_test = [] {
      std::atomic<int> on_close_call_count{0};
      std::atomic<bool> server_ready{false};

      // Create WebSocket server
      auto ws_server = std::make_shared<websocket_server>();

      ws_server->on_open([](std::shared_ptr<websocket_connection> conn, const request&) {
         // Try to close multiple times - should only send one close frame
         conn->close(ws_close_code::normal, "First close");
         conn->close(ws_close_code::normal, "Second close");
         conn->close(ws_close_code::normal, "Third close");
      });

      ws_server->on_close([&](std::shared_ptr<websocket_connection>) { on_close_call_count++; });

      // Create HTTP server
      http_server server;
      server.websocket("/ws", ws_server);

      std::thread server_thread([&]() {
         server.bind(18085);
         server_ready = true;
         server.start();
      });

      // Wait for server to be ready
      expect(wait_for_condition([&] { return server_ready.load(); })) << "Server should start";
      std::this_thread::sleep_for(std::chrono::milliseconds(100));

      // Create client
      asio::io_context io_context;
      asio::ip::tcp::socket socket(io_context);
      asio::ip::tcp::resolver resolver(io_context);
      auto endpoints = resolver.resolve("127.0.0.1", "18085");

      asio::connect(socket, endpoints);

      // Send WebSocket handshake
      std::string handshake = "GET /ws HTTP/1.1\r\n"
                              "Host: localhost:18085\r\n"
                              "Upgrade: websocket\r\n"
                              "Connection: Upgrade\r\n"
                              "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                              "Sec-WebSocket-Version: 13\r\n\r\n";

      asio::write(socket, asio::buffer(handshake));

      // Read handshake response
      std::array<char, 1024> response_buffer;
      socket.read_some(asio::buffer(response_buffer));

      // Give the server time to send frames
      std::this_thread::sleep_for(std::chrono::milliseconds(500));

      // Read close frame - should only receive one
      int close_frame_count = 0;
      std::array<uint8_t, 1024> frame_buffer;
      std::error_code ec;

      // First, try to read the close frame (blocking read)
      size_t frame_len = socket.read_some(asio::buffer(frame_buffer), ec);

      if (!ec && frame_len >= 2) {
         uint8_t opcode = frame_buffer[0] & 0x0F;
         if (opcode == 0x08) {
            close_frame_count++;
         }

         // Now set non-blocking to check if there are any more frames
         socket.non_blocking(true);

         // Try to read more frames (there shouldn't be any)
         for (int i = 0; i < 5; i++) {
            frame_len = socket.read_some(asio::buffer(frame_buffer), ec);
            if (ec || frame_len < 2) break;

            opcode = frame_buffer[0] & 0x0F;
            if (opcode == 0x08) {
               close_frame_count++;
            }
         }
      }

      socket.close();

      // Wait for close callback
      expect(wait_for_condition([&] { return on_close_call_count.load() > 0; })) << "on_close should be called";

      server.stop();
      server_thread.join();

      expect(close_frame_count == 1) << "Should only receive one close frame despite multiple close() calls";
      expect(on_close_call_count.load() == 1) << "on_close should only be called once";
   };
};

int main() {}
