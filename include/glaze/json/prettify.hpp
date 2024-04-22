// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

// Minified JSONC only works with /**/ style comments, so we only supports this

#include "glaze/json/json_format.hpp"

namespace glz
{
   namespace detail
   {
      template <opts Opts>
      inline void prettify_json(is_context auto&& ctx, auto&& it, auto&& end, auto&& b, auto&& ix) noexcept
      {
         constexpr bool use_tabs = Opts.indentation_char == '\t';
         constexpr auto indent_width = Opts.indentation_width;

         using enum json_type;

         std::vector<json_type> state(64);
         int64_t indent{};

         while (it < end) {
            switch (json_types[size_t(*it)]) {
            case String: {
               const auto value = read_json_string(it, end);
               dump(value, b, ix);
               break;
            }
            case Comma: {
               dump<','>(b, ix);
               ++it;
               if constexpr (Opts.new_lines_in_arrays) {
                  append_new_line<use_tabs, indent_width>(b, ix, indent);
               }
               else {
                  if (state[indent] == Object_Start) {
                     append_new_line<use_tabs, indent_width>(b, ix, indent);
                  }
                  else {
                     dump<' '>(b, ix);
                  }
               }
               break;
            }
            case Number: {
               const auto value = read_json_number(it);
               dump(value, b, ix);
               break;
            }
            case Colon: {
               dump<": ">(b, ix);
               ++it;
               break;
            }
            case Array_Start: {
               dump<'['>(b, ix);
               ++it;
               ++indent;
               state[indent] = Array_Start;
               if (size_t(indent) >= state.size()) [[unlikely]] {
                  state.resize(state.size() * 2);
               }
               if constexpr (Opts.new_lines_in_arrays) {
                  if (*it != ']') {
                     append_new_line<use_tabs, indent_width>(b, ix, indent);
                  }
               }
               break;
            }
            case Array_End: {
               --indent;
               if (indent < 0) {
                  ctx.error = error_code::syntax_error;
                  return;
               }
               if constexpr (Opts.new_lines_in_arrays) {
                  if (it[-1] != '[') {
                     append_new_line<use_tabs, indent_width>(b, ix, indent);
                  }
               }
               dump<']'>(b, ix);
               ++it;
               break;
            }
            case Null: {
               dump<"null">(b, ix);
               it += 4;
               break;
            }
            case Bool: {
               if (*it == 't') {
                  dump<"true">(b, ix);
                  it += 4;
                  break;
               }
               else {
                  dump<"false">(b, ix);
                  it += 5;
                  break;
               }
            }
            case Object_Start: {
               dump<'{'>(b, ix);
               ++it;
               ++indent;
               state[indent] = Object_Start;
               if (size_t(indent) >= state.size()) [[unlikely]] {
                  state.resize(state.size() * 2);
               }
               if (*it != '}') {
                  append_new_line<use_tabs, indent_width>(b, ix, indent);
               }
               break;
            }
            case Object_End: {
               --indent;
               if (indent < 0) {
                  ctx.error = error_code::syntax_error;
                  return;
               }
               if (it[-1] != '{') {
                  append_new_line<use_tabs, indent_width>(b, ix, indent);
               }
               dump<'}'>(b, ix);
               ++it;
               break;
            }
            case Comment: {
               if constexpr (Opts.comments) {
                  const auto value = read_jsonc_comment(it, end);
                  dump(value, b, ix);
                  break;
               }
               else {
                  [[fallthrough]];
               }
            }
               [[unlikely]] default:
               {
                  ctx.error = error_code::syntax_error;
                  return;
               }
            }
         }
      }

      template <opts Opts, contiguous In, output_buffer Out>
      inline void prettify_json(is_context auto&& ctx, In&& in, Out&& out) noexcept
      {
         if constexpr (resizeable<Out>) {
            if (in.empty()) {
               out.clear();
               return;
            }
            out.resize(in.size() * 2);
         }
         size_t ix = 0;
         auto [it, end] = read_iterators<Opts>(ctx, in);
         if (bool(ctx.error)) [[unlikely]] {
            return;
         }
         prettify_json<Opts>(ctx, it, end, out, ix);
         if constexpr (resizeable<Out>) {
            out.resize(ix);
         }
      }
   }

   // We don't return errors from prettifying even though they are handled because the error case
   // should not happen since we prettify auto-generated JSON.
   // The detail version can be used if error context is needed

   template <opts Opts = opts{}>
   inline void prettify_json(const auto& in, auto& out) noexcept
   {
      context ctx{};
      detail::prettify_json<Opts>(ctx, in, out);
   }

   /// <summary>
   /// allocating version of prettify
   /// </summary>
   template <opts Opts = opts{}>
   inline std::string prettify_json(const auto& in) noexcept
   {
      context ctx{};
      std::string out{};
      detail::prettify_json<Opts>(ctx, in, out);
      return out;
   }

