// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

// TODO this only handles /**/ style comments

namespace glaze
{
   enum class GeneralState {
      NORMAL,
      ESCAPED,
      STRING,
      BEFORE_ASTERISK,
      COMMENT,
      BEFORE_FSLASH
   };

   template <class Buffer, class NLF>
   void prettify_normal_state(char c, Buffer& out, int& indent, NLF nl,
                              GeneralState& state)
   {
      switch (c) {
      case ',':
         out += c;
         nl();
         break;
      case '[':
         out += c;
         indent++;
         nl();
         break;
      case ']':
         indent--;
         nl();
         out += c;
         break;
      case '{':
         out += c;
         indent++;
         nl();
         break;
      case '}':
         indent--;
         nl();
         out += c;
         break;
      case '\\':
         out += c;
         state = GeneralState::ESCAPED;
         break;
      case '\"':
         out += c;
         state = GeneralState::STRING;
         break;
      case '/':
         out += " /";
         state = GeneralState::BEFORE_ASTERISK;
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

   inline void prettify_other_states(char c, GeneralState& state)
   {
      switch (state) {
      case GeneralState::ESCAPED:
         state = GeneralState::NORMAL;
         break;
      case GeneralState::STRING:
         if (c == '"') {
            state = GeneralState::NORMAL;
         }
         break;
      case GeneralState::BEFORE_ASTERISK:
         state = GeneralState::COMMENT;
         break;
      case GeneralState::COMMENT:
         if (c == '*') {
            state = GeneralState::BEFORE_FSLASH;
         }
         break;
      case GeneralState::BEFORE_FSLASH:
         if (c == '/') {
            state = GeneralState::NORMAL;
         }
         else {
            state = GeneralState::COMMENT;
         }
         break;
      default:
         break;
      }
   }

   /// <summary>
   /// pretty print a JSON string
   /// </summary>
   template <class Buffer>
   void prettify(Buffer const& in, Buffer& out, const bool tabs = false,
                 const int indent_size = 3) noexcept
   {
      out.reserve(in.size());
      int indent{};

      auto nl = [&]() {
         out += "\n";

         for (auto i = 0; i < indent * (tabs ? 1 : indent_size); i++) {
            out += tabs ? "\t" : " ";
         }
      };

      GeneralState state{GeneralState::NORMAL};

      for (auto c : in) {
         if (state == GeneralState::NORMAL) {
            prettify_normal_state(c, out, indent, nl, state);
            continue;
         }
         else {
            out += c;
            prettify_other_states(c, state);
         }
      }
   }
}  // namespace test
