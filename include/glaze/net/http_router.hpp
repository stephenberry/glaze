// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <algorithm>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <optional>
#include <source_location>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "glaze/json/generic.hpp"
#include "glaze/net/http.hpp"
#include "glaze/net/url.hpp"
#include "glaze/util/key_transformers.hpp"

namespace glz
{
   // Request context object
   struct request
   {
      http_method method{};
      std::string target{}; // Full request target (path + query string)
      std::string path{}; // Path component only (without query string)
      std::unordered_map<std::string, std::string> params{}; // Path parameters (e.g., :id)
      std::unordered_map<std::string, std::string> query{}; // Query parameters (e.g., ?limit=10)
      std::unordered_map<std::string, std::string> headers{};
      std::string body{};
      std::string remote_ip{};
      uint16_t remote_port{};
   };

   // Serialized to the response body when response::body<Opts>(value) fails
   // to write `value`. Declared at namespace scope so glaze's reflection can
   // name the type; function-local types are not reflectable on GCC.
   struct glz_write_error
   {
      std::string_view error{"glaze write failure"};
      uint32_t code{};
      std::string_view message{};
   };

   // Response builder
   struct response
   {
      enum header_flag : uint8_t {
         has_content_length = 1,
         has_date = 2,
         has_server = 4,
         has_connection = 8,
      };

      int status_code = 200;
      std::unordered_map<std::string, std::string> response_headers{};
      std::string response_body{};
      uint8_t user_headers_set{};

      inline response& status(int code)
      {
         status_code = code;
         return *this;
      }

      inline response& header(std::string_view name, std::string_view value)
      {
         // Convert header name to lowercase for case-insensitive lookups (RFC 7230)
         std::string key(name);
         for (auto& c : key) c = ascii_tolower(c);

         // Track which default headers the user has set
         if (key == "content-length")
            user_headers_set |= has_content_length;
         else if (key == "date")
            user_headers_set |= has_date;
         else if (key == "server")
            user_headers_set |= has_server;
         else if (key == "connection")
            user_headers_set |= has_connection;

         response_headers[std::move(key)] = std::string(value);
         return *this;
      }

      inline response& body(std::string_view content)
      {
         response_body.assign(content.data(), content.size());
         return *this;
      }

      // Use glz::opts for format deduction and serialization
      // my_response.res<Opts>(value);
      template <auto Opts, class T>
      response& body(T&& value)
      {
         if constexpr (Opts.format == JSON) {
            content_type("application/json");
         }
         else if constexpr (Opts.format == BEVE) {
            content_type("application/beve");
         }
         auto ec = glz::write<Opts>(std::forward<T>(value), response_body);
         if (ec) {
            // The body may have been partially written before the error
            // fired; clear it so the response is never a mix of a partial
            // success payload and the error report.
            response_body.clear();

            // Serialize a structured glz_write_error object instead of the
            // legacy hardcoded placeholder. The original error_code and
            // any custom_error_message are surfaced so callers can debug
            // failed writes without having to reproduce them locally.
            glz_write_error info{
               .code = uint32_t(ec.ec),
               .message = ec.custom_error_message,
            };
            if (auto write_ec = glz::write_json(info, response_body); write_ec) {
               // Re-erroring on a tiny struct is essentially impossible,
               // but keep a safe static fallback so the response body is
               // never left empty after a failed write attempt.
               response_body = R"({"error":"glaze write failure"})";
            }
         }
         return *this;
      }

      // Reset response for reuse, preserving allocated capacity
      void clear()
      {
         status_code = 200;
         response_headers.clear();
         response_body.clear(); // preserves capacity
         user_headers_set = 0;
      }

      inline response& content_type(std::string_view type) { return header("content-type", type); }

      // JSON response helper using Glaze
      template <class T = glz::generic>
      response& json(T&& value)
      {
         content_type("application/json");
         auto ec = glz::write_json(std::forward<T>(value), response_body);
         if (ec) {
            response_body = R"({"error":"glz::write_json error"})"; // rare that this would ever happen
         }
         return *this;
      }
   };

