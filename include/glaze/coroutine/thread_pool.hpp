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
#include <coroutine>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <ranges>
#include <thread>
#include <variant>
#include <vector>

#include "glaze/coroutine/concepts.hpp"
#include "glaze/coroutine/event.hpp"
#include "glaze/coroutine/task.hpp"

namespace glz
{
   /**
    * Creates a thread pool that executes arbitrary coroutine tasks in a FIFO scheduler policy.
    * The thread pool by default will create an execution thread per available core on the system.
    *
    * When shutting down, either by the thread pool destructing or by manually calling shutdown()
    * the thread pool will stop accepting new tasks but will complete all tasks that were scheduled
    * prior to the shutdown request.
    */
   struct thread_pool
   {
      /**
       * An operation is an awaitable type with a coroutine to resume the task scheduled on one of
       * the executor threads.
       */
      class operation
      {
         friend struct thread_pool;
         /**
          * Only thread_pools can create operations when a task is being scheduled.
          * @param tp The thread pool that created this operation.
          */
         explicit operation(thread_pool& tp) noexcept : m_thread_pool(tp) {}

        public:
         /**
          * Operations always pause so the executing thread can be switched.
          */
         auto await_ready() noexcept -> bool { return false; }

         /**
          * Suspending always returns to the caller (using void return of await_suspend()) and
          * stores the coroutine internally for the executing thread to resume from.
          */
         void await_suspend(std::coroutine_handle<> awaiting_coroutine) noexcept
         {
            m_awaiting_coroutine = awaiting_coroutine;
            m_thread_pool.schedule_impl(m_awaiting_coroutine);

            // void return on await_suspend suspends the _this_ coroutine, which is now scheduled on the
            // thread pool and returns control to the caller.  They could be sync_wait'ing or go do
            // something else while this coroutine gets picked up by the thread pool.
         }

         /**
          * no-op as this is the function called first by the thread pool's executing thread.
          */
         auto await_resume() noexcept -> void {}

        private:
         /// The thread pool that this operation will execute on.
         thread_pool& m_thread_pool;
         /// The coroutine awaiting execution.
         std::coroutine_handle<> m_awaiting_coroutine{nullptr};
      };

      struct options
      {
         /// The number of executor threads for this thread pool.  Uses the hardware concurrency
         /// value by default.
         uint32_t thread_count = std::thread::hardware_concurrency();
         /// Functor to call on each executor thread upon starting execution.  The parameter is the
         /// thread's ID assigned to it by the thread pool.
         std::function<void(size_t)> on_thread_start_functor = nullptr;
         /// Functor to call on each executor thread upon stopping execution.  The parameter is the
         /// thread's ID assigned to it by the thread pool.
         std::function<void(size_t)> on_thread_stop_functor = nullptr;
      };

      /**
       * @param opts Thread pool configuration options.
       */
      explicit thread_pool(options opts = options{.thread_count = std::thread::hardware_concurrency(),
                                                  .on_thread_start_functor = nullptr,
                                                  .on_thread_stop_functor = nullptr})
         : m_opts(std::move(opts))
      {
         m_threads.reserve(m_opts.thread_count);

         for (uint32_t i = 0; i < m_opts.thread_count; ++i) {
            m_threads.emplace_back([this, i]() { executor(i); });
         }
      }

      thread_pool(const thread_pool&) = delete;
      thread_pool(thread_pool&&) = delete;
      auto operator=(const thread_pool&) -> thread_pool& = delete;
      auto operator=(thread_pool&&) -> thread_pool& = delete;

      virtual ~thread_pool() { shutdown(); }

      /**
       * @return The number of executor threads for processing tasks.
       */
      auto thread_count() const noexcept -> size_t { return m_threads.size(); }

      /**
       * Schedules the currently executing coroutine to be run on this thread pool.  This must be
       * called from within the coroutines function body to schedule the coroutine on the thread pool.
       * @throw std::runtime_error If the thread pool is `shutdown()` scheduling new tasks is not permitted.
       * @return The operation to switch from the calling scheduling thread to the executor thread
       *         pool thread.
       */
      [[nodiscard]] auto schedule() -> operation
      {
         if (!m_shutdown_requested.load(std::memory_order::acquire)) {
            m_size.fetch_add(1, std::memory_order::release);
            return operation{*this};
         }

         GLZ_THROW_OR_ABORT(std::runtime_error("glz::thread_pool is shutting down, unable to schedule new tasks."));
      }

      /**
       * @throw std::runtime_error If the thread pool is `shutdown()` scheduling new tasks is not permitted.
       * @param f The function to execute on the thread pool.
       * @param args The arguments to call the functor with.
       * @return A task that wraps the given functor to be executed on the thread pool.
       */
      template <typename functor, typename... arguments>
      [[nodiscard]] auto schedule(functor&& f, arguments... args) -> task<decltype(f(std::forward<arguments>(args)...))>
      {
         co_await schedule();

         if constexpr (std::is_same_v<void, decltype(f(std::forward<arguments>(args)...))>) {
            f(std::forward<arguments>(args)...);
            co_return;
         }
         else {
            co_return f(std::forward<arguments>(args)...);
         }
      }

      /**
       * Schedules any coroutine handle that is ready to be resumed.
       * @param handle The coroutine handle to schedule.
       * @return True if the coroutine is resumed, false if its a nullptr.
       */
      bool resume(std::coroutine_handle<> handle) noexcept
      {
         if (handle == nullptr) {
            return false;
         }

         if (m_shutdown_requested.load(std::memory_order::acquire)) {
            return false;
         }

         m_size.fetch_add(1, std::memory_order::release);
         schedule_impl(handle);
         return true;
      }

