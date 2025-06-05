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
#include <sstream>
#include <source_location>
#include <thread>
#include <unordered_map>

#include "glaze/net/cors.hpp"
#include "glaze/net/http_router.hpp"
#include "glaze/net/websocket_connection.hpp"

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

// Conditionally include SSL headers only when needed
#ifdef GLZ_ENABLE_SSL
#if __has_include(<asio.hpp>) && !defined(GLZ_USE_BOOST_ASIO)
#include <asio/ssl.hpp>
#elif __has_include(<boost/asio.hpp>)
#include <boost/asio/ssl.hpp>
#endif
#endif

namespace glz
{
   // Server implementation using non-blocking asio with WebSocket support
   template<bool EnableTLS = false>
   struct http_server
   {
      // Socket type abstraction
      using socket_type = std::conditional_t<EnableTLS, 
#ifdef GLZ_ENABLE_SSL
         asio::ssl::stream<asio::ip::tcp::socket>,
#else
         asio::ip::tcp::socket,
#endif
         asio::ip::tcp::socket>;

      inline http_server() : io_context(std::make_unique<asio::io_context>())
      {
         error_handler = [](std::error_code ec, std::source_location loc) {
            std::fprintf(stderr, "Error at %s:%d: %s\n", loc.file_name(), static_cast<int>(loc.line()),
                         ec.message().c_str());
         };

         // Initialize SSL context for TLS-enabled servers
         if constexpr (EnableTLS) {
#ifdef GLZ_ENABLE_SSL
            ssl_context = std::make_unique<asio::ssl::context>(asio::ssl::context::tlsv12);
            ssl_context->set_default_verify_paths();
#else
            static_assert(!EnableTLS, "TLS support requires GLZ_ENABLE_SSL to be defined and OpenSSL to be available");
#endif
         }
      }

      inline ~http_server()
      {
         if (running) {
            stop();
         }
      }

      inline http_server& bind(std::string_view address, uint16_t port)
      {
         try {
            asio::ip::tcp::endpoint endpoint(asio::ip::make_address(address), port);
            acceptor = std::make_unique<asio::ip::tcp::acceptor>(*io_context, endpoint);
         }
         catch (const std::exception& e) {
            error_handler(std::make_error_code(std::errc::address_in_use), std::source_location::current());
         }
         return *this;
      }

      inline http_server& bind(uint16_t port) { return bind("0.0.0.0", port); }

      inline void start(size_t num_threads = 0)
      {
         if (running || !acceptor) {
            return;
         }

         running = true;

         // Use hardware concurrency if not specified
         if (num_threads == 0) {
            num_threads = std::thread::hardware_concurrency();
         }

         // Start the acceptor
         do_accept();

         // Start worker threads
         threads.reserve(num_threads);
         for (size_t i = 0; i < num_threads; ++i) {
            threads.emplace_back([this] {
               try {
                  io_context->run();
               }
               catch (const std::exception& e) {
                  error_handler(std::make_error_code(std::errc::operation_canceled), std::source_location::current());
               }
            });
         }
      }

      inline void stop()
      {
         if (!running) {
            return;
         }

         running = false;

         // Stop accepting new connections
         if (acceptor) {
            acceptor->close();
         }

         // Stop the io_context and join all threads
         io_context->stop();

         for (auto& thread : threads) {
            if (thread.joinable()) {
               thread.join();
            }
         }

         threads.clear();

         // Reset for potential restart
         io_context = std::make_unique<asio::io_context>();
      }

      inline http_server& mount(std::string_view base_path, const http_router& router)
      {
         // Mount all routes from the router at the specified base path
         for (const auto& [path, method_handlers] : router.routes) {
            std::string full_path = std::string(base_path);
            if (!full_path.empty() && full_path.back() == '/') {
               full_path.pop_back();
            }
            full_path += path;

            for (const auto& [method, handle] : method_handlers) {
               root_router.route(method, full_path, handle);
            }
         }

         // Add middleware
         for (const auto& middleware : router.middlewares) {
            root_router.use(middleware);
         }

         return *this;
      }

