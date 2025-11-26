// Glaze Library
// For the license information refer to glaze.hpp

// Unit tests for WebSocket close frame and error handling

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <string_view>
#include <thread>
#include <vector>

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

struct handshake_result
{
   std::string response;
   std::vector<uint8_t> leftover;
};

handshake_result read_handshake_response(asio::ip::tcp::socket& socket)
{
   constexpr std::string_view terminator = "\r\n\r\n";
   std::vector<uint8_t> buffer;
   buffer.reserve(1024);
   std::array<uint8_t, 1024> chunk{};
   std::error_code ec;

   while (true) {
      std::size_t bytes_read = socket.read_some(asio::buffer(chunk), ec);

      if (ec && bytes_read == 0) {
         break;
      }

      buffer.insert(buffer.end(), chunk.begin(), chunk.begin() + bytes_read);

      auto it = std::search(buffer.begin(), buffer.end(), terminator.begin(), terminator.end());
      if (it != buffer.end()) {
         std::size_t header_bytes = static_cast<std::size_t>(std::distance(buffer.begin(), it)) + terminator.size();

         std::string response(buffer.begin(), buffer.begin() + header_bytes);
         std::vector<uint8_t> leftover(buffer.begin() + header_bytes, buffer.end());
         return {std::move(response), std::move(leftover)};
      }
   }

   return {std::string(buffer.begin(), buffer.end()), {}};
}

struct websocket_frame
{
   uint8_t opcode{};
   std::vector<uint8_t> payload;
};

std::optional<websocket_frame> consume_frame(std::vector<uint8_t>& buffer)
{
   if (buffer.size() < 2) {
      return std::nullopt;
   }

   std::size_t offset = 0;
   uint8_t first = buffer[offset++];
   uint8_t second = buffer[offset++];
   uint8_t opcode = first & 0x0F;
   bool masked = (second & 0x80) != 0;
   uint64_t payload_length = static_cast<uint64_t>(second & 0x7F);

   if (payload_length == 126) {
      if (buffer.size() < offset + 2) {
         return std::nullopt;
      }
      payload_length = (static_cast<uint64_t>(buffer[offset]) << 8) | buffer[offset + 1];
      offset += 2;
   }
   else if (payload_length == 127) {
      if (buffer.size() < offset + 8) {
         return std::nullopt;
      }
      payload_length = 0;
      for (int i = 0; i < 8; ++i) {
         payload_length = (payload_length << 8) | buffer[offset + i];
      }
      offset += 8;
   }

   std::array<uint8_t, 4> mask_key{};
   if (masked) {
      if (buffer.size() < offset + 4) {
         return std::nullopt;
      }
      std::copy_n(buffer.data() + offset, 4, mask_key.data());
      offset += 4;
   }

   if (buffer.size() < offset + payload_length) {
      return std::nullopt;
   }

   std::vector<uint8_t> payload(static_cast<std::size_t>(payload_length));
   for (uint64_t i = 0; i < payload_length; ++i) {
      uint8_t byte = buffer[offset + static_cast<std::size_t>(i)];
      if (masked) {
         byte ^= mask_key[static_cast<std::size_t>(i) % 4];
      }
      payload[static_cast<std::size_t>(i)] = byte;
   }

   buffer.erase(buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(offset + payload_length));

   return websocket_frame{opcode, std::move(payload)};
}

std::optional<websocket_frame> poll_for_frame(asio::ip::tcp::socket& socket, std::vector<uint8_t>& pending,
                                              std::chrono::milliseconds timeout)
{
   if (auto frame = consume_frame(pending)) {
      return frame;
   }

   std::array<uint8_t, 1024> buffer{};
   auto start = std::chrono::steady_clock::now();

   while (std::chrono::steady_clock::now() - start < timeout) {
      std::error_code ec;
      std::size_t bytes_read = socket.read_some(asio::buffer(buffer), ec);

      if (ec == asio::error::would_block) {
         std::this_thread::sleep_for(std::chrono::milliseconds(10));
         continue;
      }

      if (bytes_read > 0) {
         pending.insert(pending.end(), buffer.begin(), buffer.begin() + bytes_read);
         if (auto frame = consume_frame(pending)) {
            return frame;
         }
         continue;
      }

      if (ec) {
         return std::nullopt;
      }
   }

   return std::nullopt;
}

// Helper to send a close frame (client must use masking)
void send_close_frame(asio::ip::tcp::socket& socket, uint16_t code = 1000)
{
   // WebSocket close frame from client (must be masked)
   // Frame: 0x88 (FIN=1, opcode=8), 0x82 (mask=1, len=2), 4-byte mask, 2-byte code (masked)
   uint8_t mask[4] = {0x12, 0x34, 0x56, 0x78}; // Simple mask
   uint8_t code_bytes[2] = {static_cast<uint8_t>(code >> 8), static_cast<uint8_t>(code & 0xFF)};

   std::vector<uint8_t> frame = {0x88,
                                 0x82,
                                 mask[0],
                                 mask[1],
                                 mask[2],
                                 mask[3],
                                 static_cast<uint8_t>(code_bytes[0] ^ mask[0]),
                                 static_cast<uint8_t>(code_bytes[1] ^ mask[1])};

   asio::write(socket, asio::buffer(frame));
}