   using handler = std::function<void(const request&, response&)>;
   using async_handler = std::function<std::future<void>(const request&, response&)>;
   using error_handler = std::function<void(std::error_code, std::source_location)>;

   // Forward declarations for streaming and WebSocket support.
   // Full definitions live in glaze/net/http_server.hpp and glaze/net/websocket_connection.hpp.
   // Forward declarations are sufficient here because streaming_handler holds streaming_response
   // by reference and websocket_handler holds websocket_server through std::shared_ptr.
   struct streaming_response;
   struct websocket_server;

   // Streaming handler signature. Streaming routes take over the connection lifecycle
   // (no keep-alive) and write chunked responses through streaming_response.
   using streaming_handler = std::function<void(request&, streaming_response&)>;

   // WebSocket handler value: a websocket_server instance is bound to a path. The HTTP
   // server detects the upgrade handshake (Upgrade: websocket) and dispatches to the
   // matching server entry.
   using websocket_handler = std::shared_ptr<websocket_server>;

   /**
    * @brief Parameter constraint for route validation
    *
    * Defines validation rules for route parameters using a validation function.
    */
   struct param_constraint
   {
      /**
       * @brief Human-readable description of the constraint
       *
       * Used for OpenAPI parameter descriptions and debugging output.
       */
      std::string description{};

      /**
       * @brief Validation function for parameter values
       *
       * This function is used to validate parameter values. It should return true if the parameter value is valid,
       * false otherwise.
       */
      std::function<bool(std::string_view)> validation = [](std::string_view) { return true; };
   };

   /**
    * @brief An entry for a registered route.
    */
   struct route_spec
   {
      std::string description{};
      std::vector<std::string> tags{};
      std::unordered_map<std::string, param_constraint> constraints{};

      // Type information for schema generation
      std::optional<std::string> request_body_schema{};
      std::optional<std::string> response_schema{};
      std::optional<std::string> request_body_type_name{};
      std::optional<std::string> response_type_name{};
   };

   /**
    * @brief Generic radix-tree route table parameterized over the stored handler type.
    *
    * route_table provides path matching with support for static, parameterized (":param"),
    * and wildcard ("*name") segments. It is used as a building block by basic_http_router
    * to store normal routes (typed by Handler), streaming routes (streaming_handler), and
    * WebSocket routes (websocket_handler).
    *
    * @tparam H The stored handler type. Must be default-constructible (the empty value
    *           returned by match() when no route matches is H{}).
    */
   template <class H>
   struct route_table
   {
      /**
       * @brief Stored route entry: handler plus optional spec for schema/constraints.
       */
      struct route_entry
      {
         H handle{};
         route_spec spec{};
      };

      /**
       * @brief Node in the radix tree routing structure
       *
       * Each node represents a segment of a path, which can be a static string,
       * a parameter (prefixed with ":"), or a wildcard (prefixed with "*").
       */
      struct radix_node
      {
         std::string segment;
         bool is_parameter = false;
         bool is_wildcard = false;
         std::string parameter_name;
         std::unordered_map<std::string, std::unique_ptr<radix_node>> static_children;
         std::unique_ptr<radix_node> parameter_child;
         std::unique_ptr<radix_node> wildcard_child;
         std::unordered_map<http_method, H> handlers;
         std::unordered_map<http_method, std::unordered_map<std::string, param_constraint>> constraints;
         bool is_endpoint = false;
         std::string full_path;

         std::string to_string() const
         {
            std::string result;
            result.reserve(80 + segment.size() + full_path.size());

            result.append("Node[");
            result.append(is_parameter ? "PARAM:" : (is_wildcard ? "WILD:" : ""));
            result.append(segment);
            result.append(", endpoint=");
            result.append(is_endpoint ? "true" : "false");
            result.append(", children=");
            result.append(std::to_string(static_children.size()));
            result.append(parameter_child ? "+param" : "");
            result.append(wildcard_child ? "+wild" : "");
            result.append(", full_path=");
            result.append(full_path);
            result.append("]");
            return result;
         }
      };

