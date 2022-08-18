// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/format.hpp"
#include "glaze/util/dump.hpp"
#include "glaze/binary/header.hpp"
#include "glaze/util/for_each.hpp"

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

      template <func_t T>
      struct to_binary<T>
      {
         static void op(auto&& value, auto&& b) noexcept {}
      };
      
      void dump_type(auto&& value, auto&& b) noexcept
      {
         dump(std::as_bytes(std::span{ &value, 1 }), b);
      }
      
      void dump_int(size_t i, auto&& b)
      {
         if (i < 64) {
            dump_type(header8{ 0, static_cast<uint8_t>(i) }, b);
         }
         else if (i < 16384) {
            dump_type(header16{ 1, static_cast<uint16_t>(i) }, b);
         }
         else if (i < 1073741824) {
            dump_type(header32{ 2, static_cast<uint32_t>(i) }, b);
         }
         else if (i < 4611686018427387904) {
            dump_type(header64{ 3, i }, b);
         }
         else {
            throw std::runtime_error("size not supported");
         }
      }
      
      void dump_reduce(size_t i, auto&& b)
      {
         if (i < 256) {
            dump_type(static_cast<uint8_t>(i), b);
         }
         else if (i < 65536) {
            dump_type(static_cast<uint16_t>(i), b);
         }
         else if (i < 4294967296) {
            dump_type(static_cast<uint32_t>(i), b);
         }
         else {
            dump_type(i, b);
         }
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
            dump_int(value.size(), b);
            dump(std::as_bytes(std::span{ value.data(), value.size() }), b);
         }
      };
      
      template <array_t T>
      struct to_binary<T>
      {
         static void op(auto&& value, auto&& b)
         {
            if constexpr (resizeable<T>) {
               dump_int(value.size(), b);
            }
            for (auto&& x : value) {
               write<binary>::op(x, b);
            }
         }
      };
      
      template <map_t T>
      struct to_binary<T>
      {
         static void op(auto&& value, auto&& b) noexcept
         {
            dump_int(value.size(), b);
            for (auto&& [k, v] : value) {
               write<binary>::op(k, b);
               write<binary>::op(v, b);
            }
         }
      };
      
      template <nullable_t T>
      struct to_binary<T>
      {
         static void op(auto&& value, auto&& b) noexcept
         {
            if (value) {
               dump<static_cast<std::byte>(1)>(b);
               write<binary>::op(*value, b);
            }
            else {
               dump<static_cast<std::byte>(0)>(b);
            }
         }
      };
      
      template <class T>
      requires glaze_object_t<T>
      struct to_binary<T>
      {
         static void op(auto&& value, auto&& b) noexcept
         {
            using V = std::decay_t<T>;
            static constexpr auto N = std::tuple_size_v<meta_t<V>>;
            dump_reduce(N, b); // do not compress, N is known at compile time
            
            for_each<N>([&](auto I) {
               static constexpr auto item = std::get<I>(meta_v<V>);
               dump_int(I, b); // dump the known key as an integer
               write<binary>::op(value.*std::get<1>(item), b);
            });
         }
      };
   }
   
   template <class T, class Buffer>
   inline void write_binary(T&& value, Buffer&& buffer) {
      write<binary>(std::forward<T>(value), std::forward<Buffer>(buffer));
   }
}
