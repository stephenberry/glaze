// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/json/read.hpp"
#include "glaze/json/write.hpp"

namespace glz
{
   template <>
   struct parse<NDJSON>
   {
      template <auto Opts, class T, is_context Ctx, class It0, class It1>
      static void op(T&& value, Ctx&& ctx, It0&& it, It1&& end)
      {
         from<NDJSON, std::remove_reference_t<T>>::template op<Opts>(std::forward<T>(value), std::forward<Ctx>(ctx),
                                                                     std::forward<It0>(it), std::forward<It1>(end));
      }
   };

   template <>
   struct serialize<NDJSON>
   {
      template <auto Opts, class T, is_context Ctx, class B, class IX>
      static void op(T&& value, Ctx&& ctx, B&& b, IX&& ix)
      {
         to<NDJSON, std::decay_t<T>>::template op<Opts>(std::forward<T>(value), std::forward<Ctx>(ctx),
                                                        std::forward<B>(b), std::forward<IX>(ix));
      }
   };

   // Consume the line breaks that separate two records.
   //
   // A null-terminated buffer stops on the trailing '\0' sentinel. A non-null-terminated buffer has
   // no sentinel, so bound the scan on it != end.
   template <auto Opts>
   GLZ_ALWAYS_INLINE void skip_record_separators(is_context auto& ctx, auto& it, auto& end) noexcept
   {
      if constexpr (Opts.null_terminated) {
         while (*it == '\r') {
            ++it;
            if (*it == '\n') {
               ++it;
            }
            else {
               ctx.error = error_code::syntax_error; // Expected '\n' after '\r'
               return;
            }
         }
         while (*it == '\n') {
            ++it;
         }
      }
      else {
         while (it != end && *it == '\r') {
            ++it;
            if (it != end && *it == '\n') {
               ++it;
            }
            else {
               ctx.error = error_code::syntax_error; // Expected '\n' after '\r'
               return;
            }
         }
         while (it != end && *it == '\n') {
            ++it;
         }
      }
   }

   // Position `it` at the next record, and report whether there is one.
   //
   // Records are independent, so the gap between two of them is where a streaming window can
   // release what has been parsed and pull in more. Doing that here is what lets a document larger
   // than the window be read at all; without it the read stops at the first window's edge, carrying
   // only the records that happened to fit and calling that a complete document.
   //
   // Returns false both when the input is exhausted and when the separators were malformed; the
   // caller tells them apart from ctx.error, which is what distinguishes a document that ended from
   // one that broke.
   template <auto Opts, class Ctx>
   bool ndjson_has_next_record(Ctx& ctx, auto& it, auto& end) noexcept
   {
      // A run of separators can be wider than the window, so this loops: each pass consumes at
      // least the byte the previous refill made available, and stops as soon as the source runs
      // dry. Without it a window ending in separators looks like the start of a record.
      while (true) {
         refill_between_values(ctx, it, end);
         if (it >= end) {
            return false;
         }

         skip_record_separators<Opts>(ctx, it, end);
         if (bool(ctx.error)) [[unlikely]] {
            return false;
         }
         if (it < end) {
            return true;
         }
      }
   }

   // Read one record into `record`. Returns false on error.
   //
   // A record that closed cleanly leaves ctx.depth at zero, so end_reached there means the input
   // ran out exactly where the record ended rather than in the middle of it -- the record is
   // complete either way, and whether anything follows it is the next iteration's question.
   //
   // Any other way of running out of input means the record itself was cut short. For a buffer that
   // is a truncated document. For a stream with input still pending it is instead the window that
   // ran out: the JSON reader refills between the members of an object and the elements of an
   // array, but nothing refills in the middle of a string or a number, so a single token has to fit
   // in the window. Naming that beats reporting a truncated document, since the document is fine
   // and the buffer is the thing to change.
   template <auto Opts, class Ctx>
   bool read_ndjson_record(auto&& record, Ctx& ctx, auto& it, auto& end)
   {
      parse<JSON>::op<Opts>(record, ctx, it, end);
      resync_window_end(ctx, end); // the JSON reader may have refilled inside the record

      if (!bool(ctx.error)) [[likely]] {
         return true;
      }
      if (ctx.error == error_code::end_reached && ctx.depth == 0) {
         ctx.error = error_code::none;
         return true;
      }

      if constexpr (has_streaming_state<Ctx>) {
         const bool ran_out_of_input = ctx.error == error_code::end_reached || ctx.error == error_code::unexpected_end;
         if (ran_out_of_input && ctx.stream.enabled() && !ctx.stream.source_at_eof()) {
            ctx.error = error_code::streaming_unsupported;
            ctx.custom_error_message =
               "a string or number in this NDJSON record is wider than the buffer window; nothing "
               "refills inside a single token";
         }
      }
      return false;
   }

