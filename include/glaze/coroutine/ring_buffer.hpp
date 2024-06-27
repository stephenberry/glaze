// Glaze Library
// For the license information refer to glaze.hpp

// Modified from the awesome: https://github.com/jbaldwin/libcoro

#pragma once

#include <array>
#include <atomic>
#include <coroutine>
#include <mutex>
#include <optional>

#include "glaze/util/expected.hpp"

namespace glz
{
   namespace rb
   {
      enum struct produce_result { produced, ring_buffer_stopped };

      enum struct consume_result { ring_buffer_stopped };
   }

   /**
    * @tparam element The type of element the ring buffer will store.  Note that this type should be
    *         cheap to move if possible as it is moved into and out of the buffer upon produce and
    *         consume operations.
    * @tparam num_elements The maximum number of elements the ring buffer can store, must be >= 1.
    */
   template <typename element, size_t num_elements>
   class ring_buffer
   {
     public:
      /**
       * static_assert If `num_elements` == 0.
       */
      ring_buffer() { static_assert(num_elements != 0, "num_elements cannot be zero"); }

      ~ring_buffer()
      {
         // Wake up anyone still using the ring buffer.
         notify_waiters();
      }

      ring_buffer(const ring_buffer<element, num_elements>&) = delete;
      ring_buffer(ring_buffer<element, num_elements>&&) = delete;

      auto operator=(const ring_buffer<element, num_elements>&) noexcept
         -> ring_buffer<element, num_elements>& = delete;
      auto operator=(ring_buffer<element, num_elements>&&) noexcept -> ring_buffer<element, num_elements>& = delete;

      struct produce_operation
      {
         produce_operation(ring_buffer<element, num_elements>& rb, element e) : m_rb(rb), m_e(std::move(e)) {}

         auto await_ready() noexcept -> bool
         {
            std::unique_lock lk{m_rb.m_mutex};
            return m_rb.try_produce_locked(lk, m_e);
         }

         auto await_suspend(std::coroutine_handle<> awaiting_coroutine) noexcept -> bool
         {
            std::unique_lock lk{m_rb.m_mutex};
            // Its possible a consumer on another thread consumed an item between await_ready() and await_suspend()
            // so we must check to see if there is space again.
            if (m_rb.try_produce_locked(lk, m_e)) {
               return false;
            }

            // Don't suspend if the stop signal has been set.
            if (m_rb.m_stopped.load(std::memory_order::acquire)) {
               m_stopped = true;
               return false;
            }

            m_awaiting_coroutine = awaiting_coroutine;
            m_next = m_rb.m_produce_waiters;
            m_rb.m_produce_waiters = this;
            return true;
         }

         /**
          * @return produce_result
          */
         auto await_resume() -> rb::produce_result
         {
            return !m_stopped ? rb::produce_result::produced : rb::produce_result::ring_buffer_stopped;
         }

        private:
         template <typename element_subtype, size_t num_elements_subtype>
         friend class ring_buffer;

         /// The ring buffer the element is being produced into.
         ring_buffer<element, num_elements>& m_rb;
         /// If the operation needs to suspend, the coroutine to resume when the element can be produced.
         std::coroutine_handle<> m_awaiting_coroutine;
         /// Linked list of produce operations that are awaiting to produce their element.
         produce_operation* m_next{nullptr};
         /// The element this produce operation is producing into the ring buffer.
         element m_e;
         /// Was the operation stopped?
         bool m_stopped{false};
      };

      struct consume_operation
      {
         explicit consume_operation(ring_buffer<element, num_elements>& rb) : m_rb(rb) {}

         auto await_ready() noexcept -> bool
         {
            std::unique_lock lk{m_rb.m_mutex};
            return m_rb.try_consume_locked(lk, this);
         }

         auto await_suspend(std::coroutine_handle<> awaiting_coroutine) noexcept -> bool
         {
            std::unique_lock lk{m_rb.m_mutex};
            // We have to check again as there is a race condition between await_ready() and now on the mutex acquire.
            // It is possible that a producer added items between await_ready() and await_suspend().
            if (m_rb.try_consume_locked(lk, this)) {
               return false;
            }

            // Don't suspend if the stop signal has been set.
            if (m_rb.m_stopped.load(std::memory_order::acquire)) {
               m_stopped = true;
               return false;
            }
            m_awaiting_coroutine = awaiting_coroutine;
            m_next = m_rb.m_consume_waiters;
            m_rb.m_consume_waiters = this;
            return true;
         }

         /**
          * @return The consumed element or std::nullopt if the consume has failed.
          */
         auto await_resume() -> expected<element, rb::consume_result>
         {
            if (m_stopped) {
               return unexpected<rb::consume_result>(rb::consume_result::ring_buffer_stopped);
            }

            return std::move(m_e);
         }

