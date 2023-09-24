// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/binary/write.hpp"

namespace glz::rpc
{
   struct delimiter {
      static constexpr uint8_t value = 0b00000'110;
   };
   
   struct version {
      static constexpr uint8_t value = 0;
   };
   
   struct header {
      glz::version version{}; // the RPC version
      bool notification = false; // whether this RPC is a notification (no response returned)
      uint64_t body_length = 0; // the length of the body message
      std::string method = ""; // the RPC method to call
      std::variant<null_t, uint64_t, std::string> id{}; // an identifier for this call
      bool error = false; // whether an error has occurred
      
      struct glaze {
         using T = header;
         static constexpr auto value = glz::array(&T::version, //
                                                  &T::notification, //
                                                  &T::body_length, //
                                                  &T::method, //
                                                  &T::id, //
                                                  &T::error);
      };
   };
   
   // response/request
   template <class T>
   struct resreq {
      glz::header header{};
      glz::delimiter delimiter{};
      T& body{};
   };
   
   template <>
   struct resreq<void> {
      glz::header header{};
      glz::delimiter delimiter{};
      null_t body{};
   };
   
   template <class T>
   struct call
   {
      
   };
}
