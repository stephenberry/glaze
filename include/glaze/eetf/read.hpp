#pragma once

#include <glaze/core/opts.hpp>
#include <glaze/core/read.hpp>
#include <glaze/core/reflect.hpp>

#include "defs.hpp"
#include "ei.hpp"

namespace glz
{
   template <opts Opts, bool Padded = false>
      requires(has_format(Opts, ERLANG))
   auto read_iterators(is_context auto&& ctx, contiguous auto&& buffer) noexcept
   {
      auto [s, e] = detail::read_iterators_impl<Padded>(buffer);

      // decode version
      decode_version(ctx, s);
      return std::pair{s, e};
   }

   namespace detail
   {
      // TODO skip value
      template <>
      struct skip_value<ERLANG>
      {
         template <auto Opts>
         GLZ_ALWAYS_INLINE static void op(is_context auto&& /* ctx */, auto&& /* it */, auto&& /* end */) noexcept
         {}
      };

      template <>
      struct read<ERLANG>
      {
         template <auto Opts, class T, is_context Ctx, class It0, class It1>
            requires(not has_no_header(Opts))
         GLZ_ALWAYS_INLINE static void op(T&& value, Ctx&& ctx, It0&& it, It1&& end) noexcept
         {
            if (bool(ctx.error)) {
               return;
            }

            if constexpr (std::is_const_v<std::remove_reference_t<T>>) {
               if constexpr (Opts.error_on_const_read) {
                  ctx.error = error_code::attempt_const_read;
               }
               else {
                  // do not read anything into the const value
                  skip_value<ERLANG>::op<Opts>(std::forward<Ctx>(ctx), std::forward<It0>(it), std::forward<It1>(end));
               }
            }
            else {
               from<ERLANG, std::remove_cvref_t<T>>::template op<Opts>(std::forward<T>(value), std::forward<Ctx>(ctx),
                                                                       std::forward<It0>(it), std::forward<It1>(end));
            }
         }
      };

      template <readable_array_t T>
      struct from<ERLANG, T> final
      {
         template <auto Opts, is_context Ctx, class It0, class It1>
         GLZ_ALWAYS_INLINE static void op(auto&& value, Ctx&& ctx, It0&& it, It1&& end) noexcept
         {
            GLZ_END_CHECK();
            decode_sequence<Opts>(std::forward<T>(value), std::forward<Ctx>(ctx), std::forward<It0>(it),
                                           std::forward<It1>(end));
         }
      };

      template <boolean_like T>
      struct from<ERLANG, T>
      {
         template <auto Opts, is_context Ctx, class It0, class It1>
         GLZ_ALWAYS_INLINE static void op(auto&& value, Ctx&& ctx, It0&& it, It1&& end) noexcept
         {
            GLZ_END_CHECK();
            decode_boolean(std::forward<T>(value), std::forward<Ctx>(ctx), std::forward<It0>(it),
                                    std::forward<It1>(end));
         }
      };

      template <num_t T>
      struct from<ERLANG, T> final
      {
         template <auto Opts, is_context Ctx, class It0, class It1>
         GLZ_ALWAYS_INLINE static void op(auto&& value, Ctx&& ctx, It0&& it, It1&& end) noexcept
         {
            GLZ_END_CHECK();

            decode_number(std::forward<T>(value), std::forward<Ctx>(ctx), std::forward<It0>(it),
                                   std::forward<It1>(end));
         }
      };

      template <atom_t T>
      struct from<ERLANG, T> final
      {
         template <auto Opts, is_context Ctx, class It0, class It1>
         GLZ_ALWAYS_INLINE static void op(auto&& value, Ctx&& ctx, It0&& it, It1&& end) noexcept
         {
            GLZ_END_CHECK();

            decode_token(std::forward<T>(value), std::forward<Ctx>(ctx), std::forward<It0>(it),
                                  std::forward<It1>(end));
         }
      };

      template <str_t T>
      struct from<ERLANG, T> final
      {
         template <auto Opts, is_context Ctx, class It0, class It1>
         GLZ_ALWAYS_INLINE static void op(auto&& value, Ctx&& ctx, It0&& it, It1&& end) noexcept
         {
            GLZ_END_CHECK();

            decode_token(std::forward<T>(value), std::forward<Ctx>(ctx), std::forward<It0>(it),
                                  std::forward<It1>(end));
         }
      };

      template <class T>
         requires(tuple_t<T> || is_std_tuple<T>)
      struct from<ERLANG, T> final
      {
         template <auto Opts>
         static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            GLZ_END_CHECK();

            auto [fields_count, index] = decode_tuple_header(ctx, it);
            if (bool(ctx.error)) [[unlikely]] {
               return;
            }

            if (it + index > end) {
               ctx.error = error_code::unexpected_end;
               return;
            }

            it += index;

            using V = std::decay_t<T>;
            constexpr auto N = glz::tuple_size_v<V>;

            if (fields_count != N) {
               ctx.error = error_code::syntax_error;
               return;
            }

