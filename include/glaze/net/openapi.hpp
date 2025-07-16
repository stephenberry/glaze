// Glaze Library - For the license information refer to glaze.hpp
#pragma once

#include <unordered_map>

#include "glaze/json.hpp"

namespace glz
{
   struct openapi_info
   {
      std::string title = "API";
      std::string version = "1.0.0";
      std::optional<std::string> description{};
   };

   struct openapi_parameter
   {
      std::string name{};
      std::string in{}; // "path", "query", "header", or "cookie"
      std::optional<std::string> description{};
      bool required = false;
   };

   struct openapi_response
   {
      std::string description{};
   };

   struct openapi_operation
   {
      std::optional<std::vector<std::string>> tags{};
      std::optional<std::string> summary{};
      std::optional<std::string> operationId{};
      std::optional<std::vector<openapi_parameter>> parameters{};
      std::unordered_map<std::string, openapi_response> responses{};
   };

   struct openapi_path_item
   {
      std::optional<openapi_operation> get{};
      std::optional<openapi_operation> put{};
      std::optional<openapi_operation> post{};
      std::optional<openapi_operation> del{}; // Mapped to "delete" in JSON
      std::optional<openapi_operation> patch{};
   };

   struct open_api
   {
      std::string openapi = "3.0.3";
      openapi_info info{};
      std::unordered_map<std::string, openapi_path_item> paths{};
   };
}

template <>
struct glz::meta<glz::openapi_path_item>
{
   using T = glz::openapi_path_item;
   static constexpr auto value = glz::object(&T::get, &T::put, &T::post, "delete", &T::del, &T::patch);
};
