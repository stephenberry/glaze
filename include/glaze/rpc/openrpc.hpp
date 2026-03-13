// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <string>
#include <vector>

#include "glaze/core/common.hpp"

namespace glz
{
   struct openrpc_info
   {
      std::string title = "API";
      std::string version = "1.0.0";
      std::optional<std::string> description{};
   };

   struct openrpc_content_descriptor
   {
      std::string name{};
      std::optional<std::string> description{};
      raw_json schema{"{}"}; // Pre-serialized JSON Schema
      std::optional<bool> required{};
   };

   struct openrpc_method
   {
      std::string name{};
      std::optional<std::string> description{};
      std::vector<openrpc_content_descriptor> params{};
      std::optional<openrpc_content_descriptor> result{};
   };

   struct open_rpc
   {
      std::string openrpc = "1.3.2";
      openrpc_info info{};
      std::vector<openrpc_method> methods{};
   };

   // Metadata captured at registration time for generating OpenRPC spec
   struct method_metadata
   {
      std::string name{};
      std::string params_schema{}; // JSON Schema for input type (empty if no params)
      std::string result_schema{}; // JSON Schema for output type (empty if void)
      bool params_required{false}; // true for functions that require params
   };
}
