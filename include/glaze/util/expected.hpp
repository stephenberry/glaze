/*
 * MIT License
 *
 * Copyright (c) 2022 Rishabh Dwivedi<rishabhdwivedi17@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#include <utility>

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

#ifdef __has_include
#if __has_include(<expected>)
#include <expected>
#elif __has_include(<experimental/expected>)
#include <experimental/expected>
#endif
#endif

#if defined(__cpp_lib_expected)
namespace glz
{
   template <class Expected, class Unexpected>
   using expected = std::expected<Expected, Unexpected>;

   template <class T>
   concept is_expected =
      std::same_as<std::remove_cvref_t<T>, expected<typename T::value_type, typename T::error_type> >;

#ifdef __clang__
   template <class Unexpected>
   struct unexpected : public std::unexpected<Unexpected>
   {
      using std::unexpected<Unexpected>::unexpected;
   };
   template <class Unexpected>
   unexpected(Unexpected) -> unexpected<Unexpected>;
#else
   template <class Unexpected>
   using unexpected = std::unexpected<Unexpected>;
#endif
}
#else

#include <concepts>
#include <deque>
#include <exception>
#include <functional>
#include <memory>
#include <type_traits>
#include <variant>

namespace glz
{

   template <class E>
   class unexpected
   {
     public:
      using value_type = E;
      constexpr unexpected(const unexpected&) = default;
      constexpr unexpected(unexpected&&) = default; // NOLINT
      constexpr auto operator=(const unexpected&) -> unexpected& = default;
      constexpr auto operator=(unexpected&&) -> unexpected& = default; // NOLINT
      ~unexpected() = default;

      template <class... Args>
         requires std::constructible_from<E, Args...>
      constexpr explicit unexpected(std::in_place_t /*unused*/, Args&&... args) : val(std::forward<Args>(args)...)
      {}

      template <class U, class... Args>
         requires std::constructible_from<E, std::initializer_list<U>&, Args...>
      constexpr explicit unexpected(std::in_place_t /*unused*/, std::initializer_list<U> i_list, Args&&... args)
         : val(i_list, std::forward<Args>(args)...)
      {}

      template <class Err = E>
         requires(!std::same_as<std::remove_cvref_t<Err>, unexpected>) &&
                 (!std::same_as<std::remove_cvref_t<Err>, std::in_place_t>) && std::constructible_from<E, Err>
      constexpr explicit unexpected(Err&& err) // NOLINT
         : val(std::forward<Err>(err))
      {}

      constexpr auto value() const& noexcept -> const E& { return val; }
      constexpr auto value() & noexcept -> E& { return val; }
      constexpr auto value() const&& noexcept -> const E&& { return std::move(val); }
      constexpr auto value() && noexcept -> E&& { return std::move(val); }

      constexpr void swap(unexpected& other) noexcept(std::is_nothrow_swappable_v<E>)
         requires(std::is_swappable_v<E>)
      {
         using std::swap;
         swap(val, other.val);
      }

      template <class E2>
         requires(requires(const E& x, E2 const& y) {
            {
               x == y
            } -> std::convertible_to<bool>;
         })
      friend constexpr auto operator==(const unexpected& x, const unexpected<E2>& y) -> bool
      {
         return x.value() == y.value();
      }

      friend constexpr void swap(unexpected& x, unexpected& y) noexcept(noexcept(x.swap(y)))
         requires(std::is_swappable_v<E>)
      {
         x.swap(y);
      }

     private:
      E val;
   };

   template <class E>
   unexpected(E) -> unexpected<E>;

   template <class E>
   class bad_expected_access;

   template <>
   class bad_expected_access<void> : public std::exception
   {
     protected:
      bad_expected_access() noexcept = default;
      bad_expected_access(const bad_expected_access&) = default;
      bad_expected_access(bad_expected_access&&) = default;
      auto operator=(const bad_expected_access&) -> bad_expected_access& = default;
      auto operator=(bad_expected_access&&) -> bad_expected_access& = default;
      ~bad_expected_access() override = default;

     public:
      auto what() const noexcept -> const char* override
      { // NOLINT
         return "bad expected access";
      }
   };

   template <class E>
   class bad_expected_access : public bad_expected_access<void>
   {
     public:
      explicit bad_expected_access(E e) : val(std::move(e)) {}
      auto what() const noexcept -> const char* override
      { // NOLINT
         return "bad expected access";
      }

      auto error() & noexcept -> E& { return val; }
      auto error() const& noexcept -> const E& { return val; }
      auto error() && noexcept -> E&& { return std::move(val); }
      auto error() const&& noexcept -> const E&& { return std::move(val); }

     private:
      E val;
   };

   struct unexpect_t
   {};
   inline constexpr unexpect_t unexpect{};

   namespace detail
   {
      template <typename T>
      concept non_void_destructible = std::same_as<T, void> || std::destructible<T>;
   }

   template <detail::non_void_destructible T, std::destructible E>
   class expected;

   namespace detail
   {
      template <typename T, typename E, typename U, typename G, typename UF, typename GF>
      concept expected_constructible_from_other =
         std::constructible_from<T, UF> && std::constructible_from<E, GF> &&
         (!std::constructible_from<T, expected<U, G>&>)&&(!std::constructible_from<T, expected<U, G> >)&&(!std::constructible_from<T, const expected<U, G>&>)&&(
            !std::constructible_from<
               T,
               const expected<
                  U,
                  G> >)&&(!std::
                             convertible_to<
                                expected<U, G>&,
                                T>)&&(!std::
                                         convertible_to<
                                            expected<U, G>&&,
                                            T>)&&(!std::
                                                     convertible_to<
                                                        const expected<U, G>&,
                                                        T>)&&(!std::
                                                                 convertible_to<
                                                                    const expected<U, G>&&,
                                                                    T>)&&(!std::
                                                                             constructible_from<
                                                                                unexpected<E>,
                                                                                expected<
                                                                                   U,
                                                                                   G>&>)&&(!std::
                                                                                              constructible_from<
                                                                                                 unexpected<E>,
                                                                                                 expected<
                                                                                                    U,
                                                                                                    G> >)&&(!std::
                                                                                                               constructible_from<
                                                                                                                  unexpected<
                                                                                                                     E>,
                                                                                                                  const expected<
                                                                                                                     U,
                                                                                                                     G>&>)&&(!std::
                                                                                                                                constructible_from<
                                                                                                                                   unexpected<
                                                                                                                                      E>,
                                                                                                                                   const expected<
                                                                                                                                      U,
                                                                                                                                      G> >);

      template <typename T>
      concept is_unexpected = std::same_as<std::remove_cvref_t<T>, unexpected<typename T::value_type> >;

      template <typename T>
      concept is_expected =
         std::same_as<std::remove_cvref_t<T>, expected<typename T::value_type, typename T::error_type> >;

      // This function makes sure expected doesn't get into valueless_by_exception
      // state due to any exception while assignment
      template <class T, class U, class... Args>
      constexpr void reinit_expected(T& newval, U& oldval, Args&&... args)
      {
         if constexpr (std::is_nothrow_constructible_v<T, Args...>) {
            std::destroy_at(std::addressof(oldval));
            std::construct_at(std::addressof(newval), std::forward<Args>(args)...);
         }
         else if constexpr (std::is_nothrow_move_constructible_v<T>) {
            T tmp(std::forward<Args>(args)...);
            std::destroy_at(std::addressof(oldval));
            std::construct_at(std::addressof(newval), std::move(tmp));
         }
         else {
            U tmp(std::move(oldval));
            std::destroy_at(std::addressof(oldval));
#if __cpp_exceptions
            try {
               std::construct_at(std::addressof(newval), std::forward<Args>(args)...);
            }
            catch (...) {
               std::construct_at(std::addressof(oldval), std::move(tmp));
               throw;
            }
#else
            std::abort();
#endif
         }
      }

   } // namespace detail

   template <detail::non_void_destructible T, std::destructible E>
   class expected
   {
     public:
      using value_type = T;
      using error_type = E;
      using unexpected_type = unexpected<E>;

      template <class U>
      using rebind = expected<U, error_type>;

      // constructors
      // postcondition: has_value() = true
      constexpr expected()
         requires std::default_initializable<T>
         : val{} {};

      // postcondition: has_value() = rhs.has_value()
      constexpr expected(const expected& rhs)
         requires std::copy_constructible<T> && std::copy_constructible<E> &&
                     std::is_trivially_copy_constructible_v<T> && std::is_trivially_copy_constructible_v<E>
      = default;

      // postcondition: has_value() = rhs.has_value()
      constexpr expected(const expected& rhs)
         requires std::copy_constructible<T> && std::copy_constructible<E>
         : has_val(rhs.has_val)
      {
         if (rhs.has_value()) {
            std::construct_at(std::addressof(this->val), *rhs);
         }
         else {
            std::construct_at(std::addressof(this->unex), rhs.error());
         }
      }

      constexpr expected(expected&&) noexcept(
         std::is_nothrow_move_constructible_v<T>&& std::is_nothrow_move_constructible_v<T>)
         requires std::move_constructible<T> && std::move_constructible<E> &&
                     std::is_trivially_move_constructible_v<T> && std::is_trivially_move_constructible_v<E>
      = default;

      constexpr expected(expected&& rhs) noexcept(
         std::is_nothrow_move_constructible_v<T>&& std::is_nothrow_move_constructible_v<T>)
         requires std::move_constructible<T> && std::move_constructible<E>
         : has_val(rhs.has_value())
      {
         if (rhs.has_value()) {
            std::construct_at(std::addressof(this->val), std::move(*rhs));
         }
         else {
            std::construct_at(std::addressof(this->unex), std::move(rhs.error()));
         }
      }

      template <class U, class G>
         requires detail::expected_constructible_from_other<T, E, U, G, const U&, const G&>
      constexpr explicit(!std::convertible_to<const U&, T> || !std::convertible_to<const G&, E>)
         expected(const expected<U, G>& rhs) // NOLINT
         : has_val(rhs.has_value())
      {
         using UF = const U&;
         using GF = const G&;
         if (rhs.has_value()) {
            std::construct_at(std::addressof(this->val), std::forward<UF>(*rhs));
         }
         else {
            std::construct_at(std::addressof(this->unex), std::forward<GF>(rhs.error()));
         }
      }

      template <class U, class G>
         requires detail::expected_constructible_from_other<T, E, U, G, U, G>
      constexpr explicit(!std::convertible_to<U, T> || !std::convertible_to<G, E>)
         expected(expected<U, G>&& rhs) // NOLINT
         : has_val(rhs.has_value())
      {
         using UF = const U&;
         using GF = const G&;
         if (rhs.has_value()) {
            std::construct_at(std::addressof(this->val), std::forward<UF>(*rhs));
         }
         else {
            std::construct_at(std::addressof(this->unex), std::forward<GF>(rhs.error()));
         }
      }

      template <class U = T>
         requires(!std::same_as<std::remove_cvref_t<U>, std::in_place_t>) &&
                 (!std::same_as<expected<T, E>, std::remove_cvref_t<U> >) &&
                 (!detail::is_unexpected<U>) && std::constructible_from<T, U>
      constexpr explicit(!std::convertible_to<U, T>) expected(U&& v) // NOLINT
         : val(std::forward<U>(v))
      {}

      template <class G>
         requires std::constructible_from<E, const G&>
      constexpr explicit(!std::convertible_to<const G&, E>) expected(const unexpected<G>& e) // NOLINT
         : has_val{false}, unex(std::forward<const G&>(e.value()))
      {}

      template <class G>
         requires std::constructible_from<E, G>
      constexpr explicit(!std::convertible_to<G, E>) expected(unexpected<G>&& e) // NOLINT
         : has_val{false}, unex(std::forward<G>(e.value()))
      {}

      template <class... Args>
         requires std::constructible_from<T, Args...>
      constexpr explicit expected(std::in_place_t /*unused*/, Args&&... args) : val(std::forward<Args>(args)...)
      {}

      template <class U, class... Args>
         requires std::constructible_from<T, std::initializer_list<U>&, Args...>
      constexpr explicit expected(std::in_place_t /*unused*/, std::initializer_list<U> il, Args&&... args)
         : val(il, std::forward<Args>(args)...)
      {}

      template <class... Args>
         requires std::constructible_from<E, Args...>
      constexpr explicit expected(unexpect_t /*unused*/, Args&&... args)
         : has_val{false}, unex(std::forward<Args>(args)...)
      {}

      template <class U, class... Args>
         requires std::constructible_from<E, std::initializer_list<U>&, Args...>
      constexpr explicit expected(unexpect_t /*unused*/, std::initializer_list<U> il, Args&&... args)
         : has_val(false), unex(il, std::forward<Args>(args)...)
      {}

      // destructor
      constexpr ~expected()
      {
         if constexpr (std::is_trivially_destructible_v<T> and std::is_trivially_destructible_v<E>) {
         }
         else if constexpr (std::is_trivially_destructible_v<T>) {
            if (!has_val) {
               std::destroy_at(std::addressof(this->unex));
            }
         }
         else if constexpr (std::is_trivially_destructible_v<E>) {
            if (has_val) {
               std::destroy_at(std::addressof(this->val));
            }
         }
         else {
            if (has_val) {
               std::destroy_at(std::addressof(this->val));
            }
            else {
               std::destroy_at(std::addressof(this->unex));
            }
         }
      }

      // assignment
      constexpr auto operator=(const expected& rhs) // NOLINT
         -> expected&
         requires std::is_copy_assignable_v<T> && std::is_copy_constructible_v<T> && std::is_copy_assignable_v<E> &&
                  std::is_copy_constructible_v<E> &&
                  (std::is_nothrow_move_constructible_v<E> || std::is_nothrow_move_constructible_v<T>)
      {
         has_val = rhs.has_value();
         if (this->has_value() and rhs.has_value()) {
            this->val = *rhs;
         }
         else if (this->has_value()) {
            detail::reinit_expected(this->unex, this->val, rhs.error());
         }
         else if (rhs.has_value()) {
            detail::reinit_expected(this->val, this->unex, *rhs);
         }
         else {
            this->unex = rhs.error();
         }
         return *this;
      }

      constexpr auto operator=(expected&& rhs) //
         noexcept(std::is_nothrow_move_assignable_v<T>&& std::is_nothrow_move_constructible_v<T>&&
                     std::is_nothrow_move_assignable_v<E>&& std::is_nothrow_move_constructible_v<E>) -> expected&
         requires std::is_move_constructible_v<T> && std::is_move_assignable_v<T> && std::is_move_constructible_v<E> &&
                  std::is_move_assignable_v<E> &&
                  (std::is_nothrow_move_constructible_v<T> || std::is_nothrow_move_constructible_v<E>)
      {
         has_val = rhs.has_value();
         if (this->has_value() and rhs.has_value()) {
            this->val = std::move(*rhs);
         }
         else if (this->has_value()) {
            detail::reinit_expected(this->unex, this->val, std::move(rhs.error()));
         }
         else if (rhs.has_value()) {
            detail::reinit_expected(this->val, this->unex, std::move(*rhs));
         }
         else {
            this->unex = std::move(rhs.error());
         }
         return *this;
      }

      template <class U = T>
         constexpr auto operator=(U&& rhs) -> expected& requires(!std::same_as<expected, std::remove_cvref_t<U> >) &&
         (!detail::is_unexpected<std::remove_cvref_t<U> >)&&std::constructible_from<T, U>&& std::is_assignable_v<T&,
                                                                                                                 U> &&
         (std::is_nothrow_constructible_v<T, U> || std::is_nothrow_move_constructible_v<T> ||
          std::is_nothrow_move_constructible_v<E>)
      {
         if (this->has_value()) {
            this->val = std::forward<U>(rhs);
            return *this;
         }
         detail::reinit_expected(this->val, this->unex, std::forward<U>(rhs));
         has_val = true;
         return *this;
      }

      template <class G>
         requires std::constructible_from<E, const G&> && std::is_assignable_v<E&, const G&> &&
                  (std::is_nothrow_constructible_v<E, const G&> || std::is_nothrow_move_constructible_v<T> ||
                   std::is_nothrow_move_constructible_v<E>)
      constexpr auto operator=(const unexpected<G>& e) -> expected&
      {
         using GF = const G&;
         if (has_value()) {
            detail::reinit_expected(this->unex, this->val, std::forward<GF>(e.value()));
         }
         else {
            this->unex = std::forward<GF>(e.value());
         }
         has_val = false;
         return *this;
      }

      template <class G>
         requires std::constructible_from<E, G> && std::is_assignable_v<E&, G> &&
                  (std::is_nothrow_constructible_v<E, G> || std::is_nothrow_move_constructible_v<T> ||
                   std::is_nothrow_move_constructible_v<E>)
      constexpr auto operator=(unexpected<G>&& e) -> expected&
      {
         using GF = G;
         if (has_value()) {
            detail::reinit_expected(this->unex, this->val, std::forward<GF>(e.value()));
         }
         else {
            this->unex = std::forward<GF>(e.value());
         }
         has_val = false;
         return *this;
      }

      // modifiers
      template <class... Args>
         requires std::is_nothrow_constructible_v<T, Args...>
      constexpr auto emplace(Args&&... args) noexcept -> T&
      {
         if (has_value()) {
            std::destroy_at(std::addressof(this->val));
         }
         else {
            std::destroy_at(std::addressof(this->unex));
            has_val = true;
         }
         return *std::construct_at(std::addressof(this->val), std::forward<Args>(args)...);
      }

      template <class U, class... Args>
         requires std::is_nothrow_constructible_v<T, std::initializer_list<U>&, Args...>
      constexpr auto emplace(std::initializer_list<U> il, Args&&... args) noexcept -> T&
      {
         if (has_value()) {
            std::destroy_at(std::addressof(this->val));
         }
         else {
            std::destroy_at(std::addressof(this->unex));
            has_val = true;
         }
         return *std::construct_at(std::addressof(this->val), il, std::forward<Args>(args)...);
      }

      // swap
      constexpr void swap(expected& rhs) noexcept(
         std::is_nothrow_constructible_v<T>&& std::is_nothrow_swappable_v<T>&& std::is_nothrow_move_constructible_v<E>&&
            std::is_nothrow_swappable_v<E>)
         requires std::is_swappable_v<T> && std::is_swappable_v<E> && std::is_move_constructible_v<T> &&
                  std::is_move_constructible_v<E> &&
                  (std::is_nothrow_constructible_v<T> || std::is_nothrow_constructible_v<E>)
      {
         if (rhs.has_value()) {
            if (has_value()) {
               using std::swap;
               swap(this->val, rhs.val);
            }
            else {
               rhs.swap(*this);
            }
         }
         else {
            if (has_value()) {
               if constexpr (std::is_nothrow_move_constructible_v<E>) {
                  E tmp(std::move(rhs.unex));
                  std::destroy_at(std::addressof(rhs.unex));
#if __cpp_exceptions
                  try {
                     std::construct_at(std::addressof(rhs.val), std::move(this->val));
                     std::destroy_at(std::addressof(this->val));
                     std::construct_at(std::addressof(this->unex), std::move(tmp));
                  }
                  catch (...) {
                     std::construct_at(std::addressof(rhs.unex), std::move(tmp));
                     throw;
                  }
#else
                  std::abort();
#endif
               }
               else {
                  T tmp(std::move(this->val));
                  std::destroy_at(std::addressof(this->val));
#if __cpp_exceptions
                  try {
                     std::construct_at(std::addressof(this->unex), std::move(rhs.unex));
                     std::destroy_at(std::addressof(rhs.unex));
                     std::construct_at(std::addressof(rhs.val), std::move(tmp));
                  }
                  catch (...) {
                     std::construct_at(std::addressof(this->val), std::move(tmp));
                     throw;
                  }
#else
                  std::abort();
#endif
               }
               has_val = false;
               rhs.has_val = true;
            }
            else {
               using std::swap;
               swap(this->unex, rhs.unex);
            }
         }
      }

      // observers

      // precondition: has_value() = true
      constexpr auto operator->() const noexcept -> const T* { return std::addressof(this->val); }

      // precondition: has_value() = true
      constexpr auto operator->() noexcept -> T* { return std::addressof(this->val); }

      // precondition: has_value() = true
      constexpr auto operator*() const& noexcept -> const T& { return this->val; }

      // precondition: has_value() = true
      constexpr auto operator*() & noexcept -> T& { return this->val; }

      // precondition: has_value() = true
      constexpr auto operator*() const&& noexcept -> const T&& { return std::move(this->val); }

      // precondition: has_value() = true
      constexpr auto operator*() && noexcept -> T&& { return std::move(this->val); }

      constexpr explicit operator bool() const noexcept { return has_val; }

      [[nodiscard]] constexpr auto has_value() const noexcept -> bool { return has_val; }

      constexpr auto value() const& -> const T&
      {
         if (has_value()) {
            return this->val;
         }
         GLZ_THROW_OR_ABORT(bad_expected_access(error()));
      }

      constexpr auto value() & -> T&
      {
         if (has_value()) {
            return this->val;
         }
         GLZ_THROW_OR_ABORT(bad_expected_access(error()));
      }

      constexpr auto value() const&& -> const T&&
      {
         if (has_value()) {
            return std::move(this->val);
         }
         GLZ_THROW_OR_ABORT(bad_expected_access(std::move(error())));
      }

      constexpr auto value() && -> T&&
      {
         if (has_value()) {
            return std::move(this->val);
         }
         GLZ_THROW_OR_ABORT(bad_expected_access(std::move(error())));
      }

      // precondition: has_value() = false
      constexpr auto error() const& -> const E& { return this->unex; }

      // precondition: has_value() = false
      constexpr auto error() & -> E& { return this->unex; }

      // precondition: has_value() = false
      constexpr auto error() const&& -> const E&& { return std::move(this->unex); }

      // precondition: has_value() = false
      constexpr auto error() && -> E&& { return std::move(this->unex); }

      template <class U>
         requires std::is_copy_constructible_v<T> && std::is_convertible_v<U, T>
      constexpr auto value_or(U&& v) const& -> T
      {
         return has_value() ? **this : static_cast<T>(std::forward<U>(v));
      }

      template <class U>
         requires std::is_move_constructible_v<T> && std::is_convertible_v<U, T>
      constexpr auto value_or(U&& v) && -> T
      {
         return has_value() ? std::move(**this) : static_cast<T>(std::forward<U>(v));
      }

      template <class F, class V = T&, class U = std::remove_cvref_t<std::invoke_result_t<F, V> > >
         requires detail::is_expected<U> && std::is_same_v<typename U::error_type, E> &&
                  std::is_copy_constructible_v<E> && std::is_copy_constructible_v<T>
      constexpr auto and_then(F&& f) &
      {
         if (has_value()) {
            return std::invoke(std::forward<F>(f), **this);
         }
         return U(unexpect, error());
      }

      template <class F, class V = const T&, class U = std::remove_cvref_t<std::invoke_result_t<F, V> > >
         requires detail::is_expected<U> && std::is_same_v<typename U::error_type, E> &&
                  std::is_copy_constructible_v<E> && std::is_copy_constructible_v<T>
      constexpr auto and_then(F&& f) const&
      {
         if (has_value()) {
            return std::invoke(std::forward<F>(f), **this);
         }
         return U(unexpect, error());
      }

      template <class F, class V = T&&, class U = std::remove_cvref_t<std::invoke_result_t<F, V> > >
         requires detail::is_expected<U> && std::is_same_v<typename U::error_type, E> &&
                  std::is_move_constructible_v<E> && std::is_move_constructible_v<T>
      constexpr auto and_then(F&& f) &&
      {
         if (has_value()) {
            return std::invoke(std::forward<F>(f), std::move(**this));
         }
         return U(unexpect, std::move(error()));
      }

      template <class F, class V = const T&&, class U = std::remove_cvref_t<std::invoke_result_t<F, V> > >
         requires detail::is_expected<U> && std::is_same_v<typename U::error_type, E> &&
                  std::is_move_constructible_v<E> && std::is_move_constructible_v<T>
      constexpr auto and_then(F&& f) const&&
      {
         if (has_value()) {
            return std::invoke(std::forward<F>(f), std::move(**this));
         }
         return U(unexpect, std::move(error()));
      }

      template <class F, class V = E&, class G = std::remove_cvref_t<std::invoke_result_t<F, V> > >
         requires detail::is_expected<G> && std::is_same_v<typename G::value_type, T> &&
                  std::is_copy_constructible_v<T> && std::is_copy_constructible_v<E>
      constexpr auto or_else(F&& f) &
      {
         if (has_value()) {
            return G(**this);
         }
         return std::invoke(std::forward<F>(f), error());
      }

      template <class F, class V = const E&, class G = std::remove_cvref_t<std::invoke_result_t<F, V> > >
         requires detail::is_expected<G> && std::is_same_v<typename G::value_type, T> &&
                  std::is_copy_constructible_v<T> && std::is_copy_constructible_v<E>
      constexpr auto or_else(F&& f) const&
      {
         if (has_value()) {
            return G(**this);
         }
         return std::invoke(std::forward<F>(f), error());
      }

      template <class F, class V = E&&, class G = std::remove_cvref_t<std::invoke_result_t<F, V> > >
         requires detail::is_expected<G> && std::is_same_v<typename G::value_type, T> &&
                  std::is_move_constructible_v<T> && std::is_move_constructible_v<E>
      constexpr auto or_else(F&& f) &&
      {
         if (has_value()) {
            return G(std::move(**this));
         }
         return std::invoke(std::forward<F>(f), std::move(error()));
      }

      template <class F, class V = const E&&, class G = std::remove_cvref_t<std::invoke_result_t<F, V> > >
         requires detail::is_expected<G> && std::is_same_v<typename G::value_type, T> &&
                  std::is_move_constructible_v<T> && std::is_move_constructible_v<E>
      constexpr auto or_else(F&& f) const&&
      {
         if (has_value()) {
            return G(std::move(**this));
         }
         return std::invoke(std::forward<F>(f), std::move(error()));
      }

      template <class F, class V = E&, class G = std::remove_cvref_t<std::invoke_result_t<F, V> > >
         requires std::is_void_v<G> && std::is_copy_constructible_v<T> && std::is_copy_constructible_v<E>
      constexpr auto or_else(F&& f) &
      {
         if (has_value()) {
            return expected(*this);
         }
         std::invoke(std::forward<F>(f), error());
         return expected(*this);
      }

      template <class F, class V = const E&, class G = std::remove_cvref_t<std::invoke_result_t<F, V> > >
         requires std::is_void_v<G> && std::is_copy_constructible_v<T> && std::is_copy_constructible_v<E>
      constexpr auto or_else(F&& f) const&
      {
         if (has_value()) {
            return expected(*this);
         }
         std::invoke(std::forward<F>(f), error());
         return expected(*this);
      }

      template <class F, class V = E&&, class G = std::remove_cvref_t<std::invoke_result_t<F, V> > >
         requires std::is_void_v<G> && std::is_move_constructible_v<T> && std::is_move_constructible_v<E> &&
                  std::is_copy_constructible_v<E>
      constexpr auto or_else(F&& f) &&
      {
         if (has_value()) {
            return expected(std::move(*this));
         }
         // TODO: is this copy necessary, as f can be just read argument function
         std::invoke(std::forward<F>(f), error());
         return expected(std::move(*this));
      }

      template <class F, class V = const E&&, class G = std::remove_cvref_t<std::invoke_result_t<F, V> > >
         requires std::is_void_v<G> && std::is_move_constructible_v<T> && std::is_move_constructible_v<E> &&
                  std::is_copy_constructible_v<E>
      constexpr auto or_else(F&& f) const&&
      {
         if (!has_value()) {
            return expected(std::move(*this));
         }
         // TODO: is this copy necessary, as f can be just read argument function
         std::invoke(std::forward<F>(f), error());
         return expected(std::move(*this));
      }

      template <class F, class V = T&, class U = std::remove_cvref_t<std::invoke_result_t<F, V> > >
         requires std::is_copy_constructible_v<E> && std::is_copy_constructible_v<T>
      constexpr auto transform(F&& f) &
      {
         if (has_value()) {
            if constexpr (!std::same_as<U, void>) {
               return expected<U, E>(std::invoke(std::forward<F>(f), **this));
            }
            else {
               std::invoke(std::forward<F>(f), std::move(**this));
               return expected<U, E>();
            }
         }
         return expected<U, E>(unexpect, error());
      }

      template <class F, class V = const T&, class U = std::remove_cvref_t<std::invoke_result_t<F, V> > >
         requires std::is_copy_constructible_v<E> && std::is_copy_constructible_v<T>
      constexpr auto transform(F&& f) const&
      {
         if (has_value()) {
            if constexpr (!std::same_as<U, void>) {
               return expected<U, E>(std::invoke(std::forward<F>(f), **this));
            }
            else {
               std::invoke(std::forward<F>(f), std::move(**this));
               return expected<U, E>();
            }
         }
         return expected<U, E>(unexpect, error());
      }

      template <class F, class V = T&&, class U = std::remove_cvref_t<std::invoke_result_t<F, V> > >
         requires std::is_move_constructible_v<E> && std::is_move_constructible_v<T>
      constexpr auto transform(F&& f) &&
      {
         if (has_value()) {
            if constexpr (!std::same_as<U, void>) {
               return expected<U, E>(std::invoke(std::forward<F>(f), std::move(**this)));
            }
            else {
               std::invoke(std::forward<F>(f), std::move(**this));
               return expected<U, E>();
            }
         }
         return expected<U, E>(unexpect, std::move(error()));
      }

      template <class F, class V = const T&&, class U = std::remove_cvref_t<std::invoke_result_t<F, V> > >
         requires std::is_move_constructible_v<E> && std::is_move_constructible_v<T>
      constexpr auto transform(F&& f) const&&
      {
         if (has_value()) {
            if constexpr (!std::same_as<U, void>) {
               return expected<U, E>(std::invoke(std::forward<F>(f), std::move(**this)));
            }
            else {
               std::invoke(std::forward<F>(f), std::move(**this));
               return expected<U, E>();
            }
         }
         return expected<U, E>(unexpect, std::move(error()));
      }

      template <class F, class V = E&, class G = std::remove_cvref_t<std::invoke_result_t<F, V> > >
         requires std::is_copy_constructible_v<T> && std::is_copy_constructible_v<E>
      constexpr auto transform_error(F&& f) &
      {
         if (has_value()) {
            return expected<T, G>(**this);
         }
         return expected<T, G>(unexpect, std::invoke(std::forward<F>(f), error()));
      }

      template <class F, class V = const E&, class G = std::remove_cvref_t<std::invoke_result_t<F, V> > >
         requires std::is_copy_constructible_v<T> && std::is_copy_constructible_v<E>
      constexpr auto transform_error(F&& f) const&
      {
         if (has_value()) {
            return expected<T, G>(**this);
         }
         return expected<T, G>(unexpect, std::invoke(std::forward<F>(f), error()));
      }

      template <class F, class V = E&&, class G = std::remove_cvref_t<std::invoke_result_t<F, V> > >
         requires std::is_move_constructible_v<T> && std::is_move_constructible_v<E>
      constexpr auto transform_error(F&& f) &&
      {
         if (has_value()) {
            return expected<T, G>(std::move(**this));
         }
         return expected<T, G>(unexpect, std::invoke(std::forward<F>(f), std::move(error())));
      }

      template <class F, class V = const E&&, class G = std::remove_cvref_t<std::invoke_result_t<F, V> > >
         requires std::is_move_constructible_v<T> && std::is_move_constructible_v<E>
      constexpr auto transform_error(F&& f) const&&
      {
         if (has_value()) {
            return expected<T, G>(std::move(**this));
         }
         return expected<T, G>(unexpect, std::invoke(std::forward<F>(f), std::move(error())));
      }

      // equality operators
      template <class T2, class E2>
         requires(!std::is_void_v<T2>) && requires(const T& t1, T2 const& t2, const E& e1, E2 const& e2) {
            {
               t1 == t2
            } -> std::convertible_to<bool>;
            {
               e1 == e2
            } -> std::convertible_to<bool>;
         }
      friend constexpr auto operator==(const expected& x, const expected<T2, E2>& y) -> bool
      {
         if (x.has_value() != y.has_value()) {
            return false;
         }
         return x.has_value() ? (*x == *y) : (x.error() == y.error());
      }

      template <typename T2>
         requires(!detail::is_expected<T2>)
      friend constexpr bool operator==(const expected& x, const T2& v)
      {
         return x.has_value() && bool(*x == v);
      }

      template <class E2>
         requires requires(const E& x, const unexpected<E2>& e) {
            {
               x == e.value()
            } -> std::convertible_to<bool>;
         }
      friend constexpr auto operator==(const expected& x, const unexpected<E2>& e) -> bool
      {
         return !x.has_value() && bool(x.error() == e.value());
      }

      // specialized algorithms
      friend constexpr void swap(expected& x, expected& y) noexcept(noexcept(x.swap(y))) { x.swap(y); }

     private:
      bool has_val{true};
      union {
         T val;
         E unex;
      };
   };

   template <std::destructible E>
   class expected<void, E>
   {
     public:
      using value_type = void;
      using error_type = E;
      using unexpected_type = unexpected<E>;

      template <class U>
      using rebind = expected<U, error_type>;

      // constructors

      // postcondition: has_value() = true
      constexpr expected() noexcept {} // NOLINT

      constexpr expected(const expected& rhs)
         requires std::is_copy_constructible_v<E> && std::is_trivially_copy_constructible_v<E>
      = default;

      constexpr expected(const expected& rhs)
         requires std::is_copy_constructible_v<E>
         : has_val(rhs.has_value())
      {
         if (!rhs.has_value()) {
            std::construct_at(std::addressof(this->unex), rhs.error());
         }
      }

      constexpr expected(expected&&) noexcept(std::is_nothrow_move_constructible_v<E>)
         requires std::is_move_constructible_v<E> && std::is_trivially_move_constructible_v<E>
      = default;

      constexpr expected(expected&& rhs) noexcept(std::is_nothrow_move_constructible_v<E>)
         requires std::is_move_constructible_v<E>
         : has_val(rhs.has_value())
      {
         if (!rhs.has_value()) {
            std::construct_at(std::addressof(this->unex), std::move(rhs.error()));
         }
      }

      template <class U, class G>
         requires std::is_void_v<U> && std::is_constructible_v<E, const G&> &&
                  (!std::is_constructible_v<unexpected<E>, expected<U, G>&>) &&
                  (!std::is_constructible_v<unexpected<E>, expected<U, G> >) &&
                  (!std::is_constructible_v<unexpected<E>, const expected<U, G>&>) &&
                  (!std::is_constructible_v<unexpected<E>, const expected<U, G>&>)
      constexpr explicit(!std::is_convertible_v<const G&, E>) expected(const expected<U, G>& rhs) // NOLINT
         : has_val(rhs.has_value())
      {
         if (!rhs.has_value()) {
            std::construct_at(std::addressof(this->unex), std::forward<const G&>(rhs.error()));
         }
      }

      template <class U, class G>
         requires std::is_void_v<U> && std::is_constructible_v<E, G> &&
                  (!std::is_constructible_v<unexpected<E>, expected<U, G>&>) &&
                  (!std::is_constructible_v<unexpected<E>, expected<U, G> >) &&
                  (!std::is_constructible_v<unexpected<E>, const expected<U, G>&>) &&
                  (!std::is_constructible_v<unexpected<E>, const expected<U, G>&>)
      constexpr explicit(!std::is_convertible_v<const G&, E>) expected(expected<U, G>&& rhs) // NOLINT
         : has_val(rhs.has_value())
      {
         if (!rhs.has_value()) {
            std::construct_at(std::addressof(this->unex), std::forward<G>(rhs.error()));
         }
      }

      template <class G>
         requires std::is_constructible_v<E, const G&>
      constexpr explicit(!std::is_convertible_v<const G&, E>) expected(const unexpected<G>& e) // NOLINT
         : has_val(false), unex(std::forward<const G&>(e.value()))
      {}

      template <class G>
         requires std::is_constructible_v<E, G>
      constexpr explicit(!std::is_convertible_v<G, E>) expected(unexpected<G>&& e) // NOLINT
         : has_val(false), unex(std::forward<G>(e.value()))
      {}

      constexpr explicit expected(std::in_place_t /*unused*/) noexcept {}

      template <class... Args>
         requires std::is_constructible_v<E, Args...>
      constexpr explicit expected(unexpect_t /*unused*/, Args&&... args)
         : has_val(false), unex(std::forward<Args>(args)...)
      {}

      template <class U, class... Args>
         requires std::is_constructible_v<E, std::initializer_list<U>&, Args...>
      constexpr explicit expected(unexpect_t /*unused*/, std::initializer_list<U> il, Args... args)
         : has_val(false), unex(il, std::forward<Args>(args)...)
      {}

      // destructor
      constexpr ~expected()
      {
         if constexpr (std::is_trivially_destructible_v<E>) {
         }
         else {
            if (!has_value()) std::destroy_at(std::addressof(this->unex));
         }
      }

      // assignment
      constexpr auto operator=(const expected& rhs) -> expected& // NOLINT
         requires std::is_copy_assignable_v<E> && std::is_copy_constructible_v<E>
      {
         if (has_value() && rhs.has_value()) {
         }
         else if (has_value()) {
            std::construct_at(std::addressof(this->unex), rhs.unex);
            has_val = false;
         }
         else if (rhs.has_value()) {
            std::destroy_at(std::addressof(this->unex));
            has_val = true;
         }
         else {
            this->unex = rhs.error();
         }
         return *this;
      }

      constexpr auto operator=(expected&& rhs) noexcept(
         std::is_nothrow_move_constructible_v<E>&& std::is_nothrow_move_assignable_v<E>) -> expected&
         requires std::is_move_constructible_v<E> && std::is_move_assignable_v<E>
      {
         if (has_value() && rhs.has_value()) {
         }
         else if (has_value()) {
            std::construct_at(std::addressof(this->unex), std::move(rhs.unex));
            has_val = false;
         }
         else if (rhs.has_value()) {
            std::destroy_at(std::addressof(this->unex));
            has_val = true;
         }
         else {
            this->unex = std::move(rhs.error());
         }
         return *this;
      }

      template <class G>
         requires std::is_constructible_v<E, const G&> and std::is_assignable_v<E&, const G&>
      constexpr auto operator=(const unexpected<G>& e) -> expected&
      {
         if (has_value()) {
            std::construct_at(std::addressof(this->unex), std::forward<const G&>(e.value()));
            has_val = false;
         }
         else {
            this->unex = std::forward<const G&>(e.value());
         }
         return *this;
      }

      template <class G>
         requires std::is_constructible_v<E, G> && std::is_assignable_v<E&, G>
      constexpr auto operator=(unexpected<G>&& e) -> expected&
      {
         if (has_value()) {
            std::construct_at(std::addressof(this->unex), std::forward<G>(e.value()));
            has_val = false;
         }
         else {
            this->unex = std::forward<G>(e.value());
         }
         return *this;
      }

      // modifiers
      constexpr void emplace() noexcept
      {
         if (!has_value()) {
            std::destroy_at(std::addressof(this->unex));
            has_val = true;
         }
      }

      // swap
      constexpr void swap(expected& rhs) noexcept(
         std::is_nothrow_move_constructible_v<E>&& std::is_nothrow_swappable_v<E>)
         requires std::is_swappable_v<E> && std::is_move_constructible_v<E>
      {
         if (rhs.has_value()) {
            if (has_value()) {
            }
            else {
               rhs.swap(*this);
            }
         }
         else {
            if (has_value()) {
               std::construct_at(std::addressof(this->unex), std::move(rhs.unex));
               std::destroy_at(std::addressof(rhs.unex));
               has_val = false;
               rhs.has_val = true;
            }
            else {
               using std::swap;
               swap(this->unex, rhs.unex);
            }
         }
      }

      // observers
      constexpr explicit operator bool() const noexcept { return has_val; }

      [[nodiscard]] constexpr auto has_value() const noexcept -> bool { return has_val; }

      // precondition: has_value() = true
      constexpr void operator*() const noexcept {}

      constexpr void value() const&
      {
         if (!has_value()) {
            GLZ_THROW_OR_ABORT(bad_expected_access(error()));
         }
      }

      constexpr void value() &&
      {
         if (!has_value()) {
            GLZ_THROW_OR_ABORT(bad_expected_access(std::move(error())));
         }
      }

      // precondition: has_value() = false
      constexpr auto error() const& -> const E& { return this->unex; }

      // precondition: has_value() = false
      constexpr auto error() & -> E& { return this->unex; }

      // precondition: has_value() = false
      constexpr auto error() const&& -> const E&& { return std::move(this->unex); }

      // precondition: has_value() = false
      constexpr auto error() && -> E&& { return std::move(this->unex); }

      // monadic
      template <class F, class U = std::remove_cvref_t<std::invoke_result_t<F> > >
         requires detail::is_expected<U> && std::is_same_v<typename U::error_type, E> && std::is_copy_constructible_v<E>
      constexpr auto and_then(F&& f) &
      {
         if (has_value()) {
            return std::invoke(std::forward<F>(f));
         }
         return U(unexpect, error());
      }

      template <class F, class U = std::remove_cvref_t<std::invoke_result_t<F> > >
         requires detail::is_expected<U> && std::is_same_v<typename U::error_type, E> && std::is_copy_constructible_v<E>
      constexpr auto and_then(F&& f) const&
      {
         if (has_value()) {
            return std::invoke(std::forward<F>(f));
         }
         return U(unexpect, error());
      }

      template <class F, class U = std::remove_cvref_t<std::invoke_result_t<F> > >
         requires detail::is_expected<U> && std::is_same_v<typename U::error_type, E> && std::is_move_constructible_v<E>
      constexpr auto and_then(F&& f) &&
      {
         if (has_value()) {
            return std::invoke(std::forward<F>(f));
         }
         return U(unexpect, std::move(error()));
      }

      template <class F, class U = std::remove_cvref_t<std::invoke_result_t<F> > >
         requires detail::is_expected<U> && std::is_same_v<typename U::error_type, E> && std::is_move_constructible_v<E>
      constexpr auto and_then(F&& f) const&&
      {
         if (has_value()) {
            return std::invoke(std::forward<F>(f));
         }
         return U(unexpect, std::move(error()));
      }

      template <class F, class V = E&, class G = std::remove_cvref_t<std::invoke_result_t<F, V> > >
         requires detail::is_expected<G> && std::is_same_v<typename G::value_type, void> &&
                  std::is_copy_constructible_v<E>
      constexpr auto or_else(F&& f) &
      {
         if (has_value()) {
            return G{};
         }
         return std::invoke(std::forward<F>(f), error());
      }

      template <class F, class V = const E&, class G = std::remove_cvref_t<std::invoke_result_t<F, V> > >
         requires detail::is_expected<G> && std::is_same_v<typename G::value_type, void> &&
                  std::is_copy_constructible_v<E>
      constexpr auto or_else(F&& f) const&
      {
         if (has_value()) {
            return G{};
         }
         return std::invoke(std::forward<F>(f), error());
      }

      template <class F, class V = E&&, class G = std::remove_cvref_t<std::invoke_result_t<F, V> > >
         requires detail::is_expected<G> && std::is_same_v<typename G::value_type, void> &&
                  std::is_move_constructible_v<E>
      constexpr auto or_else(F&& f) &&
      {
         if (has_value()) {
            return G{};
         }
         return std::invoke(std::forward<F>(f), std::move(error()));
      }

      template <class F, class V = const E&&, class G = std::remove_cvref_t<std::invoke_result_t<F, V> > >
         requires detail::is_expected<G> && std::is_same_v<typename G::value_type, void> &&
                  std::is_move_constructible_v<E>
      constexpr auto or_else(F&& f) const&&
      {
         if (has_value()) {
            return G{};
         }
         return std::invoke(std::forward<F>(f), std::move(error()));
      }

      template <class F, class V = E&, class G = std::remove_cvref_t<std::invoke_result_t<F, V> > >
         requires std::is_void_v<G> && std::is_copy_constructible_v<E>
      constexpr auto or_else(F&& f) &
      {
         if (has_value()) {
            return expected(*this);
         }
         std::invoke(std::forward<F>(f), error());
         return expected(*this);
      }

      template <class F, class V = const E&, class G = std::remove_cvref_t<std::invoke_result_t<F, V> > >
         requires std::is_void_v<G> && std::is_copy_constructible_v<E>
      constexpr auto or_else(F&& f) const&
      {
         if (has_value()) {
            return expected(*this);
         }
         std::invoke(std::forward<F>(f), error());
         return expected(*this);
      }

      template <class F, class V = E&&, class G = std::remove_cvref_t<std::invoke_result_t<F, V> > >
         requires std::is_void_v<G> && std::is_move_constructible_v<E> && std::is_copy_constructible_v<E>
      constexpr auto or_else(F&& f) &&
      {
         if (has_value()) {
            return expected(std::move(*this));
         }
         // TODO: is this copy necessary, as f can be just read argument function
         std::invoke(std::forward<F>(f), error());
         return expected(std::move(*this));
      }

      template <class F, class V = const E&&, class G = std::remove_cvref_t<std::invoke_result_t<F, V> > >
         requires std::is_void_v<G> && std::is_move_constructible_v<E> && std::is_copy_constructible_v<E>
      constexpr auto or_else(F&& f) const&&
      {
         if (!has_value()) {
            return expected(std::move(*this));
         }
         // TODO: is this copy necessary, as f can be just read argument function
         std::invoke(std::forward<F>(f), error());
         return expected(std::move(*this));
      }

      template <class F, class U = std::remove_cvref_t<std::invoke_result_t<F> > >
         requires std::is_copy_constructible_v<E>
      constexpr auto transform(F&& f) &
      {
         if (has_value()) {
            if constexpr (!std::same_as<U, void>) {
               return expected<U, E>(std::invoke(std::forward<F>(f)));
            }
            else {
               std::invoke(std::forward<F>(f));
               return expected<U, E>();
            }
         }
         return expected<U, E>(unexpect, error());
      }

      template <class F, class U = std::remove_cvref_t<std::invoke_result_t<F> > >
         requires std::is_copy_constructible_v<E>
      constexpr auto transform(F&& f) const&
      {
         if (has_value()) {
            if constexpr (!std::same_as<U, void>) {
               return expected<U, E>(std::invoke(std::forward<F>(f)));
            }
            else {
               std::invoke(std::forward<F>(f));
               return expected<U, E>();
            }
         }
         return expected<U, E>(unexpect, error());
      }

      template <class F, class U = std::remove_cvref_t<std::invoke_result_t<F> > >
         requires std::is_move_constructible_v<E>
      constexpr auto transform(F&& f) &&
      {
         if (has_value()) {
            if constexpr (!std::same_as<U, void>) {
               return expected<U, E>(std::invoke(std::forward<F>(f)));
            }
            else {
               std::invoke(std::forward<F>(f));
               return expected<U, E>();
            }
         }
         return expected<U, E>(unexpect, std::move(error()));
      }

      template <class F, class U = std::remove_cvref_t<std::invoke_result_t<F> > >
         requires std::is_move_constructible_v<E>
      constexpr auto transform(F&& f) const&&
      {
         if (has_value()) {
            if constexpr (!std::same_as<U, void>) {
               return expected<U, E>(std::invoke(std::forward<F>(f)));
            }
            else {
               std::invoke(std::forward<F>(f));
               return expected<U, E>();
            }
         }
         return expected<U, E>(unexpect, std::move(error()));
      }

      template <class F, class V = E&, class G = std::remove_cvref_t<std::invoke_result_t<F, V> > >
         requires std::is_copy_constructible_v<E>
      constexpr auto transform_error(F&& f) &
      {
         if (has_value()) {
            return expected<void, G>{};
         }
         return expected<void, G>(unexpect, std::invoke(std::forward<F>(f), error()));
      }

      template <class F, class V = const E&, class G = std::remove_cvref_t<std::invoke_result_t<F, V> > >
         requires std::is_copy_constructible_v<E>
      constexpr auto transform_error(F&& f) const&
      {
         if (has_value()) {
            return expected<void, G>{};
         }
         return expected<void, G>(unexpect, std::invoke(std::forward<F>(f), error()));
      }

      template <class F, class V = E&&, class G = std::remove_cvref_t<std::invoke_result_t<F, V> > >
         requires std::is_move_constructible_v<E>
      constexpr auto transform_error(F&& f) &&
      {
         if (has_value()) {
            return expected<void, G>{};
         }
         return expected<void, G>(unexpect, std::invoke(std::forward<F>(f), std::move(error())));
      }

      template <class F, class V = const E&&, class G = std::remove_cvref_t<std::invoke_result_t<F, V> > >
         requires std::is_move_constructible_v<E>
      constexpr auto transform_error(F&& f) const&&
      {
         if (has_value()) {
            return expected<void, G>{};
         }
         return expected<void, G>(unexpect, std::invoke(std::forward<F>(f), std::move(error())));
      }

      // expected equality operators
      template <class T2, class E2>
         requires std::is_void_v<T2> && requires(E e, E2 e2) {
            {
               e == e2
            } -> std::convertible_to<bool>;
         }
      friend constexpr auto operator==(const expected& x, const expected<T2, E2>& y) -> bool
      {
         if (x.has_value() != y.has_value()) return false;
         return x.has_value() or bool(x.error() == y.error());
      }

      template <class E2>
         requires requires(const expected& x, const unexpected<E2>& e) {
            {
               x.error() == e.value()
            } -> std::convertible_to<bool>;
         }
      friend constexpr auto operator==(const expected& x, const unexpected<E2>& e) -> bool
      {
         return !x.has_value() && bool(x.error() == e.value());
      }

      // specialized algorithms
      friend constexpr void swap(expected& x, expected& y) noexcept(noexcept(x.swap(y))) { x.swap(y); }

     private:
      bool has_val{true};
      union {
         E unex;
      };
   };

}

#endif
