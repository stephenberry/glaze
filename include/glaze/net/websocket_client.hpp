#pragma once

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <memory>
#include <mutex>
#include <random>
#include <vector>
#include <variant>

#include "glaze/net/http_client.hpp"
#include "glaze/net/websocket_connection.hpp"

namespace glz
{
   struct websocket_client
   {
      enum class header_validation_error : uint8_t
      {
         none,
         empty_name,
         reserved_name,
         invalid_name,
         invalid_value,
      };

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

      // Internal implementation - prevent callbacks from firing after client destruction
      // by using weak_from_this() pattern
      struct impl : std::enable_shared_from_this<impl>
      {
         std::shared_ptr<asio::io_context> ctx;
         connection_variant connection;
         std::mutex connection_mutex;

         // Callbacks
         std::shared_ptr<message_handler_t> on_message;
         std::shared_ptr<open_handler_t> on_open;
         std::shared_ptr<close_handler_t> on_close;
         std::shared_ptr<error_handler_t> on_error;

         // Connection resources
         std::shared_ptr<asio::ip::tcp::resolver> resolver_;
         std::shared_ptr<tcp_socket> tcp_socket_;
#ifdef GLZ_ENABLE_SSL
         std::shared_ptr<asio::ssl::context> ssl_ctx_;
         std::shared_ptr<ssl_socket> ssl_socket_;
#endif
         mutable std::mutex request_headers_mutex_;
         std::vector<std::pair<std::string, std::string>> request_headers_;
         std::atomic<header_validation_error> last_header_validation_error_{header_validation_error::none};

         size_t max_message_size{1024 * 1024 * 16}; // 16 MB limit
#ifdef GLZ_ENABLE_SSL
         asio::ssl::verify_mode ssl_verify_mode_{asio::ssl::verify_peer}; // Default to verify peer
#endif

         explicit impl(std::shared_ptr<asio::io_context> context) : ctx(std::move(context)) {}

         static bool header_name_equal(std::string_view lhs, std::string_view rhs)
         {
            if (lhs.size() != rhs.size()) return false;
            return std::equal(lhs.begin(), lhs.end(), rhs.begin(), [](char a, char b) {
               return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
            });
         }

         static bool header_name_starts_with(std::string_view value, std::string_view prefix)
         {
            if (value.size() < prefix.size()) return false;
            return std::equal(prefix.begin(), prefix.end(), value.begin(), [](char a, char b) {
               return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
            });
         }

         static bool is_tchar(unsigned char c)
         {
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) return true;

            switch (c) {
            case '!':
            case '#':
            case '$':
            case '%':
            case '&':
            case '\'':
            case '*':
            case '+':
            case '-':
            case '.':
            case '^':
            case '_':
            case '`':
            case '|':
            case '~':
               return true;
            default:
               return false;
            }
         }

         static bool is_reserved_handshake_header(std::string_view name)
         {
            return header_name_equal(name, "Host") || header_name_equal(name, "Upgrade") ||
                   header_name_equal(name, "Connection") || header_name_starts_with(name, "Sec-WebSocket-");
         }

         static bool validate_header_name(std::string_view name, header_validation_error& error)
         {
            if (name.empty()) {
               error = header_validation_error::empty_name;
               return false;
            }

            if (is_reserved_handshake_header(name)) {
               error = header_validation_error::reserved_name;
               return false;
            }

            for (const unsigned char c : name) {
               if (!is_tchar(c)) {
                  error = header_validation_error::invalid_name;
                  return false;
               }
            }
            return true;
         }

         static bool validate_header_value(std::string_view value, header_validation_error& error)
         {
            for (const unsigned char c : value) {
               if (c == '\r' || c == '\n' || c == 127) {
                  error = header_validation_error::invalid_value;
                  return false;
               }
               if (c < 32 && c != '\t') {
                  error = header_validation_error::invalid_value;
                  return false;
               }
            }
            return true;
         }

         std::vector<std::pair<std::string, std::string>> request_headers_snapshot() const
         {
            std::lock_guard<std::mutex> lock(request_headers_mutex_);
            return request_headers_;
         }

         bool set_request_header(std::string_view name, std::string_view value)
         {
            header_validation_error error = header_validation_error::none;
            if (!validate_header_name(name, error) || !validate_header_value(value, error)) {
               last_header_validation_error_.store(error, std::memory_order_relaxed);
               return false;
            }

            std::lock_guard<std::mutex> lock(request_headers_mutex_);
            for (auto& [existing_name, existing_value] : request_headers_) {
               if (header_name_equal(existing_name, name)) {
                  existing_value = std::string(value);
                  last_header_validation_error_.store(header_validation_error::none, std::memory_order_relaxed);
                  return true;
               }
            }
            request_headers_.emplace_back(std::string(name), std::string(value));
            last_header_validation_error_.store(header_validation_error::none, std::memory_order_relaxed);
            return true;
         }