      inline http_router& route(http_method method, std::string_view path, handler handle)
      {
         return root_router.route(method, path, handle);
      }

      inline http_router& get(std::string_view path, handler handle) { return root_router.get(path, handle); }

      inline http_router& post(std::string_view path, handler handle) { return root_router.post(path, handle); }

      inline http_server& on_error(error_handler handle)
      {
         error_handler = std::move(handle);
         return *this;
      }

      /**
       * @brief Enable CORS with default configuration (allows all origins)
       *
       * This is suitable for development environments
       *
       * @return Reference to this server for method chaining
       */
      http_server& enable_cors()
      {
         root_router.use(glz::simple_cors());
         return *this;
      }

      /**
       * @brief Enable CORS with custom configuration
       *
       * @param config The CORS configuration to use
       * @return Reference to this server for method chaining
       */
      http_server& enable_cors(const glz::cors_config& config)
      {
         root_router.use(glz::create_cors_middleware(config));
         return *this;
      }

      /**
       * @brief Enable CORS for specific origins
       *
       * This is suitable for production environments where you want to restrict
       * which origins can access your API
       *
       * @param origins Vector of allowed origins (e.g., {"https://example.com", "https://app.example.com"})
       * @param allow_credentials Whether to allow credentials (cookies, auth headers)
       * @return Reference to this server for method chaining
       */
      http_server& enable_cors(const std::vector<std::string>& origins, bool allow_credentials = false)
      {
         root_router.use(glz::restrictive_cors(origins, allow_credentials));
         return *this;
      }

      /**
       * @brief Register a WebSocket handler for a specific path
       *
       * @param path The path to handle WebSocket connections on
       * @param server The WebSocket server instance to handle connections
       * @return Reference to this http_server for method chaining
       */
      inline http_server& websocket(std::string_view path, std::shared_ptr<websocket_server> server)
      {
         websocket_handlers_[std::string(path)] = server;
         return *this;
      }

      /**
       * @brief Load SSL certificate and private key for HTTPS servers
       * 
       * @param cert_file Path to the certificate file (PEM format)
       * @param key_file Path to the private key file (PEM format)
       * @return Reference to this server for method chaining
       */
      inline http_server& load_certificate(const std::string& cert_file, const std::string& key_file)
      {
         if constexpr (EnableTLS) {
#ifdef GLZ_ENABLE_SSL
            ssl_context->use_certificate_chain_file(cert_file);
            ssl_context->use_private_key_file(key_file, asio::ssl::context::pem);
#endif
         }
         return *this;
      }

      /**
       * @brief Set SSL verification mode
       * 
       * @param mode SSL verification mode
       * @return Reference to this server for method chaining
       */
      inline http_server& set_ssl_verify_mode(int mode)
      {
         if constexpr (EnableTLS) {
#ifdef GLZ_ENABLE_SSL
            ssl_context->set_verify_mode(mode);
#endif
         }
         return *this;
      }

     private:
      std::unique_ptr<asio::io_context> io_context;
      std::unique_ptr<asio::ip::tcp::acceptor> acceptor;
      std::vector<std::thread> threads;
      http_router root_router;
      bool running = false;
      glz::error_handler error_handler;
      std::unordered_map<std::string, std::shared_ptr<websocket_server>> websocket_handlers_;

#ifdef GLZ_ENABLE_SSL
      std::conditional_t<EnableTLS, std::unique_ptr<asio::ssl::context>, std::monostate> ssl_context;
#endif

      inline void do_accept()
      {
         acceptor->async_accept([this](std::error_code ec, asio::ip::tcp::socket socket) {
            if (!ec) {
               // Process the connection in a separate coroutine
               process_request(std::move(socket));
            }
            else {
               error_handler(ec, std::source_location::current());
            }

            // Continue accepting if still running
            if (running) {
               do_accept();
            }
         });
      }

