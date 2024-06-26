// Glaze Library
// For the license information refer to glaze.hpp

// Modified from the awesome: https://github.com/jbaldwin/libcoro

#pragma once

#include <string>

#include "glaze/network/core.hpp"

namespace glz
{   
   enum struct poll_op : uint32_t {
      read,
      write,
      read_write
   };

   std::string to_string(poll_op op)
   {
      switch (op) {
      case poll_op::read:
         return "read";
      case poll_op::write:
         return "write";
      case poll_op::read_write:
         return "read_write";
      default:
         return "unknown";
      }
   }

   enum class poll_status {
      /// The poll operation was was successful.
      event,
      /// The poll operation timed out.
      timeout,
      /// The file descriptor had an error while polling.
      error,
      /// The file descriptor has been closed by the remote or an internal error/close.
      closed
   };

   std::string to_string(poll_status status)
   {
      switch (status) {
      case poll_status::event:
         return "event";
      case poll_status::timeout:
         return "timeout";
      case poll_status::error:
         return "error";
      case poll_status::closed:
         return "closed";
      default:
         return "unknown";
      }
   }
}