   template <opts Opts = opts{}>
   inline void prettify_jsonc(const auto& in, auto& out) noexcept
   {
      context ctx{};
      detail::prettify_json<opt_true<Opts, &opts::comments>>(ctx, in, out);
   }

   /// <summary>
   /// allocating version of prettify
   /// </summary>
   template <opts Opts = opts{}>
   inline std::string prettify_jsonc(const auto& in) noexcept
   {
      context ctx{};
      std::string out{};
      detail::prettify_json<opt_true<Opts, &opts::comments>>(ctx, in, out);
      return out;
   }
}

// TODO: delete these deprecated versions of minify and prettify
namespace glz
{
   namespace detail
   {
      enum class general_state : uint32_t { NORMAL, ESCAPED, STRING, BEFORE_ASTERISK, COMMENT, BEFORE_FSLASH };

      [[deprecated("Use new glz::prettify_json/glz::minify_json")]] inline void handle_other_states(
         const char c, general_state& state) noexcept
      {
         switch (state) {
         case general_state::ESCAPED:
            state = general_state::NORMAL;
            break;
         case general_state::STRING:
            if (c == '"') {
               state = general_state::NORMAL;
            }
            break;
         case general_state::BEFORE_ASTERISK:
            state = general_state::COMMENT;
            break;
         case general_state::COMMENT:
            if (c == '*') {
               state = general_state::BEFORE_FSLASH;
            }
            break;
         case general_state::BEFORE_FSLASH:
            state = (c == '/' ? general_state::NORMAL : general_state::COMMENT);
            break;
         default:
            break;
         }
      }

      [[deprecated("Use new glz::prettify_json/glz::minify_json")]] inline void minify_normal_state(
         const char c, auto& out, general_state& state) noexcept
      {
         switch (c) {
         case '\\':
            out += c;
            state = general_state::ESCAPED;
            break;
         case '\"':
            out += c;
            state = general_state::STRING;
            break;
         case '/':
            out += c;
            state = general_state::BEFORE_ASTERISK;
         case ' ':
         case '\n':
         case '\r':
         case '\t':
            break;
         default:
            out += c;
            break;
         }
      }

      [[deprecated("Use new glz::prettify_json/glz::minify_json")]] inline void prettify_normal_state(
         const char c, auto& out, uint32_t& indent, auto nl, general_state& state) noexcept
      {
         switch (c) {
         case ',':
            out += c;
            nl();
            break;
         case '[':
            out += c;
            ++indent;
            nl();
            break;
         case ']':
            --indent;
            nl();
            out += c;
            break;
         case '{':
            out += c;
            ++indent;
            nl();
            break;
         case '}':
            --indent;
            nl();
            out += c;
            break;
         case '\\':
            out += c;
            state = general_state::ESCAPED;
            break;
         case '\"':
            out += c;
            state = general_state::STRING;
            break;
         case '/':
            out += " /";
            state = general_state::BEFORE_ASTERISK;
            break;
         case ':':
            out += ": ";
            break;
         case ' ':
         case '\n':
         case '\r':
         case '\t':
            break;
         default:
            out += c;
            break;
         }
      }
   }

   /// <summary>
   /// Minify a JSON string
   /// </summary>
   [[deprecated("Use new glz::prettify_json/glz::minify_json")]] inline void minify(const auto& in, auto& out) noexcept
   {
      out.reserve(in.size());

      using namespace detail;
      general_state state{general_state::NORMAL};

      for (auto c : in) {
         if (state == general_state::NORMAL) {
            minify_normal_state(c, out, state);
            continue;
         }
         else {
            out += c;
            handle_other_states(c, state);
         }
      }
   }

   /// <summary>
   /// allocating version of minify
   /// </summary>
   [[deprecated("Use new glz::prettify_json/glz::minify_json")]] inline std::string minify(const auto& in) noexcept
   {
      std::string out{};
      minify(in, out);
      return out;
   }

   /// <summary>
   /// pretty print a JSON string
   /// </summary>
   [[deprecated("Use new glz::prettify_json/glz::minify_json")]] inline void prettify(
      const auto& in, auto& out, const bool tabs = false, const uint32_t indent_size = 3) noexcept
   {
      out.reserve(in.size());
      uint32_t indent{};

      auto nl = [&]() {
         out += "\n";

         for (uint32_t i = 0; i < indent * (tabs ? 1 : indent_size); i++) {
            out += tabs ? "\t" : " ";
         }
      };

      using namespace detail;
      general_state state{general_state::NORMAL};

      for (auto c : in) {
         if (state == general_state::NORMAL) {
            prettify_normal_state(c, out, indent, nl, state);
            continue;
         }
         else {
            out += c;
            handle_other_states(c, state);
         }
      }
   }

   /// <summary>
   /// allocating version of prettify
   /// </summary>
   [[deprecated("Use new glz::prettify_json/glz::minify_json")]] inline std::string prettify(
      const auto& in, const bool tabs = false, const uint32_t indent_size = 3) noexcept
   {
      std::string out{};
      prettify(in, out, tabs, indent_size);
      return out;
   }
}
