// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#define CLOSESOCKET closesocket
#define SOCKET_ERROR_CODE WSAGetLastError()
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
#define CLOSESOCKET close
#define SOCKET_ERROR_CODE errno
#define SOCKET int
#endif

#include <format>
#include <functional>
#include <iostream>
#include <mutex>
#include <system_error>
#include <thread>
#include <vector>

#include "glaze/thread/threadpool.hpp"

namespace glz
{
   namespace ip
   {
      struct opts
      {};
   }

   inline void wsa_startup()
   {
      static std::once_flag flag{};
      std::call_once(flag, [] {
#ifdef _WIN32
         WSADATA wsaData;
         WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
      });
   }

   enum ip_error { none = 0, socket_connect_failed = 1001, socket_bind_failed = 1002 };

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
         case socket_connect_failed:
            return "socket_connect_failed";
         case socket_bind_failed:
            return "socket_bind_failed";
         default:
            return "unknown_error";
         }
      }
   };
   
   template <class Function>
   concept is_callback = std::invocable<Function, std::string&> || std::invocable<Function, const std::string&>;

   struct socket
   {
      SOCKET socket_fd{-1};

      void set_non_blocking(SOCKET /*fd*/)
      {
/*#ifdef _WIN32
         u_long mode = 1;
         ioctlsocket(fd, FIONBIO, &mode);
#else
         int flags = fcntl(fd, F_GETFL, 0);
         fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#endif*/
      }

      socket() { wsa_startup(); }

      socket(SOCKET fd) : socket_fd(fd)
      {
         wsa_startup();
         set_non_blocking(socket_fd);
      }

      ~socket()
      {
         if (socket_fd != -1) {
            CLOSESOCKET(socket_fd);
         }
#ifdef _WIN32
         WSACleanup();
#endif
      }

      std::error_code connect(const std::string& address, int port)
      {
         socket_fd = ::socket(AF_INET, SOCK_STREAM, 0);
         if (socket_fd == -1) {
            return {ip_error::socket_connect_failed, ip_error_category::instance()};
         }

         sockaddr_in server_addr;
         server_addr.sin_family = AF_INET;
         server_addr.sin_port = htons(port);
         inet_pton(AF_INET, address.c_str(), &server_addr.sin_addr);

         if (::connect(socket_fd, (sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
            return {ip_error::socket_connect_failed, ip_error_category::instance()};
         }

         set_non_blocking(socket_fd);

         return {};
      }

      bool no_delay()
      {
         int flag = 1;
         int result = setsockopt(socket_fd, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(int));
         return result == 0;
      }

      std::error_code bind_and_listen(int port)
      {
         socket_fd = ::socket(AF_INET, SOCK_STREAM, 0);
         if (socket_fd == -1) {
            return {ip_error::socket_bind_failed, ip_error_category::instance()};
         }

         sockaddr_in server_addr;
         server_addr.sin_family = AF_INET;
         server_addr.sin_addr.s_addr = INADDR_ANY;
         server_addr.sin_port = htons(port);

         if (::bind(socket_fd, (sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
            return {ip_error::socket_bind_failed, ip_error_category::instance()};
         }

         if (::listen(socket_fd, SOMAXCONN) == -1) {
            return {ip_error::socket_bind_failed, ip_error_category::instance()};
         }

         set_non_blocking(socket_fd);
         no_delay();

         return {};
      }

      ssize_t read(std::string& buffer)
      {
         buffer.resize(4096);
         ssize_t bytes = ::recv(socket_fd, buffer.data(), buffer.size(), 0);
         if (bytes == -1) {
            buffer.clear();
         } else {
            buffer.resize(bytes);
         }
         return bytes;
      }

      ssize_t write(const std::string& buffer)
      {
         return ::send(socket_fd, buffer.c_str(), buffer.size(), 0);
      }
   };

   struct destructor final
   {
      std::function<void()> destroy{};

      ~destructor()
      {
         if (destroy) {
            destroy();
         }
      }

      template <class Func>
      destructor(Func&& f) : destroy(std::forward<Func>(f))
      {}
   };

   struct server final
   {
      int port{};
      glz::pool threads{1};

      std::atomic<bool> active = true;

      destructor on_destruct{[this] { active = false; }};

      using AcceptCallback = std::function<void(socket&&)>;

      template <class AcceptCallback>
      std::error_code async_accept(AcceptCallback&& callback)
      {
         std::unique_ptr<glz::socket> accept_socket = std::make_unique<glz::socket>();

         const auto ec = accept_socket->bind_and_listen(port);
         if (ec) {
            return {ip_error::socket_bind_failed, ip_error_category::instance()};
         }
         else {
            std::thread([this, accept_socket = std::move(accept_socket),
                         callback = std::forward<AcceptCallback>(callback)]() {
               while (active) {
                  sockaddr_in client_addr;
                  socklen_t client_len = sizeof(client_addr);
                  // As long as we're not calling accept on the same port we are safe
                  SOCKET client_fd = ::accept(accept_socket->socket_fd, (sockaddr*)&client_addr, &client_len);
                  if (client_fd != -1) {
                     threads.emplace_back([callback = std::move(callback), client_fd] {
                        callback(socket{client_fd});
                     });
                  }
                  else {
                     if (SOCKET_ERROR_CODE != EWOULDBLOCK && SOCKET_ERROR_CODE != EAGAIN) {
                        break;
                     }
                  }
                  std::this_thread::sleep_for(std::chrono::milliseconds(10));
               }
            }).detach();
         }

         return {};
      }
   };
}


/*template <is_callback Callback>
void read(Callback&& callback)
{
   std::string buffer('\0', 1024);
   
   uint64_t size{};
   ssize_t size_bytes{};
   while (size_bytes < 8) {
      const ssize_t bytes = ::recv(int(socket_fd), (char*)(&size) + size_bytes, sizeof(uint64_t) - size_bytes, 0);
      if (bytes > 0) {
         size_bytes += bytes;
         continue;
      }
      else if (bytes == 0) {
         continue; // connection has been closed
      }
      else {
         if (SOCKET_ERROR_CODE == EWOULDBLOCK || SOCKET_ERROR_CODE == EAGAIN) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
         }
         else {
            return; // error
         }
      }
   }
   
   if (size_bytes > 8) {
      return; // error
   }
   
   size_t total_bytes{};
   while (total_bytes < size) {
      const ssize_t bytes = ::recv(int(socket_fd), buffer.data() + total_bytes, buffer.size() - total_bytes, 0);
      if (bytes > 0) {
         total_bytes += bytes;
         continue;
      }
      else if (bytes == 0) {
         continue; // connection has been closed
      }
      else {
         if (SOCKET_ERROR_CODE == EWOULDBLOCK || SOCKET_ERROR_CODE == EAGAIN) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
         }
         else {
            return; // error
         }
      }
   }
   callback(buffer);
}

void write(const std::string& data)
{
   const uint64_t size = data.size();
   
   ssize_t size_bytes{};
   while (size_bytes < 8) {
      const ssize_t bytes = ::send(int(socket_fd), (char*)(&size) + size_bytes, sizeof(uint64_t) - size_bytes, 0);
      if (bytes > 0) {
         size_bytes += bytes;
      }
      else {
         if (SOCKET_ERROR_CODE != EWOULDBLOCK && SOCKET_ERROR_CODE != EAGAIN) {
            break;
         }
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
   }
   
   if (size_bytes > 8) {
      return; // error
   }
   
   size_t total_bytes = 0;
   while (total_bytes < size) {
      const ssize_t bytes =
         ::send(int(socket_fd), data.data() + total_bytes, data.size() - total_bytes, 0);
      if (bytes > 0) {
         total_bytes += bytes;
      }
      else {
         if (SOCKET_ERROR_CODE != EWOULDBLOCK && SOCKET_ERROR_CODE != EAGAIN) {
            break;
         }
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
   }
   
   if (total_bytes != size) {
      // error
   }
}
*/
