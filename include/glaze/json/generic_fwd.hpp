// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#ifndef GLZ_THROW_OR_ABORT
#if __cpp_exceptions
#define GLZ_THROW_OR_ABORT(EXC) (throw(EXC))
#else
#define GLZ_THROW_OR_ABORT(EXC) (std::abort())
#endif
#endif

#ifndef GLZ_NOEXCEPT
#if __cpp_exceptions
#define GLZ_NOEXCEPT noexcept(false)
#else
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

#include "glaze/containers/ordered_small_map.hpp"
#include "glaze/core/context.hpp"
#include "glaze/forward.hpp"
#include "glaze/util/expected.hpp"

#if defined(_MSC_VER) && !defined(__clang__)
// Turn off broken MSVC warning for "declaration of 'v' hides previous local declaration"
#pragma warning(push)
#pragma warning(disable : 4456)
#endif

namespace glz
{
   // Number storage mode for generic JSON types
   enum class num_mode {
      f64, // double only - fast, JavaScript-compatible (default)
      i64, // int64_t → double - signed integer precision
      u64 // uint64_t → int64_t → double - full integer range
   };

   // Helper to determine variant type based on mode
   template <num_mode Mode, class null_t, class array_t, class object_t>
   struct generic_val_type;

   template <class null_t, class array_t, class object_t>
   struct generic_val_type<num_mode::f64, null_t, array_t, object_t>
   {
      using type = std::variant<null_t, double, std::string, bool, array_t, object_t>;
   };

   template <class null_t, class array_t, class object_t>
   struct generic_val_type<num_mode::i64, null_t, array_t, object_t>
   {
      using type = std::variant<null_t, int64_t, double, std::string, bool, array_t, object_t>;
   };

   template <class null_t, class array_t, class object_t>
   struct generic_val_type<num_mode::u64, null_t, array_t, object_t>
   {
      using type = std::variant<null_t, uint64_t, int64_t, double, std::string, bool, array_t, object_t>;
   };

   // Concept for map types usable with generic_json.
   // MapType must be a template taking a single type parameter (the mapped value type),
   // with std::string keys and support for heterogeneous lookup via std::string_view.
   template <template <class> class MapType>
   concept generic_map_type =
      requires(MapType<int>& m, const MapType<int>& cm, std::string_view sv, std::pair<std::string, int> p) {
         { m.find(sv) };
         { cm.find(sv) };
         { m.insert(std::move(p)) };
         { cm.empty() } -> std::convertible_to<bool>;
         { cm.size() } -> std::convertible_to<std::size_t>;
         { m.begin() };
         { m.end() };
      };

   template <class generic_json_t, class T>
   concept assignable_generic_type =
      !std::is_same_v<std::decay_t<T>, generic_json_t> && !std::is_same_v<std::decay_t<T>, std::nullptr_t> &&
      !std::is_same_v<std::decay_t<T>, double> && !std::is_same_v<std::decay_t<T>, bool> &&
      !std::is_same_v<std::decay_t<T>, std::string> && !std::is_same_v<std::decay_t<T>, std::string_view> &&
      !std::is_same_v<std::decay_t<T>, const char*> &&
      !std::is_same_v<std::decay_t<T>, typename generic_json_t::array_t> &&
      !std::is_same_v<std::decay_t<T>, typename generic_json_t::object_t> && !std::integral<std::decay_t<T>> &&
      !std::floating_point<std::decay_t<T>>;

   // Generic json type.
   // First template parameter controls number storage mode for precise integer handling.
   // Second template parameter controls the map type used for JSON objects.
   // MapType must satisfy generic_map_type (heterogeneous find with std::string_view).
   template <num_mode Mode = num_mode::f64, template <class> class MapType = ordered_small_map>
      requires generic_map_type<MapType>
   struct generic_json
   {
      ~generic_json() = default;

      using array_t = std::vector<generic_json<Mode, MapType>>;
      using object_t = MapType<generic_json<Mode, MapType>>;
      using null_t = std::nullptr_t;

