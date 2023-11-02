// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <list>
#include <mutex>
#include <thread>
#include <vector>

namespace glz
{
   // A thread pool
   struct pool final
   {
      pool() : pool(concurrency()) {}

      pool(const size_t n) { n_threads(n); }

      void n_threads(const size_t n)
      {
         finish_work(); // finish any active work
         std::lock_guard lock(mtx);
         closed = false;

         threads.clear();
         threads.reserve(n);
         for (size_t i = 0; i < n; ++i) {
            threads.emplace_back(std::thread(&pool::worker, this, i));
         }
      }

      size_t concurrency() const noexcept { return std::thread::hardware_concurrency(); }

      template <class F>
      std::future<std::invoke_result_t<std::decay_t<F>>> emplace_back(F&& func)
      {
         using result_type = std::invoke_result_t<std::decay_t<F>>;

         std::lock_guard lock(mtx);

         auto promise = std::make_shared<std::promise<result_type>>();

         queue.emplace_back([promise, f = std::move(func)](const size_t /*thread_number*/) {
#if __cpp_exceptions
            try {
               if constexpr (std::is_void_v<result_type>) {
                  f();
               }
               else {
                  promise->set_value(f());
               }
            }
            catch (...) {
               promise->set_exception(std::current_exception());
            }
#else
            if constexpr (std::is_void_v<result_type>) {
               f();
            }
            else {
               promise->set_value(f());
            }
#endif
         });

         work_cv.notify_one();

         return promise->get_future();
      }

      // Takes a function whose input is the thread number (size_t)
      template <class F>
         requires std::invocable<std::decay_t<F>, size_t>
      std::future<std::invoke_result_t<std::decay_t<F>, size_t>> emplace_back(F&& func)
      {
         using result_type = std::invoke_result_t<std::decay_t<F>, size_t>;

         std::lock_guard lock(mtx);

         auto promise = std::make_shared<std::promise<result_type>>();

         queue.emplace_back([promise, f = std::move(func)](const size_t thread_number) {
#if __cpp_exceptions
            try {
               if constexpr (std::is_void_v<result_type>) {
                  f(thread_number);
               }
               else {
                  promise->set_value(f(thread_number));
               }
            }
            catch (...) {
               promise->set_exception(std::current_exception());
            }
#else
            if constexpr (std::is_void_v<result_type>) {
               f(thread_number);
            }
            else {
               promise->set_value(f(thread_number));
            }
#endif
         });

         work_cv.notify_one();

         return promise->get_future();
      }

      bool computing() const noexcept { return (working != 0); }

      uint32_t number_working() const noexcept { return working; }

      void wait()
      {
         std::unique_lock lock(mtx);
         if (queue.empty() && (working == 0)) return;
         done_cv.wait(lock, [&]() { return queue.empty() && (working == 0); });
      }

      size_t size() const { return threads.size(); }

      ~pool() { finish_work(); }

     private:
      std::vector<std::thread> threads;
      // using std::deque for the queue causes random function call issues
      std::list<std::function<void(const size_t)>> queue;
      std::atomic<uint32_t> working = 0;
      std::atomic<bool> closed = false;
      std::mutex mtx;
      std::condition_variable work_cv;
      std::condition_variable done_cv;

      void finish_work()
      {
         // Close the queue and finish all the remaining work
         std::unique_lock lock(mtx);
         closed = true;
         work_cv.notify_all();
         lock.unlock();

         for (auto& t : threads) {
            if (t.joinable()) t.join();
         }
      }

      void worker(const size_t thread_number)
      {
         while (true) {
            // Wait for work
            std::unique_lock lock(mtx);
            work_cv.wait(lock, [this]() { return closed || !queue.empty(); });
            if (queue.empty()) {
               if (closed) {
                  return;
               }
            }
            else {
               // Grab work
               ++working;
               auto work = std::move(queue.front());
               queue.pop_front();
               lock.unlock();

               work(thread_number);

               lock.lock();

               // Notify that work is finished
               --working;
               done_cv.notify_all();
            }
         }
      }
   };
}
