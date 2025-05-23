// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/glaze.hpp"
#include "glaze/rest/http_router.hpp"

namespace glz
{
   // Forward declaration of the registry template
   template <auto Opts, uint32_t Proto>
   struct registry;
   
   template <>
   struct protocol_storage<REST>
   {
      using type = http_router;
   };

   // Implementation for REST protocol
   template <auto Opts>
   struct registry_impl<Opts, REST>
   {
      using enum http_method;
      
      // Helper method to convert a JSON pointer path to a REST path
      static std::string convert_to_rest_path(sv json_pointer_path)
      {
         // Make a copy of the path
         std::string rest_path(json_pointer_path);

         // Remove trailing slashes
         if (!rest_path.empty() && rest_path.back() == '/') {
            rest_path.pop_back();
         }

         return rest_path;
      }

      template <class T, class RegistryType>
      static void register_endpoint(sv path, T& value, RegistryType& reg)
      {
         std::string rest_path = convert_to_rest_path(path);

         // GET handler for the entire object
         reg.endpoints.route(GET, rest_path,
                             [&value](const request& /*req*/, response& res) { res.json(value); });

         // PUT handler for updating the entire object
         reg.endpoints.route(PUT, rest_path, [&value](const request& req, response& res) {
            // Parse the JSON request body
            auto ec = read_json(value, req.body);
            if (ec) {
               res.status(400).body("Invalid request body: " + format_error(ec, req.body));
               return;
            }

            res.status(204); // No Content
         });
      }

      template <class Func, class Result, class RegistryType>
      static void register_function_endpoint(sv path, Func& func, RegistryType& reg)
      {
         std::string rest_path = convert_to_rest_path(path);

         // GET handler for functions
         reg.endpoints.route(GET, rest_path, [&func](const request& /*req*/, response& res) {
            if constexpr (std::same_as<Result, void>) {
               func();
               res.status(204); // No Content
            }
            else {
               auto result = func();
               res.json(result);
            }
         });
      }

      template <class Func, class Params, class RegistryType>
      static void register_param_function_endpoint(sv path, Func& func, RegistryType& reg)
      {
         std::string rest_path = convert_to_rest_path(path);

         // POST handler for functions with parameters
         reg.endpoints.route(POST, rest_path, [&func](const request& req, response& res) {
            // Parse the JSON request body
            auto params_result = read_json<Params>(req.body);
            if (!params_result) {
               res.status(400).body("Invalid request body: " + format_error(params_result, req.body));
               return;
            }

            using Result = std::invoke_result_t<decltype(func), Params>;

            if constexpr (std::same_as<Result, void>) {
               func(params_result.value());
               res.status(204); // No Content
            }
            else {
               auto result = func(params_result.value());
               res.json(result);
            }
         });
      }

      template <class Obj, class RegistryType>
      static void register_object_endpoint(sv path, Obj& obj, RegistryType& reg)
      {
         std::string rest_path = convert_to_rest_path(path);

         // GET handler for nested objects
         reg.endpoints.route(GET, rest_path,
                             [&obj](const request& /*req*/, response& res) { res.json(obj); });

         // PUT handler for updating nested objects
         reg.endpoints.route(PUT, rest_path, [&obj](const request& req, response& res) {
            // Parse the JSON request body
            auto ec = read_json(obj, req.body);
            if (ec) {
               res.status(400).body("Invalid request body: " + format_error(ec, req.body));
               return;
            }

            res.status(204); // No Content
         });
      }

      template <class Value, class RegistryType>
      static void register_value_endpoint(sv path, Value& value, RegistryType& reg)
      {
         std::string rest_path = convert_to_rest_path(path);

         // GET handler for values
         reg.endpoints.route(GET, rest_path,
                             [&value](const request& /*req*/, response& res) { res.json(value); });

         // PUT handler for updating values
         reg.endpoints.route(PUT, rest_path, [&value](const request& req, response& res) {
            // Parse the JSON request body
            auto ec = read_json(value, req.body);
            if (!ec) {
               res.status(400).body("Invalid request body: " + format_error(ec, req.body));
               return;
            }

            res.status(204); // No Content
         });
      }

      template <class Var, class RegistryType>
      static void register_variable_endpoint(sv path, Var& var, RegistryType& reg)
      {
         std::string rest_path = convert_to_rest_path(path);

         // GET handler for variables
         reg.endpoints.route(GET, rest_path,
                             [&var](const request& /*req*/, response& res) { res.json(var); });

         // PUT handler for updating variables
         reg.endpoints.route(PUT, rest_path, [&var](const request& req, response& res) {
            // Parse the JSON request body
            auto ec = read_json(var, req.body);
            if (!ec) {
               res.status(400).body("Invalid request body: " + format_error(ec, req.body));
               return;
            }

            res.status(204); // No Content
         });
      }

      template <class T, class F, class Ret, class RegistryType>
      static void register_member_function_endpoint(sv path, T& value, F func, RegistryType& reg)
      {
         std::string rest_path = convert_to_rest_path(path);

         // GET handler for member functions with no args
         reg.endpoints.route(GET, rest_path, [&value, func](const request& /*req*/, response& res) {
            if constexpr (std::same_as<Ret, void>) {
               (value.*func)();
               res.status(204); // No Content
            }
            else {
               auto result = (value.*func)();
               res.json(result);
            }
         });
      }

      template <class T, class F, class Input, class Ret, class RegistryType>
      static void register_member_function_with_params_endpoint(sv path, T& value, F func, RegistryType& reg)
      {
         std::string rest_path = convert_to_rest_path(path);

         // POST handler for member functions with args
         reg.endpoints.route(POST, rest_path, [&value, func](const request& req, response& res) {
            // Parse the JSON request body
            auto params_result = read_json<Input>(req.body);
            if (!params_result) {
               res.status(400).body("Invalid request body: " + format_error(params_result, req.body));
               return;
            }

            if constexpr (std::same_as<Ret, void>) {
               (value.*func)(params_result.value());
               res.status(204); // No Content
            }
            else {
               auto result = (value.*func)(std::move(params_result.value()));
               res.json(result);
            }
         });
      }
   };
}