      inline void process_request(asio::ip::tcp::socket socket)
      {
         // Capture socket in a shared_ptr for async operations
         auto socket_ptr = std::make_shared<asio::ip::tcp::socket>(std::move(socket));
         auto remote_endpoint = socket_ptr->remote_endpoint();

         // Buffer for the request
         auto buffer = std::make_shared<asio::streambuf>();

         // Read the request asynchronously
         asio::async_read_until(
            *socket_ptr, *buffer, "\r\n\r\n",
            [this, socket_ptr, buffer, remote_endpoint](std::error_code ec, std::size_t /*bytes_transferred*/) {
               if (ec) {
                  error_handler(ec, std::source_location::current());
                  return;
               }

               // Parse the request headers
               std::istream request_stream(buffer.get());
               std::string request_line;
               std::getline(request_stream, request_line);

               // Remove carriage return if present
               if (!request_line.empty() && request_line.back() == '\r') {
                  request_line.pop_back();
               }

               // Parse method, target, and HTTP version using manual parsing
               if (request_line.empty()) {
                  send_error_response(socket_ptr, 400, "Bad Request");
                  return;
               }

               // Find the first space to separate method from target
               size_t first_space = request_line.find(' ');
               if (first_space == std::string::npos) {
                  send_error_response(socket_ptr, 400, "Bad Request");
                  return;
               }

               // Extract method
               std::string method_str = request_line.substr(0, first_space);
               // Validate method (must be only word characters: [a-zA-Z0-9_])
               if (method_str.empty() || !std::all_of(method_str.begin(), method_str.end(),
                                                      [](char c) { return std::isalnum(c) || c == '_'; })) {
                  send_error_response(socket_ptr, 400, "Bad Request");
                  return;
               }

               // Find the second space to separate target from HTTP version
               size_t second_space = request_line.find(' ', first_space + 1);
               if (second_space == std::string::npos) {
                  send_error_response(socket_ptr, 400, "Bad Request");
                  return;
               }

               // Extract target
               std::string target = request_line.substr(first_space + 1, second_space - first_space - 1);
               if (target.empty() || target.find(' ') != std::string::npos) {
                  send_error_response(socket_ptr, 400, "Bad Request");
                  return;
               }

               // Extract HTTP version
               std::string http_version_part = request_line.substr(second_space + 1);

               // Validate HTTP version format
               if (http_version_part.size() < 7 || http_version_part.substr(0, 5) != "HTTP/") {
                  send_error_response(socket_ptr, 400, "Bad Request");
                  return;
               }

               // Extract version number and verify format (must be digit.digit)
               std::string version_number = http_version_part.substr(5);
               size_t decimal_point = version_number.find('.');
               if (decimal_point == std::string::npos || decimal_point == 0 ||
                   decimal_point == version_number.length() - 1) {
                  send_error_response(socket_ptr, 400, "Bad Request");
                  return;
               }

               // Verify major and minor version numbers are digits
               std::string major_version = version_number.substr(0, decimal_point);
               std::string minor_version = version_number.substr(decimal_point + 1);

               if (major_version.empty() || !std::all_of(major_version.begin(), major_version.end(), ::isdigit) ||
                   minor_version.empty() || !std::all_of(minor_version.begin(), minor_version.end(), ::isdigit)) {
                  send_error_response(socket_ptr, 400, "Bad Request");
                  return;
               }

               auto method_opt = from_string(method_str);
               if (!method_opt) {
                  // Unsupported method
                  send_error_response(socket_ptr, 501, "Not Implemented");
                  return;
               }

               // Parse headers
               std::unordered_map<std::string, std::string> headers;
               std::string header_line;
               while (std::getline(request_stream, header_line) && header_line != "\r") {
                  if (header_line.back() == '\r') {
                     header_line.pop_back();
                  }

                  auto colon_pos = header_line.find(':');
                  if (colon_pos != std::string::npos) {
                     std::string name = header_line.substr(0, colon_pos);
                     std::string value = header_line.substr(colon_pos + 1);

                     // Trim leading whitespace from value
                     value.erase(0, value.find_first_not_of(" \t"));

                     // Convert header name to lowercase for easier matching
                     std::transform(name.begin(), name.end(), name.begin(), ::tolower);

                     headers[name] = value;
                  }
               }

               // Check if this is a WebSocket upgrade request
               if (is_websocket_upgrade(headers)) {
                  handle_websocket_upgrade(socket_ptr, *method_opt, target, headers, remote_endpoint);
                  return;
               }

               // Check if there's a Content-Length header
               std::size_t content_length = 0;
               auto content_length_it = headers.find("content-length");
               if (content_length_it != headers.end()) {
                  try {
                     content_length = std::stoul(content_length_it->second);
                  }
                  catch (const std::exception&) {
                     send_error_response(socket_ptr, 400, "Bad Request");
                     return;
                  }
               }

               // Read the request body if needed
               if (content_length > 0) {
                  // Create a string for the body with proper capacity
                  std::string body;
                  body.reserve(content_length);

                  // Create a stream for reading from the buffer
                  std::istream request_stream(buffer.get());

                  // Calculate how much of the body we've already read
                  std::size_t available = buffer->in_avail();
                  std::size_t to_read = std::min(available, content_length);

                  if (to_read > 0) {
                     // Read what's already in the buffer
                     body.resize(to_read);
                     request_stream.read(&body[0], to_read);
                  }

                  if (to_read < content_length) {
                     // Need to read more data
                     asio::async_read(*socket_ptr, *buffer, asio::transfer_exactly(content_length - to_read),
                                      [this, socket_ptr, buffer, method_opt, target, headers, remote_endpoint,
                                       content_length, body = std::move(body),
                                       to_read](std::error_code ec, std::size_t /*bytes_transferred*/) mutable {
                                         if (ec) {
                                            error_handler(ec, std::source_location::current());
                                            return;
                                         }

                                         // Read the remaining data
                                         std::istream request_stream(buffer.get());
                                         body.resize(content_length);
                                         request_stream.read(&body[to_read], content_length - to_read);

                                         process_full_request(socket_ptr, *method_opt, target, headers, std::move(body),
                                                              remote_endpoint);
                                      });
                  }
                  else {
                     // We already have the full body
                     process_full_request(socket_ptr, *method_opt, target, headers, std::move(body), remote_endpoint);
                  }
               }
               else {
                  // No body, process the request immediately
                  process_full_request(socket_ptr, *method_opt, target, headers, "", remote_endpoint);
               }
            });
      }

