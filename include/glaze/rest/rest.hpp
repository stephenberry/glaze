// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <atomic>
#include <chrono>
#include <expected>
#include <format>
#include <functional>
#include <future>
#include <glaze/glaze.hpp>
#include <regex>
#include <source_location>
#include <thread>
#include <unordered_map>

#include "glaze/rest/http.hpp"

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

// TODO: Get rid of iostream dependency
#include <iostream>

namespace glz
{
   // Forward declarations
   struct Request;
   struct Response;
   struct http_router;
   struct http_server;
   struct http_client;

   using handler = std::function<void(const Request&, Response&)>;
   using async_handler = std::function<std::future<void>(const Request&, Response&)>;
   using error_handler = std::function<void(std::error_code, std::source_location)>;

   enum struct http_method { GET, POST, PUT, DELETE, PATCH, HEAD, OPTIONS };

   // Utility functions for HTTP methods
   inline std::string_view to_string(http_method method)
   {
      using enum http_method;
      switch (method) {
      case GET:
         return "GET";
      case POST:
         return "POST";
      case PUT:
         return "PUT";
      case DELETE:
         return "DELETE";
      case PATCH:
         return "PATCH";
      case HEAD:
         return "HEAD";
      case OPTIONS:
         return "OPTIONS";
      default:
         return "UNKNOWN";
      }
   }

   inline std::optional<http_method> from_string(std::string_view method)
   {
      using enum http_method;
      if (method == "GET") return GET;
      if (method == "POST") return POST;
      if (method == "PUT") return PUT;
      if (method == "DELETE") return DELETE;
      if (method == "PATCH") return PATCH;
      if (method == "HEAD") return HEAD;
      if (method == "OPTIONS") return OPTIONS;
      return std::nullopt;
   }

   // Request context object
   struct Request
   {
      http_method method{};
      std::string target{};
      std::unordered_map<std::string, std::string> params{};
      std::unordered_map<std::string, std::string> headers{};
      std::string body{};
      asio::ip::tcp::endpoint remote_endpoint{};
   };

   // Response builder
   struct Response
   {
      int status_code = 200;
      std::unordered_map<std::string, std::string> response_headers{};
      std::string response_body{};

      inline Response& status(int code)
      {
         status_code = code;
         return *this;
      }

      inline Response& header(std::string_view name, std::string_view value)
      {
         response_headers[std::string(name)] = std::string(value);
         return *this;
      }

      inline Response& body(std::string_view content)
      {
         response_body = std::string(content);
         return *this;
      }

      inline Response& content_type(std::string_view type) { return header("Content-Type", type); }

      // JSON response helper using Glaze
      template <class T>
      Response& json(T&& value)
      {
         content_type("application/json");
         auto ec = glz::write_json(std::forward<T>(value), response_body);
         if (ec) {
            response_body = R"({"error":"glz::write_json error"})"; // rare that this would ever happen
         }
         return *this;
      }
   };

   // Router for handling different paths
   struct http_router
   {
      // Defines parameter constraints for validation
      struct param_constraint
      {
         std::string regex_pattern{}; // Custom regex for this parameter
         std::string description{}; // For error reporting
      };

      struct route_pattern
      {
         std::regex regex;
         std::vector<std::string> param_names;
         std::unordered_map<std::string, param_constraint> constraints;
         http_method method;
         handler handle;
      };

      // Default constructor
      http_router() = default;

      // Add a route with a specific method
      inline http_router& route(http_method method, std::string_view path, handler handle,
                           const std::unordered_map<std::string, param_constraint>& constraints = {})
      {
         try {
            // Check if path contains parameters (e.g., ":id")
            if (path.find(':') != std::string_view::npos) {
               // This is a parameterized route - parse it
               auto pattern = parse_route_pattern(path, method, handle, constraints);
               route_patterns.push_back(std::move(pattern));
            }
            else {
               // This is a simple route - store it directly
               routes[std::string(path)][method] = std::move(handle);
            }
         }
         catch (const std::exception& e) {
            // Log the error instead of propagating it
            std::cerr << "Error adding route '" << path << "': " << e.what() << std::endl;
         }
         return *this;
      }

