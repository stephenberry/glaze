// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "glaze/util/key_transformers.hpp"

namespace glz
{
   class http_headers
   {
     public:
      struct field_type
      {
         std::string name;
         std::string value;
      };

      using value_type = field_type;
      using iterator = std::vector<value_type>::iterator;
      using const_iterator = std::vector<value_type>::const_iterator;
      using size_type = std::vector<value_type>::size_type;

     private:
      template <bool IsConst>
      class matching_iterator
      {
         using owner_type = std::conditional_t<IsConst, const http_headers*, http_headers*>;

        public:
         using iterator_concept = std::forward_iterator_tag;
         using iterator_category = std::forward_iterator_tag;
         using difference_type = std::ptrdiff_t;
         using value_type = http_headers::value_type;
         using reference = std::conditional_t<IsConst, const value_type&, value_type&>;
         using pointer = std::conditional_t<IsConst, const value_type*, value_type*>;

         matching_iterator() = default;
         matching_iterator(const matching_iterator&) = default;
         matching_iterator(matching_iterator&&) = default;
         matching_iterator& operator=(const matching_iterator&) = default;
         matching_iterator& operator=(matching_iterator&&) = default;
         ~matching_iterator() = default;

         matching_iterator(owner_type owner, size_t start_index, std::string key)
            : owner(owner), index(start_index), key(std::move(key))
         {
            seek_forward();
         }

         [[nodiscard]] reference operator*() const noexcept { return owner->items[index]; }
         [[nodiscard]] pointer operator->() const noexcept { return std::addressof(owner->items[index]); }

         matching_iterator& operator++() noexcept
         {
            if (index < owner->items.size()) {
               ++index;
               seek_forward();
            }
            return *this;
         }

         matching_iterator operator++(int) noexcept
         {
            auto copy = *this;
            ++(*this);
            return copy;
         }

         [[nodiscard]] friend bool operator==(const matching_iterator& left, const matching_iterator& right) noexcept
         {
            return left.owner == right.owner && left.index == right.index && ascii_str_iequal(left.key, right.key);
         }

        private:
         void seek_forward() noexcept
         {
            while (index < owner->items.size()) {
               const auto& current = owner->items[index].name;
               if (ascii_str_iequal(current, key)) {
                  return;
               }
               ++index;
            }
         }

         owner_type owner{};
         size_t index{0};
         std::string key;
      };

     public:
      using range_iterator = matching_iterator<false>;
      using const_range_iterator = matching_iterator<true>;

      http_headers() = default;
      http_headers(std::initializer_list<value_type> fields) : items(fields) {}

      [[nodiscard]] bool empty() const& noexcept { return items.empty(); }
      [[nodiscard]] size_type size() const& noexcept { return items.size(); }
      [[nodiscard]] iterator begin() & noexcept { return items.begin(); }
      [[nodiscard]] iterator end() & noexcept { return items.end(); }
      [[nodiscard]] const_iterator begin() const& noexcept { return items.begin(); }
      [[nodiscard]] const_iterator end() const& noexcept { return items.end(); }
      [[nodiscard]] const_iterator cbegin() const& noexcept { return items.cbegin(); }
      [[nodiscard]] const_iterator cend() const& noexcept { return items.cend(); }

      /// @brief Clears all of the fields
      void clear() noexcept { items.clear(); }

      /// @brief Checks if fields contain any field with specified name
      /// @param name Field name
      /// @return `true` if field with specified name exists, `false` otherwise
      [[nodiscard]] bool contains(std::string_view name) const noexcept { return find(name) != end(); }

      /// @brief Find first iterator whose name matches the specified name
      /// @param name Field name
      /// @return iterator
      [[nodiscard]] iterator find(std::string_view name) & noexcept
      {
         return std::ranges::find_if(
            items, [&](const value_type& field) noexcept { return ascii_str_iequal(field.name, name); });
      }

      /// @brief Find first iterator whose name matches the specified name
      /// @param name Field name
      /// @return const_iterator
      [[nodiscard]] const_iterator find(std::string_view name) const& noexcept
      {
         return std::ranges::find_if(
            items, [&](const value_type& field) noexcept { return ascii_str_iequal(field.name, name); });
      }

      /// @brief Get all field lines whose name matches the specified name
      /// @param name Field name
      /// @return Mutable range of http_headers::field_type
      [[nodiscard]] auto fields(std::string_view name) &
      {
         // REVIEW: Should we care more about allocations and prefer views,
         // even if that potentially complicates lifetime handling for caller?
         auto key_copy = std::string{name};
         auto first = range_iterator{this, 0, key_copy};
         auto last = range_iterator{this, items.size(), std::move(key_copy)};
         return std::ranges::subrange{first, last};
      }

      /// @brief Get all field lines whose name matches the specified name
      /// @param name Field name
      /// @return Immutable range of http_headers::field_type
      [[nodiscard]] auto fields(std::string_view name) const&
      {
         auto key_copy = std::string{name};
         auto first = const_range_iterator{this, 0, key_copy};
         auto last = const_range_iterator{this, items.size(), std::move(key_copy)};
         return std::ranges::subrange{first, last};
      }

      /// @brief Get all field names
      /// @return Range of string_view
      [[nodiscard]] auto names() const&
      {
         return items |
                std::views::transform([](const field_type& field) noexcept -> std::string_view { return field.name; });
      }

