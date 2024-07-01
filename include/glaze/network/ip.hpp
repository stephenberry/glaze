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

namespace glz
{
   enum struct ip_version : int { ipv4 = AF_INET, ipv6 = AF_INET6 };
   
   inline std::optional<std::string> binary_to_ip_string(const std::string_view binary_address, ip_version ipv = ip_version::ipv6) {
      std::string output{};
      output.resize(16);

      auto success = ::inet_ntop(int(ipv), binary_address.data(), output.data(), uint32_t(output.size()));
      if (success) {
         output.resize(::strnlen(success, output.length()));
         return {output};
      }
      return {};
   }
   
   GLZ_ENUM(connect_status, connected, invalid_ip_address, timeout, error);
   
   GLZ_ENUM(recv_status, ok, closed, udp_not_bound, try_again, //
            would_block, bad_file_descriptor, connection_refused, //
            memory_fault, interrupted, invalid_argument, no_memory, //
            not_connected, not_a_socket);
   
   /*
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
    */
}