      /**
       * @brief Map of registered routes, keyed by full path then HTTP method.
       *
       * Public for backward compatibility and to support introspection (e.g., the
       * OpenAPI generator iterates this map).
       */
      std::unordered_map<std::string, std::unordered_map<http_method, route_entry>> routes;

      /**
       * @brief Split a path into segments
       *
       * Splits a path like "/users/:id/profile" into ["users", ":id", "profile"].
       * This is the canonical implementation; basic_http_router::split_path is a
       * thin backward-compatible wrapper that delegates here.
       *
       * @param path The path to split
       * @return Vector of path segments
       */
      static std::vector<std::string> split_path(std::string_view path)
      {
         std::vector<std::string> segments;
         segments.reserve(std::count(path.begin(), path.end(), '/') + 1);

         size_t start = 0;
         while (start < path.size()) {
            if (path[start] == '/') {
               start++;
               continue;
            }

            size_t end = path.find('/', start);
            if (end == std::string::npos) end = path.size();

            segments.push_back(std::string(path.substr(start, end - start)));
            start = end;
         }

         return segments;
      }

      /**
       * @brief Register a route in the table.
       *
       * @param method The HTTP method (GET, POST, etc.)
       * @param path The route path, may contain ":param" or trailing "*name" wildcards.
       * @param handle The handler to associate with the route.
       * @param spec Optional spec for the route.
       */
      void add(http_method method, std::string_view path, H handle, const route_spec& spec = {})
      {
         try {
            // Install in the radix tree first. add_route can throw on conflict
            // (duplicate :param or *wildcard names at the same position); if it
            // throws we want the routes map to stay in sync with the tree, not
            // to silently retain an entry that iteration sees but match() never
            // reaches. Pass handle by value so we still own a copy to move into
            // the routes entry below.
            add_route(method, path, handle, spec.constraints);

            auto& entry = routes[std::string(path)][method];
            entry.handle = std::move(handle);
            entry.spec = spec;
         }
         catch (const std::exception& e) {
            std::fprintf(stderr, "Error adding route '%.*s': %s\n", static_cast<int>(path.length()), path.data(),
                         e.what());
         }
      }

      /**
       * @brief Match a target against the registered routes.
       *
       * @param method The HTTP method of the request.
       * @param target The request target (may include a query string).
       * @return Pair of (matched handler, extracted path parameters). The handler is
       *         a default-constructed H if no route matched.
       */
      std::pair<H, std::unordered_map<std::string, std::string>> match(http_method method,
                                                                       std::string_view target) const
      {
         std::unordered_map<std::string, std::string> params;
         H result{};

         // Strip query string from target for matching
         const auto [path, query_string] = split_target(target);

         // First try direct lookup for non-parameterized routes (optimization)
         auto direct_it = direct_routes.find(std::string(path));
         if (direct_it != direct_routes.end()) {
            auto method_it = direct_it->second.find(method);
            if (method_it != direct_it->second.end()) {
               return {method_it->second, params};
            }
         }

         std::vector<std::string> segments = split_path(path);
         match_node(&root, segments, 0, method, params, result);

         return {result, params};
      }

      /**
       * @brief Print the entire tree structure for debugging.
       *
       * Prints only the tree contents. The caller is responsible for any header
       * (basic_http_router::print_tree groups three of these under labelled sections).
       */
      void print_tree() const { print_node(&root, 0); }

     private:
      mutable radix_node root;
      std::unordered_map<std::string, std::unordered_map<http_method, H>> direct_routes;

