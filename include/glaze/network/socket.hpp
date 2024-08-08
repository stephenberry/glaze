// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/glaze.hpp"

#ifdef _WIN32
#define GLZ_CLOSE_SOCKET closesocket
#define GLZ_EWOULDBLOCK WSAEWOULDBLOCK
#define GLZ_INVALID_SOCKET INVALID_SOCKET
#define GLZ_SOCKET SOCKET
#define GLZ_SOCKET_ERROR SOCKET_ERROR
#define GLZ_SOCKET_ERROR_CODE WSAGetLastError()
#include <winsock2.h>
#include <ws2tcpip.h>

#include <cstdint>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <sys/socket.h>
#include <sys/types.h>
#if __has_include(<netinet/in.h>)
#include <netinet/in.h>
#endif
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <unistd.h>

#include <cerrno>
#define GLZ_CLOSE_SOCKET ::close
#define GLZ_EWOULDBLOCK EWOULDBLOCK
#define GLZ_INVALID_SOCKET (-1)
#define GLZ_SOCKET int
#define GLZ_SOCKET_ERROR (-1)
#define GLZ_SOCKET_ERROR_CODE errno
#endif

#if defined(__APPLE__)
#include <sys/event.h> // for kqueue on macOS
#elif defined(__linux__)
#include <sys/epoll.h> // for epoll on Linux
#endif

#include <csignal>
#include <format>
#include <functional>
#include <future>
#include <mutex>
#include <system_error>
#include <thread>
#include <vector>

namespace glz
{
   namespace ip
   {
      struct opts
      {};
   }

   inline std::string get_ip_port(const sockaddr_in& server_addr)
   {
      char ip_str[INET_ADDRSTRLEN]{};

#ifdef _WIN32
      inet_ntop(AF_INET, &(server_addr.sin_addr), ip_str, INET_ADDRSTRLEN);
#else
      inet_ntop(AF_INET, &(server_addr.sin_addr), ip_str, sizeof(ip_str));
#endif

      return {std::format("{}:{}", ip_str, ntohs(server_addr.sin_port))};
   }

