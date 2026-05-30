// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <expected>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

#include "glaze/util/key_transformers.hpp"

namespace glz
{
   /// @brief Ordered collection of HTTP header field lines.
   ///
   /// Header names are matched ASCII-case-insensitively, while their original
   /// spelling, insertion order, and duplicate field lines are preserved.
   ///
   /// This container does not validate fields on insertion. Parsers are
   /// expected to reject invalid wire input, and serialize() validates before
   /// emitting an HTTP header section.
   class http_headers
   {
     public:
      /// @brief Single HTTP header field line.
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
            return left.owner == right.owner && left.index == right.index && keys_equal(left.key, right.key);
         }

        private:
         [[nodiscard]] static bool keys_equal(std::string_view s1, std::string_view s2) noexcept
         {
            return http_headers::ascii_str_iequal(s1, s2);
         }

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

      /// @brief Clears all stored field lines
      void clear() noexcept { items.clear(); }

      /// @brief Checks if any field line has the specified name
      /// @param name Field name
      /// @return `true` if field with specified name exists, `false` otherwise
      [[nodiscard]] bool contains(std::string_view name) const noexcept { return find(name) != end(); }

      /// @brief Finds the first field line whose name matches the specified name
      /// @param name Field name
      /// @return Iterator to the first matching field line, or end() if missing
      [[nodiscard]] iterator find(std::string_view name) & noexcept
      {
         return std::ranges::find_if(
            items, [&](const value_type& field) noexcept { return ascii_str_iequal(field.name, name); });
      }

      /// @brief Finds the first field line whose name matches the specified name
      /// @param name Field name
      /// @return Const iterator to the first matching field line, or end() if missing
      [[nodiscard]] const_iterator find(std::string_view name) const& noexcept
      {
         return std::ranges::find_if(
            items, [&](const value_type& field) noexcept { return ascii_str_iequal(field.name, name); });
      }

      /// @brief Gets all field lines whose name matches the specified name
      /// @param name Field name
      /// @return Mutable range of matching field lines
      [[nodiscard]] auto fields(std::string_view name) &
      {
         auto key_copy = std::string{name};
         auto first = range_iterator{this, 0, key_copy};
         auto last = range_iterator{this, items.size(), std::move(key_copy)};
         return std::ranges::subrange{first, last};
      }

      /// @brief Gets all field lines whose name matches the specified name
      /// @param name Field name
      /// @return Immutable range of matching field lines
      [[nodiscard]] auto fields(std::string_view name) const&
      {
         auto key_copy = std::string{name};
         auto first = const_range_iterator{this, 0, key_copy};
         auto last = const_range_iterator{this, items.size(), std::move(key_copy)};
         return std::ranges::subrange{first, last};
      }

      /// @brief Gets all stored field names in insertion order
      /// @return Range of string_view into the stored names
      [[nodiscard]] auto names() const&
      {
         return items |
                std::views::transform([](const field_type& field) noexcept -> std::string_view { return field.name; });
      }

      /// @brief Gets all stored field values in insertion order
      /// @return Range of string_view into the stored values
      [[nodiscard]] auto values() const&
      {
         return items |
                std::views::transform([](const field_type& field) noexcept -> std::string_view { return field.value; });
      }

      /// @brief Gets values of all field lines whose name matches the specified name
      /// @param name Field name
      /// @return Range of string_view into the matching stored values
      [[nodiscard]] auto values(std::string_view name) const&
      {
         return fields(name) |
                std::views::transform([](const field_type& field) noexcept -> std::string_view { return field.value; });
      }

      /// @brief Counts field lines whose name matches the specified name
      /// @param name Field name
      /// @return Number of entries as size_t
      [[nodiscard]] size_t occurrences(std::string_view name) const& noexcept
      {
         return std::ranges::count_if(
            items, [name](const field_type& field) noexcept { return ascii_str_iequal(field.name, name); });
      }

      /// @brief Gets the first stored value whose field name matches the specified name
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

      /// @brief Serializes stored field lines as an HTTP header section.
      ///
      /// Empty containers serialize to the terminal CRLF. Non-empty containers
      /// must contain only valid HTTP field names and field values; invalid
      /// stored fields fail serialization instead of being skipped or escaped.
      ///
      /// @returns Valid HTTP header section, or protocol_error if a stored field is invalid
      [[nodiscard]] std::expected<std::string, std::error_code> serialize() const
      {
         if (empty()) {
            return "\r\n";
         }

         // Two loops to accumulate the total length and to append headers should
         // be faster than a single append loop with potential reallocations
         size_t expected_length = 0;
         for (const auto& [name, value] : items) {
            if (!is_field_name_valid(name) || !is_field_value_valid(value)) {
               return std::unexpected(std::make_error_code(std::errc::protocol_error));
            }

            constexpr size_t separators_length = 4; // ": " + CRLF
            expected_length += name.length() + value.length() + separators_length;
         }

         std::string result;
         result.reserve(expected_length + 2); // + CRLF

         for (const auto& [name, value] : items) {
            result.append(name).append(": ").append(value).append("\r\n");
         }

         result.append("\r\n");

         return result;
      }

      /// @brief Replaces all matching field lines with one new field line
      /// @param name Field name
      /// @param value Field value
      /// @note Does not validate the new field line or preserve the previous field order
      void replace(std::string name, std::string value)
      {
         erase(name);
         add(std::move(name), std::move(value));
      }

      /// @brief Appends a new field line
      /// @param name Field name
      /// @param value Field value
      /// @returns Iterator to the appended field line
      /// @note Does not validate the new field line
      iterator add(std::string name, std::string value) &
      {
         items.emplace_back(std::move(name), std::move(value));
         return std::prev(items.end());
      }

      /// @brief Appends copies of all field lines from another http_headers object
      /// @param other http_headers to be copied from
      void append(const http_headers& other)
      {
         if (std::addressof(other) == this || other.empty()) {
            return;
         }
         items.reserve(items.size() + other.items.size());
         std::ranges::copy(other.items, std::back_inserter(items));
      }

      /// @brief Appends all field lines moved from another http_headers object
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

      /// @brief Erases all field lines whose name matches the specified name
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

      [[nodiscard]] static bool is_field_name_valid(std::string_view name) noexcept
      {
         if (name.empty()) {
            return false;
         }

         static constexpr std::array valid_name_tokens{'!', '#', '$', '%', '^', '&', '\'', '*',
                                                       '+', '-', '.', '_', '`', '|', '~'};

         return std::ranges::all_of(name, [](char c) noexcept {
            const auto byte = static_cast<unsigned char>(c);
            return (byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z') || (byte >= '0' && byte <= '9') ||
                   std::ranges::contains(valid_name_tokens, byte);
         });
      }

      [[nodiscard]] static bool is_field_value_valid(std::string_view value) noexcept
      {
         return std::ranges::all_of(value, [](char c) noexcept {
            const auto byte = static_cast<unsigned char>(c);
            return byte == '\t' || (byte >= 0x20 && byte <= 0x7e) || byte >= 0x80;
         });
      }

      std::vector<value_type> items;
   };

   static_assert(std::ranges::viewable_range<http_headers>);
}
