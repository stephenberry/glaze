// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <algorithm>
#include <compare>
#include <cstddef>
#include <cstring>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <new>
#include <ranges>
#include <stdexcept>
#include <type_traits>
#include <utility>

#ifndef GLZ_THROW_OR_ABORT
#if __cpp_exceptions
#define GLZ_THROW_OR_ABORT(EXC) (throw(EXC))
#define GLZ_NOEXCEPT noexcept(false)
#else
#include <cstdlib> // for std::abort
#define GLZ_THROW_OR_ABORT(EXC) (std::abort())
#define GLZ_NOEXCEPT noexcept(true)
#endif
#endif

namespace glz
{
   template <class T, size_t N>
   class inplace_vector
   {
     public:
      // types
      using value_type = T;
      using pointer = T*;
      using const_pointer = const T*;
      using reference = value_type&;
      using const_reference = const value_type&;
      using size_type = size_t;
      using difference_type = std::ptrdiff_t;
      using iterator = T*;
      using const_iterator = const T*;
      using reverse_iterator = std::reverse_iterator<iterator>;
      using const_reverse_iterator = std::reverse_iterator<const_iterator>;

     private:
      // Storage for elements
      alignas(T) unsigned char storage[sizeof(T) * N];
      size_type size_ = 0;

      // Get pointer to the first element
      constexpr T* data_ptr() noexcept { return std::launder(reinterpret_cast<T*>(storage)); }

      constexpr const T* data_ptr() const noexcept { return std::launder(reinterpret_cast<const T*>(storage)); }

      // Helper to destroy a range of elements
      constexpr void destroy_range(size_type start, size_type end) noexcept
      {
         if constexpr (!std::is_trivially_destructible_v<T>) {
            for (size_type i = start; i < end; ++i) {
               std::destroy_at(data_ptr() + i);
            }
         }
      }

     public:
      // Constructors
      constexpr inplace_vector() noexcept = default;

      constexpr explicit inplace_vector(size_type n)
      {
         if (n > N) GLZ_THROW_OR_ABORT(std::bad_alloc());

         // Use std::uninitialized_value_construct_n for proper value-initialization
         std::uninitialized_value_construct_n(data_ptr(), n);
         size_ = n;
      }

      constexpr inplace_vector(size_type n, const T& value)
      {
         if (n > N) GLZ_THROW_OR_ABORT(std::bad_alloc());

         if constexpr (std::is_trivially_copyable_v<T>) {
            // Construct first element then copy it
            if (n > 0) {
               std::construct_at(data_ptr(), value);
               for (size_type i = 1; i < n; ++i) {
                  std::memcpy(data_ptr() + i, data_ptr(), sizeof(T));
               }
            }
         }
         else {
            for (size_type i = 0; i < n; ++i) std::construct_at(data_ptr() + i, value);
         }

         size_ = n;
      }

      template <class InputIterator>
      constexpr inplace_vector(InputIterator first, InputIterator last)
      {
         assign(first, last);
      }

#ifdef __cpp_lib_containers_ranges
      template <std::ranges::input_range R>
         requires std::convertible_to<std::ranges::range_reference_t<R>, T>
      constexpr inplace_vector(std::from_range_t, R&& rg)
      {
         assign_range(std::forward<R>(rg));
      }
#endif

      // Trivially copyable copy constructor
      constexpr inplace_vector(const inplace_vector& other)
         requires std::is_trivially_copyable_v<T>
      = default;

      // Non-trivially copyable copy constructor
      constexpr inplace_vector(const inplace_vector& other)
         requires(!std::is_trivially_copyable_v<T>)
      {
         assign(other.begin(), other.end());
      }

      // Trivially copyable move constructor
      constexpr inplace_vector(inplace_vector&& other) noexcept(N == 0 || std::is_nothrow_move_constructible_v<T>)
         requires std::is_trivially_copyable_v<T>
      = default;

      // Non-trivially copyable move constructor
      constexpr inplace_vector(inplace_vector&& other) noexcept(N == 0 || std::is_nothrow_move_constructible_v<T>)
         requires(!std::is_trivially_copyable_v<T>)
      {
         assign(std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()));
         other.clear();
      }

      constexpr inplace_vector(std::initializer_list<T> il) { assign(il.begin(), il.end()); }

      // Trivially destructible destructor
      constexpr ~inplace_vector()
         requires std::is_trivially_destructible_v<T>
      = default;

      // Non-trivially destructible destructor
      constexpr ~inplace_vector()
         requires(!std::is_trivially_destructible_v<T>)
      {
         destroy_range(0, size_);
      }

      // Trivially copyable copy assignment
      constexpr inplace_vector& operator=(const inplace_vector& other)
         requires std::is_trivially_copyable_v<T>
      = default;

