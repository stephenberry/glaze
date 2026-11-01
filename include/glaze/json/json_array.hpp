// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <ranges>
#include <utility>

#include "glaze/core/read.hpp"
#include "glaze/core/write.hpp"
#include "glaze/json/read.hpp"
#include "glaze/json/write.hpp"

namespace glz
{
   // Reads a JSON array out of a contiguous buffer one element at a time, keeping its place between
   // calls, so that a header element can decide how the rest is read. Each element goes through the
   // same parse glz::read uses, under the given options.
   //
   //   glz::json_array_reader reader{buffer};
   //   header h{};
   //   if (auto ec = reader.read(h)) { /* format_error(ec, buffer) */ }
   //   std::vector<record> rest{};
   //   if (auto ec = reader.read_rest(rest)) { /* format_error(ec, buffer) */ }
   //
   // Every call returns error_ctx with count measured from the start of the buffer. read and skip
   // report a closed array as error_code::end_reached. An error is final: the iterator may have
   // stopped inside the failed element, so later calls return the same error.
   template <auto Options = opts{}>
   class json_array_reader
   {
      static_assert(Options.format == JSON, "json_array_reader only reads JSON");

      // Always bounded. A std::string_view is taken at its word by glz::read, and the option
      // defaults to on, so honoring it would run past the end of any view without a terminator.
      static constexpr auto Opts = [] {
         auto o = is_padded_off<Options>();
         o.null_terminated = false;
         return o;
      }();

      const char* start_{};
      const char* it_{};
      const char* end_{};
      bool opened_{};
      // Kept across elements so that the speculation budget covers the document, as it would for a
      // single glz::read of it, rather than being refilled for every element. error doubles as the
      // reader's terminal state, with end_reached meaning the array closed.
      context ctx_{};

      [[nodiscard]] error_ctx result() const noexcept
      {
         return {size_t(it_ - start_), ctx_.error, ctx_.custom_error_message};
      }

      // Skips to the next token, which is then safe to dereference.
      [[nodiscard]] bool next_token() noexcept
      {
         if (skip_ws<Opts>(ctx_, it_, end_)) {
            if (ctx_.error == error_code::end_reached) {
               ctx_.error = error_code::unexpected_end;
            }
            return false;
         }
         return true;
      }

      void close() noexcept
      {
         ++it_;
         if constexpr (check_validate_trailing_whitespace(Opts)) {
            if (not skip_ws<Opts>(ctx_, it_, end_)) {
               ctx_.error = error_code::syntax_error;
            }
            if (ctx_.error != error_code::none && ctx_.error != error_code::end_reached) {
               return;
            }
         }
         ctx_.error = error_code::end_reached;
      }

      // Moves to the start of the next element. False when there is none, and ctx_.error says why.
      // Other than for a missing element, these are the errors reading the whole array into a
      // container reports at the same byte.
      [[nodiscard]] bool advance() noexcept
      {
         if (bool(ctx_.error)) {
            return false;
         }
         if (not opened_) {
            opened_ = true;
            if (not next_token()) {
               if (ctx_.error == error_code::unexpected_end) {
                  ctx_.error = error_code::no_read_input;
               }
               return false;
            }
            if (*it_ != '[') {
               ctx_.error = error_code::expected_bracket;
               return false;
            }
            ++it_;
            if (not next_token()) {
               return false;
            }
            if (*it_ == ']') {
               close();
               return false;
            }
         }
         else {
            if (not next_token()) {
               return false;
            }
            if (*it_ == ']') {
               close();
               return false;
            }
            if (*it_ != ',') {
               ctx_.error = error_code::expected_bracket;
               return false;
            }
            ++it_;
            if (not next_token()) {
               return false;
            }
         }
         // Not left to the element's parse. glz::skip and glz::raw_json stop on these without
         // moving, so a missing element would pass, and for the rest the code depends on the type.
         if (*it_ == ',' || *it_ == ']') {
            ctx_.error = error_code::syntax_error;
            return false;
         }
         return true;
      }

      template <class T>
      void read_element(T& value)
      {
         const char* const element = it_;
         parse<JSON>::op<Opts>(value, ctx_, it_, end_);
         // Readers count depth by hand and leave it raised on the way out of an error or a partial
         // read, and a raised depth would count against the recursion limit of the next element.
         ctx_.depth = 0;

         // A partial read stops wherever it has what it wanted, possibly on a comma inside this
         // element, which the next call would take for a separator. Not every reader says so with
         // partial_read_complete: a tuple or a sized vector just returns, so under the option `it`
         // cannot be trusted at all. The glz::partial_read wrapper can end in it without the option.
         if (ctx_.error == error_code::partial_read_complete ||
             (check_partial_read(Opts) && ctx_.error == error_code::none)) {
            ctx_.error = error_code::none;
            it_ = element;
            skip_value<JSON>::op<Opts>(ctx_, it_, end_);
            ctx_.depth = 0;
         }

         // An element that reached the end of the buffer has no closing bracket after it. The
         // element cannot be vouched for either: a number may have been cut short by the end.
         if (ctx_.error == error_code::end_reached || (ctx_.error == error_code::none && it_ == end_)) {
            ctx_.error = error_code::unexpected_end;
         }
      }

