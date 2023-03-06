// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/opts.hpp"
#include "glaze/util/dump.hpp"
#include "glaze/binary/header.hpp"
#include "glaze/util/for_each.hpp"
#include "glaze/core/write.hpp"
#include "glaze/json/json_ptr.hpp"
#include "glaze/util/error.hpp"
#include "glaze/util/murmur.hpp"
#include "glaze/util/variant.hpp"

#include <utility>

namespace glz
{
   namespace detail
   {
      template <class T = void>
      struct to_binary {};

      template <>
      struct write<binary>
      {
         template <auto Opts, class T, is_context Ctx, class B>
         GLZ_ALWAYS_INLINE static void op(T&& value, Ctx&& ctx, B&& b) noexcept
         {
            to_binary<std::decay_t<T>>::template op<Opts>(
               std::forward<T>(value), std::forward<Ctx>(ctx), std::forward<B>(b));
         }

         template <auto Opts, class T, is_context Ctx, class B, class IX>
         GLZ_ALWAYS_INLINE static void op(T&& value, Ctx&& ctx, B&& b, IX&& ix) noexcept
         {
            to_binary<std::decay_t<T>>::template op<Opts>(
               std::forward<T>(value), std::forward<Ctx>(ctx), std::forward<B>(b), std::forward<IX>(ix));
         }
      };
      
      template <glaze_value_t T>
      struct to_binary<T>
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static void op(auto&& value, Args&&... args) noexcept {
            using V = decltype(get_member(std::declval<T>(), meta_wrapper_v<T>));
            to_binary<V>::template op<Opts>(get_member(value, meta_wrapper_v<T>), std::forward<Args>(args)...);
         }
      };
      