      /**
       * Schedules the set of coroutine handles that are ready to be resumed.
       * @param handles The coroutine handles to schedule.
       * @param uint64_t The number of tasks resumed, if any where null they are discarded.
       */
      template <range_of<std::coroutine_handle<>> range_type>
      uint64_t resume(const range_type& handles) noexcept
      {
         m_size.fetch_add(std::size(handles), std::memory_order::release);

         size_t null_handles{0};

         {
            std::scoped_lock lk{m_wait_mutex};
            for (const auto& handle : handles) {
               if (handle) [[likely]] {
                  m_queue.emplace_back(handle);
               }
               else {
                  ++null_handles;
               }
            }
         }

         if (null_handles > 0) {
            m_size.fetch_sub(null_handles, std::memory_order::release);
         }

         uint64_t total = std::size(handles) - null_handles;
         if (total >= m_threads.size()) {
            m_wait_cv.notify_all();
         }
         else {
            for (uint64_t i = 0; i < total; ++i) {
               m_wait_cv.notify_one();
            }
         }

         return total;
      }

      /**
       * Immediately yields the current task and places it at the end of the queue of tasks waiting
       * to be processed.  This will immediately be picked up again once it naturally goes through the
       * FIFO task queue.  This function is useful to yielding long processing tasks to let other tasks
       * get processing time.
       */
      [[nodiscard]] operation yield() { return schedule(); }

      /**
       * Shutsdown the thread pool.  This will finish any tasks scheduled prior to calling this
       * function but will prevent the thread pool from scheduling any new tasks.  This call is
       * blocking and will wait until all inflight tasks are completed before returnin.
       */
      void shutdown() noexcept
      {
         // Only allow shutdown to occur once.
         if (m_shutdown_requested.exchange(true, std::memory_order::acq_rel) == false) {
            {
               // There is a race condition if we are not holding the lock with the executors
               // to always receive this.  std::jthread stop token works without this properly.
               std::unique_lock<std::mutex> lk{m_wait_mutex};
               m_wait_cv.notify_all();
            }

            for (auto& thread : m_threads) {
               if (thread.joinable()) {
                  thread.join();
               }
            }
         }
      }

      /**
       * @return The number of tasks waiting in the task queue + the executing tasks.
       */
      size_t size() const noexcept { return m_size.load(std::memory_order::acquire); }

      /**
       * @return True if the task queue is empty and zero tasks are currently executing.
       */
      bool empty() const noexcept { return size() == 0; }

      /**
       * @return The number of tasks waiting in the task queue to be executed.
       */
      size_t queue_size() const noexcept
      {
         std::atomic_thread_fence(std::memory_order::acquire);
         return m_queue.size();
      }

      /**
       * @return True if the task queue is currently empty.
       */
      bool queue_empty() const noexcept { return queue_size() == 0; }

     private:
      /// The configuration options.
      options m_opts;
      /// The background executor threads.
      std::vector<std::thread> m_threads;
      /// Mutex for executor threads to sleep on the condition variable.
      std::mutex m_wait_mutex;
      /// Condition variable for each executor thread to wait on when no tasks are available.
      std::condition_variable_any m_wait_cv;
      /// FIFO queue of tasks waiting to be executed.
      std::deque<std::coroutine_handle<>> m_queue;
      /**
       * Each background thread runs from this function.
       * @param idx The executor's idx for internal data structure accesses.
       */
      void executor(size_t idx)
      {
         if (m_opts.on_thread_start_functor) {
            m_opts.on_thread_start_functor(idx);
         }

         // Process until shutdown is requested.
         while (!m_shutdown_requested.load(std::memory_order::acquire)) {
            std::unique_lock<std::mutex> lk{m_wait_mutex};
            m_wait_cv.wait(lk,
                           [&]() { return !m_queue.empty() || m_shutdown_requested.load(std::memory_order::acquire); });

            if (m_queue.empty()) {
               continue;
            }

            auto handle = m_queue.front();
            m_queue.pop_front();
            lk.unlock();

            // Release the lock while executing the coroutine.
            handle.resume();
            m_size.fetch_sub(1, std::memory_order::release);
         }

         // Process until there are no ready tasks left.
         while (m_size.load(std::memory_order::acquire) > 0) {
            std::unique_lock<std::mutex> lk{m_wait_mutex};
            // m_size will only drop to zero once all executing coroutines are finished
            // but the queue could be empty for threads that finished early.
            if (m_queue.empty()) {
               break;
            }

            auto handle = m_queue.front();
            m_queue.pop_front();
            lk.unlock();

            // Release the lock while executing the coroutine.
            handle.resume();
            m_size.fetch_sub(1, std::memory_order::release);
         }

         if (m_opts.on_thread_stop_functor) {
            m_opts.on_thread_stop_functor(idx);
         }
      }
      /**
       * @param handle Schedules the given coroutine to be executed upon the first available thread.
       */
      void schedule_impl(std::coroutine_handle<> handle) noexcept
      {
         if (handle == nullptr) {
            return;
         }

         {
            std::scoped_lock lk{m_wait_mutex};
            m_queue.emplace_back(handle);
            m_wait_cv.notify_one();
         }
      }

      /// The number of tasks in the queue + currently executing.
      std::atomic<size_t> m_size{0};
      /// Has the thread pool been requested to shut down?
      std::atomic<bool> m_shutdown_requested{false};
   };

}
