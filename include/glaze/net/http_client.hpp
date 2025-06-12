// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <atomic>
#include <chrono>
#include <expected>
#include <functional>
#include <future>
#include <glaze/glaze.hpp>
#include <iostream>
#include <memory>
#include <mutex>
#include <source_location>
#include <thread>
#include <unordered_map>
#include <vector>

#include "glaze/net/http_router.hpp"

#if __has_include(<asio.hpp>) && !defined(GLZ_USE_BOOST_ASIO)
#include <asio.hpp>
#elif __has_include(<boost/asio.hpp>)
#ifndef GLZ_USING_BOOST_ASIO
#define GLZ_USING_BOOST_ASIO
#endif
#include <boost/asio.hpp>
#else
static_assert(false, "asio not included");
#endif

namespace glz
{
   struct url_parts
   {
      std::string protocol;
      std::string host;
      uint16_t port;
      std::string path;
   };

   inline std::expected<url_parts, std::error_code> parse_url(std::string_view url)
   {
      // Check minimum length
      if (url.size() < 8) { // Minimum for "http://" + 1 char host
         return std::unexpected(std::make_error_code(std::errc::invalid_argument));
      }

      // Find protocol
      size_t protocol_end = url.find("://");
      if (protocol_end == std::string_view::npos) {
         return std::unexpected(std::make_error_code(std::errc::invalid_argument));
      }

      std::string protocol(url.substr(0, protocol_end));
      if (protocol != "http" && protocol != "https") {
         return std::unexpected(std::make_error_code(std::errc::invalid_argument));
      }

      // Process host, port and path
      size_t host_start = protocol_end + 3;
      if (host_start >= url.size()) {
         return std::unexpected(std::make_error_code(std::errc::invalid_argument));
      }

      size_t host_end = url.find_first_of("/:", host_start);
      std::string host;
      std::string port_str;
      std::string path = "/";

      if (host_end == std::string_view::npos) {
         host = std::string(url.substr(host_start));
      }
      else if (url[host_end] == ':') {
         host = std::string(url.substr(host_start, host_end - host_start));
         size_t port_start = host_end + 1;
         size_t port_end = url.find('/', port_start);

         if (port_end == std::string_view::npos) {
            port_str = std::string(url.substr(port_start));
         }
         else {
            port_str = std::string(url.substr(port_start, port_end - port_start));
            path = std::string(url.substr(port_end));
         }

         if (!port_str.empty() && !std::all_of(port_str.begin(), port_str.end(), ::isdigit)) {
            return std::unexpected(std::make_error_code(std::errc::invalid_argument));
         }
      }
      else if (url[host_end] == '/') {
         host = std::string(url.substr(host_start, host_end - host_start));
         path = std::string(url.substr(host_end));
      }

      if (host.empty() || host.find_first_of(":/") != std::string::npos) {
         return std::unexpected(std::make_error_code(std::errc::invalid_argument));
      }

      uint16_t port = 0;
      if (port_str.empty()) {
         port = (protocol == "https") ? 443 : 80;
      }
      else {
         try {
            long port_long = std::stol(port_str);
            if (port_long <= 0 || port_long > 65535) {
               return std::unexpected(std::make_error_code(std::errc::invalid_argument));
            }
            port = static_cast<uint16_t>(port_long);
         }
         catch (const std::exception&) {
            return std::unexpected(std::make_error_code(std::errc::invalid_argument));
         }
      }

      return url_parts{std::move(protocol), std::move(host), port, std::move(path)};
   }

   // HTTP connection pool for reusing sockets
   struct http_connection_pool
   {
      struct connection_key
      {
         std::string host;
         uint16_t port;

         bool operator==(const connection_key& other) const { return host == other.host && port == other.port; }
      };

      struct connection_key_hash
      {
         size_t operator()(const connection_key& key) const
         {
            return std::hash<std::string>{}(key.host) ^ (std::hash<uint16_t>{}(key.port) << 1);
         }
      };

      std::mutex mtx;
      std::unordered_map<connection_key, std::vector<std::shared_ptr<asio::ip::tcp::socket>>, connection_key_hash>
         available_connections;
      std::shared_ptr<asio::io_context> io_context;

      http_connection_pool(std::shared_ptr<asio::io_context> ctx) : io_context(ctx) {}

