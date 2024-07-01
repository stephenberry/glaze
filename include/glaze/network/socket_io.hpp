// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/glaze.hpp"
#include "glaze/network/socket.hpp"
#include "glaze/core/error.hpp"

namespace glz
{
   template <opts Opts = opts{.format = binary}, class Buffer>
   [[nodiscard]] std::error_code send(socket& sckt, Buffer& buffer)
   {
      uint64_t header = uint64_t(buffer.size());
      
      if (auto ec = raw_send(sckt, sv{reinterpret_cast<char*>(&header), sizeof(header)})) {
         return ec;
      }

      return raw_send(sckt, buffer);
   }

   template <opts Opts = opts{.format = binary}, class T>
   [[nodiscard]] std::error_code send_value(socket& sckt, T&& value)
   {
      static thread_local std::string buffer{};

      if (auto ec = glz::write<Opts>(std::forward<T>(value), buffer)) {
         return {int(ec.ec), error_category::instance()};
      }

      uint64_t header = uint64_t(buffer.size());

      if (auto ec = raw_send(sckt, sv{reinterpret_cast<char*>(&header), sizeof(header)})) {
         return ec;
      }

      return raw_send(sckt, buffer);
   }

      template <opts Opts = opts{.format = binary}, class Buffer>
   [[nodiscard]] std::error_code receive(socket& sckt, Buffer& buffer)
   {
      uint64_t header{};
      if (auto ec = receive(sckt, header, buffer)) {
         return ec;
      }
      return {};
   }

   template <opts Opts = opts{.format = binary}, class T>
   [[nodiscard]] std::error_code receive_value(socket& sckt, T&& value)
   {
      static thread_local std::string buffer{};

      uint64_t header{};
      if (auto ec = raw_receive(sckt, header, buffer)) {
         return ec;
      }

      if (auto ec = glz::read<Opts>(std::forward<T>(value), buffer)) {
         return {int(ec.ec), error_category::instance()};
      }

      return {};
   }
}
