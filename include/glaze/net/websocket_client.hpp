#pragma once

#include "glaze/net/websocket_connection.hpp"
#include "glaze/net/http_client.hpp"

namespace glz
{
   struct websocket_client
   {
      using message_handler_t = std::function<void(std::string_view, ws_opcode)>;
      using open_handler_t = std::function<void()>;
      using close_handler_t = std::function<void(ws_close_code, std::string_view)>;
      using error_handler_t = std::function<void(std::error_code)>;

      std::shared_ptr<asio::io_context> ctx_;
      std::shared_ptr<websocket_connection> connection_;
      
      // Callbacks
      message_handler_t on_message_;
      open_handler_t on_open_;
      close_handler_t on_close_;
      error_handler_t on_error_;

      // Keep alive the resolver/socket during connection
      std::shared_ptr<asio::ip::tcp::resolver> resolver_;
      std::shared_ptr<asio::ip::tcp::socket> socket_;

      websocket_client(std::shared_ptr<asio::io_context> ctx = nullptr) 
      {
          if (ctx) ctx_ = ctx;
          else ctx_ = std::make_shared<asio::io_context>();
      }

      void on_message(message_handler_t handler) { on_message_ = std::move(handler); }
      void on_open(open_handler_t handler) { on_open_ = std::move(handler); }
      void on_close(close_handler_t handler) { on_close_ = std::move(handler); }
      void on_error(error_handler_t handler) { on_error_ = std::move(handler); }

      void run() { ctx_->run(); }

      void connect(std::string_view url_str) {
         auto url_result = parse_url(url_str);
         if (!url_result) {
            if (on_error_) on_error_(url_result.error());
            return;
         }

         auto& url = *url_result;
         resolver_ = std::make_shared<asio::ip::tcp::resolver>(*ctx_);
         socket_ = std::make_shared<asio::ip::tcp::socket>(*ctx_);

         resolver_->async_resolve(url.host, std::to_string(url.port),
            [this, url](std::error_code ec, asio::ip::tcp::resolver::results_type results) {
               if (ec) {
                  if (on_error_) on_error_(ec);
                  return;
               }
               
               asio::async_connect(*socket_, results,
                  [this, url](std::error_code ec, const asio::ip::tcp::endpoint&) {
                     if (ec) {
                        if (on_error_) on_error_(ec);
                        return;
                     }
                     perform_handshake(url);
                  });
            });
      }

      void send(std::string_view msg) {
          if (connection_) connection_->send_text(msg);
      }

      void close() {
          if (connection_) connection_->close();
      }

   private:
      void perform_handshake(const url_parts& url) {
          // Generate random Sec-WebSocket-Key
          std::string key_bytes(16, '\0');
          std::mt19937 rng(std::random_device{}());
          std::uniform_int_distribution<uint16_t> dist(0, 255);
          for (auto& b : key_bytes) b = static_cast<char>(dist(rng));
          std::string key = glz::write_base64(key_bytes);

          std::string handshake = 
              "GET " + url.path + " HTTP/1.1\r\n" + 
              "Host: " + url.host + "\r\n" + 
              "Upgrade: websocket\r\n" + 
              "Connection: Upgrade\r\n" + 
              "Sec-WebSocket-Key: " + key + "\r\n" + 
              "Sec-WebSocket-Version: 13\r\n\r\n";

          auto req_buf = std::make_shared<std::string>(std::move(handshake));
          
          asio::async_write(*socket_, asio::buffer(*req_buf),
             [this, req_buf, key](std::error_code ec, std::size_t) {
                 if (ec) {
                     if (on_error_) on_error_(ec);
                     return;
                 }
                 read_handshake_response(key);
             });
      }

      void read_handshake_response(const std::string& expected_key) {
          auto response_buf = std::make_shared<asio::streambuf>();
          asio::async_read_until(*socket_, *response_buf, "\r\n\r\n",
             [this, response_buf, expected_key](std::error_code ec, std::size_t) {
                 if (ec) {
                     if (on_error_) on_error_(ec);
                     return;
                 }

                 std::istream response_stream(response_buf.get());
                 std::string http_version;
                 unsigned int status_code;
                 std::string status_message;

                 response_stream >> http_version >> status_code;
                 std::getline(response_stream, status_message);

                 if (!response_stream || status_code != 101) {
                     if (on_error_) on_error_(std::make_error_code(std::errc::protocol_error));
                     return;
                 }

                 // Parse headers to verify upgrade and accept key
                 // (Simplified parsing)
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
                         while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.erase(0, 1);

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
                     if (on_error_) on_error_(std::make_error_code(std::errc::protocol_error));
                     return;
                 }

                 // Handshake successful. Transfer socket to websocket_connection.
                 connection_ = std::make_shared<websocket_connection>(std::move(*socket_));
                 connection_->set_client_mode(true);
                 
                 if (response_buf->size() > 0) {
                     std::string_view initial_data{
                         static_cast<const char*>(response_buf->data().data()), 
                         response_buf->size()
                     };
                     connection_->set_initial_data(initial_data);
                 }
                 
                 if (on_message_) connection_->on_message(on_message_);
                 if (on_close_) connection_->on_close(on_close_);
                 if (on_error_) connection_->on_error(on_error_);
                 if (on_open_) on_open_();

                 connection_->start_read();
             });
      }
   };
}
