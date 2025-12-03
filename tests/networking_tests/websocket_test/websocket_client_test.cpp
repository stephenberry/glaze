#include "glaze/net/websocket_client.hpp"

#include <atomic>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

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

   ws_server->on_close([](auto /*conn*/, ws_close_code /*code*/, std::string_view /*reason*/) {});

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

   ws_server->on_close([](auto /*conn*/, ws_close_code /*code*/, std::string_view /*reason*/) {});

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

   ws_server->on_message([&message_count](auto conn, std::string_view /*message*/, ws_opcode opcode) {
      if (opcode == ws_opcode::text) {
         message_count++;
         conn->send_text(std::string("Message ") + std::to_string(message_count.load()));
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
         std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
      server.stop();
   }
   catch (const std::exception& e) {
      std::cerr << "Server Exception: " << e.what() << "\n";
      server_ready = true;
   }
}

struct TestMessage
{
   std::string type;
   std::string content;
   int64_t timestamp;
};

template <>
struct glz::meta<TestMessage>
{
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
         std::cerr << "Client Error: " << ec.message() << " (code=" << ec.value()
                   << ", category=" << ec.category().name() << ")\n";
      });

      client.on_close([&](ws_close_code, std::string_view) {
         connection_closed = true;
         client.context()->stop();
      });

      std::string client_url = "ws://localhost:" + std::to_string(port) + "/ws";
      client.connect(client_url);

      std::thread client_thread([&client]() { client.context()->run(); });

      expect(wait_for_condition([&] { return message_received.load(); })) << "Message was not received/echoed";
      expect(wait_for_condition([&] { return connection_closed.load(); })) << "Connection was not closed gracefully";

      if (!client.context()->stopped()) {
         client.context()->stop();
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

      client.on_message([&](std::string_view /*message*/, ws_opcode opcode) {
         if (opcode == ws_opcode::text) {
            messages_received++;
            if (messages_received >= expected_messages) {
               client.close();
            }
         }
      });

      client.on_error([](std::error_code ec) { std::cerr << "Client Error: " << ec.message() << "\n"; });

      client.on_close([&](ws_close_code, std::string_view) { client.context()->stop(); });

      std::string client_url = "ws://localhost:" + std::to_string(port) + "/ws";
      client.connect(client_url);

      std::thread client_thread([&client]() { client.context()->run(); });

      expect(wait_for_condition([&] { return messages_received.load() >= expected_messages; }))
         << "Did not receive all messages";

      if (!client.context()->stopped()) {
         client.context()->stop();
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

      client.on_close([&](ws_close_code, std::string_view) { client.context()->stop(); });

      std::string client_url = "ws://localhost:" + std::to_string(port) + "/ws";
      client.connect(client_url);

      std::thread client_thread([&client]() { client.context()->run(); });

      expect(wait_for_condition([&] { return binary_received.load(); })) << "Binary message was not received";

      if (!client.context()->stopped()) {
         client.context()->stop();
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
         std::cout << "[large_message_test] Received " << message.size()
                   << " bytes, opcode=" << static_cast<int>(opcode) << std::endl;
         if (opcode == ws_opcode::text && message.size() > 256 * 1024 &&
             message.find("END") != std::string_view::npos) {
            std::cout << "[large_message_test] Large message received successfully" << std::endl;
            large_message_received = true;
            client.close();
         }
         else {
            std::cout << "[large_message_test] Message didn't match criteria (size=" << message.size()
                      << ", has_END=" << (message.find("END") != std::string_view::npos) << ")" << std::endl;
         }
      });

      client.on_error([](std::error_code ec) {
         std::cerr << "[large_message_test] Client Error: " << ec.message() << " (code=" << ec.value() << ")\n";
      });

      client.on_close([&](ws_close_code, std::string_view) { client.context()->stop(); });

      std::string client_url = "ws://localhost:" + std::to_string(port) + "/ws";
      client.connect(client_url);

      std::thread client_thread([&client]() { client.context()->run(); });

      bool received =
         wait_for_condition([&] { return large_message_received.load(); }, std::chrono::milliseconds(60000));

      if (!received) {
         std::cerr << "[large_message_test] Test failed - connection_opened=" << connection_opened.load()
                   << ", large_message_received=" << large_message_received.load() << std::endl;
      }

      expect(received) << "Large message was not received";

      if (!client.context()->stopped()) {
         client.context()->stop();
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
         client.context()->stop();
      });

      // Try to connect to a port with no server
      client.connect("ws://localhost:9999/ws");

      std::thread client_thread([&client]() { client.context()->run(); });

      expect(wait_for_condition([&] { return error_received.load(); })) << "Expected connection error";

      if (!client.context()->stopped()) {
         client.context()->stop();
      }

      if (client_thread.joinable()) {
         client_thread.join();
      }
   };

   "invalid_url_test"_test = [] {
      websocket_client client;
      std::atomic<bool> error_received{false};

      client.on_error([&](std::error_code) {
         error_received = true;
         if (!client.context()->stopped()) {
            client.context()->stop();
         }
      });

      // Invalid URL
      client.connect("not-a-valid-url");

      std::thread client_thread([&client]() { client.context()->run(); });

      expect(wait_for_condition([&] { return error_received.load(); })) << "Expected URL parse error";

      if (!client.context()->stopped()) {
         client.context()->stop();
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

      client.on_close([&](ws_close_code code, std::string_view) {
         std::cout << "Connection closed with code: " << static_cast<int>(code) << std::endl;
         connection_closed = true;
         if (code == ws_close_code::normal) {
            close_code_correct = true;
         }
         client.context()->stop();
      });

      client.on_error([](std::error_code ec) { std::cerr << "Client Error: " << ec.message() << "\n"; });

      std::string client_url = "ws://localhost:" + std::to_string(port) + "/ws";
      client.connect(client_url);

      std::thread client_thread([&client]() { client.context()->run(); });

      expect(wait_for_condition([&] { return connection_closed.load(); })) << "Connection was not closed by server";
      expect(close_code_correct.load()) << "Close code or reason was incorrect";

      if (!client.context()->stopped()) {
         client.context()->stop();
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

         client_ptr->on_open(
            [client_ptr, client_id]() { client_ptr->send("Hello from client " + std::to_string(client_id)); });

         client_ptr->on_message(
            [&messages_received, client_ptr, client_id](std::string_view message, ws_opcode opcode) {
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

      client.on_close([&](ws_close_code, std::string_view) { client.context()->stop(); });

      std::string client_url = "ws://localhost:" + std::to_string(port) + "/ws";
      client.connect(client_url);

      std::thread client_thread([&client]() { client.context()->run(); });

      // Wait for connection
      expect(wait_for_condition([&] { return connected.load(); })) << "Failed to connect";

      // Send messages from multiple threads
      std::vector<std::thread> sender_threads;
      for (int i = 0; i < 4; ++i) {
         sender_threads.emplace_back([&client, i]() {
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

      if (!client.context()->stopped()) {
         client.context()->stop();
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

      ws_server->on_close([](auto /*conn*/, ws_close_code /*code*/, std::string_view /*reason*/) {});
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

      client.on_close([&](ws_close_code, std::string_view) { client.context()->stop(); });

      std::string client_url = "ws://localhost:" + std::to_string(port) + "/ws";
      client.connect(client_url);

      std::thread client_thread([&client]() { client.context()->run(); });

      expect(wait_for_condition([&] { return json_received.load(); })) << "JSON message was not received correctly";

      if (!client.context()->stopped()) {
         client.context()->stop();
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

      client.on_close([&](ws_close_code, std::string_view) { client.context()->stop(); });

      std::string client_url = "ws://localhost:" + std::to_string(port) + "/ws";
      client.connect(client_url);

      std::thread client_thread([&client]() { client.context()->run(); });

      expect(wait_for_condition([&] { return empty_message_received.load(); })) << "Empty message handling failed";

      if (!client.context()->stopped()) {
         client.context()->stop();
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

      client.on_close([&](ws_close_code, std::string_view) { client.context()->stop(); });

      std::string client_url = "ws://localhost:" + std::to_string(port) + "/ws";
      client.connect(client_url);

      std::thread client_thread([&client]() { client.context()->run(); });

      expect(wait_for_condition([&] { return small_message_received.load(); }))
         << "Small message within limit was not received";

      if (!client.context()->stopped()) {
         client.context()->stop();
      }

      if (client_thread.joinable()) {
         client_thread.join();
      }

      stop_server = true;
      server_thread.join();
   };
};

// =============================================================================
// Tests for write queue fix (GitHub issue #2089 - WebSocket message corruption)
// These tests verify that rapid/concurrent message sending doesn't cause
// frame corruption due to interleaved async_write operations.
// =============================================================================

// Helper server that validates message integrity and echoes with sequence number
void run_integrity_check_server(std::atomic<bool>& server_ready, std::atomic<bool>& should_stop, uint16_t port,
                                std::atomic<int>& valid_messages, std::atomic<int>& invalid_messages)
{
   http_server server;
   auto ws_server = std::make_shared<websocket_server>();
   std::atomic<int> seq{0};

   ws_server->on_open([](auto /*conn*/, const request&) {});

   ws_server->on_message([&](auto conn, std::string_view message, ws_opcode opcode) {
      if (opcode == ws_opcode::text) {
         // Validate message format: should start with "MSG:" and contain only valid chars
         bool valid = message.size() >= 4 && message.substr(0, 4) == "MSG:";
         if (valid) {
            // Check for any binary garbage that would indicate frame corruption
            for (char c : message) {
               if (c < 32 && c != '\n' && c != '\r' && c != '\t') {
                  valid = false;
                  break;
               }
            }
         }

         if (valid) {
            valid_messages++;
            int current_seq = seq++;
            conn->send_text("ACK:" + std::to_string(current_seq) + ":" + std::string(message.substr(4)));
         }
         else {
            invalid_messages++;
            std::cerr << "[integrity_server] Invalid message detected! Size=" << message.size() << std::endl;
            // Print hex dump of first 64 bytes for debugging
            std::cerr << "[integrity_server] Hex dump: ";
            for (size_t i = 0; i < std::min(message.size(), size_t(64)); ++i) {
               std::cerr << std::hex << std::setw(2) << std::setfill('0') << (int)(unsigned char)message[i] << " ";
            }
            std::cerr << std::dec << std::endl;
         }
      }
   });

   ws_server->on_close([](auto /*conn*/, ws_close_code /*code*/, std::string_view /*reason*/) {});
   ws_server->on_error([](auto /*conn*/, std::error_code ec) {
      std::cerr << "[integrity_server] Error: " << ec.message() << std::endl;
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
      std::cerr << "[integrity_server] Exception: " << e.what() << "\n";
      server_ready = true;
   }
}

// Helper server that broadcasts messages rapidly to all connected clients
void run_broadcast_server(std::atomic<bool>& server_ready, std::atomic<bool>& should_stop, uint16_t port,
                          std::atomic<bool>& start_broadcast, int broadcast_count)
{
   http_server server;
   auto ws_server = std::make_shared<websocket_server>();
   std::vector<std::shared_ptr<websocket_connection<asio::ip::tcp::socket>>> connections;
   std::mutex conn_mutex;

   ws_server->on_open([&](auto conn, const request&) {
      std::lock_guard<std::mutex> lock(conn_mutex);
      connections.push_back(conn);
   });

   ws_server->on_message([&](auto /*conn*/, std::string_view message, ws_opcode opcode) {
      if (opcode == ws_opcode::text && message == "START_BROADCAST") {
         start_broadcast = true;
      }
   });

   ws_server->on_close([&](auto conn, ws_close_code /*code*/, std::string_view /*reason*/) {
      std::lock_guard<std::mutex> lock(conn_mutex);
      connections.erase(std::remove(connections.begin(), connections.end(), conn), connections.end());
   });

   ws_server->on_error([](auto /*conn*/, std::error_code /*ec*/) {});

   server.websocket("/ws", ws_server);

   try {
      server.bind(port);
      server.start();
      server_ready = true;

      // Wait for broadcast signal
      while (!start_broadcast && !should_stop) {
         std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }

      // Rapid-fire broadcast
      if (start_broadcast && !should_stop) {
         std::lock_guard<std::mutex> lock(conn_mutex);
         for (int i = 0; i < broadcast_count; ++i) {
            std::string msg = "BROADCAST:" + std::to_string(i) + ":";
            msg += std::string(100, 'X'); // Add some payload
            for (auto& conn : connections) {
               conn->send_text(msg);
            }
         }
      }

      while (!should_stop) {
         std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
      server.stop();
   }
   catch (const std::exception& e) {
      std::cerr << "[broadcast_server] Exception: " << e.what() << "\n";
      server_ready = true;
   }
}

suite websocket_write_queue_tests = [] {
   // Test: Rapid fire messages from a single thread (no delays)
   "rapid_fire_single_thread_test"_test = [] {
      uint16_t port = 8110;
      std::atomic<bool> server_ready{false};
      std::atomic<bool> stop_server{false};
      std::atomic<int> valid_messages{0};
      std::atomic<int> invalid_messages{0};

      std::thread server_thread(run_integrity_check_server, std::ref(server_ready), std::ref(stop_server), port,
                                std::ref(valid_messages), std::ref(invalid_messages));

      expect(wait_for_condition([&] { return server_ready.load(); })) << "Server failed to start";

      websocket_client client;
      std::atomic<bool> connected{false};
      std::atomic<int> acks_received{0};
      const int messages_to_send = 100;

      client.on_open([&]() {
         connected = true;
         // Send all messages as fast as possible (no delays!)
         for (int i = 0; i < messages_to_send; ++i) {
            client.send("MSG:" + std::to_string(i) + ":payload_data_" + std::to_string(i));
         }
      });

      client.on_message([&](std::string_view message, ws_opcode opcode) {
         if (opcode == ws_opcode::text && message.substr(0, 4) == "ACK:") {
            acks_received++;
            if (acks_received >= messages_to_send) {
               client.close();
            }
         }
      });

      client.on_error(
         [](std::error_code ec) { std::cerr << "[rapid_fire_test] Client Error: " << ec.message() << "\n"; });

      client.on_close([&](ws_close_code, std::string_view) { client.context()->stop(); });

      std::string client_url = "ws://localhost:" + std::to_string(port) + "/ws";
      client.connect(client_url);

      std::thread client_thread([&client]() { client.context()->run(); });

      bool success =
         wait_for_condition([&] { return acks_received.load() >= messages_to_send; }, std::chrono::milliseconds(10000));

      expect(success) << "Did not receive all ACKs (got " << acks_received.load() << "/" << messages_to_send << ")";
      expect(invalid_messages.load() == 0) << "Server detected " << invalid_messages.load() << " corrupted messages!";
      expect(valid_messages.load() == messages_to_send)
         << "Server received " << valid_messages.load() << "/" << messages_to_send << " valid messages";

      if (!client.context()->stopped()) {
         client.context()->stop();
      }

      if (client_thread.joinable()) {
         client_thread.join();
      }

      stop_server = true;
      server_thread.join();
   };

   // Test: Concurrent message sending from multiple threads (the key race condition test)
   "concurrent_multi_thread_send_test"_test = [] {
      uint16_t port = 8111;
      std::atomic<bool> server_ready{false};
      std::atomic<bool> stop_server{false};
      std::atomic<int> valid_messages{0};
      std::atomic<int> invalid_messages{0};

      std::thread server_thread(run_integrity_check_server, std::ref(server_ready), std::ref(stop_server), port,
                                std::ref(valid_messages), std::ref(invalid_messages));

      expect(wait_for_condition([&] { return server_ready.load(); })) << "Server failed to start";

      websocket_client client;
      std::atomic<bool> connected{false};
      std::atomic<int> acks_received{0};
      const int threads_count = 8;
      const int messages_per_thread = 25;
      const int total_messages = threads_count * messages_per_thread;

      client.on_open([&]() { connected = true; });

      client.on_message([&](std::string_view message, ws_opcode opcode) {
         if (opcode == ws_opcode::text && message.substr(0, 4) == "ACK:") {
            int current = ++acks_received;
            if (current >= total_messages) {
               client.close();
            }
         }
      });

      client.on_error(
         [](std::error_code ec) { std::cerr << "[concurrent_test] Client Error: " << ec.message() << "\n"; });

      client.on_close([&](ws_close_code, std::string_view) { client.context()->stop(); });

      std::string client_url = "ws://localhost:" + std::to_string(port) + "/ws";
      client.connect(client_url);

      std::thread client_thread([&client]() { client.context()->run(); });

      // Wait for connection
      expect(wait_for_condition([&] { return connected.load(); })) << "Failed to connect";

      // Launch multiple threads all sending simultaneously
      std::vector<std::thread> sender_threads;
      std::atomic<bool> start_flag{false};

      for (int t = 0; t < threads_count; ++t) {
         sender_threads.emplace_back([&client, &start_flag, t]() {
            // Wait for all threads to be ready
            while (!start_flag.load()) {
               std::this_thread::yield();
            }
            // Send messages without any delays
            for (int i = 0; i < messages_per_thread; ++i) {
               client.send("MSG:T" + std::to_string(t) + "_M" + std::to_string(i) + ":data");
            }
         });
      }

      // Start all threads simultaneously
      start_flag = true;

      // Wait for all sender threads to complete
      for (auto& t : sender_threads) {
         t.join();
      }

      // Wait for all responses
      bool success =
         wait_for_condition([&] { return acks_received.load() >= total_messages; }, std::chrono::milliseconds(15000));

      std::cout << "[concurrent_test] Received " << acks_received.load() << "/" << total_messages << " ACKs"
                << std::endl;
      std::cout << "[concurrent_test] Server: valid=" << valid_messages.load()
                << ", invalid=" << invalid_messages.load() << std::endl;

      expect(success) << "Did not receive all ACKs (got " << acks_received.load() << "/" << total_messages << ")";
      expect(invalid_messages.load() == 0) << "CRITICAL: Server detected " << invalid_messages.load()
                                           << " corrupted messages! This indicates the write queue fix is not working.";

      if (!client.context()->stopped()) {
         client.context()->stop();
      }

      if (client_thread.joinable()) {
         client_thread.join();
      }

      stop_server = true;
      server_thread.join();
   };

   // Test: Server-side rapid broadcasting (tests server write queue)
   "server_broadcast_stress_test"_test = [] {
      uint16_t port = 8112;
      std::atomic<bool> server_ready{false};
      std::atomic<bool> stop_server{false};
      std::atomic<bool> start_broadcast{false};
      const int broadcast_count = 100;

      std::thread server_thread(run_broadcast_server, std::ref(server_ready), std::ref(stop_server), port,
                                std::ref(start_broadcast), broadcast_count);

      expect(wait_for_condition([&] { return server_ready.load(); })) << "Server failed to start";

      websocket_client client;
      std::atomic<bool> connected{false};
      std::atomic<int> messages_received{0};
      std::atomic<int> valid_broadcasts{0};
      std::atomic<int> invalid_broadcasts{0};

      client.on_open([&]() {
         connected = true;
         client.send("START_BROADCAST");
      });

      client.on_message([&](std::string_view message, ws_opcode opcode) {
         if (opcode == ws_opcode::text) {
            messages_received++;
            // Validate message format
            if (message.size() >= 10 && message.substr(0, 10) == "BROADCAST:") {
               // Check for corruption
               bool valid = true;
               for (char c : message) {
                  if (c < 32 && c != '\n' && c != '\r' && c != '\t') {
                     valid = false;
                     break;
                  }
               }
               if (valid) {
                  valid_broadcasts++;
               }
               else {
                  invalid_broadcasts++;
                  std::cerr << "[broadcast_test] Corrupted message detected!" << std::endl;
               }
            }
            else {
               invalid_broadcasts++;
            }

            if (messages_received >= broadcast_count) {
               client.close();
            }
         }
      });

      client.on_error(
         [](std::error_code ec) { std::cerr << "[broadcast_test] Client Error: " << ec.message() << "\n"; });

      client.on_close([&](ws_close_code, std::string_view) { client.context()->stop(); });

      std::string client_url = "ws://localhost:" + std::to_string(port) + "/ws";
      client.connect(client_url);

      std::thread client_thread([&client]() { client.context()->run(); });

      bool success = wait_for_condition([&] { return messages_received.load() >= broadcast_count; },
                                        std::chrono::milliseconds(10000));

      std::cout << "[broadcast_test] Received " << messages_received.load() << "/" << broadcast_count
                << " messages (valid=" << valid_broadcasts.load() << ", invalid=" << invalid_broadcasts.load() << ")"
                << std::endl;

      expect(success) << "Did not receive all broadcast messages";
      expect(invalid_broadcasts.load() == 0)
         << "CRITICAL: Detected " << invalid_broadcasts.load() << " corrupted broadcast messages!";

      if (!client.context()->stopped()) {
         client.context()->stop();
      }

      if (client_thread.joinable()) {
         client_thread.join();
      }

      stop_server = true;
      server_thread.join();
   };

   // Test: Mixed message sizes with concurrent sends
   "mixed_size_concurrent_test"_test = [] {
      uint16_t port = 8113;
      std::atomic<bool> server_ready{false};
      std::atomic<bool> stop_server{false};
      std::atomic<int> valid_messages{0};
      std::atomic<int> invalid_messages{0};

      std::thread server_thread(run_integrity_check_server, std::ref(server_ready), std::ref(stop_server), port,
                                std::ref(valid_messages), std::ref(invalid_messages));

      expect(wait_for_condition([&] { return server_ready.load(); })) << "Server failed to start";

      websocket_client client;
      std::atomic<bool> connected{false};
      std::atomic<int> acks_received{0};
      const int threads_count = 4;
      const int messages_per_thread = 20;
      const int total_messages = threads_count * messages_per_thread;

      client.on_open([&]() { connected = true; });

      client.on_message([&](std::string_view message, ws_opcode opcode) {
         if (opcode == ws_opcode::text && message.substr(0, 4) == "ACK:") {
            int current = ++acks_received;
            if (current >= total_messages) {
               client.close();
            }
         }
      });

      client.on_error(
         [](std::error_code ec) { std::cerr << "[mixed_size_test] Client Error: " << ec.message() << "\n"; });

      client.on_close([&](ws_close_code, std::string_view) { client.context()->stop(); });

      std::string client_url = "ws://localhost:" + std::to_string(port) + "/ws";
      client.connect(client_url);

      std::thread client_thread([&client]() { client.context()->run(); });

      expect(wait_for_condition([&] { return connected.load(); })) << "Failed to connect";

      std::vector<std::thread> sender_threads;
      std::atomic<bool> start_flag{false};

      // Different sized payloads for each thread
      std::vector<size_t> payload_sizes = {10, 100, 1000, 10000}; // 10B to 10KB

      for (int t = 0; t < threads_count; ++t) {
         sender_threads.emplace_back([&client, &start_flag, t, &payload_sizes]() {
            while (!start_flag.load()) {
               std::this_thread::yield();
            }
            size_t payload_size = payload_sizes[t % payload_sizes.size()];
            std::string payload(payload_size, 'A' + (t % 26));
            for (int i = 0; i < messages_per_thread; ++i) {
               client.send("MSG:T" + std::to_string(t) + "_M" + std::to_string(i) + ":" + payload);
            }
         });
      }

      start_flag = true;

      for (auto& t : sender_threads) {
         t.join();
      }

      bool success =
         wait_for_condition([&] { return acks_received.load() >= total_messages; }, std::chrono::milliseconds(15000));

      std::cout << "[mixed_size_test] Received " << acks_received.load() << "/" << total_messages << " ACKs"
                << std::endl;
      std::cout << "[mixed_size_test] Server: valid=" << valid_messages.load()
                << ", invalid=" << invalid_messages.load() << std::endl;

      expect(success) << "Did not receive all ACKs";
      expect(invalid_messages.load() == 0) << "Detected corrupted messages with mixed sizes!";

      if (!client.context()->stopped()) {
         client.context()->stop();
      }

      if (client_thread.joinable()) {
         client_thread.join();
      }

      stop_server = true;
      server_thread.join();
   };

   // Test: Concurrent send and close - exercises close_after_write_ race condition fix
   "concurrent_send_and_close_test"_test = [] {
      uint16_t port = 8115;
      std::atomic<bool> server_ready{false};
      std::atomic<bool> stop_server{false};
      std::atomic<int> valid_messages{0};
      std::atomic<int> invalid_messages{0};

      std::thread server_thread(run_integrity_check_server, std::ref(server_ready), std::ref(stop_server), port,
                                std::ref(valid_messages), std::ref(invalid_messages));

      expect(wait_for_condition([&] { return server_ready.load(); })) << "Server failed to start";

      websocket_client client;
      std::atomic<bool> connected{false};
      std::atomic<bool> closed{false};
      std::atomic<int> acks_received{0};

      client.on_open([&]() { connected = true; });

      client.on_message([&](std::string_view message, ws_opcode opcode) {
         if (opcode == ws_opcode::text && message.size() >= 4 && message.substr(0, 4) == "ACK:") {
            acks_received++;
         }
      });

      client.on_error(
         [](std::error_code ec) { std::cerr << "[concurrent_close_test] Client Error: " << ec.message() << "\n"; });

      client.on_close([&](ws_close_code code, std::string_view) {
         std::cout << "[concurrent_close_test] Connection closed with code: " << static_cast<int>(code) << std::endl;
         closed = true;
         client.context()->stop();
      });

      std::string client_url = "ws://localhost:" + std::to_string(port) + "/ws";
      client.connect(client_url);

      std::thread client_thread([&client]() { client.context()->run(); });

      expect(wait_for_condition([&] { return connected.load(); })) << "Failed to connect";

      // Launch multiple threads: some sending, one closing
      std::vector<std::thread> threads;
      std::atomic<bool> start_flag{false};
      const int messages_per_thread = 10;
      const int sender_threads = 4;

      // Sender threads
      for (int t = 0; t < sender_threads; ++t) {
         threads.emplace_back([&client, &start_flag, t]() {
            while (!start_flag.load()) {
               std::this_thread::yield();
            }
            for (int i = 0; i < messages_per_thread; ++i) {
               client.send("MSG:T" + std::to_string(t) + "_M" + std::to_string(i) + ":data");
            }
         });
      }

      // Closer thread - waits a tiny bit then closes
      threads.emplace_back([&client, &start_flag]() {
         while (!start_flag.load()) {
            std::this_thread::yield();
         }
         // Small delay to let some messages queue
         std::this_thread::sleep_for(std::chrono::microseconds(100));
         client.close();
      });

      // Start all threads simultaneously
      start_flag = true;

      for (auto& t : threads) {
         t.join();
      }

      // Wait for close to complete
      bool close_success = wait_for_condition([&] { return closed.load(); }, std::chrono::milliseconds(5000));

      std::cout << "[concurrent_close_test] Received " << acks_received.load() << " ACKs before close" << std::endl;
      std::cout << "[concurrent_close_test] Server: valid=" << valid_messages.load()
                << ", invalid=" << invalid_messages.load() << std::endl;

      expect(close_success) << "Connection did not close gracefully";
      expect(invalid_messages.load() == 0) << "Detected corrupted messages during concurrent send/close!";

      if (!client.context()->stopped()) {
         client.context()->stop();
      }

      if (client_thread.joinable()) {
         client_thread.join();
      }

      stop_server = true;
      server_thread.join();
   };

   // Test: Stress test with many messages to ensure queue doesn't leak/corrupt under load
   "high_volume_stress_test"_test = [] {
      uint16_t port = 8114;
      std::atomic<bool> server_ready{false};
      std::atomic<bool> stop_server{false};
      std::atomic<int> valid_messages{0};
      std::atomic<int> invalid_messages{0};

      std::thread server_thread(run_integrity_check_server, std::ref(server_ready), std::ref(stop_server), port,
                                std::ref(valid_messages), std::ref(invalid_messages));

      expect(wait_for_condition([&] { return server_ready.load(); })) << "Server failed to start";

      websocket_client client;
      std::atomic<bool> connected{false};
      std::atomic<int> acks_received{0};
      const int total_messages = 500;

      client.on_open([&]() { connected = true; });

      client.on_message([&](std::string_view message, ws_opcode opcode) {
         if (opcode == ws_opcode::text && message.size() >= 4 && message.substr(0, 4) == "ACK:") {
            int current = ++acks_received;
            if (current >= total_messages) {
               client.close();
            }
         }
      });

      client.on_error([](std::error_code ec) { std::cerr << "[stress_test] Client Error: " << ec.message() << "\n"; });

      client.on_close([&](ws_close_code, std::string_view) { client.context()->stop(); });

      std::string client_url = "ws://localhost:" + std::to_string(port) + "/ws";
      client.connect(client_url);

      std::thread client_thread([&client]() { client.context()->run(); });

      expect(wait_for_condition([&] { return connected.load(); })) << "Failed to connect";

      // Send all messages as fast as possible
      for (int i = 0; i < total_messages; ++i) {
         client.send("MSG:" + std::to_string(i) + ":stress_test_payload_" + std::to_string(i));
      }

      bool success =
         wait_for_condition([&] { return acks_received.load() >= total_messages; }, std::chrono::milliseconds(30000));

      std::cout << "[stress_test] Received " << acks_received.load() << "/" << total_messages << " ACKs" << std::endl;
      std::cout << "[stress_test] Server: valid=" << valid_messages.load() << ", invalid=" << invalid_messages.load()
                << std::endl;

      expect(success) << "Did not receive all ACKs in stress test";
      expect(invalid_messages.load() == 0) << "Detected corrupted messages under high load!";

      if (!client.context()->stopped()) {
         client.context()->stop();
      }

      if (client_thread.joinable()) {
         client_thread.join();
      }

      stop_server = true;
      server_thread.join();
   };
};

int main() {}
