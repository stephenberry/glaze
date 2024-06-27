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
      co_return x* x;
   };

   // Var args allows you to pass in tasks with different return types and returns
   // the result as a std::tuple.
   auto tuple_results = glz::sync_wait(glz::when_all(square(2), twice(10)));

   auto first = std::get<0>(tuple_results).return_value();
   auto second = std::get<1>(tuple_results).return_value();

   expect(first == 4);
   expect(second == 20);
};

suite event = [] {
   std::cout << "\nEvent test:\n";
   glz::event e;

   // These tasks will wait until the given event has been set before advancing.
   auto make_wait_task = [](const glz::event& e, uint64_t i) -> glz::task<void> {
      std::cout << "task " << i << " is waiting on the event...\n";
      co_await e;
      std::cout << "task " << i << " event triggered, now resuming.\n";
      co_return;
   };

   // This task will trigger the event allowing all waiting tasks to proceed.
   auto make_set_task = [](glz::event& e) -> glz::task<void> {
      std::cout << "set task is triggering the event\n";
      e.set();
      co_return;
   };

   // Given more than a single task to synchronously wait on, use when_all() to execute all the
   // tasks concurrently on this thread and then sync_wait() for them all to complete.
   glz::sync_wait(glz::when_all(make_wait_task(e, 1), make_wait_task(e, 2), make_wait_task(e, 3), make_set_task(e)));
};

suite latch = [] {
   std::cout << "\nLatch test:\n";
   // Complete worker tasks faster on a thread pool, using the io_scheduler version so the worker
   // tasks can yield for a specific amount of time to mimic difficult work.  The pool is only
   // setup with a single thread to showcase yield_for().
   glz::io_scheduler tp{glz::io_scheduler::options{.pool = glz::thread_pool::options{.thread_count = 1}}};

   // This task will wait until the given latch setters have completed.
   auto make_latch_task = [](glz::latch& l) -> glz::task<void> {
      // It seems like the dependent worker tasks could be created here, but in that case it would
      // be superior to simply do: `co_await coro::when_all(tasks);`
      // It is also important to note that the last dependent task will resume the waiting latch
      // task prior to actually completing -- thus the dependent task's frame could be destroyed
      // by the latch task completing before it gets a chance to finish after calling resume() on
      // the latch task!

      std::cout << "latch task is now waiting on all children tasks...\n";
      co_await l;
      std::cout << "latch task dependency tasks completed, resuming.\n";
      co_return;
   };

   // This task does 'work' and counts down on the latch when completed.  The final child task to
   // complete will end up resuming the latch task when the latch's count reaches zero.
   auto make_worker_task = [](glz::io_scheduler& tp, glz::latch& l, int64_t i) -> glz::task<void> {
      // Schedule the worker task onto the thread pool.
      co_await tp.schedule();
      std::cout << "worker task " << i << " is working...\n";
      // Do some expensive calculations, yield to mimic work...!  Its also important to never use
      // std::this_thread::sleep_for() within the context of coroutines, it will block the thread
      // and other tasks that are ready to execute will be blocked.
      co_await tp.yield_for(std::chrono::milliseconds{i * 20});
      std::cout << "worker task " << i << " is done, counting down on the latch\n";
      l.count_down();
      co_return;
   };

   const int64_t num_tasks{5};
   glz::latch l{num_tasks};
   std::vector<glz::task<void>> tasks{};

   // Make the latch task first so it correctly waits for all worker tasks to count down.
   tasks.emplace_back(make_latch_task(l));
   for (int64_t i = 1; i <= num_tasks; ++i) {
      tasks.emplace_back(make_worker_task(tp, l, i));
   }

   // Wait for all tasks to complete.
   glz::sync_wait(glz::when_all(std::move(tasks)));
};

suite mutex_test = [] {
   std::cout << "\nMutex test:\n";

   glz::thread_pool tp{glz::thread_pool::options{.thread_count = 4}};
   std::vector<uint64_t> output{};
   glz::mutex mutex;

   auto make_critical_section_task = [&](uint64_t i) -> glz::task<void> {
      co_await tp.schedule();
      // To acquire a mutex lock co_await its lock() function.  Upon acquiring the lock the
      // lock() function returns a coro::scoped_lock that holds the mutex and automatically
      // unlocks the mutex upon destruction.  This behaves just like std::scoped_lock.
      {
         auto scoped_lock = co_await mutex.lock();
         output.emplace_back(i);
      } // <-- scoped lock unlocks the mutex here.
      co_return;
   };

   const size_t num_tasks{100};
   std::vector<glz::task<void>> tasks{};
   tasks.reserve(num_tasks);
   for (size_t i = 1; i <= num_tasks; ++i) {
      tasks.emplace_back(make_critical_section_task(i));
   }

   glz::sync_wait(glz::when_all(std::move(tasks)));

   // The output will be variable per run depending on how the tasks are picked up on the
   // thread pool workers.
   for (const auto& value : output) {
      std::cout << value << ", ";
   }
};

suite shared_mutex_test = [] {
   // Shared mutexes require an excutor type to be able to wake up multiple shared waiters when
   // there is an exclusive lock holder releasing the lock.  This example uses a single thread
   // to also show the interleaving of coroutines acquiring the shared lock in shared and
   // exclusive mode as they resume and suspend in a linear manner.  Ideally the thread pool
   // executor would have more than 1 thread to resume all shared waiters in parallel.
   auto tp = std::make_shared<glz::thread_pool>(glz::thread_pool::options{.thread_count = 1});
   glz::shared_mutex mutex{tp};

   auto make_shared_task = [&](uint64_t i) -> glz::task<void> {
      co_await tp->schedule();
      {
         std::cerr << "shared task " << i << " lock_shared()\n";
         auto scoped_lock = co_await mutex.lock_shared();
         std::cerr << "shared task " << i << " lock_shared() acquired\n";
         /// Immediately yield so the other shared tasks also acquire in shared state
         /// while this task currently holds the mutex in shared state.
         co_await tp->yield();
         std::cerr << "shared task " << i << " unlock_shared()\n";
      }
      co_return;
   };

   auto make_exclusive_task = [&]() -> glz::task<void> {
      co_await tp->schedule();

      std::cerr << "exclusive task lock()\n";
      auto scoped_lock = co_await mutex.lock();
      std::cerr << "exclusive task lock() acquired\n";
      // Do the exclusive work..
      std::cerr << "exclusive task unlock()\n";
      co_return;
   };

   // Create 3 shared tasks that will acquire the mutex in a shared state.
   const size_t num_tasks{3};
   std::vector<glz::task<void>> tasks{};
   for (size_t i = 1; i <= num_tasks; ++i) {
      tasks.emplace_back(make_shared_task(i));
   }
   // Create an exclusive task.
   tasks.emplace_back(make_exclusive_task());
   // Create 3 more shared tasks that will be blocked until the exclusive task completes.
   for (size_t i = num_tasks + 1; i <= num_tasks * 2; ++i) {
      tasks.emplace_back(make_shared_task(i));
   }

   glz::sync_wait(glz::when_all(std::move(tasks)));
};

int main() { return 0; }