      void add_route(http_method method, std::string_view path, H handle,
                     const std::unordered_map<std::string, param_constraint>& constraints = {})
      {
         std::string path_str(path);

         if (path_str.find(':') == std::string::npos && path_str.find('*') == std::string::npos) {
            direct_routes[path_str][method] = handle;
            return;
         }

         std::vector<std::string> segments = split_path(path);

         radix_node* current = &root;

         for (size_t i = 0; i < segments.size(); ++i) {
            const std::string& segment = segments[i];

            if (segment.empty()) continue;

            if (segment[0] == ':') {
               std::string param_name = segment.substr(1);

               if (!current->parameter_child) {
                  current->parameter_child = std::make_unique<radix_node>();
                  current->parameter_child->is_parameter = true;
                  current->parameter_child->parameter_name = param_name;
                  current->parameter_child->segment = segment;
                  current->parameter_child->full_path = current->full_path + "/" + segment;
               }
               else if (current->parameter_child->parameter_name != param_name) {
                  throw std::runtime_error("Route conflict: different parameter names at same position: :" +
                                           current->parameter_child->parameter_name + " vs :" + param_name);
               }

               current = current->parameter_child.get();
            }
            else if (segment[0] == '*') {
               std::string wildcard_name = segment.substr(1);

               if (i != segments.size() - 1) {
                  throw std::runtime_error("Wildcard must be the last segment in route: " + path_str);
               }

               if (!current->wildcard_child) {
                  current->wildcard_child = std::make_unique<radix_node>();
                  current->wildcard_child->is_wildcard = true;
                  current->wildcard_child->parameter_name = wildcard_name;
                  current->wildcard_child->segment = segment;
                  current->wildcard_child->full_path = current->full_path + "/" + segment;
               }
               else if (current->wildcard_child->parameter_name != wildcard_name) {
                  throw std::runtime_error("Route conflict: different wildcard names at same position: *" +
                                           current->wildcard_child->parameter_name + " vs *" + wildcard_name);
               }

               current = current->wildcard_child.get();
               break;
            }
            else {
               if (current->static_children.find(segment) == current->static_children.end()) {
                  current->static_children[segment] = std::make_unique<radix_node>();
                  current->static_children[segment]->segment = segment;
                  current->static_children[segment]->full_path = current->full_path + "/" + segment;
               }

               current = current->static_children[segment].get();
            }
         }

         current->is_endpoint = true;
         current->handlers[method] = handle;

         if (!constraints.empty()) {
            current->constraints[method] = constraints;
         }
      }

      bool match_node(radix_node* node, const std::vector<std::string>& segments, size_t index, http_method method,
                      std::unordered_map<std::string, std::string>& params, H& result) const
      {
         if (index == segments.size()) {
            if (node->is_endpoint) {
               auto it = node->handlers.find(method);
               if (it != node->handlers.end()) {
                  bool constraints_passed = true;
                  auto constraints_it = node->constraints.find(method);
                  if (constraints_it != node->constraints.end()) {
                     for (const auto& [param_name, constraint] : constraints_it->second) {
                        auto param_it = params.find(param_name);
                        if (param_it != params.end()) {
                           const std::string& value = param_it->second;
                           if (!constraint.validation(value)) {
                              constraints_passed = false;
                              break;
                           }
                        }
                     }
                  }

                  if (constraints_passed) {
                     result = it->second;
                     return true;
                  }
                  return false;
               }
            }
            return false;
         }

         const std::string& segment = segments[index];

         auto static_it = node->static_children.find(segment);
         if (static_it != node->static_children.end()) {
            if (match_node(static_it->second.get(), segments, index + 1, method, params, result)) {
               return true;
            }
         }

         if (node->parameter_child) {
            std::string param_name = node->parameter_child->parameter_name;
            params[param_name] = url_decode(segment);

            if (match_node(node->parameter_child.get(), segments, index + 1, method, params, result)) {
               return true;
            }

            params.erase(param_name);
         }

         if (node->wildcard_child) {
            std::string full_capture;
            for (size_t i = index; i < segments.size(); i++) {
               if (i > index) full_capture += "/";
               full_capture += url_decode(segments[i]);
            }

            const auto& wildcard_name = node->wildcard_child->parameter_name;
            params[wildcard_name] = full_capture;

            if (node->wildcard_child->is_endpoint) {
               auto it = node->wildcard_child->handlers.find(method);
               if (it != node->wildcard_child->handlers.end()) {
                  bool constraints_passed = true;
                  auto constraints_it = node->wildcard_child->constraints.find(method);
                  if (constraints_it != node->wildcard_child->constraints.end()) {
                     for (const auto& [param_name, constraint] : constraints_it->second) {
                        auto param_it = params.find(param_name);
                        if (param_it != params.end()) {
                           const std::string& value = param_it->second;
                           if (!constraint.validation(value)) {
                              constraints_passed = false;
                              break;
                           }
                        }
                     }
                  }

                  if (constraints_passed) {
                     result = it->second;
                     return true;
                  }
               }
            }

            // Wildcard match failed (not an endpoint, wrong method, or
            // constraint failure). Mirror the parameter-branch behavior and
            // erase the capture so the caller's params map reflects only the
            // matched route's parameters.
            params.erase(wildcard_name);
         }

         return false;
      }

