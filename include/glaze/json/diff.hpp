// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <algorithm>
#include <charconv>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "glaze/core/context.hpp"
#include "glaze/json/json_t.hpp"
#include "glaze/json/read.hpp"
#include "glaze/json/write.hpp"
#include "glaze/util/expected.hpp"

namespace glz
{
   namespace detail
   {
      inline constexpr std::string_view invalid_patch_document_msg = "JSON patch document must be an array";
      inline constexpr std::string_view invalid_patch_op_msg = "JSON patch operation requires a valid 'op' field";
      inline constexpr std::string_view invalid_patch_path_msg = "JSON patch path is invalid or missing";
      inline constexpr std::string_view missing_value_msg = "JSON patch operation is missing required 'value'";
      inline constexpr std::string_view invalid_index_msg = "JSON patch array index is invalid";
      inline constexpr std::string_view path_not_found_msg = "JSON patch target path does not exist";
      inline constexpr std::string_view test_failed_msg = "JSON patch test operation failed";

      inline error_ctx make_patch_error(const error_code code, const std::string_view message)
      {
         return error_ctx{code, message, 0, {}};
      }

      inline std::string escape_json_pointer_token(const std::string_view token)
      {
         std::string escaped{};
         escaped.reserve(token.size());
         for (const char c : token) {
            if (c == '~') {
               escaped += "~0";
            }
            else if (c == '/') {
               escaped += "~1";
            }
            else {
               escaped.push_back(c);
            }
         }
         return escaped;
      }

      inline std::string append_json_pointer(const std::string& base, const std::string_view token)
      {
         std::string result{};
         result.reserve(base.size() + token.size() + 1);
         result = base;
         result.push_back('/');
         const auto escaped = escape_json_pointer_token(token);
         result.append(escaped);
         return result;
      }

      inline bool json_equals(const json_t& lhs, const json_t& rhs) noexcept;

      inline bool array_equals(const json_t::array_t& lhs, const json_t::array_t& rhs) noexcept
      {
         if (lhs.size() != rhs.size()) return false;
         for (size_t i = 0; i < lhs.size(); ++i) {
            if (!json_equals(lhs[i], rhs[i])) return false;
         }
         return true;
      }

      inline bool object_equals(const json_t::object_t& lhs, const json_t::object_t& rhs) noexcept
      {
         if (lhs.size() != rhs.size()) return false;
         auto it_lhs = lhs.begin();
         auto it_rhs = rhs.begin();
         for (; it_lhs != lhs.end(); ++it_lhs, ++it_rhs) {
            if (it_lhs->first != it_rhs->first) return false;
            if (!json_equals(it_lhs->second, it_rhs->second)) return false;
         }
         return true;
      }

      inline bool json_equals(const json_t& lhs, const json_t& rhs) noexcept
      {
         if (lhs.is_object() && rhs.is_object()) {
            return object_equals(lhs.get_object(), rhs.get_object());
         }
         if (lhs.is_array() && rhs.is_array()) {
            return array_equals(lhs.get_array(), rhs.get_array());
         }
         if (lhs.is_string() && rhs.is_string()) {
            return lhs.get_string() == rhs.get_string();
         }
         if (lhs.is_number() && rhs.is_number()) {
            return lhs.get_number() == rhs.get_number();
         }
         if (lhs.is_boolean() && rhs.is_boolean()) {
            return lhs.get_boolean() == rhs.get_boolean();
         }
         if (lhs.is_null() && rhs.is_null()) {
            return true;
         }
         return false;
      }

