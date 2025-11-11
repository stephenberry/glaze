// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#ifndef GLZ_THROW_OR_ABORT
#if __cpp_exceptions
#define GLZ_THROW_OR_ABORT(EXC) (throw(EXC))
#define GLZ_NOEXCEPT noexcept(false)
#else
#define GLZ_THROW_OR_ABORT(EXC) (std::abort())
#define GLZ_NOEXCEPT noexcept(true)
#endif
#endif

#if __cpp_exceptions
#include <stdexcept>
#endif

namespace glz
{
   inline void glaze_error([[maybe_unused]] const char* msg) GLZ_NOEXCEPT
   {
      GLZ_THROW_OR_ABORT(std::runtime_error(msg));
   }
}

#include <cstddef>
#include <map>
#include <variant>
#include <vector>

#include "glaze/api/std/string.hpp"
#include "glaze/api/std/variant.hpp"
#include "glaze/json/read.hpp"
#include "glaze/json/write.hpp"
#include "glaze/util/expected.hpp"

#if defined(_MSC_VER) && !defined(__clang__)
// Turn off broken MSVC warning for "declaration of 'v' hides previous local declaration"
#pragma warning(push)
#pragma warning(disable : 4456)
#endif

namespace glz
{
   // Generic json type.
   struct generic
   {
      virtual ~generic() {}

      using array_t = std::vector<generic>;
      using object_t = std::map<std::string, generic, std::less<>>;
      using null_t = std::nullptr_t;
      using val_t = std::variant<null_t, double, std::string, bool, array_t, object_t>;
      val_t data{};

      /**
       * @brief Converts the JSON data to a string representation.
       * @return An `expected` containing a JSON string if successful, or an error context.
       */
      expected<std::string, error_ctx> dump() const { return write_json(data); }

      /**
       * @brief Gets the value as the specified type.
       * @tparam T The type to get the value as.
       * @return Reference to the value of the specified type.
       */
      template <class T>
      [[nodiscard]] T& get()
      {
         return std::get<T>(data);
      }

      template <class T>
      [[nodiscard]] const T& get() const
      {
         return std::get<T>(data);
      }

      template <class T>
      [[nodiscard]] T* get_if() noexcept
      {
         return std::get_if<T>(&data);
      }

      template <class T>
      [[nodiscard]] const T* get_if() const noexcept
      {
         return std::get_if<T>(&data);
      }

      template <class T>
      [[nodiscard]] T as() const
      {
         // Prefer get becuase it returns a reference
         return get<T>();
      }

      template <class T>
         requires(requires { static_cast<T>(std::declval<double>()); })
      [[nodiscard]] T as() const
      {
         // Can be used for int and the like
         return static_cast<T>(get<double>());
      }

      template <class T>
         requires std::convertible_to<std::string, T>
      [[nodiscard]] T as() const
      {
         // Can be used for string_view and the like
         return get<std::string>();
      }

      template <class T>
      [[nodiscard]] bool holds() const noexcept
      {
         return std::holds_alternative<T>(data);
      }

      generic& operator[](std::integral auto&& index) { return std::get<array_t>(data)[index]; }

      const generic& operator[](std::integral auto&& index) const { return std::get<array_t>(data)[index]; }

      generic& operator[](std::convertible_to<std::string_view> auto&& key)
      {
         //[] operator for maps does not support heterogeneous lookups yet
         if (holds<null_t>()) data = object_t{};
         auto& object = std::get<object_t>(data);
         auto iter = object.find(key);
         if (iter == object.end()) {
            iter = object.insert(std::make_pair(std::string(key), generic{})).first;
         }
         return iter->second;
      }

      const generic& operator[](std::convertible_to<std::string_view> auto&& key) const
      {
         //[] operator for maps does not support heterogeneous lookups yet
         auto& object = std::get<object_t>(data);
         auto iter = object.find(key);
         if (iter == object.end()) {
            glaze_error("Key not found.");
         }
         return iter->second;
      }

      generic& operator=(const std::nullptr_t value)
      {
         data = value;
         return *this;
      }

      generic& operator=(const double value)
      {
         data = value;
         return *this;
      }

      // for integers
      template <int_t T>
      generic& operator=(const T value)
      {
         data = static_cast<double>(value);
         return *this;
      }

      generic& operator=(const std::string& value)
      {
         data = value;
         return *this;
      }