      inline bool is_websocket_upgrade(const std::unordered_map<std::string, std::string>& headers)
      {
         auto upgrade_it = headers.find("upgrade");
         if (upgrade_it == headers.end()) return false;

         auto connection_it = headers.find("connection");
         if (connection_it == headers.end()) return false;

         // Check if upgrade header contains "websocket" (case-insensitive)
         std::string upgrade_value = upgrade_it->second;
         std::transform(upgrade_value.begin(), upgrade_value.end(), upgrade_value.begin(), ::tolower);
         if (upgrade_value.find("websocket") == std::string::npos) return false;

         // Check if connection header contains "upgrade" (case-insensitive)
         std::string connection_value = connection_it->second;
         std::transform(connection_value.begin(), connection_value.end(), connection_value.begin(), ::tolower);
         return connection_value.find("upgrade") != std::string::npos;
      }

      inline void handle_websocket_upgrade(std::shared_ptr<asio::ip::tcp::socket> socket, http_method method,
                                           const std::string& target,
                                           const std::unordered_map<std::string, std::string>& headers,
                                           asio::ip::tcp::endpoint remote_endpoint)
      {
         // Find matching WebSocket handler
         auto ws_it = websocket_handlers_.find(target);
         if (ws_it == websocket_handlers_.end()) {
            send_error_response(socket, 404, "Not Found");
            return;
         }

         // Create request object for WebSocket handler
         request req;
         req.method = method;
         req.target = target;
         req.headers = headers;
         req.remote_ip = remote_endpoint.address().to_string();
         req.remote_port = remote_endpoint.port();

         // Create WebSocket connection and start it
         // Need to include websocket_connection.hpp for this to work
         auto ws_conn = std::make_shared<websocket_connection>(std::move(*socket), ws_it->second.get());
         ws_conn->start(req);
      }

