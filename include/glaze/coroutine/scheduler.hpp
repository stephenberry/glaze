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

#include "glaze/coroutine/poll.hpp"
#include "glaze/coroutine/poll_info.hpp"
#include "glaze/coroutine/task_container.hpp"
#include "glaze/coroutine/thread_pool.hpp"
#include "glaze/network/core.hpp"
#include "glaze/network/socket.hpp"

#include <array>
#include <chrono>
#include <cstring>
#include <functional>
#include <memory>
#include <optional>
#include <thread>
#include <vector>

namespace glz
{
   enum struct thread_strategy {
      /// Spawns a dedicated background thread for the scheduler to run on.
      spawn,
      /// Requires the user to call process_events() to drive the scheduler.
      manual
   };
   
   struct scheduler final
   {
      using clock = std::chrono::steady_clock;
      using time_point = clock::time_point;
      using timed_events = poll_info::timed_events;

      enum struct execution_strategy {
         /// Tasks will be FIFO queued to be executed on a thread pool.  This is better for tasks that
         /// are long lived and will use lots of CPU because long lived tasks will block other i/o
         /// operations while they complete.  This strategy is generally better for lower latency
         /// requirements at the cost of throughput.
         process_tasks_on_thread_pool,
         /// Tasks will be executed inline on the io scheduler thread.  This is better for short tasks
         /// that can be quickly processed and not block other i/o operations for very long.  This
         /// strategy is generally better for higher throughput at the cost of latency.
         process_tasks_inline
      };

      struct options
      {
         /// Should the io scheduler spawn a dedicated event processor?
         glz::thread_strategy thread_strategy{glz::thread_strategy::spawn};
         /// If spawning a dedicated event processor a functor to call upon that thread starting.
         std::function<void()> on_io_thread_start_functor{};
         /// If spawning a dedicated event processor a functor to call upon that thread stopping.
         std::function<void()> on_io_thread_stop_functor{};
         /// Thread pool options for the task processor threads.  See thread pool for more details.
         thread_pool::options pool{
            .thread_count = ((std::thread::hardware_concurrency() > 1) ? (std::thread::hardware_concurrency() - 1) : 1),
            .on_thread_start_functor = nullptr,
            .on_thread_stop_functor = nullptr};

         /// If inline task processing is enabled then the io worker will resume tasks on its thread
         /// rather than scheduling them to be picked up by the thread pool.
         const execution_strategy execution_strategy{execution_strategy::process_tasks_on_thread_pool};
      };

      scheduler() { init(); }

      scheduler(options opts) : opts(std::move(opts)) { init(); }

      scheduler(const scheduler&) = delete;
      scheduler(scheduler&&) = delete;
      scheduler& operator=(const scheduler&) = delete;
      scheduler& operator=(scheduler&&) = delete;

      ~scheduler()
      {
         shutdown();

         if (m_io_thread.joinable()) {
            m_io_thread.join();
         }

#if defined(__linux__)
         glz::net::close_socket(event_fd);
         glz::net::close_socket(timer_fd);
         glz::net::close_socket(schedule_fd);

#elif defined(_WIN32)
         glz::net::close_event(event_fd);
         glz::net::close_event(timer_fd);
         glz::net::close_event(schedule_fd);
#endif
      }

      /**
       * Given a thread_strategy_t::manual this function should be called at regular intervals to
       * process events that are ready.  If a using thread_strategy_t::spawn this is run continously
       * on a dedicated background thread and does not need to be manually invoked.
       * @param timeout If no events are ready how long should the function wait for events to be ready?
       *                Passing zero (default) for the timeout will check for any events that are
       *                ready now, and then return.  This could be zero events.  Passing -1 means block
       *                indefinitely until an event happens.
       * @param return The number of tasks currently executing or waiting to execute.
       */
      size_t process_events(std::chrono::milliseconds timeout = std::chrono::milliseconds{0})
      {
         process_events_manual(timeout);
         return size();
      }

      struct schedule_operation
      {
         /// The thread pool that this operation will execute on.
         glz::scheduler& scheduler;

         /**
          * Operations always pause so the executing thread can be switched.
          */
         bool await_ready() noexcept { return false; }