   template <class T>
      requires readable_array_t<T> && (emplace_backable<T> || !resizable<T>)
   struct from<NDJSON, T>
   {
      template <auto Opts>
      static void op(auto& value, is_context auto&& ctx, auto&& it, auto end)
      {
         if (bool(ctx.error)) [[unlikely]] {
            return;
         }

         if (it == end) {
            if constexpr (resizable<T>) {
               value.clear();

               if constexpr (check_shrink_to_fit(Opts) && has_shrink_to_fit<T>) {
                  value.shrink_to_fit();
               }
            }
         }

         const auto n = value.size();

         auto value_it = value.begin();

         const auto truncate_to = [&](auto first_unwritten) {
            if constexpr (erasable<T>) {
               // erase rather than resize, for element types that are not default constructible
               value.erase(first_unwritten, value.end());

               if constexpr (check_shrink_to_fit(Opts) && has_shrink_to_fit<T>) {
                  value.shrink_to_fit();
               }
            }
         };

         for (size_t i = 0; i < n; ++i) {
            if (!ndjson_has_next_record<Opts>(ctx, it, end)) {
               if (bool(ctx.error)) [[unlikely]] {
                  return;
               }
               truncate_to(value_it);
               return;
            }
            if (!read_ndjson_record<Opts>(*value_it, ctx, it, end)) [[unlikely]] {
               return;
            }
            ++value_it;
         }

         // growing
         if constexpr (emplace_backable<T>) {
            while (ndjson_has_next_record<Opts>(ctx, it, end)) {
               if (!read_ndjson_record<Opts>(value.emplace_back(), ctx, it, end)) [[unlikely]] {
                  return;
               }
            }
            if (bool(ctx.error)) [[unlikely]] {
               return; // malformed separators, not the end of the document
            }

            if constexpr (check_shrink_to_fit(Opts) && has_shrink_to_fit<T>) {
               value.shrink_to_fit();
            }
         }
         else {
            ctx.error = error_code::exceeded_static_array_size;
         }
      }
   };

   template <class T>
      requires glaze_array_t<T> || tuple_t<T> || is_std_tuple<T>
   struct from<NDJSON, T>
   {
      template <auto Opts>
      static void op(auto& value, is_context auto&& ctx, auto&& it, auto end)
      {
         if (bool(ctx.error)) [[unlikely]] {
            return;
         }

         static constexpr auto N = []() constexpr {
            if constexpr (glaze_array_t<T>) {
               return reflect<T>::size;
            }
            else {
               return glz::tuple_size_v<T>;
            }
         }();

         for_each<N>([&]<auto I>() {
            if (bool(ctx.error) || !ndjson_has_next_record<Opts>(ctx, it, end)) {
               return; // for_each has no early exit; the guard above stands in for one
            }
            if constexpr (is_std_tuple<T>) {
               (void)read_ndjson_record<Opts>(std::get<I>(value), ctx, it, end);
            }
            else if constexpr (glaze_array_t<T>) {
               (void)read_ndjson_record<Opts>(get_member(value, glz::get<I>(meta_v<T>)), ctx, it, end);
            }
            else {
               (void)read_ndjson_record<Opts>(glz::get<I>(value), ctx, it, end);
            }
         });
      }
   };