      inline void diff_impl(const json_t& from, const json_t& to, const std::string& path, json_t::array_t& ops)
      {
         if (json_equals(from, to)) return;

         if (from.is_object() && to.is_object()) {
            const auto& from_obj = from.get_object();
            const auto& to_obj = to.get_object();

            // removals
            for (const auto& [key, value] : from_obj) {
               if (!to_obj.contains(key)) {
                  ops.emplace_back(json_t{{"op", "remove"}, {"path", append_json_pointer(path, key)}});
               }
            }

            // recursive diffs
            for (const auto& [key, value] : from_obj) {
               auto iter = to_obj.find(key);
               if (iter != to_obj.end()) {
                  diff_impl(value, iter->second, append_json_pointer(path, key), ops);
               }
            }

            // additions
            for (const auto& [key, value] : to_obj) {
               if (!from_obj.contains(key)) {
                  ops.emplace_back(json_t{{"op", "add"}, {"path", append_json_pointer(path, key)}, {"value", value}});
               }
            }
            return;
         }

         if (from.is_array() && to.is_array()) {
            const auto& from_arr = from.get_array();
            const auto& to_arr = to.get_array();
            const auto min_size = std::min(from_arr.size(), to_arr.size());

            for (size_t i = 0; i < min_size; ++i) {
               diff_impl(from_arr[i], to_arr[i], append_json_pointer(path, std::to_string(i)), ops);
            }

            for (size_t i = from_arr.size(); i-- > min_size;) {
               ops.emplace_back(json_t{{"op", "remove"}, {"path", append_json_pointer(path, std::to_string(i))}});
            }

            for (size_t i = min_size; i < to_arr.size(); ++i) {
               ops.emplace_back(json_t{{"op", "add"}, {"path", append_json_pointer(path, std::to_string(i))}, {"value", to_arr[i]}});
            }
            return;
         }

         // fallback replace
         if (path.empty()) {
            ops.emplace_back(json_t{{"op", "replace"}, {"path", ""}, {"value", to}});
         }
         else {
            ops.emplace_back(json_t{{"op", "replace"}, {"path", path}, {"value", to}});
         }
      }

      inline expected<std::vector<std::string>, error_ctx> parse_json_pointer(std::string_view path)
      {
         std::vector<std::string> tokens{};
         if (path.empty()) {
            return tokens;
         }
         if (path.front() != '/') {
            return unexpected(make_patch_error(error_code::json_patch_invalid_path, invalid_patch_path_msg));
         }

         std::string current{};
         for (size_t i = 1; i < path.size(); ++i) {
            const char c = path[i];
            if (c == '/') {
               tokens.push_back(current);
               current.clear();
            }
            else if (c == '~') {
               if (++i >= path.size()) {
                  return unexpected(make_patch_error(error_code::json_patch_invalid_path, invalid_patch_path_msg));
               }
               const char esc = path[i];
               if (esc == '0') {
                  current.push_back('~');
               }
               else if (esc == '1') {
                  current.push_back('/');
               }
               else {
                  return unexpected(make_patch_error(error_code::json_patch_invalid_path, invalid_patch_path_msg));
               }
            }
            else {
               current.push_back(c);
            }
         }
         tokens.push_back(current);
         return tokens;
      }

      inline expected<size_t, error_ctx> parse_array_index(const std::string& token)
      {
         if (token.empty()) {
            return unexpected(make_patch_error(error_code::json_patch_invalid_index, invalid_index_msg));
         }
         size_t index{};
         const auto begin = token.data();
         const auto end = begin + token.size();
         const auto result = std::from_chars(begin, end, index);
         if (result.ec != std::errc{} || result.ptr != end) {
            return unexpected(make_patch_error(error_code::json_patch_invalid_index, invalid_index_msg));
         }
         return index;
      }

      inline expected<json_t*, error_ctx> resolve_pointer(json_t& root, const std::vector<std::string>& tokens)
      {
         json_t* current = &root;
         for (const auto& token : tokens) {
            if (current->is_object()) {
               auto& obj = current->get_object();
               auto it = obj.find(token);
               if (it == obj.end()) {
                  return unexpected(make_patch_error(error_code::json_patch_invalid_path, path_not_found_msg));
               }
               current = &it->second;
            }
            else if (current->is_array()) {
               auto arr_index = parse_array_index(token);
               if (!arr_index) {
                  return unexpected(arr_index.error());
               }
               auto& arr = current->get_array();
               if (*arr_index >= arr.size()) {
                  return unexpected(make_patch_error(error_code::json_patch_invalid_path, path_not_found_msg));
               }
               current = &arr[*arr_index];
            }
            else {
               return unexpected(make_patch_error(error_code::json_patch_invalid_path, path_not_found_msg));
            }
         }
         return current;
      }

      inline expected<std::pair<json_t*, std::string>, error_ctx> resolve_parent(json_t& root,
                                                                                 const std::vector<std::string>& tokens)
      {
         if (tokens.empty()) {
            return std::pair<json_t*, std::string>{&root, {}};
         }
         json_t* current = &root;
         for (size_t i = 0; i + 1 < tokens.size(); ++i) {
            const auto& token = tokens[i];
            if (current->is_object()) {
               auto& obj = current->get_object();
               auto it = obj.find(token);
               if (it == obj.end()) {
                  return unexpected(make_patch_error(error_code::json_patch_invalid_path, path_not_found_msg));
               }
               current = &it->second;
            }
            else if (current->is_array()) {
               auto idx = parse_array_index(token);
               if (!idx) {
                  return unexpected(idx.error());
               }
               auto& arr = current->get_array();
               if (*idx >= arr.size()) {
                  return unexpected(make_patch_error(error_code::json_patch_invalid_path, path_not_found_msg));
               }
               current = &arr[*idx];
            }
            else {
               return unexpected(make_patch_error(error_code::json_patch_invalid_path, path_not_found_msg));
            }
         }
         return std::pair<json_t*, std::string>{current, tokens.back()};
      }

