// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/format.hpp"
#include "glaze/util/dump.hpp"
#include "glaze/binary/header.hpp"
#include "glaze/core/read.hpp"
#include "glaze/file/file_ops.hpp"

namespace glz
{
   namespace detail
   {
      template <class T = void>
      struct from_binary {};
      
      template <>
      struct read<binary>
      {
         template <auto Opts, class T, is_context Ctx, class It0, class It1>
         static void op(T&& value, Ctx&& ctx, It0&& it, It1&& end) noexcept {
            from_binary<std::decay_t<T>>::template op<Opts>(std::forward<T>(value), std::forward<Ctx>(ctx), std::forward<It0>(it), std::forward<It1>(end));
         }
      };
      
      template <glaze_value_t T>
      struct from_binary<T>
      {
         template <auto Opts, is_context Ctx, class It0, class It1>
         static void op(auto&& value, Ctx&& ctx, It0&& it, It1&& end) {
            using V = decltype(get_member(std::declval<T>(), meta_wrapper_v<T>));
            from_binary<V>::template op<Opts>(get_member(value, meta_wrapper_v<T>), std::forward<Ctx>(ctx), std::forward<It0>(it), std::forward<It1>(end));
         }
      };
      
      template <glaze_flags_t T>
      struct from_binary<T>
      {
         template <auto Opts, is_context Ctx, class It0, class It1>
         static void op(auto&& value, Ctx&& ctx, It0&& it, It1&& end) {
            static constexpr auto N = std::tuple_size_v<meta_t<T>>;
            
            static constexpr auto Length = byte_length<T>();
            uint8_t data[Length];
            
            std::memcpy(data, &(*it), Length);
            std::advance(it, Length);
            
            for_each<N>([&](auto I) {
               static constexpr auto item = glz::tuplet::get<I>(meta_v<T>);
               
               get_member(value, glz::tuplet::get<1>(item)) = data[I / 8] & (uint8_t{1} << (7 - (I % 8)));
            });
         }
      };
      
      template <class T>
      requires(num_t<T> || char_t<T> || glaze_enum_t<T>)
      struct from_binary<T>
      {
         template <auto Opts>
         static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            using V = std::decay_t<T>;
            std::memcpy(&value, &(*it), sizeof(V));
            std::advance(it, sizeof(V));
         }
      };

