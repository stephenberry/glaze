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
static_assert(false, "standalone or boost asio must be included to use glaze/ext/glaze_asio.hpp");
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
                  return socket;
               }
            }
         }

         // Create new connection
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
      }
   };

   // Client implementation using connection pooling and async operations
   struct http_client
   {
      http_client()
         : io_context(std::make_shared<asio::io_context>()),
           connection_pool(std::make_shared<http_connection_pool>(io_context))
      {
         // Start worker threads
         start_workers();
      }

      ~http_client() { stop_workers(); }

      // Synchronous GET request
      std::expected<response, std::error_code> get(std::string_view url,
                                                   const std::unordered_map<std::string, std::string>& headers = {})
      {
         auto promise = std::make_shared<std::promise<std::expected<response, std::error_code>>>();
         auto future = promise->get_future();

         get_async(url, headers, [promise](std::expected<response, std::error_code> result) {
            promise->set_value(std::move(result));
         });

         return future.get();
      }

      // Synchronous POST request
      std::expected<response, std::error_code> post(std::string_view url, std::string_view body,
                                                    const std::unordered_map<std::string, std::string>& headers = {})
      {
         auto promise = std::make_shared<std::promise<std::expected<response, std::error_code>>>();
         auto future = promise->get_future();

         post_async(url, body, headers, [promise](std::expected<response, std::error_code> result) {
            promise->set_value(std::move(result));
         });

         return future.get();
      }

      // Asynchronous GET request
      template <typename CompletionHandler>
      void get_async(std::string_view url, const std::unordered_map<std::string, std::string>& headers,
                     CompletionHandler&& handler)
      {
         auto url_result = parse_url(url);
         if (!url_result) {
            asio::post(*io_context, [handler = std::forward<CompletionHandler>(handler), error = url_result.error()]() {
               handler(std::unexpected(error));
            });
            return;
         }

         perform_request_async("GET", *url_result, "", headers, std::forward<CompletionHandler>(handler));
      }

      // Overload for get_async without completion handler (returns future)
      std::future<std::expected<response, std::error_code>> get_async(
         std::string_view url, const std::unordered_map<std::string, std::string>& headers = {})
      {
         auto promise = std::make_shared<std::promise<std::expected<response, std::error_code>>>();
         auto future = promise->get_future();

         get_async(url, headers, [promise](std::expected<response, std::error_code> result) {
            promise->set_value(std::move(result));
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
            asio::post(*io_context, [handler = std::forward<CompletionHandler>(handler), error = url_result.error()]() {
               handler(std::unexpected(error));
            });
            return;
         }

         perform_request_async("POST", *url_result, std::string(body), headers,
                               std::forward<CompletionHandler>(handler));
      }

      // Overload for post_async without completion handler (returns future)
      std::future<std::expected<response, std::error_code>> post_async(
         std::string_view url, std::string_view body, const std::unordered_map<std::string, std::string>& headers = {})
      {
         auto promise = std::make_shared<std::promise<std::expected<response, std::error_code>>>();
         auto future = promise->get_future();

         post_async(url, body, headers, [promise](std::expected<response, std::error_code> result) {
            promise->set_value(std::move(result));
         });

         return future;
      }

      // JSON POST request
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

      // Async JSON POST request
      template <class T, typename CompletionHandler>
      void post_json_async(std::string_view url, const T& data,
                           const std::unordered_map<std::string, std::string>& headers, CompletionHandler&& handler)
      {
         std::string json_str;
         auto ec = glz::write_json(data, json_str);
         if (ec) {
            asio::post(*io_context, [handler = std::forward<CompletionHandler>(handler)]() {
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
         auto promise = std::make_shared<std::promise<std::expected<response, std::error_code>>>();
         auto future = promise->get_future();

         post_json_async(url, data, headers, [promise](std::expected<response, std::error_code> result) {
            promise->set_value(std::move(result));
         });

         return future;
      }

     private:
      std::shared_ptr<asio::io_context> io_context;
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
                     io_context->run();
                     break; // Normal exit
                  }
                  catch (const std::exception& e) {
                     // Log error and continue
                     std::cerr << "HTTP client worker error: " << e.what() << std::endl;
                  }
               }
            });
         }
      }

      void stop_workers()
      {
         running = false;
         io_context->stop();

         for (auto& thread : worker_threads) {
            if (thread.joinable()) {
               thread.join();
            }
         }
      }

      template <typename CompletionHandler>
      void perform_request_async(const std::string& method, const url_parts& url, const std::string& body,
                                 const std::unordered_map<std::string, std::string>& headers,
                                 CompletionHandler&& handler)
      {
         auto socket = connection_pool->get_connection(url.host, url.port);
         auto resolver = std::make_shared<asio::ip::tcp::resolver>(*io_context);

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

      template <typename CompletionHandler>
      void connect_and_send(std::shared_ptr<asio::ip::tcp::socket> socket,
                            asio::ip::tcp::resolver::results_type results, const std::string& method,
                            const url_parts& url, const std::string& body,
                            const std::unordered_map<std::string, std::string>& headers, CompletionHandler&& handler)
      {
         // Check if socket is already connected
         if (socket->is_open()) {
            send_request(socket, method, url, body, headers, std::forward<CompletionHandler>(handler));
            return;
         }

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
         auto request_str = std::make_shared<std::string>();
         request_str->append(method);
         request_str->append(" ");
         request_str->append(url.path);
         request_str->append(" HTTP/1.1\r\n");
         request_str->append("Host: ");
         request_str->append(url.host);
         request_str->append("\r\n");
         request_str->append("Connection: keep-alive\r\n");

         if (!body.empty()) {
            request_str->append("Content-Length: ");
            request_str->append(std::to_string(body.size()));
            request_str->append("\r\n");
         }

         for (const auto& [name, value] : headers) {
            request_str->append(name);
            request_str->append(": ");
            request_str->append(value);
            request_str->append("\r\n");
         }

         request_str->append("\r\n");
         request_str->append(body);

         asio::async_write(*socket, asio::buffer(*request_str),
                           [this, socket, request_str, url, handler = std::forward<CompletionHandler>(handler)](
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

         // Read response body
         asio::async_read(
            *socket, *buffer, asio::transfer_all(),
            [this, socket, buffer, url, status_code = parsed_status->status_code, response_headers,
             handler = std::forward<CompletionHandler>(handler)](std::error_code ec, std::size_t) mutable {
               // EOF is expected for HTTP/1.1 with Connection: close
               if (ec && ec != asio::error::eof) {
                  handler(std::unexpected(ec));
                  return;
               }

               std::string body{std::istreambuf_iterator<char>(buffer.get()), std::istreambuf_iterator<char>()};

               response resp;
               resp.status_code = status_code;
               resp.response_headers = response_headers;
               resp.response_body = std::move(body);

               // Return connection to pool if it's still usable
               auto connection_header = response_headers.find("Connection");
               if (connection_header == response_headers.end() || connection_header->second != "close") {
                  connection_pool->return_connection(url.host, url.port, socket);
               }

               handler(std::move(resp));
            });
      }
   };
}