      // Non-trivially copyable copy assignment
      constexpr inplace_vector& operator=(const inplace_vector& other)
         requires(!std::is_trivially_copyable_v<T>)
      {
         if (this != &other) {
            assign(other.begin(), other.end());
         }
         return *this;
      }

      // Trivially copyable move assignment
      constexpr inplace_vector& operator=(inplace_vector&& other) noexcept(N == 0 ||
                                                                           (std::is_nothrow_move_assignable_v<T> &&
                                                                            std::is_nothrow_move_constructible_v<T>))
         requires std::is_trivially_copyable_v<T>
      = default;

      // Non-trivially copyable move assignment
      constexpr inplace_vector& operator=(inplace_vector&& other) noexcept(N == 0 ||
                                                                           (std::is_nothrow_move_assignable_v<T> &&
                                                                            std::is_nothrow_move_constructible_v<T>))
         requires(!std::is_trivially_copyable_v<T>)
      {
         if (this != &other) {
            clear();
            assign(std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()));
            other.clear();
         }
         return *this;
      }

      constexpr inplace_vector& operator=(std::initializer_list<T> il)
      {
         assign(il.begin(), il.end());
         return *this;
      }

      // Assign methods
      template <std::input_iterator InputIterator>
      constexpr void assign(InputIterator first, InputIterator last)
      {
         clear();

         // Calculate distance if possible (for random access iterators)
         if constexpr (std::random_access_iterator<InputIterator>) {
            if (std::distance(first, last) > static_cast<difference_type>(N)) GLZ_THROW_OR_ABORT(std::bad_alloc());
         }

         if constexpr (std::is_trivially_copyable_v<T> && std::contiguous_iterator<InputIterator>) {
            // Fast path for contiguous trivially copyable types
            auto count = std::distance(first, last);
            if (count > 0) {
               std::memcpy(storage, std::to_address(first), count * sizeof(T));
               size_ = count;
            }
         }
         else {
            // General path
            while (first != last) {
               if (size_ >= N) GLZ_THROW_OR_ABORT(std::bad_alloc());

               std::construct_at(data_ptr() + size_, *first);
               ++size_;
               ++first;
            }
         }
      }

      template <std::ranges::input_range R>
         requires std::convertible_to<std::ranges::range_reference_t<R>, T>
      constexpr void assign_range(R&& rg)
      {
         clear();

         // Check size if possible
         if constexpr (std::ranges::sized_range<R>) {
            if (std::ranges::size(rg) > N) GLZ_THROW_OR_ABORT(std::bad_alloc());
         }

         // Fast path for contiguous ranges of trivially copyable types
         if constexpr (std::is_trivially_copyable_v<T> && std::ranges::contiguous_range<R> &&
                       std::same_as<std::remove_cvref_t<std::ranges::range_reference_t<R>>, T>) {
            auto count = std::ranges::size(rg);
            if (count > 0) {
               std::memcpy(storage, std::ranges::data(rg), count * sizeof(T));
               size_ = count;
            }
         }
         else {
            // General path
            for (auto&& item : rg) {
               if (size_ >= N) GLZ_THROW_OR_ABORT(std::bad_alloc());

               std::construct_at(data_ptr() + size_, std::forward<decltype(item)>(item));
               ++size_;
            }
         }
      }

      constexpr void assign(size_type n, const T& value)
      {
         if (n > N) GLZ_THROW_OR_ABORT(std::bad_alloc());

         clear();

         if constexpr (std::is_trivially_copyable_v<T>) {
            if (n > 0) {
               // Construct first element, then use memcpy for the rest
               std::construct_at(data_ptr(), value);
               for (size_type i = 1; i < n; ++i) {
                  std::memcpy(data_ptr() + i, data_ptr(), sizeof(T));
               }
               size_ = n;
            }
         }
         else {
            for (size_type i = 0; i < n; ++i) std::construct_at(data_ptr() + size_++, value);
         }
      }

      // Iterators
      constexpr iterator begin() noexcept { return data_ptr(); }

      constexpr const_iterator begin() const noexcept { return data_ptr(); }

      constexpr iterator end() noexcept { return data_ptr() + size_; }

      constexpr const_iterator end() const noexcept { return data_ptr() + size_; }

      constexpr reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }

      constexpr const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }

      constexpr reverse_iterator rend() noexcept { return reverse_iterator(begin()); }

      constexpr const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }

      constexpr const_iterator cbegin() const noexcept { return begin(); }

      constexpr const_iterator cend() const noexcept { return end(); }

      constexpr const_reverse_iterator crbegin() const noexcept { return rbegin(); }

      constexpr const_reverse_iterator crend() const noexcept { return rend(); }

      // Capacity
      constexpr bool empty() const noexcept { return size_ == 0; }

      constexpr size_type size() const noexcept { return size_; }

      static constexpr size_type max_size() noexcept { return N; }

      static constexpr size_type capacity() noexcept { return N; }

      constexpr void resize(size_type sz)
      {
         if (sz > N) GLZ_THROW_OR_ABORT(std::bad_alloc());

         if (sz > size_) {
            // Value-initialize the new elements
            std::uninitialized_value_construct_n(data_ptr() + size_, sz - size_);
            size_ = sz;
         }
         else if (sz < size_) {
            // Destroy excess elements
            destroy_range(sz, size_);
            size_ = sz;
         }
      }

      constexpr void resize(size_type sz, const T& c)
      {
         if (sz > N) GLZ_THROW_OR_ABORT(std::bad_alloc());

         if (sz > size_) {
            if constexpr (std::is_trivially_copyable_v<T>) {
               // For trivially copyable types, construct one and copy the rest
               if (size_ < sz) {
                  if (size_ == 0 && sz > 0) {
                     std::construct_at(data_ptr(), c);
                     for (size_type i = 1; i < sz; ++i) {
                        std::memcpy(data_ptr() + i, data_ptr(), sizeof(T));
                     }
                  }
                  else {
                     for (size_type i = size_; i < sz; ++i) {
                        std::memcpy(data_ptr() + i, &c, sizeof(T));
                     }
                  }
               }
            }
            else {
               // Expand and copy-construct new elements
               for (size_type i = size_; i < sz; ++i) std::construct_at(data_ptr() + i, c);
            }
            size_ = sz;
         }
         else if (sz < size_) {
            // Destroy excess elements
            destroy_range(sz, size_);
            size_ = sz;
         }
      }

      static constexpr void reserve(size_type n)
      {
         if (n > N) GLZ_THROW_OR_ABORT(std::bad_alloc());
         // No-op otherwise since capacity is fixed
      }

      static constexpr void shrink_to_fit() noexcept
      {
         // No-op for inplace_vector since capacity is fixed
      }

      // Element access
      constexpr reference operator[](size_type n) { return data_ptr()[n]; }

      constexpr const_reference operator[](size_type n) const { return data_ptr()[n]; }

      constexpr reference at(size_type n)
      {
         if (n >= size_) GLZ_THROW_OR_ABORT(std::out_of_range("inplace_vector::at: index out of range"));
         return data_ptr()[n];
      }

      constexpr const_reference at(size_type n) const
      {
         if (n >= size_) GLZ_THROW_OR_ABORT(std::out_of_range("inplace_vector::at: index out of range"));
         return data_ptr()[n];
      }

      constexpr reference front() { return data_ptr()[0]; }

      constexpr const_reference front() const { return data_ptr()[0]; }

      constexpr reference back() { return data_ptr()[size_ - 1]; }

      constexpr const_reference back() const { return data_ptr()[size_ - 1]; }

      constexpr T* data() noexcept { return data_ptr(); }

      constexpr const T* data() const noexcept { return data_ptr(); }

      // Standard modifiers
      template <class... Args>
      constexpr reference emplace_back(Args&&... args)
      {
         if (size_ >= N) GLZ_THROW_OR_ABORT(std::bad_alloc());

         std::construct_at(data_ptr() + size_, std::forward<Args>(args)...);
         return data_ptr()[size_++];
      }

      constexpr reference push_back(const T& x)
      {
         if (size_ >= N) GLZ_THROW_OR_ABORT(std::bad_alloc());

         if constexpr (std::is_trivially_copyable_v<T>) {
            std::memcpy(data_ptr() + size_, &x, sizeof(T));
         }
         else {
            std::construct_at(data_ptr() + size_, x);
         }
         return data_ptr()[size_++];
      }

      constexpr reference push_back(T&& x)
      {
         if (size_ >= N) GLZ_THROW_OR_ABORT(std::bad_alloc());

         std::construct_at(data_ptr() + size_, std::move(x));
         return data_ptr()[size_++];
      }

      template <std::ranges::input_range R>
         requires std::convertible_to<std::ranges::range_reference_t<R>, T>
      constexpr void append_range(R&& rg)
      {
         // Check size if possible
         if constexpr (std::ranges::sized_range<R>) {
            if (size_ + std::ranges::size(rg) > N) GLZ_THROW_OR_ABORT(std::bad_alloc());
         }

         for (auto&& item : rg) {
            if (size_ >= N) GLZ_THROW_OR_ABORT(std::bad_alloc());

            std::construct_at(data_ptr() + size_, std::forward<decltype(item)>(item));
            ++size_;
         }
      }

      constexpr void pop_back() { std::destroy_at(data_ptr() + --size_); }

      // Fallible APIs
      template <class... Args>
      constexpr T* try_emplace_back(Args&&... args)
      {
         if (size_ >= N) return nullptr;

         std::construct_at(data_ptr() + size_, std::forward<Args>(args)...);
         return data_ptr() + size_++;
      }

      constexpr T* try_push_back(const T& x)
      {
         if (size_ >= N) return nullptr;

         if constexpr (std::is_trivially_copyable_v<T>) {
            std::memcpy(data_ptr() + size_, &x, sizeof(T));
         }
         else {
            std::construct_at(data_ptr() + size_, x);
         }
         return data_ptr() + size_++;
      }

      constexpr T* try_push_back(T&& x)
      {
         if (size_ >= N) return nullptr;

         std::construct_at(data_ptr() + size_, std::move(x));
         return data_ptr() + size_++;
      }

      template <std::ranges::input_range R>
         requires std::convertible_to<std::ranges::range_reference_t<R>, T>
      constexpr auto try_append_range(R&& rg) -> std::ranges::borrowed_iterator_t<R>
      {
         auto it = std::ranges::begin(rg);
         const auto end = std::ranges::end(rg);
         for (; size_ != N && it != end; ++it) {
            try_emplace_back(*it);
         }
         return it;
      }

      // Unchecked APIs
      template <class... Args>
      constexpr reference unchecked_emplace_back(Args&&... args)
      {
         return *try_emplace_back(std::forward<Args>(args)...);
      }

      constexpr reference unchecked_push_back(const T& x) { return *try_push_back(x); }

      constexpr reference unchecked_push_back(T&& x) { return *try_push_back(std::move(x)); }

      // Insert operations
      template <class... Args>
      constexpr iterator emplace(const_iterator position, Args&&... args)
      {
         if (size_ >= N) GLZ_THROW_OR_ABORT(std::bad_alloc());

         // Calculate position index
         size_type pos_idx = position - cbegin();

         if (pos_idx == size_) {
            // Fast path: insert at end
            std::construct_at(data_ptr() + size_, std::forward<Args>(args)...);
            ++size_;
            return begin() + pos_idx;
         }

         // Move elements after position one position to the right
         if constexpr (std::is_trivially_copyable_v<T> && std::is_trivially_destructible_v<T>) {
            if (size_ > pos_idx) {
               std::memmove(data_ptr() + pos_idx + 1, data_ptr() + pos_idx, (size_ - pos_idx) * sizeof(T));
            }
         }
         else {
            // Construct a new element at the end
            std::construct_at(data_ptr() + size_, std::move(data_ptr()[size_ - 1]));

            // Move each element one position to the right, starting from the end
            for (size_type i = size_ - 1; i > pos_idx; --i) {
               data_ptr()[i] = std::move(data_ptr()[i - 1]);
            }

            // Destroy the element at position
            std::destroy_at(data_ptr() + pos_idx);
         }

         // Construct new element
         std::construct_at(data_ptr() + pos_idx, std::forward<Args>(args)...);
         ++size_;

         return begin() + pos_idx;
      }

      constexpr iterator insert(const_iterator position, const T& x) { return emplace(position, x); }

      constexpr iterator insert(const_iterator position, T&& x) { return emplace(position, std::move(x)); }

      constexpr iterator insert(const_iterator position, size_type n, const T& x)
      {
         if (size_ + n > N) GLZ_THROW_OR_ABORT(std::bad_alloc());

         if (n == 0) return begin() + (position - cbegin());

         // Calculate position index
         size_type pos_idx = position - cbegin();

         if constexpr (std::is_trivially_copyable_v<T> && std::is_trivially_destructible_v<T>) {
            // Move existing elements to make space
            if (pos_idx < size_) {
               std::memmove(data_ptr() + pos_idx + n, data_ptr() + pos_idx, (size_ - pos_idx) * sizeof(T));
            }

            // Copy the new elements
            for (size_type i = 0; i < n; ++i) {
               if constexpr (std::is_trivially_copyable_v<T>) {
                  std::memcpy(data_ptr() + pos_idx + i, &x, sizeof(T));
               }
               else {
                  std::construct_at(data_ptr() + pos_idx + i, x);
               }
            }
         }
         else {
            // General case for non-trivial types
            // First, move elements at the end to their new positions
            for (size_type i = 0; i < std::min(n, size_ - pos_idx); ++i) {
               std::construct_at(data_ptr() + size_ + n - 1 - i, std::move(data_ptr()[size_ - 1 - i]));
            }

            // Move the remaining elements that need to be shifted but not constructed
            for (size_type i = size_ - std::min(n, size_ - pos_idx) - 1; i >= pos_idx && i < size_; --i) {
               data_ptr()[i + n] = std::move(data_ptr()[i]);
            }

            // Destroy elements that will be overwritten
            for (size_type i = pos_idx; i < std::min(pos_idx + n, size_); ++i) {
               std::destroy_at(data_ptr() + i);
            }

            // Construct new elements
            for (size_type i = 0; i < n; ++i) {
               std::construct_at(data_ptr() + pos_idx + i, x);
            }
         }

         size_ += n;
         return begin() + pos_idx;
      }

      template <class InputIterator>
      constexpr iterator insert(const_iterator position, InputIterator first, InputIterator last)
      {
         // Calculate position index
         size_type pos_idx = position - cbegin();

         // For random access iterators, we can check size upfront
         if constexpr (std::random_access_iterator<InputIterator>) {
            auto count = std::distance(first, last);
            if (size_ + count > N) GLZ_THROW_OR_ABORT(std::bad_alloc());

            if (count == 0) return begin() + pos_idx;

            if constexpr (std::is_trivially_copyable_v<T> && std::contiguous_iterator<InputIterator> &&
                          std::is_trivially_destructible_v<T> &&
                          std::same_as<std::remove_cvref_t<std::iter_reference_t<InputIterator>>, T>) {
               // Move existing elements to make space
               if (pos_idx < size_) {
                  std::memmove(data_ptr() + pos_idx + count, data_ptr() + pos_idx, (size_ - pos_idx) * sizeof(T));
               }

               // Copy the new elements directly
               std::memcpy(data_ptr() + pos_idx, std::to_address(first), count * sizeof(T));
               size_ += count;
            }
            else {
               // General implementation
               // Make space first
               if (pos_idx < size_) {
                  // Move elements after position count positions to the right
                  for (size_type i = 0; i < std::min(count, size_ - pos_idx); ++i) {
                     std::construct_at(data_ptr() + size_ + count - 1 - i, std::move(data_ptr()[size_ - 1 - i]));
                  }

                  // Move the remaining elements
                  for (size_type i = size_ - std::min(count, size_ - pos_idx) - 1; i >= pos_idx && i < size_; --i) {
                     data_ptr()[i + count] = std::move(data_ptr()[i]);
                  }

                  // Destroy elements that will be overwritten
                  for (size_type i = pos_idx; i < std::min(pos_idx + count, size_); ++i) {
                     std::destroy_at(data_ptr() + i);
                  }
               }

               // Insert new elements
               for (size_type i = 0; i < count; ++i, ++first) {
                  std::construct_at(data_ptr() + pos_idx + i, *first);
               }

               size_ += count;
            }
         }
         else {
            // For non-random access iterators, copy to temporary buffer first
            inplace_vector<T, N> temp(first, last);

            if (size_ + temp.size() > N) GLZ_THROW_OR_ABORT(std::bad_alloc());

            insert(position, temp.begin(), temp.end());
         }

         return begin() + pos_idx;
      }

      template <std::ranges::input_range R>
         requires std::convertible_to<std::ranges::range_reference_t<R>, T>
      constexpr iterator insert_range(const_iterator position, R&& rg)
      {
         // Calculate position index
         size_type pos_idx = position - cbegin();

         if constexpr (std::ranges::sized_range<R>) {
            auto count = std::ranges::size(rg);
            if (size_ + count > N) GLZ_THROW_OR_ABORT(std::bad_alloc());

            if (count == 0) return begin() + pos_idx;

            if constexpr (std::is_trivially_copyable_v<T> && std::ranges::contiguous_range<R> &&
                          std::is_trivially_destructible_v<T> &&
                          std::same_as<std::remove_cvref_t<std::ranges::range_reference_t<R>>, T>) {
               // Move existing elements to make space
               if (pos_idx < size_) {
                  std::memmove(data_ptr() + pos_idx + count, data_ptr() + pos_idx, (size_ - pos_idx) * sizeof(T));
               }

               // Copy the new elements directly
               std::memcpy(data_ptr() + pos_idx, std::ranges::data(rg), count * sizeof(T));
               size_ += count;
            }
            else {
               // General implementation
               // Make space first
               if (pos_idx < size_) {
                  // Move elements after position count positions to the right
                  for (size_type i = 0; i < std::min(count, size_ - pos_idx); ++i) {
                     std::construct_at(data_ptr() + size_ + count - 1 - i, std::move(data_ptr()[size_ - 1 - i]));
                  }

                  // Move the remaining elements
                  for (size_type i = size_ - std::min(count, size_ - pos_idx) - 1; i >= pos_idx && i < size_; --i) {
                     data_ptr()[i + count] = std::move(data_ptr()[i]);
                  }

                  // Destroy elements that will be overwritten
                  for (size_type i = pos_idx; i < std::min(pos_idx + count, size_); ++i) {
                     std::destroy_at(data_ptr() + i);
                  }
               }

               // Insert new elements
               size_type i = 0;
               for (auto&& item : rg) {
                  std::construct_at(data_ptr() + pos_idx + i, std::forward<decltype(item)>(item));
                  ++i;
               }

               size_ += count;
            }
         }
         else {
            // For non-sized ranges, convert to temporary buffer first
#ifdef __cpp_lib_containers_ranges
            inplace_vector<T, N> temp(std::from_range, std::forward<R>(rg));
#else
            inplace_vector<T, N> temp;
            for (auto&& item : rg) {
               temp.push_back(std::forward<decltype(item)>(item));
            }
#endif

            if (size_ + temp.size() > N) GLZ_THROW_OR_ABORT(std::bad_alloc());

            insert(position, temp.begin(), temp.end());
         }

         return begin() + pos_idx;
      }

      constexpr iterator insert(const_iterator position, std::initializer_list<T> il)
      {
         return insert(position, il.begin(), il.end());
      }

      // Erase operations
      constexpr iterator erase(const_iterator position)
      {
         size_type pos_idx = position - cbegin();

         // Destroy element at position
         std::destroy_at(data_ptr() + pos_idx);

         if constexpr (std::is_trivially_copyable_v<T> && std::is_trivially_destructible_v<T>) {
            if (pos_idx < size_ - 1) {
               std::memmove(data_ptr() + pos_idx, data_ptr() + pos_idx + 1, (size_ - pos_idx - 1) * sizeof(T));
            }
         }
         else {
            // Move elements after position one position to the left
            for (size_type i = pos_idx; i < size_ - 1; ++i) data_ptr()[i] = std::move(data_ptr()[i + 1]);

            // Destroy the last element that was moved from
            std::destroy_at(data_ptr() + size_ - 1);
         }

         --size_;

         return begin() + pos_idx;
      }

      constexpr iterator erase(const_iterator first, const_iterator last)
      {
         size_type first_idx = first - cbegin();
         size_type last_idx = last - cbegin();
         size_type count = last_idx - first_idx;

         if (count == 0) return begin() + first_idx;

         // Destroy elements in range [first, last)
         for (size_type i = first_idx; i < last_idx; ++i) std::destroy_at(data_ptr() + i);

         if constexpr (std::is_trivially_copyable_v<T> && std::is_trivially_destructible_v<T>) {
            if (last_idx < size_) {
               std::memmove(data_ptr() + first_idx, data_ptr() + last_idx, (size_ - last_idx) * sizeof(T));
            }
         }
         else {
            // Move elements after last count positions to the left
            for (size_type i = first_idx; i < size_ - count; ++i) data_ptr()[i] = std::move(data_ptr()[i + count]);

            // Destroy the last elements that were moved from
            for (size_type i = size_ - count; i < size_; ++i) std::destroy_at(data_ptr() + i);
         }

         size_ -= count;

         return begin() + first_idx;
      }

      constexpr void clear() noexcept
      {
         destroy_range(0, size_);
         size_ = 0;
      }

      constexpr void swap(inplace_vector& x) noexcept(N == 0 || (std::is_nothrow_swappable_v<T> &&
                                                                 std::is_nothrow_move_constructible_v<T>))
      {
         if (this == &x) return;

         if constexpr (std::is_trivially_copyable_v<T>) {
            // For trivially copyable types, we can use a temporary buffer
            alignas(T) unsigned char temp[sizeof(T) * N];
            std::memcpy(temp, storage, size_ * sizeof(T));
            std::memcpy(storage, x.storage, x.size_ * sizeof(T));
            std::memcpy(x.storage, temp, size_ * sizeof(T));
            std::swap(size_, x.size_);
         }
         else {
            const auto min_size = std::min(size_, x.size_);

            // Swap common elements
            for (size_type i = 0; i < min_size; ++i) std::swap(data_ptr()[i], x.data_ptr()[i]);

            // Handle the case where sizes are different
            if (size_ > x.size_) {
               // Move our excess elements to x
               for (size_type i = min_size; i < size_; ++i)
                  std::construct_at(x.data_ptr() + i, std::move(data_ptr()[i]));

               // Destroy our excess elements
               for (size_type i = min_size; i < size_; ++i) std::destroy_at(data_ptr() + i);
            }
            else if (size_ < x.size_) {
               // Move x's excess elements to us
               for (size_type i = min_size; i < x.size_; ++i)
                  std::construct_at(data_ptr() + i, std::move(x.data_ptr()[i]));

               // Destroy x's excess elements
               for (size_type i = min_size; i < x.size_; ++i) std::destroy_at(x.data_ptr() + i);
            }

            // Swap sizes
            std::swap(size_, x.size_);
         }
      }

      // Comparison operators
      friend constexpr bool operator==(const inplace_vector& lhs, const inplace_vector& rhs)
      {
         if (lhs.size_ != rhs.size_) return false;

         if constexpr (std::is_trivially_copyable_v<T>) {
            return std::memcmp(lhs.storage, rhs.storage, lhs.size_ * sizeof(T)) == 0;
         }
         else {
            return std::equal(lhs.begin(), lhs.end(), rhs.begin());
         }
      }

      friend constexpr auto operator<=>(const inplace_vector& lhs, const inplace_vector& rhs)
      {
         return std::lexicographical_compare_three_way(lhs.begin(), lhs.end(), rhs.begin(), rhs.end(),
                                                       std::compare_three_way{});
      }
   };

   // Non-member functions
   template <class T, size_t N>
   constexpr void swap(inplace_vector<T, N>& x, inplace_vector<T, N>& y) noexcept(noexcept(x.swap(y)))
   {
      x.swap(y);
   }

   template <class T, size_t N, class U>
   constexpr typename inplace_vector<T, N>::size_type erase(inplace_vector<T, N>& c, const U& value)
   {
      auto it = std::remove(c.begin(), c.end(), value);
      auto r = c.end() - it;
      c.erase(it, c.end());
      return r;
   }

   template <class T, size_t N, class Predicate>
   constexpr typename inplace_vector<T, N>::size_type erase_if(inplace_vector<T, N>& c, Predicate pred)
   {
      auto it = std::remove_if(c.begin(), c.end(), pred);
      auto r = c.end() - it;
      c.erase(it, c.end());
      return r;
   }

   // Partial specialization for N = 0
   template <class T>
   class inplace_vector<T, 0>
   {
     public:
      // types
      using value_type = T;
      using pointer = T*;
      using const_pointer = const T*;
      using reference = value_type&;
      using const_reference = const value_type&;
      using size_type = size_t;
      using difference_type = std::ptrdiff_t;
      using iterator = T*;
      using const_iterator = const T*;
      using reverse_iterator = std::reverse_iterator<iterator>;
      using const_reverse_iterator = std::reverse_iterator<const_iterator>;

     public:
      // Constructors
      constexpr inplace_vector() noexcept = default;
      constexpr explicit inplace_vector(size_type n)
      {
         if (n > 0) GLZ_THROW_OR_ABORT(std::bad_alloc());
      }
      constexpr inplace_vector(size_type n, const T&)
      {
         if (n > 0) GLZ_THROW_OR_ABORT(std::bad_alloc());
      }

      template <class InputIterator>
      constexpr inplace_vector(InputIterator, InputIterator)
      {}

      constexpr inplace_vector(const inplace_vector&) = default;
      constexpr inplace_vector(inplace_vector&&) noexcept = default;
      constexpr inplace_vector(std::initializer_list<T> il)
      {
         if (il.size() > 0) GLZ_THROW_OR_ABORT(std::bad_alloc());
      }
      constexpr ~inplace_vector() = default;

      // Assignment operators
      constexpr inplace_vector& operator=(const inplace_vector&) = default;
      constexpr inplace_vector& operator=(inplace_vector&&) noexcept = default;
      constexpr inplace_vector& operator=(std::initializer_list<T> il)
      {
         if (il.size() > 0) GLZ_THROW_OR_ABORT(std::bad_alloc());
         return *this;
      }

      // Iterators - all return null pointers
      constexpr iterator begin() noexcept { return nullptr; }
      constexpr const_iterator begin() const noexcept { return nullptr; }
      constexpr iterator end() noexcept { return nullptr; }
      constexpr const_iterator end() const noexcept { return nullptr; }
      constexpr reverse_iterator rbegin() noexcept { return reverse_iterator(nullptr); }
      constexpr const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(nullptr); }
      constexpr reverse_iterator rend() noexcept { return reverse_iterator(nullptr); }
      constexpr const_reverse_iterator rend() const noexcept { return const_reverse_iterator(nullptr); }
      constexpr const_iterator cbegin() const noexcept { return nullptr; }
      constexpr const_iterator cend() const noexcept { return nullptr; }
      constexpr const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(nullptr); }
      constexpr const_reverse_iterator crend() const noexcept { return const_reverse_iterator(nullptr); }

      // Capacity - all hardcoded to 0
      constexpr bool empty() const noexcept { return true; }
      constexpr size_type size() const noexcept { return 0; } // Always returns 0 instead of size_ member
      static constexpr size_type max_size() noexcept { return 0; }
      static constexpr size_type capacity() noexcept { return 0; }
      constexpr void resize(size_type sz)
      {
         if (sz > 0) GLZ_THROW_OR_ABORT(std::bad_alloc());
      }
      constexpr void resize(size_type sz, const T&)
      {
         if (sz > 0) GLZ_THROW_OR_ABORT(std::bad_alloc());
      }
      static constexpr void reserve(size_type n)
      {
         if (n > 0) GLZ_THROW_OR_ABORT(std::bad_alloc());
      }
      static constexpr void shrink_to_fit() noexcept {}

      // Element access - all will throw
      constexpr reference operator[](size_type)
      {
         GLZ_THROW_OR_ABORT(std::out_of_range("inplace_vector: empty container"));
      }
      constexpr const_reference operator[](size_type) const
      {
         GLZ_THROW_OR_ABORT(std::out_of_range("inplace_vector: empty container"));
      }
      constexpr reference at(size_type) { GLZ_THROW_OR_ABORT(std::out_of_range("inplace_vector: empty container")); }
      constexpr const_reference at(size_type) const
      {
         GLZ_THROW_OR_ABORT(std::out_of_range("inplace_vector: empty container"));
      }
      constexpr reference front() { GLZ_THROW_OR_ABORT(std::out_of_range("inplace_vector: empty container")); }
      constexpr const_reference front() const
      {
         GLZ_THROW_OR_ABORT(std::out_of_range("inplace_vector: empty container"));
      }
      constexpr reference back() { GLZ_THROW_OR_ABORT(std::out_of_range("inplace_vector: empty container")); }
      constexpr const_reference back() const
      {
         GLZ_THROW_OR_ABORT(std::out_of_range("inplace_vector: empty container"));
      }
      constexpr T* data() noexcept { return nullptr; }
      constexpr const T* data() const noexcept { return nullptr; }

      // Modifiers - all throw on attempt to add elements
      template <class... Args>
      constexpr reference emplace_back(Args&&...)
      {
         GLZ_THROW_OR_ABORT(std::bad_alloc());
      }
      constexpr reference push_back(const T&) { GLZ_THROW_OR_ABORT(std::bad_alloc()); }
      constexpr reference push_back(T&&) { GLZ_THROW_OR_ABORT(std::bad_alloc()); }
      constexpr void pop_back() { GLZ_THROW_OR_ABORT(std::out_of_range("inplace_vector: empty container")); }

      // Fallible APIs
      template <class... Args>
      constexpr T* try_emplace_back(Args&&...)
      {
         return nullptr;
      }
      constexpr T* try_push_back(const T&) { return nullptr; }
      constexpr T* try_push_back(T&&) { return nullptr; }

      // Unchecked APIs
      template <class... Args>
      constexpr reference unchecked_emplace_back(Args&&...)
      {
         GLZ_THROW_OR_ABORT(std::bad_alloc());
      }
      constexpr reference unchecked_push_back(const T&) { GLZ_THROW_OR_ABORT(std::bad_alloc()); }
      constexpr reference unchecked_push_back(T&&) { GLZ_THROW_OR_ABORT(std::bad_alloc()); }

      // Insert operations
      template <class... Args>
      constexpr iterator emplace(const_iterator, Args&&...)
      {
         GLZ_THROW_OR_ABORT(std::bad_alloc());
      }
      constexpr iterator insert(const_iterator, const T&) { GLZ_THROW_OR_ABORT(std::bad_alloc()); }
      constexpr iterator insert(const_iterator, T&&) { GLZ_THROW_OR_ABORT(std::bad_alloc()); }
      constexpr iterator insert(const_iterator, size_type, const T&) { GLZ_THROW_OR_ABORT(std::bad_alloc()); }
      template <class InputIterator>
      constexpr iterator insert(const_iterator, InputIterator, InputIterator)
      {
         GLZ_THROW_OR_ABORT(std::bad_alloc());
      }
      constexpr iterator insert(const_iterator, std::initializer_list<T>) { GLZ_THROW_OR_ABORT(std::bad_alloc()); }

      // Erase operations
      constexpr iterator erase(const_iterator)
      {
         GLZ_THROW_OR_ABORT(std::out_of_range("inplace_vector: empty container"));
      }
      constexpr iterator erase(const_iterator, const_iterator) { return nullptr; }
      constexpr void clear() noexcept {}
      constexpr void swap(inplace_vector&) noexcept {}

      // Comparison operators
      friend constexpr bool operator==(const inplace_vector&, const inplace_vector&) { return true; }
      friend constexpr auto operator<=>(const inplace_vector&, const inplace_vector&)
      {
         return std::strong_ordering::equal;
      }
   };
}