      std::shared_ptr<asio::ip::tcp::socket> get_connection(const std::string& host, uint16_t port)
      {
         connection_key key{host, port};

         {
            std::lock_guard<std::mutex> lock(mtx);
            auto it = available_connections.find(key);
            if (it != available_connections.end() && !it->second.empty()) {
               auto socket = it->second.back();
               it->second.pop_back();

               // Check if socket is still connected
               if (socket && socket->is_open()) {
                  // A simple check might not be enough for stale connections.
                  // A more robust implementation might send a probe or check for readability.
                  // For now, we assume is_open() is sufficient.
                  return socket;
               }
            }
         }

         // Create new connection if none are available or they are closed
         return std::make_shared<asio::ip::tcp::socket>(*io_context);
      }

      void return_connection(const std::string& host, uint16_t port, std::shared_ptr<asio::ip::tcp::socket> socket)
      {
         if (!socket || !socket->is_open()) {
            return;
         }

         connection_key key{host, port};
         std::lock_guard<std::mutex> lock(mtx);

         auto& connections = available_connections[key];
         if (connections.size() < 10) { // Limit pool size per host
            connections.push_back(socket);
         }
         // else, the socket is just closed when the shared_ptr goes out of scope.
      }
   };

   // Forward declaration for streaming connection
   struct http_stream_connection;

   // Handler function types for streaming
   using http_data_handler = std::function<void(std::string_view data)>;
   using http_error_handler = std::function<void(std::error_code ec)>;
   using http_connect_handler = std::function<void(const response& headers)>;
   using http_disconnect_handler = std::function<void()>;

   // Streaming HTTP connection handle
   struct http_stream_connection
   {
      std::shared_ptr<asio::ip::tcp::socket> socket;
      std::shared_ptr<asio::steady_timer> timer;
      std::array<uint8_t, 8192> buffer;
      bool is_connected{false};
      std::atomic<bool> should_stop{false};

      // User-facing disconnect. Signals the internal loops to stop.
      // The actual socket closing/pooling is handled by the internal disconnect handler.
      void disconnect()
      {
         bool expected = false;
         if (should_stop.compare_exchange_strong(expected, true)) {
            if (socket && socket->is_open()) {
               std::error_code ec;
               // This cancels pending async operations on the socket, triggering their handlers
               // with asio::error::operation_aborted.
               socket->cancel(ec);
            }
            if (timer) {
               timer->cancel();
            }
         }
      }

      ~http_stream_connection() { disconnect(); }
   };

   struct http_client
   {
      http_client()
         : async_io_context(std::make_shared<asio::io_context>()),
           connection_pool(std::make_shared<http_connection_pool>(async_io_context))
      {
         start_workers();
      }

      ~http_client() { stop_workers(); }

      // Synchronous GET request - truly synchronous, no promises/futures
      std::expected<response, std::error_code> get(std::string_view url,
                                                   const std::unordered_map<std::string, std::string>& headers = {})
      {
         auto url_result = parse_url(url);
         if (!url_result) {
            return std::unexpected(url_result.error());
         }

         return perform_sync_request("GET", *url_result, "", headers);
      }

      // Synchronous POST request - truly synchronous, no promises/futures
      std::expected<response, std::error_code> post(std::string_view url, std::string_view body,
                                                    const std::unordered_map<std::string, std::string>& headers = {})
      {
         auto url_result = parse_url(url);
         if (!url_result) {
            return std::unexpected(url_result.error());
         }

         return perform_sync_request("POST", *url_result, std::string(body), headers);
      }

      // Synchronous JSON POST request
      template <class T>
      std::expected<response, std::error_code> post_json(
         std::string_view url, const T& data, const std::unordered_map<std::string, std::string>& headers = {})
      {
         std::string json_str;
         auto ec = glz::write_json(data, json_str);
         if (ec) {
            return std::unexpected(std::make_error_code(std::errc::invalid_argument));
         }

         auto merged_headers = headers;
         merged_headers["Content-Type"] = "application/json";

         return post(url, json_str, merged_headers);
      }

