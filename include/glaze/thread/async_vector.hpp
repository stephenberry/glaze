#pragma once

#include <algorithm>
#include <cassert>
#include <iterator>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <utility>
#include <vector>

#ifndef GLZ_THROW_OR_ABORT
#if __cpp_exceptions
#define GLZ_THROW_OR_ABORT(EXC) (throw(EXC))
#define GLZ_NOEXCEPT noexcept(false)
#else
#define GLZ_THROW_OR_ABORT(EXC) (std::abort())
#define GLZ_NOEXCEPT noexcept(true)
#endif
#endif

// Provides a thread-safe vector
// This async_vector copies its state when the vector is copied
// It ensures thread safety during read and write operations

namespace glz
{
   template <class V>
   struct async_vector
   {
   private:
      std::vector<std::unique_ptr<V>> items;
      mutable std::shared_mutex mutex;
      
   public:
      using value_type = V;
      using size_type = std::size_t;
      using difference_type = std::ptrdiff_t;
      using reference = V&;
      using const_reference = const V&;
      using pointer = V*;
      using const_pointer = const V*;
      
      // Constructors
      async_vector() = default;
      
      // Copy constructor - performs deep copy
      async_vector(const async_vector& other)
      {
         std::shared_lock lock(other.mutex);
         items.reserve(other.items.size());
         for (const auto& item : other.items) {
            items.push_back(std::make_unique<V>(*item));
         }
      }
      
      // Move constructor
      async_vector(async_vector&& other) noexcept
      {
         std::unique_lock lock(other.mutex);
         items = std::move(other.items);
      }
      
      // Copy assignment - performs deep copy
      async_vector& operator=(const async_vector& other)
      {
         if (this != &other) {
            std::unique_lock lock1(mutex, std::defer_lock);
            std::shared_lock lock2(other.mutex, std::defer_lock);
            std::lock(lock1, lock2);
            
            items.clear();
            items.reserve(other.items.size());
            for (const auto& item : other.items) {
               items.push_back(std::make_unique<V>(*item));
            }
         }
         return *this;
      }
      
      // Move assignment
      async_vector& operator=(async_vector&& other) noexcept
      {
         if (this != &other) {
            std::unique_lock lock1(mutex, std::defer_lock);
            std::unique_lock lock2(other.mutex, std::defer_lock);
            std::lock(lock1, lock2);
            
            items = std::move(other.items);
         }
         return *this;
      }
      
      // Forward declaration of iterator classes
      class iterator;
      class const_iterator;
      
      // Iterator Class Definition
      class iterator
      {
      public:
         using iterator_category = std::random_access_iterator_tag;
         using value_type = V;
         using difference_type = std::ptrdiff_t;
         using pointer = V*;
         using reference = V&;
         
      private:
         typename std::vector<std::unique_ptr<V>>::iterator item_it;
         async_vector* parent;
         std::shared_ptr<std::shared_lock<std::shared_mutex>> shared_lock_ptr;
         std::shared_ptr<std::unique_lock<std::shared_mutex>> unique_lock_ptr;
         
      public:
         iterator(typename std::vector<std::unique_ptr<V>>::iterator item_it, async_vector* parent,
                  std::shared_ptr<std::shared_lock<std::shared_mutex>> existing_shared_lock = nullptr,
                  std::shared_ptr<std::unique_lock<std::shared_mutex>> existing_unique_lock = nullptr)
         : item_it(item_it),
         parent(parent),
         shared_lock_ptr(existing_shared_lock),
         unique_lock_ptr(existing_unique_lock)
         {
            // Acquire a shared lock only if no lock is provided
            if (!shared_lock_ptr && !unique_lock_ptr) {
               shared_lock_ptr = std::make_shared<std::shared_lock<std::shared_mutex>>(parent->mutex);
            }
         }
         
         // Copy Constructor
         iterator(const iterator& other)
         : item_it(other.item_it),
         parent(other.parent),
         shared_lock_ptr(other.shared_lock_ptr),
         unique_lock_ptr(other.unique_lock_ptr)
         {}
         
