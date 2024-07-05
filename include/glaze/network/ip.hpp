// Glaze Library
// For the license information refer to glaze.hpp

// Modified from the awesome: https://github.com/jbaldwin/libcoro

#pragma once

#if defined(__linux__) || defined(__APPLE__)
#include <arpa/inet.h>
#elif defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include <cstring>
#include <optional>
#include <string>

#include "glaze/reflection/enum_macro.hpp"

namespace glz {
   enum struct ip_version : int { ipv4 = AF_INET, ipv6 = AF_INET6 };

   GLZ_ENUM(ip_status, 
            unset, 
            ok, 
            closed, 
            connected, 
            connection_refused, 
            invalid_ip_address, 
            timeout, 
            error, 
            try_again, 
            would_block, 
            bad_file_descriptor, 
            invalid_socket
   );

   inline ip_status errno_to_ip_status() noexcept {
#if defined(__linux__) || defined(__APPLE__)
      const auto err = errno;
      using enum ip_status;
      switch (err) {
      case 0: 
         return ok;
      case -1: 
         return closed;
      case EWOULDBLOCK: 
         return would_block;
      case ECONNREFUSED: 
         return connection_refused;
      default: 
         return error;
      }
#endif
   }

   // Get's human readable ip if binary; returns ip if full socket address (ip::port); handles IPV4 and 
   // IPV6 formats.
   //
   inline std::optional<std::string> to_ip_string(std::string_view input, ip_version ipv = ip_version::ipv6) {
      if (input.empty()) return std::nullopt;

      // Remove trailing whitespace...here because input contained a '\n'...
      while (!input.empty() && std::isspace(static_cast<unsigned char>(input.back()))) {
         input.remove_suffix(1);
      }

      size_t buffer_size = (ipv == ip_version::ipv4) ? INET_ADDRSTRLEN : INET6_ADDRSTRLEN;
      std::string output(buffer_size, '\0');

      // Handle human-readable IP addresses (with or without port)
      std::string_view ip_part = input;
      if (input.find_first_not_of("0123456789.:abcdefABCDEF[]") == std::string_view::npos) {
         // Remove port if present
         size_t colon_pos = input.rfind(':');
         size_t bracket_pos = input.rfind(']');
         if (colon_pos != std::string_view::npos &&
             (bracket_pos == std::string_view::npos || colon_pos > bracket_pos)) {
            ip_part = input.substr(0, colon_pos);
         }

         // Remove brackets for IPv6 addresses
         if (ip_part.front() == '[' && ip_part.back() == ']') {
            ip_part = ip_part.substr(1, ip_part.size() - 2);
         }

         // Try to convert the string representation
         int family = (ip_part.find(':') != std::string_view::npos) ? AF_INET6 : AF_INET;
         unsigned char buf[sizeof(struct in6_addr)];
         if (inet_pton(family, std::string(ip_part).c_str(), buf) == 1) {
            if (inet_ntop(family, buf, output.data(), output.size())) {
               output.resize(std::strlen(output.c_str()));
               return output;
            }
         }
      }

      // Handle binary IP addresses here
      const void* addr_ptr;
      int family;
      if (input.size() == sizeof(struct in_addr)) {
         addr_ptr = input.data();
         family = AF_INET;
      } else if (input.size() == sizeof(struct in6_addr)) {
         addr_ptr = input.data();
         family = AF_INET6;
      } else {
         return std::nullopt; // Invalid input size for binary IP
      }

      if (inet_ntop(family, addr_ptr, output.data(), output.size())) {
         output.resize(std::strlen(output.c_str()));
         return output;
      } else {
         // TODO: Implement Error handling for cross-platform portability.
         int error_code = errno;
         switch (error_code) {
         case EAFNOSUPPORT:
            std::cerr << "Address family not supported for socket address: " << input << '\n';
            break;
         case ENOSPC:
            std::cerr << "Insufficient space in output buffer for socket address: " << input << '\n';
            break;
         default:
            std::cerr << "Unexpected 'inet_ntop' error converting socket address: " << input << '\n';
         }
      }
      return std::nullopt;
   }

   /*
    // ip_status
    ok = 0,
    /// The peer closed the socket.
    closed = -1,
    /// The udp socket has not been bind()'ed to a local port.
    udp_not_bound = -2,
    try_again     = EAGAIN,
    // Note: that only the tcp::client will return this, a tls::client returns the specific ssl_would_block_* status.
    would_block         = EWOULDBLOCK,
    bad_file_descriptor = EBADF,
    connection_refused  = ECONNREFUSED,
    memory_fault        = EFAULT,
    interrupted         = EINTR,
    invalid_argument    = EINVAL,
    no_memory           = ENOMEM,
    not_connected       = ENOTCONN,
    not_a_socket        = ENOTSOCK,
    */

   /*
    // ip_status
    ok                       = 0,
    closed                   = -1,
    permission_denied        = EACCES,
    try_again                = EAGAIN,
    would_block              = EWOULDBLOCK,
    already_in_progress      = EALREADY,
    bad_file_descriptor      = EBADF,
    connection_reset         = ECONNRESET,
    no_peer_address          = EDESTADDRREQ,
    memory_fault             = EFAULT,
    interrupted              = EINTR,
    is_connection            = EISCONN,
    message_size             = EMSGSIZE,
    output_queue_full        = ENOBUFS,
    no_memory                = ENOMEM,
    not_connected            = ENOTCONN,
    not_a_socket             = ENOTSOCK,
    operation_not_supported  = EOPNOTSUPP,
    pipe_closed              = EPIPE,
    */
}