      // Streaming GET request
      std::shared_ptr<http_stream_connection> get_stream(
         std::string_view url, http_data_handler on_data, http_error_handler on_error,
         const std::unordered_map<std::string, std::string>& headers = {}, http_connect_handler on_connect = {},
         http_disconnect_handler on_disconnect = {}, std::chrono::seconds timeout = std::chrono::seconds{30})
      {
         auto url_result = parse_url(url);
         if (!url_result) {
            asio::post(*async_io_context, [on_error, error = url_result.error()]() { on_error(error); });
            return nullptr;
         }

         return perform_stream_request("GET", *url_result, "", headers, timeout, std::move(on_data),
                                       std::move(on_error), std::move(on_connect), std::move(on_disconnect));
      }

      // Streaming POST request
      std::shared_ptr<http_stream_connection> post_stream(
         std::string_view url, std::string_view body, http_data_handler on_data, http_error_handler on_error,
         const std::unordered_map<std::string, std::string>& headers = {}, http_connect_handler on_connect = {},
         http_disconnect_handler on_disconnect = {}, std::chrono::seconds timeout = std::chrono::seconds{30})
      {
         auto url_result = parse_url(url);
         if (!url_result) {
            asio::post(*async_io_context, [on_error, error = url_result.error()]() { on_error(error); });
            return nullptr;
         }

         return perform_stream_request("POST", *url_result, std::string(body), headers, timeout, std::move(on_data),
                                       std::move(on_error), std::move(on_connect), std::move(on_disconnect));
      }

      // Generic streaming request
      std::shared_ptr<http_stream_connection> request_stream(
         std::string_view method, std::string_view url, std::string_view body, http_data_handler on_data,
         http_error_handler on_error, const std::unordered_map<std::string, std::string>& headers = {},
         http_connect_handler on_connect = {}, http_disconnect_handler on_disconnect = {},
         std::chrono::seconds timeout = std::chrono::seconds{30})
      {
         auto url_result = parse_url(url);
         if (!url_result) {
            asio::post(*async_io_context, [on_error, error = url_result.error()]() { on_error(error); });
            return nullptr;
         }

         return perform_stream_request(std::string(method), *url_result, std::string(body), headers, timeout,
                                       std::move(on_data), std::move(on_error), std::move(on_connect),
                                       std::move(on_disconnect));
      }

      // Asynchronous GET request
      template <typename CompletionHandler>
      void get_async(std::string_view url, const std::unordered_map<std::string, std::string>& headers,
                     CompletionHandler&& handler)
      {
         auto url_result = parse_url(url);
         if (!url_result) {
            asio::post(*async_io_context, [handler = std::forward<CompletionHandler>(handler),
                                           error = url_result.error()] mutable { handler(std::unexpected(error)); });
            return;
         }

         perform_request_async("GET", *url_result, "", headers, std::forward<CompletionHandler>(handler));
      }

      // Overload for get_async without completion handler (returns future)
      std::future<std::expected<response, std::error_code>> get_async(
         std::string_view url, const std::unordered_map<std::string, std::string>& headers = {})
      {
         std::promise<std::expected<response, std::error_code>> promise;
         auto future = promise.get_future();

         get_async(url, headers,
                   [promise = std::move(promise)](std::expected<response, std::error_code> result) mutable {
                      promise.set_value(std::move(result));
                   });

         return future;
      }

      // Asynchronous POST request
      template <typename CompletionHandler>
      void post_async(std::string_view url, std::string_view body,
                      const std::unordered_map<std::string, std::string>& headers, CompletionHandler&& handler)
      {
         auto url_result = parse_url(url);
         if (!url_result) {
            asio::post(*async_io_context, [handler = std::forward<CompletionHandler>(handler),
                                           error = url_result.error()] mutable { handler(std::unexpected(error)); });
            return;
         }

         perform_request_async("POST", *url_result, std::string(body), headers,
                               std::forward<CompletionHandler>(handler));
      }

      // Overload for post_async without completion handler (returns future)
      std::future<std::expected<response, std::error_code>> post_async(
         std::string_view url, std::string_view body, const std::unordered_map<std::string, std::string>& headers = {})
      {
         std::promise<std::expected<response, std::error_code>> promise;
         auto future = promise.get_future();

         post_async(url, body, headers,
                    [promise = std::move(promise)](std::expected<response, std::error_code> result) mutable {
                       promise.set_value(std::move(result));
                    });

         return future;
      }