         /**
          * Suspending always returns to the caller (using void return of await_suspend()) and
          * stores the coroutine internally for the executing thread to resume from.
          */
         void await_suspend(std::coroutine_handle<> awaiting_coroutine) noexcept
         {
            if (scheduler.opts.execution_strategy == execution_strategy::process_tasks_inline) {
               scheduler.n_active_tasks.fetch_add(1, std::memory_order::release);
               {
                  std::scoped_lock lk{scheduler.m_scheduled_tasks_mutex};
                  scheduler.m_scheduled_tasks.emplace_back(awaiting_coroutine);
               }

               // Trigger the event to wake-up the scheduler if this event isn't currently triggered.
               bool expected{false};
               if (scheduler.m_schedule_fd_triggered.compare_exchange_strong(
                      expected, true, std::memory_order::release, std::memory_order::relaxed)) {
#if defined(__linux__)
                  eventfd_t value{1};
                  eventfd_write(scheduler.schedule_fd, value);
#elif defined(__APPLE__)
                  net::poll_event_t e{
                     .filter = EVFILT_USER, .fflags = NOTE_TRIGGER, .udata = const_cast<void*>(m_schedule_ptr)};
                  if (::kevent(scheduler.event_fd, &e, 1, nullptr, 0, nullptr) == -1) {
                     GLZ_THROW_OR_ABORT(std::runtime_error("Failed to trigger wke up"));
                  }
#endif
               }
            }
            else {
               scheduler.m_thread_pool->resume(awaiting_coroutine);
            }
         }

         /**
          * no-op as this is the function called first by the thread pool's executing thread.
          */
         void await_resume() noexcept {}
      };

      /**
       * Schedules the current task onto this scheduler for execution.
       */
      schedule_operation schedule() { return {*this}; }

      /**
       * Schedules the current task to run after the given amount of time has elapsed.
       * @param amount The amount of time to wait before resuming execution of this task.
       *               Given zero or negative amount of time this behaves identical to schedule().
       */
      [[nodiscard]] auto schedule_after(std::chrono::milliseconds amount) -> glz::task<void>
      {
         return yield_for(amount);
      }

      /**
       * Schedules the current task to run at a given time point in the future.
       * @param time The time point to resume execution of this task.  Given 'now' or a time point
       *             in the past this behaves identical to schedule().
       */
      [[nodiscard]] auto schedule_at(time_point time) -> glz::task<void> { return yield_until(time); }

      /**
       * Yields the current task to the end of the queue of waiting tasks.
       */
      [[nodiscard]] auto yield() -> schedule_operation { return schedule_operation{*this}; };

      /**
       * Yields the current task for the given amount of time.
       * @param amount The amount of time to yield for before resuming executino of this task.
       *               Given zero or negative amount of time this behaves identical to yield().
       */
      [[nodiscard]] auto yield_for(std::chrono::milliseconds amount) -> glz::task<void>
      {
         if (amount <= std::chrono::milliseconds(0)) {
            co_await schedule();
         }
         else {
            // Yield/timeout tasks are considered live in the scheduler and must be accounted for. Note
            // that if the user gives an invalid amount and schedule() is directly called it will account
            // for the scheduled task there.
            n_active_tasks.fetch_add(1, std::memory_order::release);

            // Yielding does not requiring setting the timer position on the poll info since
            // it doesn't have a corresponding 'event' that can trigger, it always waits for
            // the timeout to occur before resuming.

            poll_info pi{};
            add_timer_token(clock::now() + amount, pi);
            co_await pi;

            n_active_tasks.fetch_sub(1, std::memory_order::release);
         }
         co_return;
      }

      /**
       * Yields the current task until the given time point in the future.
       * @param time The time point to resume execution of this task.  Given 'now' or a time point in the
       *             in the past this behaves identical to yield().
       */
      [[nodiscard]] auto yield_until(time_point time) -> glz::task<void>
      {
         auto now = clock::now();

         // If the requested time is in the past (or now!) bail out!
         if (time <= now) {
            co_await schedule();
         }
         else {
            n_active_tasks.fetch_add(1, std::memory_order::release);

            auto amount = std::chrono::duration_cast<std::chrono::milliseconds>(time - now);

            poll_info pi{};
            add_timer_token(now + amount, pi);
            co_await pi;

            n_active_tasks.fetch_sub(1, std::memory_order::release);
         }
         co_return;
      }

