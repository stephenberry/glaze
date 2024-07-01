// Glaze Library
// For the license information refer to glaze.hpp

// Modified from the awesome: https://github.com/jbaldwin/libcoro

#pragma once

#if defined(__linux__) || defined(__APPLE__)
#include <arpa/inet.h>
#endif

#include <cstring>
#include <optional>
#include <string>

namespace glz
{
   enum struct ip_version : int { ipv4 = AF_INET, ipv6 = AF_INET6 };
   
   std::optional<std::string> binary_to_ip_string(const std::string_view binary_address, ip_version ipv = ip_version::ipv6) {
      std::string output{};
      output.resize(16);

      auto success = ::inet_ntop(int(ipv), binary_address.data(), output.data(), uint32_t(output.size()));
      if (success) {
         output.resize(::strnlen(success, output.length()));
         return {output};
      }
      return {};
   }
   
   enum struct connect_status
   {
       connected,
       invalid_ip_address,
       timeout,
       error
   };
   
   inline std::string to_string(const connect_status& status) noexcept
   {
      using enum connect_status;
       switch (status)
       {
           case connected:
               return "connected";
           case invalid_ip_address:
               return "invalid_ip_address";
           case timeout:
               return "timeout";
           case error:
               return "error";
          default:
             return "unknown";
       }
   }
   
   enum struct recv_status : int64_t
   {
       ok = 0,
       /// The peer closed the socket.
       closed = -1,
       /// The udp socket has not been bind()'ed to a local port.
       udp_not_bound = -2,
       try_again     = EAGAIN,
       // Note: that only the tcp::client will return this, a tls::client returns the specific ssl_would_block_* status'.
       would_block         = EWOULDBLOCK,
       bad_file_descriptor = EBADF,
       connection_refused  = ECONNREFUSED,
       memory_fault        = EFAULT,
       interrupted         = EINTR,
       invalid_argument    = EINVAL,
       no_memory           = ENOMEM,
       not_connected       = ENOTCONN,
       not_a_socket        = ENOTSOCK,
   };
   
   std::string to_string(recv_status status)
   {
      using enum recv_status;
       switch (status)
       {
           case ok:
               return "ok";
           case closed:
               return "closed";
           case udp_not_bound:
               return "udp_not_bound";
           //case try_again:
           //  return "try_again";
           case would_block:
               return "would_block";
           case bad_file_descriptor:
               return "bad_file_descriptor";
           case connection_refused:
               return "connection_refused";
           case memory_fault:
               return "memory_fault";
           case interrupted:
               return "interrupted";
           case invalid_argument:
               return "invalid_argument";
           case no_memory:
               return "no_memory";
           case not_connected:
               return "not_connected";
           case not_a_socket:
               return "not_a_socket";
          default:
             return "unknown";
       }
   }
}
