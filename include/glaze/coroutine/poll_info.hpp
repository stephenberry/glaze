// Glaze Library
// For the license information refer to glaze.hpp

// Modified from the awesome: https://github.com/jbaldwin/libcoro

#pragma once

#pragma once

#include <atomic>
#include <chrono>
#include <coroutine>
#include <map>
#include <optional>

#include "glaze/coroutine/poll.hpp"
#include "glaze/network/core.hpp"

namespace glz
{
   /**
    * Poll Info encapsulates everything about a poll operation for the event as well as its paired
    * timeout.  This is important since coroutines that are waiting on an event or timeout do not
    * immediately execute, they are re-scheduled onto the thread pool, so its possible its pair
    * event or timeout also triggers while the coroutine is still waiting to resume.  This means that
    * the first one to happen, the event itself or its timeout, needs to disable the other pair item
    * prior to resuming the coroutine.
    *
    * Finally, its also important to note that the event and its paired timeout could happen during
    * the same epoll_wait and possibly trigger the coroutine to start twice.  Only one can win, so the
    * first one processed sets m_processed to true and any subsequent events in the same epoll batch
    * are effectively discarded.
    */
   struct poll_info final
   {
      using timed_events = std::multimap<std::chrono::steady_clock::time_point, poll_info*>;

      poll_info() = default;
      ~poll_info() = default;

      poll_info(const poll_info&) = delete;
      poll_info(poll_info&&) = delete;
      auto operator=(const poll_info&) -> poll_info& = delete;
      auto operator=(poll_info&&) -> poll_info& = delete;

      struct poll_awaiter final
      {
         glz::poll_info& poll_info;

         bool await_ready() const noexcept { return false; }
         void await_suspend(std::coroutine_handle<> awaiting_coroutine) noexcept
         {
            poll_info.m_awaiting_coroutine = awaiting_coroutine;
            std::atomic_thread_fence(std::memory_order::release);
         }
         poll_status await_resume() noexcept { return poll_info.m_poll_status; }
      };

      poll_awaiter operator co_await() noexcept { return {*this}; }

      /// The file descriptor being polled on.  This is needed so that if the timeout occurs first then
      /// the event loop can immediately disable the event within epoll.
      net::event_handle_t m_fd{net::invalid_event_handle};
      /// The timeout's position in the timeout map.  A poll() with no timeout or yield() this is empty.
      /// This is needed so that if the event occurs first then the event loop can immediately disable
      /// the timeout within epoll.
      std::optional<timed_events::iterator> m_timer_pos{};
      /// The awaiting coroutine for this poll info to resume upon event or timeout.
      std::coroutine_handle<> m_awaiting_coroutine{};
      /// The status of the poll operation.
      poll_status m_poll_status{glz::poll_status::error};
      /// Did the timeout and event trigger at the same time on the same epoll_wait call?
      /// Once this is set to true all future events on this poll info are null and void.
      bool m_processed{false};
      
      /// The operation for deleting on Mac
      glz::poll_op op{};
   };
}
