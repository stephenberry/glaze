// Glaze Library
// For the license information refer to glaze.hpp
//
// PMR (Polymorphic Memory Resource) support for generic types
//
// glz::pmr::generic is an allocator-aware generic type that stores a polymorphic_allocator
// and propagates it to all internal containers. This allows the entire tree
// to use a single memory resource (e.g., a stack-allocated buffer).

#pragma once

#include <map>
#include <memory_resource>
#include <variant>
#include <vector>

// std::pmr::string is not available with the old GCC ABI
#if !defined(_GLIBCXX_USE_CXX11_ABI) || _GLIBCXX_USE_CXX11_ABI != 0

#include "glaze/json/generic.hpp"

namespace glz::pmr
{
   // ============================================================================
   // glz::pmr::generic - Allocator-aware PMR generic type
   // ============================================================================
   // This type stores a polymorphic_allocator and propagates it to all internal
   // containers, allowing the entire tree to use a single memory resource.
   //
   // Example usage:
   //   std::array<std::byte, 16384> buffer{};
   //   std::pmr::monotonic_buffer_resource mbr(buffer.data(), buffer.size());
   //
   //   glz::pmr::generic json(&mbr);
   //   json["name"] = "Alice";        // string allocated from buffer
   //   json["scores"].push_back(...); // array allocated from buffer
   //   json["nested"]["key"] = 42;    // nested objects use buffer

   template <num_mode Mode = num_mode::f64>
   struct generic
   {
      using self_type = generic<Mode>;
      using allocator_type = std::pmr::polymorphic_allocator<std::byte>;

      using string_t = std::pmr::string;
      using array_t = std::pmr::vector<self_type>;
      using object_t = std::pmr::map<string_t, self_type, std::less<>>;
      using null_t = std::nullptr_t;

   private:
      // Variant type depends on Mode
      template <num_mode M, class null, class str, class arr, class obj>
      struct val_type_helper;

      template <class null, class str, class arr, class obj>
      struct val_type_helper<num_mode::f64, null, str, arr, obj>
      {
         using type = std::variant<null, double, str, bool, arr, obj>;
      };

      template <class null, class str, class arr, class obj>
      struct val_type_helper<num_mode::i64, null, str, arr, obj>
      {
         using type = std::variant<null, int64_t, double, str, bool, arr, obj>;
      };

      template <class null, class str, class arr, class obj>
      struct val_type_helper<num_mode::u64, null, str, arr, obj>
      {
         using type = std::variant<null, uint64_t, int64_t, double, str, bool, arr, obj>;
      };

   public:
      using val_t = typename val_type_helper<Mode, null_t, string_t, array_t, object_t>::type;

   private:
      val_t data_{};
      allocator_type alloc_{};

   public:
      // ============================================================================
      // Constructors
      // ============================================================================

      generic() : generic(std::pmr::get_default_resource()) {}

      explicit generic(std::pmr::memory_resource* resource) : data_(null_t{}), alloc_(resource) {}

      explicit generic(const allocator_type& alloc) : data_(null_t{}), alloc_(alloc) {}

      // Copy constructor - uses the same allocator
      generic(const generic& other) : data_(copy_with_allocator(other.data_, other.alloc_)), alloc_(other.alloc_) {}

      // Copy constructor with allocator
      generic(const generic& other, const allocator_type& alloc)
         : data_(copy_with_allocator(other.data_, alloc)), alloc_(alloc)
      {}

      // Move constructor
      generic(generic&& other) noexcept : data_(std::move(other.data_)), alloc_(other.alloc_) {}

      // Move constructor with allocator
      // Allocators must be equal; use copy constructor for cross-allocator transfers
      generic(generic&& other, const allocator_type& alloc) : alloc_(alloc)
      {
         assert(alloc_ == other.alloc_ && "Cannot move across different allocators; use copy instead");
         data_ = std::move(other.data_);
      }

      // Value constructors with allocator
      generic(double val, std::pmr::memory_resource* resource = std::pmr::get_default_resource())
         : data_(val), alloc_(resource)
      {}