      generic& operator=(const std::string_view value)
      {
         data = std::string(value);
         return *this;
      }

      generic& operator=(const char* value)
      {
         data = std::string(value);
         return *this;
      }

      generic& operator=(const bool value)
      {
         data = value;
         return *this;
      }

      generic& operator=(const array_t& value)
      {
         data = value;
         return *this;
      }

      generic& operator=(const object_t& value)
      {
         data = value;
         return *this;
      }

      template <class T>
         requires(!std::is_same_v<std::decay_t<T>, generic> && !std::is_same_v<std::decay_t<T>, std::nullptr_t> &&
                  !std::is_same_v<std::decay_t<T>, double> && !std::is_same_v<std::decay_t<T>, bool> &&
                  !std::is_same_v<std::decay_t<T>, std::string> && !std::is_same_v<std::decay_t<T>, std::string_view> &&
                  !std::is_same_v<std::decay_t<T>, const char*> && !std::is_same_v<std::decay_t<T>, array_t> &&
                  !std::is_same_v<std::decay_t<T>, object_t> && !int_t<T> &&
                  requires { write_json(std::declval<T>()); })
      generic& operator=(T&& value)
      {
         auto json_str = write_json(std::forward<T>(value));
         if (json_str) {
            auto result = read_json<generic>(*json_str);
            if (result) {
               *this = std::move(*result);
            }
         }
         return *this;
      }

      [[nodiscard]] generic& at(std::convertible_to<std::string_view> auto&& key) { return operator[](key); }

      [[nodiscard]] const generic& at(std::convertible_to<std::string_view> auto&& key) const
      {
         return operator[](key);
      }

      [[nodiscard]] bool contains(std::convertible_to<std::string_view> auto&& key) const
      {
         if (!holds<object_t>()) return false;
         auto& object = std::get<object_t>(data);
         auto iter = object.find(key);
         return iter != object.end();
      }

      explicit operator bool() const { return !std::holds_alternative<null_t>(data); }

      val_t* operator->() noexcept { return &data; }

      val_t& operator*() noexcept { return data; }

      const val_t& operator*() const noexcept { return data; }

      void reset() noexcept { data = null_t{}; }

      generic() = default;
      generic(const generic&) = default;
      generic& operator=(const generic&) = default;
      generic(generic&&) = default;
      generic& operator=(generic&&) = default;

      template <class T>
         requires std::convertible_to<T, val_t> && (!std::derived_from<std::decay_t<T>, generic>)
      generic(T&& val)
      {
         data = val;
      }

      template <class T>
         requires std::convertible_to<T, double> && (!std::derived_from<std::decay_t<T>, generic>) &&
                  (!std::convertible_to<T, val_t>)
      generic(T&& val)
      {
         data = static_cast<double>(val);
      }

      generic(const std::string_view value) { data = std::string(value); }

      generic(std::initializer_list<std::pair<const char*, generic>>&& obj)
      {
         data.emplace<object_t>();
         auto& data_obj = std::get<object_t>(data);
         for (auto&& pair : obj) {
            // std::initializer_list holds const values that cannot be moved
            data_obj.emplace(pair.first, pair.second);
         }
      }

      // Prevent conflict with object initializer list
      template <bool deprioritize = true>
      generic(std::initializer_list<generic>&& arr)
      {
         data.emplace<array_t>(std::move(arr));
      }

      [[nodiscard]] bool is_array() const noexcept { return holds<generic::array_t>(); }

      [[nodiscard]] bool is_object() const noexcept { return holds<generic::object_t>(); }

      [[nodiscard]] bool is_number() const noexcept { return holds<double>(); }

      [[nodiscard]] bool is_string() const noexcept { return holds<std::string>(); }

      [[nodiscard]] bool is_boolean() const noexcept { return holds<bool>(); }

      [[nodiscard]] bool is_null() const noexcept { return holds<std::nullptr_t>(); }

      [[nodiscard]] array_t& get_array() { return get<array_t>(); }
      [[nodiscard]] const array_t& get_array() const { return get<array_t>(); }

      [[nodiscard]] object_t& get_object() { return get<object_t>(); }
      [[nodiscard]] const object_t& get_object() const { return get<object_t>(); }

      [[nodiscard]] double& get_number() { return get<double>(); }
      [[nodiscard]] const double& get_number() const { return get<double>(); }

