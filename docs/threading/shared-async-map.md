# glz::shared_async_map

## Features

- **Thread Safety:** Automatically manages synchronization, allowing safe concurrent insertions, deletions, and accesses.
- **Flat Map Structure:** Utilizes a sorted `std::vector` for storage, ensuring cache-friendly access patterns and efficient lookups via binary search.
- **Iterator Support:** Provides both mutable and immutable iterators with built-in synchronization.
- **Value Proxies:** Offers proxy objects to manage access to values, ensuring that locks are held appropriately during value manipulation.
- **Avoids Manual Mutex Management:** Developers do not need to explicitly handle mutexes, reducing the risk of synchronization bugs.

## Why Use `shared_async_map`?

In multi-threaded applications, managing shared data structures often requires explicit synchronization using mutexes. This can be error-prone and cumbersome, leading to potential deadlocks, race conditions, and other concurrency issues. `glz::shared_async_map` abstracts away the complexity of mutex management by internally handling synchronization, allowing developers to focus on the core logic of their applications.

### Benefits Over Manual Mutex Management

1. **Simplified Code:** Eliminates the need for boilerplate mutex locking and unlocking, resulting in cleaner and more readable code.
2. **Reduced Risk of Errors:** Minimizes the chances of common synchronization mistakes such as forgetting to unlock a mutex or locking in the wrong order.
3. **Automatic Lock Handling:** Ensures that appropriate locks are held during operations, maintaining data integrity without developer intervention.
4. **Optimized Performance:** Uses `std::shared_mutex` to allow multiple concurrent readers, improving performance in read-heavy scenarios.

### Additional Benefits

- **Cache Efficiency:** The use of a sorted `std::vector` ensures better cache locality compared to node-based structures like `std::map`.
- **Memory Overhead:** Lower memory footprint due to the contiguous storage of elements.
- **Flexible Access Patterns:** Supports both direct access via `operator[]` and range-based iteration with iterators.
- **Exception Safety:** Provides strong exception guarantees, ensuring that the internal state remains consistent even in the presence of exceptions.

## Implementation Details

### Internal Structure

- **Storage:** Elements are stored as `std::unique_ptr<std::pair<K, V>>` within a sorted `std::vector`. The sorting is based on the keys, enabling efficient binary search operations.
- **Synchronization:** A `std::shared_mutex` guards the internal vector, allowing multiple threads to read concurrently while ensuring exclusive access for modifications.

### Key Components

1. **Shared State (`shared_state`):**
   - Contains the `std::vector` of elements and the `std::shared_mutex`.
   - Shared among all instances of the map via `std::shared_ptr`.
2. **Iterators (`iterator` and `const_iterator`):**
   - Provide access to the elements with built-in synchronization.
   - Acquire shared locks to ensure thread-safe traversal.
3. **Value Proxies (`value_proxy` and `const_value_proxy`):**
   - Manage access to the values, holding necessary locks to ensure safe manipulation.

### Thread Safety Mechanisms

- **Read Operations:**
  - Utilize `std::shared_lock` to allow multiple concurrent readers.
  - Methods like `find`, `at`, `count`, and `contains` acquire shared locks.
- **Write Operations:**
  - Utilize `std::unique_lock` to ensure exclusive access during modifications.
  - Methods like `insert`, `emplace`, `erase`, and `clear` acquire unique locks.

## Internal Operations

### Binary Search for Key Lookup

The `shared_async_map` maintains its elements in a sorted `std::vector`, enabling efficient binary search operations for key lookup. This ensures that operations like `find`, `insert`, and `erase` have logarithmic time complexity (`O(log n)`).

### Lock Management

- **Shared Locks (`std::shared_lock`):** Acquired for read-only operations, allowing multiple threads to read concurrently.
- **Unique Locks (`std::unique_lock`):** Acquired for write operations, ensuring exclusive access to modify the map.

### Iterator Locking

Iterators hold shared locks to guarantee that the underlying data remains consistent during traversal. When an iterator is dereferenced or incremented, the lock is maintained to prevent data races and ensure thread safety.

## Exception Safety

All public member functions of `shared_async_map` provide strong exception safety guarantees. In the event of an exception (e.g., during memory allocation or key comparisons), the internal state of the map remains consistent, and no resources are leaked.

## Limitations

- **Element Type Requirements:** The value type `V` must be thread-safe, as `shared_async_map` assumes that concurrent access to individual elements does not require additional synchronization.
- **Performance Overhead:** While `shared_async_map` optimizes for concurrent access, the use of shared locks and dynamic memory allocations (via `std::unique_ptr`) may introduce some performance overhead compared to non-thread-safe containers.
- **Insertion Order:** The map maintains elements in sorted order based on keys, which may not be suitable for scenarios where insertion order needs to be preserved.
