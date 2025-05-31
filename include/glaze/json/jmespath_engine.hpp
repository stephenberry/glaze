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
   
   // Expression types
   enum class expression_type {
      identifier,
      index,
      slice,
      pipe,
      multi_select_hash,
      multi_select_list,
      function_call,
      filter,
      comparison,
      logical
   };
   
   // Parsed expression node
   struct expression_node
   {
      expression_type type;
      std::string identifier;
      std::optional<int32_t> index;
      std::optional<int32_t> slice_start;
      std::optional<int32_t> slice_end;
      std::optional<int32_t> slice_step;
      std::vector<std::unique_ptr<expression_node>> children;
      std::string function_name;
      std::vector<std::unique_ptr<expression_node>> arguments;
      
      expression_node(expression_type t) : type(t) {}
   };
   
   // Main expression evaluator
   class expression_evaluator
   {
      private:
      // Helper to normalize array indices
      static int32_t normalize_index(int32_t idx, size_t size) {
         if (idx < 0) {
            idx += static_cast<int32_t>(size);
         }
         return std::max(0, std::min(idx, static_cast<int32_t>(size)));
      }
      
      // Apply slice to array
      static query_result apply_slice(const json_t::array_t& arr,
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
      
      // Evaluate identifier expression
      static query_result eval_identifier(const std::string& name, const json_t& current) {
         if (current.is_object()) {
            const auto& obj = current.get_object();
            auto it = obj.find(name);
            if (it != obj.end()) {
               return query_result(it->second);
            }
         }
         return query_result(json_t{}); // null
      }
      
      // Evaluate index expression
      static query_result eval_index(int32_t idx, const json_t& current) {
         if (current.is_array()) {
            const auto& arr = current.get_array();
            const int32_t normalized_idx = normalize_index(idx, arr.size());
            if (normalized_idx >= 0 && normalized_idx < static_cast<int32_t>(arr.size())) {
               return query_result(arr[normalized_idx]);
            }
         }
         return query_result(json_t{}); // null
      }
      
      // Evaluate slice expression
      static query_result eval_slice(const json_t& current,
                                     std::optional<int32_t> start,
                                     std::optional<int32_t> end,
                                     std::optional<int32_t> step) {
         if (current.is_array()) {
            return apply_slice(current.get_array(), start, end, step);
         }
         return query_result(json_t{}); // null
      }
      
      // Evaluate function call
      static query_result eval_function(const std::string& func_name,
                                        const std::vector<std::unique_ptr<expression_node>>& args,
                                        const json_t& current,
                                        query_context& ctx);
      
   public:
      // Main evaluation function
      static query_result evaluate(const expression_node& expr, const json_t& current, query_context& ctx);
   };
   
   // Parse token into expression node
   std::unique_ptr<expression_node> parse_token(std::string_view token);
   
   // Parse full expression
   std::unique_ptr<expression_node> parse_expression(const std::vector<std::string_view>& tokens);
   
   // Main query function
   inline query_result query(const json_t& data, std::string_view jmespath_expr, context& ctx)
   {
      // Tokenize the expression
      jmespath_expression expr(jmespath_expr);
      if (expr.error != tokenization_error::none) {
         return query_result({error_code::syntax_error, "Invalid JMESPath expression"});
      }
      
      if (expr.tokens.empty()) {
         return query_result(data);
      }
      
      // Parse tokens into expression tree
      auto root_expr = parse_expression(expr.tokens);
      if (!root_expr) {
         return query_result({error_code::syntax_error, "Failed to parse expression"});
      }
      
      // Evaluate expression
      query_context query_ctx(data, ctx);
      return expression_evaluator::evaluate(*root_expr, data, query_ctx);
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
         
         // Simple implementation - in practice, you'd want to evaluate the second argument
         // as an expression on each array element
         std::sort(result_array.begin(), result_array.end(), [](const json_t& a, const json_t& b) {
            // Simple comparison based on type and value
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
   
   // Forward declaration for recursive parsing
   std::unique_ptr<expression_node> parse_expression(const std::vector<std::string_view>& tokens);
   
   // Parse token implementation
   inline std::unique_ptr<expression_node> parse_token(std::string_view token)
   {
      // Check for function call
      auto paren_pos = token.find('(');
      if (paren_pos != std::string_view::npos) {
         auto close_paren = token.rfind(')');
         if (close_paren != std::string_view::npos && close_paren > paren_pos) {
            auto node = std::make_unique<expression_node>(expression_type::function_call);
            node->function_name = std::string(token.substr(0, paren_pos));
            
            // Parse arguments - need to handle complex expressions
            auto args_str = token.substr(paren_pos + 1, close_paren - paren_pos - 1);
            if (!args_str.empty()) {
               // Trim whitespace
               while (!args_str.empty() && std::isspace(args_str.front())) {
                  args_str.remove_prefix(1);
               }
               while (!args_str.empty() && std::isspace(args_str.back())) {
                  args_str.remove_suffix(1);
               }
               
               if (!args_str.empty()) {
                  // Parse the argument as a full expression
                  jmespath_expression arg_expr(args_str);
                  if (arg_expr.error == tokenization_error::none && !arg_expr.tokens.empty()) {
                     auto arg_tree = parse_expression(arg_expr.tokens);
                     if (arg_tree) {
                        node->arguments.push_back(std::move(arg_tree));
                     } else {
                        // Return null to propagate the parsing failure up the call stack
                        return nullptr;
                     }
                  } else {
                     // Fall back to treating as simple identifier if tokenization fails
                     auto arg_node = std::make_unique<expression_node>(expression_type::identifier);
                     arg_node->identifier = std::string(args_str);
                     node->arguments.push_back(std::move(arg_node));
                  }
               }
            }
            return node;
         }
      }
      
      auto result = parse_jmespath_token(token);
      
      if (result.error) {
         return nullptr;
      }
      
      if (result.is_array_access) {
         if (result.colon_count > 0) {
            // Slice expression
            auto node = std::make_unique<expression_node>(expression_type::slice);
            node->identifier = std::string(result.key);
            node->slice_start = result.start;
            node->slice_end = result.end;
            node->slice_step = result.step;
            return node;
         } else {
            // Index expression
            auto node = std::make_unique<expression_node>(expression_type::index);
            node->identifier = std::string(result.key);
            node->index = result.start;
            return node;
         }
      } else {
         // Simple identifier
         auto node = std::make_unique<expression_node>(expression_type::identifier);
         node->identifier = std::string(result.key);
         return node;
      }
   }
   
   // Parse expression implementation
   inline std::unique_ptr<expression_node> parse_expression(const std::vector<std::string_view>& tokens)
   {
      if (tokens.empty()) {
         return nullptr;
      }
      
      // For now, handle simple dot-separated expressions
      // In a full implementation, you'd handle pipes, multi-select, etc.
      
      std::unique_ptr<expression_node> root = parse_token(tokens[0]);
      if (!root) {
         return nullptr;
      }
      
      std::unique_ptr<expression_node>* current = &root;
      
      for (size_t i = 1; i < tokens.size(); ++i) {
         auto next_node = parse_token(tokens[i]);
         if (!next_node) {
            return nullptr;
         }
         
         (*current)->children.push_back(std::move(next_node));
         current = &(*current)->children.back();
      }
      
      return root;
   }
   
   // Expression evaluator implementation
   inline query_result expression_evaluator::evaluate(const expression_node& expr, const json_t& current, query_context& ctx)
   {
      switch (expr.type) {
         case expression_type::identifier: {
            auto result = eval_identifier(expr.identifier, current);
            
            // Process children if any
            if (!expr.children.empty() && result) {
               for (const auto& child : expr.children) {
                  result = evaluate(*child, result.value, ctx);
                  if (!result) break;
               }
            }
            
            return result;
         }
            
         case expression_type::index: {
            json_t target = current;
            
            // If we have an identifier, navigate to it first
            if (!expr.identifier.empty()) {
               auto id_result = eval_identifier(expr.identifier, current);
               if (!id_result) return id_result;
               target = std::move(id_result.value);
            }
            
            auto result = eval_index(expr.index.value_or(0), target);
            
            // Process children if any
            if (!expr.children.empty() && result) {
               for (const auto& child : expr.children) {
                  result = evaluate(*child, result.value, ctx);
                  if (!result) break;
               }
            }
            
            return result;
         }
            
         case expression_type::slice: {
            json_t target = current;
            
            // If we have an identifier, navigate to it first
            if (!expr.identifier.empty()) {
               auto id_result = eval_identifier(expr.identifier, current);
               if (!id_result) return id_result;
               target = std::move(id_result.value);
            }
            
            auto result = eval_slice(target, expr.slice_start, expr.slice_end, expr.slice_step);
            
            // Process children if any
            if (!expr.children.empty() && result) {
               for (const auto& child : expr.children) {
                  result = evaluate(*child, result.value, ctx);
                  if (!result) break;
               }
            }
            
            return result;
         }
            
         case expression_type::function_call: {
            return eval_function(expr.function_name, expr.arguments, current, ctx);
         }
            
         default:
            return query_result({error_code::feature_not_supported, "Expression type not yet implemented"});
      }
   }
   
   inline query_result expression_evaluator::eval_function(const std::string& func_name,
                                                           const std::vector<std::unique_ptr<expression_node>>& args,
                                                           const json_t& current,
                                                           query_context& ctx)
   {
      const auto& registry = function_registry::get_global_registry();
      const auto* func = registry.get_function(func_name);
      
      if (!func) {
         return query_result({error_code::method_not_found, "Unknown function: " + func_name});
      }
      
      // Evaluate arguments
      std::vector<json_t> arg_values;
      arg_values.reserve(args.size());
      
      for (const auto& arg : args) {
         auto arg_result = evaluate(*arg, current, ctx);
         if (!arg_result) {
            return arg_result;
         }
         arg_values.push_back(std::move(arg_result.value));
      }
      
      return (*func)(arg_values, ctx);
   }
   
} // namespace glz::jmespath
