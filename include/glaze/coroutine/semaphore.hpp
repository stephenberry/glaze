// Glaze Library
// For the license information refer to glaze.hpp

// Modified from the awesome: https://github.com/jbaldwin/libcoro

#pragma once

#include <atomic>
#include "glaze/util/expected.hpp"
#include <coroutine>
#include <mutex>
#include <string>

namespace glz
{
   struct semaphore
   {
      enum struct acquire_result { acquired, semaphore_stopped };

      static std::string to_string(acquire_result ar)
      {
         switch (ar) {
         case acquire_result::acquired:
            return "acquired";
         case acquire_result::semaphore_stopped:
            return "semaphore_stopped";
         }

         return "unknown";
      }

      explicit semaphore(std::ptrdiff_t least_max_value_and_starting_value)
          : semaphore(least_max_value_and_starting_value, least_max_value_and_starting_value)
      {}
      
      explicit semaphore(std::ptrdiff_t least_max_value, std::ptrdiff_t starting_value)
          : m_least_max_value(least_max_value),
            m_counter(starting_value <= least_max_value ? starting_value : least_max_value)
      {}
      
      
      ~semaphore() {
          notify_waiters();
      }

      semaphore(const semaphore&) = delete;
      semaphore(semaphore&&) = delete;

      auto operator=(const semaphore&) noexcept -> semaphore& = delete;
      auto operator=(semaphore&&) noexcept -> semaphore& = delete;

      struct acquire_operation
      {
         explicit acquire_operation(semaphore& s) : m_semaphore(s)
         {}

         bool await_ready() const noexcept
         {
             if (m_semaphore.m_notify_all_set.load(std::memory_order::relaxed))
             {
                 return true;
             }
             return m_semaphore.try_acquire();
         }
         
         bool await_suspend(std::coroutine_handle<> awaiting_coroutine) noexcept
         {
             std::unique_lock lk{m_semaphore.m_waiter_mutex};
             if (m_semaphore.m_notify_all_set.load(std::memory_order::relaxed))
             {
                 return false;
             }

             if (m_semaphore.try_acquire())
             {
                 return false;
             }

             if (m_semaphore.m_acquire_waiters == nullptr)
             {
                 m_semaphore.m_acquire_waiters = this;
             }
             else
             {
                 // This is LIFO, but semaphores are not meant to be fair.

                 // Set our next to the current head.
                 m_next = m_semaphore.m_acquire_waiters;
                 // Set the semaphore head to this.
                 m_semaphore.m_acquire_waiters = this;
             }

             m_awaiting_coroutine = awaiting_coroutine;
             return true;
         }
         
         acquire_result await_resume() const
         {
             if (m_semaphore.m_notify_all_set.load(std::memory_order::relaxed))
             {
                 return acquire_result::semaphore_stopped;
             }
             return acquire_result::acquired;
         }

        private:
         friend semaphore;

         semaphore& m_semaphore;
         std::coroutine_handle<> m_awaiting_coroutine;
         acquire_operation* m_next{nullptr};
      };

      void release()
      {
          // It seems like the atomic counter could be incremented, but then resuming a waiter could have
          // a race between a new acquirer grabbing the just incremented resource value from us.  So its
          // best to check if there are any waiters first, and transfer owernship of the resource thats
          // being released directly to the waiter to avoid this problem.

          std::unique_lock lk{m_waiter_mutex};
          if (m_acquire_waiters != nullptr)
          {
              acquire_operation* to_resume = m_acquire_waiters;
              m_acquire_waiters            = m_acquire_waiters->m_next;
              lk.unlock();

              // This will transfer ownership of the resource to the resumed waiter.
              to_resume->m_awaiting_coroutine.resume();
          }
          else
          {
              // Normally would be release but within a lock use releaxed.
              m_counter.fetch_add(1, std::memory_order::relaxed);
          }
      }

      /**
       * Acquires a resource from the semaphore, if the semaphore has no resources available then
       * this will wait until a resource becomes available.
       */
      [[nodiscard]] acquire_operation acquire() { return acquire_operation{*this}; }

      /**
       * Attemtps to acquire a resource if there is any resources available.
       * @return True if the acquire operation was able to acquire a resource.
       */
      bool try_acquire()
      {
          // Optimistically grab the resource.
          auto previous = m_counter.fetch_sub(1, std::memory_order::acq_rel);
          if (previous <= 0)
          {
              // If it wasn't available undo the acquisition.
              m_counter.fetch_add(1, std::memory_order::release);
              return false;
          }
          return true;
      }

      /**
       * @return The maximum number of resources the semaphore can contain.
       */
      std::ptrdiff_t max_resources() const noexcept { return m_least_max_value; }

      /**
       * The current number of resources available in this semaphore.
       */
      std::ptrdiff_t value() const noexcept { return m_counter.load(std::memory_order::relaxed); }

      /**
       * Stops the semaphore and will notify all release/acquire waiters to wake up in a failed state.
       * Once this is set it cannot be un-done and all future oprations on the semaphore will fail.
       */
      void notify_waiters() noexcept
      {
          m_notify_all_set.exchange(true, std::memory_order::release);
          while (true)
          {
              std::unique_lock lk{m_waiter_mutex};
              if (m_acquire_waiters != nullptr)
              {
                  acquire_operation* to_resume = m_acquire_waiters;
                  m_acquire_waiters            = m_acquire_waiters->m_next;
                  lk.unlock();

                  to_resume->m_awaiting_coroutine.resume();
              }
              else
              {
                  break;
              }
          }
      }

     private:
      friend struct release_operation;
      friend struct acquire_operation;

      const std::ptrdiff_t m_least_max_value;
      std::atomic<std::ptrdiff_t> m_counter;

      std::mutex m_waiter_mutex{};
      acquire_operation* m_acquire_waiters{nullptr};

      std::atomic<bool> m_notify_all_set{false};
   };
}