         void clear_request_headers()
         {
            std::lock_guard<std::mutex> lock(request_headers_mutex_);
            request_headers_.clear();
         }

         header_validation_error last_header_validation_error() const
         {
            return last_header_validation_error_.load(std::memory_order_relaxed);
         }

         void cancel_all()
         {
            // Clear handlers first to prevent callbacks during cleanup
            on_message.reset();
            on_open.reset();
            on_close.reset();
            on_error.reset();

            // Force close the connection - this closes the socket immediately,
            // ensuring it's deregistered from the reactor before io_context destruction
            {
               std::lock_guard<std::mutex> lock(connection_mutex);
               std::visit(
                  [](auto&& conn) {
                     if constexpr (!std::is_same_v<std::decay_t<decltype(conn)>, std::monostate>) {
                        if (conn) {
                           conn->force_close();
                        }
                     }
                  },
                  connection);
               connection = std::monostate{};
            }

            // Cancel resolver
            if (resolver_) {
               resolver_->cancel();
               resolver_.reset();
            }

            // Reset socket pointers (sockets already closed via force_close above)
            tcp_socket_.reset();
#ifdef GLZ_ENABLE_SSL
            ssl_socket_.reset();
#endif
         }

         asio::ip::tcp::socket& get_tcp_socket_ref()
         {
#ifdef GLZ_ENABLE_SSL
            // Use next_layer() instead of lowest_layer() to get the correct type.
            // In ASIO 1.32+, lowest_layer() returns basic_socket<tcp> which differs
            // from basic_stream_socket<tcp> (the actual socket type).
            if (ssl_socket_) return ssl_socket_->next_layer();
#endif
            return *tcp_socket_;
         }

         const asio::ip::tcp::socket& get_tcp_socket_ref() const
         {
#ifdef GLZ_ENABLE_SSL
            if (ssl_socket_) return ssl_socket_->next_layer();
#endif
            return *tcp_socket_;
         }

         void connect(std::string_view url_str)
         {
            auto url_result = parse_url(url_str);
            if (!url_result) {
               if (on_error && *on_error) (*on_error)(url_result.error());
               return;
            }

            auto& url = *url_result;

            if (url.protocol == "wss") {
#ifdef GLZ_ENABLE_SSL
               if (!ssl_ctx_) {
                  ssl_ctx_ = std::make_shared<asio::ssl::context>(asio::ssl::context::tls_client);
                  ssl_ctx_->set_default_verify_paths();
                  ssl_ctx_->set_verify_mode(ssl_verify_mode_);
               }
               ssl_socket_ = std::make_shared<asio::ssl::stream<asio::ip::tcp::socket>>(*ctx, *ssl_ctx_);

               if (!detail::configure_ssl_client_hostname(*ssl_socket_, url.host)) {
                  if (on_error && *on_error) (*on_error)(make_error_code(ssl_error::sni_hostname_failed));
                  return;
               }
#else
               if (on_error && *on_error) (*on_error)(std::make_error_code(std::errc::protocol_not_supported));
               return;
#endif
            }
            else {
               tcp_socket_ = std::make_shared<asio::ip::tcp::socket>(*ctx);
            }

            resolver_ = std::make_shared<asio::ip::tcp::resolver>(*ctx);

            std::weak_ptr<impl> weak_self = weak_from_this();

            resolver_->async_resolve(
               url.host, std::to_string(url.port),
               [weak_self, url](std::error_code ec, asio::ip::tcp::resolver::results_type results) {
                  auto self = weak_self.lock();
                  if (!self) return; // Client was destroyed

                  if (ec) {
                     if (self->on_error && *self->on_error) (*self->on_error)(ec);
                     return;
                  }
                  self->on_resolved(url, results);
               });
         }

         void on_resolved(const url_parts& url, const asio::ip::tcp::resolver::results_type& results)
         {
            std::weak_ptr<impl> weak_self = weak_from_this();
            auto& socket_ref = get_tcp_socket_ref();

            asio::async_connect(socket_ref, results,
                                [weak_self, url](std::error_code ec, const asio::ip::tcp::endpoint&) {
                                   auto self = weak_self.lock();
                                   if (!self) return; // Client was destroyed

                                   if (ec) {
                                      if (self->on_error && *self->on_error) (*self->on_error)(ec);
                                      return;
                                   }
                                   self->on_connected(url);
                                });
         }

