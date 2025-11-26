#pragma once

#include <mutex>
#include <random>
#include <variant>

#include "glaze/net/http_client.hpp"
#include "glaze/net/websocket_connection.hpp"

#ifdef GLZ_ENABLE_SSL
#include <asio/ssl.hpp>
#endif

namespace glz
{
   struct websocket_client
   {
      using message_handler_t = std::function<void(std::string_view, ws_opcode)>;
      using open_handler_t = std::function<void()>;
      using close_handler_t = std::function<void(ws_close_code, std::string_view)>;
      using error_handler_t = std::function<void(std::error_code)>;

      using tcp_socket = asio::ip::tcp::socket;
#ifdef GLZ_ENABLE_SSL
      using ssl_socket = asio::ssl::stream<asio::ip::tcp::socket>;
      using connection_variant = std::variant<std::monostate, std::shared_ptr<websocket_connection<tcp_socket>>,
                                              std::shared_ptr<websocket_connection<ssl_socket>>>;
#else
      using connection_variant = std::variant<std::monostate, std::shared_ptr<websocket_connection<tcp_socket>>>;
#endif

      std::shared_ptr<asio::io_context> ctx_;
      connection_variant connection_;
      std::shared_ptr<std::mutex> connection_mutex_;

      // Callbacks - wrapped in shared_ptr to survive client destruction
      std::shared_ptr<message_handler_t> on_message_;
      std::shared_ptr<open_handler_t> on_open_;
      std::shared_ptr<close_handler_t> on_close_;
      std::shared_ptr<error_handler_t> on_error_;

      // Keep alive the resolver/socket during connection
      std::shared_ptr<asio::ip::tcp::resolver> resolver_;
      std::shared_ptr<tcp_socket> tcp_socket_;
#ifdef GLZ_ENABLE_SSL
      std::shared_ptr<asio::ssl::context> ssl_ctx_;
      std::shared_ptr<ssl_socket> ssl_socket_;
#endif

      websocket_client(std::shared_ptr<asio::io_context> ctx = nullptr)
      {
         if (ctx)
            ctx_ = ctx;
         else
            ctx_ = std::make_shared<asio::io_context>();

         connection_mutex_ = std::make_shared<std::mutex>();
      }

      ~websocket_client() = default;

      void on_message(message_handler_t handler)
      {
         on_message_ = std::make_shared<message_handler_t>(std::move(handler));
      }
      void on_open(open_handler_t handler) { on_open_ = std::make_shared<open_handler_t>(std::move(handler)); }
      void on_close(close_handler_t handler) { on_close_ = std::make_shared<close_handler_t>(std::move(handler)); }
      void on_error(error_handler_t handler) { on_error_ = std::make_shared<error_handler_t>(std::move(handler)); }

      void set_max_message_size(size_t size) { max_message_size_ = size; }

      void run() { ctx_->run(); }

