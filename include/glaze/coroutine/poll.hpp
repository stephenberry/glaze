// Glaze Library
// For the license information refer to glaze.hpp

// Modified from the awesome: https://github.com/jbaldwin/libcoro

#pragma once

#include <string>

namespace glz
{
   // TODO: handle poll flags
   constexpr auto poll_in = 0b00000001;
   constexpr auto poll_out = 0b00000010;
   
   enum struct poll_op : uint64_t {
      read = poll_in,
      write = poll_out,
      read_write = poll_in | poll_out
   };

   inline bool poll_op_readable(poll_op op) { return (uint64_t(op) & poll_in); }

   inline bool poll_op_writeable(poll_op op) { return (uint64_t(op) & poll_out); }

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