      // Async JSON POST request
      template <class T, typename CompletionHandler>
      void post_json_async(std::string_view url, const T& data,
                           const std::unordered_map<std::string, std::string>& headers, CompletionHandler&& handler)
      {
         std::string json_str;
         auto ec = glz::write_json(data, json_str);
         if (ec) {
            asio::post(*async_io_context, [handler = std::forward<CompletionHandler>(handler)]() {
               handler(std::unexpected(std::make_error_code(std::errc::invalid_argument)));
            });
            return;
         }

         auto merged_headers = headers;
         merged_headers["Content-Type"] = "application/json";

         post_async(url, json_str, merged_headers, std::forward<CompletionHandler>(handler));
      }

      // Overload for post_json_async without completion handler (returns future)
      template <class T>
      std::future<std::expected<response, std::error_code>> post_json_async(
         std::string_view url, const T& data, const std::unordered_map<std::string, std::string>& headers = {})
      {
         std::promise<std::expected<response, std::error_code>> promise;
         auto future = promise.get_future();

         post_json_async(url, data, headers,
                         [promise = std::move(promise)](std::expected<response, std::error_code> result) mutable {
                            promise.set_value(std::move(result));
                         });

         return future;
      }

     private:
      std::shared_ptr<asio::io_context> async_io_context; // For async operations only
      std::shared_ptr<http_connection_pool> connection_pool;
      std::vector<std::thread> worker_threads;
      std::atomic<bool> running{true};

      void start_workers()
      {
         size_t num_threads = std::max(2u, std::thread::hardware_concurrency());
         worker_threads.reserve(num_threads);

         for (size_t i = 0; i < num_threads; ++i) {
            worker_threads.emplace_back([this]() {
               while (running) {
                  try {
                     async_io_context->run();
                     // Reset context to allow it to be run again after being stopped
                     if (async_io_context->stopped()) {
                        async_io_context->restart();
                     }
                  }
                  catch (const std::exception& e) {
                     std::cerr << "HTTP client worker error: " << e.what() << std::endl;
                  }
               }
            });
         }
      }

      void stop_workers()
      {
         running = false;
         async_io_context->stop();

         for (auto& thread : worker_threads) {
            if (thread.joinable()) {
               thread.join();
            }
         }
      }

      std::shared_ptr<http_stream_connection> perform_stream_request(
         const std::string& method, const url_parts& url, const std::string& body,
         const std::unordered_map<std::string, std::string>& headers, std::chrono::seconds timeout,
         http_data_handler on_data, http_error_handler on_error, http_connect_handler on_connect,
         http_disconnect_handler on_disconnect)
      {
         auto connection = std::make_shared<http_stream_connection>();
         connection->socket = connection_pool->get_connection(url.host, url.port);
         connection->timer = std::make_shared<asio::steady_timer>(*async_io_context);

         // Wrap the disconnect handler to return the socket to the pool
         auto internal_on_disconnect = [this, user_on_disconnect = std::move(on_disconnect), connection, url]() {
            connection->is_connected = false;
            // Call the user's handler if provided
            if (user_on_disconnect) {
               user_on_disconnect();
            }
            // Return the connection to the pool for reuse
            connection_pool->return_connection(url.host, url.port, connection->socket);
         };

         // Set connection timeout
         connection->timer->expires_after(timeout);
         connection->timer->async_wait([connection, on_error, internal_on_disconnect](std::error_code ec) {
            if (!ec && !connection->is_connected && !connection->should_stop) {
               // Mark for stop to prevent race conditions
               connection->disconnect();
               on_error(std::make_error_code(std::errc::timed_out));
               internal_on_disconnect();
            }
         });

         // Check if the socket is already open
         if (connection->socket->is_open()) {
            // If already connected, skip resolve and connect, and just send the request
            asio::post(*async_io_context, [this, url, method, body, headers, connection, on_data = std::move(on_data),
                                           on_error = std::move(on_error), on_connect = std::move(on_connect),
                                           internal_on_disconnect = std::move(internal_on_disconnect)]() mutable {
               send_stream_request(url, method, body, headers, connection, std::move(on_data), std::move(on_error),
                                   std::move(on_connect), std::move(internal_on_disconnect));
            });
         }
         else {
            // If not connected, resolve and connect as before
            auto resolver = std::make_shared<asio::ip::tcp::resolver>(*async_io_context);
            resolver->async_resolve(
               url.host, std::to_string(url.port),
               [this, url, method, body, headers, connection, resolver, on_data = std::move(on_data),
                on_error = std::move(on_error), on_connect = std::move(on_connect),
                internal_on_disconnect = std::move(internal_on_disconnect)](
                  std::error_code ec, asio::ip::tcp::resolver::results_type results) mutable {
                  if (ec || connection->should_stop) {
                     on_error(ec);
                     internal_on_disconnect(); // Ensure cleanup on resolve error
                     return;
                  }

                  asio::async_connect(*connection->socket, results,
                                      [this, url, method, body, headers, connection, on_data = std::move(on_data),
                                       on_error = std::move(on_error), on_connect = std::move(on_connect),
                                       internal_on_disconnect = std::move(internal_on_disconnect)](
                                         std::error_code ec, const asio::ip::tcp::endpoint&) mutable {
                                         if (ec || connection->should_stop) {
                                            on_error(ec);
                                            internal_on_disconnect(); // Ensure cleanup on connect error
                                            return;
                                         }

                                         send_stream_request(url, method, body, headers, connection, std::move(on_data),
                                                             std::move(on_error), std::move(on_connect),
                                                             std::move(internal_on_disconnect));
                                      });
               });
         }

         return connection;
      }

