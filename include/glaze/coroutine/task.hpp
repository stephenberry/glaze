// Glaze Library
// For the license information refer to glaze.hpp

// Modified from the awesome: https://github.com/jbaldwin/libcoro

#pragma once

#include <coroutine>
#include <exception>
#include <stdexcept>
#include <utility>
#include <variant>

#ifndef GLZ_THROW_OR_ABORT
#if __cpp_exceptions
#define GLZ_THROW_OR_ABORT(EXC) (throw(EXC))
#else
#define GLZ_THROW_OR_ABORT(EXC) (std::abort())
#endif
#endif

namespace glz
{
   template <class Return = void>
   struct task;

   namespace detail
   {
      struct promise_base
      {
         friend struct final_awaitable;
         struct final_awaitable
         {
            bool await_ready() const noexcept { return false; }

            template <class promise_type>
            auto await_suspend(std::coroutine_handle<promise_type> coroutine) noexcept -> std::coroutine_handle<>
            {
               // If there is a continuation call it, otherwise this is the end of the line.
               auto& promise = coroutine.promise();
               if (promise.m_continuation) {
                  return promise.m_continuation;
               }
               else {
                  return std::noop_coroutine();
               }
            }

            void await_resume() noexcept
            {
               // no-op
            }
         };

         promise_base() noexcept = default;
         ~promise_base() = default;

         auto initial_suspend() noexcept { return std::suspend_always{}; }

         auto final_suspend() noexcept { return final_awaitable{}; }

         auto continuation(std::coroutine_handle<> continuation) noexcept -> void { m_continuation = continuation; }

        protected:
         std::coroutine_handle<> m_continuation{};
      };

      template <class Return>
      struct promise final : public promise_base
      {
         using task_type = task<Return>;
         using coroutine_handle = std::coroutine_handle<promise<Return>>;
         static constexpr bool return_type_is_reference = std::is_reference_v<Return>;
         using stored_type = std::conditional_t<return_type_is_reference, std::remove_reference_t<Return>*,
                                                std::remove_const_t<Return>>;
         using variant_type = std::variant<std::monostate, stored_type, std::exception_ptr>;

         promise() noexcept {}
         promise(const promise&) = delete;
         promise(promise&& other) = delete;
         promise& operator=(const promise&) = delete;
         promise& operator=(promise&& other) = delete;
         ~promise() = default;

         auto get_return_object() noexcept -> task_type;

         template <class T>
            requires(return_type_is_reference and std::is_constructible_v<Return, T&&>) or
                    (not return_type_is_reference and std::is_constructible_v<stored_type, T&&>)
         void return_value(T&& value)
         {
            if constexpr (return_type_is_reference) {
               Return ref = static_cast<T&&>(value);
               m_storage.template emplace<stored_type>(std::addressof(ref));
            }
            else {
               m_storage.template emplace<stored_type>(std::forward<T>(value));
            }
         }

         void return_value(stored_type value)
            requires(not return_type_is_reference)
         {
            if constexpr (std::is_move_constructible_v<stored_type>) {
               m_storage.template emplace<stored_type>(std::move(value));
            }
            else {
               m_storage.template emplace<stored_type>(value);
            }
         }

         void unhandled_exception() noexcept { new (&m_storage) variant_type(std::current_exception()); }

         decltype(auto) result() &
         {
            if (std::holds_alternative<stored_type>(m_storage)) {
               if constexpr (return_type_is_reference) {
                  return static_cast<Return>(*std::get<stored_type>(m_storage));
               }
               else {
                  return static_cast<const Return&>(std::get<stored_type>(m_storage));
               }
            }
            else if (std::holds_alternative<std::exception_ptr>(m_storage)) {
               std::rethrow_exception(std::get<std::exception_ptr>(m_storage));
            }
            else {
               GLZ_THROW_OR_ABORT(std::runtime_error{"The return value was never set, did you execute the coroutine?"});
            }
         }

         decltype(auto) result() const&
         {
            if (std::holds_alternative<stored_type>(m_storage)) {
               if constexpr (return_type_is_reference) {
                  return static_cast<std::add_const_t<Return>>(*std::get<stored_type>(m_storage));
               }
               else {
                  return static_cast<const Return&>(std::get<stored_type>(m_storage));
               }
            }
            else if (std::holds_alternative<std::exception_ptr>(m_storage)) {
               std::rethrow_exception(std::get<std::exception_ptr>(m_storage));
            }
            else {
               GLZ_THROW_OR_ABORT(std::runtime_error{"The return value was never set, did you execute the coroutine?"});
            }
         }