      /**
       * Polls the given file descriptor for the given operations.
       * @param fd The file descriptor to poll for events.
       * @param op The operations to poll for.
       * @param timeout The amount of time to wait for the events to trigger.  A timeout of zero will
       *                block indefinitely until the event triggers.
       * @return The result of the poll operation.
       */
      [[nodiscard]] task<poll_status> poll(net::event_handle_t fd, glz::poll_op op,
                                                std::chrono::milliseconds timeout = std::chrono::milliseconds{0})
      {
         // Because the size will drop when this coroutine suspends every poll needs to undo the subtraction
         // on the number of active tasks in the scheduler.  When this task is resumed by the event loop.
         n_active_tasks.fetch_add(1, std::memory_order::release);

         // Setup two events, a timeout event and the actual poll for op event.
         // Whichever triggers first will delete the other to guarantee only one wins.
         // The resume token will be set by the scheduler to what the event turned out to be.

         bool timeout_requested = (timeout > std::chrono::milliseconds(0));

         glz::poll_info poll_info{};
         poll_info.m_fd = fd;
         poll_info.op = op;

         if (timeout_requested) {
            poll_info.m_timer_pos = add_timer_token(clock::now() + timeout, poll_info);
         }

#if defined(__linux__)
         net::poll_event_t e{};
         
         switch (op)
         {
            case poll_op::read: {
               e.events = EPOLLIN | EPOLLONESHOT | EPOLLRDHUP;
               break;
            }
            case poll_op::write: {
               e.events = EPOLLOUT | EPOLLONESHOT | EPOLLRDHUP;
               break;
            }
            case poll_op::read_write: {
               e.events = EPOLLIN | EPOLLOUT | EPOLLONESHOT | EPOLLRDHUP;
               break;
            }
            default {
               break;
            }
         }
         
         e.data.ptr = &poll_info;
         if (epoll_ctl(event_fd, EPOLL_CTL_ADD, fd, &e) == -1) {
            std::cerr << "epoll ctl error on fd " << fd << "\n";
         }
#elif defined(__APPLE__)
         if (op == poll_op::read_write) {
            struct kevent events[2];

            EV_SET(&events[0], fd, EVFILT_READ, EV_ADD | EV_EOF, 0, 0, &poll_info);
            EV_SET(&events[1], fd, EVFILT_WRITE, EV_ADD | EV_EOF, 0, 0, &poll_info);

            if (kevent(event_fd, events, 2, nullptr, 0, nullptr) == -1) {
                std::cerr << "kqueue failed to register read/write for file_descriptor: " << fd << "\n";
            }
         }
         else {
            net::poll_event_t e{.flags = EV_ADD | EV_EOF, .udata = &poll_info};
            
            switch (op)
            {
               case poll_op::read: {
                  e.filter = EVFILT_READ;
                  break;
               }
               case poll_op::write: {
                  e.filter = EVFILT_WRITE;
                  break;
               }
               default: {
                  break;
               }
            }
            
            if (::kevent(event_fd, &e, 1, nullptr, 0, nullptr) == -1) {
               std::cerr << "kqueue failed to register for file_descriptor: " << fd << "\n";
            }
         }
#elif defined(_WIN32)

#endif

         // The event loop will 'clean-up' whichever event didn't win since the coroutine is scheduled
         // onto the thread poll its possible the other type of event could trigger while its waiting
         // to execute again, thus restarting the coroutine twice, that would be quite bad.
         auto result = co_await poll_info;
         n_active_tasks.fetch_sub(1, std::memory_order::release);
         co_return result;
      }
      
      /**
       * Polls the given coro::net::socket for the given operations.
       * @param sock The socket to poll for events on.
       * @param op The operations to poll for.
       * @param timeout The amount of time to wait for the events to trigger.  A timeout of zero will
       *                block indefinitely until the event triggers.
       * @return THe result of the poll operation.
       */
      [[nodiscard]] task<poll_status> poll(const socket& sock, poll_op op,
                              std::chrono::milliseconds timeout = std::chrono::milliseconds{0})
      {
         return poll(sock.socket_fd, op, timeout);
      }

