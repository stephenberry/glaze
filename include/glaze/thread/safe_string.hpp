// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <algorithm>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <utility>

#include "glaze/core/common.hpp"

namespace glz
{
   struct safe_string
   {
      std::string str;
      mutable std::shared_mutex mutex;

      safe_string() = default;
      safe_string(const char* s) : str(s) {}
      safe_string(const std::string& s) : str(s) {}
      safe_string(std::string&& s) : str(std::move(s)) {}
      safe_string(const std::string_view& sv) : str(sv) {}

      safe_string(const safe_string& other)
      {
         std::shared_lock lock(other.mutex);
         str = other.str;
      }

      safe_string(safe_string&& other) noexcept
      {
         std::unique_lock lock(other.mutex);
         str = std::move(other.str);
      }

      safe_string& operator=(const safe_string& other)
      {
         if (this != &other) {
            std::unique_lock lock1(mutex, std::defer_lock);
            std::shared_lock lock2(other.mutex, std::defer_lock);
            std::lock(lock1, lock2);
            str = other.str;
         }
         return *this;
      }

      safe_string& operator=(safe_string&& other) noexcept
      {
         if (this != &other) {
            std::unique_lock lock1(mutex, std::defer_lock);
            std::unique_lock lock2(other.mutex, std::defer_lock);
            std::lock(lock1, lock2);
            str = std::move(other.str);
         }
         return *this;
      }

      safe_string& operator=(const std::string& s)
      {
         std::unique_lock lock(mutex);
         str = s;
         return *this;
      }

      safe_string& operator=(std::string&& s)
      {
         std::unique_lock lock(mutex);
         str = std::move(s);
         return *this;
      }

      safe_string& operator=(const char* s)
      {
         std::unique_lock lock(mutex);
         str = s;
         return *this;
      }

      safe_string& operator=(const std::string_view& sv)
      {
         std::unique_lock lock(mutex);
         str = sv;
         return *this;
      }

      // Capacity
      size_t size() const noexcept
      {
         std::shared_lock lock(mutex);
         return str.size();
      }

      size_t length() const noexcept
      {
         std::shared_lock lock(mutex);
         return str.length();
      }

      bool empty() const noexcept
      {
         std::shared_lock lock(mutex);
         return str.empty();
      }

      // Modifiers
      void clear() noexcept
      {
         std::unique_lock lock(mutex);
         str.clear();
      }

      void push_back(char c)
      {
         std::unique_lock lock(mutex);
         str.push_back(c);
      }

      void pop_back()
      {
         std::unique_lock lock(mutex);
         str.pop_back();
      }

      safe_string& append(const std::string& s)
      {
         std::unique_lock lock(mutex);
         str.append(s);
         return *this;
      }

      safe_string& append(const char* s)
      {
         std::unique_lock lock(mutex);
         str.append(s);
         return *this;
      }

      safe_string& append(const std::string_view& sv)
      {
         std::unique_lock lock(mutex);
         str.append(sv);
         return *this;
      }

      safe_string& operator+=(const std::string& s) { return append(s); }

      safe_string& operator+=(const char* s) { return append(s); }

      safe_string& operator+=(const std::string_view& sv) { return append(sv); }

      safe_string& operator+=(char c)
      {
         std::unique_lock lock(mutex);
         str += c;
         return *this;
      }

      // Element access
      char at(size_t pos) const
      {
         std::shared_lock lock(mutex);
         return str.at(pos);
      }

      char operator[](size_t pos) const
      {
         std::shared_lock lock(mutex);
         return str[pos];
      }

      char front() const
      {
         std::shared_lock lock(mutex);
         return str.front();
      }

      char back() const
      {
         std::shared_lock lock(mutex);
         return str.back();
      }

      int compare(const safe_string& other) const
      {
         std::shared_lock lock1(mutex, std::defer_lock);
         std::shared_lock lock2(other.mutex, std::defer_lock);
         std::lock(lock1, lock2);
         return str.compare(other.str);
      }

      // Obtain a copy
      std::string string() const
      {
         std::shared_lock lock(mutex);
         return str;
      }

      friend bool operator==(const safe_string& lhs, const safe_string& rhs)
      {
         std::shared_lock lock1(lhs.mutex, std::defer_lock);
         std::shared_lock lock2(rhs.mutex, std::defer_lock);
         std::lock(lock1, lock2);
         return lhs.str == rhs.str;
      }

      friend bool operator!=(const safe_string& lhs, const safe_string& rhs) { return !(lhs == rhs); }

      friend bool operator<(const safe_string& lhs, const safe_string& rhs)
      {
         std::shared_lock lock1(lhs.mutex, std::defer_lock);
         std::shared_lock lock2(rhs.mutex, std::defer_lock);
         std::lock(lock1, lock2);
         return lhs.str < rhs.str;
      }

      friend bool operator<=(const safe_string& lhs, const safe_string& rhs) { return !(rhs < lhs); }

      friend bool operator>(const safe_string& lhs, const safe_string& rhs) { return rhs < lhs; }

      friend bool operator>=(const safe_string& lhs, const safe_string& rhs) { return !(lhs < rhs); }

      void swap(safe_string& other)
      {
         if (this == &other) return;
         std::unique_lock lock1(mutex, std::defer_lock);
         std::unique_lock lock2(other.mutex, std::defer_lock);
         std::lock(lock1, lock2);
         str.swap(other.str);
      }

      friend void swap(safe_string& lhs, safe_string& rhs) { lhs.swap(rhs); }
   };

}

namespace glz::detail
{
   template <uint32_t Format>
   struct from<Format, glz::safe_string>
   {
      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
      {
         std::shared_lock lock{value.mutex};
         read<Format>::template op<Opts>(value.str, ctx, it, end);
      }
   };

   template <uint32_t Format>
   struct to<Format, glz::safe_string>
   {
      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&&... args) noexcept
      {
         std::unique_lock lock{value.mutex};
         write<Format>::template op<Opts>(value.str, ctx, args...);
      }
   };
}