suite websocket_close_frame_tests = [] {
   "close_frame_is_sent"_test = [] {
      std::atomic<bool> close_frame_received{false};
      std::atomic<bool> on_close_called{false};
      std::atomic<bool> server_ready{false};

      // Create WebSocket server
      auto ws_server = std::make_shared<websocket_server>();

      ws_server->on_open([](std::shared_ptr<websocket_connection<asio::ip::tcp::socket>> conn, const request&) {
         // Server initiates close - this should send a close frame
         conn->close(ws_close_code::normal, "Test close");
      });

      ws_server->on_close([&](std::shared_ptr<websocket_connection<asio::ip::tcp::socket>>, ws_close_code,
                              std::string_view) { on_close_called = true; });

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
      std::string handshake =
         "GET /ws HTTP/1.1\r\n"
         "Host: localhost:18081\r\n"
         "Upgrade: websocket\r\n"
         "Connection: Upgrade\r\n"
         "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
         "Sec-WebSocket-Version: 13\r\n\r\n";

      asio::write(socket, asio::buffer(handshake));

      auto handshake_response = read_handshake_response(socket);
      expect(handshake_response.response.find("101 Switching Protocols") != std::string::npos)
         << "Handshake should succeed";

      socket.non_blocking(true);
      std::vector<uint8_t> pending = std::move(handshake_response.leftover);
      auto frame = poll_for_frame(socket, pending, std::chrono::milliseconds(1000));

      if (frame && frame->opcode == 0x08) {
         close_frame_received = true;
         // Send close frame response to complete the handshake
         socket.non_blocking(false);
         send_close_frame(socket, 1000);
      }

      expect(close_frame_received.load()) << "Close frame should be received by client";
      expect(wait_for_condition([&] { return on_close_called.load(); })) << "on_close callback should be called";

      socket.close();
      server.stop();
      server_thread.join();

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

      ws_server->on_open([](std::shared_ptr<websocket_connection<asio::ip::tcp::socket>> conn, const request&) {
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
      std::string handshake =
         "GET /ws HTTP/1.1\r\n"
         "Host: localhost:18082\r\n"
         "Upgrade: websocket\r\n"
         "Connection: Upgrade\r\n"
         "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
         "Sec-WebSocket-Version: 13\r\n\r\n";

      asio::write(socket, asio::buffer(handshake));

      auto handshake_response = read_handshake_response(socket);
      expect(handshake_response.response.find("101 Switching Protocols") != std::string::npos)
         << "Handshake should succeed";

      socket.non_blocking(true);
      std::vector<uint8_t> pending = std::move(handshake_response.leftover);
      auto frame = poll_for_frame(socket, pending, std::chrono::milliseconds(1000));

      if (frame && frame->opcode == 0x08) {
         close_frame_received = true;

         if (frame->payload.size() >= 2) {
            uint16_t code = (static_cast<uint16_t>(frame->payload[0]) << 8) | frame->payload[1];
            received_close_code = code;

            if (frame->payload.size() > 2) {
               std::lock_guard<std::mutex> lock(reason_mutex);
               received_reason.assign(reinterpret_cast<const char*>(frame->payload.data() + 2),
                                      frame->payload.size() - 2);
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
      std::shared_ptr<websocket_connection<asio::ip::tcp::socket>> server_conn;
      std::mutex conn_mutex;

      // Create WebSocket server
      auto ws_server = std::make_shared<websocket_server>();

      ws_server->on_open([&](std::shared_ptr<websocket_connection<asio::ip::tcp::socket>> conn, const request&) {
         std::lock_guard<std::mutex> lock(conn_mutex);
         server_conn = conn;
      });

      ws_server->on_error([&](std::shared_ptr<websocket_connection<asio::ip::tcp::socket>>, std::error_code) {
         on_error_called = true;
      });

      ws_server->on_close([&](std::shared_ptr<websocket_connection<asio::ip::tcp::socket>>, ws_close_code,
                              std::string_view) { on_close_called = true; });

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
      std::string handshake =
         "GET /ws HTTP/1.1\r\n"
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

      ws_server->on_error([&](std::shared_ptr<websocket_connection<asio::ip::tcp::socket>>, std::error_code) {
         on_error_called = true;
      });

      ws_server->on_close([&](std::shared_ptr<websocket_connection<asio::ip::tcp::socket>>, ws_close_code,
                              std::string_view) { on_close_called = true; });

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
      std::string handshake =
         "GET /ws HTTP/1.1\r\n"
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
      expect(wait_for_condition([&] { return on_close_called.load(); }))
         << "on_close should be called after handshake error";

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

      ws_server->on_open([](std::shared_ptr<websocket_connection<asio::ip::tcp::socket>> conn, const request&) {
         // Try to close multiple times - should only send one close frame
         conn->close(ws_close_code::normal, "First close");
         conn->close(ws_close_code::normal, "Second close");
         conn->close(ws_close_code::normal, "Third close");
      });

      ws_server->on_close([&](std::shared_ptr<websocket_connection<asio::ip::tcp::socket>>, ws_close_code,
                              std::string_view) { on_close_call_count++; });

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
      std::string handshake =
         "GET /ws HTTP/1.1\r\n"
         "Host: localhost:18085\r\n"
         "Upgrade: websocket\r\n"
         "Connection: Upgrade\r\n"
         "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
         "Sec-WebSocket-Version: 13\r\n\r\n";

      asio::write(socket, asio::buffer(handshake));

      auto handshake_response = read_handshake_response(socket);
      expect(handshake_response.response.find("101 Switching Protocols") != std::string::npos)
         << "Handshake should succeed";

      socket.non_blocking(true);
      std::vector<uint8_t> pending = std::move(handshake_response.leftover);

      int close_frame_count = 0;
      auto frame = poll_for_frame(socket, pending, std::chrono::milliseconds(1000));

      while (frame) {
         if (frame->opcode == 0x08) {
            close_frame_count++;
            // Send close frame response after receiving the first close frame
            if (close_frame_count == 1) {
               socket.non_blocking(false);
               send_close_frame(socket, 1000);
               socket.non_blocking(true);
            }
         }
         frame = poll_for_frame(socket, pending, std::chrono::milliseconds(200));
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
