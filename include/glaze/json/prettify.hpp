// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

// TODO: this only handles /**/ style comments

namespace glaze
{
   enum class GeneralState : uint32_t {
      NORMAL,
      ESCAPED,
      STRING,
      BEFORE_ASTERISK,
      COMMENT,
      BEFORE_FSLASH
   };

   inline void prettify_normal_state(char c, auto& out, int& indent, auto nl,
                              GeneralState& state) noexcept
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

   inline void prettify_other_states(char c, GeneralState& state) noexcept
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
   inline void prettify(const auto& in, auto& out, const bool tabs = false,
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
   
   inline std::string prettify(const auto& in, const bool tabs = false,
                 const int indent_size = 3) noexcept
   {
      std::string out{};
      prettify(in, out, tabs, indent_size);
      return out;
   }
}  // namespace test
