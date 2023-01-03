// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/json/read.hpp"
#include "glaze/json/write.hpp"

namespace glz
{
   namespace detail
   {
      template <class T = void>
      struct from_ndjson {};
      
      template <>
      struct read<ndjson>
      {
         template <auto Opts, class T, is_context Ctx, class It0, class It1>
         static void op(T&& value, Ctx&& ctx, It0&& it, It1&& end) {
            from_ndjson<std::decay_t<T>>::template op<Opts>(std::forward<T>(value), std::forward<Ctx>(ctx), std::forward<It0>(it), std::forward<It1>(end));
         }
      };
      
      template <class T> requires array_t<T> &&
      (emplace_backable<T> ||
       !resizeable<T>)
      struct from_ndjson<T>
      {
         template <auto Opts>
         static void op(auto& value, is_context auto&& ctx, auto&& it, auto&& end)
         {
            if (it == end) {
               if constexpr (resizeable<T>) {
                  value.clear();
                  
                  if constexpr (Opts.shrink_to_fit) {
                     value.shrink_to_fit();
                  }
               }
            }
                        
            const auto n = value.size();
            
            auto value_it = value.begin();
            
            auto read_new_lines = [&] {
               while (*it == '\r') {
                  ++it;
                  if (*it == '\n') {
                     ++it;
                  }
                  else {
                     throw std::runtime_error(R"(Expected '\n' after '\r')");
                  }
               }
               while (*it == '\n') {
                  ++it;
               }
            };
            
            for (size_t i = 0; i < n; ++i) {
               read<json>::op<Opts>(*value_it++, ctx, it, end);
               if (it == end) {
                  value.resize(i + 1);
                  
                  if constexpr (Opts.shrink_to_fit) {
                     value.shrink_to_fit();
                  }
                  
                  return;
               }
               
               read_new_lines();
            }
            
            // growing
            if constexpr (emplace_backable<T>) {
               while (it < end) {
                  read<json>::op<Opts>(value.emplace_back(), ctx, it, end);
                  
                  read_new_lines();
               }
            }
            else {
               throw std::runtime_error("Exceeded static array size.");
            }
         }
      };

      template <class T>
      requires glaze_array_t<T> || tuple_t<T> || is_std_tuple<T>
      struct from_ndjson<T>
      {
         template <auto Opts>
         static void op(auto& value, is_context auto&& ctx, auto&& it, auto&& end)
         {
            static constexpr auto N = []() constexpr
            {
               if constexpr (glaze_array_t<T>) {
                  return std::tuple_size_v<meta_t<T>>;
               }
               else {
                  return std::tuple_size_v<T>;
               }
            }
            ();
            
            auto read_new_lines = [&] {
               while (*it == '\r') {
                  ++it;
                  if (*it == '\n') {
                     ++it;
                  }
                  else {
                     throw std::runtime_error(R"(Expected '\n' after '\r')");
                  }
               }
               while (*it == '\n') {
                  ++it;
               }
            };
            
            for_each<N>([&](auto I) {
               if (it == end) {
                  return;
               }
               if constexpr (I != 0) {
                  read_new_lines();
               }
               if constexpr (is_std_tuple<T>) {
                  read<json>::op<Opts>(std::get<I>(value), ctx, it, end);
               }
               else if constexpr (glaze_array_t<T>) {
                  read<json>::op<Opts>(get_member(value, glz::tuplet::get<I>(meta_v<T>)), ctx, it, end);
               }
               else {
                  read<json>::op<Opts>(glz::tuplet::get<I>(value), ctx, it, end);
               }
            });
         }
      };
      
      template <class T = void>
      struct to_ndjson {};
      
      template <>
      struct write<ndjson>
      {
         template <auto Opts, class T, is_context Ctx, class B, class IX>
         static void op(T&& value, Ctx&& ctx, B&& b, IX&& ix) {
            to_ndjson<std::decay_t<T>>::template op<Opts>(std::forward<T>(value), std::forward<Ctx>(ctx), std::forward<B>(b), std::forward<IX>(ix));
         }
      };
      
      template <array_t T>
      struct to_ndjson<T>
      {
         template <auto Opts, class... Args>
         static void op(auto&& value, is_context auto&& ctx, Args&&... args) noexcept
         {
            const auto is_empty = [&]() -> bool {
               if constexpr (has_size<T>) {
                  return value.size() ? false : true;
               }
               else {
                  return value.empty();
               }
            }();
            
            if (!is_empty) {
               auto it = value.begin();
               write<json>::op<Opts>(*it, ctx, std::forward<Args>(args)...);
               ++it;
               const auto end = value.end();
               for (; it != end; ++it) {
                  dump<'\n'>(std::forward<Args>(args)...);
                  write<json>::op<Opts>(*it, ctx, std::forward<Args>(args)...);
               }
            }
         }
      };

