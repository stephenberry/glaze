// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/format.hpp"
#include "glaze/util/dump.hpp"
#include "glaze/binary/header.hpp"
#include "glaze/core/read.hpp"

namespace glaze
{
   namespace detail
   {
      template <class T = void>
      struct from_binary {};
      
      template <>
      struct read<binary>
      {
         template <class T, class It0, class It1>
         static void op(T&& value, It0&& it, It1&& end) {
            from_binary<std::decay_t<T>>::op(std::forward<T>(value), std::forward<It0>(it), std::forward<It1>(end));
         }
      };
      
      template <class T>
      requires (num_t<T> || std::same_as<T, bool> || std::same_as<T, std::vector<bool>::reference> || std::same_as<T, std::vector<bool>::const_reference>)
      struct from_binary<T>
      {
         static void op(auto&& value, auto&& it, auto&& end)
         {
            using V = std::decay_t<T>;
            if (it != end) {
               std::memcpy(&value, &(*it), sizeof(V));
            }
            else {
               throw std::runtime_error("Missing binary data");
            }
            ++it;
         }
      };
      
      // TODO: Handle errors
      inline constexpr size_t int_from_header(auto&& it, auto&& end)
      {
         header8 h8;
         std::memcpy(&h8, &(*it), 1);
         switch (h8.config) {
            case 0:
               ++it;
               return h8.size;
               break;
            case 1: {
               header16 h;
               std::memcpy(&h, &(*it), 2);
               std::advance(it, 2);
               return h.size;
               break;
            }
            case 2: {
               header32 h;
               std::memcpy(&h, &(*it), 4);
               std::advance(it, 4);
               return h.size;
               break;
            }
            case 3: {
               header64 h;
               std::memcpy(&h, &(*it), 8);
               std::advance(it, 8);
               return h.size;
               break;
            }
            default:
               return 0;
         }
      }
      
      template <size_t N>
      inline constexpr size_t int_from_reduced(auto&& it, auto&& end)
      {
         if constexpr (N < 256) {
            uint8_t i;
            std::memcpy(&i, &(*it), 1);
            ++it;
            return i;
         }
         else if constexpr (N < 65536) {
            uint16_t i;
            std::memcpy(&i, &(*it), 2);
            ++it;
            return i;
         }
         else if constexpr (N < 4294967296) {
            uint32_t i;
            std::memcpy(&i, &(*it), 4);
            ++it;
            return i;
         }
         else {
            uint64_t i;
            std::memcpy(&i, &(*it), 8);
            ++it;
            return i;
         }
      }
      
      template <str_t T>
      struct from_binary<T>
      {
         static void op(auto&& value, auto&& it, auto&& end)
         {
            const auto n = int_from_header(it, end);
            using V = typename std::decay_t<T>::value_type;
            const auto n_bytes = sizeof(V) * n;
            value.resize(value.size() + n);
            std::memcpy(value.data(), &(*it), n_bytes);
         }
      };
      
      template <array_t T>
      struct from_binary<T>
      {
         static void op(auto&& value, auto&& it, auto&& end)
         {
            const auto n = int_from_header(it, end);
            using V = typename std::decay_t<T>::value_type;
            const auto n_bytes = sizeof(V) * n;
            
            if constexpr (resizeable<T>) {
               value.resize(value.size() + n);
               std::memcpy(value.data(), &(*it), n_bytes);
            }
            else {
               std::memcpy(value.data(), &(*it), n_bytes);
            }
            std::advance(it, n_bytes);
         }
      };
      
      template <class T>
      requires glaze_object_t<std::decay_t<T>>
      struct from_binary<T>
      {
         static void op(auto&& value, auto&& it, auto&& end)
         {
            using V = std::decay_t<T>;
            static constexpr auto N = std::tuple_size_v<meta_t<V>>;
            const auto n_keys = int_from_reduced<N>(it, end);
            for (size_t i = 0; i < n_keys; ++i) {
               const auto key = int_from_header(it, end);
               static constexpr auto frozen_map = detail::make_int_map<T>();
               const auto& member_it = frozen_map.find(key);
               if (member_it != frozen_map.end()) {
                  std::visit(
                     [&](auto&& member_ptr) {
                        read<binary>::op(value.*member_ptr, it, end);
                     },
                     member_it->second);
               }
            }
         }
      };
   }
   
   template <class T, class Buffer>
   inline void read_binary(T&& value, Buffer&& buffer) {
      read<binary>(std::forward<T>(value), std::forward<Buffer>(buffer));
   }
}