         decltype(auto) result() &&
         {
            if (std::holds_alternative<stored_type>(m_storage)) {
               if constexpr (return_type_is_reference) {
                  return static_cast<Return>(*std::get<stored_type>(m_storage));
               }
               else if constexpr (std::is_move_constructible_v<Return>) {
                  return static_cast<Return&&>(std::get<stored_type>(m_storage));
               }
               else {
                  return static_cast<const Return&&>(std::get<stored_type>(m_storage));
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
      struct promise<void> : public promise_base
      {
         using task_type = task<void>;
         using coroutine_handle = std::coroutine_handle<promise<void>>;

         promise() noexcept = default;
         promise(const promise&) = delete;
         promise(promise&& other) = delete;
         promise& operator=(const promise&) = delete;
         promise& operator=(promise&& other) = delete;
         ~promise() = default;

         auto get_return_object() noexcept -> task_type;

         auto return_void() noexcept -> void {}

         auto unhandled_exception() noexcept -> void { m_exception_ptr = std::current_exception(); }

         auto result() -> void
         {
            if (m_exception_ptr) {
               std::rethrow_exception(m_exception_ptr);
            }
         }

        private:
         std::exception_ptr m_exception_ptr{};
      };

   } // namespace detail

   template <class Return>
   struct [[nodiscard]] task
   {
      using task_type = task<Return>;
      using promise_type = detail::promise<Return>;
      using coroutine_handle = std::coroutine_handle<promise_type>;

      struct awaitable_base
      {
         awaitable_base(coroutine_handle coroutine) noexcept : m_coroutine(coroutine) {}

         bool await_ready() const noexcept { return !m_coroutine || m_coroutine.done(); }

         auto await_suspend(std::coroutine_handle<> awaiting_coroutine) noexcept -> std::coroutine_handle<>
         {
            m_coroutine.promise().continuation(awaiting_coroutine);
            return m_coroutine;
         }

         std::coroutine_handle<promise_type> m_coroutine{};
      };

      task() noexcept : m_coroutine(nullptr) {}

      explicit task(coroutine_handle handle) : m_coroutine(handle) {}
      task(const task&) = delete;
      task(task&& other) noexcept : m_coroutine(std::exchange(other.m_coroutine, nullptr)) {}

      ~task()
      {
         if (m_coroutine) {
            m_coroutine.destroy();
         }
      }

      auto operator=(const task&) -> task& = delete;

      auto operator=(task&& other) noexcept -> task&
      {
         if (std::addressof(other) != this) {
            if (m_coroutine) {
               m_coroutine.destroy();
            }

            m_coroutine = std::exchange(other.m_coroutine, nullptr);
         }

         return *this;
      }

      /**
       * @return True if the task is in its final suspend or if the task has been destroyed.
       */
      bool is_ready() const noexcept { return m_coroutine == nullptr || m_coroutine.done(); }

      bool resume()
      {
         if (!m_coroutine.done()) {
            m_coroutine.resume();
         }
         return !m_coroutine.done();
      }

      bool destroy()
      {
         if (m_coroutine) {
            m_coroutine.destroy();
            m_coroutine = nullptr;
            return true;
         }

         return false;
      }

      auto operator co_await() const& noexcept
      {
         struct awaitable : public awaitable_base
         {
            auto await_resume() -> decltype(auto) { return this->m_coroutine.promise().result(); }
         };

         return awaitable{m_coroutine};
      }

      auto operator co_await() const&& noexcept
      {
         struct awaitable : public awaitable_base
         {
            auto await_resume() -> decltype(auto) { return std::move(this->m_coroutine.promise()).result(); }
         };

         return awaitable{m_coroutine};
      }

      auto promise() & -> promise_type& { return m_coroutine.promise(); }
      auto promise() const& -> const promise_type& { return m_coroutine.promise(); }
      auto promise() && -> promise_type&& { return std::move(m_coroutine.promise()); }

      auto handle() -> coroutine_handle { return m_coroutine; }

     private:
      coroutine_handle m_coroutine{};
   };

   namespace detail
   {
      template <class Return>
      task<Return> promise<Return>::get_return_object() noexcept
      {
         return task<Return>{coroutine_handle::from_promise(*this)};
      }

      inline task<> promise<void>::get_return_object() noexcept
      {
         return task<>{coroutine_handle::from_promise(*this)};
      }
   }
}
