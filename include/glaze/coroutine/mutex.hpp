// Glaze Library
// For the license information refer to glaze.hpp

// Modified from the awesome: https://github.com/jbaldwin/libcoro

#pragma once

#include <atomic>
#include <coroutine>
#include <mutex>
#include <utility>

namespace glz
{
   struct mutex;

   /**
    * A scoped RAII lock holder, just like std::lock_guard or std::scoped_lock in that the coro::mutex
    * is always unlocked unpon this coro::scoped_lock going out of scope.  It is possible to unlock the
    * coro::mutex prior to the end of its current scope by manually calling the unlock() function.
    */
   struct scoped_lock
   {
      friend struct mutex;

     public:
      enum struct lock_strategy {
         /// The lock is already acquired, adopt it as the new owner.
         adopt
      };

      explicit scoped_lock(mutex& m, lock_strategy strategy = lock_strategy::adopt) : m_mutex(&m)
      {
         // Future -> support acquiring the lock?  Not sure how to do that without being able to
         // co_await in the constructor.
         (void)strategy;
      }

      /**
       * Unlocks the mutex upon this shared lock destructing.
       */
      ~scoped_lock()
      {
          unlock();
      }

      scoped_lock(const scoped_lock&) = delete;
      scoped_lock(scoped_lock&& other) : m_mutex(std::exchange(other.m_mutex, nullptr)) {}
      auto operator=(const scoped_lock&) -> scoped_lock& = delete;
      auto operator=(scoped_lock&& other) noexcept -> scoped_lock&
      {
         if (std::addressof(other) != this) {
            m_mutex = std::exchange(other.m_mutex, nullptr);
         }
         return *this;
      }

      /**
       * Unlocks the scoped lock prior to it going out of scope.  Calling this multiple times has no
       * additional affect after the first call.
       */
      void unlock();

     private:
      mutex* m_mutex{};
   };

   struct mutex
   {
      explicit mutex() noexcept : m_state(const_cast<void*>(unlocked_value())) {}
      ~mutex() = default;

      mutex(const mutex&) = delete;
      mutex(mutex&&) = delete;
      auto operator=(const mutex&) -> mutex& = delete;
      auto operator=(mutex&&) -> mutex& = delete;

      struct lock_operation
      {
         explicit lock_operation(mutex& m) : m_mutex(m) {}

         bool await_ready() const noexcept
         {
             if (m_mutex.try_lock())
             {
                 // Since there is no mutex acquired, insert a memory fence to act like it.
                 std::atomic_thread_fence(std::memory_order::acquire);
                 return true;
             }
             return false;
         }
         
         bool await_suspend(std::coroutine_handle<> awaiting_coroutine) noexcept
         {
             m_awaiting_coroutine = awaiting_coroutine;
             void* current = m_mutex.m_state.load(std::memory_order::acquire);
             void* new_value;

             const void* unlocked_value = m_mutex.unlocked_value();
             do
             {
                 if (current == unlocked_value)
                 {
                     // If the current value is 'unlocked' then attempt to lock it.
                     new_value = nullptr;
                 }
                 else
                 {
                     // If the current value is a waiting lock operation, or nullptr, set our next to that
                     // lock op and attempt to set ourself as the head of the waiter list.
                     m_next    = static_cast<lock_operation*>(current);
                     new_value = static_cast<void*>(this);
                 }
             } while (!m_mutex.m_state.compare_exchange_weak(current, new_value, std::memory_order::acq_rel));

             // Don't suspend if the state went from unlocked -> locked with zero waiters.
             if (current == unlocked_value)
             {
                 std::atomic_thread_fence(std::memory_order::acquire);
                 m_awaiting_coroutine = nullptr; // nothing to await later since this doesn't suspend
                 return false;
             }

             return true;
         }
         
         scoped_lock await_resume() noexcept { return scoped_lock{m_mutex}; }

        private:
         friend struct mutex;

         mutex& m_mutex;
         std::coroutine_handle<> m_awaiting_coroutine;
         lock_operation* m_next{nullptr};
      };

      /**
       * To acquire the mutex's lock co_await this function.  Upon acquiring the lock it returns
       * a coro::scoped_lock which will hold the mutex until the coro::scoped_lock destructs.
       * @return A co_await'able operation to acquire the mutex.
       */
      [[nodiscard]] auto lock() -> lock_operation { return lock_operation{*this}; };

      /**
       * Attempts to lock the mutex.
       * @return True if the mutex lock was acquired, otherwise false.
       */
      bool try_lock()
      {
          void* expected = const_cast<void*>(unlocked_value());
          return m_state.compare_exchange_strong(expected, nullptr, std::memory_order::acq_rel, std::memory_order::relaxed);
      }

      /**
       * Releases the mutex's lock.
       */
      void unlock()
      {
          if (m_internal_waiters == nullptr)
          {
              void* current = m_state.load(std::memory_order::relaxed);
              if (current == nullptr)
              {
                  // If there are no internal waiters and there are no atomic waiters, attempt to set the
                  // mutex as unlocked.
                  if (m_state.compare_exchange_strong(
                          current,
                          const_cast<void*>(unlocked_value()),
                          std::memory_order::release,
                          std::memory_order::relaxed))
                  {
                      return; // The mutex is now unlocked with zero waiters.
                  }
                  // else we failed to unlock, someone added themself as a waiter.
              }

              // There are waiters on the atomic list, acquire them and update the state for all others.
              m_internal_waiters = static_cast<lock_operation*>(m_state.exchange(nullptr, std::memory_order::acq_rel));

              // Should internal waiters be reversed to allow for true FIFO, or should they be resumed
              // in this reverse order to maximum throuhgput?  If this list ever gets 'long' the reversal
              // will take some time, but it might guarantee better latency across waiters.  This LIFO
              // middle ground on the atomic waiters means the best throughput at the cost of the first
              // waiter possibly having added latency based on the queue length of waiters.  Either way
              // incurs a cost but this way for short lists will most likely be faster even though it
              // isn't completely fair.
          }

          // assert m_internal_waiters != nullptr

          lock_operation* to_resume = m_internal_waiters;
          m_internal_waiters        = m_internal_waiters->m_next;
          to_resume->m_awaiting_coroutine.resume();
      }

     private:
      friend struct lock_operation;

      /// unlocked -> state == unlocked_value()
      /// locked but empty waiter list == nullptr
      /// locked with waiters == lock_operation*
      std::atomic<void*> m_state;

      /// A list of grabbed internal waiters that are only accessed by the unlock()'er.
      lock_operation* m_internal_waiters{nullptr};

      /// Inactive value, this cannot be nullptr since we want nullptr to signify that the mutex
      /// is locked but there are zero waiters, this makes it easy to CAS new waiters into the
      /// m_state linked list.
      auto unlocked_value() const noexcept -> const void* { return &m_state; }
   };
   
   void scoped_lock::unlock()
   {
       if (m_mutex)
       {
           std::atomic_thread_fence(std::memory_order::release);
           m_mutex->unlock();
           // Only allow a scoped lock to unlock the mutex a single time.
           m_mutex = nullptr;
       }
   }
}