      [[nodiscard]] std::string& get_string() { return get<std::string>(); }
      [[nodiscard]] const std::string& get_string() const { return get<std::string>(); }

      [[nodiscard]] bool& get_boolean() { return get<bool>(); }
      [[nodiscard]] const bool& get_boolean() const { return get<bool>(); }

      // empty() returns true if the value is an empty JSON object, array, or string, or a null value
      // otherwise returns false
      [[nodiscard]] bool empty() const noexcept
      {
         if (auto* v = get_if<object_t>()) {
            return v->empty();
         }
         else if (auto* v = get_if<array_t>()) {
            return v->empty();
         }
         else if (auto* v = get_if<std::string>()) {
            return v->empty();
         }
         else if (is_null()) {
            return true;
         }
         else {
            return false;
         }
      }

      // returns the count of items in an object or an array, or the size of a string, otherwise returns zero
      [[nodiscard]] size_t size() const noexcept
      {
         if (auto* v = get_if<object_t>()) {
            return v->size();
         }
         else if (auto* v = get_if<array_t>()) {
            return v->size();
         }
         else if (auto* v = get_if<std::string>()) {
            return v->size();
         }
         else {
            return 0;
         }
      }
   };

   // Backwards compatibility alias
   using json_t [[deprecated("glz::json_t is deprecated, use glz::generic instead")]] = generic;

   [[nodiscard]] inline bool is_array(const generic& value) noexcept { return value.is_array(); }

   [[nodiscard]] inline bool is_object(const generic& value) noexcept { return value.is_object(); }

   [[nodiscard]] inline bool is_number(const generic& value) noexcept { return value.is_number(); }

   [[nodiscard]] inline bool is_string(const generic& value) noexcept { return value.is_string(); }

   [[nodiscard]] inline bool is_boolean(const generic& value) noexcept { return value.is_boolean(); }

   [[nodiscard]] inline bool is_null(const generic& value) noexcept { return value.is_null(); }
}

template <>
struct glz::meta<glz::generic>
{
   static constexpr std::string_view name = "glz::generic";
   using T = glz::generic;
   static constexpr auto value = &T::data;
};

namespace glz
{
   // Concept for types that can be directly converted from generic without JSON round-trip
   template <class T>
   concept directly_convertible_from_generic =
      std::same_as<T, bool> || std::same_as<T, double> || std::same_as<T, std::string> ||
      std::same_as<T, std::string_view> || std::integral<T> || (readable_array_t<T> && !std::same_as<T, std::string>) ||
      readable_map_t<T>;

   // These functions allow a generic value to be read/written to a C++ struct

   // Optimized overload for types that support direct conversion
   template <auto Opts, class T>
      requires read_supported<T, Opts.format> && directly_convertible_from_generic<T>
   [[nodiscard]] error_ctx read(T& value, const generic& source)
   {
      return convert_from_generic(value, source);
   }

   // General overload for complex types (structs, etc.) that need JSON round-trip
   template <auto Opts, class T>
      requires read_supported<T, Opts.format> && (!directly_convertible_from_generic<T>)
   [[nodiscard]] error_ctx read(T& value, const generic& source)
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
   template <read_supported<JSON> T>
      requires directly_convertible_from_generic<T>
   [[nodiscard]] error_ctx read_json(T& value, const generic& source)
   {
      return convert_from_generic(value, source);
   }

   // General overload for complex types (structs, etc.) that need JSON round-trip
   template <read_supported<JSON> T>
      requires(!directly_convertible_from_generic<T>)
   [[nodiscard]] error_ctx read_json(T& value, const generic& source)
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
   template <read_supported<JSON> T>
      requires directly_convertible_from_generic<T>
   [[nodiscard]] expected<T, error_ctx> read_json(const generic& source)
   {
      T result;
      auto ec = convert_from_generic(result, source);
      if (ec) {
         return unexpected(ec);
      }
      return result;
   }

   // General overload for complex types (structs, etc.) that need JSON round-trip
   template <read_supported<JSON> T>
      requires(!directly_convertible_from_generic<T>)
   [[nodiscard]] expected<T, error_ctx> read_json(const generic& source)
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

#if defined(_MSC_VER) && !defined(__clang__)
// restore disabled warning
#pragma warning(pop)
#endif

#include "glaze/core/seek.hpp"

namespace glz
{
   // Specialization for glz::generic
   template <>
   struct seek_op<glz::generic>
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

