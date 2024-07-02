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
#include <iostream>
#include <list>
#include <memory>
#include <mutex>
#include <queue>
#include <vector>

#include "glaze/coroutine/concepts.hpp"
#include "glaze/coroutine/task.hpp"

namespace glz
{
   struct scheduler;

   template <executor executor_type>
   struct task_container
   {
      struct options
      {
         /// The number of task spots to reserve space for upon creating the container.
         size_t reserve_size{8};
         /// The growth factor for task space in the container when capacity is full.
         double growth_factor{2};
      };

      /**
       * @param e Tasks started in the container are scheduled onto this executor.  For tasks created
       *           from a coro::scheduler, this would usually be that coro::scheduler instance.
       * @param opts Task container options.
       */
      task_container(std::shared_ptr<executor_type> e,
                     const options opts = options{.reserve_size = 8, .growth_factor = 2})
         : m_growth_factor(opts.growth_factor), m_executor(std::move(e)), m_executor_ptr(m_executor.get())
      {
         if (m_executor == nullptr) {
            GLZ_THROW_OR_ABORT(std::runtime_error{"task_container cannot have a nullptr executor"});
         }

         init(opts.reserve_size);
      }
      task_container(const task_container&) = delete;
      task_container(task_container&&) = delete;
      auto operator=(const task_container&) -> task_container& = delete;
      auto operator=(task_container&&) -> task_container& = delete;
      ~task_container()
      {
         // This will hang the current thread.. but if tasks are not complete thats also pretty bad.
         while (!empty()) {
            garbage_collect();
         }
      }

      enum struct garbage_collect_t {
         /// Execute garbage collection.
         yes,
         /// Do not execute garbage collection.
         no
      };

      /**
       * Stores a user task and starts its execution on the container's thread pool.
       * @param user_task The scheduled user's task to store in this task container and start its execution.
       * @param cleanup Should the task container run garbage collect at the beginning of this store
       *                call?  Calling at regular intervals will reduce memory usage of completed
       *                tasks and allow for the task container to re-use allocated space.
       */
      auto start(glz::task<void>&& user_task, garbage_collect_t cleanup = garbage_collect_t::yes) -> void
      {
         m_size.fetch_add(1, std::memory_order::relaxed);

         std::unique_lock lk{m_mutex};

         if (cleanup == garbage_collect_t::yes) {
            gc_internal();
         }

         // Only grow if completely full and attempting to add more.
         if (m_free_task_indices.empty()) {
            grow();
         }

         // Reserve a free task index
         size_t index = m_free_task_indices.front();
         m_free_task_indices.pop();

         // We've reserved the slot, we can release the lock.
         lk.unlock();

         // Store the task inside a cleanup task for self deletion.
         m_tasks[index] = make_cleanup_task(std::move(user_task), index);

         // Start executing from the cleanup task to schedule the user's task onto the thread pool.
         m_tasks[index].resume();
      }

      /**
       * Garbage collects any tasks that are marked as deleted.  This frees up space to be re-used by
       * the task container for newly stored tasks.
       * @return The number of tasks that were deleted.
       */
      auto garbage_collect() -> size_t
      {
         std::scoped_lock lk{m_mutex};
         return gc_internal();
      }

      /**
       * @return The number of active tasks in the container.
       */
      auto size() const -> size_t { return m_size.load(std::memory_order::relaxed); }

      /**
       * @return True if there are no active tasks in the container.
       */
      auto empty() const -> bool { return size() == 0; }

      /**
       * @return The capacity of this task manager before it will need to grow in size.
       */
      auto capacity() const -> size_t
      {
         std::atomic_thread_fence(std::memory_order::acquire);
         return m_tasks.size();
      }

      /**
       * Will continue to garbage collect and yield until all tasks are complete.  This method can be
       * co_await'ed to make it easier to wait for the task container to have all its tasks complete.
       *
       * This does not shut down the task container, but can be used when shutting down, or if your
       * logic requires all the tasks contained within to complete, it is similar to coro::latch.
       */
      auto garbage_collect_and_yield_until_empty() -> glz::task<void>
      {
         while (!empty()) {
            garbage_collect();
            co_await m_executor_ptr->yield();
         }
      }

