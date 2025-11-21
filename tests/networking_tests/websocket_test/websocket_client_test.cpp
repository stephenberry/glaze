#include "glaze/net/websocket_client.hpp"

#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
#include <random>

#include "glaze/glaze.hpp"
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

   ws_server->on_open([](auto /*conn*/, const request&) {});

   ws_server->on_message([](auto conn, std::string_view message, ws_opcode opcode) {
      if (opcode == ws_opcode::text) {
         // Log large messages
         if (message.size() > 100000) {
            std::cout << "[echo_server] Received large text message: " << message.size() << " bytes" << std::endl;
         }
         // Efficiently build echo response for large messages
         std::string echo_msg;
         echo_msg.reserve(6 + message.size());
         echo_msg = "Echo: ";
         echo_msg.append(message);
         conn->send_text(echo_msg);
         if (message.size() > 100000) {
            std::cout << "[echo_server] Sent echo response: " << echo_msg.size() << " bytes" << std::endl;
         }
      }
      else if (opcode == ws_opcode::binary) {
         conn->send_binary(message);
      }
   });

   ws_server->on_close([](auto /*conn*/) {});

   ws_server->on_error([](auto /*conn*/, std::error_code ec) {
      std::cerr << "[echo_server] Server Error: " << ec.message() << " (code=" << ec.value() << ")\n";
   });

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
      server_ready = true;
   }
}

// Helper to run a server that closes connections after first message
void run_close_after_message_server(std::atomic<bool>& server_ready, std::atomic<bool>& should_stop, uint16_t port)
{
   http_server server;
   auto ws_server = std::make_shared<websocket_server>();

   ws_server->on_open([](auto /*conn*/, const request&) {});

   ws_server->on_message([](auto conn, std::string_view /*message*/, ws_opcode /*opcode*/) {
      conn->close(ws_close_code::normal, "Test close");
   });

   ws_server->on_close([](auto /*conn*/) {});

   ws_server->on_error([](auto /*conn*/, std::error_code /*ec*/) {});

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
      server_ready = true;
   }
}

// Helper to run a server that counts messages
void run_counting_server(std::atomic<bool>& server_ready, std::atomic<bool>& should_stop, uint16_t port,
                         std::atomic<int>& message_count)
{
   http_server server;
   auto ws_server = std::make_shared<websocket_server>();

   ws_server->on_open([](auto /*conn*/, const request&) {});

   ws_server->on_message([&message_count](auto conn, std::string_view message, ws_opcode opcode) {
      if (opcode == ws_opcode::text) {
         message_count++;
         conn->send_text(std::string("Message ") + std::to_string(message_count.load()));
      }
   });

   ws_server->on_close([](auto /*conn*/) {});

   ws_server->on_error([](auto /*conn*/, std::error_code /*ec*/) {});

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
      server_ready = true;
   }
}

struct TestMessage {
   std::string type;
   std::string content;
   int64_t timestamp;
};

template <>
struct glz::meta<TestMessage> {
   using T = TestMessage;
   static constexpr auto value = object("type", &T::type, "content", &T::content, "timestamp", &T::timestamp);
};