   // Helper function to navigate to a specific location in a glz::generic using a JSON pointer
   // Returns a pointer to the generic at that location, or nullptr if not found
   // Template implementation to handle both const and non-const cases
   template <class GenericPtr>
   inline auto navigate_to_impl(GenericPtr root, sv json_ptr) noexcept -> GenericPtr
   {
      if (!root) return nullptr;
      if (json_ptr.empty()) return root;
      if (json_ptr[0] != '/' || json_ptr.size() < 2) return nullptr;

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
   inline generic* navigate_to(generic* root, sv json_ptr) noexcept { return navigate_to_impl(root, json_ptr); }

   // Const version
   inline const generic* navigate_to(const generic* root, sv json_ptr) noexcept
   {
      return navigate_to_impl(root, json_ptr);
   }

   // Direct conversion from generic to target types without JSON serialization
   // Takes result by reference to allow memory reuse and avoid extra moves

   // Forward declaration for recursive conversion
   template <class T>
   error_ctx convert_from_generic(T& result, const generic& source);

   // Specialization for bool
   template <>
   inline error_ctx convert_from_generic<bool>(bool& result, const generic& source)
   {
      if (!source.is_boolean()) {
         return error_ctx{error_code::syntax_error};
      }
      result = source.get<bool>();
      return {};
   }

   // Specialization for double
   template <>
   inline error_ctx convert_from_generic<double>(double& result, const generic& source)
   {
      if (!source.is_number()) {
         return error_ctx{error_code::syntax_error};
      }
      result = source.get<double>();
      return {};
   }

   // Specialization for string
   template <>
   inline error_ctx convert_from_generic<std::string>(std::string& result, const generic& source)
   {
      if (!source.is_string()) {
         return error_ctx{error_code::syntax_error};
      }
      result = source.get<std::string>();
      return {};
   }

   // Specialization for integer types (convert from double)
   template <class T>
      requires(int_t<T>)
   error_ctx convert_from_generic(T& result, const generic& source)
   {
      if (!source.is_number()) {
         return error_ctx{error_code::syntax_error};
      }
      result = static_cast<T>(source.get<double>());
      return {};
   }

   // Specialization for array-like containers
   template <class T>
      requires(readable_array_t<T> && !std::same_as<T, std::string>)
   error_ctx convert_from_generic(T& result, const generic& source)
   {
      if (!source.is_array()) {
         return error_ctx{error_code::syntax_error};
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
               return error_ctx{error_code::syntax_error};
            }
            result[index] = std::move(converted);
         }
         else if constexpr (emplaceable<T>) {
            result.emplace(std::move(converted));
         }
         else {
            return error_ctx{error_code::syntax_error};
         }

         ++index;
      }

      return {};
   }

   // Specialization for map-like containers
   template <class T>
      requires readable_map_t<T>
   error_ctx convert_from_generic(T& result, const generic& source)
   {
      if (!source.is_object()) {
         return error_ctx{error_code::syntax_error};
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
            return error_ctx{error_code::syntax_error};
         }
      }

      return {};
   }

   // Concept to determine if a type needs deserialization from generic
   // These are container types that can't be directly referenced from glz::generic
   // because they're not the exact types stored in generic's variant
   template <class V>
   concept needs_container_deserialization =
      (readable_array_t<V> || readable_map_t<V>) && !std::same_as<V, generic> && !std::same_as<V, generic::array_t> &&
      !std::same_as<V, generic::object_t> && !std::same_as<V, std::string> && !std::same_as<V, double> &&
      !std::same_as<V, bool> && !std::same_as<V, std::nullptr_t>;

   // Overload of get for glz::generic that deserializes containers
   // This overload is only selected for container types that need deserialization
   // Uses direct traversal instead of JSON serialization for better performance
   template <class V>
      requires needs_container_deserialization<V>
   expected<V, error_ctx> get(generic& root, sv json_ptr)
   {
      // Navigate to the target location
      generic* target = navigate_to(&root, json_ptr);
      if (!target) {
         return unexpected(error_ctx{error_code::get_nonexistent_json_ptr});
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
   template <class V>
      requires needs_container_deserialization<V>
   expected<V, error_ctx> get(const generic& root, sv json_ptr)
   {
      const generic* target = navigate_to(&root, json_ptr);
      if (!target) {
         return unexpected(error_ctx{error_code::get_nonexistent_json_ptr});
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