   template <writable_array_t T>
   struct to<NDJSON, T>
   {
      template <auto Opts, class... Args>
      static void op(auto&& value, is_context auto&& ctx, auto&& b, auto& ix)
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
            using Value = core_t<decltype(*it)>;
            to<JSON, Value>::template op<Opts>(*it, ctx, b, ix);
            ++it;
            const auto end = value.end();
            for (; it != end; ++it) {
               dump('\n', b, ix);
               to<JSON, Value>::template op<Opts>(*it, ctx, b, ix);
            }
         }
      }
   };

   template <class T>
      requires glaze_array_t<T> || tuple_t<T>
   struct to<NDJSON, T>
   {
      template <auto Opts, class... Args>
      static void op(auto&& value, is_context auto&& ctx, Args&&... args)
      {
         static constexpr auto N = []() constexpr {
            if constexpr (glaze_array_t<std::decay_t<T>>) {
               return glz::tuple_size_v<meta_t<std::decay_t<T>>>;
            }
            else {
               return glz::tuple_size_v<std::decay_t<T>>;
            }
         }();

         using V = std::decay_t<T>;
         for_each<N>([&]<auto I>() {
            if constexpr (glaze_array_t<V>) {
               serialize<JSON>::op<Opts>(get_member(value, glz::get<I>(meta_v<T>)), ctx, args...);
            }
            else {
               serialize<JSON>::op<Opts>(glz::get<I>(value), ctx, args...);
            }
            constexpr bool needs_new_line = I < N - 1;
            if constexpr (needs_new_line) {
               dump('\n', args...);
            }
         });
      }
   };

   template <class T>
      requires is_std_tuple<std::decay_t<T>>
   struct to<NDJSON, T>
   {
      template <auto Opts, class... Args>
      static void op(auto&& value, is_context auto&& ctx, Args&&... args)
      {
         static constexpr auto N = []() constexpr {
            if constexpr (glaze_array_t<std::decay_t<T>>) {
               return glz::tuple_size_v<meta_t<std::decay_t<T>>>;
            }
            else {
               return glz::tuple_size_v<std::decay_t<T>>;
            }
         }();

         using V = std::decay_t<T>;
         for_each<N>([&]<auto I>() {
            if constexpr (glaze_array_t<V>) {
               serialize<JSON>::op<Opts>(value.*std::get<I>(meta_v<V>), ctx, std::forward<Args>(args)...);
            }
            else {
               serialize<JSON>::op<Opts>(std::get<I>(value), ctx, std::forward<Args>(args)...);
            }
            constexpr bool needs_new_line = I < N - 1;
            if constexpr (needs_new_line) {
               dump('\n', std::forward<Args>(args)...);
            }
         });
      }
   };

   template <read_supported<NDJSON> T, class Buffer>
   [[nodiscard]] auto read_ndjson(T& value, Buffer&& buffer)
   {
      context ctx{};
      return read<opts{.format = NDJSON}>(value, std::forward<Buffer>(buffer), ctx);
   }

   template <read_supported<NDJSON> T, class Buffer>
   [[nodiscard]] expected<T, error_ctx> read_ndjson(Buffer&& buffer)
   {
      T value{};
      context ctx{};
      const auto ec = read<opts{.format = NDJSON}>(value, std::forward<Buffer>(buffer), ctx);
      if (ec == error_code::none) {
         return value;
      }
      return unexpected(ec);
   }

   template <auto Opts = opts{.format = NDJSON}, read_supported<NDJSON> T>
   [[nodiscard]] error_ctx read_file_ndjson(T& value, const sv file_name)
   {
      context ctx{};
      ctx.current_file = file_name;

      std::string buffer;

      const auto ec = file_to_buffer(buffer, ctx.current_file);

      if (bool(ec)) {
         return {0, ec};
      }

      return read<Opts>(value, buffer, ctx);
   }

   template <write_supported<NDJSON> T, class Buffer>
   [[nodiscard]] error_ctx write_ndjson(T&& value, Buffer&& buffer)
   {
      return write<opts{.format = NDJSON}>(std::forward<T>(value), std::forward<Buffer>(buffer));
   }

   template <write_supported<NDJSON> T>
   [[nodiscard]] expected<std::string, error_ctx> write_ndjson(T&& value)
   {
      return write<opts{.format = NDJSON}>(std::forward<T>(value));
   }

   template <write_supported<NDJSON> T>
   [[nodiscard]] error_ctx write_file_ndjson(T&& value, const std::string& file_name, auto&& buffer)
   {
      const auto ec = write<opts{.format = NDJSON}>(std::forward<T>(value), buffer);
      if (bool(ec)) [[unlikely]] {
         return ec;
      }
      const auto file_ec = buffer_to_file(buffer, file_name);
      if (bool(file_ec)) [[unlikely]] {
         return {0, file_ec};
      }
      return {buffer.size(), error_code::none};
   }
}