      inline error_ctx add_operation(json_t& target, const std::vector<std::string>& tokens, const json_t& value)
      {
         if (tokens.empty()) {
            target = value;
            return {};
         }
         auto parent_info = resolve_parent(target, tokens);
         if (!parent_info) {
            return parent_info.error();
         }
         auto [parent, raw_token] = *parent_info;
         if (parent->is_object()) {
            parent->get_object()[raw_token] = value;
            return {};
         }
         if (parent->is_array()) {
            auto& arr = parent->get_array();
            if (raw_token == "-") {
               arr.push_back(value);
               return {};
            }
            auto idx = parse_array_index(raw_token);
            if (!idx) {
               return idx.error();
            }
            if (*idx > arr.size()) {
               return make_patch_error(error_code::json_patch_invalid_index, invalid_index_msg);
            }
            arr.insert(arr.begin() + static_cast<std::ptrdiff_t>(*idx), value);
            return {};
         }
         return make_patch_error(error_code::json_patch_invalid_path, path_not_found_msg);
      }

      inline error_ctx remove_operation(json_t& target, const std::vector<std::string>& tokens)
      {
         if (tokens.empty()) {
            target.reset();
            return {};
         }
         auto parent_info = resolve_parent(target, tokens);
         if (!parent_info) {
            return parent_info.error();
         }
         auto [parent, raw_token] = *parent_info;
         if (parent->is_object()) {
            auto& obj = parent->get_object();
            auto it = obj.find(raw_token);
            if (it == obj.end()) {
               return make_patch_error(error_code::json_patch_invalid_path, path_not_found_msg);
            }
            obj.erase(it);
            return {};
         }
         if (parent->is_array()) {
            auto idx = parse_array_index(raw_token);
            if (!idx) {
               return idx.error();
            }
            auto& arr = parent->get_array();
            if (*idx >= arr.size()) {
               return make_patch_error(error_code::json_patch_invalid_path, path_not_found_msg);
            }
            arr.erase(arr.begin() + static_cast<std::ptrdiff_t>(*idx));
            return {};
         }
         return make_patch_error(error_code::json_patch_invalid_path, path_not_found_msg);
      }

      inline error_ctx replace_operation(json_t& target, const std::vector<std::string>& tokens, const json_t& value)
      {
         if (tokens.empty()) {
            target = value;
            return {};
         }
         auto parent_info = resolve_parent(target, tokens);
         if (!parent_info) {
            return parent_info.error();
         }
         auto [parent, raw_token] = *parent_info;
         if (parent->is_object()) {
            auto& obj = parent->get_object();
            auto it = obj.find(raw_token);
            if (it == obj.end()) {
               return make_patch_error(error_code::json_patch_invalid_path, path_not_found_msg);
            }
            it->second = value;
            return {};
         }
         if (parent->is_array()) {
            auto idx = parse_array_index(raw_token);
            if (!idx) {
               return idx.error();
            }
            auto& arr = parent->get_array();
            if (*idx >= arr.size()) {
               return make_patch_error(error_code::json_patch_invalid_path, path_not_found_msg);
            }
            arr[*idx] = value;
            return {};
         }
         return make_patch_error(error_code::json_patch_invalid_path, path_not_found_msg);
      }

      inline error_ctx test_operation(json_t& target, const std::vector<std::string>& tokens, const json_t& value)
      {
         auto resolved = resolve_pointer(target, tokens);
         if (!resolved) {
            return resolved.error();
         }
         if (!json_equals(*resolved.value(), value)) {
            return make_patch_error(error_code::json_patch_test_failed, test_failed_msg);
         }
         return {};
      }
   } // namespace detail

   inline json_t diff(const json_t& from, const json_t& to)
   {
      json_t::array_t ops{};
      detail::diff_impl(from, to, std::string{}, ops);
      return json_t(std::move(ops));
   }

