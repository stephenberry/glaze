// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/common.hpp"
#include "glaze/json/generic_fwd.hpp"
#include "glaze/json/write.hpp"

namespace glz
{

   template <num_mode Mode, template <class> class MapType>
      requires generic_map_type<MapType>
   expected<std::string, error_ctx> generic_json<Mode, MapType>::dump() const
   {
      return write_json(data);
   }

   template <num_mode Mode, template <class> class MapType>
      requires generic_map_type<MapType>
   [[nodiscard]] std::string format_error(const error_ctx& ec, const generic_json<Mode, MapType>& source)
   {
      const auto buffer = source.dump();
      if (buffer) {
         return format_error(ec, *buffer);
      }
      return format_error(ec);
   }

   template <num_mode Mode, template <class> class MapType>
      requires generic_map_type<MapType>
   template <class T>
      requires(assignable_generic_type<generic_json<Mode, MapType>, T>)
   generic_json<Mode, MapType>& generic_json<Mode, MapType>::operator=(T&& value)
   {
      static_assert(requires { write_json(std::declval<T>()); });
      auto json_str = write_json(std::forward<T>(value));
      if (json_str) {
         auto result = read_json<generic_json>(*json_str);
         if (result) {
            *this = std::move(*result);
         }
      }
      return *this;
   }

   // Concept for types that can be directly converted from generic without JSON round-trip
   template <class T>
   concept directly_convertible_from_generic =
      std::same_as<T, bool> || std::same_as<T, double> || std::same_as<T, std::string> ||
      std::same_as<T, std::string_view> || std::integral<T> || (readable_array_t<T> && !std::same_as<T, std::string>) ||
      readable_map_t<T>;

   // These functions allow a generic value to be read/written to a C++ struct

   // Optimized overload for types that support direct conversion
   template <auto Opts, class T, num_mode Mode, template <class> class MapType>
      requires read_supported<T, Opts.format> && directly_convertible_from_generic<T>
   [[nodiscard]] error_ctx read(T& value, const generic_json<Mode, MapType>& source)
   {
      return convert_from_generic(value, source);
   }

   // General overload for complex types (structs, etc.) that need JSON round-trip
   template <auto Opts, class T, num_mode Mode, template <class> class MapType>
      requires read_supported<T, Opts.format> && (!directly_convertible_from_generic<T>)
   [[nodiscard]] error_ctx read(T& value, const generic_json<Mode, MapType>& source)
   {
      auto buffer = source.dump();
      if (buffer) {
         context ctx{};
         return read<Opts>(value, *buffer, ctx);
      }
      else {
         return buffer.error();
      }
   }

   // Optimized overload for types that support direct conversion
   template <read_supported<JSON> T, num_mode Mode, template <class> class MapType>
      requires directly_convertible_from_generic<T>
   [[nodiscard]] error_ctx read_json(T& value, const generic_json<Mode, MapType>& source)
   {
      return convert_from_generic(value, source);
   }

   // General overload for complex types (structs, etc.) that need JSON round-trip
   template <read_supported<JSON> T, num_mode Mode, template <class> class MapType>
      requires(!directly_convertible_from_generic<T>)
   [[nodiscard]] error_ctx read_json(T& value, const generic_json<Mode, MapType>& source)
   {
      auto buffer = source.dump();
      if (buffer) {
         return read_json(value, *buffer);
      }
      else {
         return buffer.error();
      }
   }

   // Optimized overload for types that support direct conversion
   template <read_supported<JSON> T, num_mode Mode, template <class> class MapType>
      requires directly_convertible_from_generic<T>
   [[nodiscard]] expected<T, error_ctx> read_json(const generic_json<Mode, MapType>& source)
   {
      T result;
      auto ec = convert_from_generic(result, source);
      if (ec) {
         return unexpected(ec);
      }
      return result;
   }

   // General overload for complex types (structs, etc.) that need JSON round-trip
   template <read_supported<JSON> T, num_mode Mode, template <class> class MapType>
      requires(!directly_convertible_from_generic<T>)
   [[nodiscard]] expected<T, error_ctx> read_json(const generic_json<Mode, MapType>& source)
   {
      auto buffer = source.dump();
      if (buffer) {
         return read_json<T>(*buffer);
      }
      else {
         return unexpected(buffer.error());
      }
   }
}

#include "glaze/core/seek.hpp"