      // Standard HTTP method helpers
      inline http_router& get(std::string_view path, handler handle,
                         const std::unordered_map<std::string, param_constraint>& constraints = {})
      {
         return route(http_method::GET, path, std::move(handle), constraints);
      }

      inline http_router& post(std::string_view path, handler handle,
                          const std::unordered_map<std::string, param_constraint>& constraints = {})
      {
         return route(http_method::POST, path, std::move(handle), constraints);
      }

      inline http_router& put(std::string_view path, handler handle,
                         const std::unordered_map<std::string, param_constraint>& constraints = {})
      {
         return route(http_method::PUT, path, std::move(handle), constraints);
      }

      inline http_router& del(std::string_view path, handler handle,
                         const std::unordered_map<std::string, param_constraint>& constraints = {})
      {
         return route(http_method::DELETE, path, std::move(handle), constraints);
      }

      inline http_router& patch(std::string_view path, handler handle,
                           const std::unordered_map<std::string, param_constraint>& constraints = {})
      {
         return route(http_method::PATCH, path, std::move(handle), constraints);
      }

      // Async versions
      inline http_router& route_async(http_method method, std::string_view path, async_handler handle,
                                 const std::unordered_map<std::string, param_constraint>& constraints = {})
      {
         // Convert async handle to sync handle
         return route(
            method, path,
            [handle](const Request& req, Response& res) {
               // Create a future and wait for it
               auto future = handle(req, res);
               future.wait();
            },
            constraints);
      }

      inline http_router& get_async(std::string_view path, async_handler handle,
                               const std::unordered_map<std::string, param_constraint>& constraints = {})
      {
         return route_async(http_method::GET, path, std::move(handle), constraints);
      }

      inline http_router& post_async(std::string_view path, async_handler handle,
                                const std::unordered_map<std::string, param_constraint>& constraints = {})
      {
         return route_async(http_method::POST, path, std::move(handle), constraints);
      }

      // Middleware support
      inline http_router& use(handler middleware)
      {
         middlewares.push_back(std::move(middleware));
         return *this;
      }

      // Methods to check if a route matches a request
      inline std::pair<handler, std::unordered_map<std::string, std::string>> match(http_method method,
                                                                                    const std::string& target) const
      {
         handler handle = nullptr;
         std::unordered_map<std::string, std::string> params;

         // 1. Try to find an exact match first (for efficiency)
         auto route_it = routes.find(target);
         if (route_it != routes.end()) {
            // Found a matching path, check method
            auto method_it = route_it->second.find(method);
            if (method_it != route_it->second.end()) {
               handle = method_it->second;
               return {handle, params}; // No params for exact match
            }
         }

         // 2. If no exact match, try pattern matching
         for (const auto& pattern : route_patterns) {
            // Skip patterns for different methods
            if (pattern.method != method) {
               continue;
            }

            std::smatch matches;
            if (std::regex_match(target, matches, pattern.regex)) {
               // We have a match!
               handle = pattern.handle;

               // Extract parameters
               bool all_constraints_passed = true;
               for (size_t i = 0; i < pattern.param_names.size() && i + 1 < matches.size(); ++i) {
                  std::string param_name = pattern.param_names[i];
                  std::string param_value = matches[i + 1].str();

                  // Store parameter
                  params[param_name] = param_value;
               }

               if (all_constraints_passed) {
                  // All constraints passed, use this handle
                  break;
               }
               else {
                  // Reset handle and params if constraints failed
                  handle = nullptr;
                  params.clear();
               }
            }
         }

         return {handle, params};
      }

      // Data members
      std::unordered_map<std::string, std::unordered_map<http_method, handler>> routes;
      std::vector<route_pattern> route_patterns;
      std::vector<handler> middlewares;