      /**
       * Resumes execution of a direct coroutine handle on this io scheduler.
       * @param handle The coroutine handle to resume execution.
       */
      auto resume(std::coroutine_handle<> handle) -> bool
      {
         if (not handle) {
            return false;
         }

         if (m_shutdown_requested.load(std::memory_order::acquire)) {
            return false;
         }

         if (opts.execution_strategy == execution_strategy::process_tasks_inline) {
            {
               std::scoped_lock lk{m_scheduled_tasks_mutex};
               m_scheduled_tasks.emplace_back(handle);
            }

            bool expected{false};
            if (m_schedule_fd_triggered.compare_exchange_strong(expected, true, std::memory_order::release,
                                                                std::memory_order::relaxed)) {
#if defined(__linux__)
               eventfd_t value{1};
               eventfd_write(schedule_fd, value);
#elif defined(__APPLE__)
               net::poll_event_t e{
                  .filter = EVFILT_USER, .fflags = NOTE_TRIGGER, .udata = const_cast<void*>(m_schedule_ptr)};
               if (::kevent(event_fd, &e, 1, nullptr, 0, nullptr) == -1) {
                  GLZ_THROW_OR_ABORT(std::runtime_error("Failed to trigger wake up"));
               }
#endif
            }

            return true;
         }
         else {
            return m_thread_pool->resume(handle);
         }
      }

      /**
       * @return The number of tasks waiting in the task queue + the executing tasks.
       */
      size_t size() const noexcept
      {
         if (opts.execution_strategy == execution_strategy::process_tasks_inline) {
            return n_active_tasks.load(std::memory_order::acquire);
         }
         else {
            return n_active_tasks.load(std::memory_order::acquire) + m_thread_pool->size();
         }
      }

      /**
       * @return True if the task queue is empty and zero tasks are currently executing.
       */
      bool empty() const noexcept { return size() == 0; }

      /**
       * Starts the shutdown of the io scheduler.  All currently executing and pending tasks will complete
       * prior to shutting down.  This call is blocking and will not return until all tasks complete.
       */
      void shutdown() noexcept
      {
         // Only allow shutdown to occur once.
         if (m_shutdown_requested.exchange(true, std::memory_order::acq_rel) == false) {
            if (m_thread_pool) {
               m_thread_pool->shutdown();
            }

            // Signal the event loop to stop asap, triggering the event fd is safe.
#if defined(__linux__)
            uint64_t value{1};
            auto written = ::write(shutdown_fd, &value, sizeof(value));
            (void)written;
#elif defined(__APPLE__)
            net::poll_event_t e{
               .filter = EVFILT_USER, .fflags = NOTE_TRIGGER, .udata = const_cast<void*>(m_shutdown_ptr)};
            if (::kevent(event_fd, &e, 1, nullptr, 0, nullptr) == -1) {
               GLZ_THROW_OR_ABORT(std::runtime_error("Failed to signal shutdown event"));
            }
#elif defined(_WIN32)

#endif

            if (m_io_thread.joinable()) {
               m_io_thread.join();
            }
         }
      }

     private:
      /// The configuration options.
      options opts{};

      /// The event loop epoll file descriptor.
      net::event_handle_t event_fd{net::create_event_poll()};
      /// The event loop fd to trigger a shutdown.
      net::event_handle_t shutdown_fd{net::create_shutdown_handle()};
      /// The event loop timer fd for timed events, e.g. yield_for() or scheduler_after().
      net::event_handle_t timer_fd{net::create_timer_handle()};
      /// The schedule file descriptor if the scheduler is in inline processing mode.
      net::event_handle_t schedule_fd{net::create_schedule_handle()};
      std::atomic<bool> m_schedule_fd_triggered{false};

      /// The number of tasks executing or awaiting events in this io scheduler.
      std::atomic<size_t> n_active_tasks{0};

      /// The background io worker threads.
      std::thread m_io_thread;
      /// Thread pool for executing tasks when not in inline mode.
      std::unique_ptr<thread_pool> m_thread_pool{nullptr};

      std::mutex m_timed_events_mutex{};
      /// The map of time point's to poll infos for tasks that are yielding for a period of time
      /// or for tasks that are polling with timeouts.
      timed_events m_timed_events{};

      /// Has the scheduler been requested to shut down?
      std::atomic<bool> m_shutdown_requested{false};