namespace glz
{
   // Specialization for glz::generic_json in all number handling modes and map types
   template <glz::num_mode Mode, template <class> class MapType>
   struct seek_op<glz::generic_json<Mode, MapType>>
   {
      template <class F>
      static bool op(F&& func, auto&& value, sv json_ptr)
      {
         if (json_ptr.empty()) {
            // At target - call func with the actual variant data, not the generic wrapper
            std::visit([&func](auto&& variant_value) { func(variant_value); }, value.data);
            return true;
         }

         if (json_ptr[0] != '/' || json_ptr.size() < 2) return false;

         // Handle object access
         if (value.is_object()) {
            auto& obj = value.get_object();

            // Parse the key (with JSON Pointer escaping)
            std::string key;
            size_t i = 1;
            for (; i < json_ptr.size(); ++i) {
               auto c = json_ptr[i];
               if (c == '/')
                  break;
               else if (c == '~') {
                  if (++i == json_ptr.size()) return false;
                  c = json_ptr[i];
                  if (c == '0')
                     c = '~';
                  else if (c == '1')
                     c = '/';
                  else
                     return false;
               }
               key.push_back(c);
            }

            auto it = obj.find(key);
            if (it == obj.end()) return false;

            sv remaining_ptr = json_ptr.substr(i);
            return seek(std::forward<F>(func), it->second, remaining_ptr);
         }
         // Handle array access
         else if (value.is_array()) {
            auto& arr = value.get_array();

            // Parse the index
            size_t index{};
            auto [p, ec] = std::from_chars(&json_ptr[1], json_ptr.data() + json_ptr.size(), index);
            if (ec != std::errc{}) return false;

            if (index >= arr.size()) return false;

            sv remaining_ptr = json_ptr.substr(p - json_ptr.data());
            return seek(std::forward<F>(func), arr[index], remaining_ptr);
         }

         return false;
      }
   };

   // Helper function to navigate to a specific location in a glz::generic_json using a JSON pointer
   // Returns a pointer to the generic_json at that location, or nullptr if not found
   // Template implementation to handle both const and non-const cases
   template <class GenericPtr>
   inline auto navigate_to_impl(GenericPtr root, sv json_ptr) noexcept -> GenericPtr
   {
      if (!root) return nullptr;
      if (json_ptr.empty()) return root;
      // RFC 6901: "/" is valid (refers to empty string key), so don't require size >= 2
      if (json_ptr[0] != '/') return nullptr;

      GenericPtr current = root;
      sv remaining = json_ptr;

      while (!remaining.empty() && remaining[0] == '/') {
         remaining.remove_prefix(1); // Remove leading '/'

         // Find the next '/' or end of string
         size_t key_end = remaining.find('/');
         sv key = (key_end == sv::npos) ? remaining : remaining.substr(0, key_end);

         // Check if JSON Pointer escaping is needed
         const bool needs_unescape = key.find('~') != sv::npos;

         // Navigate based on current type
         if (current->is_object()) {
            auto& obj = current->get_object();

            if (needs_unescape) {
               // Only allocate string when escaping is present
               std::string unescaped_key;
               unescaped_key.reserve(key.size());

               for (size_t i = 0; i < key.size(); ++i) {
                  if (key[i] == '~' && i + 1 < key.size()) {
                     if (key[i + 1] == '0')
                        unescaped_key += '~';
                     else if (key[i + 1] == '1')
                        unescaped_key += '/';
                     else
                        return nullptr; // Invalid escape sequence
                     ++i;
                  }
                  else {
                     unescaped_key += key[i];
                  }
               }

               auto it = obj.find(unescaped_key);
               if (it == obj.end()) return nullptr;
               current = &(it->second);
            }
            else {
               // Use heterogeneous lookup with string_view (no allocation)
               auto it = obj.find(key);
               if (it == obj.end()) return nullptr;
               current = &(it->second);
            }
         }
         else if (current->is_array()) {
            // Array indices must be plain numbers (no escaping applies to indices)
            // If key contains '~', it's invalid as an array index and will fail to parse
            size_t index = 0;
            auto [ptr, ec] = std::from_chars(key.data(), key.data() + key.size(), index);
            if (ec != std::errc{} || ptr != key.data() + key.size()) {
               return nullptr; // Invalid index
            }
            auto& arr = current->get_array();
            if (index >= arr.size()) return nullptr;
            current = &arr[index];
         }
         else {
            return nullptr; // Can't navigate further
         }

         // Move to next segment
         if (key_end == sv::npos) {
            remaining = sv{};
         }
         else {
            remaining = remaining.substr(key_end);
         }
      }

      return current;
   }

   // Non-const version
   template <num_mode Mode, template <class> class MapType>
   inline generic_json<Mode, MapType>* navigate_to(generic_json<Mode, MapType>* root, sv json_ptr) noexcept
   {
      return navigate_to_impl(root, json_ptr);
   }

