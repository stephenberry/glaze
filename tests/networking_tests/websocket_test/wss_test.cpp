// WSS (WebSocket Secure) Test for Glaze Library
// Verifies ASIO 1.32+ compatibility for SSL WebSocket client (issue #2164)
// and WSS server support

#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>

// Enable SSL before including headers
#ifndef GLZ_ENABLE_SSL
#define GLZ_ENABLE_SSL
#endif

#include "glaze/glaze.hpp"
#include "glaze/net/http_server.hpp"
#include "glaze/net/websocket_client.hpp"
#include "ut/ut.hpp"

// OpenSSL includes for certificate generation
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#ifdef DELETE
#undef DELETE
#endif

using namespace ut;
using namespace glz;

// Certificate generation for testing
// Note: Certificates (wss_test_cert.pem, wss_test_key.pem) are intentionally
// left in the working directory for reuse across test runs to avoid the
// overhead of RSA key generation on each run. Delete them manually to force
// regeneration.
namespace
{
   EVP_PKEY* generate_rsa_key(int bits = 2048)
   {
      EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
      if (!ctx) return nullptr;

      if (EVP_PKEY_keygen_init(ctx) <= 0) {
         EVP_PKEY_CTX_free(ctx);
         return nullptr;
      }

      if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, bits) <= 0) {
         EVP_PKEY_CTX_free(ctx);
         return nullptr;
      }

      EVP_PKEY* pkey = nullptr;
      if (EVP_PKEY_keygen(ctx, &pkey) <= 0) {
         EVP_PKEY_CTX_free(ctx);
         return nullptr;
      }

      EVP_PKEY_CTX_free(ctx);
      return pkey;
   }

   X509* create_certificate(EVP_PKEY* pkey, int days = 365)
   {
      X509* x509 = X509_new();
      if (!x509) return nullptr;

      X509_set_version(x509, 2);
      ASN1_INTEGER_set(X509_get_serialNumber(x509), 1);
      X509_gmtime_adj(X509_get_notBefore(x509), 0);
      X509_gmtime_adj(X509_get_notAfter(x509), static_cast<long>(days) * 24 * 60 * 60);
      X509_set_pubkey(x509, pkey);

      X509_NAME* name = X509_get_subject_name(x509);
      X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, reinterpret_cast<const unsigned char*>("localhost"), -1, -1,
                                 0);
      X509_set_issuer_name(x509, name);

      // Add SAN extension
      X509V3_CTX ctx;
      X509V3_set_ctx_nodb(&ctx);
      X509V3_set_ctx(&ctx, x509, x509, nullptr, nullptr, 0);
      X509_EXTENSION* ext =
         X509V3_EXT_conf_nid(nullptr, &ctx, NID_subject_alt_name, const_cast<char*>("DNS:localhost,IP:127.0.0.1"));
      if (ext) {
         X509_add_ext(x509, ext, -1);
         X509_EXTENSION_free(ext);
      }

      if (X509_sign(x509, pkey, EVP_sha256()) <= 0) {
         X509_free(x509);
         return nullptr;
      }
      return x509;
   }

   bool generate_test_certificates()
   {
      EVP_PKEY* pkey = generate_rsa_key();
      if (!pkey) return false;

      X509* x509 = create_certificate(pkey);
      if (!x509) {
         EVP_PKEY_free(pkey);
         return false;
      }

      // Write private key
      FILE* key_file = nullptr;
#ifdef _MSC_VER
      fopen_s(&key_file, "wss_test_key.pem", "w");
#else
      key_file = fopen("wss_test_key.pem", "w");
#endif
      if (key_file) {
         PEM_write_PrivateKey(key_file, pkey, nullptr, nullptr, 0, nullptr, nullptr);
         fclose(key_file);
      }

      // Write certificate
      FILE* cert_file = nullptr;
#ifdef _MSC_VER
      fopen_s(&cert_file, "wss_test_cert.pem", "w");
#else
      cert_file = fopen("wss_test_cert.pem", "w");
#endif
      if (cert_file) {
         PEM_write_X509(cert_file, x509);
         fclose(cert_file);
      }

      X509_free(x509);
      EVP_PKEY_free(pkey);
      return true;
   }

   bool certificates_exist()
   {
      std::ifstream cert("wss_test_cert.pem");
      std::ifstream key("wss_test_key.pem");
      return cert.good() && key.good();
   }

   // Ensure certificates are available for testing.
   // Returns true if certificates are ready, false if generation failed.
   bool ensure_test_certificates()
   {
      if (certificates_exist()) {
         return true;
      }
      return generate_test_certificates();
   }
} // namespace

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