      // Needs to take the wrapped disconnect handler and use keep-alive
      void send_stream_request(const url_parts& url, const std::string& method, const std::string& body,
                               const std::unordered_map<std::string, std::string>& headers,
                               std::shared_ptr<http_stream_connection> connection, http_data_handler on_data,
                               http_error_handler on_error, http_connect_handler on_connect,
                               http_disconnect_handler on_disconnect)
      {
         std::string request_str;
         request_str.reserve(512 + body.size());

         request_str.append(method);
         request_str.append(" ");
         request_str.append(url.path);
         request_str.append(" HTTP/1.1\r\n");
         request_str.append("Host: ");
         request_str.append(url.host);
         request_str.append("\r\n");
         // Use keep-alive to allow the connection to be pooled
         request_str.append("Connection: keep-alive\r\n");

         if (!body.empty()) {
            request_str.append("Content-Length: ");
            request_str.append(std::to_string(body.size()));
            request_str.append("\r\n");
         }

         for (const auto& [name, value] : headers) {
            request_str.append(name);
            request_str.append(": ");
            request_str.append(value);
            request_str.append("\r\n");
         }
         request_str.append("\r\n");
         request_str.append(body);

         auto request_buffer = std::make_shared<std::string>(std::move(request_str));

         asio::async_write(*connection->socket, asio::buffer(*request_buffer),
                           [this, connection, request_buffer, on_data = std::move(on_data),
                            on_error = std::move(on_error), on_connect = std::move(on_connect),
                            on_disconnect = std::move(on_disconnect)](std::error_code ec, std::size_t) mutable {
                              if (ec || connection->should_stop) {
                                 on_error(ec);
                                 if (on_disconnect) on_disconnect();
                                 return;
                              }

                              read_stream_response(connection, std::move(on_data), std::move(on_error),
                                                   std::move(on_connect), std::move(on_disconnect));
                           });
      }

