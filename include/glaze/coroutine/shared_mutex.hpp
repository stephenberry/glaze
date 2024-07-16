// Glaze Library
// For the license information refer to glaze.hpp

// Modified from the awesome: https://github.com/jbaldwin/libcoro

#pragma once

#ifndef GLZ_THROW_OR_ABORT
#if __cpp_exceptions
#define GLZ_THROW_OR_ABORT(EXC) (throw(EXC))
#include <exception>
#else
#define GLZ_THROW_OR_ABORT(EXC) (std::abort())
#endif
#endif

#include <atomic>
#include <coroutine>
#include <mutex>

#include "glaze/coroutine/concepts.hpp"

namespace glz
{
   template <executor executor_type>
   struct shared_mutex;

   /**
    * A scoped RAII lock holder for a coro::shared_mutex.  It will call the appropriate unlock() or
    * unlock_shared() based on how the coro::shared_mutex was originally acquired, either shared or
    * exclusive modes.
    */
   template <executor executor_type>
   struct shared_scoped_lock
   {
      shared_scoped_lock(shared_mutex<executor_type>& sm, bool exclusive) : m_shared_mutex(&sm), m_exclusive(exclusive)
      {}

      /**
       * Unlocks the mutex upon this shared scoped lock destructing.
       */
      ~shared_scoped_lock() { unlock(); }

      shared_scoped_lock(const shared_scoped_lock&) = delete;
      shared_scoped_lock(shared_scoped_lock&& other)
         : m_shared_mutex(std::exchange(other.m_shared_mutex, nullptr)), m_exclusive(other.m_exclusive)
      {}

      auto operator=(const shared_scoped_lock&) -> shared_scoped_lock& = delete;
      auto operator=(shared_scoped_lock&& other) noexcept -> shared_scoped_lock&
      {
         if (std::addressof(other) != this) {
            m_shared_mutex = std::exchange(other.m_shared_mutex, nullptr);
            m_exclusive = other.m_exclusive;
         }
         return *this;
      }

      /**
       * Unlocks the shared mutex prior to this lock going out of scope.
       */
      auto unlock() -> void
      {
         if (m_shared_mutex != nullptr) {
            if (m_exclusive) {
               m_shared_mutex->unlock();
            }
            else {
               m_shared_mutex->unlock_shared();
            }

            m_shared_mutex = nullptr;
         }
      }

     private:
      shared_mutex<executor_type>* m_shared_mutex{nullptr};
      bool m_exclusive{false};
   };

   template <executor executor_type>
   struct shared_mutex
   {
      /**
       * @param e The executor for when multiple shared waiters can be woken up at the same time,
       *          each shared waiter will be scheduled to immediately run on this executor in
       *          parallel.
       */
      explicit shared_mutex(std::shared_ptr<executor_type> e) : m_executor(std::move(e))
      {
         if (m_executor == nullptr) {
            GLZ_THROW_OR_ABORT(std::runtime_error{"shared_mutex cannot have a nullptr executor"});
         }
      }
      ~shared_mutex() = default;

      shared_mutex(const shared_mutex&) = delete;
      shared_mutex(shared_mutex&&) = delete;
      auto operator=(const shared_mutex&) -> shared_mutex& = delete;
      auto operator=(shared_mutex&&) -> shared_mutex& = delete;

      struct lock_operation
      {
         lock_operation(shared_mutex& sm, bool exclusive) : m_shared_mutex(sm), m_exclusive(exclusive) {}

         auto await_ready() const noexcept -> bool
         {
            if (m_exclusive) {
               return m_shared_mutex.try_lock();
            }
            else {
               return m_shared_mutex.try_lock_shared();
            }
         }

         auto await_suspend(std::coroutine_handle<> awaiting_coroutine) noexcept -> bool
         {
            std::unique_lock lk{m_shared_mutex.m_mutex};
            // Its possible the lock has been released between await_ready() and await_suspend(), double
            // check and make sure we are not going to suspend when nobody holds the lock.
            if (m_exclusive) {
               if (m_shared_mutex.try_lock_locked(lk)) {
                  return false;
               }
            }
            else {
               if (m_shared_mutex.try_lock_shared_locked(lk)) {
                  return false;
               }
            }

            // For sure the lock is currently held in a manner that it cannot be acquired, suspend ourself
            // at the end of the waiter list.

            if (m_shared_mutex.m_tail_waiter == nullptr) {
               m_shared_mutex.m_head_waiter = this;
               m_shared_mutex.m_tail_waiter = this;
            }
            else {
               m_shared_mutex.m_tail_waiter->m_next = this;
               m_shared_mutex.m_tail_waiter = this;
            }

            // If this is an exclusive lock acquire then mark it as so so that shared locks after this
            // exclusive one will also suspend so this exclusive lock doens't get starved.
            if (m_exclusive) {
               ++m_shared_mutex.m_exclusive_waiters;
            }

            m_awaiting_coroutine = awaiting_coroutine;
            return true;
         }
         auto await_resume() noexcept -> shared_scoped_lock<executor_type>
         {
            return shared_scoped_lock{m_shared_mutex, m_exclusive};
         }

        private:
         friend struct shared_mutex;

         shared_mutex& m_shared_mutex;
         bool m_exclusive{false};
         std::coroutine_handle<> m_awaiting_coroutine;
         lock_operation* m_next{nullptr};
      };