   // Const version
   template <num_mode Mode, template <class> class MapType>
   inline const generic_json<Mode, MapType>* navigate_to(const generic_json<Mode, MapType>* root, sv json_ptr) noexcept
   {
      return navigate_to_impl(root, json_ptr);
   }

   // Direct conversion from generic to target types without JSON serialization
   // Takes result by reference to allow memory reuse and avoid extra moves

   // Forward declaration for recursive conversion
   template <class T, num_mode Mode, template <class> class MapType>
   error_ctx convert_from_generic(T& result, const generic_json<Mode, MapType>& source);

   // Specialization for bool
   template <num_mode Mode, template <class> class MapType>
   inline error_ctx convert_from_generic(bool& result, const generic_json<Mode, MapType>& source)
   {
      if (!source.is_boolean()) {
         return error_ctx{0, error_code::syntax_error};
      }
      result = source.template get<bool>();
      return {};
   }

   // Specialization for double
   template <num_mode Mode, template <class> class MapType>
   inline error_ctx convert_from_generic(double& result, const generic_json<Mode, MapType>& source)
   {
      if (!source.is_number()) {
         return error_ctx{0, error_code::syntax_error};
      }
      if constexpr (Mode == num_mode::u64) {
         if (source.template holds<uint64_t>()) {
            result = static_cast<double>(source.template get<uint64_t>());
         }
         else if (source.template holds<int64_t>()) {
            result = static_cast<double>(source.template get<int64_t>());
         }
         else {
            result = source.template get<double>();
         }
      }
      else if constexpr (Mode == num_mode::i64) {
         if (source.template holds<int64_t>()) {
            result = static_cast<double>(source.template get<int64_t>());
         }
         else {
            result = source.template get<double>();
         }
      }
      else {
         result = source.template get<double>();
      }
      return {};
   }

   // Specialization for uint64_t (only in u64 mode)
   template <num_mode Mode, template <class> class MapType>
      requires(Mode == num_mode::u64)
   inline error_ctx convert_from_generic(uint64_t& result, const generic_json<Mode, MapType>& source)
   {
      if (!source.is_number()) {
         return error_ctx{0, error_code::syntax_error};
      }
      if (source.template holds<uint64_t>()) {
         result = source.template get<uint64_t>();
      }
      else if (source.template holds<int64_t>()) {
         result = static_cast<uint64_t>(source.template get<int64_t>());
      }
      else {
         result = static_cast<uint64_t>(source.template get<double>());
      }
      return {};
   }

   // Specialization for int64_t (in i64 and u64 modes)
   template <num_mode Mode, template <class> class MapType>
      requires(Mode != num_mode::f64)
   inline error_ctx convert_from_generic(int64_t& result, const generic_json<Mode, MapType>& source)
   {
      if (!source.is_number()) {
         return error_ctx{0, error_code::syntax_error};
      }
      if constexpr (Mode == num_mode::u64) {
         if (source.template holds<uint64_t>()) {
            result = static_cast<int64_t>(source.template get<uint64_t>());
         }
         else if (source.template holds<int64_t>()) {
            result = source.template get<int64_t>();
         }
         else {
            result = static_cast<int64_t>(source.template get<double>());
         }
      }
      else {
         if (source.template holds<int64_t>()) {
            result = source.template get<int64_t>();
         }
         else {
            result = static_cast<int64_t>(source.template get<double>());
         }
      }
      return {};
   }

   // Specialization for string
   template <num_mode Mode, template <class> class MapType>
   inline error_ctx convert_from_generic(std::string& result, const generic_json<Mode, MapType>& source)
   {
      if (!source.is_string()) {
         return error_ctx{0, error_code::syntax_error};
      }
      result = source.template get<std::string>();
      return {};
   }

   // Specialization for integer types (convert from integer types or double)
   template <class T, num_mode Mode, template <class> class MapType>
      requires(std::integral<T> && !std::same_as<T, bool> && !std::same_as<T, int64_t> && !std::same_as<T, uint64_t>)
   error_ctx convert_from_generic(T& result, const generic_json<Mode, MapType>& source)
   {
      if (!source.is_number()) {
         return error_ctx{0, error_code::syntax_error};
      }
      if constexpr (Mode == num_mode::u64) {
         if (source.template holds<uint64_t>()) {
            result = static_cast<T>(source.template get<uint64_t>());
         }
         else if (source.template holds<int64_t>()) {
            result = static_cast<T>(source.template get<int64_t>());
         }
         else {
            result = static_cast<T>(source.template get<double>());
         }
      }
      else if constexpr (Mode == num_mode::i64) {
         if (source.template holds<int64_t>()) {
            result = static_cast<T>(source.template get<int64_t>());
         }
         else {
            result = static_cast<T>(source.template get<double>());
         }
      }
      else {
         result = static_cast<T>(source.template get<double>());
      }
      return {};
   }