      void read_stream_response(std::shared_ptr<http_stream_connection> connection, http_data_handler on_data,
                                http_error_handler on_error, http_connect_handler on_connect,
                                http_disconnect_handler on_disconnect)
      {
         auto buffer = std::make_shared<asio::streambuf>();

         asio::async_read_until(
            *connection->socket, *buffer, "\r\n\r\n",
            [this, connection, buffer, on_data = std::move(on_data), on_error = std::move(on_error),
             on_connect = std::move(on_connect),
             on_disconnect = std::move(on_disconnect)](std::error_code ec, std::size_t) mutable {
               if (ec || connection->should_stop) {
                  on_error(ec);
                  if (on_disconnect) on_disconnect(); // Cleanup on header read error
                  return;
               }

               std::istream response_stream(buffer.get());
               std::string status_line;
               std::getline(response_stream, status_line);
               if (!status_line.empty() && status_line.back() == '\r') {
                  status_line.pop_back();
               }

               auto parsed_status = parse_http_status_line(status_line);
               if (!parsed_status) {
                  on_error(parsed_status.error());
                  if (on_disconnect) on_disconnect();
                  return;
               }

               response response_headers;
               response_headers.status_code = parsed_status->status_code;

               std::string header_line;
               while (std::getline(response_stream, header_line) && header_line != "\r") {
                  if (header_line.back() == '\r') {
                     header_line.pop_back();
                  }
                  auto colon_pos = header_line.find(':');
                  if (colon_pos != std::string::npos) {
                     std::string name = header_line.substr(0, colon_pos);
                     std::string value = header_line.substr(colon_pos + 1);
                     value.erase(0, value.find_first_not_of(" \t"));
                     response_headers.response_headers[name] = value;
                  }
               }

               connection->is_connected = true;
               connection->timer->cancel();

               if (on_connect) {
                  on_connect(response_headers);
               }

               if (parsed_status->status_code >= 400) {
                  // Status code indicates an error, so we report it and end the stream.
                  on_error(std::make_error_code(std::errc::connection_refused)); // Generic error for now
                  if (on_disconnect) on_disconnect();
                  return;
               }

               if (buffer->in_avail() > 0) {
                  std::string initial_data{std::istreambuf_iterator<char>(buffer.get()),
                                           std::istreambuf_iterator<char>()};
                  if (!initial_data.empty()) {
                     on_data(initial_data);
                  }
               }

               start_stream_reading(connection, std::move(on_data), std::move(on_error), std::move(on_disconnect));
            });
      }

      void start_stream_reading(std::shared_ptr<http_stream_connection> connection, http_data_handler on_data,
                                http_error_handler on_error, http_disconnect_handler on_disconnect)
      {
         if (connection->should_stop) {
            if (on_disconnect) on_disconnect();
            return;
         }

         connection->socket->async_read_some(
            asio::buffer(connection->buffer),
            [this, connection, on_data, on_error, on_disconnect](std::error_code ec, std::size_t bytes_transferred) {
               if (ec || connection->should_stop) {
                  if (ec != asio::error::eof && ec != asio::error::operation_aborted && !connection->should_stop) {
                     on_error(ec);
                  }
                  // This is the primary exit point for a stream; always call disconnect here.
                  if (on_disconnect) on_disconnect();
                  return;
               }

               // Call data handler with received data
               std::string_view data(reinterpret_cast<const char*>(connection->buffer.data()), bytes_transferred);
               on_data(data);

               // Continue reading
               start_stream_reading(connection, on_data, on_error, on_disconnect);
            });
      }