         // Move Constructor
         iterator(iterator&& other) noexcept
         : item_it(std::move(other.item_it)),
         parent(other.parent),
         shared_lock_ptr(std::move(other.shared_lock_ptr)),
         unique_lock_ptr(std::move(other.unique_lock_ptr))
         {}
         
         // Copy Assignment
         iterator& operator=(const iterator& other)
         {
            if (this != &other) {
               item_it = other.item_it;
               parent = other.parent;
               shared_lock_ptr = other.shared_lock_ptr;
               unique_lock_ptr = other.unique_lock_ptr;
            }
            return *this;
         }
         
         friend struct async_vector<V>;
         
         // Dereference operators
         reference operator*() const { return *(*item_it); }
         pointer operator->() const { return (*item_it).get(); }
         
         // Increment and Decrement operators
         iterator& operator++()
         {
            ++item_it;
            return *this;
         }
         
         iterator operator++(int)
         {
            iterator tmp(*this);
            ++(*this);
            return tmp;
         }
         
         iterator& operator--()
         {
            --item_it;
            return *this;
         }
         
         iterator operator--(int)
         {
            iterator tmp(*this);
            --(*this);
            return tmp;
         }
         
         // Arithmetic operators
         iterator operator+(difference_type n) const
         {
            return iterator(item_it + n, parent, shared_lock_ptr, unique_lock_ptr);
         }
         
         iterator& operator+=(difference_type n)
         {
            item_it += n;
            return *this;
         }
         
         iterator operator-(difference_type n) const
         {
            return iterator(item_it - n, parent, shared_lock_ptr, unique_lock_ptr);
         }
         
         iterator& operator-=(difference_type n)
         {
            item_it -= n;
            return *this;
         }
         
         difference_type operator-(const iterator& other) const { return item_it - other.item_it; }
         
         // Comparison operators
         bool operator==(const iterator& other) const noexcept { return item_it == other.item_it; }
         bool operator!=(const iterator& other) const noexcept { return item_it != other.item_it; }
         bool operator<(const iterator& other) const noexcept { return item_it < other.item_it; }
         bool operator>(const iterator& other) const noexcept { return item_it > other.item_it; }
         bool operator<=(const iterator& other) const noexcept { return item_it <= other.item_it; }
         bool operator>=(const iterator& other) const noexcept { return item_it >= other.item_it; }
         
         // Access operator
         reference operator[](difference_type n) const { return *(*(item_it + n)); }
      };
      
      // Const Iterator Class Definition
      class const_iterator
      {
      public:
         using iterator_category = std::random_access_iterator_tag;
         using value_type = V;
         using difference_type = std::ptrdiff_t;
         using pointer = const V*;
         using reference = const V&;
         
      private:
         typename std::vector<std::unique_ptr<V>>::const_iterator item_it;
         const async_vector* parent;
         std::shared_ptr<std::shared_lock<std::shared_mutex>> shared_lock_ptr;
         
      public:
         const_iterator(typename std::vector<std::unique_ptr<V>>::const_iterator item_it,
                        const async_vector* parent,
                        std::shared_ptr<std::shared_lock<std::shared_mutex>> existing_shared_lock = nullptr)
         : item_it(item_it), parent(parent), shared_lock_ptr(existing_shared_lock)
         {
            // Acquire a shared lock only if no lock is provided
            if (!shared_lock_ptr) {
               shared_lock_ptr = std::make_shared<std::shared_lock<std::shared_mutex>>(parent->mutex);
            }
         }
         
         // Copy Constructor
         const_iterator(const const_iterator& other)
         : item_it(other.item_it), parent(other.parent), shared_lock_ptr(other.shared_lock_ptr)
         {}
         
         // Move Constructor
         const_iterator(const_iterator&& other) noexcept
         : item_it(std::move(other.item_it)),
         parent(other.parent),
         shared_lock_ptr(std::move(other.shared_lock_ptr))
         {}
         
         // Copy Assignment
         const_iterator& operator=(const const_iterator& other)
         {
            if (this != &other) {
               item_it = other.item_it;
               parent = other.parent;
               shared_lock_ptr = other.shared_lock_ptr;
            }
            return *this;
         }
         
         friend struct async_vector<V>;
         
