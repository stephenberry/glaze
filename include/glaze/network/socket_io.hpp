// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/glaze.hpp"
#include "glaze/network/socket.hpp"

namespace glz
{
   template <opts Opts = opts{.format = binary}, class Buffer>
   [[nodiscard]] std::error_code receive(socket& sckt, Buffer& buffer)
   {
      uint64_t header{};
      if (auto ec = sckt.receive(header, buffer)) {
         return ec;
      }
      return {};
   }

   template <opts Opts = opts{.format = binary}, class Buffer>
   [[nodiscard]] std::error_code send(socket& sckt, Buffer& buffer)
   {
      uint64_t header = uint64_t(buffer.size());

      if (auto ec = sckt.send(sv{reinterpret_cast<char*>(&header), sizeof(header)})) {
         return ec;
      }

      if (auto ec = sckt.send(buffer)) {
         return ec;
      }

      return {};
   }

   template <opts Opts = opts{.format = binary}, class T>
   [[nodiscard]] std::error_code receive_value(socket& sckt, T&& value)
   {
      static thread_local std::string buffer{};

      uint64_t header{};
      if (auto ec = sckt.receive(header, buffer)) {
         return ec;
      }

      if (auto ec = glz::read<Opts>(std::forward<T>(value), buffer)) {
         std::fprintf(stderr, "%s\n", glz::format_error(ec, buffer).c_str());
         return {ip_error::receive_failed, ip_error_category::instance()};
      }

      return {};
   }

   template <opts Opts = opts{.format = binary}, class T>
   [[nodiscard]] std::error_code send_value(socket& sckt, T&& value)
   {
      static thread_local std::string buffer{};

      if (auto ec = glz::write<Opts>(std::forward<T>(value), buffer)) {
         std::fprintf(stderr, "%s\n", glz::format_error(ec, buffer).c_str());
         return {ip_error::send_failed, ip_error_category::instance()};
      }

      uint64_t header = uint64_t(buffer.size());

      if (auto ec = sckt.send(sv{reinterpret_cast<char*>(&header), sizeof(header)})) {
         return ec;
      }

      if (auto ec = sckt.send(buffer)) {
         return ec;
      }

      return {};
   }
}
