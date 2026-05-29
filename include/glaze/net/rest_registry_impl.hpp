// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/opts.hpp" // For REST constant
#include "glaze/glaze.hpp"
#include "glaze/json/schema.hpp"
#include "glaze/net/http_router.hpp"
#include "glaze/rpc/repe/repe.hpp" // For protocol_storage template

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

   namespace detail
   {
      // Maps a reflected handler's return type to the type that actually appears in a
      // successful REST response body: glz::expected<T, E> unwraps to T so the generated
      // response schema describes the value rather than the expected wrapper.
      template <class R>
      struct rest_success_type
      {
         using type = R;
      };
      template <class T, class E>
      struct rest_success_type<expected<T, E>>
      {
         using type = T;
      };
      template <class R>
      using rest_success_type_t = typename rest_success_type<std::remove_cvref_t<R>>::type;
   }

   // Implementation for REST protocol
   template <auto Opts>
   struct registry_impl<Opts, REST>
   {
      using enum http_method;

      // --- glz::expected -> HTTP translation for reflected handlers --------------------
      // A reflected handler may signal failure by returning glz::expected<T, E> instead of
      // throwing (required under -fno-exceptions, convenient otherwise). These helpers are
      // the REST peer of repe::set_handler_error / jsonrpc::set_handler_error: the success
      // value is serialized as the body, while the error is mapped to an HTTP status and a
      // glz::http_error body. Note: rpc::error/error_e (full JSON-RPC fidelity) are not
      // accepted here, since glaze/net must not depend on the glaze/ext JSON-RPC types;
      // use glz::http_error for explicit status control.

      // Map a Glaze error_code to the most appropriate HTTP status.
      static int http_status_for(error_code ec)
      {
         switch (ec) {
         case error_code::invalid_body:
         case error_code::parse_error:
         case error_code::invalid_query:
         case error_code::syntax_error:
         case error_code::constraint_violated:
            return 400;
         case error_code::method_not_found:
         case error_code::key_not_found:
         case error_code::nonexistent_json_ptr:
            return 404;
         case error_code::timeout:
            return 504;
         default:
            return 500;
         }
      }

      // Translate a handler's glz::expected error into an HTTP status + body. Supported
      // error types:
      //   - glz::http_error -> its explicit status and message
      //   - glz::error_code -> http_status_for(ec) with the code's name
      //   - glz::error_ctx  -> 500 with the formatted context
      //   - string-like     -> 500 with the message
      template <class E>
      static void set_handler_error(response& res, E&& error)
      {
         using DE = std::remove_cvref_t<E>;
         if constexpr (std::same_as<DE, http_error>) {
            res.status(error.status).template body<Opts>(std::forward<E>(error));
         }
         else if constexpr (std::same_as<DE, error_code>) {
            const int s = http_status_for(error);
            res.status(s).template body<Opts>(http_error{s, format_error(error)});
         }
         else if constexpr (std::same_as<DE, error_ctx>) {
            res.status(500).template body<Opts>(http_error{500, format_error(error)});
         }
         else {
            static_assert(std::convertible_to<DE, std::string_view>,
                          "A REST handler's glz::expected error type must be glz::http_error, glz::error_code, "
                          "glz::error_ctx, or convertible to std::string_view.");
            res.status(500).template body<Opts>(http_error{500, std::string(std::string_view(error))});
         }
      }

      // Serialize a (possibly glz::expected) handler result into the response. On a
      // glz::expected, the success value is serialized (204 for an expected<void, E>);
      // the error is translated by set_handler_error. Plain results serialize as before.
      template <class R>
      static void write_result(response& res, R&& result)
      {
         using V = std::remove_cvref_t<R>;
         if constexpr (is_expected<V>) {
            if (result.has_value()) {
               if constexpr (std::is_void_v<typename V::value_type>) {
                  res.status(204); // No Content
               }
               else {
                  res.template body<Opts>(*std::forward<R>(result));
               }
            }
            else {
               set_handler_error(res, std::forward<R>(result).error());
            }
         }
         else {
            res.template body<Opts>(std::forward<R>(result));
         }
      }

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

      // Helper to generate JSON schema for a type
      template <class T>
      static std::string generate_schema_for_type()
      {
         std::string schema_str;
         auto ec = write_json_schema<T>(schema_str);
         if (ec) {
            return "{}";
         }
         return schema_str;
      }

      template <class T>
      static std::string get_type_name()
      {
         if constexpr (string_t<T>) {
            return "string";
         }
         else if constexpr (num_t<T>) {
            return "number";
         }
         else if constexpr (readable_array_t<T> || tuple_t<T> || is_std_tuple<T>) {
            return "array";
         }
         else if constexpr (boolean_like<T>) {
            return "boolean";
         }
         else if constexpr (glaze_object_t<T> || reflectable<T> || writable_map_t<T>) {
            return "object";
         }
         else {
            return "";
         }
      }

      // Helper to create route spec with type information
      template <class RequestType = void, class ResponseType = void>
      static route_spec create_route_spec_with_types(const std::string& description = "",
                                                     const std::vector<std::string>& tags = {})
      {
         route_spec spec;
         spec.description = description;
         spec.tags = tags;

         // A void response means the handler replies 204 No Content (update endpoints,
         // void functions, and expected<void, E> handlers, which all pass ResponseType =
         // void here); a non-void response is serialized with a 200 body.
         spec.success_status = std::same_as<ResponseType, void> ? 204 : 200;

         if constexpr (!std::same_as<RequestType, void>) {
            spec.request_body_schema = generate_schema_for_type<RequestType>();
            spec.request_body_type_name = get_type_name<RequestType>();
         }

         if constexpr (!std::same_as<ResponseType, void>) {
            spec.response_schema = generate_schema_for_type<ResponseType>();
            spec.response_type_name = get_type_name<ResponseType>();
         }

         return spec;
      }

      // Record the error response for a reflected handler whose return type is the
      // (possibly glz::expected) Result. set_handler_error always serializes a
      // glz::http_error body, so that is the documented error schema; the response key
      // is fixed at "500" for string/error_ctx errors and "default" otherwise, since an
      // error_code maps to several statuses and a glz::http_error carries any status.
      template <class Result>
      static void describe_error_response(route_spec& spec)
      {
         using R = std::remove_cvref_t<Result>;
         if constexpr (is_expected<R>) {
            using E = typename R::error_type;
            spec.error_response_schema = generate_schema_for_type<http_error>();
            spec.error_response_type_name = "http_error";
            if constexpr (std::same_as<E, error_ctx> || std::convertible_to<E, std::string_view>) {
               spec.error_status_key = "500";
            }
            else {
               spec.error_status_key = "default";
            }
         }
      }

      template <class T, class RegistryType>
      static void register_endpoint(const sv path, T& value, RegistryType& reg)
      {
         std::string rest_path = convert_to_rest_path(path);

         // GET handler for the entire object
         auto get_spec = create_route_spec_with_types<void, T>("Get " + get_type_name<T>(), {"data"});
         reg.endpoints.route(
            GET, rest_path, [&value](const request& /*req*/, response& res) { res.template body<Opts>(value); },
            get_spec);

         // PUT handler for updating the entire object
         auto put_spec = create_route_spec_with_types<T, void>("Update " + get_type_name<T>(), {"data"});
         reg.endpoints.route(
            PUT, rest_path,
            [&value](const request& req, response& res) {
               // Parse the JSON request body
               auto ec = read<Opts>(value, req.body);
               if (ec) {
                  res.status(400).body("Invalid request body: " + format_error(ec, req.body));
                  return;
               }

               res.status(204); // No Content
            },
            put_spec);
      }

      template <class Func, class Result, class RegistryType>
      static void register_function_endpoint(const sv path, Func& func, RegistryType& reg)
      {
         std::string rest_path = convert_to_rest_path(path);

         using ResponseType = detail::rest_success_type_t<Result>;
         auto get_spec =
            create_route_spec_with_types<void, ResponseType>("Get " + get_type_name<ResponseType>(), {"data"});
         describe_error_response<Result>(get_spec);
         // GET handler for functions
         reg.endpoints.route(
            GET, rest_path,
            [&func](const request& /*req*/, response& res) {
               if constexpr (std::same_as<Result, void>) {
                  func();
                  res.status(204); // No Content
               }
               else {
                  write_result(res, func());
               }
            },
            get_spec);
      }

      template <class Func, class Params, class RegistryType>
      static void register_param_function_endpoint(const sv path, Func& func, RegistryType& reg)
      {
         std::string rest_path = convert_to_rest_path(path);

         using Result = std::invoke_result_t<decltype(func), Params>;
         using ResponseType = detail::rest_success_type_t<Result>;
         auto post_spec =
            create_route_spec_with_types<Params, ResponseType>("Create " + get_type_name<ResponseType>(), {"data"});
         describe_error_response<Result>(post_spec);
         // POST handler for functions with parameters
         reg.endpoints.route(
            POST, rest_path,
            [&func](const request& req, response& res) {
               // Parse the JSON request body
               Params params_result{};
               auto ec = read<Opts>(params_result, req.body);
               if (bool(ec)) {
                  res.status(400).body("Invalid request body: " + format_error(ec, req.body));
                  return;
               }

               if constexpr (std::same_as<Result, void>) {
                  func(std::move(params_result));
                  res.status(204); // No Content
               }
               else {
                  write_result(res, func(std::move(params_result)));
               }
            },
            post_spec);
      }

      template <class Obj, class RegistryType>
      static void register_object_endpoint(const sv path, Obj& obj, RegistryType& reg)
      {
         std::string rest_path = convert_to_rest_path(path);

         auto get_spec = create_route_spec_with_types<void, Obj>("Get " + get_type_name<Obj>(), {"data"});
         // GET handler for nested objects
         reg.endpoints.route(
            GET, rest_path, [&obj](const request& /*req*/, response& res) { res.body<Opts>(obj); }, get_spec);

         auto put_spec = create_route_spec_with_types<Obj, void>("Update " + get_type_name<Obj>(), {"data"});
         // PUT handler for updating nested objects
         reg.endpoints.route(
            PUT, rest_path,
            [&obj](const request& req, response& res) {
               // Parse the JSON request body
               auto ec = read<Opts>(obj, req.body);
               if (ec) {
                  res.status(400).body("Invalid request body: " + format_error(ec, req.body));
                  return;
               }

               res.status(204); // No Content
            },
            put_spec);
      }

      template <class Value, class RegistryType>
      static void register_value_endpoint(const sv path, Value& value, RegistryType& reg)
      {
         std::string rest_path = convert_to_rest_path(path);

         auto get_spec = create_route_spec_with_types<void, Value>("Get " + get_type_name<Value>(), {"data"});
         // GET handler for values
         reg.endpoints.route(
            GET, rest_path, [&value](const request& /*req*/, response& res) { res.body<Opts>(value); }, get_spec);

         auto put_spec = create_route_spec_with_types<Value, void>("Update " + get_type_name<Value>(), {"data"});
         // PUT handler for updating values
         reg.endpoints.route(
            PUT, rest_path,
            [&value](const request& req, response& res) {
               // Parse the JSON request body
               auto ec = read<Opts>(value, req.body);
               if (ec) {
                  res.status(400).body("Invalid request body: " + format_error(ec, req.body));
                  return;
               }

               res.status(204); // No Content
            },
            put_spec);
      }

      template <class Var, class RegistryType>
      static void register_variable_endpoint(const sv path, Var& var, RegistryType& reg)
      {
         std::string rest_path = convert_to_rest_path(path);

         auto get_spec = create_route_spec_with_types<void, Var>("Get " + get_type_name<Var>(), {"data"});
         // GET handler for variables
         reg.endpoints.route(
            GET, rest_path, [&var](const request& /*req*/, response& res) { res.body<Opts>(var); }, get_spec);

         auto put_spec = create_route_spec_with_types<Var, void>("Update " + get_type_name<Var>(), {"data"});
         // PUT handler for updating variables
         reg.endpoints.route(
            PUT, rest_path,
            [&var](const request& req, response& res) {
               // Parse the JSON request body
               auto ec = read<Opts>(var, req.body);
               if (ec) {
                  res.status(400).body("Invalid request body: " + format_error(ec, req.body));
                  return;
               }

               res.status(204); // No Content
            },
            put_spec);
      }

      template <class T, class F, class Ret, class RegistryType>
      static void register_member_function_endpoint(const sv path, T& value, F func, RegistryType& reg)
      {
         std::string rest_path = convert_to_rest_path(path);

         using ResponseType = detail::rest_success_type_t<Ret>;
         auto get_spec =
            create_route_spec_with_types<void, ResponseType>("Get " + get_type_name<ResponseType>(), {"data"});
         describe_error_response<Ret>(get_spec);
         // GET handler for member functions with no args
         reg.endpoints.route(
            GET, rest_path,
            [&value, func](const request& /*req*/, response& res) {
               if constexpr (std::same_as<Ret, void>) {
                  (value.*func)();
                  res.status(204); // No Content
               }
               else {
                  write_result(res, (value.*func)());
               }
            },
            get_spec);
      }

      template <class T, class F, class Input, class Ret, class RegistryType>
      static void register_member_function_with_params_endpoint(const sv path, T& value, F func, RegistryType& reg)
      {
         std::string rest_path = convert_to_rest_path(path);

         using ResponseType = detail::rest_success_type_t<Ret>;
         auto post_spec =
            create_route_spec_with_types<Input, ResponseType>("Create " + get_type_name<ResponseType>(), {"data"});
         describe_error_response<Ret>(post_spec);
         // POST handler for member functions with args
         reg.endpoints.route(
            POST, rest_path,
            [&value, func](const request& req, response& res) {
               // Parse the JSON request body
               Input params_result{};
               auto ec = read<Opts>(params_result, req.body);
               if (bool(ec)) {
                  res.status(400).body("Invalid request body: " + format_error(ec, req.body));
                  return;
               }

               if constexpr (std::same_as<Ret, void>) {
                  (value.*func)(std::move(params_result));
                  res.status(204); // No Content
               }
               else {
                  write_result(res, (value.*func)(std::move(params_result)));
               }
            },
            post_spec);
      }
   };
}
