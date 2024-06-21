// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#ifdef _WIN32
#define GLZ_CLOSE_SOCKET closesocket
#define GLZ_EVENT_CLOSE WSACloseEvent
#define GLZ_INVALID_EVENT WSA_INVALID_EVENT
#define GLZ_INVALID_SOCKET INVALID_SOCKET
#define GLZ_SOCKET SOCKET
#define GLZ_SOCKET_ERROR GLZ_SOCKET_ERROR
#define GLZ_SOCKET_ERROR_CODE WSAGetLastError()
#define GLZ_WAIT_FAILED WSA_WAIT_FAILED
#define GLZ_WAIT_RESULT_TYPE DWORD
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>

#include <cstdint>
#pragma comment(lib, "Ws2_32.lib")
using ssize_t = int64_t;
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
#define GLZ_EVENT_CLOSE ::close
#define GLZ_INVALID_EVENT (-1)
#define GLZ_INVALID_SOCKET (-1)
#define GLZ_SOCKET int
#define GLZ_SOCKET_ERROR (-1)
#define GLZ_SOCKET_ERROR_CODE errno
#define GLZ_WAIT_FAILED (-1)
#define GLZ_WAIT_RESULT_TYPE int
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
#include <iostream>
#include <mutex>
#include <stop_token>
#include <system_error>
#include <thread>
#include <vector>

#include "glaze/rpc/repe.hpp"

// New REPE header
// TOD0: update the REPE code
namespace glz
{
   struct Header
   {
      static constexpr size_t max_method_size = 256;

      uint8_t version = 1; // the REPE version
      bool error{}; // whether an error has occurred
      bool notify{}; // whether this message does not require a response
      bool has_body{}; // whether a body is provided
      uint32_t reserved1{};
      // ---
      uint64_t id{}; // identifier
      int64_t body_size = -1; // the total size of the body
      uint32_t reserved2{};
      uint16_t reserved3{};
      // ---
      uint16_t method_size{}; // the size of the method string
      char method[max_method_size]{}; // the method name
   };