      generic(bool val, std::pmr::memory_resource* resource = std::pmr::get_default_resource())
         : data_(val), alloc_(resource)
      {}

      generic(std::nullptr_t, std::pmr::memory_resource* resource = std::pmr::get_default_resource())
         : data_(null_t{}), alloc_(resource)
      {}

      generic(int val, std::pmr::memory_resource* resource = std::pmr::get_default_resource()) : alloc_(resource)
      {
         if constexpr (Mode == num_mode::f64) {
            data_ = static_cast<double>(val);
         }
         else {
            data_ = static_cast<int64_t>(val);
         }
      }

      generic(long val, std::pmr::memory_resource* resource = std::pmr::get_default_resource()) : alloc_(resource)
      {
         if constexpr (Mode == num_mode::f64) {
            data_ = static_cast<double>(val);
         }
         else {
            data_ = static_cast<int64_t>(val);
         }
      }

      generic(long long val, std::pmr::memory_resource* resource = std::pmr::get_default_resource()) : alloc_(resource)
      {
         if constexpr (Mode == num_mode::f64) {
            data_ = static_cast<double>(val);
         }
         else {
            data_ = static_cast<int64_t>(val);
         }
      }

      generic(unsigned int val, std::pmr::memory_resource* resource = std::pmr::get_default_resource())
         : alloc_(resource)
      {
         if constexpr (Mode == num_mode::u64) {
            data_ = static_cast<uint64_t>(val);
         }
         else if constexpr (Mode == num_mode::i64) {
            data_ = static_cast<int64_t>(val);
         }
         else {
            data_ = static_cast<double>(val);
         }
      }

      generic(unsigned long val, std::pmr::memory_resource* resource = std::pmr::get_default_resource())
         : alloc_(resource)
      {
         if constexpr (Mode == num_mode::u64) {
            data_ = static_cast<uint64_t>(val);
         }
         else if constexpr (Mode == num_mode::i64) {
            data_ = static_cast<int64_t>(val);
         }
         else {
            data_ = static_cast<double>(val);
         }
      }

      generic(unsigned long long val, std::pmr::memory_resource* resource = std::pmr::get_default_resource())
         : alloc_(resource)
      {
         if constexpr (Mode == num_mode::u64) {
            data_ = static_cast<uint64_t>(val);
         }
         else if constexpr (Mode == num_mode::i64) {
            data_ = static_cast<int64_t>(val);
         }
         else {
            data_ = static_cast<double>(val);
         }
      }

      generic(std::string_view val, std::pmr::memory_resource* resource = std::pmr::get_default_resource())
         : data_(string_t(val, resource)), alloc_(resource)
      {}

      generic(const char* val, std::pmr::memory_resource* resource = std::pmr::get_default_resource())
         : data_(string_t(val, resource)), alloc_(resource)
      {}

      // Assignment operators
      generic& operator=(const generic& other)
      {
         if (this != &other) {
            data_ = copy_with_allocator(other.data_, alloc_);
         }
         return *this;
      }

      // Allocators must be equal; use copy assignment for cross-allocator transfers
      generic& operator=(generic&& other) noexcept
      {
         assert(alloc_ == other.alloc_ && "Cannot move across different allocators; use copy instead");
         if (this != &other) {
            data_ = std::move(other.data_);
         }
         return *this;
      }

      generic& operator=(double val)
      {
         data_ = val;
         return *this;
      }

      generic& operator=(int val)
      {
         if constexpr (Mode == num_mode::f64) {
            data_ = static_cast<double>(val);
         }
         else {
            data_ = static_cast<int64_t>(val);
         }
         return *this;
      }

      generic& operator=(long val)
      {
         if constexpr (Mode == num_mode::f64) {
            data_ = static_cast<double>(val);
         }
         else {
            data_ = static_cast<int64_t>(val);
         }
         return *this;
      }

      generic& operator=(long long val)
      {
         if constexpr (Mode == num_mode::f64) {
            data_ = static_cast<double>(val);
         }
         else {
            data_ = static_cast<int64_t>(val);
         }
         return *this;
      }

