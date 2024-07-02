// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/glaze.hpp"
#include "glaze/network/socket.hpp"
#include "glaze/core/error.hpp"

namespace glz
{
   template <opts Opts = opts{.format = binary}, class T, class Buffer>
   [[nodiscard]] std::error_code send(socket& sckt, T&& value, Buffer&& buffer)
   {
      if (auto ec = glz::write<Opts>(std::forward<T>(value), buffer)) {
         return {int(ec.ec), error_category::instance()};
      }

      uint64_t header = uint64_t(buffer.size());

      if (auto ec = blocking_send(sckt, sv{reinterpret_cast<char*>(&header), sizeof(header)})) {
         return ec;
      }

      return blocking_send(sckt, buffer);
   }

   template <opts Opts = opts{.format = binary}, class T, class Buffer>
   [[nodiscard]] std::error_code receive(socket& sckt, T&& value, Buffer&& buffer, size_t timeout_ms)
   {
      uint64_t header{};
      if (auto ec = blocking_header_receive(sckt, header, buffer, timeout_ms)) {
         return ec;
      }

      if (auto ec = glz::read<Opts>(std::forward<T>(value), buffer)) {
         return {int(ec.ec), error_category::instance()};
      }

      return {};
   }
}
