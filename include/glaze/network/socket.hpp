// Glaze Library
// For the license information refer to glaze.hpp

// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/network/core.hpp"
#include "glaze/network/ip.hpp"
#include "glaze/network/socket_core.hpp"

namespace glz
{
#ifdef _WIN32
   constexpr auto e_would_block = WSAEWOULDBLOCK;
   using socket_t = SOCKET;
   constexpr auto socket_error = SOCKET_ERROR;
#else
   constexpr auto e_would_block = EWOULDBLOCK;
   using socket_t = int;
   constexpr auto socket_error = -1;
#endif
}

#include <csignal>
#include <functional>
#include <future>
#include <mutex>
#include <thread>
#include <vector>

namespace glz
{
   struct socket
   {
      socket_t socket_fd{net::invalid_socket};

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

      socket(socket_t fd) : socket_fd(fd) { set_non_blocking(); }

      void close()
      {
         if (socket_fd != net::invalid_socket) {
            net::close_socket(socket_fd);
         }
      }

      ~socket() { close(); }

      [[nodiscard]] bool no_delay()
      {
         int flag = 1;
         int result = setsockopt(socket_fd, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(int));
         return result == 0;
      }
   };
   
   [[nodiscard]] inline std::error_code connect(socket& sckt, const std::string& address, const int port)
   {
      sckt.socket_fd = ::socket(AF_INET, SOCK_STREAM, 0);
      if (sckt.socket_fd == -1) {
         return {int(ip_error::socket_connect_failed), ip_error_category::instance()};
      }

      sockaddr_in server_addr;
      server_addr.sin_family = AF_INET;
      server_addr.sin_port = htons(uint16_t(port));
      inet_pton(AF_INET, address.c_str(), &server_addr.sin_addr);

      if (::connect(sckt.socket_fd, (sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
         return {int(ip_error::socket_connect_failed), ip_error_category::instance()};
      }

      sckt.set_non_blocking();

      return {};
   }
   
   [[nodiscard]] inline std::error_code bind_and_listen(socket& sckt, int port)
   {
      sckt.socket_fd = ::socket(AF_INET, SOCK_STREAM, 0);
      if (sckt.socket_fd == net::invalid_socket) {
         return {int(ip_error::socket_bind_failed), ip_error_category::instance()};
      }

      sockaddr_in server_addr;
      server_addr.sin_family = AF_INET;
      server_addr.sin_addr.s_addr = INADDR_ANY;
      server_addr.sin_port = htons(uint16_t(port));

      if (::bind(sckt.socket_fd, (sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
         return {int(ip_error::socket_bind_failed), ip_error_category::instance()};
      }

      if (::listen(sckt.socket_fd, SOMAXCONN) == -1) {
         return {int(ip_error::socket_bind_failed), ip_error_category::instance()};
      }

      sckt.set_non_blocking();
      if (not sckt.no_delay()) {
         return {int(ip_error::socket_bind_failed), ip_error_category::instance()};
      }

      return {};
   }
   
   GLZ_ENUM(socket_event, bytes_read, wait, client_disconnected, receive_failed);
   
   struct socket_state
   {
      size_t bytes_read{};
      socket_event event{};
   };
   
   [[nodiscard]] inline socket_state async_recv(socket& sckt, char* buffer, size_t size)
   {
      auto bytes = ::recv(sckt.socket_fd, buffer, net::ssize_t(size), 0);
      if (bytes == -1) {
         if (GLZ_SOCKET_ERROR_CODE == e_would_block || GLZ_SOCKET_ERROR_CODE == EAGAIN) {
            return {0, socket_event::wait};
         }
         else {
            return {0, socket_event::receive_failed};
         }
      }
      else if (bytes == 0) {
         return {0, socket_event::client_disconnected};
      }
      return {size_t(bytes), socket_event::bytes_read};
   }
   
   template <ip_header Header>
   [[nodiscard]] std::error_code blocking_header_receive(socket& sckt, Header& header, std::string& buffer)
   {
      // first receive the header
      size_t total_bytes{};
      while (total_bytes < sizeof(Header)) {
         auto[bytes, event] = async_recv(sckt, reinterpret_cast<char*>(&header) + total_bytes, sizeof(Header) - total_bytes);
         using enum socket_event;
         switch (event)
         {
            case bytes_read: {
               total_bytes += bytes;
               break;
            }
            case wait: {
               std::this_thread::sleep_for(std::chrono::milliseconds(1));
               continue;
            }
            case client_disconnected: {
               return {int(ip_error::client_disconnected), ip_error_category::instance()};
            }
            case receive_failed: {
               [[fallthrough]];
            }
            default: {
               buffer.clear();
               return {int(ip_error::receive_failed), ip_error_category::instance()};
            }
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

      total_bytes = 0;
      while (total_bytes < size) {
         auto[bytes, event] = async_recv(sckt, buffer.data() + total_bytes, buffer.size() - total_bytes);
         using enum socket_event;
         switch (event)
         {
            case bytes_read: {
               total_bytes += bytes;
               break;
            }
            case wait: {
               std::this_thread::sleep_for(std::chrono::milliseconds(1));
               continue;
            }
            case client_disconnected: {
               return {int(ip_error::client_disconnected), ip_error_category::instance()};
            }
            case receive_failed: {
               [[fallthrough]];
            }
            default: {
               buffer.clear();
               return {int(ip_error::receive_failed), ip_error_category::instance()};
            }
         }
      }
      return {};
   }
   
   [[nodiscard]] inline std::error_code blocking_send(socket& sckt, const std::string_view buffer)
   {
      const size_t size = buffer.size();
      size_t total_bytes{};
      while (total_bytes < size) {
         auto bytes = ::send(sckt.socket_fd, buffer.data() + total_bytes, glz::net::ssize_t(buffer.size() - total_bytes), 0);
         if (bytes == -1) {
            if (GLZ_SOCKET_ERROR_CODE == e_would_block || GLZ_SOCKET_ERROR_CODE == EAGAIN) {
               std::this_thread::yield();
               continue;
            }
            else {
               return {int(ip_error::send_failed), ip_error_category::instance()};
            }
         }

         total_bytes += bytes;
      }
      return {};
   }
}
