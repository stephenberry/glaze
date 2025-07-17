// Glaze Library - For the license information refer to glaze.hpp
#pragma once

#include <unordered_map>

#include "glaze/json.hpp"
#include "glaze/json/schema.hpp"

namespace glz
{
   struct openapi_info
   {
      std::string title = "API";
      std::string version = "1.0.0";
      std::optional<std::string> description{};
   };

   // Use glz::detail::schematic directly for OpenAPI schemas
   using openapi_schema = detail::schematic;

   struct openapi_parameter
   {
      std::string name{};
      std::string in{}; // "path", "query", "header", or "cookie"
      std::optional<std::string> description{};
      bool required = false;
      std::optional<openapi_schema> schema{};
   };

   struct openapi_media_type
   {
      std::optional<openapi_schema> schema{};
   };

   struct openapi_response
   {
      std::string description{};
      std::optional<std::map<std::string, openapi_media_type>> content{};
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

   struct openapi_components
   {
      std::optional<std::map<std::string, openapi_schema>> schemas{};
   };

   struct open_api
   {
      std::string openapi = "3.0.3";
      openapi_info info{};
      std::unordered_map<std::string, openapi_path_item> paths{};
      std::optional<openapi_components> components{};
   };
}


template <>
struct glz::meta<glz::openapi_parameter>
{
   using T = glz::openapi_parameter;
   static constexpr auto value = glz::object("name", &T::name, 
                                            "in", &T::in, 
                                            "description", &T::description, 
                                            "required", &T::required, 
                                            "schema", &T::schema);
};

template <>
struct glz::meta<glz::openapi_media_type>
{
   using T = glz::openapi_media_type;
   static constexpr auto value = glz::object("schema", &T::schema);
};

template <>
struct glz::meta<glz::openapi_response>
{
   using T = glz::openapi_response;
   static constexpr auto value = glz::object("description", &T::description, 
                                            "content", &T::content);
};

template <>
struct glz::meta<glz::openapi_components>
{
   using T = glz::openapi_components;
   static constexpr auto value = glz::object("schemas", &T::schemas);
};

template <>
struct glz::meta<glz::open_api>
{
   using T = glz::open_api;
   static constexpr auto value = glz::object("openapi", &T::openapi, 
                                            "info", &T::info, 
                                            "paths", &T::paths, 
                                            "components", &T::components);
};

template <>
struct glz::meta<glz::openapi_path_item>
{
   using T = glz::openapi_path_item;
   static constexpr auto value = glz::object(&T::get, &T::put, &T::post, "delete", &T::del, &T::patch);
};