     private:
      // Parse a route pattern like "/api/users/:id" into a regex and parameter names
      route_pattern parse_route_pattern(std::string_view path, http_method method, handler handle,
                                        const std::unordered_map<std::string, param_constraint>& constraints = {})
      {
         std::string pattern_str;
         std::vector<std::string> param_names;
         std::unordered_map<std::string, param_constraint> applied_constraints;

         // Convert "/api/users/:id" to "^/api/users/([^/]+)$" and extract param names
         std::string path_str(path);
         std::string remaining = path_str;
         size_t pos = 0;

         while (pos < remaining.size()) {
            size_t param_start = remaining.find(':', pos);
            if (param_start == std::string::npos) {
               // No more parameters, add the rest of the path
               pattern_str += escape_regex_except_groups(remaining.substr(pos));
               break;
            }

            // Add everything up to the parameter
            pattern_str += escape_regex_except_groups(remaining.substr(pos, param_start - pos));

            // Find the end of the parameter name
            size_t param_end = remaining.find_first_of("/?#", param_start);
            if (param_end == std::string::npos) {
               param_end = remaining.size();
            }

            // Extract parameter name (without the ':')
            std::string param_name = remaining.substr(param_start + 1, param_end - param_start - 1);
            param_names.push_back(param_name);

            // Check if this parameter has a constraint
            if (constraints.find(param_name) != constraints.end()) {
               // Use the custom regex pattern
               pattern_str += "(" + constraints.at(param_name).regex_pattern + ")";
               applied_constraints[param_name] = constraints.at(param_name);
            }
            else {
               // Default: match anything except slashes
               pattern_str += "([^/]+)";
            }

            // Move past this parameter
            pos = param_end;
         }

         // Add start and end anchors for exact matching
         pattern_str = "^" + pattern_str + "$";

         try {
            // Create the regex - may throw if pattern is invalid
            std::regex regex(pattern_str);
            return {regex, param_names, applied_constraints, method, handle};
         }
         catch (const std::regex_error& e) {
            throw std::runtime_error("Invalid route pattern: " + std::string(path) +
                                     " (generated regex: " + pattern_str + ") - " + e.what());
         }
      }

      // Helper to escape regex special characters, but preserve our capture groups
      std::string escape_regex_except_groups(const std::string& input)
      {
         std::string result;
         for (char c : input) {
            if (c == '.' || c == '^' || c == '$' || c == '*' || c == '+' || c == '?' || c == '[' || c == ']' ||
                c == '{' || c == '}' || c == '|' || c == '\\') {
               result += '\\';
            }
            result += c;
         }
         return result;
      }
   };