         void on_connected(const url_parts& url)
         {
            if (url.protocol == "wss") {
#ifdef GLZ_ENABLE_SSL
               std::weak_ptr<impl> weak_self = weak_from_this();

               ssl_socket_->async_handshake(asio::ssl::stream_base::client, [weak_self, url](std::error_code ec) {
                  auto self = weak_self.lock();
                  if (!self) return; // Client was destroyed

                  if (ec) {
                     if (self->on_error && *self->on_error) (*self->on_error)(ec);
                     return;
                  }
                  self->perform_handshake(self->ssl_socket_, url);
               });
#endif
            }
            else {
               perform_handshake(tcp_socket_, url);
            }
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
                                    "\r\n" + "Sec-WebSocket-Version: 13\r\n";

            for (const auto& [name, value] : request_headers_snapshot()) {
               handshake += name + ": " + value + "\r\n";
            }
            handshake += "\r\n";

            auto req_buf = std::make_shared<std::string>(std::move(handshake));
            std::weak_ptr<impl> weak_self = weak_from_this();

            asio::async_write(*socket, asio::buffer(*req_buf),
                              [weak_self, socket, req_buf, key](std::error_code ec, std::size_t) {
                                 auto self = weak_self.lock();
                                 if (!self) return; // Client was destroyed

                                 if (ec) {
                                    if (self->on_error && *self->on_error) (*self->on_error)(ec);
                                    return;
                                 }
                                 self->read_handshake_response(socket, key);
                              });
         }

         template <typename SocketType>
         void read_handshake_response(std::shared_ptr<SocketType> socket, const std::string& expected_key)
         {
            static constexpr size_t max_handshake_size = 1024 * 16;
            auto response_buf = std::make_shared<asio::streambuf>(max_handshake_size);
            std::weak_ptr<impl> weak_self = weak_from_this();

            asio::async_read_until(*socket, *response_buf, "\r\n\r\n",
                                   [weak_self, socket, response_buf, expected_key](std::error_code ec, std::size_t) {
                                      auto self = weak_self.lock();
                                      if (!self) return; // Client was destroyed

                                      if (ec) {
                                         if (self->on_error && *self->on_error) (*self->on_error)(ec);
                                         return;
                                      }

                                      std::istream response_stream(response_buf.get());
                                      std::string http_version;
                                      unsigned int status_code;
                                      std::string status_message;

                                      response_stream >> http_version >> status_code;
                                      std::getline(response_stream, status_message);

                                      if (!response_stream || status_code != 101) {
                                         if (self->on_error && *self->on_error)
                                            (*self->on_error)(std::make_error_code(std::errc::protocol_error));
                                         return;
                                      }

                                      // Parse headers to verify upgrade and accept key
                                      std::string header;
                                      bool upgrade_websocket = false;
                                      bool connection_upgrade = false;
                                      bool accept_key_valid = false;

                                      std::string expected_accept = ws_util::generate_accept_key(expected_key);

                                      while (std::getline(response_stream, header) && header != "\r") {
                                         if (!header.empty() && header.back() == '\r') header.pop_back();

                                         auto colon = header.find(':');
                                         if (colon != std::string::npos) {
                                            std::string name = header.substr(0, colon);
                                            std::string value = header.substr(colon + 1);

                                            while (!value.empty() && (value.front() == ' ' || value.front() == '\t'))
                                               value.erase(0, 1);
                                            while (!value.empty() && (value.back() == ' ' || value.back() == '\t'))
                                               value.pop_back();

                                            if (strncasecmp(name.c_str(), "Upgrade", 7) == 0 &&
                                                ws_util::header_contains(value, "websocket")) {
                                               upgrade_websocket = true;
                                            }
                                            else if (strncasecmp(name.c_str(), "Connection", 10) == 0 &&
                                                     ws_util::header_contains(value, "upgrade")) {
                                               connection_upgrade = true;
                                            }
                                            else if (strncasecmp(name.c_str(), "Sec-WebSocket-Accept", 20) == 0) {
                                               if (value == expected_accept) accept_key_valid = true;
                                            }
                                         }
                                      }

                                      if (!upgrade_websocket || !connection_upgrade || !accept_key_valid) {
                                         if (self->on_error && *self->on_error)
                                            (*self->on_error)(std::make_error_code(std::errc::protocol_error));
                                         return;
                                      }

                                      // Handshake successful. Transfer socket to websocket_connection.
                                      auto ws_conn = std::make_shared<websocket_connection<SocketType>>(socket);
                                      ws_conn->set_client_mode(true);
                                      ws_conn->set_max_message_size(self->max_message_size);

                                      if (response_buf->size() > 0) {
                                         std::string_view initial_data{
                                            static_cast<const char*>(response_buf->data().data()),
                                            response_buf->size()};
                                         ws_conn->set_initial_data(initial_data);
                                      }

                                      if (self->on_message && *self->on_message) ws_conn->on_message(*self->on_message);
                                      if (self->on_close && *self->on_close) ws_conn->on_close(*self->on_close);
                                      if (self->on_error && *self->on_error) ws_conn->on_error(*self->on_error);

                                      ws_conn->start_read();

                                      {
                                         std::lock_guard<std::mutex> lock(self->connection_mutex);
                                         self->connection = ws_conn;
                                      }

                                      if (self->on_open && *self->on_open) (*self->on_open)();
                                   });
         }

