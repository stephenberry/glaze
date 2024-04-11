// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

// TODO: this only handles /**/ style comments

#include "glaze/core/common.hpp"
#include "glaze/core/opts.hpp"
#include "glaze/util/dump.hpp"
#include "glaze/util/parse.hpp"

namespace glz
{
   namespace detail
   {
      enum class general_state : uint32_t { NORMAL, ESCAPED, STRING, BEFORE_ASTERISK, COMMENT, BEFORE_FSLASH };
      
      inline void handle_other_states(const char c, general_state& state) noexcept
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
      
      inline void minify_normal_state(const char c, auto& out, general_state& state) noexcept
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
   }

   /// <summary>
   /// Minify a JSON string
   /// </summary>
   inline void minify(const auto& in, auto& out) noexcept
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
   inline std::string minify(const auto& in) noexcept
   {
      std::string out{};
      minify(in, out);
      return out;
   }
}

namespace glz
{
   namespace detail
   {
      enum struct json_type : char {
         Unset = -1,
         String = '"',
         Comma = ',',
         Number = '-',
         Colon = ':',
         Array_Start = '[',
         Array_End = ']',
         Null = 'n',
         Bool = 't',
         Object_Start = '{',
         Object_End = '}'
      };
      
      constexpr std::array<json_type, 128> ascii_json_types = []{
         std::array<json_type, 128> t{};
         using enum json_type;
         t['"'] = String;
         t[','] = Comma;
         t['0'] = Number;
         t['1'] = Number;
         t['2'] = Number;
         t['3'] = Number;
         t['4'] = Number;
         t['5'] = Number;
         t['6'] = Number;
         t['7'] = Number;
         t['8'] = Number;
         t['9'] = Number;
         t['-'] = Number;
         t[':'] = Colon;
         t['['] = Array_Start;
         t[']'] = Array_End;
         t['n'] = Null;
         t['t'] = Bool;
         t['{'] = Object_Start;
         t['}'] = Object_End;
         return t;
      }();
      
      template <bool use_tabs, uint8_t indentation_width>
      inline void append_new_line(auto&& b, auto&& ix, const int64_t indent) {
         dump<'\n'>(b, ix);
         if constexpr (use_tabs) {
            dumpn<'\t'>(indent, b, ix);
         }
         else {
            dumpn<' '>(indent * indentation_width, b, ix);
         }
      };
      
      inline sv read_json_string(auto&& it, auto&& end) noexcept
      {
         auto start = it;
         for (const auto end_m7 = end - 7; it < end_m7; it += 8) {
            uint64_t chunk;
            std::memcpy(&chunk, it, 8);
            const uint64_t quote = has_quote(chunk);
            if (quote) {
               it += (std::countr_zero(quote) >> 3);
               
               auto* prev = it - 1;
               while (*prev == '\\') {
                  --prev;
               }
               if (size_t(it - prev) % 2) {
                  const sv ret{ start, size_t(it - start) };
                  ++it; // skip quote
                  return ret;
               }
               // else we have an escaped quote, so continue
            }
         }
         
         // Tail end of buffer. Should be rare we even get here
         while (it < end) {
            switch (*it) {
               case '"': {
                  auto* prev = it - 1;
                  while (*prev == '\\') {
                     --prev;
                  }
                  if (size_t(it - prev) % 2) {
                     const sv ret{ start, size_t(it - start) };
                     ++it; // skip quote
                     return ret;
                  }
                  ++it; // skip the escaped quote
                  break;
               }
               default: {
                  ++it;
               }
            }
         }
         
         return {};
      }
      
      constexpr std::array<bool, 128> numeric_ascii_table = []{
         std::array<bool, 128> t{};
         t['0'] = true;
         t['1'] = true;
         t['2'] = true;
         t['3'] = true;
         t['4'] = true;
         t['5'] = true;
         t['6'] = true;
         t['7'] = true;
         t['8'] = true;
         t['9'] = true;
         t['.'] = true;
         t['+'] = true;
         t['-'] = true;
         t['e'] = true;
         t['E'] = true;
         return t;
      }();
      
      inline sv read_json_number(auto&& it, auto&& end) noexcept
      {
         auto start = it;
         while (it < end && numeric_ascii_table[*it])
         {
            ++it;
         }
         return { start, size_t(it - start) };
      }
      
      template <opts Opts>
      inline void prettify_json(is_context auto&& ctx, auto&& it, auto&& end, auto&& b, auto&& ix) noexcept {
         constexpr bool use_tabs = Opts.indentation_char == '\t';
         constexpr auto indent_width = Opts.indentation_width;
         
         using enum json_type;
         
         std::vector<json_type> state(64);
         int64_t indent{};
         
         while (it < end) {
            switch (ascii_json_types[size_t(*it)]) {
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
                  } else {
                     if (state[indent] == Object_Start) {
                        append_new_line<use_tabs, indent_width>(b, ix, indent);
                     } else {
                        dump<' '>(b, ix);
                     }
                  }
                  break;
               }
               case Number: {
                  const auto value = read_json_number(it, end);
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
                     } else {
                        dump<' '>(b, ix);
                     }
                  }
                  else {
                     dump<' '>(b, ix);
                  }
                  break;
               }
               case Array_End : {
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
               case Null : {
                  dump<"null">(b, ix);
                  it += 4;
                  break;
               }
               case Bool : {
                  if (*it == 't') {
                     dump<"true">(b, ix);
                     it += 4;
                     break;
                  } else {
                     dump<"false">(b, ix);
                     it += 5;
                     break;
                  }
               }
               case Object_Start : {
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
               case Object_End : {
                  --indent;
                  if (indent < 0) {
                     ctx.error = error_code::syntax_error;
                     return;
                  }
                  if (it[-1] != '{') {
                     append_new_line<use_tabs, indent_width>(b, ix, indent);
                  }
                  dump<'}'>(b, ix);
                  break;
               }
               case Unset:
                  [[fallthrough]];
                  [[unlikely]] default : {
                     ctx.error = error_code::syntax_error;
                     return;
                  }
            }
         }
         
         if (it == end) [[unlikely]] {
            ctx.error = error_code::unexpected_end;
         }
      }
      
      template <opts Opts, contiguous In, output_buffer Out>
      inline void prettify_json(is_context auto&& ctx, In&& in, Out&& out) noexcept {
         if constexpr (resizeable<Out>) {
            if (out.empty()) {
               out.resize(128);
            }
         }
         size_t ix = 0;
         auto[it, end] = read_iterators<Opts>(ctx, in);
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
}