   inline std::string get_socket_error_message(int err)
   {
#ifdef _WIN32

      char* msg = nullptr;
      FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                     err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&msg, 0, NULL);
      std::string message(msg);
      LocalFree(msg);
      return {message};

#else
      return strerror(err);
#endif
   }

   struct socket_api_error_category_t final : public std::error_category
   {
      std::string what{};
      const char* name() const noexcept override { return "socket error"; }
      std::string message(int ev) const override
      {
         if (what.empty()) {
            return {get_socket_error_message(ev)};
         }
         else {
            return {std::format("{}\nDetails: {}", what, get_socket_error_message(ev))};
         }
      }
      void operator()(int ev, const std::string_view w)
      {
         what = w;
         this->message(ev);
      }
   };

   inline const socket_api_error_category_t& socket_api_error_category(const std::string_view what)
   {
      static socket_api_error_category_t singleton;
      singleton.what = what;
      return singleton;
   }

   inline std::error_code get_socket_error(const std::string_view what = "")
   {
#ifdef _WIN32
      int err = WSAGetLastError();
#else
      int err = errno;
#endif

      return {std::error_code(err, socket_api_error_category(what))};
   }

   inline std::error_code check_status(int ec, const std::string_view what = "")
   {
      if (ec >= 0) {
         return {};
      }

      return {get_socket_error(what)};
   }

   // Example:
   //
   // std::error_code ec = check_status(result, "connect failed");
   //
   // if (ec) {
   //    std::cerr << get_socket_error(std::format("Failed to connect to socket at address: {}.\nIs the server
   //    running?", ip_port)).message();
   // }
   // else {
   //    std::cout << "Connected successfully!";
   // }

   // For Windows WSASocket Compatability

   inline constexpr uint16_t make_version(uint8_t low_byte, uint8_t high_byte) noexcept
   {
      return uint16_t(low_byte) | (uint16_t(high_byte) << 8);
   }

   inline constexpr uint8_t major_version(uint16_t version) noexcept
   {
      return uint8_t(version & 0xFF); // Extract the low byte
   }

   inline constexpr uint8_t minor_version(uint16_t version) noexcept
   {
      return uint8_t((version >> 8) & 0xFF); // Shift right by 8 bits and extract the low byte
   }

   // Function to get Winsock version string on Windows, return "na" otherwise
   inline std::string get_winsock_version_string(uint32_t version = make_version(2, 2))
   {
#if _WIN32
      BYTE major = major_version(uint16_t(version));
      BYTE minor = minor_version(uint16_t(version));
      return std::format("{}.{}", static_cast<int>(major), static_cast<int>(minor));
#else
      (void)version;
      return ""; // Default behavior for non-Windows platforms
#endif
   }

   // The 'wsa_startup_t' calls the windows WSAStartup function. This must be the first Windows
   // Sockets function called by an application or DLL. It allows an application or DLL to
   // specify the version of Windows Sockets required and retrieve details of the specific
   // Windows Sockets implementation.The application or DLL can only issue further Windows Sockets
   // functions after successfully calling WSAStartup.
   //
   // Important: WSAStartup and its corresponding WSACleanup must be called on the same thread.
   //
   template <bool run_wsa_startup = true>
   struct windows_socket_startup_t final
   {
#ifdef _WIN64
      WSADATA wsa_data{};

      std::error_code error_code{};

      std::error_code start(const WORD win_sock_version = make_version(2, 2)) // Request latest Winsock version 2.2
      {
         static std::once_flag flag{};
         std::error_code startup_error{};
         std::call_once(flag, [this, win_sock_version, &startup_error]() {
            int result = WSAStartup(win_sock_version, &wsa_data);
            if (result != 0) {
               error_code = get_socket_error(
                  std::format("Unable to initialize Winsock library version {}.", get_winsock_version_string()));
            }
         });
         return {error_code};
      }

      windows_socket_startup_t()
      {
         if constexpr (run_wsa_startup) {
            error_code = start();
         }
      }

      ~windows_socket_startup_t() { WSACleanup(); }

#else
      std::error_code start() { return std::error_code{}; }
#endif
   };

   enum ip_error {
      none = 0,
      queue_create_failed,
      event_ctl_failed,
      event_wait_failed,
      event_enum_failed,
      socket_connect_failed,
      socket_bind_failed,
      send_failed,
      receive_failed,
      client_disconnected
   };

   template <class T>
   concept ip_header = std::same_as<T, uint64_t> || requires { T::body_size; };

   struct ip_error_category : public std::error_category
   {
      static const ip_error_category& instance()
      {
         static ip_error_category instance{};
         return instance;
      }

      const char* name() const noexcept override { return "ip_error_category"; }

      std::string message(int ec) const override
      {
         using enum ip_error;
         switch (static_cast<ip_error>(ec)) {
         case none:
            return "none";
         case queue_create_failed:
            return "queue_create_failed";
         case event_ctl_failed:
            return "event_ctl_failed";
         case event_wait_failed:
            return "event_wait_failed";
         case event_enum_failed:
            return "event_enum_failed";
         case socket_connect_failed:
            return "socket_connect_failed";
         case socket_bind_failed:
            return "socket_bind_failed";
         case send_failed:
            return "send_failed";
         case receive_failed:
            return "receive_failed";
         case client_disconnected:
            return "client_disconnected";
         default:
            return "unknown_error";
         }
      }
   };

   struct socket
   {
      GLZ_SOCKET socket_fd{GLZ_INVALID_SOCKET};

      void set_non_blocking()
      {
#ifdef _WIN32
         u_long mode = 1;
         ioctlsocket(socket_fd, FIONBIO, &mode);
#else
         int flags = fcntl(socket_fd, F_GETFL, 0);
         fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK);
#endif
      }

      socket() = default;

      socket(GLZ_SOCKET fd) : socket_fd(fd) { set_non_blocking(); }

      void close()
      {
         if (socket_fd != GLZ_INVALID_SOCKET) {
            GLZ_CLOSE_SOCKET(socket_fd);
         }
      }

      ~socket() { close(); }

      [[nodiscard]] std::error_code connect(const std::string& address, const int port)
      {
         socket_fd = ::socket(AF_INET, SOCK_STREAM, 0);
         if (socket_fd == -1) {
            return {ip_error::socket_connect_failed, ip_error_category::instance()};
         }

         sockaddr_in server_addr;
         server_addr.sin_family = AF_INET;
         server_addr.sin_port = htons(uint16_t(port));
         inet_pton(AF_INET, address.c_str(), &server_addr.sin_addr);

         if (::connect(socket_fd, (sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
            return {ip_error::socket_connect_failed, ip_error_category::instance()};
         }

         set_non_blocking();

         return {};
      }

      [[nodiscard]] bool no_delay()
      {
         int flag = 1;
         int result = setsockopt(socket_fd, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(int));
         return result == 0;
      }

      [[nodiscard]] std::error_code bind_and_listen(int port)
      {
         socket_fd = ::socket(AF_INET, SOCK_STREAM, 0);
         if (socket_fd == GLZ_INVALID_SOCKET) {
            return {ip_error::socket_bind_failed, ip_error_category::instance()};
         }

         sockaddr_in server_addr;
         server_addr.sin_family = AF_INET;
         server_addr.sin_addr.s_addr = INADDR_ANY;
         server_addr.sin_port = htons(uint16_t(port));

         if (::bind(socket_fd, (sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
            return {ip_error::socket_bind_failed, ip_error_category::instance()};
         }

         if (::listen(socket_fd, SOMAXCONN) == -1) {
            return {ip_error::socket_bind_failed, ip_error_category::instance()};
         }

         set_non_blocking();
         if (not no_delay()) {
            return {ip_error::socket_bind_failed, ip_error_category::instance()};
         }

         return {};
      }
      
      template <ip_header Header>
      [[nodiscard]] std::error_code receive(Header& header, std::string& buffer)
      {
         // first receive the header
         size_t total_bytes{};
         while (total_bytes < sizeof(Header)) {
            auto bytes = ::recv(socket_fd, reinterpret_cast<char*>(&header) + total_bytes,
                                   size_t(sizeof(Header) - total_bytes), 0);
            if (bytes == -1) {
               if (GLZ_SOCKET_ERROR_CODE == GLZ_EWOULDBLOCK || GLZ_SOCKET_ERROR_CODE == EAGAIN) {
                  std::this_thread::sleep_for(std::chrono::milliseconds(1));
                  continue;
               }
               else {
                  // error
                  buffer.clear();
                  return {ip_error::receive_failed, ip_error_category::instance()};
               }
            }
            else if (bytes == 0) {
               return {ip_error::client_disconnected, ip_error_category::instance()};
            }

            total_bytes += bytes;
         }

         size_t size{};
         if constexpr (std::same_as<Header, uint64_t>) {
            size = header;
         }
         else {
            size = size_t(header.body_size);
         }

         buffer.resize(size);

         total_bytes = 0;
         while (total_bytes < size) {
            auto bytes = ::recv(socket_fd, buffer.data() + total_bytes, size_t(buffer.size() - total_bytes), 0);
            if (bytes == -1) {
               if (GLZ_SOCKET_ERROR_CODE == GLZ_EWOULDBLOCK || GLZ_SOCKET_ERROR_CODE == EAGAIN) {
                  std::this_thread::sleep_for(std::chrono::milliseconds(1));
                  continue;
               }
               else {
                  buffer.clear();
                  return {ip_error::receive_failed, ip_error_category::instance()};
               }
            }
            else if (bytes == 0) {
               return {ip_error::client_disconnected, ip_error_category::instance()};
            }

            total_bytes += bytes;
         }
         return {};
      }

      [[nodiscard]] std::error_code send(const std::string_view buffer)
      {
         const size_t size = buffer.size();
         size_t total_bytes{};
         while (total_bytes < size) {
            auto bytes = ::send(socket_fd, buffer.data() + total_bytes, size_t(buffer.size() - total_bytes), 0);
            if (bytes == -1) {
               if (GLZ_SOCKET_ERROR_CODE == GLZ_EWOULDBLOCK || GLZ_SOCKET_ERROR_CODE == EAGAIN) {
                  std::this_thread::yield();
                  continue;
               }
               else {
                  return {ip_error::send_failed, ip_error_category::instance()};
               }
            }
            
            total_bytes += bytes;
         }
         return {};
      }

      enum class io_result { completed, would_block, error };

      io_result send(const std::string_view& buffer, size_t& bytes_sent)
      {
         auto remaining = buffer.size() - bytes_sent;
         auto result = ::send(socket_fd, buffer.data() + bytes_sent, remaining, 0);
         if (result > 0) {
            bytes_sent += result;
            return bytes_sent == buffer.size() ? io_result::completed : io_result::would_block;
         }
         else if (result == 0 ||
                  (result == -1 && GLZ_SOCKET_ERROR_CODE != GLZ_EWOULDBLOCK && GLZ_SOCKET_ERROR_CODE != EAGAIN)) {
            return io_result::error;
         }
         return io_result::would_block;
      }

      template <ip_header Header>
      io_result receive(Header& header, std::string& buffer, size_t& bytes_received)
      {
         if (bytes_received < sizeof(Header)) {
            auto result =
               ::recv(socket_fd, reinterpret_cast<char*>(&header) + bytes_received, sizeof(Header) - bytes_received, 0);
            if (result > 0) {
               bytes_received += result;
               if (bytes_received < sizeof(Header)) {
                  return io_result::would_block;
               }
            }
            else if (result == 0 ||
                     (result == -1 && GLZ_SOCKET_ERROR_CODE != GLZ_EWOULDBLOCK && GLZ_SOCKET_ERROR_CODE != EAGAIN)) {
               return io_result::error;
            }
            else {
               return io_result::would_block;
            }
         }
         
         size_t size{};
         if constexpr (std::same_as<Header, uint64_t>) {
            size = header;
         }
         else {
            size = size_t(header.body_size);
         }
         
         buffer.resize(size);

         auto result = ::recv(socket_fd, buffer.data() + (bytes_received - sizeof(Header)),
                              buffer.size() - (bytes_received - sizeof(Header)), 0);
         if (result > 0) {
            bytes_received += result;
            return bytes_received == (sizeof(Header) + buffer.size()) ? io_result::completed : io_result::would_block;
         }
         else if (result == 0 ||
                  (result == -1 && GLZ_SOCKET_ERROR_CODE != GLZ_EWOULDBLOCK && GLZ_SOCKET_ERROR_CODE != EAGAIN)) {
            return io_result::error;
         }
         return io_result::would_block;
      }
   };
}

#undef GLZ_CLOSE_SOCKET
#undef GLZ_EWOULDBLOCK
#undef GLZ_INVALID_SOCKET
#undef GLZ_SOCKET
#undef GLZ_SOCKET_ERROR
#undef GLZ_SOCKET_ERROR_CODE
