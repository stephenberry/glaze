// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <mutex>
#include <thread>
#include <vector>

#include "glaze/util/macros.hpp"

namespace glaze
{
   // A simple threadpool
   struct pool
   {
      pool() : pool(concurrency()) {}

      pool(const unsigned int n) { n_threads(n); }

      void n_threads(const unsigned int n)
      {
#ifdef _WIN32
         // TODO smarter distrubution of threads among groups.
         auto num_groups = GetActiveProcessorGroupCount();
         WORD group = 0;
         for (size_t i = threads.size(); i < n; ++i) {
            auto thread = std::thread(&pool::worker, this);
            auto hndl = thread.native_handle();
            GROUP_AFFINITY affinity;
            if (GetThreadGroupAffinity(hndl, &affinity) &&
                affinity.Group != group) {
               affinity.Group = group;
               SetThreadGroupAffinity(hndl, &affinity, nullptr);
            }
            group++;
            if (group >= num_groups) group = 0;
            threads.emplace_back(std::move(thread));
         }
#else
         for (size_t i = threads.size(); i < n; ++i) {
            threads.emplace_back(std::thread(&pool::worker, this));
         }
#endif
      }

      size_t concurrency()
      {
#ifdef _WIN32
         auto num_groups = GetActiveProcessorGroupCount();
         unsigned int sum = 0;
         for (WORD i = 0; i < num_groups; ++i) {
            sum += GetMaximumProcessorCount(i);
         }
         return sum;
#else
         return std::thread::hardware_concurrency();
#endif
      }

      template <typename F>
      std::future<std::invoke_result_t<std::decay_t<F>>> emplace_back(F &&func)
      {
         using result_type = decltype(func());

         std::lock_guard<std::mutex> lock(m);

         auto promise = std::make_shared<std::promise<result_type>>();

         queue.emplace_back([=](auto...) {
            try {
               if constexpr (std::is_void<result_type>::value) {
                  func();
               }
               else {
                  promise->set_value(func());
               }
            }
            catch (...) {
               promise->set_exception(std::current_exception());
            }
         });

         work_cv.notify_one();

         return promise->get_future();
      }

      bool computing() const { return (working != 0); }

      void wait()
      {
         std::unique_lock<std::mutex> lock(m);
         if (queue.empty() && (working == 0)) return;
         done_cv.wait(lock, [&]() { return queue.empty() && (working == 0); });
      }

      size_t size() const { return threads.size(); }

      ~pool()
      {
         // Close the queue and finish all the remaining work
         std::unique_lock<std::mutex> lock(m);
         closed = true;
         work_cv.notify_all();
         lock.unlock();

         for (auto &t : threads)
            if (t.joinable()) t.join();
      }

     private:
      std::vector<std::thread> threads;
      std::deque<std::function<void()>> queue;
      std::atomic<unsigned int> working = 0;
      bool closed = false;
      std::mutex m;
      std::condition_variable work_cv;
      std::condition_variable done_cv;

      void worker()
      {
         while (true) {
            // Wait for work
            std::unique_lock<std::mutex> lock(m);
            work_cv.wait(lock, [this]() { return closed || !queue.empty(); });
            if (queue.empty()) {
               if (closed) {
                  return;
               }
               continue;
            }

            // Grab work
            ++working;
            auto work = queue.front();
            queue.pop_front();
            lock.unlock();

            work();

            // Notify that work is finished
            lock.lock();
            --working;
            done_cv.notify_all();
         }
      }
   };
}
