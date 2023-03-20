# Thread Pool

Glaze contains a thread pool for the sake of running studies efficiently across threads using the JSON study code. However, the thread pool is generic and can be used for various applications. It is designed to minimize copies of the data passed to threads.

```c++
glz::pool pool(4); // creates a thread pool that will run on 4 threads
std::atomic<int> x = 0;
auto f = [&] { ++x; }; // some worker function

for (auto i = 0; i < 1000; ++i) {
  pool.emplace_back(f); // run jobs in parallel
}
pool.wait(); // wait until all jobs are completed
expect(x == 1000);
```

The thread pool holds a queue of jobs, which are executed in parallel up to the number of threads designated by the pool's constructor.  If the number of threads is not specified then it uses all threads available.

The example below shows how results can be returned from a worker function and stored with `std::future`.

```c++
glz::pool pool;
auto f = [] {
  std::mt19937_64 generator{};
  std::uniform_int_distribution<size_t> dist{ 0, 100 };
  return dist(generator);
};

std::vector<std::future<size_t>> numbers;
for (auto i = 0; i < 1000; ++i) {
  numbers.emplace_back(pool.emplace_back(f));
}
```

# 
