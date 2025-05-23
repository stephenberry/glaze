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
#include <source_location>
#include <thread>
#include <unordered_map>

#include "glaze/rest/http_router.hpp"

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
   // Forward declarations
   struct http_router;
   struct http_server;
   struct http_client;

   using handler = std::function<void(const request&, response&)>;
   using async_handler = std::function<std::future<void>(const request&, response&)>;
   using error_handler = std::function<void(std::error_code, std::source_location)>;

   // Server implementation using non-blocking asio
   struct http_server
   {
      inline http_server() : io_context(std::make_unique<asio::io_context>())
      {
         error_handler = [](std::error_code ec, std::source_location loc) {
            std::fprintf(stderr, "Error at %s:%d: %s\n", loc.file_name(), static_cast<int>(loc.line()),
                         ec.message().c_str());
         };
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

     private:
      std::unique_ptr<asio::io_context> io_context;
      std::unique_ptr<asio::ip::tcp::acceptor> acceptor;
      std::vector<std::thread> threads;
      http_router root_router;
      bool running = false;
      glz::error_handler error_handler;

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
               // Must start with "HTTP/"
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

                     headers[name] = value;
                  }
               }

               // Check if there's a Content-Length header
               std::size_t content_length = 0;
               auto content_length_it = headers.find("Content-Length");
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
            oss << "Server: ResonanceHTTP/0.1\r\n";
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

   // Client implementation using non-blocking asio
   struct http_client
   {
      inline http_client() : io_context(std::make_unique<asio::io_context>())
      {
         // Start the IO thread
         io_thread = std::thread([this] { io_context->run(); });
      }

      inline ~http_client()
      {
         // Stop the IO context
         io_context->stop();

         // Join the IO thread
         if (io_thread.joinable()) {
            io_thread.join();
         }
      }

      inline std::expected<response, std::error_code> get(
         std::string_view url, const std::unordered_map<std::string, std::string>& headers = {})
      {
         // Manual URL parsing, same as regex pattern: ^(http|https)://([^:/]+)(?::(\d+))?(/.*)?$

         // Check for protocol
         if (url.size() < 8) { // Minimum for "http://" + 1 char host
            return std::unexpected(std::make_error_code(std::errc::invalid_argument));
         }

         // Check protocol prefix
         std::string_view protocolView;
         size_t protocolEnd = url.find("://");
         if (protocolEnd == std::string_view::npos) {
            return std::unexpected(std::make_error_code(std::errc::invalid_argument));
         }

         protocolView = url.substr(0, protocolEnd);
         std::string protocol(protocolView);
         if (protocol != "http" && protocol != "https") {
            return std::unexpected(std::make_error_code(std::errc::invalid_argument));
         }

         // Process host, port and path
         size_t hostStart = protocolEnd + 3;
         if (hostStart >= url.size()) {
            return std::unexpected(std::make_error_code(std::errc::invalid_argument));
         }

         // Find the end of host (first '/' or ':' character)
         size_t hostEnd = url.find_first_of("/:", hostStart);
         std::string host;
         std::string port_str;
         std::string path = "/";

         if (hostEnd == std::string_view::npos) {
            // No port or path specified
            host = std::string(url.substr(hostStart));
         }
         else if (url[hostEnd] == ':') {
            // Port specified
            host = std::string(url.substr(hostStart, hostEnd - hostStart));

            size_t portStart = hostEnd + 1;
            size_t portEnd = url.find('/', portStart);

            if (portEnd == std::string_view::npos) {
               // No path after port
               port_str = std::string(url.substr(portStart));
            }
            else {
               // Path after port
               port_str = std::string(url.substr(portStart, portEnd - portStart));
               path = std::string(url.substr(portEnd));
            }

            // Validate port (must be all digits)
            if (!port_str.empty() && !std::all_of(port_str.begin(), port_str.end(), ::isdigit)) {
               return std::unexpected(std::make_error_code(std::errc::invalid_argument));
            }
         }
         else if (url[hostEnd] == '/') {
            // Path specified, no port
            host = std::string(url.substr(hostStart, hostEnd - hostStart));
            path = std::string(url.substr(hostEnd));
         }

         // Host validation - ensure it contains valid characters
         if (host.empty() || host.find_first_of(":/") != std::string::npos) {
            return std::unexpected(std::make_error_code(std::errc::invalid_argument));
         }

         // Determine port
         uint16_t port = 0;
         if (port_str.empty()) {
            port = (protocol == "https") ? 443 : 80;
         }
         else {
            try {
               long portLong = std::stol(port_str);
               if (portLong <= 0 || portLong > 65535) {
                  return std::unexpected(std::make_error_code(std::errc::invalid_argument));
               }
               port = static_cast<uint16_t>(portLong);
            }
            catch (const std::exception&) {
               return std::unexpected(std::make_error_code(std::errc::invalid_argument));
            }
         }

         // Create a promise for the response
         std::promise<std::expected<response, std::error_code>> promise;
         auto future = promise.get_future();

         // Create a resolver to translate the hostname into IP address
         auto resolver = std::make_shared<asio::ip::tcp::resolver>(*io_context);

         // Start the async resolution
         resolver->async_resolve(
            host, std::to_string(port),
            [this, resolver, host, path, headers, promise = std::move(promise)](
               std::error_code ec, asio::ip::tcp::resolver::results_type results) mutable {
               if (ec) {
                  promise.set_value(std::unexpected(ec));
                  return;
               }

               // Create a socket
               auto socket = std::make_shared<asio::ip::tcp::socket>(*io_context);

               // Connect to the server
               asio::async_connect(
                  *socket, results,
                  [socket, host, path, headers, promise = std::move(promise)](
                     std::error_code ec, const asio::ip::tcp::endpoint& /*endpoint*/) mutable {
                     if (ec) {
                        promise.set_value(std::unexpected(ec));
                        return;
                     }

                     // Format the request
                     std::ostringstream oss;
                     oss << "GET " << path << " HTTP/1.1\r\n";
                     oss << "Host: " << host << "\r\n";
                     oss << "Connection: close\r\n";

                     // Add headers
                     for (const auto& [name, value] : headers) {
                        oss << name << ": " << value << "\r\n";
                     }

                     // End of headers
                     oss << "\r\n";

                     std::string request_str = oss.str();

                     // Send the request
                     asio::async_write(
                        *socket, asio::buffer(request_str),
                        [socket, promise = std::move(promise)](std::error_code ec,
                                                               std::size_t /*bytes_transferred*/) mutable {
                           if (ec) {
                              promise.set_value(std::unexpected(ec));
                              return;
                           }

                           // Create a buffer for the response
                           auto buffer = std::make_shared<asio::streambuf>();

                           // Read the response headers
                           asio::async_read_until(
                              *socket, *buffer, "\r\n\r\n",
                              [socket, buffer, promise = std::move(promise)](
                                 std::error_code ec, std::size_t /*bytes_transferred*/) mutable {
                                 if (ec) {
                                    promise.set_value(std::unexpected(ec));
                                    return;
                                 }

                                 // Parse the response
                                 std::istream response_stream(buffer.get());
                                 std::string status_line;
                                 std::getline(response_stream, status_line);

                                 // Remove carriage return if present
                                 if (!status_line.empty() && status_line.back() == '\r') {
                                    status_line.pop_back();
                                 }

                                 // Parse HTTP status line with similar robustness to regex: HTTP/(\d+\.\d+) (\d+) (.*)
                                 if (status_line.size() < 12) { // HTTP/1.1 200 OK (min length)
                                    promise.set_value(std::unexpected(std::make_error_code(std::errc::protocol_error)));
                                    return;
                                 }

                                 // Check HTTP prefix
                                 if (status_line.substr(0, 5) != "HTTP/") {
                                    promise.set_value(std::unexpected(std::make_error_code(std::errc::protocol_error)));
                                    return;
                                 }

                                 // Find version separator
                                 size_t versionEnd = status_line.find(' ', 5);
                                 if (versionEnd == std::string::npos) {
                                    promise.set_value(std::unexpected(std::make_error_code(std::errc::protocol_error)));
                                    return;
                                 }

                                 // Extract and validate version (should be like 1.1)
                                 std::string version = status_line.substr(5, versionEnd - 5);
                                 if (version.empty() || !std::isdigit(version[0])) {
                                    promise.set_value(std::unexpected(std::make_error_code(std::errc::protocol_error)));
                                    return;
                                 }

                                 // Check for decimal point in version
                                 size_t decimalPoint = version.find('.');
                                 if (decimalPoint == std::string::npos || decimalPoint == 0 ||
                                     decimalPoint == version.length() - 1 || !std::isdigit(version[decimalPoint + 1])) {
                                    promise.set_value(std::unexpected(std::make_error_code(std::errc::protocol_error)));
                                    return;
                                 }

                                 // Validate status code
                                 size_t statusCodeStart = versionEnd + 1;
                                 size_t statusCodeEnd = status_line.find(' ', statusCodeStart);
                                 if (statusCodeEnd == std::string::npos) {
                                    promise.set_value(std::unexpected(std::make_error_code(std::errc::protocol_error)));
                                    return;
                                 }

                                 std::string statusCodeStr =
                                    status_line.substr(statusCodeStart, statusCodeEnd - statusCodeStart);
                                 if (statusCodeStr.empty() ||
                                     !std::all_of(statusCodeStr.begin(), statusCodeStr.end(), ::isdigit)) {
                                    promise.set_value(std::unexpected(std::make_error_code(std::errc::protocol_error)));
                                    return;
                                 }

                                 int status_code;
                                 try {
                                    status_code = std::stoi(statusCodeStr);
                                    if (status_code < 100 || status_code > 599) { // Valid HTTP status codes
                                       promise.set_value(
                                          std::unexpected(std::make_error_code(std::errc::protocol_error)));
                                       return;
                                    }
                                 }
                                 catch (const std::exception&) {
                                    promise.set_value(std::unexpected(std::make_error_code(std::errc::protocol_error)));
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

                                       // Trim leading whitespace from value
                                       value.erase(0, value.find_first_not_of(" \t"));

                                       response_headers[name] = value;
                                    }
                                 }

                                 // Read the rest of the response
                                 asio::async_read(
                                    *socket, *buffer, asio::transfer_all(),
                                    [socket, buffer, status_code, response_headers, promise = std::move(promise)](
                                       std::error_code ec, std::size_t /*bytes_transferred*/) mutable {
                                       // Ignore EOF, which is expected
                                       if (ec && ec != asio::error::eof) {
                                          promise.set_value(std::unexpected(ec));
                                          return;
                                       }

                                       // Extract the body
                                       std::string body{std::istreambuf_iterator<char>(buffer.get()),
                                                        std::istreambuf_iterator<char>()};

                                       // Create response object
                                       response response;
                                       response.status_code = status_code;
                                       response.response_headers = response_headers;
                                       response.response_body = std::move(body);

                                       // Set the result
                                       promise.set_value(std::move(response));
                                    });
                              });
                        });
                  });
            });

         // Wait for the result
         return future.get();
      }

      inline std::expected<response, std::error_code> post(
         std::string_view url, std::string_view body, const std::unordered_map<std::string, std::string>& headers = {})
      {
         // Manual URL parsing, same as regex pattern: ^(http|https)://([^:/]+)(?::(\d+))?(/.*)?$

         // Check for protocol
         if (url.size() < 8) { // Minimum for "http://" + 1 char host
            return std::unexpected(std::make_error_code(std::errc::invalid_argument));
         }

         // Check protocol prefix
         std::string_view protocolView;
         size_t protocolEnd = url.find("://");
         if (protocolEnd == std::string_view::npos) {
            return std::unexpected(std::make_error_code(std::errc::invalid_argument));
         }

         protocolView = url.substr(0, protocolEnd);
         std::string protocol(protocolView);
         if (protocol != "http" && protocol != "https") {
            return std::unexpected(std::make_error_code(std::errc::invalid_argument));
         }

         // Process host, port and path
         size_t hostStart = protocolEnd + 3;
         if (hostStart >= url.size()) {
            return std::unexpected(std::make_error_code(std::errc::invalid_argument));
         }

         // Find the end of host (first '/' or ':' character)
         size_t hostEnd = url.find_first_of("/:", hostStart);
         std::string host;
         std::string port_str;
         std::string path = "/";

         if (hostEnd == std::string_view::npos) {
            // No port or path specified
            host = std::string(url.substr(hostStart));
         }
         else if (url[hostEnd] == ':') {
            // Port specified
            host = std::string(url.substr(hostStart, hostEnd - hostStart));

            size_t portStart = hostEnd + 1;
            size_t portEnd = url.find('/', portStart);

            if (portEnd == std::string_view::npos) {
               // No path after port
               port_str = std::string(url.substr(portStart));
            }
            else {
               // Path after port
               port_str = std::string(url.substr(portStart, portEnd - portStart));
               path = std::string(url.substr(portEnd));
            }

            // Validate port (must be all digits)
            if (!port_str.empty() && !std::all_of(port_str.begin(), port_str.end(), ::isdigit)) {
               return std::unexpected(std::make_error_code(std::errc::invalid_argument));
            }
         }
         else if (url[hostEnd] == '/') {
            // Path specified, no port
            host = std::string(url.substr(hostStart, hostEnd - hostStart));
            path = std::string(url.substr(hostEnd));
         }

         // Host validation - ensure it contains valid characters
         if (host.empty() || host.find_first_of(":/") != std::string::npos) {
            return std::unexpected(std::make_error_code(std::errc::invalid_argument));
         }

         // Determine port
         uint16_t port = 0;
         if (port_str.empty()) {
            port = (protocol == "https") ? 443 : 80;
         }
         else {
            try {
               long portLong = std::stol(port_str);
               if (portLong <= 0 || portLong > 65535) {
                  return std::unexpected(std::make_error_code(std::errc::invalid_argument));
               }
               port = static_cast<uint16_t>(portLong);
            }
            catch (const std::exception&) {
               return std::unexpected(std::make_error_code(std::errc::invalid_argument));
            }
         }

         // Create a promise for the response
         std::promise<std::expected<response, std::error_code>> promise;
         auto future = promise.get_future();

         // Create a resolver to translate the hostname into IP address
         auto resolver = std::make_shared<asio::ip::tcp::resolver>(*io_context);

         // Start the async resolution
         resolver->async_resolve(
            host, std::to_string(port),
            [this, resolver, host, path, body = std::string(body), headers, promise = std::move(promise)](
               std::error_code ec, asio::ip::tcp::resolver::results_type results) mutable {
               if (ec) {
                  promise.set_value(std::unexpected(ec));
                  return;
               }

               // Create a socket
               auto socket = std::make_shared<asio::ip::tcp::socket>(*io_context);

               // Connect to the server
               asio::async_connect(
                  *socket, results,
                  [socket, host, path, body, headers, promise = std::move(promise)](
                     std::error_code ec, const asio::ip::tcp::endpoint& /*endpoint*/) mutable {
                     if (ec) {
                        promise.set_value(std::unexpected(ec));
                        return;
                     }

                     // Format the request
                     std::ostringstream oss;
                     oss << "POST " << path << " HTTP/1.1\r\n";
                     oss << "Host: " << host << "\r\n";
                     oss << "Connection: close\r\n";
                     oss << "Content-Length: " << body.size() << "\r\n";

                     // Add headers
                     for (const auto& [name, value] : headers) {
                        oss << name << ": " << value << "\r\n";
                     }

                     // End of headers
                     oss << "\r\n";

                     // Add body
                     oss << body;

                     std::string request_str = oss.str();

                     // Send the request
                     asio::async_write(
                        *socket, asio::buffer(request_str),
                        [socket, promise = std::move(promise)](std::error_code ec,
                                                               std::size_t /*bytes_transferred*/) mutable {
                           if (ec) {
                              promise.set_value(std::unexpected(ec));
                              return;
                           }

                           // Create a buffer for the response
                           auto buffer = std::make_shared<asio::streambuf>();

                           // Read the response headers
                           asio::async_read_until(
                              *socket, *buffer, "\r\n\r\n",
                              [socket, buffer, promise = std::move(promise)](
                                 std::error_code ec, std::size_t /*bytes_transferred*/) mutable {
                                 if (ec) {
                                    promise.set_value(std::unexpected(ec));
                                    return;
                                 }

                                 // Parse the response
                                 std::istream response_stream(buffer.get());
                                 std::string status_line;
                                 std::getline(response_stream, status_line);

                                 // Remove carriage return if present
                                 if (!status_line.empty() && status_line.back() == '\r') {
                                    status_line.pop_back();
                                 }

                                 // Parse HTTP version, status code, and status message
                                 auto http_status_line = parse_http_status_line(status_line);
                                 if (not http_status_line) {
                                    promise.set_value(std::unexpected(http_status_line.error()));
                                    return;
                                 }

                                 int status_code = http_status_line->status_code;

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

                                       // Trim leading whitespace from value
                                       value.erase(0, value.find_first_not_of(" \t"));

                                       response_headers[name] = value;
                                    }
                                 }

                                 // Read the rest of the response
                                 asio::async_read(
                                    *socket, *buffer, asio::transfer_all(),
                                    [socket, buffer, status_code, response_headers, promise = std::move(promise)](
                                       std::error_code ec, std::size_t /*bytes_transferred*/) mutable {
                                       // Ignore EOF, which is expected
                                       if (ec && ec != asio::error::eof) {
                                          promise.set_value(std::unexpected(ec));
                                          return;
                                       }

                                       // Extract the body
                                       std::string body{std::istreambuf_iterator<char>(buffer.get()),
                                                        std::istreambuf_iterator<char>()};

                                       // Create response object
                                       response response;
                                       response.status_code = status_code;
                                       response.response_headers = response_headers;
                                       response.response_body = std::move(body);

                                       // Set the result
                                       promise.set_value(std::move(response));
                                    });
                              });
                        });
                  });
            });

         // Wait for the result
         return future.get();
      }

      inline std::future<std::expected<response, std::error_code>> get_async(
         std::string_view url, const std::unordered_map<std::string, std::string>& headers = {})
      {
         // Create a promise and future
         auto promise = std::make_shared<std::promise<std::expected<response, std::error_code>>>();
         auto future = promise->get_future();

         // Run the operation asynchronously
         asio::post(*io_context, [this, url = std::string(url), headers, promise]() {
            try {
               auto result = get(url, headers);
               promise->set_value(std::move(result));
            }
            catch (...) {
               promise->set_exception(std::current_exception());
            }
         });

         return future;
      }

      inline std::future<std::expected<response, std::error_code>> post_async(
         std::string_view url, std::string_view body, const std::unordered_map<std::string, std::string>& headers = {})
      {
         // Create a promise and future
         auto promise = std::make_shared<std::promise<std::expected<response, std::error_code>>>();
         auto future = promise->get_future();

         // Run the operation asynchronously
         asio::post(*io_context, [this, url = std::string(url), body = std::string(body), headers, promise]() {
            try {
               auto result = post(url, body, headers);
               promise->set_value(std::move(result));
            }
            catch (...) {
               promise->set_exception(std::current_exception());
            }
         });

         return future;
      }

      template <class T>
      inline std::expected<response, std::error_code> post_json(
         std::string_view url, const T& data, const std::unordered_map<std::string, std::string>& headers = {})
      {
         std::string json_str;
         glz::write_json(data, json_str);

         auto merged_headers = headers;
         merged_headers["Content-Type"] = "application/json";

         return post(url, json_str, merged_headers);
      }

     private:
      std::unique_ptr<asio::io_context> io_context;
      std::thread io_thread;
   };
}
