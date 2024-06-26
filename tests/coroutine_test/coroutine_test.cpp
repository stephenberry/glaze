// Glaze Library
// For the license information refer to glaze.hpp

#define UT_RUN_TIME_ONLY

#include "glaze/coroutine.hpp"

#include "ut/ut.hpp"

using namespace ut;

suite generator = [] {
   std::atomic<uint64_t> result{};
   auto task = [&](uint64_t count_to) -> glz::task<void> {
      // Create a generator function that will yield and incrementing
      // number each time its called.
      auto gen = []() -> glz::generator<uint64_t> {
         uint64_t i = 0;
         while (true) {
            co_yield i;
            ++i;
         }
      };

      // Generate the next number until its greater than count to.
      for (auto val : gen()) {
         // std::cout << val << ", ";
         result += val;

         if (val >= count_to) {
            break;
         }
      }
      co_return;
   };

   glz::sync_wait(task(100));

   expect(result == 5050) << result;
};

suite thread_pool = [] {
   // This lambda will create a glz::task that returns a unit64_t.
   // It can be invoked many times with different arguments.
   auto make_task_inline = [](uint64_t x) -> glz::task<uint64_t> { co_return x + x; };

   // This will block the calling thread until the created task completes.
   // Since this task isn't scheduled on any glz::thread_pool or glz::io_scheduler
   // it will execute directly on the calling thread.
   auto result = glz::sync_wait(make_task_inline(5));
   expect(result == 10);
   // std::cout << "Inline Result = " << result << "\n";

   // We'll make a 1 thread glz::thread_pool to demonstrate offloading the task's
   // execution to another thread.  We'll capture the thread pool in the lambda,
   // note that you will need to guarantee the thread pool outlives the coroutine.
   glz::thread_pool tp{glz::thread_pool::options{.thread_count = 1}};

   auto make_task_offload = [&tp](uint64_t x) -> glz::task<uint64_t> {
      co_await tp.schedule(); // Schedules execution on the thread pool.
      co_return x + x; // This will execute on the thread pool.
   };

   // This will still block the calling thread, but it will now offload to the
   // glz::thread_pool since the coroutine task is immediately scheduled.
   result = glz::sync_wait(make_task_offload(10));
   expect(result == 20);
   // std::cout << "Offload Result = " << result << "\n";
};

suite when_all = [] {
   // Create a thread pool to execute all the tasks in parallel.
   glz::thread_pool tp{glz::thread_pool::options{.thread_count = 4}};
   // Create the task we want to invoke multiple times and execute in parallel on the thread pool.
   auto twice = [&](uint64_t x) -> glz::task<uint64_t> {
      co_await tp.schedule(); // Schedule onto the thread pool.
      co_return x + x; // Executed on the thread pool.
   };

   // Make our tasks to execute, tasks can be passed in via a std::ranges::range type or var args.
   std::vector<glz::task<uint64_t>> tasks{};
   for (std::size_t i = 0; i < 5; ++i) {
      tasks.emplace_back(twice(i + 1));
   }

   // Synchronously wait on this thread for the thread pool to finish executing all the tasks in parallel.
   auto results = glz::sync_wait(glz::when_all(std::move(tasks)));
   expect(results[0].return_value() == 2);
   expect(results[1].return_value() == 4);
   expect(results[2].return_value() == 6);
   expect(results[3].return_value() == 8);
   expect(results[4].return_value() == 10);

   // Use var args instead of a container as input to glz::when_all.
   auto square = [&](uint8_t x) -> glz::task<uint8_t> {
      co_await tp.schedule();
      co_return x * x;
   };

   // Var args allows you to pass in tasks with different return types and returns
   // the result as a std::tuple.
   auto tuple_results = glz::sync_wait(glz::when_all(square(2), twice(10)));

   auto first = std::get<0>(tuple_results).return_value();
   auto second = std::get<1>(tuple_results).return_value();

   expect(first == 4);
   expect(second == 20);
};

int main() { return 0; }