      void print_node(const radix_node* node, int depth) const
      {
         if (!node) return;

         std::string indent(depth * 2, ' ');
         std::cout << indent << node->to_string() << "\n";

         if (node->is_endpoint) {
            std::cout << indent << "  Handlers: ";
            for (const auto& [method, _] : node->handlers) {
               std::cout << to_string(method) << " ";
            }
            std::cout << "\n";

            for (const auto& [method, method_constraints] : node->constraints) {
               std::cout << indent << "  Constraints for " << to_string(method) << ":\n";
               for (const auto& [param, constraint] : method_constraints) {
                  std::cout << indent << "    " << param << ": (" << constraint.description << ")\n";
               }
            }
         }

         for (const auto& [segment, child] : node->static_children) {
            print_node(child.get(), depth + 1);
         }

         if (node->parameter_child) {
            print_node(node->parameter_child.get(), depth + 1);
         }

         if (node->wildcard_child) {
            print_node(node->wildcard_child.get(), depth + 1);
         }
      }
   };

   /**
    * @brief Match a value against a pattern with advanced pattern matching features
    *
    * Supports:
    * - Wildcards (*) for matching any number of characters
    * - Question marks (?) for matching a single character
    * - Character classes ([a-z], [^0-9])
    * - Anchors (^ for start of string, $ for end of string)
    * - Escape sequences with backslash
    *
    * @param value The string to match
    * @param pattern The pattern to match against
    * @return true if the value matches the pattern, false otherwise
    */
   inline bool match_pattern(std::string_view value, std::string_view pattern)
   {
      enum struct State { Literal, Escape, CharClass };

      if (pattern.empty()) return true; // Empty pattern matches anything

      size_t v_pos = 0;
      size_t p_pos = 0;

      // For backtracking when we encounter *
      std::optional<size_t> backtrack_pattern;
      std::optional<size_t> backtrack_value;

      // For character classes
      State state = State::Literal;
      bool negate_class = false;
      bool char_class_match = false;

      while (v_pos < value.size() || p_pos < pattern.size()) {
         // Pattern exhausted but value remains
         if (p_pos >= pattern.size()) {
            if (backtrack_pattern) {
               p_pos = *backtrack_pattern;
               v_pos = ++(*backtrack_value);
               continue;
            }
            return false;
         }

         // Value exhausted but pattern remains
         if (v_pos >= value.size()) {
            if (p_pos < pattern.size() && pattern[p_pos] == '*' && p_pos == pattern.size() - 1) return true;

            if (backtrack_pattern) {
               p_pos = *backtrack_pattern;
               v_pos = ++(*backtrack_value);
               continue;
            }
            return false;
         }

         switch (state) {
         case State::Literal:
            if (pattern[p_pos] == '\\') {
               state = State::Escape;
               p_pos++;
               continue;
            }
            else if (pattern[p_pos] == '[') {
               state = State::CharClass;
               char_class_match = false;
               p_pos++;

               if (p_pos < pattern.size() && pattern[p_pos] == '^') {
                  negate_class = true;
                  p_pos++;
               }
               else {
                  negate_class = false;
               }
               continue;
            }
            else if (pattern[p_pos] == '*') {
               backtrack_pattern = p_pos;
               backtrack_value = v_pos;
               p_pos++;
               continue;
            }
            else if (pattern[p_pos] == '?') {
               p_pos++;
               v_pos++;
               continue;
            }
            else if (pattern[p_pos] == '^' && p_pos == 0) {
               p_pos++;
               continue;
            }
            else if (pattern[p_pos] == '$' && p_pos == pattern.size() - 1) {
               return v_pos == value.size();
            }
            else {
               if (pattern[p_pos] != value[v_pos]) {
                  if (backtrack_pattern) {
                     p_pos = *backtrack_pattern;
                     v_pos = ++(*backtrack_value);
                     continue;
                  }
                  return false;
               }
               p_pos++;
               v_pos++;
            }
            break;

         case State::Escape:
            if (p_pos >= pattern.size() || pattern[p_pos] != value[v_pos]) {
               if (backtrack_pattern) {
                  p_pos = *backtrack_pattern;
                  v_pos = ++(*backtrack_value);
                  state = State::Literal;
                  continue;
               }
               return false;
            }
            p_pos++;
            v_pos++;
            state = State::Literal;
            break;

         case State::CharClass:
            if (pattern[p_pos] == ']') {
               p_pos++;
               if (negate_class) {
                  if (char_class_match) {
                     return false;
                  }
                  v_pos++;
               }
               else {
                  if (!char_class_match) {
                     return false;
                  }
                  v_pos++;
               }
               state = State::Literal;
               continue;
            }
            else if (p_pos + 2 < pattern.size() && pattern[p_pos + 1] == '-') {
               char start = pattern[p_pos];
               char end = pattern[p_pos + 2];
               if (value[v_pos] >= start && value[v_pos] <= end) {
                  char_class_match = true;
               }
               p_pos += 3;
            }
            else {
               if (pattern[p_pos] == value[v_pos]) {
                  char_class_match = true;
               }
               p_pos++;
            }
            break;
         }
      }

      return v_pos == value.size() && p_pos == pattern.size();
   }