      template <glaze_flags_t T>
      struct to_binary<T>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&&, auto&& b, auto&& ix) {
            static constexpr auto N = std::tuple_size_v<meta_t<T>>;
            
            std::array<uint8_t, byte_length<T>()> data{};
            
            for_each<N>([&](auto I) {
               static constexpr auto item = glz::tuplet::get<I>(meta_v<T>);
               
               data[I / 8] |= static_cast<uint8_t>(get_member(value, glz::tuplet::get<1>(item))) << (7 - (I % 8));
            });
            
            dump(data, b, ix);
         }
      };
      
      template <is_member_function_pointer T>
      struct to_binary<T>
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static void op(auto&& /*value*/, is_context auto&&, Args&&...) noexcept {
         }
      };
      
      template <class T>
      struct to_binary<includer<T>>
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static void op(auto&& /*value*/, is_context auto&&, Args&&...) noexcept {
         }
      };

      template <boolean_like T>
      struct to_binary<T> final
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static void op(const bool value, is_context auto&&, Args&&... args) noexcept
         {
            if (value) {
               dump<static_cast<std::byte>(1)>(std::forward<Args>(args)...);
            }
            else {
               dump<static_cast<std::byte>(0)>(std::forward<Args>(args)...);
            }
         }
      };

      template <func_t T>
      struct to_binary<T> final
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static void op(auto&& /*value*/, is_context auto&&, Args&&...) noexcept
         {}
      };

      template <class T>
      requires std::same_as<std::decay_t<T>, raw_json>
      struct to_binary<T> final
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, Args&&... args) noexcept
         {
            write<binary>::op<Opts>(value.str, ctx, std::forward<Args>(args)...);
         }
      };

      template <class... Args>
      GLZ_ALWAYS_INLINE void dump_type(auto&& value, Args&&... args) noexcept
      {
         dump(std::as_bytes(std::span{ &value, 1 }), std::forward<Args>(args)...);
      }

      template <uint64_t i, class... Args>
      GLZ_ALWAYS_INLINE void dump_int(Args&&... args) noexcept
      {
         if constexpr (i < 64) {
            static constexpr auto h = header8{ 0, static_cast<uint8_t>(i) };
            return dump_type(h, std::forward<Args>(args)...);
         }
         else if constexpr (i < 16384) {
            static constexpr auto h = header16{ 1, static_cast<uint16_t>(i) };
            return dump_type(h, std::forward<Args>(args)...);
         }
         else if constexpr (i < 1073741824) {
            static constexpr auto h = header32{ 2, static_cast<uint32_t>(i) };
            return dump_type(h, std::forward<Args>(args)...);
         }
         else if constexpr (i < 4611686018427387904) {
            static constexpr auto h = header64{ 3, i };
            return dump_type(h, std::forward<Args>(args)...);
         }
         else {
            static_assert(i >= 4611686018427387904,
                          "size not supported");
         }
      }

      template <auto Opts, class... Args>
      GLZ_ALWAYS_INLINE void dump_int(size_t i, Args&&... args) noexcept
      {
         if (i < 64) {
            dump_type(header8{ 0, static_cast<uint8_t>(i) }, std::forward<Args>(args)...);
         }
         else if (i < 16384) {
            dump_type(header16{ 1, static_cast<uint16_t>(i) }, std::forward<Args>(args)...);
         }
         else if (i < 1073741824) {
            dump_type(header32{ 2, static_cast<uint32_t>(i) }, std::forward<Args>(args)...);
         }
         else if (i < 4611686018427387904) {
            dump_type(header64{ 3, i }, std::forward<Args>(args)...);
         }
         else {
            std::abort(); // this should never happen because we should never allocate containers of this size
         }
      }

      template <is_variant T>
      struct to_binary<T> final
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, Args&&... args) noexcept
         {
            std::visit(
               [&](auto&& v) {
                  using V = std::decay_t<decltype(v)>;
                  static constexpr auto index = []() {
                     T var = V{};
                     return var.index();
                  }();
                  dump_int<index>(args...);
                  write<binary>::op<Opts>(v, ctx, std::forward<Args>(args)...);
               },
               value);
         }
      };

      template <class T>
      requires num_t<T> || char_t<T> || glaze_enum_t<T>
      struct to_binary<T> final
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&&, Args&&... args) noexcept
         {
            dump_type(value, std::forward<Args>(args)...);
         }
      };

      template <str_t T>
      struct to_binary<T> final
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&&, Args&&... args) noexcept
         {
            dump_int<Opts>(value.size(), std::forward<Args>(args)...);
            dump(std::as_bytes(std::span{ value.data(), value.size() }), std::forward<Args>(args)...);
         }
      };

      template <array_t T>
      struct to_binary<T> final
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, Args&&... args) noexcept
         {
            if constexpr (!has_static_size<T>) {
               dump_int<Opts>(value.size(), std::forward<Args>(args)...);
            }
            for (auto&& x : value) {
               write<binary>::op<Opts>(x, ctx, std::forward<Args>(args)...);
            }
         }
      };

      template <map_t T>
      struct to_binary<T> final
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static auto op(auto&& value, is_context auto&& ctx, Args&&... args) noexcept
         {
            dump_int<Opts>(value.size(), std::forward<Args>(args)...);
            for (auto&& [k, v] : value) {
               write<binary>::op<Opts>(k, ctx, std::forward<Args>(args)...);
               write<binary>::op<Opts>(v, ctx, std::forward<Args>(args)...);
            }
         }
      };

      template <nullable_t T>
      struct to_binary<T> final
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, Args&&... args) noexcept
         {
            if (value) {
               dump<static_cast<std::byte>(1)>(std::forward<Args>(args)...);
               write<binary>::op<Opts>(*value, ctx, std::forward<Args>(args)...);
            }
            else {
               dump<static_cast<std::byte>(0)>(std::forward<Args>(args)...);
            }
         }
      };

      template <class T>
      requires glaze_object_t<T>
      struct to_binary<T> final
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, Args&&... args) noexcept
         {
            using V = std::decay_t<T>;
            static constexpr auto N = std::tuple_size_v<meta_t<V>>;
            if constexpr (Opts.use_cx_tags) {
               dump_int<N>(args...); // even though N is known at compile time in this case, it is not known for partial cases, so we still use a compressed integer
            }

            for_each<N>([&](auto I) {
               static constexpr auto item = glz::tuplet::get<I>(meta_v<V>);
               if constexpr (Opts.use_cx_tags) {
                  static constexpr uint32_t hash = murmur3_32(glz::tuplet::get<0>(item));
                  dump_type(hash, args...);
               }
               write<binary>::op<Opts>(get_member(value, glz::tuplet::get<1>(item)), ctx, args...);
            });
         }
      };

      template <class T>
      requires glaze_array_t<T>
      struct to_binary<T> final
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, Args&&... args) noexcept
         {
            using V = std::decay_t<T>;
            for_each<std::tuple_size_v<meta_t<V>>>([&](auto I) {
               write<binary>::op<Opts>(get_member(value, glz::tuplet::get<I>(meta_v<V>)), ctx, std::forward<Args>(args)...);
            });
         }
      };

      template <class T>
      requires is_std_tuple<std::decay_t<T>>
      struct to_binary<T> final
      {
         template <auto Opts, class... Args>
         GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, Args&&... args) noexcept
         {
            using V = std::decay_t<T>;
            for_each<std::tuple_size_v<V>>([&](auto I) {
               write<binary>::op<Opts>(std::get<I>(value), ctx, std::forward<Args>(args)...);
            });
         }
      };
   }

   template <class T, class Buffer>
   GLZ_ALWAYS_INLINE void write_binary(T&& value, Buffer&& buffer) {
      write<opts{.format = binary}>(std::forward<T>(value), std::forward<Buffer>(buffer));
   }

   template <class T>
   GLZ_ALWAYS_INLINE auto write_binary(T&& value) {
      std::string buffer{};
      write<opts{.format = binary}>(std::forward<T>(value), buffer);
      return buffer;
   }
   
   template <class Map, class Key>
   concept findable = requires(Map& map, const Key& key) {
      map.find(key);
   };

   template <auto& Partial, opts Opts, class T, output_buffer Buffer>
   [[nodiscard]] GLZ_ALWAYS_INLINE write_error write(T&& value, Buffer& buffer, is_context auto&& ctx) noexcept
   {
      static constexpr auto partial = Partial;  // MSVC 16.11 hack
      
      write_error we{};

      if constexpr (std::count(partial.begin(), partial.end(), "") > 0) {
         detail::write<binary>::op<Opts>(value, ctx, buffer);
      }
      else {
         static_assert(detail::glaze_object_t<std::decay_t<T>> ||
                          detail::map_t<std::decay_t<T>>,
                       "Only object types are supported for partial.");
         static constexpr auto sorted = sort_json_ptrs(partial);
         static constexpr auto groups = glz::group_json_ptrs<sorted>();
         static constexpr auto N =
            std::tuple_size_v<std::decay_t<decltype(groups)>>;

         detail::dump_int<N>(buffer);

         if constexpr (detail::glaze_object_t<std::decay_t<T>>) {
            glz::for_each<N>([&](auto I) {
               using index_t = decltype(I);
               using group_t = std::tuple_element_t<I, decltype(groups)>;
               static constexpr auto group = []([[maybe_unused]] index_t Index) constexpr -> group_t {
                  return glz::tuplet::get<decltype(I)::value>(groups);
               }({}); // MSVC internal compiler error workaround
               
               static constexpr auto key = std::get<0>(group);
               static constexpr auto sub_partial = std::get<1>(group);
               static constexpr auto frozen_map = detail::make_map<T>();
               static constexpr auto member_it = frozen_map.find(key);
               static_assert(member_it != frozen_map.end(),
                             "Invalid key passed to partial write");
               static constexpr auto ix = member_it->second.index();
               static constexpr decltype(auto) member_ptr = std::get<ix>(member_it->second);
               
               static constexpr uint32_t hash = murmur3_32(key);
               detail::dump_type(hash, buffer);
               std::ignore = write<sub_partial, Opts>(glz::detail::get_member(value, member_ptr), buffer, ctx);
            });
         }
         else if constexpr (detail::map_t<std::decay_t<T>>) {
            glz::for_each<N>([&](auto I) {
               using index_t = decltype(I);
               using group_t = std::tuple_element_t<I, decltype(groups)>;
               static constexpr auto group = []([[maybe_unused]] index_t Index) constexpr -> group_t {
                  return glz::tuplet::get<decltype(I)::value>(groups);
               }({}); // MSVC internal compiler error workaround
               
               static constexpr auto key_value = std::get<0>(group);
               static constexpr auto sub_partial = std::get<1>(group);
               if constexpr (findable<std::decay_t<T>, decltype(key_value)>) {
                  detail::write<binary>::op<Opts>(key_value, ctx, buffer);
                  auto it = value.find(key_value);
                  if (it != value.end()) {
                     std::ignore = write<sub_partial, Opts>(it->second, buffer, ctx);
                  }
                  else {
                     we.ec = error_code::invalid_partial_key;
                  }
               }
               else {
                  static thread_local auto key =
                     typename std::decay_t<T>::key_type(key_value); // TODO handle numeric keys
                  detail::write<binary>::op<Opts>(key, ctx, buffer);
                  auto it = value.find(key);
                  if (it != value.end()) {
                     std::ignore = write<sub_partial, Opts>(it->second, buffer, ctx);
                  }
                  else {
                     we.ec = error_code::invalid_partial_key;
                  }
               }
            });
         }
      }
      
      return we;
   }
   
   template <auto& Partial, opts Opts, class T, output_buffer Buffer>
   [[nodiscard]] GLZ_ALWAYS_INLINE write_error write(T&& value, Buffer& buffer) noexcept
   {
      context ctx{};
      return write<Partial, Opts>(std::forward<T>(value), buffer, ctx);
   }

   template <auto& Partial, class T, class Buffer>
   GLZ_ALWAYS_INLINE auto write_binary(T&& value, Buffer&& buffer) {
      return write<Partial, opts{.format = binary}>(std::forward<T>(value), std::forward<Buffer>(buffer));
   }
   
   // std::string file_name needed for std::ofstream
   template <class T>
   [[nodiscard]] GLZ_ALWAYS_INLINE write_error write_file_binary(T&& value, const std::string& file_name) noexcept {
      
      std::string buffer{};
      
      write<opts{.format = binary}>(std::forward<T>(value), buffer);
      
      std::ofstream file(file_name);
      
      if (file) {
         file << buffer;
      }
      else {
         return { error_code::file_open_failure };
      }
      
      return {};
   }
}
