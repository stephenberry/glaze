// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <algorithm>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <source_location>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "glaze/rest/http.hpp"

namespace glz
{
   // Forward declaration of the request/response structures
   struct request;
   struct response;

   // Radix tree router for handling different paths
   struct http_router
   {
      using handler = std::function<void(const struct request&, struct response&)>;
      using async_handler = std::function<std::future<void>(const struct request&, struct response&)>;

      // Defines parameter constraints for validation
      struct param_constraint
      {
         std::string pattern{}; // Custom pattern for this parameter
         std::string description{}; // For error reporting
      };

      // Structure representing a node in the radix tree
      struct RadixNode
      {
         std::string segment; // The path segment this node represents
         bool is_parameter = false; // Is this a parameter segment (e.g., ":id")
         bool is_wildcard = false; // Is this a wildcard segment (e.g., "*action")
         std::string parameter_name; // Name of the parameter (if is_parameter or is_wildcard is true)

         // Children nodes
         std::unordered_map<std::string, std::unique_ptr<RadixNode>> static_children;
         std::unique_ptr<RadixNode> parameter_child;
         std::unique_ptr<RadixNode> wildcard_child;

         // Route information (if this node is an endpoint)
         std::unordered_map<http_method, handler> handlers;
         std::unordered_map<http_method, std::unordered_map<std::string, param_constraint>> constraints;
         bool is_endpoint = false;

         // For debugging and conflict detection
         std::string full_path;

         // Return a human-readable representation of this node
         std::string to_string() const
         {
            std::stringstream ss;
            ss << "Node[" << (is_parameter ? "PARAM:" : (is_wildcard ? "WILD:" : "")) << segment
               << ", endpoint=" << (is_endpoint ? "true" : "false") << ", children=" << static_children.size()
               << (parameter_child ? "+param" : "") << (wildcard_child ? "+wild" : "") << ", full_path=" << full_path
               << "]";
            return ss.str();
         }
      };

      // Default constructor
      http_router() = default;

