// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <atomic>
#include <functional>
#include <utility>
#include <type_traits>

/**
 * @file early_shared_ptr.hpp
 * @brief Implementation of a smart pointer with early object destruction semantics
 * @ingroup memory
 *
 * @details
 * The early_shared_ptr is a reference-counted smart pointer similar to std::shared_ptr,
 * but with a key difference in its destruction semantics. Unlike std::shared_ptr, which
 * destroys the managed object when the last reference is released, early_shared_ptr
 * destroys the managed object when there is only one reference left (the control block).
 *
 * This early destruction behavior can be beneficial in scenarios where you want to
 * release resources earlier, without waiting for all shared_ptr instances to be destroyed.
 * It can help reduce memory pressure and improve efficiency in certain usage patterns.
 *
 * @note The control block continues to exist until all references are released, even
 * after the managed object has been destroyed. This allows reference counting to work
 * correctly while enabling early object destruction.
 *
 * @warning Using early_shared_ptr requires careful consideration of object lifetime.
 * After the object has been destroyed, any attempt to dereference the pointer will
 * result in undefined behavior, even if valid early_shared_ptr instances still exist.
 *
 * @tparam T The type of the managed object
 */

namespace glz
{
   template <typename T>
   class early_shared_ptr {
   private:
      struct ControlBlock {
         T* ptr = nullptr;
         std::atomic<size_t> use_count{0};
         std::function<void(T*)> deleter = std::default_delete<T>();
         
         explicit ControlBlock(T* p, std::function<void(T*)> d = std::default_delete<T>())
         : ptr(p), deleter(std::move(d)) {}
         
         void increment() { ++use_count; }
         
         size_t decrement() { return --use_count; }
      };
      
      ControlBlock* control_block = nullptr;
      
   public:
      // Default constructor
      constexpr early_shared_ptr() noexcept = default;
      
      // Nullptr constructor
      constexpr early_shared_ptr(std::nullptr_t) noexcept : early_shared_ptr() {}
      
      // Constructor with raw pointer
      template <typename U, typename Deleter = std::default_delete<U>,
      typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
      explicit early_shared_ptr(U* ptr, Deleter deleter = Deleter()) {
         if (ptr) {
            control_block = new ControlBlock(ptr, [deleter](T* p) mutable { deleter(static_cast<U*>(p)); });
            control_block->increment();
         }
      }
      
      // Copy constructor
      early_shared_ptr(const early_shared_ptr& other) noexcept {
         control_block = other.control_block;
         if (control_block) {
            control_block->increment();
         }
      }
      
      // Move constructor
      early_shared_ptr(early_shared_ptr&& other) noexcept {
         control_block = other.control_block;
         other.control_block = nullptr;
      }
      
      // Copy assignment
      early_shared_ptr& operator=(const early_shared_ptr& other) noexcept {
         if (this != &other) {
            reset();
            control_block = other.control_block;
            if (control_block) {
               control_block->increment();
            }
         }
         return *this;
      }
      
      // Move assignment
      early_shared_ptr& operator=(early_shared_ptr&& other) noexcept {
         if (this != &other) {
            reset();
            control_block = other.control_block;
            other.control_block = nullptr;
         }
         return *this;
      }
      
      // Destructor
      ~early_shared_ptr() {
         reset();
      }
      
      // Reset to empty
      void reset() noexcept {
         if (control_block) {
            // Key difference: destroy the object when count reaches 1
            size_t count = control_block->decrement();
            if (count == 1) {
               // We're at use count 1, meaning only the control block has a reference
               // Delete the managed object but keep the control block
               if (control_block->ptr) {
                  control_block->deleter(control_block->ptr);
                  control_block->ptr = nullptr;
               }
            } else if (count == 0) {
               // No more references, delete the control block
               delete control_block;
            }
            control_block = nullptr;
         }
      }
      
      // Reset with a new pointer
      template <typename U, typename Deleter = std::default_delete<U>,
      typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
      void reset(U* ptr, Deleter d = Deleter()) {
         reset();
         if (ptr) {
            control_block = new ControlBlock(ptr, [d](T* p) { d(static_cast<U*>(p)); });
            control_block->increment();
         }
      }
      
      // Swap with another early_shared_ptr
      void swap(early_shared_ptr& other) noexcept {
         std::swap(control_block, other.control_block);
      }
      
      // Get the managed object
      T* get() const noexcept {
         return control_block ? control_block->ptr : nullptr;
      }
      
      // Get the use count
      size_t use_count() const noexcept {
         return control_block ? control_block->use_count.load() : 0;
      }
      
      // Check if this is the only reference
      bool unique() const noexcept {
         return use_count() == 1;
      }
      
      // Conversion to bool
      explicit operator bool() const noexcept {
         return get() != nullptr;
      }
      
      // Dereference operators
      T& operator*() const noexcept {
         return *get();
      }
      
      T* operator->() const noexcept {
         return get();
      }
   };
   
   // Non-member functions
   template <typename T, typename U>
   bool operator==(const early_shared_ptr<T>& lhs, const early_shared_ptr<U>& rhs) noexcept {
      return lhs.get() == rhs.get();
   }
   
   template <typename T>
   bool operator==(const early_shared_ptr<T>& lhs, std::nullptr_t) noexcept {
      return !lhs;
   }
   
   template <typename T>
   bool operator==(std::nullptr_t, const early_shared_ptr<T>& rhs) noexcept {
      return !rhs;
   }
   
   template <typename T, typename U>
   bool operator!=(const early_shared_ptr<T>& lhs, const early_shared_ptr<U>& rhs) noexcept {
      return !(lhs == rhs);
   }
   
   template <typename T>
   bool operator!=(const early_shared_ptr<T>& lhs, std::nullptr_t) noexcept {
      return static_cast<bool>(lhs);
   }
   
   template <typename T>
   bool operator!=(std::nullptr_t, const early_shared_ptr<T>& rhs) noexcept {
      return static_cast<bool>(rhs);
   }
   
   // Make function (similar to std::make_shared)
   template <typename T, typename... Args>
   early_shared_ptr<T> make_early_shared(Args&&... args) {
      return early_shared_ptr<T>(new T(std::forward<Args>(args)...));
   }
}
