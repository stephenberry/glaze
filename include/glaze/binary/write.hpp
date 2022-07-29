// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/format.hpp"
#include "glaze/util/dump.hpp"
#include "glaze/binary/header.hpp"

namespace glaze
{
   namespace detail
   {      
      template <class T = void>
      struct to_binary {};
      
      template <>
      struct write<binary>
      {
         template <class T, class B>
         static void op(T&& value, B&& b) {
            to_binary<std::decay_t<T>>::op(std::forward<T>(value), std::forward<B>(b));
         }
      };
      
      template <class T>
      requires (std::same_as<T, bool> || std::same_as<T, std::vector<bool>::reference> || std::same_as<T, std::vector<bool>::const_reference>)
      struct to_binary<T>
      {
         static void op(const bool value, auto&& b) noexcept
         {
            if (value) {
               dump<static_cast<std::byte>(1)>(b);
            }
            else {
               dump<static_cast<std::byte>(0)>(b);
            }
         }
      };
      
      void dump_type(auto&& value, auto&& b) noexcept
      {
         dump(std::as_bytes(std::span{ &value, 1 }), b);
      }
      
      template <class T>
      requires num_t<T> || char_t<T>
      struct to_binary<T>
      {
         static void op(auto&& value, auto&& b) noexcept
         {
            dump_type(value, b);
         }
      };
      
      template <str_t T>
      struct to_binary<T>
      {
         static void op(auto&& value, auto&& b) noexcept
         {
            dump(std::as_bytes(std::span{ value.data(), value.size() }), b);
         }
      };
      
      template <array_t T>
      struct to_binary<T>
      {
         static void op(auto&& value, auto&& b) noexcept
         {
            if (!value.empty()) {
               const auto n = value.size();
               if (n < 64) {
                  dump_type(header8{ 0, static_cast<uint8_t>(n) }, b);
               }
               else if (n < 16384) {
                  dump_type(header16{ 1, static_cast<uint16_t>(n) }, b);
               }
               else if (n < 1073741824) {
                  dump_type(header32{ 2, static_cast<uint32_t>(n) }, b);
               }
               else if (n < 4611686018427387904) {
                  dump_type(header64{ 3, n }, b);
               }
               for (auto&& x : value) {
                  write<binary>::op(x, b);
               }
            }
         }
      };
   }
   
   template <class T, class Buffer>
   inline void write_binary(T&& value, Buffer&& buffer) {
      write<binary>(std::forward<T>(value), std::forward<Buffer>(buffer));
   }
}