   /**
    * @brief HTTP router based on a radix tree for efficient path matching
    *
    * @tparam Handler The type of the handler that gets invoked upon a route match.
    *                 Must be invocable with (const request&, response&).
    *                 Note: Middleware registered via use() must also be this type.
    *
    * The basic_http_router class provides fast route matching for HTTP requests using a radix tree
    * data structure. It supports static routes, parameterized routes (e.g., "/users/:id"),
    * wildcard routes, and parameter validation via constraints.
    *
    * In addition to normal request/response handlers, basic_http_router also stores
    * streaming routes (registered via stream_get/stream_post/stream) and WebSocket
    * routes (registered via websocket()). All three kinds share the same radix-tree
    * matching logic, so streaming and WebSocket routes also support ":param" path
    * parameters.
    *
    * Note: http_server::mount() only accepts the default http_router (basic_http_router<>).
    * Custom handler routers are intended for standalone use or with custom server implementations.
    */
   template <class Handler = std::function<void(const request&, response&)>>
      requires std::invocable<Handler, const request&, response&>
   struct basic_http_router
   {
      /**
       * @brief Function type for request handlers
       */
      using handler = Handler;

      /**
       * @brief A compile-time boolean indicating whether asynchronous request handlers are enabled.
       */
      static constexpr bool is_async_enabled =
         std::is_constructible_v<Handler, std::function<void(const request&, response&)>>;

      /**
       * @brief Function type for asynchronous request handlers
       */
      using async_handler = std::function<std::future<void>(const request&, response&)>;

      /**
       * @brief Underlying route table type for normal routes.
       */
      using normal_route_table = route_table<Handler>;

      /**
       * @brief Backward-compatible alias for the normal route entry type.
       */
      using route_entry = typename normal_route_table::route_entry;

      basic_http_router() = default;

      /**
       * @brief Backward-compatible static helper. Equivalent to route_table<Handler>::split_path.
       */
      static std::vector<std::string> split_path(std::string_view path) { return normal_route_table::split_path(path); }

