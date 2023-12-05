// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/binary/write.hpp"

namespace glz::repe
{
   constexpr uint8_t delimiter = 0b00000'110;
   
   struct header final
   {
      uint8_t version = 0b00000'110; // the REPE version
      bool error = false; // whether an error has occurred
      bool notification = false; // whether this RPC is a notification (no response returned)
      std::string method = ""; // the RPC method to call
      std::variant<std::monostate, uint64_t, std::string> id{}; // an identifier for this RPC call
      std::vector<std::byte> custom; // custom data
      
      struct glaze {
         using T = header;
         static constexpr auto value = glz::array(&T::version, //
                                                  &T::error, //
                                                  &T::notification, //
                                                  &T::method, //
                                                  &T::id, //
                                                  &T::custom //
                                                  );
      };
   };
   
   template <class Data>
   struct error final
   {
      uint32_t code = 0;
      std::string message{};
      Data& data;
      
      struct glaze {
         using T = error;
         static constexpr auto value = glz::array(&T::code, //
                                                  &T::message, //
                                                  &T::data //
                                                  );
      };
   };
   
   template <>
   struct error<void> final
   {
      uint32_t code = 0;
      std::string message{};
      
      struct glaze {
         using T = error;
         static constexpr auto value = glz::array(&T::code, //
                                                  &T::message //
                                                  );
      };
   };
   
   // response/request
   template <class Body>
   struct message final {
      repe::header header{};
      Body& body;
   };
   
   template <>
   struct message<void> final {
      repe::header header{};
   };
   
   template <class T>
   struct call final
   {
      
   };
}

namespace glz::detail
{
   template <class T>
   struct to_binary<repe::message<T>> final
   {
      template <auto Opts, class... Args>
      GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, Args&&... args) noexcept
      {
         write<binary>::op<Opts>(value.header, ctx, args...);
         dump_type(repe::delimiter, args...);
         if constexpr (requires { T::body; }) {
            write<binary>::op<Opts>(value.body, ctx, args...);
            dump_type(repe::delimiter, args...);
         }
         else {
            dump_type(uint8_t(0), args...); // null body
            dump_type(repe::delimiter, args...);
         }
      }
   };
   
   template <class T>
   struct from_binary<repe::message<T>> final
   {
      template <auto Opts, class... Args>
      GLZ_ALWAYS_INLINE static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
      {
         read<binary>::op<Opts>(value.header, ctx, it, end);
         if (bool(ctx.error)) [[unlikely]]
            return;
         
         const auto tag = uint8_t(*it);
         if (tag != repe::delimiter) {
            ctx.error = error_code::syntax_error;
            return;
         }
         ++it;
         
         if constexpr (requires { T::body; }) {
            read<binary>::op<Opts>(value.body, ctx, it, end);
            if (bool(ctx.error)) [[unlikely]]
               return;
         }
         else {
            if (uint8_t(*it) != 0) {
               ctx.error = error_code::syntax_error;
               return;
            }
         }
         
         if (uint8_t(*it) != repe::delimiter) {
            ctx.error = error_code::syntax_error;
            return;
         }
         ++it;
      }
   };
}