      generic& operator=(unsigned int val)
      {
         if constexpr (Mode == num_mode::u64) {
            data_ = static_cast<uint64_t>(val);
         }
         else if constexpr (Mode == num_mode::i64) {
            data_ = static_cast<int64_t>(val);
         }
         else {
            data_ = static_cast<double>(val);
         }
         return *this;
      }

      generic& operator=(unsigned long val)
      {
         if constexpr (Mode == num_mode::u64) {
            data_ = static_cast<uint64_t>(val);
         }
         else if constexpr (Mode == num_mode::i64) {
            data_ = static_cast<int64_t>(val);
         }
         else {
            data_ = static_cast<double>(val);
         }
         return *this;
      }

      generic& operator=(unsigned long long val)
      {
         if constexpr (Mode == num_mode::u64) {
            data_ = static_cast<uint64_t>(val);
         }
         else if constexpr (Mode == num_mode::i64) {
            data_ = static_cast<int64_t>(val);
         }
         else {
            data_ = static_cast<double>(val);
         }
         return *this;
      }

      generic& operator=(bool val)
      {
         data_ = val;
         return *this;
      }

      generic& operator=(std::nullptr_t)
      {
         data_ = null_t{};
         return *this;
      }

      generic& operator=(std::string_view val)
      {
         data_ = string_t(val, alloc_);
         return *this;
      }

      generic& operator=(const char* val)
      {
         data_ = string_t(val, alloc_);
         return *this;
      }

      // ============================================================================
      // Accessors
      // ============================================================================

      [[nodiscard]] allocator_type get_allocator() const noexcept { return alloc_; }
      [[nodiscard]] std::pmr::memory_resource* resource() const noexcept { return alloc_.resource(); }

      [[nodiscard]] val_t& data() noexcept { return data_; }
      [[nodiscard]] const val_t& data() const noexcept { return data_; }

      template <class T>
      [[nodiscard]] T& get()
      {
         return std::get<T>(data_);
      }

      template <class T>
      [[nodiscard]] const T& get() const
      {
         return std::get<T>(data_);
      }

      template <class T>
      [[nodiscard]] T* get_if() noexcept
      {
         return std::get_if<T>(&data_);
      }

      template <class T>
      [[nodiscard]] const T* get_if() const noexcept
      {
         return std::get_if<T>(&data_);
      }

      template <class T>
      [[nodiscard]] bool holds() const noexcept
      {
         return std::holds_alternative<T>(data_);
      }

      // ============================================================================
      // Type checking
      // ============================================================================

      [[nodiscard]] bool is_null() const noexcept { return holds<null_t>(); }
      [[nodiscard]] bool is_boolean() const noexcept { return holds<bool>(); }
      [[nodiscard]] bool is_string() const noexcept { return holds<string_t>(); }
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

      // ============================================================================
      // Typed getters
      // ============================================================================

      [[nodiscard]] string_t& get_string() { return get<string_t>(); }
      [[nodiscard]] const string_t& get_string() const { return get<string_t>(); }

      [[nodiscard]] array_t& get_array() { return get<array_t>(); }
      [[nodiscard]] const array_t& get_array() const { return get<array_t>(); }

      [[nodiscard]] object_t& get_object() { return get<object_t>(); }
      [[nodiscard]] const object_t& get_object() const { return get<object_t>(); }

      // ============================================================================
      // Subscript operators - these use the stored allocator
      // ============================================================================

      generic& operator[](std::integral auto&& index) { return std::get<array_t>(data_)[index]; }

      const generic& operator[](std::integral auto&& index) const { return std::get<array_t>(data_)[index]; }

      generic& operator[](std::string_view key)
      {
         if (holds<null_t>()) {
            // Create object with our allocator
            data_.template emplace<object_t>(alloc_);
         }
         auto& obj = std::get<object_t>(data_);
         auto it = obj.find(key);
         if (it == obj.end()) {
            // Insert new element with our allocator
            it = obj.emplace(string_t(key, alloc_), generic(alloc_)).first;
         }
         return it->second;
      }

      const generic& operator[](std::string_view key) const
      {
         auto& obj = std::get<object_t>(data_);
         auto it = obj.find(key);
         if (it == obj.end()) {
            glaze_error("Key not found.");
         }
         return it->second;
      }

