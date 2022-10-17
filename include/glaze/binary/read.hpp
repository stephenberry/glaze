// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/format.hpp"
#include "glaze/util/dump.hpp"
#include "glaze/binary/header.hpp"
#include "glaze/core/read.hpp"

namespace glz
{
   namespace detail
   {
      template <class T = void>
      struct from_binary {};
      
      template <>
      struct read<binary>
      {
         template <auto Opts, class T, class It0, class It1>
         static void op(T&& value, It0&& it, It1&& end) {
            from_binary<std::decay_t<T>>::template op<Opts>(std::forward<T>(value), std::forward<It0>(it), std::forward<It1>(end));
         }
      };
      
      template <class T>
      requires(num_t<T> || char_t<T> || glaze_enum_t<T>)
      struct from_binary<T>
      {
         template <auto Opts>
         static void op(auto&& value, auto&& it, auto&& end)
         {
            using V = std::decay_t<T>;
            if (it != end) {
               std::memcpy(&value, &(*it), sizeof(V));
            }
            else {
               throw std::runtime_error("Missing binary data");
            }
            std::advance(it, sizeof(V));
         }
      };

      template <class T>
      requires(std::same_as<std::decay_t<T>, bool> || std::same_as<std::decay_t<T>, std::vector<bool>::reference>) struct from_binary<T>
      {
         template <auto Opts>
         static void op(auto&& value, auto&& it, auto&& /* end */)
         {
            value = static_cast<bool>(*it);
            ++it;
         }
      };

      template <func_t T>
      struct from_binary<T>
      {
         template <auto Opts>
         static void op(auto&& /*value*/, auto&& /*it*/, auto&& /*end*/)
         {
         }
      };
      
      // TODO: Handle errors
      inline constexpr size_t int_from_header(auto&& it, auto&& /*end*/)
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
      inline constexpr size_t int_from_raw(auto&& it, auto&& /*end*/)
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
         template <auto Opts>
         static void op(auto&& value, auto&& it, auto&& end)
         {
            const auto n = int_from_header(it, end);
            using V = typename std::decay_t<T>::value_type;
            if constexpr (sizeof(V) == 1) {
               value.resize(n);
               std::memcpy(value.data(), &(*it), n);
               std::advance(it, n);
            }
            else {
               const auto n_bytes = sizeof(V) * n;
               value.resize(n);
               std::memcpy(value.data(), &(*it), n_bytes);
               std::advance(it, n_bytes);
            }
         }
      };
      
      template <array_t T>
      struct from_binary<T>
      {
         template <auto Opts>
         static void op(auto&& value, auto&& it, auto&& end)
         {
            if constexpr (has_static_size<T>) {
               for (auto&& item : value) {
                  read<binary>::op<Opts>(item, it, end);
               }
            }
            else {
               const auto n = int_from_header(it, end);

               if constexpr (resizeable<T>) {
                  value.resize(n);
               }
               else if (n != value.size()) {
                  throw std::runtime_error("Attempted to read into non resizable container with the wrong number of items.");
               }

               for (auto&& item: value) {
                  read<binary>::op<Opts>(item, it, end);
               }
            }
         }
      };

      template <map_t T>
      struct from_binary<T>
      {
         template <auto Opts>
         static void op(auto&& value, auto&& it, auto&& end)
         {
            const auto n = int_from_header(it, end);
            
            if constexpr (std::is_arithmetic_v<std::decay_t<typename T::key_type>>) {
               typename T::key_type key;
               for (size_t i = 0; i < n; ++i) {
                  read<binary>::op<Opts>(key, it, end);
                  read<binary>::op<Opts>(value[key], it, end);
               }
            }
            else {
               static thread_local typename T::key_type key;
               for (size_t i = 0; i < n; ++i) {
                  read<binary>::op<Opts>(key, it, end);
                  read<binary>::op<Opts>(value[key], it, end);
               }
            }
         };
      };

      template <nullable_t T>
      struct from_binary<T>
      {
         template <auto Opts>
         static void op(auto&& value, auto&& it, auto&& end)
         {
            const auto has_value = static_cast<bool>(*it);
            ++it;

            if (has_value) {
               if constexpr (is_specialization_v<T, std::optional>)
                  value = std::make_optional<typename T::value_type>();
               else if constexpr (is_specialization_v<T, std::unique_ptr>)
                  value = std::make_unique<typename T::element_type>();
               else if constexpr (is_specialization_v<T, std::shared_ptr>)
                  value = std::make_shared<typename T::element_type>();

               read<binary>::op<Opts>(*value, it, end);
            }
            else {
               if constexpr (is_specialization_v<T, std::optional>)
                  value = std::nullopt;
               else if constexpr (is_specialization_v<T, std::unique_ptr>)
                  value = nullptr;
               else if constexpr (is_specialization_v<T, std::shared_ptr>)
                  value = nullptr;
            }
         }
      };
      
      template <class T>
      requires glaze_object_t<T>
      struct from_binary<T>
      {
         template <auto Opts>
         static void op(auto&& value, auto&& it, auto&& end)
         {
            const auto n_keys = int_from_header(it, end);
            
            static constexpr auto frozen_map = detail::make_int_map<T>();
            
            for (size_t i = 0; i < n_keys; ++i) {
               const auto key = int_from_header(it, end);
               const auto& member_it = frozen_map.find(key);
               if (member_it != frozen_map.end()) {
                  std::visit(
                     [&](auto&& member_ptr) {
                        using V = std::decay_t<decltype(member_ptr)>;
                        if constexpr (std::is_member_pointer_v<V>) {
                           read<binary>::op<Opts>(value.*member_ptr, it, end);
                        }
                        else {
                           read<binary>::op<Opts>(member_ptr(value), it, end);
                        }
                     },
                     member_it->second);
               }
            }
         }
      };

      template <class T>
      requires glaze_array_t<T>
      struct from_binary<T>
      {
         template <auto Opts>
         static void op(auto&& value, auto&& it, auto&& end)
         {
            using V = std::decay_t<T>;
            for_each<std::tuple_size_v<meta_t<V>>>([&](auto I) {
               read<binary>::op<Opts>(value.*std::get<I>(meta_v<V>), it, end);
            });
         }
      };
   }
   
   template <class T, class Buffer>
   inline void read_binary(T&& value, Buffer&& buffer)
   {
      read<opts{.format = binary}>(value, std::forward<Buffer>(buffer));
   }
   
   template <class T, class Buffer>
   inline auto read_binary(Buffer&& buffer)
   {
      T value{};
      read<opts{.format = binary}>(value, std::forward<Buffer>(buffer));
      return value;
   }
}