      std::expected<response, std::error_code> perform_sync_request(
         const std::string& method, const url_parts& url, const std::string& body,
         const std::unordered_map<std::string, std::string>& headers)
      {
         auto socket = connection_pool->get_connection(url.host, url.port);

         try {
            // If socket is not connected, connect it synchronously
            if (!socket->is_open()) {
               asio::ip::tcp::resolver resolver(*async_io_context);
               auto endpoints = resolver.resolve(url.host, std::to_string(url.port));
               asio::connect(*socket, endpoints);
            }

            // Build HTTP request
            std::string request_str;
            request_str.append(method);
            request_str.append(" ");
            request_str.append(url.path);
            request_str.append(" HTTP/1.1\r\n");
            request_str.append("Host: ");
            request_str.append(url.host);
            request_str.append("\r\n");
            request_str.append("Connection: keep-alive\r\n"); // Keep connection alive for reuse

            if (!body.empty()) {
               request_str.append("Content-Length: ");
               request_str.append(std::to_string(body.size()));
               request_str.append("\r\n");
            }

            for (const auto& [name, value] : headers) {
               request_str.append(name);
               request_str.append(": ");
               request_str.append(value);
               request_str.append("\r\n");
            }

            request_str.append("\r\n");
            request_str.append(body);

            // Send request synchronously
            asio::write(*socket, asio::buffer(request_str));

            // Read response headers synchronously
            asio::streambuf response_buffer;
            asio::read_until(*socket, response_buffer, "\r\n\r\n");

            // Parse status line
            std::istream response_stream(&response_buffer);
            std::string status_line;
            std::getline(response_stream, status_line);

            if (!status_line.empty() && status_line.back() == '\r') {
               status_line.pop_back();
            }

            auto parsed_status = parse_http_status_line(status_line);
            if (!parsed_status) {
               return std::unexpected(parsed_status.error());
            }

            // Parse headers
            std::unordered_map<std::string, std::string> response_headers;
            std::string header_line;
            while (std::getline(response_stream, header_line) && header_line != "\r") {
               if (header_line.back() == '\r') {
                  header_line.pop_back();
               }

               auto colon_pos = header_line.find(':');
               if (colon_pos != std::string::npos) {
                  std::string name = header_line.substr(0, colon_pos);
                  std::string value = header_line.substr(colon_pos + 1);
                  value.erase(0, value.find_first_not_of(" \t"));
                  response_headers[name] = value;
               }
            }

            size_t content_length = 0;
            auto it = response_headers.find("Content-Length");
            if (it != response_headers.end()) {
               content_length = std::stoull(it->second);
            }

            std::string response_body;
            response_body.reserve(content_length);

            // Copy what's already in the buffer
            if (response_buffer.size() > 0) {
               response_body.append(asio::buffers_begin(response_buffer.data()),
                                    asio::buffers_end(response_buffer.data()));
            }

            // Read remaining body if necessary
            if (response_body.length() < content_length) {
               std::error_code ec;
               asio::read(*socket, response_buffer, asio::transfer_exactly(content_length - response_body.length()),
                          ec);
               if (ec && ec != asio::error::eof) {
                  return std::unexpected(ec);
               }
               response_body.append(asio::buffers_begin(response_buffer.data()),
                                    asio::buffers_end(response_buffer.data()));
            }

            response resp;
            resp.status_code = parsed_status->status_code;
            resp.response_headers = std::move(response_headers);
            resp.response_body = std::move(response_body);

            // Return connection to pool if it's still usable
            auto connection_header = resp.response_headers.find("Connection");
            if (connection_header == resp.response_headers.end() || connection_header->second != "close") {
               connection_pool->return_connection(url.host, url.port, socket);
            }
            // else the server will close it, so we don't return it.

            return resp;
         }
         catch (const std::system_error& e) {
            return std::unexpected(e.code());
         }
         catch (...) {
            return std::unexpected(std::make_error_code(std::errc::connection_refused));
         }
      }

      template <typename CompletionHandler>
      void perform_request_async(const std::string& method, const url_parts& url, const std::string& body,
                                 const std::unordered_map<std::string, std::string>& headers,
                                 CompletionHandler&& handler)
      {
         auto socket = connection_pool->get_connection(url.host, url.port);

         if (socket->is_open()) {
            send_request(socket, method, url, body, headers, std::forward<CompletionHandler>(handler));
         }
         else {
            auto resolver = std::make_shared<asio::ip::tcp::resolver>(*async_io_context);
            resolver->async_resolve(
               url.host, std::to_string(url.port),
               [this, socket, resolver, method, url, body, headers, handler = std::forward<CompletionHandler>(handler)](
                  std::error_code ec, asio::ip::tcp::resolver::results_type results) mutable {
                  if (ec) {
                     handler(std::unexpected(ec));
                     return;
                  }

                  connect_and_send(socket, results, method, url, body, headers, std::move(handler));
               });
         }
      }

      template <typename CompletionHandler>
      void connect_and_send(std::shared_ptr<asio::ip::tcp::socket> socket,
                            asio::ip::tcp::resolver::results_type results, const std::string& method,
                            const url_parts& url, const std::string& body,
                            const std::unordered_map<std::string, std::string>& headers, CompletionHandler&& handler)
      {
         asio::async_connect(
            *socket, results,
            [this, socket, method, url, body, headers, handler = std::forward<CompletionHandler>(handler)](
               std::error_code ec, const asio::ip::tcp::endpoint&) mutable {
               if (ec) {
                  handler(std::unexpected(ec));
                  return;
               }

               send_request(socket, method, url, body, headers, std::move(handler));
            });
      }