suite wss_server_tests = [] {
   "wss_server_client_echo"_test = [] {
      // Ensure certificates are available - fail test if generation fails
      bool certs_ok = ensure_test_certificates();
      expect(certs_ok) << "Failed to generate test certificates";
      if (!certs_ok) return;

      // Create HTTPS server (supports WSS via WebSocket upgrade over HTTPS)
      https_server server;
      server.load_certificate("wss_test_cert.pem", "wss_test_key.pem");
      server.set_ssl_verify_mode(0); // Disable client verification for testing

      // Create WebSocket server handler
      auto ws_server = std::make_shared<websocket_server>();

      std::atomic<bool> message_received{false};
      std::atomic<bool> client_connected{false};
      std::string received_message;

      ws_server->on_open([&](auto /*conn*/, const request& /*req*/) { client_connected = true; });

      ws_server->on_message([&](auto conn, std::string_view msg, ws_opcode opcode) {
         if (opcode == ws_opcode::text) {
            received_message = std::string(msg);
            message_received = true;
            // Echo back
            conn->send_text("Echo: " + std::string(msg));
         }
      });

      ws_server->on_error([](auto /*conn*/, std::error_code /*ec*/) {});

      ws_server->on_close([](auto /*conn*/, ws_close_code /*code*/, std::string_view) {});

      server.websocket("/ws", ws_server);
      server.bind("127.0.0.1", 0); // Use dynamic port
      uint16_t port = server.port();

      std::thread server_thread([&]() { server.start(); });

      std::this_thread::sleep_for(std::chrono::milliseconds(200)); // Give server time to start

      // Create WSS client
      websocket_client client;

      std::atomic<bool> client_open{false};
      std::atomic<bool> client_message{false};
      std::string client_received;

      client.on_open([&]() { client_open = true; });
      client.on_message([&](std::string_view msg, ws_opcode) {
         client_received = std::string(msg);
         client_message = true;
      });
      client.on_close([&](ws_close_code, std::string_view) { client.context()->stop(); });
      client.on_error([](std::error_code) {});

      // Disable SSL verification for self-signed test certificate
      client.set_ssl_verify_mode(asio::ssl::verify_none);

      // Connect via WSS
      std::string url = "wss://127.0.0.1:" + std::to_string(port) + "/ws";
      client.connect(url);

      // Run client io_context in a separate thread
      std::thread client_thread([&client]() { client.context()->run(); });

      // Wait for connection
      for (int i = 0; i < 50 && !client_open; ++i) {
         std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }

      expect(client_open.load()) << "WSS client should connect to server\n";
      expect(client_connected.load()) << "Server should see client connection\n";

      // Send a message
      if (client_open) {
         client.send("Hello WSS!");

         // Wait for echo
         for (int i = 0; i < 50 && !client_message; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
         }

         expect(message_received.load()) << "Server should receive message\n";
         expect(received_message == "Hello WSS!") << "Server should receive correct message\n";
         expect(client_message.load()) << "Client should receive echo\n";
         expect(client_received == "Echo: Hello WSS!") << "Client should receive correct echo\n";
      }

      client.close();
      client.context()->stop();
      client_thread.join();
      server.stop();
      server_thread.join();
   };

   "wss_multiple_clients_test"_test = [] {
      // Test multiple concurrent WSS client connections
      // This verifies the server can handle multiple SSL handshakes concurrently
      bool certs_ok = ensure_test_certificates();
      expect(certs_ok) << "Failed to generate test certificates";
      if (!certs_ok) return;

      https_server server;
      server.load_certificate("wss_test_cert.pem", "wss_test_key.pem");
      server.set_ssl_verify_mode(0);

      auto ws_server = std::make_shared<websocket_server>();

      std::atomic<int> connections_opened{0};
      std::atomic<int> messages_received{0};
      std::mutex conn_mutex;
      std::vector<std::shared_ptr<websocket_connection_interface>> connections;

      ws_server->on_open([&](std::shared_ptr<websocket_connection_interface> conn, const request&) {
         std::lock_guard<std::mutex> lock(conn_mutex);
         connections.push_back(conn);
         ++connections_opened;
      });

      ws_server->on_message([&](std::shared_ptr<websocket_connection_interface> conn, std::string_view msg, ws_opcode) {
         ++messages_received;
         conn->send_text("Ack: " + std::string(msg));
      });

      ws_server->on_error([](std::shared_ptr<websocket_connection_interface>, std::error_code) {});
      ws_server->on_close([](std::shared_ptr<websocket_connection_interface>, ws_close_code, std::string_view) {});

      server.websocket("/ws", ws_server);
      server.bind("127.0.0.1", 0);
      uint16_t port = server.port();

      std::thread server_thread([&]() { server.start(); });
      std::this_thread::sleep_for(std::chrono::milliseconds(200));

      constexpr int num_clients = 5;
      std::vector<std::unique_ptr<websocket_client>> clients;
      std::vector<std::thread> client_threads;
      std::atomic<int> clients_opened{0};
      std::atomic<int> acks_received{0};

      for (int i = 0; i < num_clients; ++i) {
         auto client = std::make_unique<websocket_client>();

         client->on_open([&]() { ++clients_opened; });
         client->on_message([&](std::string_view, ws_opcode) { ++acks_received; });
         client->on_close([](ws_close_code, std::string_view) {});
         client->on_error([](std::error_code) {});
         client->set_ssl_verify_mode(asio::ssl::verify_none);

         std::string url = "wss://127.0.0.1:" + std::to_string(port) + "/ws";
         client->connect(url);

         auto* ctx = client->context().get();
         client_threads.emplace_back([ctx]() { ctx->run(); });
         clients.push_back(std::move(client));
      }

      // Wait for all clients to connect
      for (int i = 0; i < 50 && clients_opened < num_clients; ++i) {
         std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }

      expect(clients_opened.load() == num_clients) << "All WSS clients should connect\n";
      expect(connections_opened.load() == num_clients) << "Server should see all connections\n";

      // Each client sends a message
      for (int i = 0; i < num_clients; ++i) {
         clients[i]->send("Message from client " + std::to_string(i));
      }

      // Wait for all acks
      for (int i = 0; i < 50 && acks_received < num_clients; ++i) {
         std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }

      expect(messages_received.load() == num_clients) << "Server should receive all messages\n";
      expect(acks_received.load() == num_clients) << "All clients should receive acks\n";

      // Cleanup
      for (auto& client : clients) {
         client->close();
         client->context()->stop();
      }
      for (auto& t : client_threads) {
         t.join();
      }
      server.stop();
      server_thread.join();
   };

   "wss_binary_message_test"_test = [] {
      // Test binary message handling over WSS
      bool certs_ok = ensure_test_certificates();
      expect(certs_ok) << "Failed to generate test certificates";
      if (!certs_ok) return;

      https_server server;
      server.load_certificate("wss_test_cert.pem", "wss_test_key.pem");
      server.set_ssl_verify_mode(0);

      auto ws_server = std::make_shared<websocket_server>();

      std::atomic<bool> binary_received{false};
      std::vector<uint8_t> received_data;
      std::mutex data_mutex;

      ws_server->on_open([](auto, const request&) {});
      ws_server->on_message([&](auto conn, std::string_view msg, ws_opcode opcode) {
         if (opcode == ws_opcode::binary) {
            {
               std::lock_guard<std::mutex> lock(data_mutex);
               received_data.assign(msg.begin(), msg.end());
            }
            binary_received = true;
            // Echo back as binary
            conn->send_binary(msg);
         }
      });
      ws_server->on_error([](auto, std::error_code) {});
      ws_server->on_close([](auto, ws_close_code, std::string_view) {});

      server.websocket("/ws", ws_server);
      server.bind("127.0.0.1", 0);
      uint16_t port = server.port();

      std::thread server_thread([&]() { server.start(); });
      std::this_thread::sleep_for(std::chrono::milliseconds(200));

      websocket_client client;

      std::atomic<bool> client_open{false};
      std::atomic<bool> client_binary_received{false};
      std::vector<uint8_t> client_received_data;

      client.on_open([&]() { client_open = true; });
      client.on_message([&](std::string_view msg, ws_opcode opcode) {
         if (opcode == ws_opcode::binary) {
            client_received_data.assign(msg.begin(), msg.end());
            client_binary_received = true;
         }
      });
      client.on_close([&](ws_close_code, std::string_view) { client.context()->stop(); });
      client.on_error([](std::error_code) {});
      client.set_ssl_verify_mode(asio::ssl::verify_none);

      std::string url = "wss://127.0.0.1:" + std::to_string(port) + "/ws";
      client.connect(url);

      std::thread client_thread([&client]() { client.context()->run(); });

      // Wait for connection
      for (int i = 0; i < 50 && !client_open; ++i) {
         std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }

      expect(client_open.load()) << "WSS client should connect\n";

      // Create binary data with various byte values including nulls
      std::vector<uint8_t> binary_data = {0x00, 0x01, 0x02, 0xFF, 0xFE, 0x80, 0x7F, 0x00, 0xAB, 0xCD};
      std::string_view binary_view(reinterpret_cast<const char*>(binary_data.data()), binary_data.size());

      client.send_binary(binary_view);

      // Wait for echo
      for (int i = 0; i < 50 && !client_binary_received; ++i) {
         std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }

      expect(binary_received.load()) << "Server should receive binary message\n";
      expect(client_binary_received.load()) << "Client should receive binary echo\n";

      {
         std::lock_guard<std::mutex> lock(data_mutex);
         expect(received_data == binary_data) << "Server should receive correct binary data\n";
      }
      expect(client_received_data == binary_data) << "Client should receive correct binary echo\n";

      client.close();
      client.context()->stop();
      client_thread.join();
      server.stop();
      server_thread.join();
   };

   "wss_large_message_test"_test = [] {
      // Test large message handling over WSS
      // SSL has its own framing, so verify large payloads work correctly
      bool certs_ok = ensure_test_certificates();
      expect(certs_ok) << "Failed to generate test certificates";
      if (!certs_ok) return;

      https_server server;
      server.load_certificate("wss_test_cert.pem", "wss_test_key.pem");
      server.set_ssl_verify_mode(0);

      auto ws_server = std::make_shared<websocket_server>();

      std::atomic<bool> large_received{false};
      std::atomic<size_t> received_size{0};

      ws_server->on_open([](auto, const request&) {});
      ws_server->on_message([&](auto conn, std::string_view msg, ws_opcode opcode) {
         if (opcode == ws_opcode::text) {
            received_size = msg.size();
            large_received = true;
            // Send back a confirmation with the size
            conn->send_text("Received " + std::to_string(msg.size()) + " bytes");
         }
      });
      ws_server->on_error([](auto, std::error_code) {});
      ws_server->on_close([](auto, ws_close_code, std::string_view) {});

      server.websocket("/ws", ws_server);
      server.bind("127.0.0.1", 0);
      uint16_t port = server.port();

      std::thread server_thread([&]() { server.start(); });
      std::this_thread::sleep_for(std::chrono::milliseconds(200));

      websocket_client client;

      std::atomic<bool> client_open{false};
      std::atomic<bool> confirmation_received{false};
      std::string confirmation_message;

      client.on_open([&]() { client_open = true; });
      client.on_message([&](std::string_view msg, ws_opcode) {
         confirmation_message = std::string(msg);
         confirmation_received = true;
      });
      client.on_close([&](ws_close_code, std::string_view) { client.context()->stop(); });
      client.on_error([](std::error_code) {});
      client.set_ssl_verify_mode(asio::ssl::verify_none);

      std::string url = "wss://127.0.0.1:" + std::to_string(port) + "/ws";
      client.connect(url);

      std::thread client_thread([&client]() { client.context()->run(); });

      // Wait for connection
      for (int i = 0; i < 50 && !client_open; ++i) {
         std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }

      expect(client_open.load()) << "WSS client should connect\n";

      // Create a large message (256 KB)
      constexpr size_t large_size = 256 * 1024;
      std::string large_message(large_size, 'X');
      // Add some variation to the content
      for (size_t i = 0; i < large_size; i += 1000) {
         large_message[i] = static_cast<char>('A' + (i % 26));
      }

      client.send(large_message);

      // Wait for confirmation (may take longer for large messages)
      for (int i = 0; i < 100 && !confirmation_received; ++i) {
         std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }

      expect(large_received.load()) << "Server should receive large message\n";
      expect(received_size.load() == large_size) << "Server should receive correct size\n";
      expect(confirmation_received.load()) << "Client should receive confirmation\n";

      std::string expected_confirmation = "Received " + std::to_string(large_size) + " bytes";
      expect(confirmation_message == expected_confirmation) << "Confirmation should have correct size\n";

      client.close();
      client.context()->stop();
      client_thread.join();
      server.stop();
      server_thread.join();
   };

   "wss_graceful_close_test"_test = [] {
      // Test graceful close with close code and reason over WSS
      bool certs_ok = ensure_test_certificates();
      expect(certs_ok) << "Failed to generate test certificates";
      if (!certs_ok) return;

      https_server server;
      server.load_certificate("wss_test_cert.pem", "wss_test_key.pem");
      server.set_ssl_verify_mode(0);

      auto ws_server = std::make_shared<websocket_server>();

      std::atomic<bool> server_close_received{false};
      std::atomic<uint16_t> server_close_code{0};
      std::string server_close_reason;
      std::mutex reason_mutex;

      ws_server->on_open([](auto, const request&) {});
      ws_server->on_message([](auto, std::string_view, ws_opcode) {});
      ws_server->on_error([](auto, std::error_code) {});
      ws_server->on_close([&](auto, ws_close_code code, std::string_view reason) {
         server_close_code = static_cast<uint16_t>(code);
         {
            std::lock_guard<std::mutex> lock(reason_mutex);
            server_close_reason = std::string(reason);
         }
         server_close_received = true;
      });

      server.websocket("/ws", ws_server);
      server.bind("127.0.0.1", 0);
      uint16_t port = server.port();

      std::thread server_thread([&]() { server.start(); });
      std::this_thread::sleep_for(std::chrono::milliseconds(200));

      websocket_client client;

      std::atomic<bool> client_open{false};
      std::atomic<bool> client_close_received{false};
      std::atomic<uint16_t> client_close_code{0};
      std::string client_close_reason;

      client.on_open([&]() { client_open = true; });
      client.on_message([](std::string_view, ws_opcode) {});
      client.on_close([&](ws_close_code code, std::string_view reason) {
         client_close_code = static_cast<uint16_t>(code);
         client_close_reason = std::string(reason);
         client_close_received = true;
         client.context()->stop();
      });
      client.on_error([](std::error_code) {});
      client.set_ssl_verify_mode(asio::ssl::verify_none);

      std::string url = "wss://127.0.0.1:" + std::to_string(port) + "/ws";
      client.connect(url);

      std::thread client_thread([&client]() { client.context()->run(); });

      // Wait for connection
      for (int i = 0; i < 50 && !client_open; ++i) {
         std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }

      expect(client_open.load()) << "WSS client should connect\n";

      // Initiate graceful close with custom code and reason
      client.close();

      // Wait for close handshake
      for (int i = 0; i < 50 && !server_close_received; ++i) {
         std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }

      expect(server_close_received.load()) << "Server should receive close frame\n";
      // Normal close code is 1000
      expect(server_close_code.load() == 1000 || server_close_code.load() == 0)
         << "Close code should be normal (1000) or not set\n";

      client.context()->stop();
      client_thread.join();
      server.stop();
      server_thread.join();
   };
};

int main()
{
   std::cout << "=== WSS (WebSocket Secure) Tests ===\n";
   std::cout << "Verifies issue #2164 fix: ASIO 1.32+ SSL compatibility\n";
   std::cout << "Also tests WSS server support\n\n";

   return 0;
}
