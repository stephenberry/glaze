// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <algorithm>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <utility>
#include <version>

#include "glaze/core/common.hpp"

// Provides a thread safe wrapper around a std::string, which Glaze knows how to serialize/deserialize safely

namespace glz
{
   struct async_string
   {
     private:
      std::string str;
      mutable std::shared_mutex mutex;

     public:
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

      explicit operator std::string() const
      {
         std::shared_lock lock{mutex};
         return str;
      }

      struct proxy
      {
         std::string* ptr{};
         std::unique_lock<std::shared_mutex> lock{};

        public:
         proxy(std::string& p, std::unique_lock<std::shared_mutex>&& lock) noexcept : ptr{&p}, lock(std::move(lock)) {}

         operator std::string_view() { return *ptr; }

         std::string* operator->() noexcept { return ptr; }
         const std::string* operator->() const noexcept { return ptr; }

         std::string& operator*() noexcept { return *ptr; }
         const std::string& operator*() const noexcept { return *ptr; }

         std::string& value() noexcept { return *ptr; }
         const std::string& value() const noexcept { return *ptr; }
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

         operator const std::string_view() const { return *ptr; }

         const std::string* operator->() const noexcept { return ptr; }

         const std::string& operator*() const noexcept { return *ptr; }

         const std::string& value() const noexcept { return *ptr; }
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

      template <class RHS>
         requires(std::same_as<std::remove_cvref_t<RHS>, std::string>)
      async_string& append(RHS&& s)
      {
         std::unique_lock lock(mutex);
         str.append(std::forward<RHS>(s));
         return *this;
      }

      async_string& append(const char* s, size_t count)
      {
         std::unique_lock lock(mutex);
         str.append(s, count);
         return *this;
      }

      async_string& append(const std::string_view& sv)
      {
         std::unique_lock lock(mutex);
         str.append(sv);
         return *this;
      }

      template <class RHS>
         requires(std::same_as<std::remove_cvref_t<RHS>, async_string>)
      async_string& append(RHS&& other)
      {
         std::unique_lock lock(mutex, std::defer_lock);
         std::shared_lock lock2(other.mutex, std::defer_lock);
         std::lock(lock, lock2);
         str.append(other.str);
         return *this;
      }

      template <class RHS>
         requires(std::same_as<std::remove_cvref_t<RHS>, std::string>)
      async_string& insert(size_t pos, RHS&& s)
      {
         std::unique_lock lock(mutex);
         str.insert(pos, std::forward<RHS>(s));
         return *this;
      }

      async_string& insert(size_t pos, const std::string_view& sv)
      {
         std::unique_lock lock(mutex);
         str.insert(pos, sv);
         return *this;
      }

      template <class RHS>
         requires(std::same_as<std::remove_cvref_t<RHS>, async_string>)
      async_string& insert(size_t pos, RHS&& other)
      {
         std::unique_lock lock(mutex, std::defer_lock);
         std::shared_lock lock2(other.mutex, std::defer_lock);
         std::lock(lock, lock2);
         str.insert(pos, other.str);
         return *this;
      }

      template <class RHS>
         requires(std::same_as<std::remove_cvref_t<RHS>, std::string>)
      async_string& operator+=(RHS&& s)
      {
         return append(std::forward<RHS>(s));
      }

      async_string& operator+=(const std::string_view& sv) { return append(sv); }

      async_string& operator+=(char c)
      {
         std::unique_lock lock(mutex);
         str += c;
         return *this;
      }

      void reserve(size_t count)
      {
         std::unique_lock lock(mutex);
         str.reserve(count);
      }

      void resize(size_t count)
      {
         std::unique_lock lock(mutex);
         str.resize(count);
      }

      void resize(size_t count, const char ch)
      {
         std::unique_lock lock(mutex);
         str.resize(count, ch);
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
         std::scoped_lock lock{mutex, other.mutex};
         return str.compare(other.str);
      }

      bool starts_with(const std::string_view other) const
      {
         std::shared_lock lock{mutex};
         return str.starts_with(other);
      }