      std::atomic<bool> m_io_processing{false};
      void process_events_manual(std::chrono::milliseconds timeout)
      {
         bool expected{false};
         if (m_io_processing.compare_exchange_strong(expected, true, std::memory_order::release,
                                                     std::memory_order::relaxed)) {
            process_events_execute(timeout);
            m_io_processing.exchange(false, std::memory_order::release);
         }
      }

      void init()
      {
         if (opts.execution_strategy == execution_strategy::process_tasks_on_thread_pool) {
            m_thread_pool = std::make_unique<thread_pool>(std::move(opts.pool));
         }

         [[maybe_unused]] net::poll_event_t e{};

         [[maybe_unused]] bool event_setup_failed{};
#if defined(__linux__)
         e.events = EPOLLIN;

         e.data.ptr = const_cast<void*>(m_shutdown_ptr);
         epoll_ctl(event_fd, EPOLL_CTL_ADD, shutdown_fd, &e);

         e.data.ptr = const_cast<void*>(m_timer_ptr);
         epoll_ctl(event_fd, EPOLL_CTL_ADD, timer_fd, &e);

         e.data.ptr = const_cast<void*>(m_schedule_ptr);
         epoll_ctl(event_fd, EPOLL_CTL_ADD, schedule_fd, &e);
#elif defined(__APPLE__)
         net::poll_event_t e_timer{.filter = EVFILT_TIMER, .flags = EV_ADD, .udata = const_cast<void*>(m_timer_ptr)};
         net::poll_event_t e_shutdown{
            .filter = EVFILT_USER, .flags = EV_ADD | EV_CLEAR, .udata = const_cast<void*>(m_shutdown_ptr)};
         net::poll_event_t e_schedule{
            .filter = EVFILT_USER, .flags = EV_ADD, .udata = const_cast<void*>(m_schedule_ptr)};

         ::kevent(event_fd, &e_schedule, 1, nullptr, 0, nullptr);
         ::kevent(event_fd, &e_shutdown, 1, nullptr, 0, nullptr);
         ::kevent(event_fd, &e_timer, 1, nullptr, 0, nullptr);
#endif

         if (opts.thread_strategy == glz::thread_strategy::spawn) {
            m_io_thread = std::thread([this] { process_events_dedicated_thread(); });
         }
         // else manual mode, the user must call process_events.
      }

      void process_events_dedicated_thread()
      {
         if (opts.on_io_thread_start_functor) {
            opts.on_io_thread_start_functor();
         }

         m_io_processing.exchange(true, std::memory_order::release);
         // Execute tasks until stopped or there are no more tasks to complete.
         while (!m_shutdown_requested.load(std::memory_order::acquire) || size() > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1)); // prevent pegging 100%
            process_events_execute(m_default_timeout);
         }
         m_io_processing.exchange(false, std::memory_order::release);