         // Dereference operators
         reference operator*() const { return *(*item_it); }
         pointer operator->() const { return (*item_it).get(); }
         
         // Increment and Decrement operators
         const_iterator& operator++()
         {
            ++item_it;
            return *this;
         }
         
         const_iterator operator++(int)
         {
            const_iterator tmp(*this);
            ++(*this);
            return tmp;
         }
         
         const_iterator& operator--()
         {
            --item_it;
            return *this;
         }
         
         const_iterator operator--(int)
         {
            const_iterator tmp(*this);
            --(*this);
            return tmp;
         }
         
         // Arithmetic operators
         const_iterator operator+(difference_type n) const
         {
            return const_iterator(item_it + n, parent, shared_lock_ptr);
         }
         
         const_iterator& operator+=(difference_type n)
         {
            item_it += n;
            return *this;
         }
         
         const_iterator operator-(difference_type n) const
         {
            return const_iterator(item_it - n, parent, shared_lock_ptr);
         }
         
         const_iterator& operator-=(difference_type n)
         {
            item_it -= n;
            return *this;
         }
         
         difference_type operator-(const const_iterator& other) const { return item_it - other.item_it; }
         
         // Comparison operators
         bool operator==(const const_iterator& other) const noexcept { return item_it == other.item_it; }
         bool operator!=(const const_iterator& other) const noexcept { return item_it != other.item_it; }
         bool operator<(const const_iterator& other) const noexcept { return item_it < other.item_it; }
         bool operator>(const const_iterator& other) const noexcept { return item_it > other.item_it; }
         bool operator<=(const const_iterator& other) const noexcept { return item_it <= other.item_it; }
         bool operator>=(const const_iterator& other) const noexcept { return item_it >= other.item_it; }
         
         // Access operator
         reference operator[](difference_type n) const { return *(*(item_it + n)); }
      };
      
      // Value Proxy Class Definition
      class value_proxy
      {
      private:
         V& value_ref;
         std::shared_ptr<std::shared_lock<std::shared_mutex>> shared_lock_ptr;
         std::shared_ptr<std::unique_lock<std::shared_mutex>> unique_lock_ptr;
         
      public:
         value_proxy(V& value_ref, std::shared_ptr<std::shared_lock<std::shared_mutex>> existing_shared_lock = nullptr,
                     std::shared_ptr<std::unique_lock<std::shared_mutex>> existing_unique_lock = nullptr)
         : value_ref(value_ref), shared_lock_ptr(existing_shared_lock), unique_lock_ptr(existing_unique_lock)
         {
            // Ensure that a lock is provided
            assert(shared_lock_ptr || unique_lock_ptr);
         }
         
         static constexpr bool glaze_value_proxy = true;
         
         // Disable Copy and Move
         value_proxy(const value_proxy&) = delete;
         value_proxy& operator=(const value_proxy&) = delete;
         value_proxy(value_proxy&&) = delete;
         value_proxy& operator=(value_proxy&&) = delete;
         
         // Access the value
         V& value() { return value_ref; }
         
         const V& value() const { return value_ref; }
         
         // Arrow Operator
         V* operator->() { return &value_ref; }
         
         const V* operator->() const { return &value_ref; }
         
         V& operator*() { return value_ref; }
         
         const V& operator*() const { return value_ref; }
         
         // Implicit Conversion to V&
         operator V&() { return value_ref; }
         
         operator const V&() const { return value_ref; }
         
         template <class T>
         value_proxy& operator=(const T& other)
         {
            value_ref = other;
            return *this;
         }
         
         bool operator==(const V& other) const { return value() == other; }
         
         bool operator==(const value_proxy& other) const {
            return value() == other.value();
         }
      };
      
      // Const Value Proxy Class Definition
      class const_value_proxy
      {
      private:
         const V& value_ref;
         std::shared_ptr<std::shared_lock<std::shared_mutex>> shared_lock_ptr;
         
      public:
         const_value_proxy(const V& value_ref,
                           std::shared_ptr<std::shared_lock<std::shared_mutex>> existing_shared_lock)
         : value_ref(value_ref), shared_lock_ptr(existing_shared_lock)
         {
            // Ensure that a lock is provided
            assert(shared_lock_ptr);
         }
         