      /**
       * Locks the mutex in a shared state.  If there are any exclusive waiters then the shared waiters
       * will also wait so the exclusive waiters are not starved.
       */
      [[nodiscard]] auto lock_shared() -> lock_operation { return lock_operation{*this, false}; }

      /**
       * Locks the mutex in an exclusive state.
       */
      [[nodiscard]] auto lock() -> lock_operation { return lock_operation{*this, true}; }

      /**
       * @return True if the lock could immediately be acquired in a shared state.
       */
      auto try_lock_shared() -> bool
      {
         // To acquire the shared lock the state must be one of two states:
         //   1) unlocked
         //   2) shared locked with zero exclusive waiters
         //          Zero exclusive waiters prevents exclusive starvation if shared locks are
         //          always continuously happening.

         std::unique_lock lk{m_mutex};
         return try_lock_shared_locked(lk);
      }

      /**
       * @return True if the lock could immediately be acquired in an exclusive state.
       */
      auto try_lock() -> bool
      {
         // To acquire the exclusive lock the state must be unlocked.
         std::unique_lock lk{m_mutex};
         return try_lock_locked(lk);
      }

      /**
       * Unlocks a single shared state user.  *REQUIRES* that the lock was first acquired exactly once
       * via `lock_shared()` or `try_lock_shared() -> True` before being called, otherwise undefined
       * behavior.
       *
       * If the shared user count drops to zero and this lock has an exclusive waiter then the exclusive
       * waiter acquires the lock.
       */
      auto unlock_shared() -> void
      {
         std::unique_lock lk{m_mutex};
         --m_shared_users;

         // Only wake waiters from shared state if all shared users have completed.
         if (m_shared_users == 0) {
            if (m_head_waiter != nullptr) {
               wake_waiters(lk);
            }
            else {
               m_state = state::unlocked;
            }
         }
      }

      /**
       * Unlocks the mutex from its exclusive state.  If there is a following exclusive watier then
       * that exclusive waiter acquires the lock.  If there are 1 or more shared waiters then all the
       * shared waiters acquire the lock in a shared state in parallel and are resumed on the original
       * thread pool this shared mutex was created with.
       */
      auto unlock() -> void
      {
         std::unique_lock lk{m_mutex};
         if (m_head_waiter != nullptr) {
            wake_waiters(lk);
         }
         else {
            m_state = state::unlocked;
         }
      }

     private:
      friend struct lock_operation;

      enum struct state { unlocked, locked_shared, locked_exclusive };

      /// This executor is for resuming multiple shared waiters.
      std::shared_ptr<executor_type> m_executor{nullptr};

      std::mutex m_mutex;

      state m_state{state::unlocked};

      /// The current number of shared users that have acquired the lock.
      uint64_t m_shared_users{0};
      /// The current number of exclusive waiters waiting to acquire the lock.  This is used to block
      /// new incoming shared lock attempts so the exclusive waiter is not starved.
      uint64_t m_exclusive_waiters{0};

      lock_operation* m_head_waiter{nullptr};
      lock_operation* m_tail_waiter{nullptr};

      auto try_lock_shared_locked(std::unique_lock<std::mutex>& lk) -> bool
      {
         if (m_state == state::unlocked) {
            // If the shared mutex is unlocked put it into shared mode and add ourself as using the lock.
            m_state = state::locked_shared;
            ++m_shared_users;
            lk.unlock();
            return true;
         }
         else if (m_state == state::locked_shared && m_exclusive_waiters == 0) {
            // If the shared mutex is in a shared locked state and there are no exclusive waiters
            // the add ourself as using the lock.
            ++m_shared_users;
            lk.unlock();
            return true;
         }

         // If the lock is in shared mode but there are exclusive waiters then we will also wait so
         // the writers are not starved.

         // If the lock is in exclusive mode already then we need to wait.

         return false;
      }

      auto try_lock_locked(std::unique_lock<std::mutex>& lk) -> bool
      {
         if (m_state == state::unlocked) {
            m_state = state::locked_exclusive;
            lk.unlock();
            return true;
         }
         return false;
      }

      auto wake_waiters(std::unique_lock<std::mutex>& lk) -> void
      {
         // First determine what the next lock state will be based on the first waiter.
         if (m_head_waiter->m_exclusive) {
            // If its exclusive then only this waiter can be woken up.
            m_state = state::locked_exclusive;
            lock_operation* to_resume = m_head_waiter;
            m_head_waiter = m_head_waiter->m_next;
            --m_exclusive_waiters;
            if (m_head_waiter == nullptr) {
               m_tail_waiter = nullptr;
            }

            // Since this is an exclusive lock waiting we can resume it directly.
            lk.unlock();
            to_resume->m_awaiting_coroutine.resume();
         }
         else {
            // If its shared then we will scan forward and awake all shared waiters onto the given
            // thread pool so they can run in parallel.
            m_state = state::locked_shared;
            do {
               lock_operation* to_resume = m_head_waiter;
               m_head_waiter = m_head_waiter->m_next;
               if (m_head_waiter == nullptr) {
                  m_tail_waiter = nullptr;
               }
               ++m_shared_users;

               m_executor->resume(to_resume->m_awaiting_coroutine);
            } while (m_head_waiter != nullptr && !m_head_waiter->m_exclusive);

            // Cannot unlock until the entire set of shared waiters has been traversed.  I think this
            // makes more sense than allocating space for all the shared waiters, unlocking, and then
            // resuming in a batch?
            lk.unlock();
         }
      }
   };

} // namespace coro
