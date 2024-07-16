// Glaze Library
// For the license information refer to glaze.hpp

// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/network/core.hpp"
#include "glaze/network/ip.hpp"

#ifdef _WIN32
#pragma comment(lib, "Ws2_32.lib")
#define GLZ_SOCKET_ERROR_CODE WSAGetLastError()
#else
#define GLZ_SOCKET_ERROR_CODE errno
#include <sys/socket.h>
#include <sys/types.h>
#if __has_include(<netinet/in.h>)
#include <netinet/in.h>
#endif
#include <fcntl.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <cerrno>
#endif

#include <format>
#include <system_error>

namespace glz
{
   namespace detail
   {
      inline std::string format_ip_port(const sockaddr_in& server_addr)
      {
         char ip_str[INET_ADDRSTRLEN]{};

   #ifdef _WIN32
         inet_ntop(AF_INET, &(server_addr.sin_addr), ip_str, INET_ADDRSTRLEN);
   #else
         inet_ntop(AF_INET, &(server_addr.sin_addr), ip_str, sizeof(ip_str));
   #endif

         return {std::format("{}:{}", ip_str, ntohs(server_addr.sin_port))};
      }
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

   struct socket_api_error_category_t final : std::error_category
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

      return {err, socket_api_error_category(what)};
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
      return std::format("{}.{}", int(major), int(minor));
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
   
   GLZ_ENUM(ip_error, none,
      queue_create_failed,
      event_ctl_failed,
      event_wait_failed,
      event_enum_failed,
      socket_connect_failed,
      socket_bind_failed,
      send_failed,
      receive_failed,
      receive_timeout,
      client_disconnected);

   template <class T>
   concept ip_header = std::same_as<T, uint64_t> || requires { T::body_size; };

   struct ip_error_category : std::error_category
   {
      static const ip_error_category& instance() {
         static ip_error_category instance{};
         return instance;
      }

      const char* name() const noexcept override { return "ip_error_category"; }

      std::string message(int ec) const override
      {
         return std::string{nameof(static_cast<ip_error>(ec))};
      }
   };
}
