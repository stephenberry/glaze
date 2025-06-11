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
                     std::string request_str;
                     request_str.append("GET ");
                     request_str.append(path);
                     request_str.append(" HTTP/1.1\r\n");
                     request_str.append("Host: ");
                     request_str.append(host);
                     request_str.append("\r\n");
                     request_str.append("Connection: close\r\n");

                     // Add headers
                     for (const auto& [name, value] : headers) {
                        request_str.append(name);
                        request_str.append(": ");
                        request_str.append(value);
                        request_str.append("\r\n");
                     }

                     // End of headers
                     request_str.append("\r\n");

                     // Send the request
                     asio::async_write(
                        *socket, asio::buffer(request_str),
                        [socket, promise = std::move(promise), request_str = std::move(request_str)](
                           std::error_code ec, std::size_t /*bytes_transferred*/) mutable {
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
                     std::string request_str;
                     request_str.append("POST ");
                     request_str.append(path);
                     request_str.append(" HTTP/1.1\r\n");
                     request_str.append("Host: ");
                     request_str.append(host);
                     request_str.append("\r\n");
                     request_str.append("Connection: close\r\n");
                     request_str.append("Content-Length: ");
                     request_str.append(std::to_string(body.size()));
                     request_str.append("\r\n");

                     // Add headers
                     for (const auto& [name, value] : headers) {
                        request_str.append(name);
                        request_str.append(": ");
                        request_str.append(value);
                        request_str.append("\r\n");
                     }

                     // End of headers
                     request_str.append("\r\n");

                     // Add body
                     request_str.append(body);

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