      // ============================================================================
      // Array operations - use stored allocator
      // ============================================================================

      void push_back(const generic& val)
      {
         if (holds<null_t>()) {
            data_.template emplace<array_t>(alloc_);
         }
         get_array().push_back(generic(val, alloc_));
      }

      void push_back(generic&& val)
      {
         if (holds<null_t>()) {
            data_.template emplace<array_t>(alloc_);
         }
         get_array().push_back(std::move(val));
      }

      template <class... Args>
      generic& emplace_back(Args&&... args)
      {
         if (holds<null_t>()) {
            data_.template emplace<array_t>(alloc_);
         }
         return get_array().emplace_back(generic(std::forward<Args>(args)..., alloc_.resource()));
      }

      // ============================================================================
      // Utility methods
      // ============================================================================

      [[nodiscard]] bool contains(std::string_view key) const
      {
         if (!holds<object_t>()) return false;
         auto& obj = std::get<object_t>(data_);
         return obj.find(key) != obj.end();
      }

      [[nodiscard]] bool empty() const noexcept
      {
         if (auto* v = get_if<object_t>()) return v->empty();
         if (auto* v = get_if<array_t>()) return v->empty();
         if (auto* v = get_if<string_t>()) return v->empty();
         if (is_null()) return true;
         return false;
      }

      [[nodiscard]] size_t size() const noexcept
      {
         if (auto* v = get_if<object_t>()) return v->size();
         if (auto* v = get_if<array_t>()) return v->size();
         if (auto* v = get_if<string_t>()) return v->size();
         return 0;
      }

      void clear()
      {
         if (auto* v = get_if<object_t>()) v->clear();
         else if (auto* v = get_if<array_t>()) v->clear();
         else if (auto* v = get_if<string_t>()) v->clear();
      }

      void reset() { data_ = null_t{}; }

   private:
      // Helper to deep copy a generic with a specific allocator
      static self_type deep_copy(const self_type& src, const allocator_type& alloc);

      // Helper to copy data with a specific allocator
      static val_t copy_with_allocator(const val_t& src, const allocator_type& alloc)
      {
         return std::visit(
            [&alloc](const auto& v) -> val_t {
               using T = std::decay_t<decltype(v)>;
               if constexpr (std::same_as<T, null_t>) {
                  return null_t{};
               }
               else if constexpr (std::same_as<T, bool> || std::same_as<T, double> || std::same_as<T, int64_t> ||
                                  std::same_as<T, uint64_t>) {
                  return v;
               }
               else if constexpr (std::same_as<T, string_t>) {
                  return string_t(v, alloc);
               }
               else if constexpr (std::same_as<T, array_t>) {
                  array_t result(alloc);
                  result.reserve(v.size());
                  for (const auto& elem : v) {
                     result.push_back(deep_copy(elem, alloc));
                  }
                  return result;
               }
               else if constexpr (std::same_as<T, object_t>) {
                  object_t result(alloc);
                  for (const auto& [key, val] : v) {
                     result.emplace(string_t(key, alloc), deep_copy(val, alloc));
                  }
                  return result;
               }
               else {
                  return null_t{};
               }
            },
            src);
      }

   };

   // Definition of deep_copy (must be after class definition due to self-reference)
   template <num_mode Mode>
   typename generic<Mode>::self_type generic<Mode>::deep_copy(const self_type& src, const allocator_type& alloc)
   {
      self_type result(alloc);
      result.data_ = copy_with_allocator(src.data_, alloc);
      return result;
   }

   // Type aliases for common configurations
   using generic_f64 = generic<num_mode::f64>;
   using generic_i64 = generic<num_mode::i64>;
   using generic_u64 = generic<num_mode::u64>;

} // namespace glz::pmr

template <glz::num_mode Mode>
struct glz::meta<glz::pmr::generic<Mode>>
{
   static constexpr std::string_view name = "glz::pmr::generic";
   using T = glz::pmr::generic<Mode>;
   static constexpr auto value = &T::data;
};

#endif