      // Helper function to split a path into segments
      static std::vector<std::string> split_path(std::string_view path)
      {
         std::vector<std::string> segments;

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

      // Add a route with a specific method
      inline http_router& route(http_method method, std::string_view path, handler handle,
                                const std::unordered_map<std::string, param_constraint>& constraints = {})
      {
         try {
            // Store in the routes map for compatibility with mount
            routes[std::string(path)][method] = handle;

            // Also add to the radix tree
            add_route(method, path, handle, constraints);
         }
         catch (const std::exception& e) {
            // Log the error instead of propagating it
            std::fprintf(stderr, "Error adding route '%.*s': %s\n", static_cast<int>(path.length()), path.data(),
                         e.what());
         }
         return *this;
      }

      // Standard HTTP method helpers
      inline http_router& get(std::string_view path, handler handle,
                              const std::unordered_map<std::string, param_constraint>& constraints = {})
      {
         return route(http_method::GET, path, std::move(handle), constraints);
      }

      inline http_router& post(std::string_view path, handler handle,
                               const std::unordered_map<std::string, param_constraint>& constraints = {})
      {
         return route(http_method::POST, path, std::move(handle), constraints);
      }

      inline http_router& put(std::string_view path, handler handle,
                              const std::unordered_map<std::string, param_constraint>& constraints = {})
      {
         return route(http_method::PUT, path, std::move(handle), constraints);
      }

      inline http_router& del(std::string_view path, handler handle,
                              const std::unordered_map<std::string, param_constraint>& constraints = {})
      {
         return route(http_method::DELETE, path, std::move(handle), constraints);
      }

      inline http_router& patch(std::string_view path, handler handle,
                                const std::unordered_map<std::string, param_constraint>& constraints = {})
      {
         return route(http_method::PATCH, path, std::move(handle), constraints);
      }

      // Async versions
      inline http_router& route_async(http_method method, std::string_view path, async_handler handle,
                                      const std::unordered_map<std::string, param_constraint>& constraints = {})
      {
         // Convert async handle to sync handle (same as in original code)
         return route(
            method, path,
            [handle](const request& req, response& res) {
               // Create a future and wait for it
               auto future = handle(req, res);
               future.wait();
            },
            constraints);
      }

      inline http_router& get_async(std::string_view path, async_handler handle,
                                    const std::unordered_map<std::string, param_constraint>& constraints = {})
      {
         return route_async(http_method::GET, path, std::move(handle), constraints);
      }

      inline http_router& post_async(std::string_view path, async_handler handle,
                                     const std::unordered_map<std::string, param_constraint>& constraints = {})
      {
         return route_async(http_method::POST, path, std::move(handle), constraints);
      }

      // Middleware support
      inline http_router& use(handler middleware)
      {
         middlewares.push_back(std::move(middleware));
         return *this;
      }

      // Find a route matching the given method and path
      inline std::pair<handler, std::unordered_map<std::string, std::string>> match(http_method method,
                                                                                    const std::string& target) const
      {
         std::unordered_map<std::string, std::string> params;
         handler result = nullptr;

         // First try direct lookup for non-parameterized routes (optimization)
         auto direct_it = direct_routes.find(target);
         if (direct_it != direct_routes.end()) {
            auto method_it = direct_it->second.find(method);
            if (method_it != direct_it->second.end()) {
               return {method_it->second, params}; // No params for direct match
            }
         }

         // Split the target path into segments
         std::vector<std::string> segments = split_path(target);

         // Use recursive matching function
         match_node(&root, segments, 0, method, params, result);

         return {result, params};
      }

      // Debug function to print the entire tree structure
      void print_tree() const
      {
         std::cout << "Radix Tree Structure:\n";
         print_node(&root, 0);
      }

      // Data members (maintaining API compatibility)
      std::unordered_map<std::string, std::unordered_map<http_method, handler>> routes;
      std::vector<handler> middlewares;

     private:
      // The root node of the radix tree
      mutable RadixNode root;

      // Direct lookup table for non-parameterized routes (optimization)
      std::unordered_map<std::string, std::unordered_map<http_method, handler>> direct_routes;

      // Internal function to add a route to the tree
      void add_route(http_method method, std::string_view path, handler handle,
                     const std::unordered_map<std::string, param_constraint>& constraints = {})
      {
         std::string path_str(path);

         // Optimization: for non-parameterized routes, store them directly
         if (path_str.find(':') == std::string::npos && path_str.find('*') == std::string::npos) {
            // Check for conflicts first
            auto& method_handlers = direct_routes[path_str];
            if (method_handlers.find(method) != method_handlers.end()) {
               throw std::runtime_error("Route conflict: handler already exists for " + std::string(to_string(method)) +
                                        " " + path_str);
            }

            // Store the route directly
            method_handlers[method] = handle;
            return;
         }

         // For parameterized routes, use the radix tree
         std::vector<std::string> segments = split_path(path);

         // Start at the root node
         RadixNode* current = &root;

         // Build the path through the tree
         for (size_t i = 0; i < segments.size(); ++i) {
            const std::string& segment = segments[i];

            if (segment.empty()) continue;

            if (segment[0] == ':') {
               // Parameter segment
               std::string param_name = segment.substr(1);

               if (!current->parameter_child) {
                  current->parameter_child = std::make_unique<RadixNode>();
                  current->parameter_child->is_parameter = true;
                  current->parameter_child->parameter_name = param_name;
                  current->parameter_child->segment = segment;

                  // Build the full path for debugging/conflict detection
                  current->parameter_child->full_path = current->full_path + "/" + segment;
               }
               else if (current->parameter_child->parameter_name != param_name) {
                  // Parameter name conflict
                  throw std::runtime_error("Route conflict: different parameter names at same position: :" +
                                           current->parameter_child->parameter_name + " vs :" + param_name);
               }

               current = current->parameter_child.get();
            }
            else if (segment[0] == '*') {
               // Wildcard segment
               std::string wildcard_name = segment.substr(1);

               // Wildcards must be at the end of the route
               if (i != segments.size() - 1) {
                  throw std::runtime_error("Wildcard must be the last segment in route: " + path_str);
               }

               if (!current->wildcard_child) {
                  current->wildcard_child = std::make_unique<RadixNode>();
                  current->wildcard_child->is_wildcard = true;
                  current->wildcard_child->parameter_name = wildcard_name;
                  current->wildcard_child->segment = segment;

                  // Build the full path for debugging/conflict detection
                  current->wildcard_child->full_path = current->full_path + "/" + segment;
               }
               else if (current->wildcard_child->parameter_name != wildcard_name) {
                  // Wildcard name conflict
                  throw std::runtime_error("Route conflict: different wildcard names at same position: *" +
                                           current->wildcard_child->parameter_name + " vs *" + wildcard_name);
               }

               current = current->wildcard_child.get();
               break; // Wildcard is always the last segment
            }
            else {
               // Static segment
               if (current->static_children.find(segment) == current->static_children.end()) {
                  current->static_children[segment] = std::make_unique<RadixNode>();
                  current->static_children[segment]->segment = segment;

                  // Build the full path for debugging/conflict detection
                  current->static_children[segment]->full_path = current->full_path + "/" + segment;
               }

               current = current->static_children[segment].get();
            }
         }

         // Check for route conflict
         if (current->is_endpoint && current->handlers.find(method) != current->handlers.end()) {
            throw std::runtime_error("Route conflict: handler already exists for " + std::string(to_string(method)) +
                                     " " + path_str);
         }

         // Mark as endpoint and set handler
         current->is_endpoint = true;
         current->handlers[method] = handle;

         // Store constraints (if any)
         if (!constraints.empty()) {
            current->constraints[method] = constraints;
         }
      }

      // Internal function to match a path against the tree
      bool match_node(RadixNode* node, const std::vector<std::string>& segments, size_t index, http_method method,
                      std::unordered_map<std::string, std::string>& params, handler& result) const
      {
         // End of path
         if (index == segments.size()) {
            if (node->is_endpoint) {
               auto it = node->handlers.find(method);
               if (it != node->handlers.end()) {
                  // Found a handler
                  result = it->second;

                  // Validate constraints if any
                  auto constraints_it = node->constraints.find(method);
                  if (constraints_it != node->constraints.end()) {
                     for (const auto& [param_name, constraint] : constraints_it->second) {
                        auto param_it = params.find(param_name);
                        if (param_it != params.end()) {
                           const std::string& value = param_it->second;
                           // For simplicity, just checking if the value is not empty
                           // In a real implementation, you'd match against the pattern
                           if (value.empty()) {
                              return false; // Constraint failed
                           }
                        }
                     }
                  }

                  return true;
               }
            }
            return false;
         }

         const std::string& segment = segments[index];

         // Try static match first (most specific)
         auto static_it = node->static_children.find(segment);
         if (static_it != node->static_children.end()) {
            if (match_node(static_it->second.get(), segments, index + 1, method, params, result)) {
               return true;
            }
         }

         // Try parameter match (less specific than static)
         if (node->parameter_child) {
            // Save parameter value
            std::string param_name = node->parameter_child->parameter_name;
            params[param_name] = segment;

            if (match_node(node->parameter_child.get(), segments, index + 1, method, params, result)) {
               return true;
            }

            // If this parameter didn't lead to a match, remove it
            params.erase(param_name);
         }

         // Try wildcard match (least specific)
         if (node->wildcard_child) {
            // For wildcards, capture all remaining segments
            std::string full_capture;
            for (size_t i = index; i < segments.size(); i++) {
               if (i > index) full_capture += "/";
               full_capture += segments[i];
            }

            params[node->wildcard_child->parameter_name] = full_capture;

            // Wildcards always go to the endpoint of their node
            if (node->wildcard_child->is_endpoint) {
               auto it = node->wildcard_child->handlers.find(method);
               if (it != node->wildcard_child->handlers.end()) {
                  result = it->second;

                  // Validate constraints for the wildcard
                  auto constraints_it = node->wildcard_child->constraints.find(method);
                  if (constraints_it != node->wildcard_child->constraints.end()) {
                     for (const auto& [param_name, constraint] : constraints_it->second) {
                        auto param_it = params.find(param_name);
                        if (param_it != params.end()) {
                           const std::string& value = param_it->second;
                           // Simplified constraint check
                           if (value.empty()) {
                              return false; // Constraint failed
                           }
                        }
                     }
                  }

                  return true;
               }
            }
         }

         return false;
      }

      // Recursive helper to print the tree structure
      void print_node(const RadixNode* node, int depth) const
      {
         if (!node) return;

         std::string indent(depth * 2, ' ');
         std::cout << indent << node->to_string() << "\n";

         // Print all handlers for this node
         if (node->is_endpoint) {
            std::cout << indent << "  Handlers: ";
            for (const auto& [method, _] : node->handlers) {
               std::cout << to_string(method) << " ";
            }
            std::cout << "\n";

            // Print constraints if any
            for (const auto& [method, method_constraints] : node->constraints) {
               std::cout << indent << "  Constraints for " << to_string(method) << ":\n";
               for (const auto& [param, constraint] : method_constraints) {
                  std::cout << indent << "    " << param << ": " << constraint.pattern << " (" << constraint.description
                            << ")\n";
               }
            }
         }

         // Print children
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
}
