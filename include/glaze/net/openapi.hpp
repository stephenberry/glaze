// Glaze Library - For the license information refer to glaze.hpp
#pragma once

#include <unordered_map>

#include "glaze/json.hpp"
#include "glaze/json/schema.hpp"
#include "glaze/net/http_router.hpp"

namespace glz
{
   struct openapi_info
   {
      std::string title = "API";
      std::string version = "1.0.0";
      std::optional<std::string> description{};
   };

   struct openapi_schema_ref
   {
      std::string ref{}; // Reference to schema like "#/components/schemas/TypeName"
   };

   struct openapi_media_type
   {
      std::optional<detail::schematic> schema{};
      std::optional<std::string> example{};
   };

   struct openapi_request_body
   {
      std::optional<std::string> description{};
      std::unordered_map<std::string, openapi_media_type> content{};
      bool required = false;
   };

   struct openapi_parameter
   {
      std::string name{};
      std::string in{}; // "path", "query", "header", or "cookie"
      std::optional<std::string> description{};
      bool required = false;
      std::optional<detail::schematic> schema{};
   };

   struct openapi_response
   {
      std::string description{};
      std::optional<std::unordered_map<std::string, openapi_media_type>> content{};
   };

   struct openapi_components
   {
      std::optional<std::unordered_map<std::string, detail::schematic>> schemas{};
   };

   struct openapi_operation
   {
      std::optional<std::vector<std::string>> tags{};
      std::optional<std::string> summary{};
      std::optional<std::string> operationId{};
      std::optional<std::vector<openapi_parameter>> parameters{};
      std::optional<openapi_request_body> requestBody{};
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
      std::optional<openapi_components> components{};
      std::unordered_map<std::string, openapi_path_item> paths{};
   };
}

template <>
struct glz::meta<glz::openapi_path_item>
{
   using T = glz::openapi_path_item;
   static constexpr auto value = glz::object(&T::get, &T::put, &T::post, "delete", &T::del, &T::patch);
};

namespace glz
{
   // Helper function to extract path parameters from a route path
   inline std::vector<openapi_parameter> extract_path_parameters(const std::string& path)
   {
      std::vector<openapi_parameter> parameters;

      size_t pos = 0;
      while ((pos = path.find(':', pos)) != std::string::npos) {
         size_t end = path.find('/', pos);
         if (end == std::string::npos) end = path.length();

         std::string param_name = path.substr(pos + 1, end - pos - 1);

         openapi_parameter param;
         param.name = param_name;
         param.in = "path";
         param.required = true;
         detail::schematic schema;
         schema.type = std::vector<std::string_view>{"string"};
         param.schema = schema;

         parameters.push_back(param);
         pos = end;
      }

      return parameters;
   }

   // Helper function to convert route_spec schema string to schematic
   inline std::optional<detail::schematic> parse_schema_string(const std::string& schema_str)
   {
      detail::schematic schema;
      auto ec = read_json(schema, schema_str);
      if (!ec) {
         return schema;
      }

      return std::nullopt;
   }

   // Generate OpenAPI operation from route information
   inline openapi_operation generate_operation(const std::string& path, http_method method, const route_spec& spec)
   {
      openapi_operation operation;

      // Set basic info
      if (!spec.description.empty()) {
         operation.summary = spec.description;
      }

      if (!spec.tags.empty()) {
         operation.tags = spec.tags;
      }

      // Add path parameters
      auto path_params = extract_path_parameters(path);
      if (!path_params.empty()) {
         operation.parameters = path_params;
      }

      // Add request body for POST, PUT, PATCH
      if (method == http_method::POST || method == http_method::PUT || method == http_method::PATCH) {
         if (spec.request_body_schema && !spec.request_body_schema->empty()) {
            openapi_request_body request_body;
            request_body.required = true;
            request_body.description = "Request body";

            openapi_media_type media_type;
            media_type.schema = parse_schema_string(*spec.request_body_schema);
            if (!media_type.schema) {
               // Fallback schema
               detail::schematic fallback_schema;
               fallback_schema.type = std::vector<std::string_view>{"object"};
               media_type.schema = fallback_schema;
            }

            request_body.content["application/json"] = media_type;
            operation.requestBody = request_body;
         }
      }

      // Add responses
      openapi_response success_response;
      success_response.description = "Success";

      if (spec.response_schema && !spec.response_schema->empty()) {
         openapi_media_type response_media;
         response_media.schema = parse_schema_string(*spec.response_schema);
         if (!response_media.schema) {
            // Fallback schema
            detail::schematic fallback_schema;
            fallback_schema.type = std::vector<std::string_view>{"object"};
            response_media.schema = fallback_schema;
         }

         std::unordered_map<std::string, openapi_media_type> content;
         content["application/json"] = response_media;
         success_response.content = content;
      }

      operation.responses["200"] = success_response;

      // Add standard error responses
      openapi_response bad_request;
      bad_request.description = "Bad Request";
      operation.responses["400"] = bad_request;

      openapi_response server_error;
      server_error.description = "Internal Server Error";
      operation.responses["500"] = server_error;

      return operation;
   }

   // Generate complete OpenAPI specification from router
   inline open_api generate_openapi_spec(const http_router& router, const openapi_info& info = {})
   {
      open_api spec;
      spec.info = info;

      // Initialize components
      openapi_components components;
      components.schemas = std::unordered_map<std::string, detail::schematic>{};

      // Process each route
      for (const auto& [path, methods] : router.routes) {
         openapi_path_item path_item;

         for (const auto& [method, route] : methods) {
            auto operation = generate_operation(path, method, route.spec);

            // Add schemas to components if they have type names
            if (route.spec.request_body_schema && route.spec.request_body_type_name) {
               auto schema = parse_schema_string(*route.spec.request_body_schema);
               if (schema) {
                  components.schemas.value()[*route.spec.request_body_type_name] = *schema;

                  // Update operation to use schema reference
                  if (operation.requestBody && operation.requestBody->content.count("application/json")) {
                     operation.requestBody->content["application/json"].schema = detail::schematic{};
                     // Note: In a real implementation, we'd set up a $ref here
                  }
               }
            }

            if (route.spec.response_schema && route.spec.response_type_name) {
               auto schema = parse_schema_string(*route.spec.response_schema);
               if (schema) {
                  components.schemas.value()[*route.spec.response_type_name] = *schema;

                  // Update operation to use schema reference
                  if (operation.responses.count("200") && operation.responses["200"].content) {
                     auto& content = operation.responses["200"].content.value();
                     if (content.count("application/json")) {
                        content["application/json"].schema = detail::schematic{};
                        // Note: we should set up a $ref here
                     }
                  }
               }
            }

            // Assign operation to appropriate method
            switch (method) {
            case http_method::GET:
               path_item.get = operation;
               break;
            case http_method::POST:
               path_item.post = operation;
               break;
            case http_method::PUT:
               path_item.put = operation;
               break;
            case http_method::DELETE:
               path_item.del = operation;
               break;
            case http_method::PATCH:
               path_item.patch = operation;
               break;
            default:
               break;
            }
         }

         spec.paths[path] = path_item;
      }

      // Only add components if we have schemas
      if (components.schemas && !components.schemas->empty()) {
         spec.components = components;
      }

      return spec;
   }

   // Convert OpenAPI spec to JSON string
   inline std::string openapi_to_json(const open_api& spec)
   {
      std::string result;
      auto ec = write_json(spec, result);
      if (ec) {
         return "{}"; // fallback on error
      }
      return result;
   }
}