      /**
       * @brief Register a route with the router
       *
       * @param method The HTTP method (GET, POST, etc.)
       * @param path The route path, can include parameters (":param") and wildcards ("*param")
       * @param handle The handler function to call when this route matches
       * @param spec Optional spec for the route.
       * @return Reference to this router for method chaining
       * @throws std::runtime_error if there's a route conflict
       */
      basic_http_router& route(http_method method, std::string_view path, handler handle, const route_spec& spec = {})
      {
         normal_routes.add(method, path, std::move(handle), spec);
         return *this;
      }

      /**
       * @brief Register a GET route
       */
      basic_http_router& get(std::string_view path, handler handle, const route_spec& spec = {})
      {
         return route(http_method::GET, path, std::move(handle), spec);
      }

      /**
       * @brief Register a POST route
       */
      basic_http_router& post(std::string_view path, handler handle, const route_spec& spec = {})
      {
         return route(http_method::POST, path, std::move(handle), spec);
      }

      /**
       * @brief Register a PUT route
       */
      basic_http_router& put(std::string_view path, handler handle, const route_spec& spec = {})
      {
         return route(http_method::PUT, path, std::move(handle), spec);
      }

      /**
       * @brief Register a DELETE route
       */
      basic_http_router& del(std::string_view path, handler handle, const route_spec& spec = {})
      {
         return route(http_method::DELETE, path, std::move(handle), spec);
      }

      /**
       * @brief Register a PATCH route
       */
      basic_http_router& patch(std::string_view path, handler handle, const route_spec& spec = {})
      {
         return route(http_method::PATCH, path, std::move(handle), spec);
      }

      /**
       * @brief Register an asynchronous route
       */
      basic_http_router& route_async(http_method method, std::string_view path, async_handler handle,
                                     const route_spec& spec = {})
         requires is_async_enabled
      {
         return route(
            method, path,
            [handle = std::move(handle)](const request& req, response& res) {
               auto future = handle(req, res);
               future.get();
            },
            spec);
      }

      /**
       * @brief Register an asynchronous GET route
       */
      basic_http_router& get_async(std::string_view path, async_handler handle, const route_spec& spec = {})
         requires is_async_enabled
      {
         return route_async(http_method::GET, path, std::move(handle), spec);
      }

      /**
       * @brief Register an asynchronous POST route
       */
      basic_http_router& post_async(std::string_view path, async_handler handle, const route_spec& spec = {})
         requires is_async_enabled
      {
         return route_async(http_method::POST, path, std::move(handle), spec);
      }

      /**
       * @brief Register an asynchronous PUT route
       */
      basic_http_router& put_async(std::string_view path, async_handler handle, const route_spec& spec = {})
         requires is_async_enabled
      {
         return route_async(http_method::PUT, path, std::move(handle), spec);
      }

      /**
       * @brief Register an asynchronous DELETE route
       */
      basic_http_router& del_async(std::string_view path, async_handler handle, const route_spec& spec = {})
         requires is_async_enabled
      {
         return route_async(http_method::DELETE, path, std::move(handle), spec);
      }

      /**
       * @brief Register an asynchronous PATCH route
       */
      basic_http_router& patch_async(std::string_view path, async_handler handle, const route_spec& spec = {})
         requires is_async_enabled
      {
         return route_async(http_method::PATCH, path, std::move(handle), spec);
      }

      /**
       * @brief Register a streaming route. Streaming handlers take over the connection
       * lifecycle and write chunked responses through streaming_response.
       *
       * Supports the same path-parameter syntax as normal routes (":param" and "*name").
       */
      basic_http_router& stream(http_method method, std::string_view path, streaming_handler handle,
                                const route_spec& spec = {})
      {
         streaming_routes.add(method, path, std::move(handle), spec);
         return *this;
      }

      /**
       * @brief Register a streaming GET route.
       */
      basic_http_router& stream_get(std::string_view path, streaming_handler handle, const route_spec& spec = {})
      {
         return stream(http_method::GET, path, std::move(handle), spec);
      }