      void connect(std::string_view url_str)
      {
         auto url_result = parse_url(url_str);
         if (!url_result) {
            if (on_error_ && *on_error_) (*on_error_)(url_result.error());
            return;
         }

         auto& url = *url_result;

         if (url.protocol == "wss") {
#ifdef GLZ_ENABLE_SSL
            if (!ssl_ctx_) {
               ssl_ctx_ = std::make_shared<asio::ssl::context>(asio::ssl::context::tlsv12_client);
               ssl_ctx_->set_default_verify_paths();
            }
            // Create socket but don't connect yet
            // We need to connect the TCP layer first
            // ssl_socket_ needs a socket to be constructed.
            // We can construct it with the io_context and then get the lowest layer.
            ssl_socket_ = std::make_shared<ssl_socket>(*ctx_, *ssl_ctx_);

            // Set SNI
            if (!SSL_set_tlsext_host_name(ssl_socket_->native_handle(), url.host.c_str())) {
               if (on_error_) on_error_(std::make_error_code(std::errc::address_not_available));
               return;
            }
#else
            if (on_error_ && *on_error_) (*on_error_)(std::make_error_code(std::errc::protocol_not_supported));
            return;
#endif
         }
         else {
            tcp_socket_ = std::make_shared<tcp_socket>(*ctx_);
         }

         resolver_ = std::make_shared<asio::ip::tcp::resolver>(*ctx_);

         auto error_handler = on_error_; // Copy shared_ptr
         resolver_->async_resolve(
            url.host, std::to_string(url.port),
            [this, url, error_handler](std::error_code ec, asio::ip::tcp::resolver::results_type results) {
               if (ec) {
                  if (error_handler && *error_handler) (*error_handler)(ec);
                  return;
               }

               // Determine which socket to connect
               auto& socket_ref = get_tcp_socket_ref();

               auto error_handler2 = on_error_; // Copy shared_ptr
               asio::async_connect(socket_ref, results,
                                   [this, url, error_handler2](std::error_code ec, const asio::ip::tcp::endpoint&) {
                                      if (ec) {
                                         if (error_handler2 && *error_handler2) (*error_handler2)(ec);
                                         return;
                                      }

                                      if (url.protocol == "wss") {
#ifdef GLZ_ENABLE_SSL
                                         // Perform SSL Handshake
                                         auto error_handler3 = on_error_;
                                         ssl_socket_->async_handshake(asio::ssl::stream_base::client,
                                                                      [this, url, error_handler3](std::error_code ec) {
                                                                         if (ec) {
                                                                            if (error_handler3 && *error_handler3)
                                                                               (*error_handler3)(ec);
                                                                            return;
                                                                         }
                                                                         perform_handshake(ssl_socket_, url);
                                                                      });
#endif
                                      }
                                      else {
                                         perform_handshake(tcp_socket_, url);
                                      }
                                   });
            });
      }

      void send(std::string_view msg)
      {
         auto mutex = connection_mutex_; // Copy shared_ptr to keep mutex alive
         if (!mutex) return;

         std::lock_guard<std::mutex> lock(*mutex);
         std::visit(
            [&](auto&& conn) {
               if constexpr (!std::is_same_v<std::decay_t<decltype(conn)>, std::monostate>) {
                  if (conn) conn->send_text(msg);
               }
            },
            connection_);
      }

      void send_binary(std::string_view msg)
      {
         auto mutex = connection_mutex_; // Copy shared_ptr to keep mutex alive
         if (!mutex) return;

         std::lock_guard<std::mutex> lock(*mutex);
         std::visit(
            [&](auto&& conn) {
               if constexpr (!std::is_same_v<std::decay_t<decltype(conn)>, std::monostate>) {
                  if (conn) conn->send_binary(msg);
               }
            },
            connection_);
      }

      void close()
      {
         auto mutex = connection_mutex_; // Copy shared_ptr to keep mutex alive
         if (!mutex) return;

         std::lock_guard<std::mutex> lock(*mutex);
         std::visit(
            [&](auto&& conn) {
               if constexpr (!std::is_same_v<std::decay_t<decltype(conn)>, std::monostate>) {
                  if (conn) conn->close();
               }
            },
            connection_);
      }

     private:
      size_t max_message_size_{1024 * 1024 * 16}; // 16 MB limit

      asio::ip::tcp::socket& get_tcp_socket_ref()
      {
#ifdef GLZ_ENABLE_SSL
         if (ssl_socket_) return ssl_socket_->lowest_layer();
#endif
         return *tcp_socket_;
      }

      template <typename SocketType>
      void perform_handshake(std::shared_ptr<SocketType> socket, const url_parts& url)
      {
         // Generate random Sec-WebSocket-Key
         std::string key_bytes(16, '\0');
         std::mt19937 rng(std::random_device{}());
         std::uniform_int_distribution<uint16_t> dist(0, 255);
         for (auto& b : key_bytes) b = static_cast<char>(dist(rng));
         std::string key = glz::write_base64(key_bytes);

         std::string handshake = "GET " + url.path + " HTTP/1.1\r\n" + "Host: " + url.host + "\r\n" +
                                 "Upgrade: websocket\r\n" + "Connection: Upgrade\r\n" + "Sec-WebSocket-Key: " + key +
                                 "\r\n" + "Sec-WebSocket-Version: 13\r\n\r\n";

         auto req_buf = std::make_shared<std::string>(std::move(handshake));
         auto error_handler = on_error_; // Copy shared_ptr

         asio::async_write(*socket, asio::buffer(*req_buf),
                           [this, socket, req_buf, key, error_handler](std::error_code ec, std::size_t) {
                              if (ec) {
                                 if (error_handler && *error_handler) (*error_handler)(ec);
                                 return;
                              }
                              read_handshake_response(socket, key);
                           });
      }

