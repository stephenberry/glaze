// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/format.hpp"
#include "glaze/util/dump.hpp"

namespace glaze
{
   namespace detail
   {
      struct header8 {
         uint8_t config : 2;
         uint8_t size : 6;
      };
      static_assert(sizeof(header8) == 1);
      
      struct header16 {
         uint8_t config : 2;
         uint16_t size : 14;
      };
      static_assert(sizeof(header16) == 2);
      
      struct header32 {
         uint8_t config : 2;
         uint32_t size : 30;
      };
      static_assert(sizeof(header32) == 4);
      
      struct header64 {
         uint8_t config : 2;
         uint64_t size : 62;
      };
      static_assert(sizeof(header64) == 8);
      
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
                  static constexpr auto h8 = header8{};
                  dump_type(&h8, b);
               }
               else if (n < 16384) {
                  static constexpr auto h16 = header16{};
                  dump_type(&h16, b);
               }
               else if (n < 1073741824) {
                  static constexpr auto h32 = header32{};
                  dump_type(&h32, b);
               }
               else if (n < 4611686018427387904) {
                  static constexpr auto h64 = header64{};
                  dump_type(&h64, b);
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
