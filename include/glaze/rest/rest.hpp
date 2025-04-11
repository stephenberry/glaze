// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <functional>
#include <unordered_map>
#include <future>
#include <expected>
#include <source_location>
#include <format>
#include <thread>
#include <atomic>
#include <regex>
#include <chrono>
#include <glaze/glaze.hpp>

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
   class Response;
   class Router;
   class Server;
   class Client;
   
   using Handler = std::function<void(const Request&, Response&)>;
   using AsyncHandler = std::function<std::future<void>(const Request&, Response&)>;
   using ErrorHandler = std::function<void(std::error_code, std::source_location)>;
   
   // HTTP Methods
   enum class Method {
      GET, POST, PUT, DELETE, PATCH, HEAD, OPTIONS
   };
   
   // Utility functions for HTTP methods
   inline std::string_view to_string(Method method) {
      switch (method) {
         case Method::GET: return "GET";
         case Method::POST: return "POST";
         case Method::PUT: return "PUT";
         case Method::DELETE: return "DELETE";
         case Method::PATCH: return "PATCH";
         case Method::HEAD: return "HEAD";
         case Method::OPTIONS: return "OPTIONS";
         default: return "UNKNOWN";
      }
   }
   
   inline std::optional<Method> from_string(std::string_view method) {
      if (method == "GET") return Method::GET;
      if (method == "POST") return Method::POST;
      if (method == "PUT") return Method::PUT;
      if (method == "DELETE") return Method::DELETE;
      if (method == "PATCH") return Method::PATCH;
      if (method == "HEAD") return Method::HEAD;
      if (method == "OPTIONS") return Method::OPTIONS;
      return std::nullopt;
   }
   
   // Request context object
   struct Request {
      Method method;
      std::string target;
      std::unordered_map<std::string, std::string> params;
      std::unordered_map<std::string, std::string> headers;
      std::string body;
      asio::ip::tcp::endpoint remote_endpoint;
      
      // JSON parsing helpers using Glaze
      template<typename T>
      std::expected<T, std::string> parse_json() const {
         T result;
         auto error = glz::read_json(result, body);
         if (error) {
            return std::unexpected(glz::format_error(error, body));
         }
         return result;
      }
   };
   
   // Response builder
   class Response {
   public:
      inline Response& status(int code) {
         status_code = code;
         return *this;
      }
      
      inline Response& header(std::string_view name, std::string_view value) {
         response_headers[std::string(name)] = std::string(value);
         return *this;
      }
      
      inline Response& body(std::string_view content) {
         response_body = std::string(content);
         return *this;
      }
      
      inline Response& content_type(std::string_view type) {
         return header("Content-Type", type);
      }
      
      // JSON response helper using Glaze
      template <typename T>
      Response& json(const T& value) {
         std::string json_str;
         glz::write_json(value, json_str);
         return content_type("application/json").body(json_str);
      }
      
      int status_code = 200;
      std::unordered_map<std::string, std::string> response_headers;
      std::string response_body;
   };
   
   // Router for handling different paths
   class Router {
   public:
      inline Router& route(Method method, std::string_view path, Handler handler) {
         routes[std::string(path)][method] = std::move(handler);
         return *this;
      }
      
      inline Router& get(std::string_view path, Handler handler) {
         return route(Method::GET, path, std::move(handler));
      }
      
      inline Router& post(std::string_view path, Handler handler) {
         return route(Method::POST, path, std::move(handler));
      }
      
      inline Router& put(std::string_view path, Handler handler) {
         return route(Method::PUT, path, std::move(handler));
      }
      
      inline Router& del(std::string_view path, Handler handler) {
         return route(Method::DELETE, path, std::move(handler));
      }
      
      inline Router& patch(std::string_view path, Handler handler) {
         return route(Method::PATCH, path, std::move(handler));
      }
      
      // Async versions
      inline Router& route_async(Method method, std::string_view path, AsyncHandler handler) {
         // Convert async handler to sync handler
         return route(method, path, [handler](const Request& req, Response& res) {
            // Create a future and wait for it
            auto future = handler(req, res);
            future.wait();
         });
      }
      
      inline Router& get_async(std::string_view path, AsyncHandler handler) {
         return route_async(Method::GET, path, std::move(handler));
      }
      
      inline Router& post_async(std::string_view path, AsyncHandler handler) {
         return route_async(Method::POST, path, std::move(handler));
      }
      
      inline Router& use(Handler middleware) {
         middlewares.push_back(std::move(middleware));
         return *this;
      }
      
      std::unordered_map<std::string, std::unordered_map<Method, Handler>> routes;
      std::vector<Handler> middlewares;
   };
   
   // Server implementation using non-blocking asio
   class Server {
   public:
      inline Server() : io_context(std::make_unique<asio::io_context>()) {
         error_handler = [](std::error_code ec, std::source_location loc) {
            std::cerr << std::format("Error at {}:{}: {}\n",
                                     loc.file_name(), loc.line(), ec.message());
         };
      }
      
      inline ~Server() {
         if (running) {
            stop();
         }
      }
      
      inline Server& bind(std::string_view address, uint16_t port) {
         try {
            asio::ip::tcp::endpoint endpoint(
                                             asio::ip::make_address(address), port);
            
            acceptor = std::make_unique<asio::ip::tcp::acceptor>(
                                                                 *io_context, endpoint);
         } catch (const std::exception& e) {
            error_handler(
                          std::make_error_code(std::errc::address_in_use),
                          std::source_location::current());
         }
         return *this;
      }
      
      inline Server& bind(uint16_t port) {
         return bind("0.0.0.0", port);
      }
      
      inline void start(size_t num_threads = 0) {
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
               } catch (const std::exception& e) {
                  error_handler(
                                std::make_error_code(std::errc::operation_canceled),
                                std::source_location::current());
               }
            });
         }
      }
      
      inline void stop() {
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
      
      inline Server& mount(std::string_view base_path, const Router& router) {
         // Mount all routes from the router at the specified base path
         for (const auto& [path, method_handlers] : router.routes) {
            std::string full_path = std::string(base_path);
            if (!full_path.empty() && full_path.back() == '/') {
               full_path.pop_back();
            }
            full_path += path;
            
            for (const auto& [method, handler] : method_handlers) {
               root_router.route(method, full_path, handler);
            }
         }
         
         // Add middleware
         for (const auto& middleware : router.middlewares) {
            root_router.use(middleware);
         }
         
         return *this;
      }
      
      inline Router& route(Method method, std::string_view path, Handler handler) {
         return root_router.route(method, path, handler);
      }
      
      inline Router& get(std::string_view path, Handler handler) {
         return root_router.get(path, handler);
      }
      
      inline Router& post(std::string_view path, Handler handler) {
         return root_router.post(path, handler);
      }
      
      inline Server& on_error(ErrorHandler handler) {
         error_handler = std::move(handler);
         return *this;
      }
      
   private:
      std::unique_ptr<asio::io_context> io_context;
      std::unique_ptr<asio::ip::tcp::acceptor> acceptor;
      std::vector<std::thread> threads;
      Router root_router;
      bool running = false;
      ErrorHandler error_handler;
      
      // Path pattern matching
      struct RoutePattern {
         std::regex regex;
         std::vector<std::string> param_names;
      };
      
      std::unordered_map<std::string, RoutePattern> route_patterns;
      
      inline void do_accept() {
         acceptor->async_accept(
                                [this](std::error_code ec, asio::ip::tcp::socket socket) {
                                   if (!ec) {
                                      // Process the connection in a separate coroutine
                                      process_request(std::move(socket));
                                   } else {
                                      error_handler(ec, std::source_location::current());
                                   }
                                   
                                   // Continue accepting if still running
                                   if (running) {
                                      do_accept();
                                   }
                                });
      }
      
      inline void process_request(asio::ip::tcp::socket socket) {
         // Capture socket in a shared_ptr for async operations
         auto socket_ptr = std::make_shared<asio::ip::tcp::socket>(std::move(socket));
         auto remote_endpoint = socket_ptr->remote_endpoint();
         
         // Buffer for the request
         auto buffer = std::make_shared<asio::streambuf>();
         
         // Read the request asynchronously
         asio::async_read_until(*socket_ptr, *buffer, "\r\n\r\n",
                                [this, socket_ptr, buffer, remote_endpoint](std::error_code ec, std::size_t bytes_transferred) {
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
               // Calculate how much of the body we've already read
               std::size_t body_bytes_already_read = buffer->size() - bytes_transferred;
               
               if (body_bytes_already_read < content_length) {
                  // Need to read more data
                  asio::async_read(*socket_ptr, *buffer,
                                   asio::transfer_exactly(content_length - body_bytes_already_read),
                                   [this, socket_ptr, buffer, method_opt, target, headers, remote_endpoint](
                                                                                                            std::error_code ec, std::size_t /*bytes_transferred*/) {
                                                                                                               if (ec) {
                                                                                                                  error_handler(ec, std::source_location::current());
                                                                                                                  return;
                                                                                                               }
                                                                                                               
                                                                                                               // Now we have the full request, process it
                                                                                                               std::string body(std::istreambuf_iterator<char>(buffer.get()), {});
                                                                                                               process_full_request(socket_ptr, *method_opt, target, headers,
                                                                                                                                    std::move(body), remote_endpoint);
                                                                                                            });
               } else {
                  // We've already read enough data
                  std::string body;
                  body.reserve(content_length);
                  
                  // Extract the body from the buffer
                  for (std::size_t i = 0; i < content_length; ++i) {
                     body.push_back(static_cast<char>(buffer->sbumpc()));
                  }
                  
                  process_full_request(socket_ptr, *method_opt, target, headers,
                                       std::move(body), remote_endpoint);
               }
            } else {
               // No body, process the request immediately
               process_full_request(socket_ptr, *method_opt, target, headers,
                                    "", remote_endpoint);
            }
         });
      }
      
      inline void process_full_request(
                                       std::shared_ptr<asio::ip::tcp::socket> socket,
                                       Method method,
                                       const std::string& target,
                                       const std::unordered_map<std::string, std::string>& headers,
                                       std::string body,
                                       asio::ip::tcp::endpoint remote_endpoint) {
         
         // Create the request object
         Request request;
         request.method = method;
         request.target = target;
         request.headers = headers;
         request.body = std::move(body);
         request.remote_endpoint = remote_endpoint;
         
         // Find a matching route
         Handler handler = nullptr;
         
         // Simple exact matching for now
         auto route_it = root_router.routes.find(request.target);
         if (route_it != root_router.routes.end()) {
            // Found a matching path, check method
            auto method_it = route_it->second.find(method);
            if (method_it != route_it->second.end()) {
               handler = method_it->second;
            }
         }
         
         // TODO: Implement route matching with path parameters
         // For now, just handle exact matches
         
         if (!handler) {
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
            
            // Call the handler
            handler(request, response);
            
            // Send the response
            send_response(socket, response);
         } catch (const std::exception& e) {
            // Handle exceptions
            error_handler(
                          std::make_error_code(std::errc::invalid_argument),
                          std::source_location::current());
            
            send_error_response(socket, 500, "Internal Server Error");
         }
      }
      
      inline void send_response(
                                std::shared_ptr<asio::ip::tcp::socket> socket,
                                const Response& response) {
         
         // Format the response
         std::ostringstream oss;
         oss << "HTTP/1.1 " << response.status_code << " "
         << get_status_message(response.status_code) << "\r\n";
         
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
                           [socket](std::error_code ec, std::size_t /*bytes_transferred*/) {
            // The socket will be closed when it goes out of scope
         });
      }
      
      inline void send_error_response(
                                      std::shared_ptr<asio::ip::tcp::socket> socket,
                                      int status_code,
                                      const std::string& message) {
         
         Response response;
         response.status(status_code)
            .content_type("text/plain")
            .body(message);
         
         send_response(socket, response);
      }
      
      inline std::string get_status_message(int status_code) {
         switch (status_code) {
            case 200: return "OK";
            case 201: return "Created";
            case 204: return "No Content";
            case 400: return "Bad Request";
            case 401: return "Unauthorized";
            case 403: return "Forbidden";
            case 404: return "Not Found";
            case 405: return "Method Not Allowed";
            case 409: return "Conflict";
            case 418: return "I'm a teapot";
            case 500: return "Internal Server Error";
            case 501: return "Not Implemented";
            case 503: return "Service Unavailable";
            default: return "Unknown";
         }
      }
      
      inline std::string get_current_date() {
         auto now = std::chrono::system_clock::now();
         auto time_t_now = std::chrono::system_clock::to_time_t(now);
         
         char buf[100];
         std::strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", std::gmtime(&time_t_now));
         
         return buf;
      }
   };
   
   // Client implementation using non-blocking asio
   class Client {
   public:
      inline Client() : io_context(std::make_unique<asio::io_context>()) {
         // Start the IO thread
         io_thread = std::thread([this] {
            io_context->run();
         });
      }
      
      inline ~Client() {
         // Stop the IO context
         io_context->stop();
         
         // Join the IO thread
         if (io_thread.joinable()) {
            io_thread.join();
         }
      }
      
      inline std::expected<Response, std::error_code> get(
                                                          std::string_view url,
                                                          const std::unordered_map<std::string, std::string>& headers = {}) {
         
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
         } else {
            port = static_cast<uint16_t>(std::stoi(port_str));
         }
         
         // Create a promise for the response
         std::promise<std::expected<Response, std::error_code>> promise;
         auto future = promise.get_future();
         
         // Create a resolver to translate the hostname into IP address
         auto resolver = std::make_shared<asio::ip::tcp::resolver>(*io_context);
         
         // Start the async resolution
         resolver->async_resolve(host, std::to_string(port),
                                 [this, resolver, host, path, headers, promise = std::move(promise)]
                                 (std::error_code ec, asio::ip::tcp::resolver::results_type results) mutable {
            if (ec) {
               promise.set_value(std::unexpected(ec));
               return;
            }
            
            // Create a socket
            auto socket = std::make_shared<asio::ip::tcp::socket>(*io_context);
            
            // Connect to the server
            asio::async_connect(*socket, results,
                                [this, socket, host, path, headers, promise = std::move(promise)]
                                (std::error_code ec, const asio::ip::tcp::endpoint& endpoint) mutable {
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
               asio::async_write(*socket, asio::buffer(request_str),
                                 [this, socket, promise = std::move(promise)]
                                 (std::error_code ec, std::size_t /*bytes_transferred*/) mutable {
                  if (ec) {
                     promise.set_value(std::unexpected(ec));
                     return;
                  }
                  
                  // Create a buffer for the response
                  auto buffer = std::make_shared<asio::streambuf>();
                  
                  // Read the response headers
                  asio::async_read_until(*socket, *buffer, "\r\n\r\n",
                                         [this, socket, buffer, promise = std::move(promise)]
                                         (std::error_code ec, std::size_t /*bytes_transferred*/) mutable {
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
                        promise.set_value(std::unexpected(
                                                          std::make_error_code(std::errc::protocol_error)));
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
                     asio::async_read(*socket, *buffer, asio::transfer_all(),
                                      [this, socket, buffer, status_code, response_headers, promise = std::move(promise)]
                                      (std::error_code ec, std::size_t /*bytes_transferred*/) mutable {
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
                                                           std::string_view url,
                                                           std::string_view body,
                                                           const std::unordered_map<std::string, std::string>& headers = {}) {
         
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
         } else {
            port = static_cast<uint16_t>(std::stoi(port_str));
         }
         
         // Create a promise for the response
         std::promise<std::expected<Response, std::error_code>> promise;
         auto future = promise.get_future();
         
         // Create a resolver to translate the hostname into IP address
         auto resolver = std::make_shared<asio::ip::tcp::resolver>(*io_context);
         
         // Start the async resolution
         resolver->async_resolve(host, std::to_string(port),
                                 [this, resolver, host, path, body = std::string(body), headers, promise = std::move(promise)]
                                 (std::error_code ec, asio::ip::tcp::resolver::results_type results) mutable {
            if (ec) {
               promise.set_value(std::unexpected(ec));
               return;
            }
            
            // Create a socket
            auto socket = std::make_shared<asio::ip::tcp::socket>(*io_context);
            
            // Connect to the server
            asio::async_connect(*socket, results,
                                [this, socket, host, path, body, headers, promise = std::move(promise)]
                                (std::error_code ec, const asio::ip::tcp::endpoint& endpoint) mutable {
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
               asio::async_write(*socket, asio::buffer(request_str),
                                 [this, socket, promise = std::move(promise)]
                                 (std::error_code ec, std::size_t /*bytes_transferred*/) mutable {
                  if (ec) {
                     promise.set_value(std::unexpected(ec));
                     return;
                  }
                  
                  // ... Rest of code similar to get() implementation ...
                  // Create a buffer for the response
                  auto buffer = std::make_shared<asio::streambuf>();
                  
                  // Read the response headers
                  asio::async_read_until(*socket, *buffer, "\r\n\r\n",
                                         [this, socket, buffer, promise = std::move(promise)]
                                         (std::error_code ec, std::size_t /*bytes_transferred*/) mutable {
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
                     asio::async_read(*socket, *buffer, asio::transfer_all(),
                                      [this, socket, buffer, status_code, response_headers, promise = std::move(promise)]
                                      (std::error_code ec, std::size_t /*bytes_transferred*/) mutable {
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
                                                                             std::string_view url,
                                                                             const std::unordered_map<std::string, std::string>& headers = {}) {
         
         // Create a promise and future
         auto promise = std::make_shared<std::promise<std::expected<Response, std::error_code>>>();
         auto future = promise->get_future();
         
         // Run the operation asynchronously
         asio::post(*io_context, [this, url = std::string(url), headers, promise]() {
            try {
               auto result = get(url, headers);
               promise->set_value(std::move(result));
            } catch (...) {
               promise->set_exception(std::current_exception());
            }
         });
         
         return future;
      }
      
      inline std::future<std::expected<Response, std::error_code>> post_async(
                                                                              std::string_view url,
                                                                              std::string_view body,
                                                                              const std::unordered_map<std::string, std::string>& headers = {}) {
         
         // Create a promise and future
         auto promise = std::make_shared<std::promise<std::expected<Response, std::error_code>>>();
         auto future = promise->get_future();
         
         // Run the operation asynchronously
         asio::post(*io_context, [this, url = std::string(url),
                                  body = std::string(body), headers, promise]() {
            try {
               auto result = post(url, body, headers);
               promise->set_value(std::move(result));
            } catch (...) {
               promise->set_exception(std::current_exception());
            }
         });
         
         return future;
      }
      
      // JSON helpers using Glaze
      template<typename T>
      inline std::expected<Response, std::error_code> post_json(
                                                                std::string_view url,
                                                                const T& data,
                                                                const std::unordered_map<std::string, std::string>& headers = {}) {
         
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
