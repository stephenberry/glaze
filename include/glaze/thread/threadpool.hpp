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

#include "glaze/util/windows_nominmax.hpp"

namespace glz
{
   // A simple threadpool
   struct pool
   {
      pool() : pool(concurrency()) {}

      pool(const size_t n) { n_threads(n); }

      void n_threads(const size_t n)
      {
#ifdef _WIN32
         // NOT REQUIRED IN WINDOWS 11
         // TODO smarter distrubution of threads among groups.
         auto num_groups = GetActiveProcessorGroupCount();
         WORD group = 0;
         for (size_t i = threads.size(); i < n; ++i) {
            auto thread = std::thread(&pool::worker, this, i);
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
            threads.emplace_back(std::thread(&pool::worker, this, i));
         }
#endif
      }

      size_t concurrency() const
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

      template <class F>
      std::future<std::invoke_result_t<std::decay_t<F>>> emplace_back(F&& func)
      {
         using result_type = std::invoke_result_t<std::decay_t<F>>;

         std::lock_guard lock(mtx);

         auto promise = std::make_shared<std::promise<result_type>>();

         queue.emplace(last_index++, [=, f = std::forward<F>(func)](const size_t thread_number) {
            try {
               if constexpr (std::is_void<result_type>::value) {
                  f();
               }
               else {
                  promise->set_value(f());
               }
            }
            catch (...) {
               promise->set_exception(std::current_exception());
            }
         });

         work_cv.notify_one();

         return promise->get_future();
      }
      
      template <class F>
      requires std::is_invocable_v<std::decay_t<F>, size_t>
      std::future<std::invoke_result_t<std::decay_t<F>, size_t>> emplace_back(F&& func)
      {
         using result_type = std::invoke_result_t<std::decay_t<F>, size_t>;

         std::lock_guard lock(mtx);

         auto promise = std::make_shared<std::promise<result_type>>();

         queue.emplace(last_index++, [=, f = std::forward<F>(func)](const size_t thread_number) {
            try {
               if constexpr (std::is_void<result_type>::value) {
                  f(thread_number);
               }
               else {
                  promise->set_value(f(thread_number));
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
         std::unique_lock lock(mtx);
         if (queue.empty() && (working == 0)) return;
         done_cv.wait(lock, [&]() { return queue.empty() && (working == 0); });
      }

      size_t size() const { return threads.size(); }

      ~pool()
      {
         // Close the queue and finish all the remaining work
         std::unique_lock lock(mtx);
         closed = true;
         work_cv.notify_all();
         lock.unlock();

         for (auto &t : threads)
            if (t.joinable()) t.join();
      }

     private:
      std::vector<std::thread> threads;
      std::unordered_map<size_t, std::function<void(const size_t)>> queue;
      std::atomic<size_t> front_index{};
      std::atomic<size_t> last_index{};
      std::atomic<unsigned int> working = 0;
      bool closed = false;
      std::mutex mtx;
      std::condition_variable work_cv;
      std::condition_variable done_cv;

      void worker(const size_t thread_number)
      {
         while (true) {
            // Wait for work
            std::unique_lock<std::mutex> lock(mtx);
            work_cv.wait(lock, [this]() {
               return closed || !(front_index == last_index);
            });
            if (queue.empty()) {
               if (closed) {
                  return;
               }
               continue;
            }

            // Grab work
            ++working;
            auto work = queue.find(front_index++);
            lock.unlock();

            work->second(thread_number);
            
            lock.lock();
            queue.erase(work);

            // Notify that work is finished
            --working;
            done_cv.notify_all();
         }
      }
   };
}