      template <typename SocketType>
      void read_handshake_response(std::shared_ptr<SocketType> socket, const std::string& expected_key)
      {
         // Limit handshake response size to 16KB to prevent DoS
         static constexpr size_t max_handshake_size = 1024 * 16;
         auto response_buf = std::make_shared<asio::streambuf>(max_handshake_size);
         auto error_handler = on_error_; // Copy shared_ptr
         auto message_handler = on_message_;
         auto close_handler = on_close_;
         auto open_handler = on_open_;
         auto mutex = connection_mutex_;
         auto max_msg_size = max_message_size_;

         asio::async_read_until(*socket, *response_buf, "\r\n\r\n",
                                [this, socket, response_buf, expected_key, error_handler, message_handler,
                                 close_handler, open_handler, mutex, max_msg_size](std::error_code ec, std::size_t) {
                                   if (ec) {
                                      if (error_handler && *error_handler) (*error_handler)(ec);
                                      return;
                                   }

                                   std::istream response_stream(response_buf.get());
                                   std::string http_version;
                                   unsigned int status_code;
                                   std::string status_message;

                                   response_stream >> http_version >> status_code;
                                   std::getline(response_stream, status_message);

                                   if (!response_stream || status_code != 101) {
                                      if (error_handler && *error_handler)
                                         (*error_handler)(std::make_error_code(std::errc::protocol_error));
                                      return;
                                   }

                                   // Parse headers to verify upgrade and accept key
                                   std::string header;
                                   bool upgrade_websocket = false;
                                   bool connection_upgrade = false;
                                   bool accept_key_valid = false;

                                   std::string expected_accept = ws_util::generate_accept_key(expected_key);

                                   while (std::getline(response_stream, header) && header != "\r") {
                                      // Trim trailing \r
                                      if (!header.empty() && header.back() == '\r') header.pop_back();

                                      auto colon = header.find(':');
                                      if (colon != std::string::npos) {
                                         std::string name = header.substr(0, colon);
                                         std::string value = header.substr(colon + 1);

                                         // Trim value
                                         while (!value.empty() && (value.front() == ' ' || value.front() == '\t'))
                                            value.erase(0, 1);

                                         if (strncasecmp(name.c_str(), "Upgrade", 7) == 0 &&
                                             strncasecmp(value.c_str(), "websocket", 9) == 0) {
                                            upgrade_websocket = true;
                                         }
                                         else if (strncasecmp(name.c_str(), "Connection", 10) == 0 &&
                                                  strncasecmp(value.c_str(), "Upgrade", 7) == 0) {
                                            connection_upgrade = true;
                                         }
                                         else if (strncasecmp(name.c_str(), "Sec-WebSocket-Accept", 20) == 0) {
                                            if (value == expected_accept) accept_key_valid = true;
                                         }
                                      }
                                   }

                                   if (!upgrade_websocket || !connection_upgrade || !accept_key_valid) {
                                      if (error_handler && *error_handler)
                                         (*error_handler)(std::make_error_code(std::errc::protocol_error));
                                      return;
                                   }

                                   // Handshake successful. Transfer socket to websocket_connection.
                                   auto ws_conn = std::make_shared<websocket_connection<SocketType>>(socket);
                                   ws_conn->set_client_mode(true);
                                   ws_conn->set_max_message_size(max_msg_size);

                                   if (response_buf->size() > 0) {
                                      std::string_view initial_data{
                                         static_cast<const char*>(response_buf->data().data()), response_buf->size()};
                                      ws_conn->set_initial_data(initial_data);
                                   }

                                   if (message_handler && *message_handler) ws_conn->on_message(*message_handler);
                                   if (close_handler && *close_handler) ws_conn->on_close(*close_handler);
                                   if (error_handler && *error_handler) ws_conn->on_error(*error_handler);

                                   ws_conn->start_read();
                                   {
                                      if (mutex) {
                                         std::lock_guard<std::mutex> lock(*mutex);
                                         connection_ = ws_conn;
                                      }
                                   }

                                   if (open_handler && *open_handler) (*open_handler)();
                                });
      }
   };
}