      template <typename CompletionHandler>
      void send_request(std::shared_ptr<asio::ip::tcp::socket> socket, const std::string& method, const url_parts& url,
                        const std::string& body, const std::unordered_map<std::string, std::string>& headers,
                        CompletionHandler&& handler)
      {
         // Build HTTP request
         std::string request_str;
         request_str.append(method);
         request_str.append(" ");
         request_str.append(url.path);
         request_str.append(" HTTP/1.1\r\n");
         request_str.append("Host: ");
         request_str.append(url.host);
         request_str.append("\r\n");
         request_str.append("Connection: keep-alive\r\n");

         if (!body.empty()) {
            request_str.append("Content-Length: ");
            request_str.append(std::to_string(body.size()));
            request_str.append("\r\n");
         }

         for (const auto& [name, value] : headers) {
            request_str.append(name);
            request_str.append(": ");
            request_str.append(value);
            request_str.append("\r\n");
         }

         request_str.append("\r\n");
         request_str.append(body);

         // Use shared_ptr to keep request string alive during async operation
         auto request_str_ptr = std::make_shared<std::string>(std::move(request_str));
         asio::async_write(*socket, asio::buffer(*request_str_ptr),
                           [this, socket, request_str_ptr, url, handler = std::forward<CompletionHandler>(handler)](
                              std::error_code ec, std::size_t) mutable {
                              if (ec) {
                                 handler(std::unexpected(ec));
                                 return;
                              }

                              read_response(socket, url, std::move(handler));
                           });
      }

      template <typename CompletionHandler>
      void read_response(std::shared_ptr<asio::ip::tcp::socket> socket, const url_parts& url,
                         CompletionHandler&& handler)
      {
         auto buffer = std::make_shared<asio::streambuf>();

         asio::async_read_until(*socket, *buffer, "\r\n\r\n",
                                [this, socket, buffer, url, handler = std::forward<CompletionHandler>(handler)](
                                   std::error_code ec, std::size_t) mutable {
                                   if (ec) {
                                      handler(std::unexpected(ec));
                                      return;
                                   }

                                   parse_and_read_body(socket, buffer, url, std::move(handler));
                                });
      }

      template <typename CompletionHandler>
      void parse_and_read_body(std::shared_ptr<asio::ip::tcp::socket> socket, std::shared_ptr<asio::streambuf> buffer,
                               const url_parts& url, CompletionHandler&& handler)
      {
         std::istream response_stream(buffer.get());
         std::string status_line;
         std::getline(response_stream, status_line);

         if (!status_line.empty() && status_line.back() == '\r') {
            status_line.pop_back();
         }

         auto parsed_status = parse_http_status_line(status_line);
         if (!parsed_status) {
            handler(std::unexpected(parsed_status.error()));
            return;
         }

         // Parse headers
         std::unordered_map<std::string, std::string> response_headers;
         std::string header_line;
         size_t content_length = 0;
         while (std::getline(response_stream, header_line) && header_line != "\r") {
            if (header_line.back() == '\r') {
               header_line.pop_back();
            }

            auto colon_pos = header_line.find(':');
            if (colon_pos != std::string::npos) {
               std::string name = header_line.substr(0, colon_pos);
               std::string value = header_line.substr(colon_pos + 1);
               value.erase(0, value.find_first_not_of(" \t"));
               if (name == "Content-Length") {
                  content_length = std::stoull(value);
               }
               response_headers[name] = value;
            }
         }

         // Read response body
         size_t body_already_in_buffer = buffer->size();
         size_t remaining_to_read =
            (content_length > body_already_in_buffer) ? (content_length - body_already_in_buffer) : 0;

         asio::async_read(
            *socket, *buffer, asio::transfer_exactly(remaining_to_read),
            [this, socket, buffer, url, status_code = parsed_status->status_code,
             response_headers = std::move(response_headers),
             handler = std::forward<CompletionHandler>(handler)](std::error_code ec, std::size_t) mutable {
               // EOF is expected for HTTP/1.1 with Connection: close
               if (ec && ec != asio::error::eof) {
                  handler(std::unexpected(ec));
                  return;
               }

               std::string body{std::istreambuf_iterator<char>(buffer.get()), std::istreambuf_iterator<char>()};

               response resp;
               resp.status_code = status_code;
               resp.response_headers = std::move(response_headers);
               resp.response_body = std::move(body);

               // Return connection to pool if it's still usable
               auto connection_header = resp.response_headers.find("Connection");
               if (connection_header == resp.response_headers.end() || connection_header->second != "close") {
                  connection_pool->return_connection(url.host, url.port, socket);
               }

               handler(std::move(resp));
            });
      }
   };
}
