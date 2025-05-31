// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/json/json_t.hpp"
#include "glaze/json/jmespath.hpp"
#include "glaze/core/context.hpp"
#include <algorithm>
#include <functional>
#include <unordered_map>
#include <cmath>

namespace glz::jmespath
{
   // Result wrapper for operations that may fail
   struct query_result
   {
      json_t value{};
      error_ctx error{};
      
      query_result() = default;
      query_result(json_t&& val) : value(std::move(val)) {}
      query_result(const json_t& val) : value(val) {}
      query_result(error_ctx err) : error(std::move(err)) {}
      
      operator bool() const { return !error; }
   };
   
   // Context for query execution
   struct query_context
   {
      const json_t* root = nullptr;
      context* ctx = nullptr;
      
      query_context(const json_t& root_val, context& context_val)
      : root(&root_val), ctx(&context_val) {}
   };
   
   // Function registry for JMESPath functions
   using jmespath_function = std::function<query_result(const std::vector<json_t>&, query_context&)>;
   
   struct function_registry
   {
   private:
      std::unordered_map<std::string, jmespath_function> functions_;
      
   public:
      function_registry();
      
      void register_function(const std::string& name, jmespath_function func) {
         functions_[name] = std::move(func);
      }
      
      const jmespath_function* get_function(const std::string& name) const {
         auto it = functions_.find(name);
         return it != functions_.end() ? &it->second : nullptr;
      }
      
      static function_registry& get_global_registry() {
         static function_registry registry;
         return registry;
      }
   };
   
   // Helper to normalize array indices
   inline int32_t normalize_index(int32_t idx, size_t size) {
      if (idx < 0) {
         idx += static_cast<int32_t>(size);
      }
      return std::max(0, std::min(idx, static_cast<int32_t>(size)));
   }
   
   // Apply slice to array
   inline query_result apply_slice(const json_t::array_t& arr,
                                   std::optional<int32_t> start,
                                   std::optional<int32_t> end,
                                   std::optional<int32_t> step) {
      const auto size = static_cast<int32_t>(arr.size());
      const int32_t actual_step = step.value_or(1);
      
      if (actual_step == 0) {
         return query_result({error_code::syntax_error, "Slice step cannot be zero"});
      }
      
      int32_t actual_start, actual_end;
      
      if (actual_step > 0) {
         actual_start = normalize_index(start.value_or(0), arr.size());
         actual_end = normalize_index(end.value_or(size), arr.size());
      } else {
         actual_start = normalize_index(start.value_or(size - 1), arr.size());
         actual_end = normalize_index(end.value_or(-1), arr.size());
      }
      
      json_t::array_t result;
      
      if (actual_step > 0) {
         for (int32_t i = actual_start; i < actual_end; i += actual_step) {
            if (i >= 0 && i < size) {
               result.push_back(arr[i]);
            }
         }
      } else {
         for (int32_t i = actual_start; i > actual_end; i += actual_step) {
            if (i >= 0 && i < size) {
               result.push_back(arr[i]);
            }
         }
      }
      
      return query_result(json_t(std::move(result)));
   }
   
   // Parse and execute a function call token
   inline query_result evaluate_function_token(std::string_view token, const json_t& current, query_context& ctx) {
      auto paren_pos = token.find('(');
      if (paren_pos == std::string_view::npos) {
         return query_result({error_code::syntax_error, "Invalid function call"});
      }
      
      auto close_paren = token.rfind(')');
      if (close_paren == std::string_view::npos || close_paren <= paren_pos) {
         return query_result({error_code::syntax_error, "Invalid function call"});
      }
      
      auto func_name = token.substr(0, paren_pos);
      auto args_str = token.substr(paren_pos + 1, close_paren - paren_pos - 1);
      
      const auto& registry = function_registry::get_global_registry();
      const auto* func = registry.get_function(std::string(func_name));
      
      if (!func) {
         return query_result({error_code::method_not_found, "Unknown function: " + std::string(func_name)});
      }
      
      // Parse arguments
      std::vector<json_t> arg_values;
      
      if (!args_str.empty()) {
         // Trim whitespace
         while (!args_str.empty() && std::isspace(args_str.front())) {
            args_str.remove_prefix(1);
         }
         while (!args_str.empty() && std::isspace(args_str.back())) {
            args_str.remove_suffix(1);
         }
         
         if (!args_str.empty()) {
            // For now, support single argument that's a property name
            if (current.is_object()) {
               const auto& obj = current.get_object();
               auto it = obj.find(args_str);
               if (it != obj.end()) {
                  arg_values.push_back(it->second);
               } else {
                  arg_values.push_back(json_t{});
               }
            } else {
               arg_values.push_back(json_t{});
            }
         }
      }
      
      return (*func)(arg_values, ctx);
   }
   