   static_assert((sizeof(Header) - Header::max_method_size) == 32);
}

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
      socket_connect_failed = 1001,
      socket_bind_failed = 1002,
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
         if (socket_fd != -1) {
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
         if (socket_fd == -1) {
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
            ssize_t bytes = ::recv(socket_fd, &header + total_bytes, size_t(sizeof(Header) - total_bytes), 0);
            if (bytes == -1) {
               if (GLZ_SOCKET_ERROR_CODE == EWOULDBLOCK || GLZ_SOCKET_ERROR_CODE == EAGAIN) {
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
         
         if constexpr (std::same_as<Header, uint64_t>) {
            buffer.resize(header);
         }
         else {
            buffer.resize(header.body_size);
         }
         
         total_bytes = 0;
         const auto n = size_t(header.body_size);
         while (total_bytes < n) {
            ssize_t bytes = ::recv(socket_fd, buffer.data() + total_bytes, size_t(buffer.size() - total_bytes), 0);
            if (bytes == -1) {
               if (GLZ_SOCKET_ERROR_CODE == EWOULDBLOCK || GLZ_SOCKET_ERROR_CODE == EAGAIN) {
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
         return {};
      }
      
      [[nodiscard]] std::error_code send(const std::string_view buffer)
      {
         const size_t size = buffer.size();
         size_t total_bytes{};
         while (total_bytes < size) {
            ssize_t bytes = ::send(socket_fd, buffer.data() + total_bytes, size_t(buffer.size() - total_bytes), 0);
            if (bytes == -1) {
               if (GLZ_SOCKET_ERROR_CODE == EWOULDBLOCK || GLZ_SOCKET_ERROR_CODE == EAGAIN) {
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
   };

   namespace detail
   {
      inline void server_thread_cleanup(std::vector<std::future<void>>& threads)
      {
         for (auto rit = threads.rbegin(); rit < threads.rend();)
         {
            auto& future = *rit;
            if (future.valid()) {
               if (auto status = future.wait_for(std::chrono::milliseconds(0));
                   status == std::future_status::ready) {
                  rit = std::reverse_iterator(threads.erase(std::next(rit).base()));
               }
               continue;
            }
            
            ++rit;
         }
      }
   }
      
   template <opts Opts = opts{.format = binary}, class T>
   [[nodiscard]] std::error_code receive(socket& sckt, T&& value)
   {
      static thread_local std::string buffer{};

      Header header{};
      if (auto ec = sckt.receive(header, buffer)) {
         return ec;
      }

      if (auto ec = glz::read<Opts>(std::forward<T>(value), buffer)) {
         return {ip_error::receive_failed, ip_error_category::instance()};
      }

      return {};
   }

   template <opts Opts = opts{.format = binary}, class T>
   [[nodiscard]] std::error_code send(socket& sckt, T&& value)
   {
      static thread_local std::string buffer{};

      if (auto ec = glz::write<Opts>(std::forward<T>(value), buffer)) {
         return {ip_error::send_failed, ip_error_category::instance()};
      }

      Header header{.body_size = int64_t(buffer.size())};

      if (auto ec = sckt.send(sv{reinterpret_cast<char*>(&header), sizeof(Header)})) {
         return ec;
      }

      if (auto ec = sckt.send(buffer)) {
         return ec;
      }

      return {};
   }

   struct server final
   {
      int port{};
      std::vector<std::future<void>> threads{};
      std::atomic<bool> active = true;

      ~server() { active = false; }

      template <class AcceptCallback>
      [[nodiscard]] std::error_code accept(AcceptCallback&& callback)
      {
         glz::socket accept_socket{};

         const auto ec = accept_socket.bind_and_listen(port);
         if (ec) {
            return {ip_error::socket_bind_failed, ip_error_category::instance()};
         }

#if defined(__APPLE__)
         int event_fd = ::kqueue();
#elif defined(__linux__)
         int event_fd = ::epoll_create1(0);
#elif defined(_WIN32)
         HANDLE event_fd = WSACreateEvent();
#endif

         if (event_fd == GLZ_INVALID_EVENT) {
            return {ip_error::queue_create_failed, ip_error_category::instance()};
         }

         bool event_setup_failed = false;
#if defined(__APPLE__)
         struct kevent change;
         EV_SET(&change, accept_socket.socket_fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, nullptr);
         event_setup_failed = ::kevent(event_fd, &change, 1, nullptr, 0, nullptr) == -1;
#elif defined(__linux__)
         struct epoll_event ev;
         ev.events = EPOLLIN;
         ev.data.fd = accept_socket.socket_fd;
         event_setup_failed = epoll_ctl(event_fd, EPOLL_CTL_ADD, accept_socket.socket_fd, &ev) == -1;
#elif defined(_WIN32)
         event_setup_failed = WSAEventSelect(accept_socket.socket_fd, event_fd, FD_ACCEPT) == GLZ_SOCKET_ERROR;
#endif

         if (event_setup_failed) {
            GLZ_EVENT_CLOSE(event_fd);
            return {ip_error::event_ctl_failed, ip_error_category::instance()};
         }

#if defined(__APPLE__)
         std::vector<struct kevent> events(16);
#elif defined(__linux__)
         std::vector<struct epoll_event> epoll_events(16);
#endif

         while (active) {
            GLZ_WAIT_RESULT_TYPE n{};

#if defined(__APPLE__)
            struct timespec timeout
            {
               0, 10000000
            }; // 10ms
            n = ::kevent(event_fd, nullptr, 0, events.data(), static_cast<int>(events.size()), &timeout);
#elif defined(__linux__)
            n = ::epoll_wait(event_fd, epoll_events.data(), static_cast<int>(epoll_events.size()), 10);
#elif defined(_WIN32)
            n = WSAWaitForMultipleEvents(1, &event_fd, FALSE, 10, FALSE);
#endif

            if (n == GLZ_WAIT_FAILED) {
#if defined(__APPLE__) || defined(__linux__)
               if (errno == EINTR) continue;
#else
               if (n == WSA_WAIT_TIMEOUT) continue;
#endif
               GLZ_EVENT_CLOSE(event_fd);
               return {ip_error::event_wait_failed, ip_error_category::instance()};
            }

            auto spawn_socket = [&] {
               sockaddr_in client_addr;
               socklen_t client_len = sizeof(client_addr);
               auto client_fd = ::accept(accept_socket.socket_fd, (sockaddr*)&client_addr, &client_len);
               if (client_fd != GLZ_INVALID_SOCKET) {
                  threads.emplace_back(
                     std::async([this, callback, client_fd] { callback(socket{client_fd}, active); }));
               }
            };

#if defined(__APPLE__) || defined(__linux__)
            for (int i = 0; i < n; ++i) {
#if defined(__APPLE__)
               if (events[i].ident == uintptr_t(accept_socket.socket_fd) && events[i].filter == EVFILT_READ) {
#elif defined(__linux__)
               if (epoll_events[i].data.fd == accept_socket.socket_fd && epoll_events[i].events & EPOLLIN) {
#endif
                  spawn_socket();
               }
            }

#else // Windows
            WSANETWORKEVENTS events;
            if (WSAEnumNetworkEvents(accept_socket.socket_fd, event_fd, &events) == GLZ_SOCKET_ERROR) {
               WSACloseEvent(event_fd);
               return {ip_error::event_enum_failed, ip_error_category::instance()};
            }

            if (events.lNetworkEvents & FD_ACCEPT) {
               if (events.iErrorCode[FD_ACCEPT_BIT] == 0) {
                  spawn_socket();
               }
            }
#endif

            detail::server_thread_cleanup(threads);
         }

         GLZ_EVENT_CLOSE(event_fd);
         return {};
      }
   };
}

#undef GLZ_CLOSE_SOCKET
#undef GLZ_EVENT_CLOSE
#undef GLZ_INVALID_EVENT
#undef GLZ_INVALID_SOCKET
#undef GLZ_SOCKET
#undef GLZ_SOCKET_ERROR
#undef GLZ_SOCKET_ERROR_CODE
#undef GLZ_WAIT_FAILED
#undef GLZ_WAIT_RESULT_TYPE