      template <class T>
      requires(std::same_as<std::decay_t<T>, bool> || std::same_as<std::decay_t<T>, std::vector<bool>::reference>) struct from_binary<T>
      {
         template <auto Opts>
         static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& /* end */) noexcept
         {
            value = static_cast<bool>(*it);
            ++it;
         }
      };
      
      template <is_member_function_pointer T>
      struct from_binary<T>
      {
         template <auto Opts>
         static void op(auto&& /*value*/, is_context auto&& /*ctx*/, auto&& /*it*/, auto&& /*end*/) noexcept
         {
         }
      };

      template <func_t T>
      struct from_binary<T>
      {
         template <auto Opts>
         static void op(auto&& /*value*/, is_context auto&& /*ctx*/, auto&& /*it*/, auto&& /*end*/) noexcept
         {
         }
      };

      template <class T>
      requires std::same_as<std::decay_t<T>, raw_json>
      struct from_binary<T>
      {
         template <auto Opts>
         static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            read<binary>::op<Opts>(value.str, ctx, it, end);
         }
      };

      inline constexpr size_t int_from_header(auto&& it, auto&& /*end*/) noexcept
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
      struct from_binary<T> final
      {
         template <auto Opts>
         static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
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
      struct from_binary<T> final
      {
         template <auto Opts>
         static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            if constexpr (has_static_size<T>) {
               for (auto&& item : value) {
                  read<binary>::op<Opts>(item, ctx, it, end);
               }
            }
            else {
               const auto n = int_from_header(it, end);

               if constexpr (resizeable<T>) {
                  value.resize(n);
                  
                  if constexpr (Opts.shrink_to_fit) {
                     value.shrink_to_fit();
                  }
               }

               for (auto&& item: value) {
                  read<binary>::op<Opts>(item, ctx, it, end);
               }
            }
         }
      };

      template <map_t T>
      struct from_binary<T> final
      {
         template <auto Opts>
         static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            const auto n = int_from_header(it, end);
            
            if constexpr (std::is_arithmetic_v<std::decay_t<typename T::key_type>>) {
               typename T::key_type key;
               for (size_t i = 0; i < n; ++i) {
                  read<binary>::op<Opts>(key, ctx, it, end);
                  read<binary>::op<Opts>(value[key], ctx, it, end);
               }
            }
            else {
               static thread_local typename T::key_type key;
               for (size_t i = 0; i < n; ++i) {
                  read<binary>::op<Opts>(key, ctx, it, end);
                  read<binary>::op<Opts>(value[key], ctx, it, end);
               }
            }
         };
      };

      template <nullable_t T>
      struct from_binary<T> final
      {
         template <auto Opts>
         static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
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

               read<binary>::op<Opts>(*value, ctx, it, end);
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
      struct from_binary<includer<T>>
      {
         template <auto Opts>
         static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept {
         }
      };
      
      template <class T>
      requires glaze_object_t<T>
      struct from_binary<T> final
      {
         template <auto Opts>
         static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            if constexpr (Opts.use_cx_tags) {
               const auto n_keys = int_from_header(it, end);
               
               static constexpr auto storage = detail::make_crusher_map<T>();
               
               for (size_t i = 0; i < n_keys; ++i) {
                  uint32_t key;
                  std::memcpy(&key, &(*it), 4);
                  std::advance(it, 4);
                  
                  const auto& p = storage.find(key);
                  
                  if (p != storage.end()) {
                     std::visit(
                                [&](auto&& member_ptr) {
                                   using V = std::decay_t<decltype(member_ptr)>;
                                   read<binary>::op<Opts>(get_member(value, member_ptr), ctx, it, end);
                                },
                        p->second);
                  }
               }
            }
            else {
               using V = std::decay_t<T>;
               for_each<std::tuple_size_v<meta_t<V>>>([&](auto I) {
                  read<binary>::op<Opts>(get_member(value, glz::tuplet::get<1>(glz::tuplet::get<I>(meta_v<V>))), ctx, it, end);
               });
            }
         }
      };

      template <class T>
      requires glaze_array_t<T>
      struct from_binary<T> final
      {
         template <auto Opts>
         static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            using V = std::decay_t<T>;
            for_each<std::tuple_size_v<meta_t<V>>>([&](auto I) {
               read<binary>::op<Opts>(get_member(value, glz::tuplet::get<I>(meta_v<V>)), ctx, it, end);
            });
         }
      };

      template <class T>
      requires is_std_tuple<T>
      struct from_binary<T> final
      {
         template <auto Opts>
         static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            using V = std::decay_t<T>;
            for_each<std::tuple_size_v<V>>([&](auto I) {
               read<binary>::op<Opts>(std::get<I>(value), ctx, it, end);
            });
         }
      };
   }
   
   template <class T, class Buffer>
   inline void read_binary(T&& value, Buffer&& buffer) noexcept
   {
      read<opts{.format = binary}>(value, std::forward<Buffer>(buffer));
   }
   
   template <class T, class Buffer>
   inline auto read_binary(Buffer&& buffer) noexcept
   {
      T value{};
      read<opts{.format = binary}>(value, std::forward<Buffer>(buffer));
      return value;
   }
   
   template <class T>
   inline void read_file_binary(T& value, const sv file_name) {
            
      context ctx{};
      ctx.current_file = file_name;
      
      std::string buffer;
      file_to_buffer(buffer, ctx.current_file);
      
      read<opts{.format = binary}>(value, buffer, ctx);
   }
}
