#pragma once

#include <algorithm>
#include <cassert>
#include <iterator>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <utility>
#include <vector>
#include <type_traits>
#include <thread>
#include <unordered_map>

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
      
      // Custom mutex deleter that unlocks the mutex when the shared_ptr is destroyed
      class mutex_deleter {
      private:
         bool is_unique{};
         
      public:
         explicit mutex_deleter(bool is_unique) : is_unique(is_unique) {}
         
         void operator()(std::shared_mutex* m) {
            // Unlock based on lock type
            if (is_unique) {
               m->unlock();
            } else {
               m->unlock_shared();
            }
         }
      };
      
      // Thread-local storage for tracking the current lock
      struct thread_lock_info {
         std::shared_ptr<std::shared_mutex> mutex_ptr;
         bool is_unique = false;
      };
      
      thread_lock_info& get_thread_lock_info() const {
         thread_local thread_lock_info info;
         return info;
      }
      
      // Acquire a unique lock on the mutex
      std::shared_ptr<std::shared_mutex> acquire_unique_lock() const {
         auto& info = get_thread_lock_info();
         
         // If we already have a unique lock, reuse it
         if (info.mutex_ptr && info.is_unique) {
            return info.mutex_ptr;
         }
         
         // Reset any existing lock (shared or unique)
         info.mutex_ptr.reset();
         
         // Lock the mutex
         mutex.lock();
         
         // Create a shared_ptr with custom deleter
         info.mutex_ptr = std::shared_ptr<std::shared_mutex>(&mutex, mutex_deleter(true));
         info.is_unique = true;
         
         return info.mutex_ptr;
      }
      
      // Acquire a shared lock on the mutex
      std::shared_ptr<std::shared_mutex> acquire_shared_lock() const {
         auto& info = get_thread_lock_info();
         
         // If we already have any lock, reuse it
         if (info.mutex_ptr) {
            return info.mutex_ptr;
         }
         
         // Lock the mutex for shared access
         mutex.lock_shared();
         
         // Create a shared_ptr with custom deleter
         info.mutex_ptr = std::shared_ptr<std::shared_mutex>(&mutex, mutex_deleter(false));
         info.is_unique = false;
         
         return info.mutex_ptr;
      }
      
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
         auto other_lock = other.acquire_shared_lock();
         auto this_lock = acquire_unique_lock();
         
         items.reserve(other.items.size());
         for (const auto& item : other.items) {
            items.push_back(std::make_unique<V>(*item));
         }
      }
      
      // Move constructor
      async_vector(async_vector&& other) noexcept
      {
         auto other_lock = other.acquire_unique_lock();
         auto this_lock = acquire_unique_lock();
         
         items = std::move(other.items);
      }
      
      // Copy assignment - performs deep copy
      async_vector& operator=(const async_vector& other)
      {
         if (this != &other) {
            auto other_lock = other.acquire_shared_lock();
            auto this_lock = acquire_unique_lock();
            
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
            auto other_lock = other.acquire_unique_lock();
            auto this_lock = acquire_unique_lock();
            
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
         std::shared_ptr<std::shared_mutex> lock_ptr;
         
      public:
         iterator(typename std::vector<std::unique_ptr<V>>::iterator item_it, async_vector* parent)
         : item_it(item_it), parent(parent)
         {
            // Acquire a lock for the iterator
            lock_ptr = parent->acquire_unique_lock();
         }
         
         // Copy Constructor
         iterator(const iterator& other)
         : item_it(other.item_it),
         parent(other.parent),
         lock_ptr(other.lock_ptr)
         {}
         
         // Move Constructor
         iterator(iterator&& other) noexcept
         : item_it(std::move(other.item_it)),
         parent(other.parent),
         lock_ptr(std::move(other.lock_ptr))
         {}
         
         // Copy Assignment
         iterator& operator=(const iterator& other)
         {
            if (this != &other) {
               item_it = other.item_it;
               parent = other.parent;
               lock_ptr = other.lock_ptr;
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
            iterator result = *this;
            result.item_it += n;
            return result;
         }
         
         iterator& operator+=(difference_type n)
         {
            item_it += n;
            return *this;
         }
         
         iterator operator-(difference_type n) const
         {
            iterator result = *this;
            result.item_it -= n;
            return result;
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
         std::shared_ptr<std::shared_mutex> lock_ptr;
         
      public:
         const_iterator(typename std::vector<std::unique_ptr<V>>::const_iterator item_it,
                        const async_vector* parent)
         : item_it(item_it), parent(parent)
         {
            // Acquire a lock for the iterator
            lock_ptr = parent->acquire_shared_lock();
         }
         
         // Copy Constructor
         const_iterator(const const_iterator& other)
         : item_it(other.item_it),
         parent(other.parent),
         lock_ptr(other.lock_ptr)
         {}
         
         // Move Constructor
         const_iterator(const_iterator&& other) noexcept
         : item_it(std::move(other.item_it)),
         parent(other.parent),
         lock_ptr(std::move(other.lock_ptr))
         {}
         
         // Copy Assignment
         const_iterator& operator=(const const_iterator& other)
         {
            if (this != &other) {
               item_it = other.item_it;
               parent = other.parent;
               lock_ptr = other.lock_ptr;
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
            const_iterator result = *this;
            result.item_it += n;
            return result;
         }
         
         const_iterator& operator+=(difference_type n)
         {
            item_it += n;
            return *this;
         }
         
         const_iterator operator-(difference_type n) const
         {
            const_iterator result = *this;
            result.item_it -= n;
            return result;
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
         std::shared_ptr<std::shared_mutex> lock_ptr;
         
      public:
         value_proxy(V& value_ref, async_vector* parent)
         : value_ref(value_ref)
         {
            // Acquire a lock for the proxy
            lock_ptr = parent->acquire_shared_lock();
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
         std::shared_ptr<std::shared_mutex> lock_ptr;
         
      public:
         const_value_proxy(const V& value_ref, const async_vector* parent)
         : value_ref(value_ref)
         {
            // Acquire a lock for the proxy
            lock_ptr = parent->acquire_shared_lock();
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
         auto lock = acquire_shared_lock();
         return value_proxy(*items[pos], this);
      }
      
      const_value_proxy operator[](size_type pos) const
      {
         auto lock = acquire_shared_lock();
         return const_value_proxy(*items[pos], this);
      }
      
      value_proxy at(size_type pos)
      {
         auto lock = acquire_shared_lock();
         if (pos >= items.size()) {
            GLZ_THROW_OR_ABORT(std::out_of_range("Index out of range"));
         }
         return value_proxy(*items[pos], this);
      }
      
      const_value_proxy at(size_type pos) const
      {
         auto lock = acquire_shared_lock();
         if (pos >= items.size()) {
            GLZ_THROW_OR_ABORT(std::out_of_range("Index out of range"));
         }
         return const_value_proxy(*items[pos], this);
      }
      
      value_proxy front()
      {
         auto lock = acquire_shared_lock();
         return value_proxy(*items.front(), this);
      }
      
      const_value_proxy front() const
      {
         auto lock = acquire_shared_lock();
         return const_value_proxy(*items.front(), this);
      }
      
      value_proxy back()
      {
         auto lock = acquire_shared_lock();
         return value_proxy(*items.back(), this);
      }
      
      const_value_proxy back() const
      {
         auto lock = acquire_shared_lock();
         return const_value_proxy(*items.back(), this);
      }
      
      // Capacity Methods
      bool empty() const
      {
         auto lock = acquire_shared_lock();
         return items.empty();
      }
      
      size_type size() const
      {
         auto lock = acquire_shared_lock();
         return items.size();
      }
      
      size_type max_size() const
      {
         auto lock = acquire_shared_lock();
         return items.max_size();
      }
      
      void reserve(size_type new_cap)
      {
         auto lock = acquire_unique_lock();
         items.reserve(new_cap);
      }
      
      size_type capacity() const
      {
         auto lock = acquire_shared_lock();
         return items.capacity();
      }
      
      void shrink_to_fit()
      {
         auto lock = acquire_unique_lock();
         items.shrink_to_fit();
      }
      
      // Modifiers
      void clear()
      {
         auto lock = acquire_unique_lock();
         items.clear();
      }
      
      void push_back(const V& value)
      {
         auto lock = acquire_unique_lock();
         items.push_back(std::make_unique<V>(value));
      }
      
      template <class... Args>
      void emplace_back(Args&&... args)
      {
         auto lock = acquire_unique_lock();
         items.emplace_back(std::make_unique<V>(std::forward<Args>(args)...));
      }
      
      void pop_back()
      {
         auto lock = acquire_unique_lock();
         items.pop_back();
      }
      
      iterator insert(iterator pos, const V& value)
      {
         auto lock = acquire_unique_lock();
         auto index = std::distance(items.begin(), pos.item_it);
         auto it = items.insert(items.begin() + index, std::make_unique<V>(value));
         return iterator(it, this);
      }
      
      iterator insert(iterator pos, V&& value)
      {
         auto lock = acquire_unique_lock();
         auto index = std::distance(items.begin(), pos.item_it);
         auto it = items.insert(items.begin() + index, std::make_unique<V>(std::move(value)));
         return iterator(it, this);
      }
      
      template <class... Args>
      iterator emplace(iterator pos, Args&&... args)
      {
         auto lock = acquire_unique_lock();
         auto index = std::distance(items.begin(), pos.item_it);
         auto it = items.insert(items.begin() + index, std::make_unique<V>(std::forward<Args>(args)...));
         return iterator(it, this);
      }
      
      iterator erase(iterator pos)
      {
         auto lock = acquire_unique_lock();
         auto index = std::distance(items.begin(), pos.item_it);
         auto it = items.erase(items.begin() + index);
         return iterator(it, this);
      }
      
      iterator erase(iterator first, iterator last)
      {
         auto lock = acquire_unique_lock();
         auto first_index = std::distance(items.begin(), first.item_it);
         auto last_index = std::distance(items.begin(), last.item_it);
         auto it = items.erase(items.begin() + first_index, items.begin() + last_index);
         return iterator(it, this);
      }
      
      void resize(size_type count)
      {
         auto lock = acquire_unique_lock();
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
         auto lock = acquire_unique_lock();
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
         
         auto this_lock = acquire_unique_lock();
         auto other_lock = other.acquire_unique_lock();
         
         items.swap(other.items);
      }
      
      // Iterators
      iterator begin()
      {
         auto lock = acquire_unique_lock();
         return iterator(items.begin(), this);
      }
      
      const_iterator begin() const
      {
         auto lock = acquire_shared_lock();
         return const_iterator(items.cbegin(), this);
      }
      
      const_iterator cbegin() const
      {
         auto lock = acquire_shared_lock();
         return const_iterator(items.cbegin(), this);
      }
      
      iterator end()
      {
         auto lock = acquire_unique_lock();
         return iterator(items.end(), this);
      }
      
      const_iterator end() const
      {
         auto lock = acquire_shared_lock();
         return const_iterator(items.cend(), this);
      }
      
      const_iterator cend() const
      {
         auto lock = acquire_shared_lock();
         return const_iterator(items.cend(), this);
      }
   };
}
