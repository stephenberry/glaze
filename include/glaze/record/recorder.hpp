#pragma once

#include <deque>
#include <map>
#include <string>
#include <variant>
#include <vector>

#include "glaze/core/common.hpp"
#include "glaze/csv.hpp" // TODO: split out recorder specializations so this isn't necessary
#include "glaze/json.hpp" // TODO: split out recorder specializations so this isn't necessary
#include "glaze/util/string_view.hpp"
#include "glaze/util/type_traits.hpp"
#include "glaze/util/variant.hpp"

namespace glz
{
   namespace detail
   {
      template <class Data>
      struct recorder_assigner
      {
         Data& data;
         sv name{};

         template <class T>
         void operator=(T& ref)
         {
            using container_type = std::decay_t<decltype(data[0].second.first)>;
            data.emplace_back(
               std::make_pair(name, std::make_pair(container_type{std::deque<std::decay_t<T>>{}}, &ref)));
         }
      };
   }

   /// <summary>
   /// recorder for saving state over the course of a run
   /// deques are used to avoid reallocation for large amounts of data as the recording length is typically unknown
   /// </summary>
   template <class... Ts>
   struct recorder
   {
      using container_type = std::variant<std::deque<Ts>...>;

      std::deque<std::pair<std::string, std::pair<container_type, const void*>>> data;

      auto operator[](const sv name) { return detail::recorder_assigner<decltype(data)>{data, name}; }

      void update()
      {
         for (auto& [name, value] : data) {
            auto* ptr = value.second;
            std::visit(
               [&](auto&& container) {
                  using ContainerType = std::decay_t<decltype(container)>;
                  using T = typename ContainerType::value_type;

                  container.emplace_back(*static_cast<const T*>(ptr));
               },
               value.first);
         }
      }
   };

   namespace detail
   {
      template <class T>
      concept is_recorder = is_specialization_v<T, recorder>;

      template <is_recorder T>
      struct to_json<T>
      {
         template <auto Opts, class... Args>
         static void op(auto&& value, is_context auto&& ctx, Args&&... args) noexcept
         {
            dump<'{'>(std::forward<Args>(args)...);

            if constexpr (Opts.prettify) {
               ctx.indentation_level += Opts.indentation_width;
               dump<'\n'>(args...);
               dumpn<Opts.indentation_char>(ctx.indentation_level, args...);
            }

            const size_t n = value.data.size();
            for (size_t i = 0; i < n; ++i) {
               auto& [name, v] = value.data[i];
               write<json>::op<Opts>(name, ctx, std::forward<Args>(args)...); // write name as key

               dump<':'>(std::forward<Args>(args)...);
               if constexpr (Opts.prettify) {
                  dump<' '>(args...);
               }

               write<json>::op<Opts>(v.first, ctx, std::forward<Args>(args)...); // write deque
               if (i < n - 1) {
                  dump<','>(std::forward<Args>(args)...);
               }

               if constexpr (Opts.prettify) {
                  dump<'\n'>(args...);
                  dumpn<Opts.indentation_char>(ctx.indentation_level, args...);
               }
            }

            if constexpr (Opts.prettify) {
               ctx.indentation_level -= Opts.indentation_width;
               dump<'\n'>(args...);
               dumpn<Opts.indentation_char>(ctx.indentation_level, args...);
            }
            dump<'}'>(std::forward<Args>(args)...);
         }
      };

      template <is_recorder T>
      struct from_json<T>
      {
         template <auto Options>
         static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
         {
            if (bool(ctx.error)) [[unlikely]] {
               return;
            }

            if constexpr (!Options.opening_handled) {
               skip_ws<Options>(ctx, it, end);
               match<'{'>(ctx, it, end);
            }

            skip_ws<Options>(ctx, it, end);

            static constexpr auto Opts = opening_handled_off<ws_handled_off<Options>()>();

            // we read into available containers, we do not intialize here
            const size_t n = value.data.size();
            for (size_t i = 0; i < n; ++i) {
               if (*it == '}') [[unlikely]] {
                  ctx.error = error_code::expected_brace;
               }

               // find the string, escape characters are not supported for recorders
               skip_ws<Opts>(ctx, it, end);
               const auto name = parse_key(ctx, it, end);

               auto& [str, v] = value.data[i];
               if (name != str) {
                  ctx.error = error_code::name_mismatch; // Recorder read of name does not match initialized state
                  return;
               }

               skip_ws<Opts>(ctx, it, end);
               match<':'>(ctx, it, end);
               skip_ws<Opts>(ctx, it, end);

               std::visit([&](auto&& deq) { read<json>::op<Opts>(deq, ctx, it, end); }, v.first);

               if (i < n - 1) {
                  skip_ws<Opts>(ctx, it, end);
                  match<','>(ctx, it, end);
                  skip_ws<Opts>(ctx, it, end);
               }
            }

            skip_ws<Opts>(ctx, it, end);
            match<'}'>(ctx, it, end);
         }
      };

      template <is_recorder T>
      struct to_csv<T>
      {
         template <auto Opts>
         static void op(auto&& value, is_context auto&& ctx, auto&&... args) noexcept
         {
            if constexpr (Opts.layout == rowwise) {
               const size_t n = value.data.size();
               for (size_t i = 0; i < n; ++i) {
                  auto& [name, v] = value.data[i];
                  dump(name, args...);

                  dump<','>(args...);

                  std::visit(
                     [&](auto& x) {
                        write<csv>::op<Opts>(x, ctx, args...); // write deque
                     },
                     v.first);

                  if (i < n - 1) {
                     dump<'\n'>(args...);
                  }
               }
            }
            else {
               // dump titles
               const auto n = value.data.size();
               size_t i = 0;
               for (auto& [name, data] : value.data) {
                  dump(name, args...);
                  ++i;
                  if (i < n) {
                     dump<','>(args...);
                  }
               }

               dump<'\n'>(args...);

               size_t row = 0;
               bool end = false;
               while (true) {
                  i = 0;
                  for (auto& [name, data] : value.data) {
                     bool breakout = false;
                     std::visit(
                        [&](auto& v) {
                           if (row >= v.size()) {
                              end = true;
                              breakout = true;
                              return;
                           }
                           write<csv>::op<Opts>(v[row], ctx, args...); // write deque
                        },
                        data.first);

                     if (breakout) {
                        break;
                     }

                     ++i;
                     if (i < n) {
                        dump<','>(args...);
                     }
                  }

                  if (end) {
                     break;
                  }

                  dump<'\n'>(args...);

                  ++row;
               }
            }
         }
      };
   }
}