            if constexpr (is_std_tuple<T>) {
               invoke_table<N>([&]<size_t I>() { read<ERLANG>::op<Opts>(std::get<I>(value), ctx, it, end); });
            }
            else {
               invoke_table<N>([&]<size_t I>() { read<ERLANG>::op<Opts>(glz::get<I>(value), ctx, it, end); });
            }
         }
      };

      template <class T>
         requires glaze_object_t<T> || reflectable<T>
      struct from<ERLANG, T> final
      {
         template <auto Opts, is_context Ctx, class It0, class It1>
         static void op(auto&& value, Ctx&& ctx, It0&& it, It1&& end) noexcept
         {
            GLZ_END_CHECK();

            const auto tag = get_type(ctx, it);
            if (bool(ctx.error)) [[unlikely]] {
               return;
            }

            static constexpr auto N = reflect<T>::size;

            std::size_t fields_count{0};
            if (eetf::is_map(tag)) [[likely]] {
               [[maybe_unused]] auto [arity, idx] = decode_map_header(std::forward<Ctx>(ctx), it);
               if (bool(ctx.error)) [[unlikely]] {
                  return;
               }

               if (it + idx > end) [[unlikely]] {
                  ctx.error = error_code::unexpected_end;
                  return;
               }

               it += idx;
               fields_count = arity;
            }
            else if (eetf::is_tuple(tag)) {
               // parse tuple
            }
            else if (eetf::is_list(tag)) {
               // parse list
            }
            else [[unlikely]] {
               // error
               ctx.error = error_code::elements_not_convertible_to_design;
               return;
            }

            // empty term
            if (fields_count == 0) {
               return;
            }

            for (size_t i = 0; i < fields_count; ++i) {
               if constexpr (N > 0) {
                  static constexpr auto HashInfo = hash_info<T>;

                  // TODO this is only for erlmap + atom keys
                  eetf::atom mkey;
                  from<ERLANG, eetf::atom>::op<Opts>(mkey, ctx, it, end);
                  if (bool(ctx.error)) [[unlikely]] {
                     return;
                  }

                  const auto n = mkey.size();
                  const auto index = decode_hash_with_size<ERLANG, T, HashInfo, HashInfo.type>::op(mkey.data(), end, n);
                  if (index < N) [[likely]] {
                     const sv key{mkey.data(), n};

                     jump_table<N>(
                        [&]<size_t I>() {
                           static constexpr auto TargetKey = get<I>(reflect<T>::keys);
                           static constexpr auto Length = TargetKey.size();
                           if ((Length == n) && compare<Length>(TargetKey.data(), key.data())) [[likely]] {
                              if constexpr (detail::reflectable<T>) {
                                 read<ERLANG>::op<Opts>(get_member(value, get<I>(detail::to_tuple(value))), ctx, it,
                                                        end);
                              }
                              else {
                                 read<ERLANG>::op<Opts>(get_member(value, get<I>(reflect<T>::values)), ctx, it, end);
                              }
                           }
                           else {
                              if constexpr (Opts.error_on_unknown_keys) {
                                 ctx.error = error_code::unknown_key;
                                 return;
                              }
                              else {
                                 skip_value<ERLANG>::op<Opts>(ctx, it, end);
                                 if (bool(ctx.error)) [[unlikely]]
                                    return;
                              }
                           }
                        },
                        index);

                     if (bool(ctx.error)) [[unlikely]] {
                        return;
                     }
                  }
                  else [[unlikely]] {
                     if constexpr (Opts.error_on_unknown_keys) {
                        ctx.error = error_code::unknown_key;
                        return;
                     }
                     else {
                        it += n;
                        skip_value<ERLANG>::op<Opts>(ctx, it, end);
                        if (bool(ctx.error)) [[unlikely]]
                           return;
                     }
                  }
               }
               else if constexpr (Opts.error_on_unknown_keys) {
                  ctx.error = error_code::unknown_key;
                  return;
               }
               else {
                  skip_value<ERLANG>::op<Opts>(ctx, it, end);
                  if (bool(ctx.error)) [[unlikely]] {
                     return;
                  }
               }
            }
         }
      };

   } // namespace detail

   template <class T>
   concept read_eetf_supported = requires { detail::from<ERLANG, std::remove_cvref_t<T>>{}; };

   template <class T>
   struct read_format_supported<ERLANG, T>
   {
      static constexpr auto value = read_eetf_supported<T>;
   };

   template <read_eetf_supported T, class Buffer>
   [[nodiscard]] inline error_ctx read_term(T&& value, Buffer&& buffer) noexcept
   {
      return read<opts{.format = ERLANG}>(value, std::forward<Buffer>(buffer));
   }

   template <read_eetf_supported T, is_buffer Buffer>
   [[nodiscard]] expected<T, error_ctx> read_term(Buffer&& buffer) noexcept
   {
      T value{};
      context ctx{};
      const error_ctx ec = read<opts{.format = ERLANG}>(value, std::forward<Buffer>(buffer), ctx);
      if (ec) {
         return unexpected<error_ctx>(ec);
      }
      return value;
   }

} // namespace glz
