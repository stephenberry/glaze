// Glaze Library
// For the license information refer to glaze.hpp

// Modified from the awesome: https://github.com/jbaldwin/libcoro

#pragma once

#include <arpa/inet.h>

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
}