suite websocket_client_tests = [] {
   "basic_echo_test"_test = [] {
      uint16_t port = 8091;
      std::atomic<bool> server_ready{false};
      std::atomic<bool> stop_server{false};

      std::thread server_thread(run_echo_server, std::ref(server_ready), std::ref(stop_server), port);

      expect(wait_for_condition([&] { return server_ready.load(); })) << "Server failed to start";

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

      client.on_error([&](std::error_code ec) {
         std::cerr << "Client Error: " << ec.message()
                   << " (code=" << ec.value()
                   << ", category=" << ec.category().name() << ")\n";
      });

      client.on_close([&](ws_close_code, std::string_view) {
         connection_closed = true;
         client.ctx_->stop();
      });

      std::string client_url = "ws://localhost:" + std::to_string(port) + "/ws";
      client.connect(client_url);

      std::thread client_thread([&client]() { client.ctx_->run(); });

      expect(wait_for_condition([&] { return message_received.load(); })) << "Message was not received/echoed";
      expect(wait_for_condition([&] { return connection_closed.load(); })) << "Connection was not closed gracefully";

      if (!client.ctx_->stopped()) {
         client.ctx_->stop();
      }

      if (client_thread.joinable()) {
         client_thread.join();
      }

      stop_server = true;
      server_thread.join();
   };

   "multiple_messages_test"_test = [] {
      uint16_t port = 8092;
      std::atomic<bool> server_ready{false};
      std::atomic<bool> stop_server{false};
      std::atomic<int> server_message_count{0};

      std::thread server_thread(run_counting_server, std::ref(server_ready), std::ref(stop_server), port,
                                std::ref(server_message_count));

      expect(wait_for_condition([&] { return server_ready.load(); })) << "Server failed to start";

      websocket_client client;
      std::atomic<int> messages_received{0};
      const int expected_messages = 5;

      client.on_open([&client]() {
         // Send multiple messages
         for (int i = 1; i <= 5; ++i) {
            client.send("Message " + std::to_string(i));
         }
      });

      client.on_message([&](std::string_view message, ws_opcode opcode) {
         if (opcode == ws_opcode::text) {
            messages_received++;
            if (messages_received >= expected_messages) {
               client.close();
            }
         }
      });

      client.on_error([](std::error_code ec) { std::cerr << "Client Error: " << ec.message() << "\n"; });

      client.on_close([&](ws_close_code, std::string_view) { client.ctx_->stop(); });

      std::string client_url = "ws://localhost:" + std::to_string(port) + "/ws";
      client.connect(client_url);

      std::thread client_thread([&client]() { client.ctx_->run(); });

      expect(wait_for_condition([&] { return messages_received.load() >= expected_messages; }))
         << "Did not receive all messages";

      if (!client.ctx_->stopped()) {
         client.ctx_->stop();
      }

      if (client_thread.joinable()) {
         client_thread.join();
      }

      stop_server = true;
      server_thread.join();

      expect(messages_received.load() == expected_messages) << "Expected 5 messages";
   };

   "binary_message_test"_test = [] {
      uint16_t port = 8093;
      std::atomic<bool> server_ready{false};
      std::atomic<bool> stop_server{false};

      std::thread server_thread(run_echo_server, std::ref(server_ready), std::ref(stop_server), port);

      expect(wait_for_condition([&] { return server_ready.load(); })) << "Server failed to start";

      websocket_client client;
      std::atomic<bool> binary_received{false};

      // Generate binary data
      std::vector<uint8_t> binary_data = {0x00, 0x01, 0x02, 0xFF, 0xFE, 0xFD};
      std::string binary_msg(reinterpret_cast<const char*>(binary_data.data()), binary_data.size());

      client.on_open([&client, &binary_msg]() {
         // Send binary message
         client.send_binary(binary_msg);
      });

      client.on_message([&](std::string_view message, ws_opcode opcode) {
         if (opcode == ws_opcode::binary && message == binary_msg) {
            binary_received = true;
            client.close();
         }
      });

      client.on_error([](std::error_code ec) { std::cerr << "Client Error: " << ec.message() << "\n"; });

      client.on_close([&](ws_close_code, std::string_view) { client.ctx_->stop(); });

      std::string client_url = "ws://localhost:" + std::to_string(port) + "/ws";
      client.connect(client_url);

      std::thread client_thread([&client]() { client.ctx_->run(); });

      expect(wait_for_condition([&] { return binary_received.load(); })) << "Binary message was not received";

      if (!client.ctx_->stopped()) {
         client.ctx_->stop();
      }

      if (client_thread.joinable()) {
         client_thread.join();
      }

      stop_server = true;
      server_thread.join();
   };

   "large_message_test"_test = [] {
      uint16_t port = 8094;
      std::atomic<bool> server_ready{false};
      std::atomic<bool> stop_server{false};

      std::thread server_thread(run_echo_server, std::ref(server_ready), std::ref(stop_server), port);

      expect(wait_for_condition([&] { return server_ready.load(); })) << "Server failed to start";

      websocket_client client;
      std::atomic<bool> large_message_received{false};
      std::atomic<bool> connection_opened{false};

      // Create a 256KB message to test large message handling
      std::string large_msg(256 * 1024, 'A');
      large_msg += "END";

      client.on_open([&client, &large_msg, &connection_opened]() {
         std::cout << "[large_message_test] Connected, sending " << large_msg.size() << " byte message" << std::endl;
         connection_opened = true;
         client.send(large_msg);
      });

      client.on_message([&](std::string_view message, ws_opcode opcode) {
         std::cout << "[large_message_test] Received " << message.size() << " bytes, opcode=" << static_cast<int>(opcode) << std::endl;
         if (opcode == ws_opcode::text && message.size() > 256 * 1024 && message.find("END") != std::string_view::npos) {
            std::cout << "[large_message_test] Large message received successfully" << std::endl;
            large_message_received = true;
            client.close();
         } else {
            std::cout << "[large_message_test] Message didn't match criteria (size=" << message.size()
                      << ", has_END=" << (message.find("END") != std::string_view::npos) << ")" << std::endl;
         }
      });

      client.on_error([](std::error_code ec) {
         std::cerr << "[large_message_test] Client Error: " << ec.message() << " (code=" << ec.value() << ")\n";
      });

      client.on_close([&](ws_close_code code, std::string_view reason) {
         std::cout << "[large_message_test] Connection closed. Code: " << static_cast<int>(code) << ", Reason: " << reason << std::endl;
         client.ctx_->stop();
      });

      std::string client_url = "ws://localhost:" + std::to_string(port) + "/ws";
      client.connect(client_url);

      std::thread client_thread([&client]() { client.ctx_->run(); });

      bool received = wait_for_condition([&] { return large_message_received.load(); }, std::chrono::milliseconds(60000));

      if (!received) {
         std::cerr << "[large_message_test] Test failed - connection_opened=" << connection_opened.load()
                   << ", large_message_received=" << large_message_received.load() << std::endl;
      }

      expect(received) << "Large message was not received";

      if (!client.ctx_->stopped()) {
         client.ctx_->stop();
      }

      if (client_thread.joinable()) {
         client_thread.join();
      }

      stop_server = true;
      server_thread.join();
   };

   "connection_refused_test"_test = [] {
      websocket_client client;
      std::atomic<bool> error_received{false};

      client.on_open([]() { std::cout << "Unexpectedly connected!" << std::endl; });

      client.on_error([&](std::error_code ec) {
         std::cout << "Expected error: " << ec.message() << std::endl;
         error_received = true;
         client.ctx_->stop();
      });

      // Try to connect to a port with no server
      client.connect("ws://localhost:9999/ws");

      std::thread client_thread([&client]() { client.ctx_->run(); });

      expect(wait_for_condition([&] { return error_received.load(); })) << "Expected connection error";

      if (!client.ctx_->stopped()) {
         client.ctx_->stop();
      }

      if (client_thread.joinable()) {
         client_thread.join();
      }
   };

   "invalid_url_test"_test = [] {
      websocket_client client;
      std::atomic<bool> error_received{false};

      client.on_error([&](std::error_code ec) {
         error_received = true;
         if (!client.ctx_->stopped()) {
            client.ctx_->stop();
         }
      });

      // Invalid URL
      client.connect("not-a-valid-url");

      std::thread client_thread([&client]() { client.ctx_->run(); });

      expect(wait_for_condition([&] { return error_received.load(); })) << "Expected URL parse error";

      if (!client.ctx_->stopped()) {
         client.ctx_->stop();
      }

      if (client_thread.joinable()) {
         client_thread.join();
      }
   };

   "server_initiated_close_test"_test = [] {
      uint16_t port = 8095;
      std::atomic<bool> server_ready{false};
      std::atomic<bool> stop_server{false};

      std::thread server_thread(run_close_after_message_server, std::ref(server_ready), std::ref(stop_server), port);

      expect(wait_for_condition([&] { return server_ready.load(); })) << "Server failed to start";

      websocket_client client;
      std::atomic<bool> connection_closed{false};
      std::atomic<bool> close_code_correct{false};

      client.on_open([&client]() { client.send("Trigger close"); });

      client.on_message([](std::string_view /*message*/, ws_opcode /*opcode*/) {});

      client.on_close([&](ws_close_code code, std::string_view reason) {
         std::cout << "Connection closed with code: " << static_cast<int>(code) << ", reason: " << reason << std::endl;
         connection_closed = true;
         if (code == ws_close_code::normal && reason == "Test close") {
            close_code_correct = true;
         }
         client.ctx_->stop();
      });

      client.on_error([](std::error_code ec) { std::cerr << "Client Error: " << ec.message() << "\n"; });

      std::string client_url = "ws://localhost:" + std::to_string(port) + "/ws";
      client.connect(client_url);

      std::thread client_thread([&client]() { client.ctx_->run(); });

      expect(wait_for_condition([&] { return connection_closed.load(); }))
         << "Connection was not closed by server";
      expect(close_code_correct.load()) << "Close code or reason was incorrect";

      if (!client.ctx_->stopped()) {
         client.ctx_->stop();
      }

      if (client_thread.joinable()) {
         client_thread.join();
      }

      stop_server = true;
      server_thread.join();
   };

   "multiple_clients_shared_context_test"_test = [] {
      uint16_t port = 8096;
      std::atomic<bool> server_ready{false};
      std::atomic<bool> stop_server{false};

      std::thread server_thread(run_echo_server, std::ref(server_ready), std::ref(stop_server), port);

      expect(wait_for_condition([&] { return server_ready.load(); })) << "Server failed to start";

      // Shared io_context
      auto io_ctx = std::make_shared<asio::io_context>();

      const int num_clients = 3;
      std::vector<std::unique_ptr<websocket_client>> clients;
      std::vector<std::atomic<bool>> messages_received(num_clients);

      for (int i = 0; i < num_clients; ++i) {
         messages_received[i] = false;
         clients.push_back(std::make_unique<websocket_client>(io_ctx));

         auto* client_ptr = clients[i].get();
         int client_id = i;

         client_ptr->on_open([client_ptr, client_id]() { client_ptr->send("Hello from client " + std::to_string(client_id)); });

         client_ptr->on_message([&messages_received, client_ptr, client_id](std::string_view message, ws_opcode opcode) {
            if (opcode == ws_opcode::text) {
               std::cout << "Client " << client_id << " received: " << message << std::endl;
               messages_received[client_id] = true;
               client_ptr->close();
            }
         });

         client_ptr->on_error([client_id](std::error_code ec) {
            std::cerr << "Client " << client_id << " error: " << ec.message() << "\n";
         });

         client_ptr->on_close([](ws_close_code, std::string_view) {});

         std::string client_url = "ws://localhost:" + std::to_string(port) + "/ws";
         client_ptr->connect(client_url);
      }

      // Run shared io_context
      std::thread io_thread([io_ctx]() { io_ctx->run(); });

      // Wait for all clients to receive messages
      bool all_received = wait_for_condition([&] {
         for (int i = 0; i < num_clients; ++i) {
            if (!messages_received[i].load()) return false;
         }
         return true;
      });

      expect(all_received) << "Not all clients received messages";

      // Small delay to ensure clean close
      std::this_thread::sleep_for(std::chrono::milliseconds(500));

      if (!io_ctx->stopped()) {
         io_ctx->stop();
      }

      if (io_thread.joinable()) {
         io_thread.join();
      }

      stop_server = true;
      server_thread.join();
   };

   "thread_safety_test"_test = [] {
      uint16_t port = 8097;
      std::atomic<bool> server_ready{false};
      std::atomic<bool> stop_server{false};
      std::atomic<int> server_message_count{0};

      std::thread server_thread(run_counting_server, std::ref(server_ready), std::ref(stop_server), port,
                                std::ref(server_message_count));

      expect(wait_for_condition([&] { return server_ready.load(); })) << "Server failed to start";

      websocket_client client;
      std::atomic<bool> connected{false};
      std::atomic<int> messages_received{0};
      const int messages_to_send = 20;

      client.on_open([&]() { connected = true; });

      client.on_message([&](std::string_view /*message*/, ws_opcode opcode) {
         if (opcode == ws_opcode::text) {
            messages_received++;
            if (messages_received >= messages_to_send) {
               client.close();
            }
         }
      });

      client.on_error([](std::error_code ec) { std::cerr << "Client Error: " << ec.message() << "\n"; });

      client.on_close([&](ws_close_code, std::string_view) { client.ctx_->stop(); });

      std::string client_url = "ws://localhost:" + std::to_string(port) + "/ws";
      client.connect(client_url);

      std::thread client_thread([&client]() { client.ctx_->run(); });

      // Wait for connection
      expect(wait_for_condition([&] { return connected.load(); })) << "Failed to connect";

      // Send messages from multiple threads
      std::vector<std::thread> sender_threads;
      for (int i = 0; i < 4; ++i) {
         sender_threads.emplace_back([&client, i, messages_to_send]() {
            for (int j = 0; j < messages_to_send / 4; ++j) {
               client.send("Thread " + std::to_string(i) + " message " + std::to_string(j));
               std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
         });
      }

      // Wait for all sender threads
      for (auto& t : sender_threads) {
         t.join();
      }

      // Wait for all responses
      expect(wait_for_condition([&] { return messages_received.load() >= messages_to_send; }))
         << "Did not receive all messages in thread safety test";

      if (!client.ctx_->stopped()) {
         client.ctx_->stop();
      }

      if (client_thread.joinable()) {
         client_thread.join();
      }

      stop_server = true;
      server_thread.join();
   };

   "json_message_exchange_test"_test = [] {
      uint16_t port = 8098;
      std::atomic<bool> server_ready{false};
      std::atomic<bool> stop_server{false};

      // Server that echoes JSON
      http_server server;
      auto ws_server = std::make_shared<websocket_server>();

      ws_server->on_open([](auto /*conn*/, const request&) {});

      ws_server->on_message([](auto conn, std::string_view message, ws_opcode opcode) {
         if (opcode == ws_opcode::text) {
            // Parse and echo back the JSON
            auto msg = glz::read_json<TestMessage>(message);
            if (msg) {
               msg->content = "Echo: " + msg->content;
               auto json_response = glz::write_json(*msg);
               if (json_response) {
                  conn->send_text(*json_response);
               }
            }
         }
      });

      ws_server->on_close([](auto /*conn*/) {});
      ws_server->on_error([](auto /*conn*/, std::error_code /*ec*/) {});

      server.websocket("/ws", ws_server);

      std::thread server_thread([&]() {
         try {
            server.bind(port);
            server.start();
            server_ready = true;

            while (!stop_server) {
               std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            server.stop();
         }
         catch (const std::exception& e) {
            std::cerr << "Server Exception: " << e.what() << "\n";
            server_ready = true;
         }
      });

      expect(wait_for_condition([&] { return server_ready.load(); })) << "Server failed to start";

      websocket_client client;
      std::atomic<bool> json_received{false};

      client.on_open([&client]() {
         TestMessage msg{"greeting", "Hello from Glaze!", std::time(nullptr)};
         auto json = glz::write_json(msg);
         if (json) {
            client.send(*json);
         }
      });

      client.on_message([&](std::string_view message, ws_opcode opcode) {
         if (opcode == ws_opcode::text) {
            auto msg = glz::read_json<TestMessage>(message);
            if (msg && msg->type == "greeting" && msg->content.find("Echo:") != std::string::npos) {
               std::cout << "Received JSON: type=" << msg->type << ", content=" << msg->content << std::endl;
               json_received = true;
               client.close();
            }
         }
      });

      client.on_error([](std::error_code ec) { std::cerr << "Client Error: " << ec.message() << "\n"; });

      client.on_close([&](ws_close_code, std::string_view) { client.ctx_->stop(); });

      std::string client_url = "ws://localhost:" + std::to_string(port) + "/ws";
      client.connect(client_url);

      std::thread client_thread([&client]() { client.ctx_->run(); });

      expect(wait_for_condition([&] { return json_received.load(); })) << "JSON message was not received correctly";

      if (!client.ctx_->stopped()) {
         client.ctx_->stop();
      }

      if (client_thread.joinable()) {
         client_thread.join();
      }

      stop_server = true;
      server_thread.join();
   };

   "empty_message_test"_test = [] {
      uint16_t port = 8099;
      std::atomic<bool> server_ready{false};
      std::atomic<bool> stop_server{false};

      std::thread server_thread(run_echo_server, std::ref(server_ready), std::ref(stop_server), port);

      expect(wait_for_condition([&] { return server_ready.load(); })) << "Server failed to start";

      websocket_client client;
      std::atomic<bool> empty_message_received{false};

      client.on_open([&client]() { client.send(""); });

      client.on_message([&](std::string_view message, ws_opcode opcode) {
         if (opcode == ws_opcode::text && message.find("Echo:") != std::string_view::npos) {
            empty_message_received = true;
            client.close();
         }
      });

      client.on_error([](std::error_code ec) { std::cerr << "Client Error: " << ec.message() << "\n"; });

      client.on_close([&](ws_close_code, std::string_view) { client.ctx_->stop(); });

      std::string client_url = "ws://localhost:" + std::to_string(port) + "/ws";
      client.connect(client_url);

      std::thread client_thread([&client]() { client.ctx_->run(); });

      expect(wait_for_condition([&] { return empty_message_received.load(); })) << "Empty message handling failed";

      if (!client.ctx_->stopped()) {
         client.ctx_->stop();
      }

      if (client_thread.joinable()) {
         client_thread.join();
      }

      stop_server = true;
      server_thread.join();
   };

   "max_message_size_test"_test = [] {
      uint16_t port = 8100;
      std::atomic<bool> server_ready{false};
      std::atomic<bool> stop_server{false};

      std::thread server_thread(run_echo_server, std::ref(server_ready), std::ref(stop_server), port);

      expect(wait_for_condition([&] { return server_ready.load(); })) << "Server failed to start";

      websocket_client client;
      client.set_max_message_size(1024); // 1KB limit
      std::atomic<bool> small_message_received{false};

      client.on_open([&client]() {
         // Send a message smaller than the limit
         std::string small_msg(512, 'X');
         client.send(small_msg);
      });

      client.on_message([&](std::string_view message, ws_opcode opcode) {
         if (opcode == ws_opcode::text && message.size() > 0) {
            small_message_received = true;
            client.close();
         }
      });

      client.on_error([](std::error_code ec) { std::cerr << "Client Error: " << ec.message() << "\n"; });

      client.on_close([&](ws_close_code, std::string_view) { client.ctx_->stop(); });

      std::string client_url = "ws://localhost:" + std::to_string(port) + "/ws";
      client.connect(client_url);

      std::thread client_thread([&client]() { client.ctx_->run(); });

      expect(wait_for_condition([&] { return small_message_received.load(); }))
         << "Small message within limit was not received";

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