      bool ends_with(const std::string_view other) const
      {
         std::shared_lock lock{mutex};
         return str.ends_with(other);
      }

      std::string substr(size_t pos = 0, size_t len = std::string::npos) const
      {
         std::shared_lock lock{mutex};
         return str.substr(pos, len);
      }

      friend bool operator==(const async_string& lhs, const async_string& rhs)
      {
         if (&lhs == &rhs) return true;
         std::shared_lock lock(lhs.mutex, std::defer_lock);
         std::shared_lock lock2(rhs.mutex, std::defer_lock);
         std::lock(lock, lock2);
         return lhs.str == rhs.str;
      }

      // async_string != async_string
      friend bool operator!=(const async_string& lhs, const async_string& rhs) { return !(lhs == rhs); }

      // async_string == std::string_view
      friend bool operator==(const async_string& lhs, std::string_view rhs)
      {
         std::shared_lock lock(lhs.mutex);
         return lhs.str == rhs;
      }

      // std::string_view == async_string
      friend bool operator==(std::string_view lhs, const async_string& rhs)
      {
         std::shared_lock lock(rhs.mutex);
         return lhs == rhs.str;
      }

      // async_string != std::string_view
      friend bool operator!=(const async_string& lhs, std::string_view rhs) { return !(lhs == rhs); }

      // std::string_view != async_string
      friend bool operator!=(std::string_view lhs, const async_string& rhs) { return !(lhs == rhs); }

      // async_string == std::string
      friend bool operator==(const async_string& lhs, const std::string& rhs)
      {
         std::shared_lock lock(lhs.mutex);
         return lhs.str == rhs;
      }

      // std::string == async_string
      friend bool operator==(const std::string& lhs, const async_string& rhs)
      {
         std::shared_lock lock(rhs.mutex);
         return lhs == rhs.str;
      }

      // async_string != std::string
      friend bool operator!=(const async_string& lhs, const std::string& rhs) { return !(lhs == rhs); }

      // std::string != async_string
      friend bool operator!=(const std::string& lhs, const async_string& rhs) { return !(lhs == rhs); }

      // async_string == const char*
      friend bool operator==(const async_string& lhs, const char* rhs)
      {
         std::shared_lock lock(lhs.mutex);
         return lhs.str == rhs;
      }

      // const char* == async_string
      friend bool operator==(const char* lhs, const async_string& rhs)
      {
         std::shared_lock lock(rhs.mutex);
         return lhs == rhs.str;
      }

      // async_string != const char*
      friend bool operator!=(const async_string& lhs, const char* rhs) { return !(lhs == rhs); }

      // const char* != async_string
      friend bool operator!=(const char* lhs, const async_string& rhs) { return !(lhs == rhs); }

      friend bool operator<(const async_string& lhs, const async_string& rhs)
      {
         std::scoped_lock lock{lhs.mutex, rhs.mutex};
         return lhs.str < rhs.str;
      }

      friend bool operator<=(const async_string& lhs, const async_string& rhs) { return !(rhs < lhs); }

      friend bool operator>(const async_string& lhs, const async_string& rhs) { return rhs < lhs; }

      friend bool operator>=(const async_string& lhs, const async_string& rhs) { return !(lhs < rhs); }

      void swap(async_string& other)
      {
         if (this == &other) return;
         std::scoped_lock lock{mutex, other.mutex};
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
         auto proxy = value.write();
         read<Format>::template op<Opts>(*proxy, ctx, it, end);
      }
   };

   template <uint32_t Format>
   struct to<Format, glz::async_string>
   {
      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&&... args) noexcept
      {
         auto proxy = value.read();
         write<Format>::template op<Opts>(*proxy, ctx, args...);
      }
   };
}

// Allow formatting via std::format
#ifdef __cpp_lib_format
#include <format>
namespace std
{
   template <>
   struct formatter<glz::async_string>
   {
      std::formatter<std::string> formatter;

      constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) { return formatter.parse(ctx); }

      template <class FormatContext>
      auto format(const glz::async_string& s, FormatContext& ctx) const -> decltype(ctx.out())
      {
         return formatter.format(*s.read(), ctx);
      }
   };
}
#endif