         // Disable Copy and Move
         const_value_proxy(const const_value_proxy&) = delete;
         const_value_proxy& operator=(const const_value_proxy&) = delete;
         const_value_proxy(const_value_proxy&&) = delete;
         const_value_proxy& operator=(const const_value_proxy&&) = delete;
         
         // Access the value
         const V& value() const { return value_ref; }
         
         // Arrow Operator
         const V* operator->() const { return &value_ref; }
         
         const V& operator*() const { return value_ref; }
         
         // Implicit Conversion to const V&
         operator const V&() const { return value_ref; }
         
         bool operator==(const V& other) const { return value() == other; }
         
         bool operator==(const const_value_proxy& other) const {
            return value() == other.value();
         }
      };
      
      // Element Access Methods
      value_proxy operator[](size_type pos)
      {
         auto shared_lock_ptr = std::make_shared<std::shared_lock<std::shared_mutex>>(mutex);
         return value_proxy(*items[pos], shared_lock_ptr);
      }
      
      const_value_proxy operator[](size_type pos) const
      {
         auto shared_lock_ptr = std::make_shared<std::shared_lock<std::shared_mutex>>(mutex);
         return const_value_proxy(*items[pos], shared_lock_ptr);
      }
      
      value_proxy at(size_type pos)
      {
         auto shared_lock_ptr = std::make_shared<std::shared_lock<std::shared_mutex>>(mutex);
         if (pos >= items.size()) {
            GLZ_THROW_OR_ABORT(std::out_of_range("Index out of range"));
         }
         return value_proxy(*items[pos], shared_lock_ptr);
      }
      
      const_value_proxy at(size_type pos) const
      {
         auto shared_lock_ptr = std::make_shared<std::shared_lock<std::shared_mutex>>(mutex);
         if (pos >= items.size()) {
            GLZ_THROW_OR_ABORT(std::out_of_range("Index out of range"));
         }
         return const_value_proxy(*items[pos], shared_lock_ptr);
      }
      
      value_proxy front()
      {
         auto shared_lock_ptr = std::make_shared<std::shared_lock<std::shared_mutex>>(mutex);
         return value_proxy(*items.front(), shared_lock_ptr);
      }
      
      const_value_proxy front() const
      {
         auto shared_lock_ptr = std::make_shared<std::shared_lock<std::shared_mutex>>(mutex);
         return const_value_proxy(*items.front(), shared_lock_ptr);
      }
      
      value_proxy back()
      {
         auto shared_lock_ptr = std::make_shared<std::shared_lock<std::shared_mutex>>(mutex);
         return value_proxy(*items.back(), shared_lock_ptr);
      }
      
      const_value_proxy back() const
      {
         auto shared_lock_ptr = std::make_shared<std::shared_lock<std::shared_mutex>>(mutex);
         return const_value_proxy(*items.back(), shared_lock_ptr);
      }
      
      // Capacity Methods
      bool empty() const
      {
         std::shared_lock lock(mutex);
         return items.empty();
      }
      
      size_type size() const
      {
         std::shared_lock lock(mutex);
         return items.size();
      }
      
      size_type max_size() const
      {
         std::shared_lock lock(mutex);
         return items.max_size();
      }
      
      void reserve(size_type new_cap)
      {
         std::unique_lock lock(mutex);
         items.reserve(new_cap);
      }
      
      size_type capacity() const
      {
         std::shared_lock lock(mutex);
         return items.capacity();
      }
      
      void shrink_to_fit()
      {
         std::unique_lock lock(mutex);
         items.shrink_to_fit();
      }
      
      // Modifiers
      void clear()
      {
         std::unique_lock lock(mutex);
         items.clear();
      }
      
      void push_back(const V& value)
      {
         std::unique_lock lock(mutex);
         items.push_back(std::make_unique<V>(value));
      }
      
      template <class... Args>
      void emplace_back(Args&&... args)
      {
         std::unique_lock lock(mutex);
         items.emplace_back(std::make_unique<V>(std::forward<Args>(args)...));
      }
      
