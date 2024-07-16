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
#include <condition_variable>
#include <mutex>
#include <variant>

#include "glaze/coroutine/awaitable.hpp"

namespace glz
{
   namespace detail
   {
      struct sync_wait_event
      {
         sync_wait_event(bool initially_set = false) : m_set(initially_set) {}
         sync_wait_event(const sync_wait_event&) = delete;
         sync_wait_event(sync_wait_event&&) = delete;
         auto operator=(const sync_wait_event&) -> sync_wait_event& = delete;
         auto operator=(sync_wait_event&&) -> sync_wait_event& = delete;
         ~sync_wait_event() = default;

         void set() noexcept {
            m_set.exchange(true, std::memory_order::release);
            m_cv.notify_all();
         }
         
         void reset() noexcept {
            m_set.exchange(false, std::memory_order::release);
         }
         
         void wait() noexcept {
            std::unique_lock<std::mutex> lk{m_mutex};
            m_cv.wait(lk, [this] { return m_set.load(std::memory_order::acquire); });
         }

        private:
         std::mutex m_mutex;
         std::condition_variable m_cv;
         std::atomic<bool> m_set{false};
      };

      struct sync_wait_task_promise_base
      {
         sync_wait_task_promise_base() noexcept = default;

         auto initial_suspend() noexcept -> std::suspend_always { return {}; }

         auto unhandled_exception() -> void { m_exception = std::current_exception(); }

        protected:
         sync_wait_event* m_event{nullptr};
         std::exception_ptr m_exception;

         ~sync_wait_task_promise_base() = default;
      };

      template <class return_type>
      struct sync_wait_task_promise : public sync_wait_task_promise_base
      {
         using coroutine_type = std::coroutine_handle<sync_wait_task_promise<return_type>>;

         static constexpr bool return_type_is_reference = std::is_reference_v<return_type>;
         using stored_type = std::conditional_t<return_type_is_reference, std::remove_reference_t<return_type>*,
                                                std::remove_const_t<return_type>>;
         using variant_type = std::variant<std::monostate, stored_type, std::exception_ptr>;

         sync_wait_task_promise() noexcept = default;
         sync_wait_task_promise(const sync_wait_task_promise&) = delete;
         sync_wait_task_promise(sync_wait_task_promise&&) = delete;
         auto operator=(const sync_wait_task_promise&) -> sync_wait_task_promise& = delete;
         auto operator=(sync_wait_task_promise&&) -> sync_wait_task_promise& = delete;
         ~sync_wait_task_promise() = default;

         auto start(sync_wait_event& event)
         {
            m_event = &event;
            coroutine_type::from_promise(*this).resume();
         }

         auto get_return_object() noexcept { return coroutine_type::from_promise(*this); }

         template <typename value_type>
            requires(return_type_is_reference and std::is_constructible_v<return_type, value_type &&>) or
                    (not return_type_is_reference and std::is_constructible_v<stored_type, value_type &&>)
         auto return_value(value_type&& value) -> void
         {
            if constexpr (return_type_is_reference) {
               return_type ref = static_cast<value_type&&>(value);
               m_storage.template emplace<stored_type>(std::addressof(ref));
            }
            else {
               m_storage.template emplace<stored_type>(std::forward<value_type>(value));
            }
         }

         auto return_value(stored_type value) -> void
            requires(not return_type_is_reference)
         {
            if constexpr (std::is_move_constructible_v<stored_type>) {
               m_storage.template emplace<stored_type>(std::move(value));
            }
            else {
               m_storage.template emplace<stored_type>(value);
            }
         }

         auto final_suspend() noexcept
         {
            struct completion_notifier
            {
               auto await_ready() const noexcept { return false; }
               auto await_suspend(coroutine_type coroutine) const noexcept { coroutine.promise().m_event->set(); }
               auto await_resume() noexcept {};
            };

            return completion_notifier{};
         }

         auto result() & -> decltype(auto)
         {
            if (std::holds_alternative<stored_type>(m_storage)) {
               if constexpr (return_type_is_reference) {
                  return static_cast<return_type>(*std::get<stored_type>(m_storage));
               }
               else {
                  return static_cast<const return_type&>(std::get<stored_type>(m_storage));
               }
            }
            else if (std::holds_alternative<std::exception_ptr>(m_storage)) {
               std::rethrow_exception(std::get<std::exception_ptr>(m_storage));
            }
            else {
               GLZ_THROW_OR_ABORT(std::runtime_error{"The return value was never set, did you execute the coroutine?"});
            }
         }

         auto result() const& -> decltype(auto)
         {
            if (std::holds_alternative<stored_type>(m_storage)) {
               if constexpr (return_type_is_reference) {
                  return static_cast<std::add_const_t<return_type>>(*std::get<stored_type>(m_storage));
               }
               else {
                  return static_cast<const return_type&>(std::get<stored_type>(m_storage));
               }
            }
            else if (std::holds_alternative<std::exception_ptr>(m_storage)) {
               std::rethrow_exception(std::get<std::exception_ptr>(m_storage));
            }
            else {
               GLZ_THROW_OR_ABORT(std::runtime_error{"The return value was never set, did you execute the coroutine?"});
            }
         }

