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
#include "glaze/net/http_headers.hpp"
#include "glaze/net/url.hpp"
#include "glaze/util/key_transformers.hpp"

// To deconflict Windows.h, which defines a DELETE macro that collides with the
// http_method::DELETE used in the route helpers below. http.hpp's own undef is
// include-guarded, so it does not re-run when a later include (e.g. asio via
// http_client.hpp) pulls in Windows.h before this header is parsed.
#ifdef DELETE
#undef DELETE
#endif

namespace glz
{
   namespace detail
   {
      struct request_line
      {
         http_method method;
         std::string_view target;
         bool is_http11;
      };
   }

   // Request context object
   struct request
   {
      http_method method{};
      std::string target{}; // Full request target (path + query string)
      std::string path{}; // Path component only (without query string)
      std::unordered_map<std::string, std::string> params{}; // Path parameters (e.g., :id)
      std::unordered_map<std::string, std::string> query{}; // Query parameters (e.g., ?limit=10)
      glz::http_headers headers{};
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
         has_upgrade = 16,
      };

      int status_code = 200;
      glz::http_headers response_headers{};
      std::string response_body{};
      uint8_t user_headers_set{};

      inline response& status(int code)
      {
         status_code = code;
         return *this;
      }

      // Replaces any existing field with this name.
      //
      // A field-name or field-value carrying CR or LF would terminate the field on
      // the wire, letting attacker-influenced data inject extra headers or a body
      // (CWE-113); header_field_has_crlf carries the full rationale. Rejecting it
      // here keeps it out of the container and, crucially, skips the
      // mark_user_supplied bookkeeping below - see that function for why the order
      // of these two lines is load-bearing. The wire serializers keep an
      // independent drop as a backstop for fields that bypass this setter.
      inline response& header(std::string_view name, std::string_view value)
      {
         if (header_field_has_crlf(name, value)) [[unlikely]] {
            return *this;
         }

         mark_user_supplied(name);
         response_headers.set(std::string(name), std::string(value));
         return *this;
      }