      template <class T>
      requires glaze_array_t<std::decay_t<T>> || tuple_t<std::decay_t<T>>
      struct to_ndjson<T>
      {
         template <auto Opts, class... Args>
         static void op(auto&& value, is_context auto&& ctx, Args&&... args) noexcept
         {
            static constexpr auto N = []() constexpr
            {
               if constexpr (glaze_array_t<std::decay_t<T>>) {
                  return std::tuple_size_v<meta_t<std::decay_t<T>>>;
               }
               else {
                  return std::tuple_size_v<std::decay_t<T>>;
               }
            }
            ();
            
            using V = std::decay_t<T>;
            for_each<N>([&](auto I) {
               if constexpr (glaze_array_t<V>) {
                  write<json>::op<Opts>(get_member(value, glz::tuplet::get<I>(meta_v<T>)), ctx, std::forward<Args>(args)...);
               }
               else {
                  write<json>::op<Opts>(glz::tuplet::get<I>(value), ctx, std::forward<Args>(args)...);
               }
               // MSVC bug if this logic is in the `if constexpr`
               // https://developercommunity.visualstudio.com/t/stdc20-fatal-error-c1004-unexpected-end-of-file-fo/1509806
               constexpr bool needs_new_line = I < N - 1;
               if constexpr (needs_new_line) {
                  dump<'\n'>(std::forward<Args>(args)...);
               }
            });
         }
      };

      template <class T>
      requires is_std_tuple<std::decay_t<T>>
      struct to_ndjson<T>
      {
         template <auto Opts, class... Args>
         static void op(auto&& value, is_context auto&& ctx, Args&&... args) noexcept
         {
            static constexpr auto N = []() constexpr
            {
               if constexpr (glaze_array_t<std::decay_t<T>>) {
                  return std::tuple_size_v<meta_t<std::decay_t<T>>>;
               }
               else {
                  return std::tuple_size_v<std::decay_t<T>>;
               }
            }
            ();
            
            using V = std::decay_t<T>;
            for_each<N>([&](auto I) {
               if constexpr (glaze_array_t<V>) {
                  write<json>::op<Opts>(value.*std::get<I>(meta_v<V>), ctx, std::forward<Args>(args)...);
               }
               else {
                  write<json>::op<Opts>(std::get<I>(value), ctx, std::forward<Args>(args)...);
               }
               // MSVC bug if this logic is in the `if constexpr`
               // https://developercommunity.visualstudio.com/t/stdc20-fatal-error-c1004-unexpected-end-of-file-fo/1509806
               constexpr bool needs_new_line = I < N - 1;
               if constexpr (needs_new_line) {
                  dump<'\n'>(std::forward<Args>(args)...);
               }
            });
         }
      };
   }  // namespace detail
   
   template <class T, class Buffer>
   inline void read_ndjson(T& value, Buffer&& buffer) {
      context ctx{};
      read<opts{.format = ndjson}>(value, std::forward<Buffer>(buffer), ctx);
   }
   
   template <class T, class Buffer>
   inline auto read_ndjson(Buffer&& buffer) {
      T value{};
      context ctx{};
      read<opts{.format = ndjson}>(value, std::forward<Buffer>(buffer), ctx);
      return value;
   }
   
   template <auto Opts = opts{.format = ndjson}, class T>
   inline void read_file_ndjson(T& value, const sv file_name) {
      
      context ctx{};
      ctx.current_file = file_name;
      
      std::string buffer;
      
      file_to_buffer(buffer, ctx.current_file);
      
      read<Opts>(value, buffer, ctx);
   }
   
   template <class T, class Buffer>
   inline auto write_ndjson(T&& value, Buffer&& buffer) {
      return write<opts{.format = ndjson}>(std::forward<T>(value), std::forward<Buffer>(buffer));
   }
   
   template <class T>
   inline auto write_ndjson(T&& value) {
      std::string buffer{};
      write<opts{.format = ndjson}>(std::forward<T>(value), buffer);
      return buffer;
   }
   
   // std::string file_name needed for std::ofstream
   template <class T>
   inline void write_file_ndjson(T&& value, const std::string& file_name) {
      std::string buffer{};
      write<opts{.format = ndjson}>(std::forward<T>(value), buffer);
      buffer_to_file(buffer, file_name);
   }
}