         auto result() && -> decltype(auto)
         {
            if (std::holds_alternative<stored_type>(m_storage)) {
               if constexpr (return_type_is_reference) {
                  return static_cast<return_type>(*std::get<stored_type>(m_storage));
               }
               else if constexpr (std::is_assignable_v<return_type, stored_type>) {
                  return static_cast<return_type&&>(std::get<stored_type>(m_storage));
               }
               else {
                  return static_cast<const return_type&&>(std::get<stored_type>(m_storage));
               }
            }
            else if (std::holds_alternative<std::exception_ptr>(m_storage)) {
               std::rethrow_exception(std::get<std::exception_ptr>(m_storage));
            }
            else {
               GLZ_THROW_OR_ABORT(std::runtime_error{"The return value was never set, did you execute the coroutine?"});
            }
         }

        private:
         variant_type m_storage{};
      };

      template <>
      struct sync_wait_task_promise<void> : public sync_wait_task_promise_base
      {
         using coroutine_type = std::coroutine_handle<sync_wait_task_promise<void>>;

         sync_wait_task_promise() noexcept = default;
         ~sync_wait_task_promise() = default;

         auto start(sync_wait_event& event)
         {
            m_event = &event;
            coroutine_type::from_promise(*this).resume();
         }

         auto get_return_object() noexcept { return coroutine_type::from_promise(*this); }

         auto final_suspend() noexcept
         {
            struct completion_notifier
            {
               auto await_ready() const noexcept { return false; }
               auto await_suspend(coroutine_type coroutine) const noexcept { coroutine.promise().m_event->set(); }
               auto await_resume() noexcept {};
            };

            return completion_notifier{};
         }

         auto return_void() noexcept -> void {}

         auto result() -> void
         {
            if (m_exception) {
               std::rethrow_exception(m_exception);
            }
         }
      };

      template <typename return_type>
      struct sync_wait_task
      {
         using promise_type = sync_wait_task_promise<return_type>;
         using coroutine_type = std::coroutine_handle<promise_type>;

         sync_wait_task(coroutine_type coroutine) noexcept : m_coroutine(coroutine) {}

         sync_wait_task(const sync_wait_task&) = delete;
         sync_wait_task(sync_wait_task&& other) noexcept
            : m_coroutine(std::exchange(other.m_coroutine, coroutine_type{}))
         {}
         auto operator=(const sync_wait_task&) -> sync_wait_task& = delete;
         auto operator=(sync_wait_task&& other) -> sync_wait_task&
         {
            if (std::addressof(other) != this) {
               m_coroutine = std::exchange(other.m_coroutine, coroutine_type{});
            }

            return *this;
         }

         ~sync_wait_task()
         {
            if (m_coroutine) {
               m_coroutine.destroy();
            }
         }

         auto promise() & -> promise_type& { return m_coroutine.promise(); }
         auto promise() const& -> const promise_type& { return m_coroutine.promise(); }
         auto promise() && -> promise_type&& { return std::move(m_coroutine.promise()); }

        private:
         coroutine_type m_coroutine;
      };

      template <awaitable awaitable_type,
                class return_type = awaitable_traits<awaitable_type>::return_type>
      static auto make_sync_wait_task(awaitable_type&& a) -> sync_wait_task<return_type>;

      template <awaitable awaitable_type, class return_type>
      static auto make_sync_wait_task(awaitable_type&& a) -> sync_wait_task<return_type>
      {
         if constexpr (std::is_void_v<return_type>) {
            co_await std::forward<awaitable_type>(a);
            co_return;
         }
         else {
            co_return co_await std::forward<awaitable_type>(a);
         }
      }
   }

   template <awaitable awaitable_type,
             class return_type = typename awaitable_traits<awaitable_type>::return_type>
   auto sync_wait(awaitable_type&& a) -> decltype(auto)
   {
      detail::sync_wait_event e{};
      auto task = detail::make_sync_wait_task(std::forward<awaitable_type>(a));
      task.promise().start(e);
      e.wait();

      if constexpr (std::is_void_v<return_type>) {
         task.promise().result();
         return;
      }
      else if constexpr (std::is_reference_v<return_type>) {
         return task.promise().result();
      }
      else if constexpr (std::is_move_assignable_v<return_type>) {
         // issue-242
         // For non-trivial types (or possibly types that don't fit in a register)
         // the compiler will end up calling the ~return_type() when the promise
         // is destructed at the end of sync_wait(). This causes the return_type
         // object to also be destructed causingn the final return/move from
         // sync_wait() to be a 'use after free' bug. To work around this the result
         // must be moved off the promise object before the promise is destructed.
         // Other solutions could be heap allocating the return_type but that has
         // other downsides, for now it is determined that a double move is an
         // acceptable solution to work around this bug.
         auto result = std::move(task).promise().result();
         return result;
      }
      else {
         return task.promise().result();
      }
   }
}