   // Server implementation using non-blocking asio
   struct http_server
   {
      inline http_server() : io_context(std::make_unique<asio::io_context>())
      {
         error_handler = [](std::error_code ec, std::source_location loc) {
            std::cerr << std::format("Error at {}:{}: {}\n", loc.file_name(), loc.line(), ec.message());
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

      // Path pattern matching
      struct route_pattern
      {
         std::regex regex;
         std::vector<std::string> param_names;
      };

      std::unordered_map<std::string, route_pattern> route_patterns;

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

               // Parse method, target, and HTTP version
               std::regex request_regex(R"((\w+) ([^ ]+) HTTP/(\d+\.\d+))");
               std::smatch request_match;

               if (!std::regex_match(request_line, request_match, request_regex)) {
                  // Malformed request
                  send_error_response(socket_ptr, 400, "Bad Request");
                  return;
               }

               std::string method_str = request_match[1];
               std::string target = request_match[2];

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
                  content_length = std::stoul(content_length_it->second);
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
                     asio::async_read(*socket_ptr, *buffer,
                                      asio::transfer_exactly(content_length - to_read),
                                      [this, socket_ptr, buffer, method_opt, target, headers, remote_endpoint,
                                       content_length, body = std::move(body), to_read]
                                      (std::error_code ec, std::size_t /*bytes_transferred*/) mutable {
                        if (ec) {
                           error_handler(ec, std::source_location::current());
                           return;
                        }
                        
                        // Read the remaining data
                        std::istream request_stream(buffer.get());
                        body.resize(content_length);
                        request_stream.read(&body[to_read], content_length - to_read);
                        
                        process_full_request(socket_ptr, *method_opt, target, headers, std::move(body), remote_endpoint);
                     });
                  } else {
                     // We already have the full body
                     process_full_request(socket_ptr, *method_opt, target, headers, std::move(body), remote_endpoint);
                  }
               } else {
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
         Request request;
         request.method = method;
         request.target = target;
         request.headers = headers;
         request.body = std::move(body);
         request.remote_endpoint = remote_endpoint;

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
         Response response;

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

      inline void send_response(std::shared_ptr<asio::ip::tcp::socket> socket, const Response& response)
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
         Response response;
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

      inline std::expected<Response, std::error_code> get(
         std::string_view url, const std::unordered_map<std::string, std::string>& headers = {})
      {
         // Parse the URL
         std::regex url_regex(R"(^(http|https)://([^:/]+)(?::(\d+))?(/.*)?$)");
         std::cmatch matches;

         if (!std::regex_match(url.begin(), url.end(), matches, url_regex)) {
            return std::unexpected(std::make_error_code(std::errc::invalid_argument));
         }

         std::string protocol = matches[1];
         std::string host = matches[2];
         std::string port_str = matches[3].matched ? matches[3].str() : "";
         std::string path = matches[4].matched ? matches[4].str() : "/";

         uint16_t port = 0;
         if (port_str.empty()) {
            port = (protocol == "https") ? 443 : 80;
         }
         else {
            port = static_cast<uint16_t>(std::stoi(port_str));
         }

         // Create a promise for the response
         std::promise<std::expected<Response, std::error_code>> promise;
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

                                 // Parse HTTP version, status code, and status message
                                 std::regex status_regex(R"(HTTP/(\d+\.\d+) (\d+) (.*))");
                                 std::smatch status_match;

                                 if (!std::regex_match(status_line, status_match, status_regex)) {
                                    promise.set_value(std::unexpected(std::make_error_code(std::errc::protocol_error)));
                                    return;
                                 }

                                 int status_code = std::stoi(status_match[2]);

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
                                       Response response;
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

      inline std::expected<Response, std::error_code> post(
         std::string_view url, std::string_view body, const std::unordered_map<std::string, std::string>& headers = {})
      {
         // Implementation similar to get, but with POST method and body
         // Parse the URL
         std::regex url_regex(R"(^(http|https)://([^:/]+)(?::(\d+))?(/.*)?$)");
         std::cmatch matches;

         if (!std::regex_match(url.begin(), url.end(), matches, url_regex)) {
            return std::unexpected(std::make_error_code(std::errc::invalid_argument));
         }

         std::string protocol = matches[1];
         std::string host = matches[2];
         std::string port_str = matches[3].matched ? matches[3].str() : "";
         std::string path = matches[4].matched ? matches[4].str() : "/";

         uint16_t port = 0;
         if (port_str.empty()) {
            port = (protocol == "https") ? 443 : 80;
         }
         else {
            port = static_cast<uint16_t>(std::stoi(port_str));
         }

         // Create a promise for the response
         std::promise<std::expected<Response, std::error_code>> promise;
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

                           // ... Rest of code similar to get() implementation ...
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
                                       Response response;
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

      inline std::future<std::expected<Response, std::error_code>> get_async(
         std::string_view url, const std::unordered_map<std::string, std::string>& headers = {})
      {
         // Create a promise and future
         auto promise = std::make_shared<std::promise<std::expected<Response, std::error_code>>>();
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

      inline std::future<std::expected<Response, std::error_code>> post_async(
         std::string_view url, std::string_view body, const std::unordered_map<std::string, std::string>& headers = {})
      {
         // Create a promise and future
         auto promise = std::make_shared<std::promise<std::expected<Response, std::error_code>>>();
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
      inline std::expected<Response, std::error_code> post_json(
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