     private:
      /**
       * Grows each task container by the growth factor.
       * @return The position of the free index after growing.
       */
      auto grow() -> void
      {
         // Save an index at the current last item.
         size_t new_size = m_tasks.size() * m_growth_factor;
         for (size_t i = m_tasks.size(); i < new_size; ++i) {
            m_free_task_indices.emplace(i);
         }
         m_tasks.resize(new_size);
      }

      /**
       * Internal GC call, expects the public function to lock.
       */
      auto gc_internal() -> size_t
      {
         size_t deleted{0};
         auto pos = std::begin(m_tasks_to_delete);
         while (pos != std::end(m_tasks_to_delete)) {
            // Skip tasks that are still running or have yet to start.
            if (!m_tasks[*pos].is_ready()) {
               pos++;
               continue;
            }
            // Destroy the cleanup task and the user task.
            m_tasks[*pos].destroy();
            // Put the deleted position at the end of the free indexes list.
            m_free_task_indices.emplace(*pos);
            // Remove index from tasks to delete
            m_tasks_to_delete.erase(pos++);
            // Indicate a task was deleted.
            ++deleted;
         }
         m_size.fetch_sub(deleted, std::memory_order::relaxed);
         return deleted;
      }

      /**
       * Encapsulate the users tasks in a cleanup task which marks itself for deletion upon
       * completion.  Simply co_await the users task until its completed and then mark the given
       * position within the task manager as being deletable.  The scheduler's next iteration
       * in its event loop will then free that position up to be re-used.
       *
       * This function will also unconditionally catch all unhandled exceptions by the user's
       * task to prevent the scheduler from throwing exceptions.
       * @param user_task The user's task.
       * @param index The index where the task data will be stored in the task manager.
       * @return The user's task wrapped in a self cleanup task.
       */
      auto make_cleanup_task(task<void> user_task, size_t index) -> glz::task<void>
      {
         // Immediately move the task onto the executor.
         co_await m_executor_ptr->schedule();

#if __cpp_exceptions
         try {
            // Await the users task to complete.
            co_await user_task;
         }
         catch (const std::exception& e) {
            // TODO: what would be a good way to report this to the user...?  Catching here is required
            // since the co_await will unwrap the unhandled exception on the task.
            // The user's task should ideally be wrapped in a catch all and handle it themselves, but
            // that cannot be guaranteed.
            std::cerr << "coro::task_container user_task had an unhandled exception e.what()= " << e.what() << "\n";
         }
         catch (...) {
            // don't crash if they throw something that isn't derived from std::exception
            std::cerr << "coro::task_container user_task had unhandle exception, not derived from std::exception.\n";
         }
#else
         co_await user_task;
#endif

         {
            // This scope is required around this lock otherwise if this task on destruction schedules a new task it
            // can cause a deadlock, notably tls::client schedules a task to cleanup tls resources.
            std::scoped_lock lk{m_mutex};
            m_tasks_to_delete.emplace_back(index);
         }

         co_return;
      }

      /// Mutex for safely mutating the task containers across threads, expected usage is within
      /// thread pools for indeterminate lifetime requests.
      std::mutex m_mutex{};
      /// The number of alive tasks.
      std::atomic<size_t> m_size{};
      /// Maintains the lifetime of the tasks until they are completed.
      std::vector<task<void>> m_tasks{};
      /// The full set of free indicies into `m_tasks`.
      std::queue<size_t> m_free_task_indices{};
      /// The set of tasks that have completed and need to be deleted.
      std::list<size_t> m_tasks_to_delete{};
      /// The amount to grow the containers by when all spaces are taken.
      double m_growth_factor{};
      /// The executor to schedule tasks that have just started.
      std::shared_ptr<executor_type> m_executor{nullptr};
      /// This is used internally since scheduler cannot pass itself in as a shared_ptr.
      executor_type* m_executor_ptr{nullptr};

      /**
       * Special constructor for internal types to create their embeded task containers.
       */

      friend scheduler;
      task_container(executor_type& e, const options opts = options{.reserve_size = 8, .growth_factor = 2})
         : m_growth_factor(opts.growth_factor), m_executor_ptr(&e)
      {
         init(opts.reserve_size);
      }

      auto init(size_t reserve_size) -> void
      {
         m_tasks.resize(reserve_size);
         for (size_t i = 0; i < reserve_size; ++i) {
            m_free_task_indices.emplace(i);
         }
      }
   };
}
