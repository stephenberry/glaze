// Glaze Library
// For the license information refer to glaze.hpp

#define UT_RUN_TIME_ONLY

#include "glaze/coroutine.hpp"

#include "glaze/network.hpp"
#include "ut/ut.hpp"

using namespace ut;

#define TEST_ALL

#ifdef TEST_ALL
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
   // Since this task isn't scheduled on any glz::thread_pool or glz::scheduler
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
   // Complete worker tasks faster on a thread pool, using the scheduler version so the worker
   // tasks can yield for a specific amount of time to mimic difficult work.  The pool is only
   // setup with a single thread to showcase yield_for().
   glz::scheduler tp{glz::scheduler::options{.pool = glz::thread_pool::options{.thread_count = 1}}};

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
   auto make_worker_task = [](glz::scheduler& tp, glz::latch& l, int64_t i) -> glz::task<void> {
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
   std::cout << "\nShared Mutex test:\n";
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

suite semaphore_test = [] {
   std::cout << "\nSemaphore test:\n";
   // Have more threads/tasks than the semaphore will allow for at any given point in time.
   glz::thread_pool tp{glz::thread_pool::options{.thread_count = 8}};
   glz::semaphore semaphore{1};

   auto make_rate_limited_task = [&](uint64_t task_num) -> glz::task<void> {
      co_await tp.schedule();

      // This will only allow 1 task through at any given point in time, all other tasks will
      // await the resource to be available before proceeding.
      auto result = co_await semaphore.acquire();
      if (result == glz::semaphore::acquire_result::acquired) {
         std::cout << task_num << ", ";
         semaphore.release();
      }
      else {
         std::cout << task_num << " failed to acquire semaphore [" << glz::semaphore::to_string(result) << "],";
      }
      co_return;
   };

   const size_t num_tasks{100};
   std::vector<glz::task<void>> tasks{};
   for (size_t i = 1; i <= num_tasks; ++i) {
      tasks.emplace_back(make_rate_limited_task(i));
   }

   glz::sync_wait(glz::when_all(std::move(tasks)));
};

suite ring_buffer_test = [] {
   std::cout << "\nRing Buffer test:\n";

   const size_t iterations = 100;
   const size_t consumers = 4;
   glz::thread_pool tp{glz::thread_pool::options{.thread_count = 4}};
   glz::ring_buffer<uint64_t, 16> rb{};
   glz::mutex m{};

   std::vector<glz::task<void>> tasks{};

   auto make_producer_task = [&]() -> glz::task<void> {
      co_await tp.schedule();

      for (size_t i = 1; i <= iterations; ++i) {
         co_await rb.produce(i);
      }

      // Wait for the ring buffer to clear all items so its a clean stop.
      while (!rb.empty()) {
         co_await tp.yield();
      }

      // Now that the ring buffer is empty signal to all the consumers its time to stop.  Note that
      // the stop signal works on producers as well, but this example only uses 1 producer.
      {
         auto scoped_lock = co_await m.lock();
         std::cerr << "\nproducer is sending stop signal";
      }
      rb.notify_waiters();
      co_return;
   };

   auto make_consumer_task = [&](size_t id) -> glz::task<void> {
      co_await tp.schedule();

      while (true) {
         auto expected = co_await rb.consume();
         auto scoped_lock = co_await m.lock(); // just for synchronizing std::cout/cerr
         if (!expected) {
            std::cerr << "\nconsumer " << id << " shutting down, stop signal received";
            break; // while
         }
         else {
            auto item = std::move(*expected);
            std::cout << "(id=" << id << ", v=" << item << "), ";
         }

         // Mimic doing some work on the consumed value.
         co_await tp.yield();
      }

      co_return;
   };

   // Create N consumers
   for (size_t i = 0; i < consumers; ++i) {
      tasks.emplace_back(make_consumer_task(i));
   }
   // Create 1 producer.
   tasks.emplace_back(make_producer_task());

   // Wait for all the values to be produced and consumed through the ring buffer.
   glz::sync_wait(glz::when_all(std::move(tasks)));
};
#endif

//#define SERVER_CLIENT_TEST

#ifdef SERVER_CLIENT_TEST
suite server_client_test = [] {
   std::cout << "\n\nServer/Client test:\n";

   auto scheduler = std::make_shared<glz::scheduler>(glz::scheduler::options{
      // The scheduler will spawn a dedicated event processing thread.  This is the default, but
      // it is possible to use 'manual' and call 'process_events()' to drive the scheduler yourself.
      .thread_strategy = glz::thread_strategy::spawn,
      // If the scheduler is in spawn mode this functor is called upon starting the dedicated
      // event processor thread.
      .on_io_thread_start_functor = [] { std::cout << "scheduler::process event thread start\n"; },
      // If the scheduler is in spawn mode this functor is called upon stopping the dedicated
      // event process thread.
      .on_io_thread_stop_functor = [] { std::cout << "scheduler::process event thread stop\n"; },
      // The io scheduler can use a coro::thread_pool to process the events or tasks it is given.
      // You can use an execution strategy of `process_tasks_inline` to have the event loop thread
      // directly process the tasks, this might be desirable for small tasks vs a thread pool for large tasks.
      .pool =
         glz::thread_pool::options{
            .thread_count = 1,
            .on_thread_start_functor =
               [](size_t i) { std::cout << "scheduler::thread_pool worker " << i << " starting\n"; },
            .on_thread_stop_functor =
               [](size_t i) { std::cout << "scheduler::thread_pool worker " << i << " stopping\n"; },
         },
      .execution_strategy = glz::scheduler::execution_strategy::process_tasks_on_thread_pool});

   auto make_server_task = [&]() -> glz::task<void> {
      // Start by creating a tcp server, we'll do this before putting it into the scheduler so
      // it is immediately available for the client to connect since this will create a socket,
      // bind the socket and start listening on that socket.  See tcp::server for more details on
      // how to specify the local address and port to bind to as well as enabling SSL/TLS.
      glz::server server{scheduler};

      // Now scheduler this task onto the scheduler.
      co_await scheduler->schedule();

      // Wait for an incoming connection and accept it.
      auto poll_status = co_await server.poll();
      if (poll_status != glz::poll_status::event) {
         std::cerr << "Incoming client connection failed!\n" << "Poll Status Detail: " << glz::nameof(poll_status) << '\n';
         co_return; // Handle error, see poll_status for detailed error states.
      }

      auto client = server.accept();

      if (not client.socket->valid()) {
         std::cerr << "Incoming client connection failed!\n";
         co_return; // Handle error.
      }

      // Now wait for the client message, this message is small enough it should always arrive
      // with a single recv() call.
      poll_status = co_await client.poll(glz::poll_op::read);
      if (poll_status != glz::poll_status::event) {
         if (glz::poll_status::closed == glz::poll_status::event) {
            std::cerr << "Error on: co_await client.poll(glz::poll_op::read): client Id, " << client.socket->socket_fd << ", the socket is closed.\n";
         }
         else {
              std::cerr << "Error on: co_await client.poll(glz::poll_op::read): client Id, " << client.socket->socket_fd << ".\nDetails: " << glz::nameof(poll_status) << '\n';
         }
         co_return; // Handle error.
      }

      // Prepare a buffer and recv() the client's message.  This function returns the recv() status
      // as well as a span<char> that overlaps the given buffer for the bytes that were read.  This
      // can be used to resize the buffer or work with the bytes without modifying the buffer at all.
      std::string request(256, '\0');
      auto [ip_status, recv_bytes] = client.recv(request);
      if (ip_status != glz::ip_status::ok) {
         std::cerr << "client::recv error:\n" << "Details: " << glz::nameof(poll_status) << '\n';
         co_return; // Handle error, see net::ip_status for detailed error states.
      }

      request.resize(recv_bytes.size());
      std::cout << "server: " << request << "\n";

      // Make sure the client socket can be written to.
      poll_status = co_await client.poll(glz::poll_op::write);
      if (poll_status != glz::poll_status::event) {
           std::cerr << "Error on: co_await client.poll(glz::poll_op::write): client Id" << client.socket->socket_fd << ".\nDetails: " << glz::nameof(poll_status) << '\n';
         co_return; // Handle error.
      }

      // Send the server response to the client.
      // This message is small enough that it will be sent in a single send() call, but to demonstrate
      // how to use the 'remaining' portion of the send() result this is wrapped in a loop until
      // all the bytes are sent.
      std::string response = "Hello from server.";
      std::span<const char> remaining = response;
      do {
         // Optimistically send() prior to polling.
         auto [ips, r] = client.send(remaining);
         if (ips != glz::ip_status::ok) {
            co_return; // Handle error, see net::ip_status for detailed error states.
         }

         if (r.empty()) {
            break; // The entire message has been sent.
         }

         // Re-assign remaining bytes for the next loop iteration and poll for the socket to be
         // able to be written to again.
         remaining = r;
         poll_status = co_await client.poll(glz::poll_op::write);
         if (poll_status != glz::poll_status::event) {
            co_return; // Handle error.
         }
      } while (true);

      co_return;
   };

   auto make_client_task = [&]() -> glz::task<void> {
      // Immediately schedule onto the scheduler.
      co_await scheduler->schedule();

      // Create the tcp::client with the default settings, see tcp::client for how to set the
      // ip address, port, and optionally enabling SSL/TLS.
      glz::client client{scheduler};

      // Ommitting error checking code for the client, each step should check the status and
      // verify the number of bytes sent or received.

      // Connect to the server.
      if (auto ip_status = co_await client.connect(std::chrono::milliseconds(100)); ip_status != glz::ip_status::ok) {
         std::cerr << "ip_status: " << glz::nameof(ip_status) << '\n';
      }

      // Make sure the client socket can be written to.
      if (auto status = co_await client.poll(glz::poll_op::write); bool(status)) {
         std::cerr << "poll_status: " << glz::nameof(status) << '\n';
      }

      // Send the request data.
      client.send(std::string_view{"Hello from client."});

      // Wait for the response and receive it.
      co_await client.poll(glz::poll_op::read);
      std::string response(256, '\0');
      auto [ip_status, recv_bytes] = client.recv(response);
      response.resize(recv_bytes.size());

      std::cout << "client id " << client.socket->socket_fd << ", recieved: " << response << '\n';
      co_return;
   };

   // Create and wait for the server and client tasks to complete.
   glz::sync_wait(glz::when_all(make_server_task(), make_client_task()));
};
#endif

int main()
{
   std::cout << '\n';
   return 0;
}
