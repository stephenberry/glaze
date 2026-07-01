// Glaze Library
// For the license information refer to glaze.hpp

// Unit tests for WebSocket close frame and error handling

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
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
   asio::error_code ec;

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
      asio::error_code ec;
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

handshake_result connect_raw_websocket(asio::io_context& io_context, asio::ip::tcp::socket& socket, uint16_t port)
{
   asio::ip::tcp::resolver resolver(io_context);
   auto endpoints = resolver.resolve("127.0.0.1", std::to_string(port));
   asio::connect(socket, endpoints);

   std::string handshake =
      "GET /ws HTTP/1.1\r\n"
      "Host: localhost:" +
      std::to_string(port) +
      "\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
      "Sec-WebSocket-Version: 13\r\n\r\n";

   asio::write(socket, asio::buffer(handshake));
   return read_handshake_response(socket);
}

std::size_t masked_client_frame_header_size(std::size_t payload_length)
{
   if (payload_length < 126) {
      return 6;
   }
   if (payload_length <= 0xFFFF) {
      return 8;
   }
   return 14;
}

std::vector<uint8_t> make_masked_client_frame(uint8_t opcode, const std::vector<uint8_t>& payload, bool fin = true)
{
   std::vector<uint8_t> frame;
   frame.reserve(masked_client_frame_header_size(payload.size()) + payload.size());

   frame.push_back(static_cast<uint8_t>((fin ? 0x80 : 0x00) | (opcode & 0x0F)));

   if (payload.size() < 126) {
      frame.push_back(static_cast<uint8_t>(0x80 | payload.size()));
   }
   else if (payload.size() <= 0xFFFF) {
      frame.push_back(0xFE);
      frame.push_back(static_cast<uint8_t>(payload.size() >> 8));
      frame.push_back(static_cast<uint8_t>(payload.size() & 0xFF));
   }
   else {
      frame.push_back(0xFF);
      const auto payload_length = static_cast<uint64_t>(payload.size());
      for (int i = 7; i >= 0; --i) {
         frame.push_back(static_cast<uint8_t>(payload_length >> (8 * i)));
      }
   }

   const std::array<uint8_t, 4> mask_key{0x12, 0x34, 0x56, 0x78};
   frame.insert(frame.end(), mask_key.begin(), mask_key.end());

   for (std::size_t i = 0; i < payload.size(); ++i) {
      frame.push_back(static_cast<uint8_t>(payload[i] ^ mask_key[i % mask_key.size()]));
   }

   return frame;
}

std::vector<uint8_t> make_unmasked_client_frame(uint8_t opcode, const std::vector<uint8_t>& payload, bool fin = true)
{
   std::vector<uint8_t> frame;
   frame.reserve(2 + payload.size());

   frame.push_back(static_cast<uint8_t>((fin ? 0x80 : 0x00) | (opcode & 0x0F)));
   // Mask bit (0x80) intentionally left clear
   frame.push_back(static_cast<uint8_t>(payload.size()));
   frame.insert(frame.end(), payload.begin(), payload.end());

   return frame;
}

uint16_t close_code_from_frame(const websocket_frame& frame)
{
   if (frame.payload.size() < 2) {
      return 0;
   }
   return static_cast<uint16_t>((static_cast<uint16_t>(frame.payload[0]) << 8) | frame.payload[1]);
}