      // Appends instead of replacing, for names that can repeat like Set-Cookie.
      // Content-Length and Transfer-Encoding are replaced regardless: a second one leaves
      // the body length ambiguous and opens response smuggling (RFC 9112 6.3).
      inline response& add_header(std::string_view name, std::string_view value)
      {
         if (header_field_has_crlf(name, value)) [[unlikely]] {
            return *this;
         }

         mark_user_supplied(name);

         if (header_field_frames_body(name)) [[unlikely]] {
            response_headers.set(std::string(name), std::string(value));
         }
         else {
            response_headers.add(std::string(name), std::string(value));
         }
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

      inline response& content_type(std::string_view type) { return header("Content-Type", type); }

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

     private:
      // Records that the handler supplied one of the headers the wire serializer
      // would otherwise generate, so the serializer leaves it alone. Only reached
      // once the field has passed the CR/LF check: a field dropped there must not
      // set its flag, or a rejected Content-Length or Connection would suppress
      // the auto-generated counterpart and leave the response unframed.
      void mark_user_supplied(std::string_view name) noexcept
      {
         if (glz::striequal(name, "content-length"))
            user_headers_set |= has_content_length;
         else if (glz::striequal(name, "date"))
            user_headers_set |= has_date;
         else if (glz::striequal(name, "server"))
            user_headers_set |= has_server;
         else if (glz::striequal(name, "connection"))
            user_headers_set |= has_connection;
         else if (glz::striequal(name, "upgrade"))
            user_headers_set |= has_upgrade;
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
    * @brief The methods that have a route for a target, and the captures of the first such route.
    */
   struct target_methods
   {
      http_method_set methods{};
      std::unordered_map<std::string, std::string> params{};
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
       * @brief Detect a ".." path-traversal component in a decoded capture.
       *
       * Returns true when `path` contains a ".." segment delimited by a
       * separator or a string boundary. Percent-decoding happens after the
       * target is split on literal '/', so a "%2e%2e%2f" sequence only becomes
       * a "../" here; a capture carrying such a segment can climb out of a base
       * directory once a handler resolves it as a filesystem path.
       *
       * Both '/' and '\\' are treated as separators: on Windows the backslash
       * is a path separator too, so a "%2e%2e%5c" ("..\") capture traverses the
       * same way and an encoded backslash would otherwise slip past a '/'-only
       * check. A backslash is a legal byte in a POSIX filename, but a capture
       * bound for a filesystem join is exactly the case guarded here, so the
       * traversal reading wins over the rare literal-backslash name.
       */
      static bool has_dot_dot_segment(std::string_view path) noexcept
      {
         size_t start = 0;
         while (true) {
            const size_t sep = path.find_first_of("/\\", start);
            const size_t seg_len = (sep == std::string_view::npos ? path.size() : sep) - start;
            if (seg_len == 2 && path[start] == '.' && path[start + 1] == '.') {
               return true;
            }
            if (sep == std::string_view::npos) {
               return false;
            }
            start = sep + 1;
         }
      }

      /**
       * @brief Detect a decoded capture that begins with a root or drive prefix.
       *
       * The target is split on literal '/' before percent-decoding, so a leading
       * "%2f" only becomes a "/" here. Joined onto a base directory, a rooted
       * capture replaces the base rather than extending it
       * (std::filesystem::path{"/srv/www"} / "/etc/passwd" is "/etc/passwd"), so it
       * escapes as surely as a ".." segment. A leading '\\' also roots a path
       * on Windows. A drive prefix such as "D:" in "D:\\file" or "D:file" switches
       * drives there when joined onto a base on another drive, so it is refused on
       * every platform. Only a single ASCII letter followed by ':' at the very
       * start counts; "ab:c" or "notes/C:x" do not.
       */
      static bool has_root_prefix(std::string_view path) noexcept
      {
         if (path.empty()) return false;
         const char first = path.front();
         if (first == '/' || first == '\\') return true;
         return path.size() >= 2 && path[1] == ':' &&
                ((first >= 'A' && first <= 'Z') || (first >= 'a' && first <= 'z'));
      }

      /**
       * @brief Detect a decoded capture that carries a NUL byte.
       *
       * The request target cannot hold a raw NUL, so one only reaches a capture
       * as "%00" through url_decode_path. A capture joined onto a base directory
       * reaches open() as a C string, which ends at the first NUL, so
       * "report.txt%00.pdf" opens "report.txt" while every check a handler runs
       * on the std::string first, such as an extension allowlist, still sees
       * ".pdf".
       */
      static bool has_embedded_nul(std::string_view path) noexcept { return path.find('\0') != std::string_view::npos; }

      /**
       * @brief Whether a decoded capture may bind to a parameter: no ".." component, root or
       * drive prefix, or NUL byte (see the three checks above).
       */
      static bool is_safe_capture(std::string_view capture) noexcept
      {
         return !has_dot_dot_segment(capture) && !has_root_prefix(capture) && !has_embedded_nul(capture);
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
       * @brief Find every method with a route for a path, in one walk of the table.
       *
       * A method is reported exactly when match() would find a route for it, so per-method
       * constraints apply. The captures returned are those of the first route found, in
       * match()'s order: direct routes, then static, parameter, and wildcard segments.
       *
       * @param path The request path, without a query string.
       * @param segments The path split by split_path, so a caller searching several tables
       *        splits it once.
       */
      target_methods methods_for(std::string_view path, const std::vector<std::string>& segments) const
      {
         target_methods result;
         if (routes.empty()) {
            return result;
         }

         if (auto direct_it = direct_routes.find(std::string(path)); direct_it != direct_routes.end()) {
            for (const auto& [method, _] : direct_it->second) {
               result.methods.insert(method);
            }
         }

         std::unordered_map<std::string, std::string> params;
         collect_methods(&root, segments, 0, params, result);
         return result;
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

      // Whether the captures so far satisfy the constraints a route registered for `method`
      // at this endpoint. A constraint on a parameter the path did not capture is skipped.
      static bool constraints_pass(const radix_node& node, http_method method,
                                   const std::unordered_map<std::string, std::string>& params)
      {
         auto constraints_it = node.constraints.find(method);
         if (constraints_it == node.constraints.end()) {
            return true;
         }
         for (const auto& [param_name, constraint] : constraints_it->second) {
            auto param_it = params.find(param_name);
            if (param_it != params.end() && !constraint.validation(param_it->second)) {
               return false;
            }
         }
         return true;
      }

      // A "*name" capture: the remaining segments, each decoded, joined by '/'.
      static std::string wildcard_capture(const std::vector<std::string>& segments, size_t index)
      {
         std::string full_capture;
         for (size_t i = index; i < segments.size(); i++) {
            if (i > index) full_capture += "/";
            full_capture += url_decode_path(segments[i]);
         }
         return full_capture;
      }

      bool match_endpoint(const radix_node& node, http_method method,
                          const std::unordered_map<std::string, std::string>& params, H& result) const
      {
         if (!node.is_endpoint) {
            return false;
         }
         auto it = node.handlers.find(method);
         if (it == node.handlers.end() || !constraints_pass(node, method, params)) {
            return false;
         }
         result = it->second;
         return true;
      }

      bool match_node(const radix_node* node, const std::vector<std::string>& segments, size_t index,
                      http_method method, std::unordered_map<std::string, std::string>& params, H& result) const
      {
         if (index == segments.size()) {
            return match_endpoint(*node, method, params, result);
         }

         const std::string& segment = segments[index];

         auto static_it = node->static_children.find(segment);
         if (static_it != node->static_children.end()) {
            if (match_node(static_it->second.get(), segments, index + 1, method, params, result)) {
               return true;
            }
         }

         if (node->parameter_child) {
            std::string decoded = url_decode_path(segment);

            // A ":param" captures a single segment; refuse a decoded ".."
            // component, a leading separator, a drive prefix, or a NUL byte so
            // the value cannot escape a base directory or name a different file
            // when a handler treats it as a path.
            if (is_safe_capture(decoded)) {
               std::string param_name = node->parameter_child->parameter_name;
               params[param_name] = std::move(decoded);

               if (match_node(node->parameter_child.get(), segments, index + 1, method, params, result)) {
                  return true;
               }

               params.erase(param_name);
            }
         }

         if (node->wildcard_child) {
            std::string full_capture = wildcard_capture(segments, index);

            // The capture is joined from decoded segments, so a "%2e%2e%2f" in
            // the request only resolves to a ".." here, a leading "%2f" to a
            // rooted path, and a "%00" to a NUL; refuse all of them, and a drive
            // prefix, so a mount like "/files/*path" cannot be walked outside its
            // base directory or truncated onto another file.
            if (!is_safe_capture(full_capture)) {
               return false;
            }

            const auto& wildcard_name = node->wildcard_child->parameter_name;
            params[wildcard_name] = std::move(full_capture);

            if (match_endpoint(*node->wildcard_child, method, params, result)) {
               return true;
            }

            // Wildcard match failed (not an endpoint, wrong method, or
            // constraint failure). Mirror the parameter-branch behavior and
            // erase the capture so the caller's params map reflects only the
            // matched route's parameters.
            params.erase(wildcard_name);
         }

         return false;
      }

      // Adds the methods this endpoint answers under the current captures. The first
      // endpoint to add any keeps its captures, as the route match() would try first.
      static void collect_endpoint(const radix_node& node, const std::unordered_map<std::string, std::string>& params,
                                   target_methods& result)
      {
         if (!node.is_endpoint) {
            return;
         }
         const bool first_route = result.methods.empty();
         for (const auto& [method, _] : node.handlers) {
            if (!result.methods.contains(method) && constraints_pass(node, method, params)) {
               result.methods.insert(method);
            }
         }
         if (first_route && !result.methods.empty()) {
            result.params = params;
         }
      }

      // match_node for every method at once: the same walk and capture rules, but every
      // branch is visited, since each method takes the first branch that answers it.
      void collect_methods(const radix_node* node, const std::vector<std::string>& segments, size_t index,
                           std::unordered_map<std::string, std::string>& params, target_methods& result) const
      {
         if (index == segments.size()) {
            collect_endpoint(*node, params, result);
            return;
         }

         const std::string& segment = segments[index];

         if (auto static_it = node->static_children.find(segment); static_it != node->static_children.end()) {
            collect_methods(static_it->second.get(), segments, index + 1, params, result);
         }

         if (node->parameter_child) {
            std::string decoded = url_decode_path(segment);
            if (is_safe_capture(decoded)) {
               const std::string& param_name = node->parameter_child->parameter_name;
               params[param_name] = std::move(decoded);
               collect_methods(node->parameter_child.get(), segments, index + 1, params, result);
               params.erase(param_name);
            }
         }

         if (node->wildcard_child) {
            std::string full_capture = wildcard_capture(segments, index);
            if (is_safe_capture(full_capture)) {
               const std::string& wildcard_name = node->wildcard_child->parameter_name;
               params[wildcard_name] = std::move(full_capture);
               collect_endpoint(*node->wildcard_child, params, result);
               params.erase(wildcard_name);
            }
         }
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
       * @brief Every method with a route for a target, and the captures of the first such route.
       *
       * Covers normal, streaming, and WebSocket routes (a WebSocket route counts as GET, the
       * method of its upgrade request), respecting each route's constraints. Lets a server
       * tell a target with no route at all (404 Not Found) from one requested with a method
       * it has no route for (405 Method Not Allowed, whose Allow header lists these, RFC 9110
       * 15.5.6). The path is split once and each table with routes is walked once.
       *
       * @param target The request target (may include a query string)
       */
      target_methods allowed_methods(std::string_view target) const
      {
         const auto path = split_target(target).path;
         const auto segments = normal_route_table::split_path(path);

         target_methods result = normal_routes.methods_for(path, segments);
         const auto merge = [&](target_methods&& more) {
            if (result.methods.empty()) {
               result.params = std::move(more.params);
            }
            result.methods |= more.methods;
         };
         merge(streaming_routes.methods_for(path, segments));
         merge(websocket_routes.methods_for(path, segments));
         return result;
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