      /// @brief Get all field values
      /// @return Range of string_view
      [[nodiscard]] auto values() const&
      {
         return items |
                std::views::transform([](const field_type& field) noexcept -> std::string_view { return field.value; });
      }

      /// @brief Get all field values whose name matches the specified name
      /// @param name Field name
      /// @return Range of string_view
      [[nodiscard]] auto values(std::string_view name) const&
      {
         return fields(name) |
                std::views::transform([](const field_type& field) noexcept -> std::string_view { return field.value; });
      }

      /// @brief Get the number of entries whose name matches the specified name
      /// @param name Field name
      /// @return Number of entries as size_t
      [[nodiscard]] size_t occurrences(std::string_view name) const& noexcept
      {
         return std::ranges::count_if(
            items, [name](const field_type& field) noexcept { return ascii_str_iequal(field.name, name); });
      }

      /// @brief Try to get first stored value of entry whose name matches the specified name
      /// @param name Field name
      /// @returns `string_view` if header with such name exists, otherwise returns `nullopt`
      [[nodiscard]] std::optional<std::string_view> first_value(std::string_view name) const& noexcept
      {
         auto iterator = find(name);
         if (iterator == end()) {
            return std::nullopt;
         }
         return std::string_view{iterator->value};
      }

      /// @brief Serialize stored header fields as an HTTP header section
      /// @returns CRLF if there is no headers to serialize, otherwise returns valid headers string
      [[nodiscard]] std::string serialize() const
      {
         if (empty()) {
            return "\r\n";
         }

         // REVIEW: Two loops - one to accumulate the total length and one to append
         // should be faster than a single append loop with potential reallocations
         size_t expected_length = 0;
         for (const auto& [name, value] : items) {
            constexpr size_t separators_length = 4; // ": " + CRLF
            expected_length += name.length() + value.length() + separators_length;
         }

         std::string result;
         result.reserve(expected_length + 2); // + CRLF length

         for (const auto& [name, value] : items) {
            // Skip empty field names. Empty field values are still valid under RFC9112
            if (name.empty()) {
               continue;
            }
            result.append(name).append(": ").append(value).append("\r\n");
         }

         result.append("\r\n");

         return result;
      }

      /// @brief Replace all headers whose name matches the specified name with given value
      /// @param name Field name
      /// @param value Field value
      /// @note Does not preserve the previous field order
      void replace(std::string name, std::string value)
      {
         // REVIEW: Maybe we should implement full field-name validation
         // (in the future at least), since user probably expects class
         // named 'http_headers' to store RFC-compliant header section.

         erase(name);
         add(std::move(name), std::move(value));
      }

      /// @brief Add a new field to the existing fields with the same name
      /// @param name Field name
      /// @param value Field value
      /// @returns Last iterator before operation
      iterator add(std::string name, std::string value) &
      {
         // REVIEW: See replace() function comment

         items.emplace_back(std::move(name), std::move(value));
         return std::prev(items.end());
      }

      /// @brief Copies all fields from another set of http_headers
      /// @param other http_headers to be copied from
      void append(const http_headers& other)
      {
         if (std::addressof(other) == this || other.empty()) {
            return;
         }
         items.reserve(items.size() + other.items.size());
         std::ranges::copy(other.items, std::back_inserter(items));
      }

      /// @brief Moves all fields from another set of http_headers
      /// @param other http_headers to be moved from
      void append(http_headers&& other)
      {
         if (std::addressof(other) == this || other.empty()) {
            return;
         }
         items.reserve(items.size() + other.items.size());
         std::ranges::move(other.items, std::back_inserter(items));
         other.clear();
      }

      /// @brief Erases all fields whose name matches the specified name
      /// @param name Field name
      void erase(std::string_view name)
      {
         auto to_remove = std::ranges::remove_if(
            items, [&](const value_type& field) noexcept { return ascii_str_iequal(field.name, name); });
         items.erase(to_remove.begin(), to_remove.end());
      }

      // Disallow use on temporary objects
      iterator begin() && = delete;
      iterator end() && = delete;
      const_iterator begin() const&& = delete;
      const_iterator end() const&& = delete;
      const_iterator cbegin() const&& = delete;
      const_iterator cend() const&& = delete;
      iterator find(std::string_view name) && = delete;
      const_iterator find(std::string_view name) const&& = delete;
      auto fields(std::string_view name) && = delete;
      auto fields(std::string_view name) const&& = delete;
      auto names() const&& = delete;
      auto values() const&& = delete;
      auto values(std::string_view name) const&& = delete;
      std::optional<std::string_view> first_value(std::string_view name) const&& = delete;
      iterator add(std::string name, std::string value) && = delete;

     private:
      // REVIEW: According to RFC 9112, header names can consist only of ASCII
      // characters, so I don't think this will cause any problems
      [[nodiscard]] static bool ascii_str_iequal(std::string_view s1, std::string_view s2) noexcept
      {
         if (s1.size() != s2.size()) {
            return false;
         }

         for (size_t i = 0; i < s1.size(); ++i) {
            if (glz::ascii_tolower(s1[i]) != glz::ascii_tolower(s2[i])) {
               return false;
            }
         }

         return true;
      }

      std::vector<value_type> items;
   };

   static_assert(std::ranges::viewable_range<http_headers>);
}