         void send_text(std::string_view msg)
         {
            std::lock_guard<std::mutex> lock(connection_mutex);
            std::visit(
               [&](auto&& conn) {
                  if constexpr (!std::is_same_v<std::decay_t<decltype(conn)>, std::monostate>) {
                     if (conn) conn->send_text(msg);
                  }
               },
               connection);
         }

         void send_binary(std::string_view msg)
         {
            std::lock_guard<std::mutex> lock(connection_mutex);
            std::visit(
               [&](auto&& conn) {
                  if constexpr (!std::is_same_v<std::decay_t<decltype(conn)>, std::monostate>) {
                     if (conn) conn->send_binary(msg);
                  }
               },
               connection);
         }

         void close_connection()
         {
            std::lock_guard<std::mutex> lock(connection_mutex);
            std::visit(
               [&](auto&& conn) {
                  if constexpr (!std::is_same_v<std::decay_t<decltype(conn)>, std::monostate>) {
                     if (conn) conn->close();
                  }
               },
               connection);
         }
      };

     private:
      std::shared_ptr<impl> impl_;

     public:
      websocket_client(std::shared_ptr<asio::io_context> ctx = nullptr)
      {
         if (!ctx) {
            ctx = std::make_shared<asio::io_context>();
         }
         impl_ = std::make_shared<impl>(std::move(ctx));
      }

      ~websocket_client()
      {
         // Cancel pending operations - this closes sockets and releases the connection.
         // Sockets must be closed before io_context is destroyed.
         impl_->cancel_all();
      }

      void on_message(message_handler_t handler)
      {
         impl_->on_message = std::make_shared<message_handler_t>(std::move(handler));
      }

      void on_open(open_handler_t handler) { impl_->on_open = std::make_shared<open_handler_t>(std::move(handler)); }

      void on_close(close_handler_t handler)
      {
         impl_->on_close = std::make_shared<close_handler_t>(std::move(handler));
      }

      void on_error(error_handler_t handler)
      {
         impl_->on_error = std::make_shared<error_handler_t>(std::move(handler));
      }

      void set_max_message_size(size_t size) { impl_->max_message_size = size; }

#ifdef GLZ_ENABLE_SSL
      // Set SSL verification mode before calling connect()
      // Use asio::ssl::verify_none to disable certificate verification
      // (useful for self-signed certificates in testing)
      void set_ssl_verify_mode(asio::ssl::verify_mode mode) { impl_->ssl_verify_mode_ = mode; }
#endif

      // Set an additional HTTP header for the opening WebSocket handshake.
      // Reserved handshake headers (Host, Upgrade, Connection, Sec-WebSocket-*) cannot be overridden.
      // Returns false if the name/value fails validation.
      [[nodiscard]] bool set_header(std::string_view name, std::string_view value)
      {
         return impl_->set_request_header(name, value);
      }

      // Clear all additional handshake headers previously set via set_header().
      void clear_headers() { impl_->clear_request_headers(); }

      [[nodiscard]] header_validation_error last_header_error() const
      {
         return impl_->last_header_validation_error();
      }

      std::shared_ptr<asio::io_context>& context() { return impl_->ctx; }

      asio::ip::tcp::socket& socket() { return impl_->get_tcp_socket_ref(); }
      const asio::ip::tcp::socket& socket() const { return impl_->get_tcp_socket_ref(); }

      void run() { impl_->ctx->run(); }

      void connect(std::string_view url_str) { impl_->connect(url_str); }

      void send(std::string_view msg) { impl_->send_text(msg); }

      void send_binary(std::string_view msg) { impl_->send_binary(msg); }

      void close() { impl_->close_connection(); }

      impl& internal() { return *impl_; }
      const impl& internal() const { return *impl_; }
   };
}
