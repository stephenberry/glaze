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

// Provides a thread safe wrapper around a std::string, which Glaze knows how to serialize/deserialize safely

namespace glz
{
   struct async_string
   {
      std::string str;
      mutable std::shared_mutex mutex;

      async_string() = default;
      async_string(const char* s) : str(s) {}
      async_string(const std::string& s) : str(s) {}
      async_string(std::string&& s) : str(std::move(s)) {}
      async_string(const std::string_view& sv) : str(sv) {}

      async_string(const async_string& other)
      {
         std::shared_lock lock(other.mutex);
         str = other.str;
      }

      async_string(async_string&& other) noexcept
      {
         std::unique_lock lock(other.mutex);
         str = std::move(other.str);
      }

      async_string& operator=(const async_string& other)
      {
         if (this != &other) {
            std::unique_lock lock1(mutex, std::defer_lock);
            std::shared_lock lock2(other.mutex, std::defer_lock);
            std::lock(lock1, lock2);
            str = other.str;
         }
         return *this;
      }

      async_string& operator=(async_string&& other) noexcept
      {
         if (this != &other) {
            std::unique_lock lock1(mutex, std::defer_lock);
            std::unique_lock lock2(other.mutex, std::defer_lock);
            std::lock(lock1, lock2);
            str = std::move(other.str);
         }
         return *this;
      }

      async_string& operator=(const std::string& s)
      {
         std::unique_lock lock(mutex);
         str = s;
         return *this;
      }

      async_string& operator=(std::string&& s)
      {
         std::unique_lock lock(mutex);
         str = std::move(s);
         return *this;
      }

      async_string& operator=(const char* s)
      {
         std::unique_lock lock(mutex);
         str = s;
         return *this;
      }

      async_string& operator=(const std::string_view& sv)
      {
         std::unique_lock lock(mutex);
         str = sv;
         return *this;
      }

      struct proxy
      {
         std::string* ptr{};
         std::unique_lock<std::shared_mutex> lock{};

        public:
         proxy(std::string& p, std::unique_lock<std::shared_mutex>&& lock) noexcept : ptr{&p}, lock(std::move(lock)) {}

         std::string* operator->() noexcept { return ptr; }

         std::string& operator*() noexcept { return *ptr; }
      };

      proxy write() { return {str, std::unique_lock{mutex}}; }

      struct const_proxy
      {
         const std::string* ptr{};
         std::shared_lock<std::shared_mutex> lock{};

        public:
         const_proxy(const std::string& p, std::shared_lock<std::shared_mutex>&& lock) noexcept
            : ptr{&p}, lock(std::move(lock))
         {}

         const std::string* operator->() const noexcept { return ptr; }

         const std::string& operator*() const noexcept { return *ptr; }
      };

      const_proxy read() const { return {str, std::shared_lock{mutex}}; }

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

      async_string& append(const std::string& s)
      {
         std::unique_lock lock(mutex);
         str.append(s);
         return *this;
      }

      async_string& append(const char* s)
      {
         std::unique_lock lock(mutex);
         str.append(s);
         return *this;
      }

      async_string& append(const std::string_view& sv)
      {
         std::unique_lock lock(mutex);
         str.append(sv);
         return *this;
      }

      async_string& operator+=(const std::string& s) { return append(s); }

      async_string& operator+=(const char* s) { return append(s); }

      async_string& operator+=(const std::string_view& sv) { return append(sv); }

      async_string& operator+=(char c)
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

      int compare(const async_string& other) const
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

      friend bool operator==(const async_string& lhs, const async_string& rhs)
      {
         std::shared_lock lock1(lhs.mutex, std::defer_lock);
         std::shared_lock lock2(rhs.mutex, std::defer_lock);
         std::lock(lock1, lock2);
         return lhs.str == rhs.str;
      }

      friend bool operator!=(const async_string& lhs, const async_string& rhs) { return !(lhs == rhs); }

      friend bool operator<(const async_string& lhs, const async_string& rhs)
      {
         std::shared_lock lock1(lhs.mutex, std::defer_lock);
         std::shared_lock lock2(rhs.mutex, std::defer_lock);
         std::lock(lock1, lock2);
         return lhs.str < rhs.str;
      }

      friend bool operator<=(const async_string& lhs, const async_string& rhs) { return !(rhs < lhs); }

      friend bool operator>(const async_string& lhs, const async_string& rhs) { return rhs < lhs; }

      friend bool operator>=(const async_string& lhs, const async_string& rhs) { return !(lhs < rhs); }

      void swap(async_string& other)
      {
         if (this == &other) return;
         std::unique_lock lock1(mutex, std::defer_lock);
         std::unique_lock lock2(other.mutex, std::defer_lock);
         std::lock(lock1, lock2);
         str.swap(other.str);
      }

      friend void swap(async_string& lhs, async_string& rhs) { lhs.swap(rhs); }
   };

}

namespace glz::detail
{
   template <uint32_t Format>
   struct from<Format, glz::async_string>
   {
      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&& it, auto&& end) noexcept
      {
         std::shared_lock lock{value.mutex};
         read<Format>::template op<Opts>(value.str, ctx, it, end);
      }
   };

   template <uint32_t Format>
   struct to<Format, glz::async_string>
   {
      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&&... args) noexcept
      {
         std::unique_lock lock{value.mutex};
         write<Format>::template op<Opts>(value.str, ctx, args...);
      }
   };
}
