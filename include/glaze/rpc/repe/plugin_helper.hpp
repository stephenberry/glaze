// Glaze Library
// For the license information refer to glaze.hpp

// C++ helper for implementing REPE plugins
// Provides common functionality to reduce code duplication

#pragma once

#include "glaze/rpc/repe/plugin.h"

#include "glaze/rpc/repe/buffer.hpp"
#include "glaze/rpc/repe/header.hpp"
#include "glaze/rpc/registry.hpp"

#include <string>

namespace glz::repe
{
   // Thread-local response buffer for plugin implementations
   // Note: This buffer grows as needed but does not shrink during the thread's lifetime
   inline thread_local std::string plugin_response_buffer;

   // Create an error response with proper REPE format
   inline void plugin_error_response(error_code ec, const std::string& error_msg, uint64_t id = 0)
   {
      message response{};
      response.header.spec = repe_magic;
      response.header.version = 1;
      response.header.id = id;
      encode_error(ec, response, error_msg);
      finalize_header(response);
      to_buffer(response, plugin_response_buffer);
   }

   // Common plugin call implementation
   // Dispatches a REPE request to a registry and returns the response
   // Note: Plugin initialization should be done via repe_plugin_init before any calls
   template <typename Registry>
   repe_buffer plugin_call(Registry& registry, const char* request, uint64_t request_size)
   {
      // Deserialize request
      message request_msg{};
      auto ec = from_buffer(request, request_size, request_msg);
      if (ec != error_code::none) {
         plugin_error_response(error_code::parse_error, "Failed to deserialize REPE request");
         return {plugin_response_buffer.data(), plugin_response_buffer.size()};
      }

      // Dispatch to registry
      message response_msg{};
      try {
         registry.call(request_msg, response_msg);
      }
      catch (const std::exception& e) {
         plugin_error_response(error_code::invalid_call, e.what(), request_msg.header.id);
         return {plugin_response_buffer.data(), plugin_response_buffer.size()};
      }
      catch (...) {
         plugin_error_response(error_code::invalid_call, "Unknown error during call", request_msg.header.id);
         return {plugin_response_buffer.data(), plugin_response_buffer.size()};
      }

      to_buffer(response_msg, plugin_response_buffer);
      return {plugin_response_buffer.data(), plugin_response_buffer.size()};
   }

} // namespace glz::repe