      inline void process_full_request(std::shared_ptr<asio::ip::tcp::socket> socket, http_method method,
                                       const std::string& target,
                                       const std::unordered_map<std::string, std::string>& headers, std::string body,
                                       asio::ip::tcp::endpoint remote_endpoint)
      {
         // Create the request object
         request request;
         request.method = method;
         request.target = target;
         request.headers = headers;
         request.body = std::move(body);
         request.remote_ip = remote_endpoint.address().to_string();
         request.remote_port = remote_endpoint.port();

         // Find a matching route using http_router::match which handles both exact and parameterized routes
         auto [handle, params] = root_router.match(method, target);

         // Update the request with any extracted parameters
         request.params = std::move(params);

         if (!handle) {
            // No matching route found
            send_error_response(socket, 404, "Not Found");
            return;
         }

         // Create the response object
         response response;

         try {
            // Apply middleware
            for (const auto& middleware : root_router.middlewares) {
               middleware(request, response);
            }

            // Call the handle
            handle(request, response);

            // Send the response
            send_response(socket, response);
         }
         catch (const std::exception& e) {
            // Log the exception details for debugging
            std::string error_message = e.what();
            error_handler(std::make_error_code(std::errc::invalid_argument), std::source_location::current());

            // Send a 500 error response back to the client
            send_error_response(socket, 500, "Internal Server Error");
         }
      }

      inline void send_response(std::shared_ptr<asio::ip::tcp::socket> socket, const response& response)
      {
         // Format the response
         std::ostringstream oss;
         oss << "HTTP/1.1 " << response.status_code << " " << get_status_message(response.status_code) << "\r\n";

         // Add headers
         for (const auto& [name, value] : response.response_headers) {
            oss << name << ": " << value << "\r\n";
         }

         // Add Content-Length header if not present
         if (response.response_headers.find("Content-Length") == response.response_headers.end()) {
            oss << "Content-Length: " << response.response_body.size() << "\r\n";
         }

         // Add Date header if not present
         if (response.response_headers.find("Date") == response.response_headers.end()) {
            oss << "Date: " << get_current_date() << "\r\n";
         }

         // Add Server header if not present
         if (response.response_headers.find("Server") == response.response_headers.end()) {
            oss << "Server: Glaze/1.0\r\n";
         }

         // End of headers
         oss << "\r\n";

         // Add body
         oss << response.response_body;

         // Get the string
         std::string response_str = oss.str();

         // Send the response asynchronously
         asio::async_write(*socket, asio::buffer(response_str),
                           [socket](std::error_code /*ec*/, std::size_t /*bytes_transferred*/) {
                              // The socket will be closed when it goes out of scope
                           });
      }

      inline void send_error_response(std::shared_ptr<asio::ip::tcp::socket> socket, int status_code,
                                      const std::string& message)
      {
         response response;
         response.status(status_code).content_type("text/plain").body(message);

         send_response(socket, response);
      }

      inline std::string_view get_status_message(int status_code)
      {
         switch (status_code) {
         case 200:
            return "OK";
         case 201:
            return "Created";
         case 204:
            return "No Content";
         case 400:
            return "Bad Request";
         case 401:
            return "Unauthorized";
         case 403:
            return "Forbidden";
         case 404:
            return "Not Found";
         case 405:
            return "Method Not Allowed";
         case 409:
            return "Conflict";
         case 418:
            return "I'm a teapot";
         case 500:
            return "Internal Server Error";
         case 501:
            return "Not Implemented";
         case 503:
            return "Service Unavailable";
         default:
            return "Unknown";
         }
      }

      inline std::string get_current_date()
      {
         auto now = std::chrono::system_clock::now();
         auto time_t_now = std::chrono::system_clock::to_time_t(now);

         char buf[100];
         std::strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", std::gmtime(&time_t_now));

         return buf;
      }
   };

   // Alias for HTTPS server
   using https_server = http_server<true>;
}