   // Specialization for array-like containers
   template <class T, num_mode Mode, template <class> class MapType>
      requires(readable_array_t<T> && !std::same_as<T, std::string>)
   error_ctx convert_from_generic(T& result, const generic_json<Mode, MapType>& source)
   {
      if (!source.is_array()) {
         return error_ctx{0, error_code::syntax_error};
      }

      const auto& arr = source.get_array();

      if constexpr (resizable<T>) {
         result.clear();
         if constexpr (has_reserve<T>) {
            result.reserve(arr.size());
         }
      }

      size_t index = 0;
      for (const auto& elem : arr) {
         using value_type = range_value_t<T>;
         value_type converted;
         auto ec = convert_from_generic(converted, elem);
         if (ec) {
            return ec;
         }

         if constexpr (resizable<T> && push_backable<T>) {
            result.push_back(std::move(converted));
         }
         else if constexpr (resizable<T> && emplace_backable<T>) {
            result.emplace_back(std::move(converted));
         }
         else if constexpr (accessible<T>) {
            if (index >= result.size()) {
               return error_ctx{0, error_code::syntax_error};
            }
            result[index] = std::move(converted);
         }
         else if constexpr (emplaceable<T>) {
            result.emplace(std::move(converted));
         }
         else {
            return error_ctx{0, error_code::syntax_error};
         }

         ++index;
      }

      return {};
   }

   // Specialization for map-like containers
   template <class T, num_mode Mode, template <class> class MapType>
      requires readable_map_t<T>
   error_ctx convert_from_generic(T& result, const generic_json<Mode, MapType>& source)
   {
      if (!source.is_object()) {
         return error_ctx{0, error_code::syntax_error};
      }

      const auto& obj = source.get_object();

      for (const auto& [key, value] : obj) {
         using mapped_type = typename T::mapped_type;
         mapped_type converted;
         auto ec = convert_from_generic(converted, value);
         if (ec) {
            return ec;
         }

         if constexpr (map_subscriptable<T>) {
            result[key] = std::move(converted);
         }
         else if constexpr (emplaceable<T>) {
            result.emplace(key, std::move(converted));
         }
         else {
            return error_ctx{0, error_code::syntax_error};
         }
      }

      return {};
   }

   // Concept to determine if a type needs deserialization from generic
   // These are container types that can't be directly referenced from glz::generic
   // because they're not the exact types stored in generic's variant
   template <class V, class GenericType>
   concept needs_container_deserialization_for =
      (readable_array_t<V> || readable_map_t<V>) && !std::same_as<V, GenericType> &&
      !std::same_as<V, typename GenericType::array_t> && !std::same_as<V, typename GenericType::object_t> &&
      !std::same_as<V, std::string> && !std::same_as<V, double> && !std::same_as<V, bool> &&
      !std::same_as<V, std::nullptr_t>;

   // Legacy concept for backward compatibility
   template <class V>
   concept needs_container_deserialization = needs_container_deserialization_for<V, generic>;

   // Overload of get for glz::generic_json that deserializes containers
   // This overload is only selected for container types that need deserialization
   // Uses direct traversal instead of JSON serialization for better performance
   template <class V, num_mode Mode, template <class> class MapType>
      requires needs_container_deserialization_for<V, generic_json<Mode, MapType>>
   expected<V, error_ctx> get(generic_json<Mode, MapType>& root, sv json_ptr)
   {
      // Navigate to the target location
      generic_json<Mode, MapType>* target = navigate_to(&root, json_ptr);
      if (!target) {
         return unexpected(error_ctx{0, error_code::nonexistent_json_ptr});
      }

      // Direct conversion without JSON serialization round-trip
      V result;
      auto ec = convert_from_generic(result, *target);
      if (ec) {
         return unexpected(ec);
      }
      return result;
   }

   // Const version
   template <class V, num_mode Mode, template <class> class MapType>
      requires needs_container_deserialization_for<V, generic_json<Mode, MapType>>
   expected<V, error_ctx> get(const generic_json<Mode, MapType>& root, sv json_ptr)
   {
      const generic_json<Mode, MapType>* target = navigate_to(&root, json_ptr);
      if (!target) {
         return unexpected(error_ctx{0, error_code::nonexistent_json_ptr});
      }

      // Direct conversion without JSON serialization round-trip
      V result;
      auto ec = convert_from_generic(result, *target);
      if (ec) {
         return unexpected(ec);
      }
      return result;
   }
}