        private:
         template <typename element_subtype, size_t num_elements_subtype>
         friend class ring_buffer;

         /// The ring buffer to consume an element from.
         ring_buffer<element, num_elements>& m_rb;
         /// If the operation needs to suspend, the coroutine to resume when the element can be consumed.
         std::coroutine_handle<> m_awaiting_coroutine;
         /// Linked list of consume operations that are awaiting to consume an element.
         consume_operation* m_next{nullptr};
         /// The element this consume operation will consume.
         element m_e;
         /// Was the operation stopped?
         bool m_stopped{false};
      };

      /**
       * Produces the given element into the ring buffer.  This operation will suspend until a slot
       * in the ring buffer becomes available.
       * @param e The element to produce.
       */
      [[nodiscard]] auto produce(element e) -> produce_operation { return produce_operation{*this, std::move(e)}; }

      /**
       * Consumes an element from the ring buffer.  This operation will suspend until an element in
       * the ring buffer becomes available.
       */
      [[nodiscard]] auto consume() -> consume_operation { return consume_operation{*this}; }

      /**
       * @return The current number of elements contained in the ring buffer.
       */
      auto size() const -> size_t
      {
         std::atomic_thread_fence(std::memory_order::acquire);
         return m_used;
      }

      /**
       * @return True if the ring buffer contains zero elements.
       */
      auto empty() const -> bool { return size() == 0; }

      /**
       * Wakes up all currently awaiting producers and consumers.  Their await_resume() function
       * will return an expected consume result that the ring buffer has stopped.
       */
      auto notify_waiters() -> void
      {
         std::unique_lock lk{m_mutex};
         // Only wake up waiters once.
         if (m_stopped.load(std::memory_order::acquire)) {
            return;
         }

         m_stopped.exchange(true, std::memory_order::release);

         while (m_produce_waiters != nullptr) {
            auto* to_resume = m_produce_waiters;
            to_resume->m_stopped = true;
            m_produce_waiters = m_produce_waiters->m_next;

            lk.unlock();
            to_resume->m_awaiting_coroutine.resume();
            lk.lock();
         }

         while (m_consume_waiters != nullptr) {
            auto* to_resume = m_consume_waiters;
            to_resume->m_stopped = true;
            m_consume_waiters = m_consume_waiters->m_next;

            lk.unlock();
            to_resume->m_awaiting_coroutine.resume();
            lk.lock();
         }
      }

     private:
      friend produce_operation;
      friend consume_operation;

      std::mutex m_mutex{};

      std::array<element, num_elements> m_elements{};
      /// The current front pointer to an open slot if not full.
      size_t m_front{0};
      /// The current back pointer to the oldest item in the buffer if not empty.
      size_t m_back{0};
      /// The number of items in the ring buffer.
      size_t m_used{0};

      /// The LIFO list of produce waiters.
      produce_operation* m_produce_waiters{nullptr};
      /// The LIFO list of consume watier.
      consume_operation* m_consume_waiters{nullptr};

      std::atomic<bool> m_stopped{false};

      auto try_produce_locked(std::unique_lock<std::mutex>& lk, element& e) -> bool
      {
         if (m_used == num_elements) {
            return false;
         }

         m_elements[m_front] = std::move(e);
         m_front = (m_front + 1) % num_elements;
         ++m_used;

         if (m_consume_waiters != nullptr) {
            consume_operation* to_resume = m_consume_waiters;
            m_consume_waiters = m_consume_waiters->m_next;

            // Since the consume operation suspended it needs to be provided an element to consume.
            to_resume->m_e = std::move(m_elements[m_back]);
            m_back = (m_back + 1) % num_elements;
            --m_used; // And we just consumed up another item.

            lk.unlock();
            to_resume->m_awaiting_coroutine.resume();
         }

         return true;
      }

      auto try_consume_locked(std::unique_lock<std::mutex>& lk, consume_operation* op) -> bool
      {
         if (m_used == 0) {
            return false;
         }

         op->m_e = std::move(m_elements[m_back]);
         m_back = (m_back + 1) % num_elements;
         --m_used;

         if (m_produce_waiters != nullptr) {
            produce_operation* to_resume = m_produce_waiters;
            m_produce_waiters = m_produce_waiters->m_next;

            // Since the produce operation suspended it needs to be provided a slot to place its element.
            m_elements[m_front] = std::move(to_resume->m_e);
            m_front = (m_front + 1) % num_elements;
            ++m_used; // And we just produced another item.

            lk.unlock();
            to_resume->m_awaiting_coroutine.resume();
         }

         return true;
      }
   };
}