      // Variant type depends on Mode:
      // - f64: double only (fast, JS-compatible)
      // - i64: int64_t, then double (signed integer precision)
      // - u64: uint64_t, then int64_t, then double (full integer range)
      using val_t = typename generic_val_type<Mode, null_t, array_t, object_t>::type;

      val_t data;

      /**
       * @brief Converts the JSON data to a string representation.
       * @return An `expected` containing a JSON string if successful, or an error context.
       */
      expected<std::string, error_ctx> dump() const;

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
         // Prefer get because it returns a reference
         return get<T>();
      }

      template <class T>
         requires(requires { static_cast<T>(std::declval<double>()); })
      [[nodiscard]] T as() const
      {
         // Can be used for int and the like
         if constexpr (Mode == num_mode::u64) {
            if (holds<uint64_t>()) {
               return static_cast<T>(get<uint64_t>());
            }
            if (holds<int64_t>()) {
               return static_cast<T>(get<int64_t>());
            }
         }
         else if constexpr (Mode == num_mode::i64) {
            if (holds<int64_t>()) {
               return static_cast<T>(get<int64_t>());
            }
         }
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

      generic_json& operator[](std::integral auto&& index) { return std::get<array_t>(data)[index]; }

      const generic_json& operator[](std::integral auto&& index) const { return std::get<array_t>(data)[index]; }

      generic_json& operator[](std::convertible_to<std::string_view> auto&& key)
      {
         if (holds<null_t>()) data = object_t{};
         auto& object = std::get<object_t>(data);
         auto iter = object.find(key);
         if (iter == object.end()) {
            iter = object.insert(std::make_pair(std::string(key), generic_json{})).first;
         }
         return iter->second;
      }

      const generic_json& operator[](std::convertible_to<std::string_view> auto&& key) const
      {
         auto& object = std::get<object_t>(data);
         auto iter = object.find(key);
         if (iter == object.end()) {
            glaze_error("Key not found.");
         }
         return iter->second;
      }

      generic_json& operator=(const std::nullptr_t value)
      {
         data = value;
         return *this;
      }

      generic_json& operator=(const double value)
      {
         data = value;
         return *this;
      }

      // for unsigned integers
      template <class T>
         requires(std::unsigned_integral<T> && !std::same_as<T, bool>)
      generic_json& operator=(const T value)
      {
         if constexpr (Mode == num_mode::u64) {
            data = static_cast<uint64_t>(value);
         }
         else if constexpr (Mode == num_mode::i64) {
            data = static_cast<int64_t>(value);
         }
         else {
            data = static_cast<double>(value);
         }
         return *this;
      }

      // for signed integers
      template <class T>
         requires(std::signed_integral<T>)
      generic_json& operator=(const T value)
      {
         if constexpr (Mode == num_mode::u64 || Mode == num_mode::i64) {
            data = static_cast<int64_t>(value);
         }
         else {
            data = static_cast<double>(value);
         }
         return *this;
      }

      generic_json& operator=(const std::string& value)
      {
         data = value;
         return *this;
      }

      generic_json& operator=(const std::string_view value)
      {
         data = std::string(value);
         return *this;
      }

      generic_json& operator=(const char* value)
      {
         data = std::string(value);
         return *this;
      }

      generic_json& operator=(const bool value)
      {
         data = value;
         return *this;
      }

      generic_json& operator=(const array_t& value)
      {
         data = value;
         return *this;
      }

      generic_json& operator=(const object_t& value)
      {
         data = value;
         return *this;
      }

      template <class T>
         requires(assignable_generic_type<generic_json<Mode, MapType>, T>)
      generic_json& operator=(T&& value);

      [[nodiscard]] generic_json& at(std::convertible_to<std::string_view> auto&& key) { return operator[](key); }

      [[nodiscard]] const generic_json& at(std::convertible_to<std::string_view> auto&& key) const
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

      generic_json() = default;
      generic_json(const generic_json& other) : data(other.data) {}
      generic_json& operator=(const generic_json&) = default;
      generic_json(generic_json&& other) noexcept : data(std::move(other.data)) {}
      generic_json& operator=(generic_json&&) = default;

      template <class T>
         requires std::convertible_to<T, val_t> && (!std::derived_from<std::decay_t<T>, generic_json>)
      generic_json(T&& val) : data(std::forward<T>(val))
      {}

      template <class T>
         requires std::convertible_to<T, double> && (!std::derived_from<std::decay_t<T>, generic_json>) &&
                  (!std::convertible_to<T, val_t>)
      generic_json(T&& val)
      {
         if constexpr (Mode == num_mode::u64) {
            if constexpr (std::unsigned_integral<T>) {
               data = static_cast<uint64_t>(val);
            }
            else if constexpr (std::signed_integral<T>) {
               data = static_cast<int64_t>(val);
            }
            else {
               data = static_cast<double>(val);
            }
         }
         else if constexpr (Mode == num_mode::i64) {
            if constexpr (std::integral<T>) {
               data = static_cast<int64_t>(val);
            }
            else {
               data = static_cast<double>(val);
            }
         }
         else {
            data = static_cast<double>(val);
         }
      }

      generic_json(const std::string_view value) : data(std::string(value)) {}

      generic_json(std::initializer_list<std::pair<const char*, generic_json>>&& obj)
      {
         data.template emplace<object_t>();
         auto& data_obj = std::get<object_t>(data);
         for (auto&& pair : obj) {
            // std::initializer_list holds const values that cannot be moved
            data_obj.emplace(pair.first, pair.second);
         }
      }

      // Prevent conflict with object initializer list
      template <bool deprioritize = true>
      generic_json(std::initializer_list<generic_json>&& arr)
      {
         data.template emplace<array_t>(std::move(arr));
      }

      [[nodiscard]] bool is_array() const noexcept { return holds<array_t>(); }

      [[nodiscard]] bool is_object() const noexcept { return holds<object_t>(); }

      [[nodiscard]] bool is_number() const noexcept
      {
         if constexpr (Mode == num_mode::u64) {
            return holds<uint64_t>() || holds<int64_t>() || holds<double>();
         }
         else if constexpr (Mode == num_mode::i64) {
            return holds<int64_t>() || holds<double>();
         }
         else {
            return holds<double>();
         }
      }

      [[nodiscard]] bool is_string() const noexcept { return holds<std::string>(); }

      [[nodiscard]] bool is_boolean() const noexcept { return holds<bool>(); }

      [[nodiscard]] bool is_null() const noexcept { return holds<std::nullptr_t>(); }

      [[nodiscard]] array_t& get_array() { return get<array_t>(); }
      [[nodiscard]] const array_t& get_array() const { return get<array_t>(); }

      [[nodiscard]] object_t& get_object() { return get<object_t>(); }
      [[nodiscard]] const object_t& get_object() const { return get<object_t>(); }

      [[nodiscard]] double& get_number()
      {
         if constexpr (Mode == num_mode::u64) {
            if (holds<uint64_t>()) {
               glaze_error(
                  "Cannot get reference to double when variant holds uint64_t. Use as<double>() for conversion.");
            }
            if (holds<int64_t>()) {
               glaze_error(
                  "Cannot get reference to double when variant holds int64_t. Use as<double>() for conversion.");
            }
         }
         else if constexpr (Mode == num_mode::i64) {
            if (holds<int64_t>()) {
               glaze_error(
                  "Cannot get reference to double when variant holds int64_t. Use as<double>() for conversion.");
            }
         }
         return get<double>();
      }
      [[nodiscard]] const double& get_number() const
      {
         if constexpr (Mode == num_mode::u64) {
            if (holds<uint64_t>()) {
               glaze_error(
                  "Cannot get reference to double when variant holds uint64_t. Use as<double>() for conversion.");
            }
            if (holds<int64_t>()) {
               glaze_error(
                  "Cannot get reference to double when variant holds int64_t. Use as<double>() for conversion.");
            }
         }
         else if constexpr (Mode == num_mode::i64) {
            if (holds<int64_t>()) {
               glaze_error(
                  "Cannot get reference to double when variant holds int64_t. Use as<double>() for conversion.");
            }
         }
         return get<double>();
      }

      // Get number as double, converting from integer types if needed (only available in i64/u64 modes)
      [[nodiscard]] double as_number() const
         requires(Mode != num_mode::f64)
      {
         if constexpr (Mode == num_mode::u64) {
            if (holds<uint64_t>()) {
               return static_cast<double>(get<uint64_t>());
            }
         }
         if constexpr (Mode == num_mode::u64 || Mode == num_mode::i64) {
            if (holds<int64_t>()) {
               return static_cast<double>(get<int64_t>());
            }
         }
         return get<double>();
      }

      // Check if the number is stored as uint64_t (only available in u64 mode)
      [[nodiscard]] bool is_uint64() const noexcept
         requires(Mode == num_mode::u64)
      {
         return holds<uint64_t>();
      }

      // Check if the number is stored as int64_t (only available in i64/u64 modes)
      [[nodiscard]] bool is_int64() const noexcept
         requires(Mode != num_mode::f64)
      {
         return holds<int64_t>();
      }

      // Check if the number is stored as double (only available in i64/u64 modes)
      [[nodiscard]] bool is_double() const noexcept
         requires(Mode != num_mode::f64)
      {
         return holds<double>();
      }

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

   // Type aliases for different number handling modes
   using generic = generic_json<num_mode::f64>; // double only - fast, JavaScript-compatible
   using generic_i64 = generic_json<num_mode::i64>; // int64_t → double - signed integer precision
   using generic_u64 = generic_json<num_mode::u64>; // uint64_t → int64_t → double - full integer range

   // Sorted-key map backend (lexicographic) for compatibility with legacy generic ordering.
   template <class T>
   using generic_sorted_map = std::map<std::string, T, std::less<>>;

   // Sorted-key generic aliases for legacy deterministic lexicographic object ordering.
   using generic_sorted = generic_json<num_mode::f64, generic_sorted_map>;
   using generic_sorted_i64 = generic_json<num_mode::i64, generic_sorted_map>;
   using generic_sorted_u64 = generic_json<num_mode::u64, generic_sorted_map>;

   // Backwards compatibility alias
   using json_t [[deprecated("glz::json_t is deprecated, use glz::generic instead")]] = generic;

   template <num_mode Mode, template <class> class MapType>
   [[nodiscard]] inline bool is_array(const generic_json<Mode, MapType>& value) noexcept
   {
      return value.is_array();
   }

   template <num_mode Mode, template <class> class MapType>
   [[nodiscard]] inline bool is_object(const generic_json<Mode, MapType>& value) noexcept
   {
      return value.is_object();
   }

   template <num_mode Mode, template <class> class MapType>
   [[nodiscard]] inline bool is_number(const generic_json<Mode, MapType>& value) noexcept
   {
      return value.is_number();
   }

   template <num_mode Mode, template <class> class MapType>
   [[nodiscard]] inline bool is_string(const generic_json<Mode, MapType>& value) noexcept
   {
      return value.is_string();
   }

   template <num_mode Mode, template <class> class MapType>
   [[nodiscard]] inline bool is_boolean(const generic_json<Mode, MapType>& value) noexcept
   {
      return value.is_boolean();
   }

   template <num_mode Mode, template <class> class MapType>
   [[nodiscard]] inline bool is_null(const generic_json<Mode, MapType>& value) noexcept
   {
      return value.is_null();
   }
}

template <glz::num_mode Mode, template <class> class MapType>
struct glz::meta<glz::generic_json<Mode, MapType>>
{
   static constexpr std::string_view name = "glz::generic";
   using T = glz::generic_json<Mode, MapType>;
   static constexpr auto value = &T::data;
};

#if defined(_MSC_VER) && !defined(__clang__)
// restore disabled warning
#pragma warning(pop)
#endif