      void pop_back()
      {
         std::unique_lock lock(mutex);
         items.pop_back();
      }
      
      iterator insert(const_iterator pos, const V& value)
      {
         // Use the lock from the iterator instead of creating a new one
         std::unique_lock<std::shared_mutex> lock(mutex, std::defer_lock);
         
         // Only lock if the iterator doesn't already have a lock
         if (!pos.shared_lock_ptr || !pos.shared_lock_ptr->owns_lock()) {
            lock.lock();
         }
         
         // Create a unique_ptr for the new value
         auto new_item = std::make_unique<V>(value);
         
         // Calculate the index from the iterator position
         auto index = std::distance(items.cbegin(), pos.item_it);
         
         // Insert the item
         auto it = items.insert(items.begin() + index, std::move(new_item));
         
         // Create a new shared_ptr for the unique lock if we created one
         std::shared_ptr<std::unique_lock<std::shared_mutex>> unique_lock_ptr = nullptr;
         if (lock.owns_lock()) {
            unique_lock_ptr = std::make_shared<std::unique_lock<std::shared_mutex>>(std::move(lock));
         }
         
         return iterator(it, this, pos.shared_lock_ptr, unique_lock_ptr);
      }
      
      // Modified insert method for rvalue references
      iterator insert(const_iterator pos, V&& value)
      {
         // Use the lock from the iterator instead of creating a new one
         std::unique_lock<std::shared_mutex> lock(mutex, std::defer_lock);
         
         // Only lock if the iterator doesn't already have a lock
         if (!pos.shared_lock_ptr || !pos.shared_lock_ptr->owns_lock()) {
            lock.lock();
         }
         
         // Create a unique_ptr for the new value
         auto new_item = std::make_unique<V>(std::move(value));
         
         // Calculate the index from the iterator position
         auto index = std::distance(items.cbegin(), pos.item_it);
         
         // Insert the item
         auto it = items.insert(items.begin() + index, std::move(new_item));
         
         // Create a new shared_ptr for the unique lock if we created one
         std::shared_ptr<std::unique_lock<std::shared_mutex>> unique_lock_ptr = nullptr;
         if (lock.owns_lock()) {
            unique_lock_ptr = std::make_shared<std::unique_lock<std::shared_mutex>>(std::move(lock));
         }
         
         return iterator(it, this, pos.shared_lock_ptr, unique_lock_ptr);
      }
      
      // Modified emplace method
      template <class... Args>
      iterator emplace(const_iterator pos, Args&&... args)
      {
         // Use the lock from the iterator instead of creating a new one
         std::unique_lock<std::shared_mutex> lock(mutex, std::defer_lock);
         
         // Only lock if the iterator doesn't already have a lock
         if (!pos.shared_lock_ptr || !pos.shared_lock_ptr->owns_lock()) {
            lock.lock();
         }
         
         // Create a unique_ptr for the new value
         auto new_item = std::make_unique<V>(std::forward<Args>(args)...);
         
         // Calculate the index from the iterator position
         auto index = std::distance(items.cbegin(), pos.item_it);
         
         // Insert the item
         auto it = items.insert(items.begin() + index, std::move(new_item));
         
         // Create a new shared_ptr for the unique lock if we created one
         std::shared_ptr<std::unique_lock<std::shared_mutex>> unique_lock_ptr = nullptr;
         if (lock.owns_lock()) {
            unique_lock_ptr = std::make_shared<std::unique_lock<std::shared_mutex>>(std::move(lock));
         }
         
         return iterator(it, this, pos.shared_lock_ptr, unique_lock_ptr);
      }
      
      // Similar fixes should be applied to erase methods
      iterator erase(const_iterator pos)
      {
         // Use the lock from the iterator instead of creating a new one
         std::unique_lock<std::shared_mutex> lock(mutex, std::defer_lock);
         
         // Only lock if the iterator doesn't already have a lock
         if (!pos.shared_lock_ptr || !pos.shared_lock_ptr->owns_lock()) {
            lock.lock();
         }
         
         // Calculate the index from the iterator position
         auto index = std::distance(items.cbegin(), pos.item_it);
         
         // Erase the item
         auto it = items.erase(items.begin() + index);
         
         // Create a new shared_ptr for the unique lock if we created one
         std::shared_ptr<std::unique_lock<std::shared_mutex>> unique_lock_ptr = nullptr;
         if (lock.owns_lock()) {
            unique_lock_ptr = std::make_shared<std::unique_lock<std::shared_mutex>>(std::move(lock));
         }
         
         return iterator(it, this, pos.shared_lock_ptr, unique_lock_ptr);
      }
      
