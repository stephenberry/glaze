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
         template <auto Opts, class T, class B>
         static auto op(T&& value, B&& b)
         {
            return to_binary<std::decay_t<T>>::template op<Opts>(
               std::forward<T>(value), std::forward<B>(b));
         }

         template <auto Opts, class T, class B, class IX>
         static auto op(T&& value, B&& b, IX&& ix)
         {
            return to_binary<std::decay_t<T>>::template op<Opts>(
               std::forward<T>(value), std::forward<B>(b), std::forward<IX>(ix));
         }
      };

      template <class T>
      requires (std::same_as<T, bool> || std::same_as<T, std::vector<bool>::reference> || std::same_as<T, std::vector<bool>::const_reference>)
      struct to_binary<T> final
      {
         template <auto Opts, class... Args>
         static auto op(const bool value, Args&&... args) noexcept
         {
            if (value) {
               return dump<static_cast<std::byte>(1)>(std::forward<Args>(args)...);
            }
            else {
               return dump<static_cast<std::byte>(0)>(std::forward<Args>(args)...);
            }
         }
      };

      template <func_t T>
      struct to_binary<T> final
      {
         template <auto Opts, class... Args>
         static auto op(auto&& /*value*/, Args&&...) noexcept
         { return error{}; }
      };

      template <class... Args>
      auto dump_type(auto&& value, Args&&... args) noexcept
      {
         return dump(std::as_bytes(std::span{ &value, 1 }), std::forward<Args>(args)...);
      }

      template <uint64_t i, class... Args>
      [[nodiscard]] auto dump_int(Args&&... args) noexcept
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
      [[nodiscard]] auto dump_int(size_t i, Args&&... args) noexcept(Opts.no_except)
      {
         if (i < 64) {
            return dump_type(header8{ 0, static_cast<uint8_t>(i) }, std::forward<Args>(args)...);
         }
         else if (i < 16384) {
            return dump_type(header16{ 1, static_cast<uint16_t>(i) }, std::forward<Args>(args)...);
         }
         else if (i < 1073741824) {
            return dump_type(header32{ 2, static_cast<uint32_t>(i) }, std::forward<Args>(args)...);
         }
         else if (i < 4611686018427387904) {
            return dump_type(header64{ 3, i }, std::forward<Args>(args)...);
         }
         else {
            if constexpr (Opts.no_except) {
               return error::maximum_size_exceeded;
            }
            else {
               throw std::runtime_error("size not supported");
            }
         }
      }

      template <class T>
      requires num_t<T> || char_t<T> || glaze_enum_t<T>
      struct to_binary<T> final
      {
         template <auto Opts, class... Args>
         static auto op(auto&& value, Args&&... args) noexcept
         {
            return dump_type(value, std::forward<Args>(args)...);
         }
      };

      template <str_t T>
      struct to_binary<T> final
      {
         template <auto Opts, class... Args>
         static auto op(auto&& value, Args&&... args) noexcept(Opts.no_except)
         {
            dump_int<Opts>(value.size(), std::forward<Args>(args)...);
            dump(std::as_bytes(std::span{ value.data(), value.size() }), std::forward<Args>(args)...);
         }
      };

      template <array_t T>
      struct to_binary<T> final
      {
         template <auto Opts, class... Args>
         static auto op(auto&& value, Args&&... args) noexcept(Opts.no_except)
         {
            if constexpr (!has_static_size<T>) {
               dump_int<Opts>(value.size(), std::forward<Args>(args)...);
            }
            for (auto&& x : value) {
               write<binary>::op<Opts>(x, std::forward<Args>(args)...);
            }
         }
      };

      template <map_t T>
      struct to_binary<T> final
      {
         template <auto Opts, class... Args>
         static auto op(auto&& value, Args&&... args) noexcept(Opts.no_except)
         {
            dump_int<Opts>(value.size(), std::forward<Args>(args)...);
            for (auto&& [k, v] : value) {
               write<binary>::op<Opts>(k, std::forward<Args>(args)...);
               write<binary>::op<Opts>(v, std::forward<Args>(args)...);
            }
         }
      };

      template <nullable_t T>
      struct to_binary<T> final
      {
         template <auto Opts, class... Args>
         static auto op(auto&& value, Args&&... args) noexcept(Opts.no_except)
         {
            if (value) {
               dump<static_cast<std::byte>(1)>(std::forward<Args>(args)...);
               write<binary>::op<Opts>(*value, std::forward<Args>(args)...);
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
         static auto op(auto&& value, Args&&... args) noexcept(Opts.no_except)
         {
            using V = std::decay_t<T>;
            static constexpr auto N = std::tuple_size_v<meta_t<V>>;
            dump_int<N>(std::forward<Args>(args)...); // even though N is known at compile time in this case, it is not known for partial cases, so we still use a compressed integer

            for_each<N>([&](auto I) {
               static constexpr auto item = glz::tuplet::get<I>(meta_v<V>);
               dump_int<Opts>(I, std::forward<Args>(args)...); // dump the known key as an integer
               if constexpr (std::is_member_pointer_v<
                                std::tuple_element_t<1, decltype(item)>>) {
                  write<binary>::op<Opts>(value.*glz::tuplet::get<1>(item), std::forward<Args>(args)...);
               }
               else {
                  write<binary>::op<Opts>(glz::tuplet::get<1>(item)(value), std::forward<Args>(args)...);
               }
            });
         }
      };

      template <class T>
      requires glaze_array_t<T>
      struct to_binary<T> final
      {
         template <auto Opts, class... Args>
         static auto op(auto&& value, Args&&... args) noexcept
         {
            using V = std::decay_t<T>;
            for_each<std::tuple_size_v<meta_t<V>>>([&](auto I) {
               write<binary>::op<Opts>(value.*glz::tuplet::get<I>(meta_v<V>), std::forward<Args>(args)...);
            });
         }
      };
   }

   template <class T, class Buffer>
   inline auto write_binary(T&& value, Buffer&& buffer) {
      return write<opts{.format = binary}>(std::forward<T>(value), std::forward<Buffer>(buffer));
   }

   template <class T>
   inline auto write_binary(T&& value) {
      std::string buffer{};
      write<opts{.format = binary}>(std::forward<T>(value), buffer);
      return buffer;
   }

   template <auto& Partial, opts Opts, class T, class Buffer>
   requires nano::ranges::input_range<Buffer> && (sizeof(nano::ranges::range_value_t<Buffer>) == sizeof(char))
   inline auto write(T&& value, Buffer& buffer) noexcept
   {
      static constexpr auto partial = Partial;  // MSVC 16.11 hack

      if constexpr (nano::ranges::count(partial, "") > 0) {
         detail::write<binary>::op<Opts>(value, buffer);
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
            static constexpr auto key_to_int = detail::make_key_int_map<T>();
            glz::for_each<N>([&](auto I) {
               using index_t = decltype(I);
               using group_t = std::tuple_element_t<I, decltype(groups)>;
               static constexpr auto group = [](index_t Index) constexpr -> group_t {
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

               detail::dump_int<Opts>(key_to_int.find(key)->second, buffer);
               if constexpr (std::is_member_pointer_v<std::decay_t<decltype(member_ptr)>>) {
                  write<sub_partial, Opts>(value.*member_ptr, buffer);
               }
               else {
                  write<sub_partial, Opts>(member_ptr(value), buffer);
               }
            });
         }
         else if constexpr (detail::map_t<std::decay_t<T>>) {
            glz::for_each<N>([&](auto I) {
               static constexpr auto group = []() {
                  return glz::tuplet::get<decltype(I)::value>(groups);
               }();  // MSVC internal compiler error workaround
               static constexpr auto key_value = std::get<0>(group);
               static constexpr auto sub_partial = std::get<1>(group);
               static thread_local auto key =
                  typename std::decay_t<T>::key_type(key_value); // TODO handle numeric keys
               detail::write<binary>::op<Opts>(key, buffer);
               auto it = value.find(key);
               if (it != value.end()) {
                  write<sub_partial, Opts>(it->second, buffer);
               }
               else {
                  throw std::runtime_error(
                     "Invalid key for map when writing out partial message");
               }
            });
         }
      }
   }

   template <auto& Partial, class T, class Buffer>
   inline auto write_binary(T&& value, Buffer&& buffer) {
      return write<Partial, opts{}>(std::forward<T>(value), std::forward<Buffer>(buffer));
   }
}
