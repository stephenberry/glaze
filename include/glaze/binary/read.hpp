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
      inline constexpr size_t size_from_header(auto&& it, auto&& end)
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
      
      template <str_t T>
      struct from_binary<T>
      {
         static void op(auto&& value, auto&& it, auto&& end)
         {
            const auto n = size_from_header(it, end);
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
            const auto n = size_from_header(it, end);
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
   }
   
   template <class T, class Buffer>
   inline void read_binary(T&& value, Buffer&& buffer) {
      read<binary>(std::forward<T>(value), std::forward<Buffer>(buffer));
   }
}