   inline error_ctx patch_inplace(json_t& target, const json_t& patch_ops)
   {
      using namespace detail;
      if (!patch_ops.is_array()) {
         return make_patch_error(error_code::json_patch_invalid_document, invalid_patch_document_msg);
      }
      const auto& ops = patch_ops.get_array();
      for (const auto& op_entry : ops) {
         if (!op_entry.is_object()) {
            return make_patch_error(error_code::json_patch_invalid_document, invalid_patch_document_msg);
         }
         const auto& obj = op_entry.get_object();
         auto op_it = obj.find("op");
         if (op_it == obj.end() || !op_it->second.is_string()) {
            return make_patch_error(error_code::json_patch_invalid_operation, invalid_patch_op_msg);
         }
         const auto& op_name = op_it->second.get_string();

         auto path_it = obj.find("path");
         if (path_it == obj.end() || !path_it->second.is_string()) {
            return make_patch_error(error_code::json_patch_invalid_path, invalid_patch_path_msg);
         }
         const auto path_tokens = detail::parse_json_pointer(path_it->second.get_string());
         if (!path_tokens) {
            return path_tokens.error();
         }

         if (op_name == "add") {
            auto value_it = obj.find("value");
            if (value_it == obj.end()) {
               return make_patch_error(error_code::json_patch_missing_value, missing_value_msg);
            }
            auto ec = detail::add_operation(target, *path_tokens, value_it->second);
            if (ec) return ec;
         }
         else if (op_name == "remove") {
            auto ec = detail::remove_operation(target, *path_tokens);
            if (ec) return ec;
         }
         else if (op_name == "replace") {
            auto value_it = obj.find("value");
            if (value_it == obj.end()) {
               return make_patch_error(error_code::json_patch_missing_value, missing_value_msg);
            }
            auto ec = detail::replace_operation(target, *path_tokens, value_it->second);
            if (ec) return ec;
         }
         else if (op_name == "test") {
            auto value_it = obj.find("value");
            if (value_it == obj.end()) {
               return make_patch_error(error_code::json_patch_missing_value, missing_value_msg);
            }
            auto ec = detail::test_operation(target, *path_tokens, value_it->second);
            if (ec) return ec;
         }
         else {
            return make_patch_error(error_code::json_patch_invalid_operation, invalid_patch_op_msg);
         }
      }
      return {};
   }

   inline expected<json_t, error_ctx> patch(const json_t& target, const json_t& patch_ops)
   {
      json_t copy = target;
      if (auto ec = patch_inplace(copy, patch_ops); ec) {
         return unexpected(ec);
      }
      return glz::expected<json_t, error_ctx>{std::in_place, std::move(copy)};
   }

   namespace detail
   {
      template <class T>
      [[nodiscard]] expected<json_t, error_ctx> to_json_t(const T& value)
      {
         auto buffer = write_json(value);
         if (!buffer) {
            return unexpected(buffer.error());
         }
         auto json_value = read_json<json_t>(*buffer);
         if (!json_value) {
            return unexpected(json_value.error());
         }
         return json_value;
      }
   }

   template <class LHS, class RHS>
      requires(!std::same_as<std::remove_cvref_t<LHS>, json_t> || !std::same_as<std::remove_cvref_t<RHS>, json_t>)
   [[nodiscard]] expected<json_t, error_ctx> diff(const LHS& lhs, const RHS& rhs)
   {
      auto lhs_json = detail::to_json_t(lhs);
      if (!lhs_json) {
         return unexpected(lhs_json.error());
      }
      auto rhs_json = detail::to_json_t(rhs);
      if (!rhs_json) {
         return unexpected(rhs_json.error());
      }
      return glz::expected<json_t, error_ctx>{std::in_place, diff(*lhs_json, *rhs_json)};
   }

   template <read_supported<JSON> T>
   [[nodiscard]] expected<T, error_ctx> patch(const T& target, const json_t& patch_ops)
   {
      auto target_json = detail::to_json_t(target);
      if (!target_json) {
         return unexpected(target_json.error());
      }
      auto patched_json = patch(*target_json, patch_ops);
      if (!patched_json) {
         return unexpected(patched_json.error());
      }
      return read_json<T>(*patched_json);
   }

   template <read_supported<JSON> T>
   [[nodiscard]] error_ctx patch_inplace(T& target, const json_t& patch_ops)
   {
      auto target_json = detail::to_json_t(target);
      if (!target_json) {
         return target_json.error();
      }
      if (auto ec = patch_inplace(*target_json, patch_ops); ec) {
         return ec;
      }
      if (auto ec = read_json(target, *target_json); ec) {
         return ec;
      }
      return {};
   }
}
