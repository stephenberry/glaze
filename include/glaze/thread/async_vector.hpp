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

// glz::early_shared_ptr deletes memory when the use count reaches 0 rather than 1 like a std::shared_ptr
#include "glaze/thread/early_shared_ptr.hpp"

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
   // Design considerations
   // We use a std::unique_ptr inside the vector so that we can store types that are not copyable and no movable
   // iterators need unique_locks, because they allow write access
   // const iterators use shared_locks, because the access is read only
   // We use non const iterators for instert and erase calls so that we can reuse the same unique_lock and avoid dead locks
   // We use shared_ptrs of locks so that we can share locks and avoid dead locks when copying iterators
   
   template <class V>
   struct async_vector
   {
   private:
      std::vector<std::unique_ptr<V>> items;
      mutable std::shared_mutex mutex;
      
      // Base lock class for type erasure
      class lock_base {
      public:
         virtual ~lock_base() = default;
         virtual bool owns_lock() const = 0;
         virtual bool is_unique() const = 0;
      };
      
      // Derived class for unique locks
      class unique_lock_wrapper : public lock_base {
      private:
         std::unique_lock<std::shared_mutex> lock;
         
      public:
         explicit unique_lock_wrapper(std::shared_mutex& mutex)
         : lock(mutex) {}
         
         explicit unique_lock_wrapper(std::shared_mutex& mutex, std::defer_lock_t defer)
         : lock(mutex, defer) {}
         
         bool owns_lock() const override {
            return lock.owns_lock();
         }
         
         bool is_unique() const override {
            return true;
         }
         
         std::unique_lock<std::shared_mutex>& get_lock() {
            return lock;
         }
      };
      
      // Derived class for shared locks
      class shared_lock_wrapper : public lock_base {
      private:
         std::shared_lock<std::shared_mutex> lock;
         
      public:
         explicit shared_lock_wrapper(std::shared_mutex& mutex)
         : lock(mutex) {}
         
         explicit shared_lock_wrapper(std::shared_mutex& mutex, std::defer_lock_t defer)
         : lock(mutex, defer) {}
         
         bool owns_lock() const override {
            return lock.owns_lock();
         }
         
         bool is_unique() const override {
            return false;
         }
         
         std::shared_lock<std::shared_mutex>& get_lock() {
            return lock;
         }
      };
      
      // Unified lock manager
      mutable early_shared_ptr<lock_base> global_lock;
      
      // Custom deleter for lock management
      class lock_deleter {
      private:
         async_vector* parent;
         
      public:
         explicit lock_deleter(async_vector* parent) : parent(parent) {}
         
         void operator()(lock_base* lock) {
            if (parent->global_lock.use_count() == 1) {
               // This is the last reference in the parent, reset the global pointer
               parent->global_lock.reset();
            }
            
            // Always delete the actual lock object
            delete lock;
         }
      };
      
      // Helper methods to acquire locks with proper blocking
      early_shared_ptr<lock_base> acquire_unique_lock() const
      {
         // If a unique lock already exists, use it
         if (global_lock && global_lock->owns_lock() && global_lock->is_unique()) {
            return global_lock;
         }
         
         // We need to wait for any existing locks to be released and then acquire a unique lock
         static std::mutex lock_mutex;
         std::lock_guard<std::mutex> lock_guard(lock_mutex);
         
         // Create new unique lock - will block until it can be acquired
         auto raw_lock = new unique_lock_wrapper(mutex);
         
         // Use custom deleter that will reset the global pointer when appropriate
         global_lock = early_shared_ptr<lock_base>(
                                                  raw_lock, lock_deleter(const_cast<async_vector*>(this)));
         
         return global_lock;
      }
      
      early_shared_ptr<lock_base> acquire_shared_lock() const
      {
         // If a shared lock already exists, use it
         if (global_lock && global_lock->owns_lock()) {
            return global_lock;
         }
         
         // If a unique lock already exists, we can use it instead (read access is covered by write access)
         if (global_lock && global_lock->owns_lock() && global_lock->is_unique()) {
            return global_lock;
         }
         
         // Need to create a new shared lock
         static std::mutex lock_mutex;
         std::lock_guard<std::mutex> lock_guard(lock_mutex);
         
         // Create new shared lock - will block until it can be acquired
         auto raw_lock = new shared_lock_wrapper(mutex);
         
         // Use custom deleter that will reset the global pointer when appropriate
         global_lock = early_shared_ptr<lock_base>(
                                                  raw_lock, lock_deleter(const_cast<async_vector*>(this)));
         
         return global_lock;
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
         early_shared_ptr<lock_base> lock_ptr;
         
      public:
         iterator(typename std::vector<std::unique_ptr<V>>::iterator item_it, async_vector* parent)
         : item_it(item_it), parent(parent)
         {
            // Acquire a lock for the iterator
            lock_ptr = parent->acquire_unique_lock();
         }
         
         ~iterator()
         {
            // No need to release locks here - the parent will handle it
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
         early_shared_ptr<lock_base> lock_ptr;
         
      public:
         const_iterator(typename std::vector<std::unique_ptr<V>>::const_iterator item_it,
                        const async_vector* parent)
         : item_it(item_it), parent(parent)
         {
            // Acquire a lock for the iterator
            lock_ptr = parent->acquire_shared_lock();
         }
         
         ~const_iterator()
         {
            // No need to release locks here - the parent will handle it
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
         early_shared_ptr<lock_base> lock_ptr;
         
      public:
         value_proxy(V& value_ref, async_vector* parent)
         : value_ref(value_ref)
         {
            // Acquire a lock for the proxy
            lock_ptr = parent->acquire_shared_lock();
         }
         
         ~value_proxy()
         {
            // No need to release locks here - the parent will handle it
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
         early_shared_ptr<lock_base> lock_ptr;
         
      public:
         const_value_proxy(const V& value_ref, const async_vector* parent)
         : value_ref(value_ref)
         {
            // Acquire a lock for the proxy
            lock_ptr = parent->acquire_shared_lock();
         }
         
         ~const_value_proxy()
         {
            // No need to release locks here - the parent will handle it
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
      // These methods should acquire locks that will be shared among iterators
      iterator begin()
      {
         acquire_unique_lock();
         return iterator(items.begin(), this);
      }
      
      const_iterator begin() const
      {
         acquire_shared_lock();
         return const_iterator(items.cbegin(), this);
      }
      
      const_iterator cbegin() const
      {
         acquire_shared_lock();
         return const_iterator(items.cbegin(), this);
      }
      
      iterator end()
      {
         // We need to use the same lock type as begin()
         if (!global_lock) {
            acquire_unique_lock();
         }
         return iterator(items.end(), this);
      }
      
      const_iterator end() const
      {
         // We need to use the same lock type as begin()
         if (!global_lock) {
            acquire_shared_lock();
         }
         return const_iterator(items.cend(), this);
      }
      
      const_iterator cend() const
      {
         // We need to use the same lock type as cbegin()
         if (!global_lock) {
            acquire_shared_lock();
         }
         return const_iterator(items.cend(), this);
      }
   };
}