suite websocket_close_frame_tests = [] {
   "coalesced_close_response_is_processed"_test = [] {
      constexpr uint16_t port = 18088;
      std::atomic<bool> server_ready{false};
      std::atomic<bool> server_closed{false};
      std::atomic<int> message_count{0};

      auto ws_server = std::make_shared<websocket_server>();
      ws_server->on_message([&](auto conn, std::string_view, ws_opcode) {
         ++message_count;
         conn->close(ws_close_code::normal);
      });
      ws_server->on_close([&](auto, ws_close_code, std::string_view) { server_closed = true; });

      http_server server;
      server.websocket("/ws", ws_server);

      std::thread server_thread([&]() {
         server.bind(port);
         server_ready = true;
         server.start();
      });

      expect(wait_for_condition([&] { return server_ready.load(); })) << "Server should start";
      std::this_thread::sleep_for(std::chrono::milliseconds(100));

      asio::io_context io_context;
      asio::ip::tcp::socket socket(io_context);
      auto handshake_response = connect_raw_websocket(io_context, socket, port);
      expect(handshake_response.response.find("101 Switching Protocols") != std::string::npos)
         << "Handshake should succeed";

      const std::vector<uint8_t> message_payload{'c', 'l', 'o', 's', 'e'};
      auto message_frame = make_masked_client_frame(0x01, message_payload);
      const std::vector<uint8_t> close_payload{0x03, 0xE8};
      auto close_frame = make_masked_client_frame(0x08, close_payload);
      message_frame.insert(message_frame.end(), close_frame.begin(), close_frame.end());

      asio::write(socket, asio::buffer(message_frame));

      expect(wait_for_condition([&] { return message_count.load() == 1; })) << "Text message should be delivered";
      expect(wait_for_condition([&] { return server_closed.load(); }))
         << "Coalesced close response should complete the server's close handshake";

      // The client must still receive the server's close frame echoing the normal close code.
      socket.non_blocking(true);
      std::vector<uint8_t> pending = std::move(handshake_response.leftover);
      auto server_close_frame = poll_for_frame(socket, pending, std::chrono::milliseconds(1000));
      expect(server_close_frame.has_value()) << "Server should send a close frame to the client";
      expect(server_close_frame && server_close_frame->opcode == 0x08) << "Server frame should be a close frame";
      if (server_close_frame) {
         expect(close_code_from_frame(*server_close_frame) == 1000) << "Server close code should be 1000 (normal)";
      }

      socket.close();
      server.stop();
      server_thread.join();

      std::this_thread::sleep_for(std::chrono::milliseconds(500));
   };

   "close_frame_is_sent"_test = [] {
      std::atomic<bool> close_frame_received{false};
      std::atomic<bool> on_close_called{false};
      std::atomic<bool> server_ready{false};

      // Create WebSocket server
      auto ws_server = std::make_shared<websocket_server>();

      ws_server->on_open([](auto conn, const request&) {
         // Server initiates close - this should send a close frame
         conn->close(ws_close_code::normal, "Test close");
      });

      ws_server->on_close([&](auto, ws_close_code, std::string_view) { on_close_called = true; });

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

      ws_server->on_open([](auto conn, const request&) {
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
      std::shared_ptr<websocket_connection_interface> server_conn;
      std::mutex conn_mutex;

      // Create WebSocket server
      auto ws_server = std::make_shared<websocket_server>();

      ws_server->on_open([&](auto conn, const request&) {
         std::lock_guard<std::mutex> lock(conn_mutex);
         server_conn = conn;
      });

      ws_server->on_error([&](auto, std::error_code) { on_error_called = true; });

      ws_server->on_close([&](auto, ws_close_code, std::string_view) { on_close_called = true; });

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

      ws_server->on_error([&](auto, std::error_code) { on_error_called = true; });

      ws_server->on_close([&](auto, ws_close_code, std::string_view) { on_close_called = true; });

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

      ws_server->on_open([](auto conn, const request&) {
         // Try to close multiple times - should only send one close frame
         conn->close(ws_close_code::normal, "First close");
         conn->close(ws_close_code::normal, "Second close");
         conn->close(ws_close_code::normal, "Third close");
      });

      ws_server->on_close([&](auto, ws_close_code, std::string_view) { on_close_call_count++; });

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

suite websocket_utf8_fail_fast_tests = [] {
   "invalid_utf8_in_incomplete_text_frame_fails_fast"_test = [] {
      constexpr uint16_t port = 18086;
      std::atomic<bool> server_ready{false};
      std::atomic<bool> server_closed{false};
      std::atomic<int> message_count{0};
      std::atomic<uint16_t> server_close_code{0};

      auto ws_server = std::make_shared<websocket_server>();
      ws_server->on_message([&](auto, std::string_view, ws_opcode) { ++message_count; });
      ws_server->on_close([&](auto, ws_close_code code, std::string_view) {
         server_close_code = static_cast<uint16_t>(code);
         server_closed = true;
      });

      http_server server;
      server.websocket("/ws", ws_server);

      std::thread server_thread([&]() {
         server.bind(port);
         server_ready = true;
         server.start();
      });

      expect(wait_for_condition([&] { return server_ready.load(); })) << "Server should start";
      std::this_thread::sleep_for(std::chrono::milliseconds(100));

      asio::io_context io_context;
      asio::ip::tcp::socket socket(io_context);
      auto handshake_response = connect_raw_websocket(io_context, socket, port);
      expect(handshake_response.response.find("101 Switching Protocols") != std::string::npos)
         << "Handshake should succeed";

      std::vector<uint8_t> payload(32768, static_cast<uint8_t>('a'));
      payload[0] = 0xFF;
      auto frame = make_masked_client_frame(0x01, payload);
      const auto header_size = masked_client_frame_header_size(payload.size());

      asio::write(socket, asio::buffer(frame.data(), header_size + 1));

      socket.non_blocking(true);
      std::vector<uint8_t> pending = std::move(handshake_response.leftover);
      auto close_frame = poll_for_frame(socket, pending, std::chrono::milliseconds(1000));

      expect(close_frame.has_value()) << "Server should fail fast before the full text frame arrives";
      expect(close_frame && close_frame->opcode == 0x08) << "Server should send a close frame";
      if (close_frame) {
         expect(close_code_from_frame(*close_frame) == 1007) << "Invalid UTF-8 should close with 1007";
      }

      expect(message_count.load() == 0) << "Invalid text payload must not be delivered";
      expect(wait_for_condition([&] { return server_closed.load(); })) << "Server on_close should be called";
      expect(server_close_code.load() == 1007) << "Server on_close should report 1007";

      socket.close();
      server.stop();
      server_thread.join();

      std::this_thread::sleep_for(std::chrono::milliseconds(500));
   };

   "split_masked_text_frame_preserves_utf8_payload"_test = [] {
      constexpr uint16_t port = 18087;
      std::atomic<bool> server_ready{false};
      std::atomic<int> message_count{0};
      std::mutex received_mutex;
      std::string received_message;

      auto ws_server = std::make_shared<websocket_server>();
      ws_server->on_message([&](auto, std::string_view message, ws_opcode opcode) {
         if (opcode == ws_opcode::text) {
            std::lock_guard<std::mutex> lock(received_mutex);
            received_message.assign(message);
            ++message_count;
         }
      });
      ws_server->on_close([](auto, ws_close_code, std::string_view) {});

      http_server server;
      server.websocket("/ws", ws_server);

      std::thread server_thread([&]() {
         server.bind(port);
         server_ready = true;
         server.start();
      });

      expect(wait_for_condition([&] { return server_ready.load(); })) << "Server should start";
      std::this_thread::sleep_for(std::chrono::milliseconds(100));

      asio::io_context io_context;
      asio::ip::tcp::socket socket(io_context);
      auto handshake_response = connect_raw_websocket(io_context, socket, port);
      expect(handshake_response.response.find("101 Switching Protocols") != std::string::npos)
         << "Handshake should succeed";

      std::vector<uint8_t> payload;
      const std::string prefix = "txt: ";
      payload.insert(payload.end(), prefix.begin(), prefix.end());
      const std::array<uint8_t, 4> split_code_point{0xF0, 0x9F, 0x92, 0xA9};
      payload.insert(payload.end(), split_code_point.begin(), split_code_point.end());
      const std::string suffix = " after split ";
      payload.insert(payload.end(), suffix.begin(), suffix.end());
      payload.insert(payload.end(), 64, static_cast<uint8_t>('x'));

      const std::string expected(payload.begin(), payload.end());
      auto frame = make_masked_client_frame(0x01, payload);
      const auto header_size = masked_client_frame_header_size(payload.size());
      const auto first_payload_bytes = prefix.size() + 2;

      asio::write(socket, asio::buffer(frame.data(), header_size + first_payload_bytes));
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      asio::write(socket, asio::buffer(frame.data() + header_size + first_payload_bytes,
                                       frame.size() - header_size - first_payload_bytes));

      expect(wait_for_condition([&] { return message_count.load() == 1; })) << "Split text frame should be delivered";

      {
         std::lock_guard<std::mutex> lock(received_mutex);
         expect(received_message == expected) << "Split masked text frame payload should be preserved";
      }

      send_close_frame(socket, 1000);
      socket.close();

      server.stop();
      server_thread.join();

      std::this_thread::sleep_for(std::chrono::milliseconds(500));
   };
};

suite websocket_masking_tests = [] {
   "unmasked_client_frame_fails_with_protocol_error"_test = [] {
      constexpr uint16_t port = 18089;
      std::atomic<bool> server_ready{false};
      std::atomic<bool> server_closed{false};
      std::atomic<int> message_count{0};
      std::atomic<uint16_t> server_close_code{0};

      auto ws_server = std::make_shared<websocket_server>();
      ws_server->on_message([&](auto, std::string_view, ws_opcode) { ++message_count; });
      ws_server->on_close([&](auto, ws_close_code code, std::string_view) {
         server_close_code = static_cast<uint16_t>(code);
         server_closed = true;
      });

      http_server server;
      server.websocket("/ws", ws_server);

      std::thread server_thread([&]() {
         server.bind(port);
         server_ready = true;
         server.start();
      });

      expect(wait_for_condition([&] { return server_ready.load(); })) << "Server should start";
      std::this_thread::sleep_for(std::chrono::milliseconds(100));

      asio::io_context io_context;
      asio::ip::tcp::socket socket(io_context);
      auto handshake_response = connect_raw_websocket(io_context, socket, port);
      expect(handshake_response.response.find("101 Switching Protocols") != std::string::npos)
         << "Handshake should succeed";

      const std::vector<uint8_t> payload{'H', 'i'};
      auto frame = make_unmasked_client_frame(0x01, payload);
      asio::write(socket, asio::buffer(frame));

      socket.non_blocking(true);
      std::vector<uint8_t> pending = std::move(handshake_response.leftover);
      auto close_frame = poll_for_frame(socket, pending, std::chrono::milliseconds(1000));

      expect(close_frame.has_value()) << "Server should respond to an unmasked frame with a close frame";
      expect(close_frame && close_frame->opcode == 0x08) << "Server frame should be a close frame";
      if (close_frame) {
         expect(close_code_from_frame(*close_frame) == 1002) << "Unmasked client frame should close with 1002";
      }

      expect(message_count.load() == 0) << "Unmasked frame payload must not be delivered";
      expect(wait_for_condition([&] { return server_closed.load(); })) << "Server on_close should be called";
      expect(server_close_code.load() == 1002) << "Server on_close should report 1002";

      socket.close();
      server.stop();
      server_thread.join();

      std::this_thread::sleep_for(std::chrono::milliseconds(500));
   };
};

int main() {}