      /**
       * @brief Register a streaming POST route.
       */
      basic_http_router& stream_post(std::string_view path, streaming_handler handle, const route_spec& spec = {})
      {
         return stream(http_method::POST, path, std::move(handle), spec);
      }

      /**
       * @brief Register a WebSocket handler for a path.
       *
       * Supports the same path-parameter syntax as normal routes. The HTTP server
       * detects the upgrade handshake (Upgrade: websocket) and dispatches to the
       * matching server, populating request.params from the path.
       */
      basic_http_router& websocket(std::string_view path, websocket_handler server, const route_spec& spec = {})
      {
         websocket_routes.add(http_method::GET, path, std::move(server), spec);
         return *this;
      }

      /**
       * @brief Register middleware to be executed before route handlers
       *
       * Middleware functions are executed in the order they are registered.
       *
       * @param middleware The middleware function
       * @return Reference to this router for method chaining
       */
      basic_http_router& use(handler middleware)
      {
         middlewares.push_back(std::move(middleware));
         return *this;
      }

      /**
       * @brief Match a normal request against registered routes
       *
       * @param method The HTTP method of the request
       * @param target The target path of the request (may include query string)
       * @return A pair containing the matched handler and extracted parameters
       */
      std::pair<handler, std::unordered_map<std::string, std::string>> match(http_method method,
                                                                             std::string_view target) const
      {
         return normal_routes.match(method, target);
      }

      /**
       * @brief Match a streaming request against registered streaming routes.
       */
      std::pair<streaming_handler, std::unordered_map<std::string, std::string>> match_streaming(
         http_method method, std::string_view target) const
      {
         return streaming_routes.match(method, target);
      }

      /**
       * @brief Match a WebSocket upgrade request against registered WebSocket routes.
       *
       * WebSocket upgrades are HTTP GET requests by definition, so the lookup uses
       * http_method::GET internally.
       */
      std::pair<websocket_handler, std::unordered_map<std::string, std::string>> match_websocket(
         std::string_view target) const
      {
         return websocket_routes.match(http_method::GET, target);
      }

      /**
       * @brief Print the router structure for debugging.
       */
      void print_tree() const
      {
         std::cout << "[normal routes]\n";
         normal_routes.print_tree();
         std::cout << "[streaming routes]\n";
         streaming_routes.print_tree();
         std::cout << "[websocket routes]\n";
         websocket_routes.print_tree();
      }

      /**
       * @brief Storage for normal request/response routes.
       *
       * Iterate `router.normal_routes.routes` for path -> method -> entry inspection
       * (used by the OpenAPI generator and mount()).
       *
       * Migration note: prior to the streaming/WebSocket unification, this field was
       * exposed as `router.routes`. Code that iterated `router.routes` should now
       * iterate `router.normal_routes.routes` (or use the `routes()` accessor below).
       */
      normal_route_table normal_routes;

      /**
       * @brief Backward-compatible accessor for the registered normal routes map.
       *
       * Equivalent to `normal_routes.routes`. Provided as a function (not a
       * reference data member) because a reference member would be copied verbatim
       * by the implicitly-defined move constructor and dangle into the moved-from
       * source.
       */
      auto& routes() noexcept { return normal_routes.routes; }
      const auto& routes() const noexcept { return normal_routes.routes; }

      /**
       * @brief Storage for streaming routes.
       */
      route_table<streaming_handler> streaming_routes;

      /**
       * @brief Storage for WebSocket routes.
       */
      route_table<websocket_handler> websocket_routes;

      /**
       * @brief Vector of middleware handlers
       */
      std::vector<handler> middlewares;
   };

   /**
    * @brief Default HTTP router using std::function handlers
    *
    * This is a type alias for backward compatibility. Use basic_http_router<Handler>
    * if you need to customize the handler type for coroutines, different futures
    * implementations, or callback-based architectures.
    */
   using http_router = basic_http_router<>;
}