      iterator erase(const_iterator first, const_iterator last)
      {
         // Use the lock from the iterator instead of creating a new one
         std::unique_lock<std::shared_mutex> lock(mutex, std::defer_lock);
         
         // Only lock if neither iterator has a lock
         if ((!first.shared_lock_ptr || !first.shared_lock_ptr->owns_lock()) &&
             (!last.shared_lock_ptr || !last.shared_lock_ptr->owns_lock())) {
            lock.lock();
         }
         
         // Calculate indices from iterator positions
         auto first_index = std::distance(items.cbegin(), first.item_it);
         auto last_index = std::distance(items.cbegin(), last.item_it);
         
         // Erase the items
         auto it = items.erase(items.begin() + first_index, items.begin() + last_index);
         
         // Create a new shared_ptr for the unique lock if we created one
         std::shared_ptr<std::unique_lock<std::shared_mutex>> unique_lock_ptr = nullptr;
         if (lock.owns_lock()) {
            unique_lock_ptr = std::make_shared<std::unique_lock<std::shared_mutex>>(std::move(lock));
         }
         
         // Use either iterator's lock if available
         std::shared_ptr<std::shared_lock<std::shared_mutex>> shared_lock_ptr =
         first.shared_lock_ptr ? first.shared_lock_ptr : last.shared_lock_ptr;
         
         return iterator(it, this, shared_lock_ptr, unique_lock_ptr);
      }
      
      void resize(size_type count)
      {
         std::unique_lock lock(mutex);
         if (count < items.size()) {
            items.resize(count);
         }
         else {
            while (items.size() < count) {
               items.emplace_back(std::make_unique<V>());
            }
         }
      }
      
      void resize(size_type count, const V& value)
      {
         std::unique_lock lock(mutex);
         if (count < items.size()) {
            items.resize(count);
         }
         else {
            while (items.size() < count) {
               items.emplace_back(std::make_unique<V>(value));
            }
         }
      }
      
      void swap(async_vector& other)
      {
         if (this == &other) return;
         
         std::unique_lock<std::shared_mutex> lock1(mutex, std::defer_lock);
         std::unique_lock<std::shared_mutex> lock2(other.mutex, std::defer_lock);
         std::lock(lock1, lock2);
         items.swap(other.items);
      }
      
      // Iterators
      iterator begin()
      {
         auto shared_lock_ptr = std::make_shared<std::shared_lock<std::shared_mutex>>(mutex);
         return iterator(items.begin(), this, shared_lock_ptr);
      }
      
      const_iterator begin() const
      {
         auto shared_lock_ptr = std::make_shared<std::shared_lock<std::shared_mutex>>(mutex);
         return const_iterator(items.cbegin(), this, shared_lock_ptr);
      }
      
      const_iterator cbegin() const
      {
         auto shared_lock_ptr = std::make_shared<std::shared_lock<std::shared_mutex>>(mutex);
         return const_iterator(items.cbegin(), this, shared_lock_ptr);
      }
      
      iterator end()
      {
         auto shared_lock_ptr = std::make_shared<std::shared_lock<std::shared_mutex>>(mutex);
         return iterator(items.end(), this, shared_lock_ptr);
      }
      
      const_iterator end() const
      {
         auto shared_lock_ptr = std::make_shared<std::shared_lock<std::shared_mutex>>(mutex);
         return const_iterator(items.cend(), this, shared_lock_ptr);
      }
      
      const_iterator cend() const
      {
         auto shared_lock_ptr = std::make_shared<std::shared_lock<std::shared_mutex>>(mutex);
         return const_iterator(items.cend(), this, shared_lock_ptr);
      }
   };
}