         if (opts.on_io_thread_stop_functor) {
            opts.on_io_thread_stop_functor();
         }
      }

      void process_events_execute(std::chrono::milliseconds timeout)
      {
#if defined(__APPLE__)
         struct timespec tlimit
         {
            0, timeout.count() * 1'000'000
         };
         const auto event_count = ::kevent(event_fd, nullptr, 0, m_events.data(), int(m_events.size()), &tlimit);
#elif defined(__linux__)
         const auto event_count = ::epoll_wait(event_fd, m_events.data(), max_events, timeout.count());
#elif defined(_WIN32)

         m_events = {shutdown_fd, timer_fd, schedule_fd};

         enum struct event_id : DWORD {
               shutdown,
               timer,
               schedual
         };

         const auto event_obj =
            WaitForMultipleObjects(3 /*number of object handles*/, m_events.data(), FALSE, DWORD(timeout.count()));

         if (WAIT_FAILED == event_obj) {
            GLZ_THROW_OR_ABORT(std::runtime_error{"WaitForMultipleObjects for event failed"});
         }
         using enum event_id;

         switch (event_id(event_obj)) {
         case shutdown:
            // TODO:
            break;

         case timer:
            process_timeout_execute();
            break;

         case schedual:
            process_scheduled_execute_inline();
            break;

         default:
            break;
           // GLZ_THROW_OR_ABORT(std::runtime_error{"Unhandled event id!"});
         }
#endif

#if defined(__linux__) || defined(__APPLE__)

         if (event_count == -1) {
            net::close_event(event_fd);
            GLZ_THROW_OR_ABORT(std::runtime_error{"wait for event failed"});
         }

         if (event_count > 0) {
            for (size_t i = 0; i < size_t(event_count); ++i) {
               auto& event = m_events[i];
#if defined(__linux__)
               void* handle_ptr = event.data.ptr;
#elif defined(__APPLE__)
               void* handle_ptr = event.udata;

               if (event.flags & EV_ERROR) {
                  GLZ_THROW_OR_ABORT(std::runtime_error{"event error"});
               }
#endif

               if (not handle_ptr) {
                  GLZ_THROW_OR_ABORT(std::runtime_error{"handle_ptr is null"});
               }

               if (handle_ptr == m_timer_ptr) {
                  process_timeout_execute();
               }
               else if (handle_ptr == m_schedule_ptr) {
                  process_scheduled_execute_inline();
               }
               else if (handle_ptr == m_shutdown_ptr) [[unlikely]] {
                  
               }
               else {
                  // Individual poll task wake-up.
#if defined(__linux__)
                  process_event_execute(static_cast<poll_info*>(handle_ptr), event_to_poll_status(event.events));
#elif defined(__APPLE__)
                  process_event_execute(static_cast<poll_info*>(handle_ptr), event_to_poll_status(event.flags));
#endif
               }
            }
         }
#endif
         // Its important to not resume any handles until the full set is accounted for.  If a timeout
         // and an event for the same handle happen in the same epoll_wait() call then inline processing
         // will destruct the poll_info object before the second event is handled.  This is also possible
         // with thread pool processing, but probably has an extremely low chance of occuring due to
         // the thread switch required. If max_events == 1 this would be unnecessary.

         if (!m_handles_to_resume.empty()) {
            if (opts.execution_strategy == execution_strategy::process_tasks_inline) {
               for (auto& handle : m_handles_to_resume) {
                  handle.resume();
               }
            }
            else {
               m_thread_pool->resume(m_handles_to_resume);
            }

            m_handles_to_resume.clear();
         }
      }

      static poll_status event_to_poll_status(uint32_t events)
      {
         if (events & net::poll_in || events & net::poll_out) {
            return poll_status::event;
         }
         else if (net::poll_error(events)) {
            return poll_status::error;
         }
         else if (net::event_closed(events)) {
            return poll_status::closed;
         }

         GLZ_THROW_OR_ABORT(std::runtime_error{"invalid epoll state"});
      }

      void process_scheduled_execute_inline()
      {
         std::vector<std::coroutine_handle<>> tasks{};
         {
            // Acquire the entire list, and then reset it.
            std::scoped_lock lk{m_scheduled_tasks_mutex};
            tasks.swap(m_scheduled_tasks);

            // Clear the schedule eventfd if this is a scheduled task.
#if defined(__linux__)
            eventfd_t value{0};
            eventfd_read(schedule_fd, &value);
#elif defined(__APPLE__)
#endif

            // Clear the in memory flag to reduce eventfd_* calls on scheduling.
            m_schedule_fd_triggered.exchange(false, std::memory_order::release);
         }

         // This set of handles can be safely resumed now since they do not have a corresponding timeout event.
         for (auto& task : tasks) {
            task.resume();
         }
         n_active_tasks.fetch_sub(tasks.size(), std::memory_order::release);
      }

      std::mutex m_scheduled_tasks_mutex{};
      std::vector<std::coroutine_handle<>> m_scheduled_tasks{};

      static constexpr int m_shutdown_object{0};
      static constexpr const void* m_shutdown_ptr = &m_shutdown_object;

      static constexpr int m_timer_object{0};
      static constexpr const void* m_timer_ptr = &m_timer_object;

      static constexpr int m_schedule_object{0};
      static constexpr const void* m_schedule_ptr = &m_schedule_object;

      static constexpr std::chrono::milliseconds m_default_timeout{1000};
      static constexpr std::chrono::milliseconds m_no_timeout{0};
      static constexpr size_t max_events = 16;
      std::array<net::poll_event_t, max_events> m_events{};
      std::vector<std::coroutine_handle<>> m_handles_to_resume{};
      
      void apple_delete_poll_event(glz::poll_info* poll_info)
      {
#if defined(__APPLE__)
         if (poll_info->op == poll_op::read_write) {
            struct kevent event;
            EV_SET(&event, poll_info->m_fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
            
            if (kevent(event_fd, &event, 1, nullptr, 0, nullptr) == -1) {
                // It's often okay if this fails, as the descriptor might not have been registered
                // But you might want to log it for debugging purposes
                // std::cerr << "kqueue failed to delete read event for fd: " << poll_info->m_fd << "\n";
            }
            
            EV_SET(&event, poll_info->m_fd, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);
            
            if (kevent(event_fd, &event, 1, nullptr, 0, nullptr) == -1) {
                // Again, failure here is often okay
                // std::cerr << "kqueue failed to delete write event for fd: " << poll_info->m_fd << "\n";
            }
         }
         else {
            switch (poll_info->op)
            {
               case poll_op::read: {
                  struct kevent e;
                  EV_SET(&e, poll_info->m_fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
                  
                  if (kevent(event_fd, &e, 1, nullptr, 0, nullptr) == -1) {
                      // It's often okay if this fails, as the descriptor might not have been registered
                      // But you might want to log it for debugging purposes
                      // std::cerr << "kqueue failed to delete read event for fd: " << poll_info->m_fd << "\n";
                  }
                  
                  break;
               }
               case poll_op::write: {
                  struct kevent e;
                  EV_SET(&e, poll_info->m_fd, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);
                  
                  if (kevent(event_fd, &e, 1, nullptr, 0, nullptr) == -1) {
                      // Failure here is often okay
                      // std::cerr << "kqueue failed to delete write event for fd: " << poll_info->m_fd << "\n";
                  }
                  break;
               }
               default: {
                  break;
               }
            }
         }
#endif
      }

      void process_event_execute(glz::poll_info* poll_info, poll_status status)
      {
         if (not poll_info) {
            GLZ_THROW_OR_ABORT(std::runtime_error{"invalid poll_info"});
         }

         if (not poll_info->m_processed) {
            std::atomic_thread_fence(std::memory_order::acquire);
            // Its possible the event and the timeout occurred in the same epoll, make sure only one
            // is ever processed, the other is discarded.
            poll_info->m_processed = true;

            // Given a valid fd always remove it from epoll so the next poll can blindly EPOLL_CTL_ADD.
            if (poll_info->m_fd != net::invalid_event_handle) {
#if defined(__APPLE__)
               apple_delete_poll_event(poll_info);
#elif defined(__linux__)
               epoll_ctl(event_fd, EPOLL_CTL_DEL, poll_info->m_fd, nullptr);
#endif
            }

            // Since this event triggered, remove its corresponding timeout if it has one.
            if (poll_info->m_timer_pos.has_value()) {
               remove_timer_token(poll_info->m_timer_pos.value());
            }

            poll_info->m_poll_status = status;

            while (not poll_info->m_awaiting_coroutine) {
               std::atomic_thread_fence(std::memory_order::acquire);
            }

            m_handles_to_resume.emplace_back(poll_info->m_awaiting_coroutine);
         }
      }

      void process_timeout_execute()
      {
         std::vector<poll_info*> poll_infos{};
         auto now = clock::now();

         {
            std::scoped_lock lk{m_timed_events_mutex};
            while (!m_timed_events.empty()) {
               auto first = m_timed_events.begin();
               auto [tp, pi] = *first;

               if (tp <= now) {
                  m_timed_events.erase(first);
                  poll_infos.emplace_back(pi);
               }
               else {
                  break;
               }
            }
         }

         for (auto pi : poll_infos) {
            if (!pi->m_processed) {
               // Its possible the event and the timeout occurred in the same epoll, make sure only one
               // is ever processed, the other is discarded.
               pi->m_processed = true;

               // Since this timed out, remove its corresponding event if it has one.
               if (pi->m_fd != net::invalid_event_handle) {
#if defined(__linux__)
                  epoll_ctl(event_fd, EPOLL_CTL_DEL, pi->m_fd, nullptr);
#elif defined(__APPLE__)
                  apple_delete_poll_event(pi);
#endif
               }

               while (not pi->m_awaiting_coroutine) {
                  std::atomic_thread_fence(std::memory_order::acquire);
                  std::cerr << "process_event_execute() has a null event\n";
               }

               m_handles_to_resume.emplace_back(pi->m_awaiting_coroutine);
               pi->m_poll_status = poll_status::timeout;
            }
         }

         // Update the time to the next smallest time point, re-take the current now time
         // since updating and resuming tasks could shift the time.
         update_timeout(clock::now());
      }

      auto add_timer_token(time_point tp, poll_info& pi) -> timed_events::iterator
      {
         std::scoped_lock lk{m_timed_events_mutex};
         auto pos = m_timed_events.emplace(tp, &pi);

         // If this item was inserted as the smallest time point, update the timeout.
         if (pos == m_timed_events.begin()) {
            update_timeout(clock::now());
         }

         return pos;
      }

      void remove_timer_token(timed_events::iterator pos)
      {
         std::scoped_lock lk{m_timed_events_mutex};
         auto is_first = (m_timed_events.begin() == pos);

         m_timed_events.erase(pos);

         // If this was the first item, update the timeout.  It would be acceptable to just let it
         // also fire the timeout as the event loop will ignore it since nothing will have timed
         // out but it feels like the right thing to do to update it to the correct timeout value.
         if (is_first) {
            update_timeout(clock::now());
         }
      }

      void update_timeout(time_point now)
      {
         if (!m_timed_events.empty()) {
            auto& [tp, pi] = *m_timed_events.begin();

#if defined(__linux__)
            size_t seconds{};
            size_t nanoseconds{1};
            if (tp > now) {
               const auto time_left = tp - now;
               const auto s = std::chrono::duration_cast<std::chrono::seconds>(time_left);
               seconds = s.count();
               nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(time_left - s).count();
            }

            itimerspec ts{};
            ts.it_value.tv_sec = seconds;
            ts.it_value.tv_nsec = nanoseconds;

            if (timerfd_settime(timer_fd, 0, &ts, nullptr) == -1) {
               std::cerr << "Failed to set timerfd errorno=[" << std::string{std::strerror(errno)} << "].";
            }
#elif defined(__APPLE__)
            size_t milliseconds{};
            if (tp > now) {
               milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(tp - now).count();
            }

            net::poll_event_t e{.filter = EVFILT_TIMER,
                                .fflags = NOTE_TRIGGER,
                                .data = int64_t(milliseconds),
                                .udata = const_cast<void*>(m_timer_ptr)};
            if (::kevent(event_fd, &e, 1, nullptr, 0, nullptr) == -1) {
               std::cerr << "Error: kevent (update timer).\n";
            }
#elif defined(_WIN32)
            size_t seconds{};
            size_t nanoseconds{1};
            if (tp > now) {
               const auto time_left = tp - now;
               const auto s = std::chrono::duration_cast<std::chrono::seconds>(time_left);
               seconds = s.count();
               nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(time_left - s).count();
            }
            LARGE_INTEGER signal_time{};
            signal_time.QuadPart = -std::chrono::duration_cast<std::chrono::nanoseconds>(tp - now).count() / 100;

            if (!SetWaitableTimer(timer_fd, &signal_time, 0, nullptr, nullptr, FALSE)) {
               std::cerr << "SetWaitableTimer failed (" << GetLastError() << ")\n";
            }
#endif
         }
         else {
#if defined(__linux__)
            // Setting these values to zero disables the timer.
            itimerspec ts{};
            ts.it_value.tv_sec = 0;
            ts.it_value.tv_nsec = 0;
            if (timerfd_settime(timer_fd, 0, &ts, nullptr) == -1) {
               std::cerr << "Failed to set timerfd errorno=[" << std::string{strerror(errno)} << "].";
            }
#elif defined(__APPLE__)
            net::poll_event_t e{
               .filter = EVFILT_TIMER, .fflags = NOTE_TRIGGER, .data = 0, .udata = const_cast<void*>(m_timer_ptr)};
            if (::kevent(event_fd, &e, 1, nullptr, 0, nullptr) == -1) {
               std::cerr << "Error: kevent (update timer).\n";
            }
#elif defined(_WIN32)
            LARGE_INTEGER signal_time{};
            if (!SetWaitableTimer(timer_fd, &signal_time, 0, nullptr, nullptr, FALSE)) {
               std::cerr << "Error: SetWaitableTimer (disable timer) failed (" << GetLastError() << ")\n";
            }
#endif
         }
      }
   };
}