   // Evaluate a single token against current JSON value, returning pointer to avoid copying
   inline const json_t* evaluate_token_ref(std::string_view token, const json_t& current, query_context& ctx, json_t& temp_storage) {
      // Check for function call
      if (token.find('(') != std::string_view::npos) {
         auto result = evaluate_function_token(token, current, ctx);
         if (!result) {
            return nullptr;
         }
         temp_storage = std::move(result.value);
         return &temp_storage;
      }
      
      // Parse the token
      auto result = parse_jmespath_token(token);
      if (result.error) {
         return nullptr;
      }
      
      if (result.is_array_access) {
         // Handle array access - first navigate to the key if present
         const json_t* target = &current;
         
         if (!result.key.empty()) {
            if (current.is_object()) {
               const auto& obj = current.get_object();
               auto it = obj.find(result.key);
               if (it != obj.end()) {
                  target = &it->second;
               } else {
                  return nullptr;
               }
            } else {
               return nullptr;
            }
         }
         
         if (!target->is_array()) {
            return nullptr;
         }
         
         const auto& arr = target->get_array();
         
         if (result.colon_count > 0) {
            // Slice operation - need temp storage
            auto slice_result = apply_slice(arr, result.start, result.end, result.step);
            if (!slice_result) {
               return nullptr;
            }
            temp_storage = std::move(slice_result.value);
            return &temp_storage;
         } else {
            // Index operation
            if (result.start.has_value()) {
               const int32_t normalized_idx = normalize_index(result.start.value(), arr.size());
               if (normalized_idx >= 0 && normalized_idx < static_cast<int32_t>(arr.size())) {
                  return &arr[normalized_idx];
               }
            }
            return nullptr;
         }
      } else {
         // Simple property access
         if (current.is_object()) {
            const auto& obj = current.get_object();
            auto it = obj.find(result.key);
            if (it != obj.end()) {
               return &it->second;
            }
         }
         return nullptr;
      }
   }
   
   // Main evaluation function that processes tokens sequentially using references
   inline query_result evaluate_tokens(const std::vector<std::string_view>& tokens, const json_t& data, query_context& ctx) {
      if (tokens.empty()) {
         return query_result(data);
      }
      
      const json_t* current = &data;
      json_t temp_storage; // For cases where we need to store intermediate results
      
      for (const auto& token : tokens) {
         current = evaluate_token_ref(token, *current, ctx, temp_storage);
         if (!current) {
            return query_result(json_t{}); // null result
         }
      }
      
      return query_result(*current);
   }
   
   // Main query function
   inline query_result query(const json_t& data, std::string_view jmespath_expr, context& ctx)
   {
      // Tokenize the expression
      jmespath_expression expr(jmespath_expr);
      if (expr.error != tokenization_error::none) {
         return query_result({error_code::syntax_error, "Invalid JMESPath expression"});
      }
      
      // Evaluate tokens directly
      query_context query_ctx(data, ctx);
      return evaluate_tokens(expr.tokens, data, query_ctx);
   }
   
   // Register a custom function
   inline void register_function(const std::string& name, jmespath_function func)
   {
      function_registry::get_global_registry().register_function(name, std::move(func));
   }
   
   // Implementation of function registry
   inline function_registry::function_registry()
   {
      // length function
      register_function("length", [](const std::vector<json_t>& args, query_context&) -> query_result {
         if (args.size() != 1) {
            return query_result({error_code::syntax_error, "length() requires exactly 1 argument"});
         }
         
         const auto& arg = args[0];
         double length = 0;
         
         if (arg.is_array()) {
            length = static_cast<double>(arg.get_array().size());
         } else if (arg.is_object()) {
            length = static_cast<double>(arg.get_object().size());
         } else if (arg.is_string()) {
            length = static_cast<double>(arg.get_string().size());
         } else if (arg.is_null()) {
            length = 0;
         }
         
         return query_result(json_t(length));
      });
      
      // keys function
      register_function("keys", [](const std::vector<json_t>& args, query_context&) -> query_result {
         if (args.size() != 1) {
            return query_result({error_code::syntax_error, "keys() requires exactly 1 argument"});
         }
         
         const auto& arg = args[0];
         if (!arg.is_object()) {
            return query_result(json_t{}); // null
         }
         
         json_t::array_t keys;
         for (const auto& [key, value] : arg.get_object()) {
            keys.emplace_back(key);
         }
         
         // Sort keys for consistent output
         std::sort(keys.begin(), keys.end(), [](const json_t& a, const json_t& b) {
            return a.get_string() < b.get_string();
         });
         
         return query_result(json_t(std::move(keys)));
      });
      
      // values function
      register_function("values", [](const std::vector<json_t>& args, query_context&) -> query_result {
         if (args.size() != 1) {
            return query_result({error_code::syntax_error, "values() requires exactly 1 argument"});
         }
         
         const auto& arg = args[0];
         if (!arg.is_object()) {
            return query_result(json_t{}); // null
         }
         
         json_t::array_t values;
         for (const auto& [key, value] : arg.get_object()) {
            values.emplace_back(value);
         }
         
         return query_result(json_t(std::move(values)));
      });
      
      // type function
      register_function("type", [](const std::vector<json_t>& args, query_context&) -> query_result {
         if (args.size() != 1) {
            return query_result({error_code::syntax_error, "type() requires exactly 1 argument"});
         }
         
         const auto& arg = args[0];
         std::string type_name;
         
         if (arg.is_null()) type_name = "null";
         else if (arg.is_boolean()) type_name = "boolean";
         else if (arg.is_number()) type_name = "number";
         else if (arg.is_string()) type_name = "string";
         else if (arg.is_array()) type_name = "array";
         else if (arg.is_object()) type_name = "object";
         
         return query_result(json_t(type_name));
      });
      
      // sort_by function
      register_function("sort_by", [](const std::vector<json_t>& args, query_context&) -> query_result {
         if (args.size() != 2) {
            return query_result({error_code::syntax_error, "sort_by() requires exactly 2 arguments"});
         }
         
         const auto& arr_arg = args[0];
         if (!arr_arg.is_array()) {
            return query_result(json_t{}); // null
         }
         
         auto result_array = arr_arg.get_array();
         
         // Simple implementation - sort based on type and value
         std::sort(result_array.begin(), result_array.end(), [](const json_t& a, const json_t& b) {
            if (a.is_number() && b.is_number()) {
               return a.get_number() < b.get_number();
            }
            if (a.is_string() && b.is_string()) {
               return a.get_string() < b.get_string();
            }
            return false;
         });
         
         return query_result(json_t(std::move(result_array)));
      });
   }
   
} // namespace glz::jmespath
