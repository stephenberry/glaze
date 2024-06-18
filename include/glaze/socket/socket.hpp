// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#ifdef _WIN32
#define NOMINMAX
#include <cstdint>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#define CLOSESOCKET closesocket
#define SOCKET_ERROR_CODE WSAGetLastError()
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
#define CLOSESOCKET close
#define SOCKET_ERROR_CODE errno
#define SOCKET int
#define SOCKET_ERROR (-1)
#endif

#include <csignal>
#include <format>
#include <functional>
#include <iostream>
#include <mutex>
#include <system_error>
#include <thread>
#include <vector>
#include <future>

#include "glaze/rpc/repe.hpp"

namespace glz
{
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

      if (what.empty())
         return {std::error_code(err, socket_api_error_category(what))};
      else
         return {std::error_code(err, socket_api_error_category(what))};
   }

   inline std::error_code check_status(int ec = -1 /*assume error*/, const std::string_view what = "")
   {
      if (ec >= 0) {
         return {std::error_code{}};
      }

      return {get_socket_error(what)};
   }

   // Example:
   // 
   // std::error_code ec = check_status(result, "connect failed");
   // 
   // if (ec) {
   //    std::cerr << get_socket_error(std::format("Failed to connect to socket at address: {}.\nIs the server running?", ip_port)).message();
   // }
   // else {
   //    std::cout << "Connected successfully!";
   // }

   namespace ip
   {
      struct opts
      {};
   }

   inline void windows_startup()
   {
      static std::once_flag flag{};
      std::call_once(flag, [] {
#ifdef _WIN32
         WSADATA wsaData;
         WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
      });
   }
   
   inline void windows_cleanup() {
#ifdef _WIN32
         WSACleanup();
#endif
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

   struct socket
   {
      SOCKET socket_fd{INVALID_SOCKET};

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

      socket(SOCKET fd) : socket_fd(fd)
      {
         //set_non_blocking();
      }

      ~socket()
      {
         if (socket_fd != -1) {
            write_value("disconnect");
            CLOSESOCKET(socket_fd);
         }
      }

      std::error_code connect(const std::string& address, int port)
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

         //set_non_blocking();

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
         server_addr.sin_port = htons(uint16_t(port));

         if (::bind(socket_fd, (sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
            return {ip_error::socket_bind_failed, ip_error_category::instance()};
         }

         if (::listen(socket_fd, SOMAXCONN) == -1) {
            return {ip_error::socket_bind_failed, ip_error_category::instance()};
         }

         //set_non_blocking();
         no_delay();

         return {};
      }

      ssize_t read(std::string& buffer)
      {
         buffer.resize(4096); // allocate enough bytes for reading size from header
         uint64_t size = (std::numeric_limits<uint64_t>::max)();
         size_t total_bytes{};
         bool size_obtained = false;
         while (total_bytes < size) {
            ssize_t bytes = ::recv(socket_fd, buffer.data() + total_bytes, uint16_t(buffer.size() - total_bytes), 0);
            if (bytes == -1) {
               buffer.clear();
               return bytes;
            } else {
               total_bytes += bytes;
            }
            
            if (total_bytes > 17 && not size_obtained) {
               std::tuple<std::tuple<uint8_t, uint8_t, int64_t>> header{};
               const auto ec = glz::read<glz::opts{.format = glz::binary, .partial_read = true}>(header, buffer);
               if (ec) {
                  // error
               }
               size = std::get<2>(std::get<0>(header)); // reading size from header
               buffer.resize(size);
               size_obtained = true;
            }
         }
         return buffer.size();
      }

      ssize_t write(const std::string& buffer)
      {
         const size_t size = buffer.size();
         size_t total_bytes{};
         while (total_bytes < size) {
            ssize_t bytes = ::send(socket_fd, buffer.data() + total_bytes, uint16_t(buffer.size() - total_bytes), 0);
            if (bytes == -1) {
               return bytes;
            }
            else {
               total_bytes += bytes;
            }
         }
         return buffer.size();
      }
      
      template <class T>
      ssize_t read_value(T&& value)
      {
         static thread_local std::string buffer{}; // TODO: use a buffer pool
         
         read(buffer);
        
         const auto ec = glz::read_binary(std::forward_as_tuple(repe::header{}, std::forward<T>(value)), buffer);
         if (ec) {
            // error
         }
         
         return buffer.size();
      }
      
      template <class T>
      ssize_t write_value(T&& value)
      {
         static thread_local std::string buffer{}; // TODO: use a buffer pool
        
         const auto ec = glz::write_binary(std::forward_as_tuple(repe::header{}, std::forward<T>(value)), buffer);
         if (ec) {
            // error
         }
         
         // write into location of size
         const uint64_t size = buffer.size();
         std::memcpy(buffer.data() + 9, &size, sizeof(uint64_t));
         
         write(buffer);
         
         return buffer.size();
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
   
   inline static std::atomic<bool> active = true;

   struct server final
   {
      int port{};
      std::vector<std::future<void>> threads{}; // TODO: Remove dead clients

      destructor on_destruct{[] {
         active = false;
      }};

      template <class AcceptCallback>
      std::error_code accept(AcceptCallback&& callback)
      {
         glz::socket accept_socket{};

         const auto ec = accept_socket.bind_and_listen(port);
         if (ec) {
            return {ip_error::socket_bind_failed, ip_error_category::instance()};
         }
         
         while (active) {
            sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            // As long as we're not calling accept on the same port we are safe
            auto client_fd = ::accept(accept_socket.socket_fd, (sockaddr*)&client_addr, &client_len);
            if (client_fd != -1) {
               threads.emplace_back(std::async([callback, client_fd] {
                  callback(socket{client_fd});
               }));
            }
            else {
               if (SOCKET_ERROR_CODE == EWOULDBLOCK || SOCKET_ERROR_CODE == EAGAIN) {
                  // keep polling
               }
               else {
                  break; // error
               }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            threads.erase(std::partition(threads.begin(), threads.end(), [](auto& future) {
               if (auto status = future.wait_for(std::chrono::milliseconds(0)); status == std::future_status::ready) {
                  return false;
               }
               return true;
            }), threads.end());
         }

         return {};
      }
   };
}