     public:
      // The buffer is not copied and has to outlive the reader, and views read out of it (such as
      // std::string_view elements) stay valid as long as the buffer does.
      template <contiguous Buffer>
         requires(not is_input_streaming<std::remove_reference_t<Buffer>> && std::ranges::borrowed_range<Buffer>)
      explicit json_array_reader(Buffer&& buffer)
      {
         static_assert(sizeof(decltype(*buffer.data())) == 1);
         start_ = reinterpret_cast<const char*>(buffer.data());
         it_ = start_;
         end_ = start_ + buffer.size();
         const size_t proportional = max_speculative_parse_factor * buffer.size();
         ctx_.speculation_budget =
            (proportional > min_speculative_parse_bytes ? proportional : min_speculative_parse_bytes) + 2;
      }

      // Reads the next element. On success count is the position just past it.
      template <class T>
         requires read_supported<T, JSON>
      [[nodiscard]] error_ctx read(T& value)
      {
         if (advance()) {
            read_element(value);
         }
         return result();
      }

      // Steps over the next element without materializing it.
      [[nodiscard]] error_ctx skip()
      {
         glz::skip ignored{};
         return read(ignored);
      }

      // Appends every remaining element to `out`, reporting success when the array closes. On error
      // `out` holds the elements before the one that failed.
      template <class Container>
         requires(not fixed_capacity_container<Container> && read_supported<typename Container::value_type, JSON> &&
                  requires(Container& c, typename Container::value_type&& v) { c.push_back(std::move(v)); })
      [[nodiscard]] error_ctx read_rest(Container& out)
      {
         while (advance()) {
            typename Container::value_type element{};
            read_element(element);
            if (bool(ctx_.error)) {
               return result();
            }
            out.push_back(std::move(element));
         }
         if (ctx_.error == error_code::end_reached) {
            return {size_t(it_ - start_), error_code::none, ctx_.custom_error_message};
         }
         return result();
      }
   };

   // Appends values to a resizable buffer as the elements of one JSON array. Each value goes through
   // the same serialization glz::write uses, under the given options, and the result is what
   // glz::write produces for a std::vector of the same values.
   //
   //   std::string buffer{};
   //   glz::json_array_writer writer{buffer};
   //   if (auto ec = writer.append(value)) { /* ... */ }
   //
   // The array is closed after every call, and the next append overwrites the closing bracket. So
   // there is no close to forget: from construction on, what follows the buffer's original contents
   // is a complete array. A failed append restores the buffer, and later appends may still succeed.
   // The buffer must not be changed by anything else while the writer is in use.
   template <output_buffer Buffer, auto Options = opts{}>
      requires(buffer_traits<Buffer>::is_resizable && not is_output_streaming<Buffer>)
   class json_array_writer
   {
      static_assert(Options.format == JSON, "json_array_writer only writes JSON");

      static constexpr bool multiline = Options.prettify && check_new_lines_in_arrays(Options);

      Buffer& buffer_;
      bool empty_{true};

      // The closing that a successful append leaves at the end of the buffer.
      [[nodiscard]] size_t closing_size() const noexcept { return (multiline && not empty_) ? 2 : 1; }

      void finish(size_t ix)
      {
         if constexpr (multiline) {
            if (not empty_) {
               dump('\n', buffer_, ix);
            }
         }
         dump(']', buffer_, ix);
         buffer_traits<Buffer>::finalize(buffer_, ix);
      }

     public:
      // Whatever the buffer already holds is kept, and the array starts after it.
      explicit json_array_writer(Buffer& buffer) : buffer_(buffer)
      {
         size_t ix = buffer_.size();
         dump('[', buffer_, ix);
         finish(ix);
      }

      // On success count is the size of the buffer, which is also the count on failure, since
      // nothing the failed value wrote is left in it.
      template <class T>
         requires write_supported<T, JSON>
      [[nodiscard]] error_ctx append(T&& value)
      {
         const size_t insert = buffer_.size() - closing_size();
         size_t ix = insert;

         if (buffer_.size() < insert + 2 * write_padding_bytes) {
            resize_unfilled(buffer_, insert + 2 * write_padding_bytes);
         }

         context ctx{};
         if constexpr (multiline) {
            ctx.depth = check_indentation_width(Options);
         }
         if (empty_) {
            if constexpr (multiline) {
               dump_newline_indent(check_indentation_char(Options), ctx.depth, buffer_, ix);
            }
         }
         else {
            write_array_entry_separator<Options>(ctx, buffer_, ix);
         }
         to<JSON, std::remove_cvref_t<T>>::template op<Options>(std::forward<T>(value), ctx, buffer_, ix);

         if (bool(ctx.error)) [[unlikely]] {
            finish(insert);
            return {buffer_.size(), ctx.error, ctx.custom_error_message};
         }
         empty_ = false;
         finish(ix);
         return {buffer_.size(), error_code::none, ctx.custom_error_message};
      }
   };
}
