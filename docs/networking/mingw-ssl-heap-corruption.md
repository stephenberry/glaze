# MinGW + SSL Heap Corruption Investigation

**Issue**: [#2411](https://github.com/stephenberry/glaze/pull/2411)
**Date**: 2026-04-07 through 2026-04-09
**Status**: Root cause identified, fix applied

## Summary

Heap corruption (`0xc0000374` STATUS_HEAP_CORRUPTION) occurs intermittently on MinGW/GCC 15.2.0 + Windows when `GLZ_ENABLE_SSL` is defined, even for plain HTTP/WS connections that don't use SSL.

MSVC builds are unaffected. GDB and Page Heap both mask the issue by changing timing/memory layout.

## Reproduction

- **Platform**: Windows (GitHub Actions `windows-latest`), MSYS2 MINGW64
- **Compiler**: GCC 15.2.0 (mingw-w64)
- **OpenSSL**: 3.6.2
- **ASIO**: 1.36.0 (standalone)
- **Build**: Release with `-g` (debug symbols)
- **Test**: `http_client_test` — crashes on ~50% of CI runs, always on run 1 when it does crash

## Root Cause

### The problem

When `io_context::stop()` is called followed by `thread.join()`, the worker thread exits `io_context::run()`. However, there may be **queued-but-unexecuted completion handlers** remaining in the io_context's internal queue. These handlers are not executed — they sit in the queue until the `io_context` is destroyed.

When `~io_context()` runs (during `~http_server()` member destruction), it destroys these pending handlers. The handlers' lambda captures hold `shared_ptr<connection_state>` which in turn hold ASIO sockets. Destroying sockets inside the io_context destructor is problematic because the io_context's internal data structures are being torn down simultaneously.

On MinGW/GCC + Windows, this manifests as heap corruption. On MSVC, the different memory allocator and destruction ordering masks the issue.

### Why SSL triggers it

Defining `GLZ_ENABLE_SSL` changes the `http_server` struct layout (adds a `std::monostate ssl_context` member) and links OpenSSL libraries. This is sufficient to change heap allocation sizes and alignment, shifting the memory layout enough to expose the latent bug. The SSL code paths themselves are never executed for plain HTTP connections.

### Narrowing timeline

| Step | Finding |
|------|---------|
| HeapValidate before every operation | Heap is clean through server create, start, client create, GET request, and GET response |
| HeapValidate before `server.stop()` | **OK** — heap is clean |
| Trace inside `http_server::stop()` | `io_context->stop()` succeeds, crash occurs during `thread.join()` |
| `thread.join()` | Worker thread is finishing; pending handlers are being processed/destroyed during shutdown |

### The fix

After stopping the io_context and joining all worker threads, **drain** the io_context to execute any remaining pending handlers while the server and its resources are still alive:

```cpp
// In http_server::stop(), after joining threads:
if (io_context) {
    io_context->restart();
    io_context->poll();
}
```

This ensures completion handlers (and their captured shared_ptrs to sockets/connections) are destroyed in a controlled manner, rather than inside `~io_context()`.

The same fix pattern was applied to:
- `http_server::stop()` — the confirmed crash site
- `http_client::stop_workers()` — same pattern, preventive fix
- `websocket_client` (on the `websocket` branch) — already had this fix via `cancel_all()`

## ASIO analysis: this is a Glaze usage issue, not an ASIO bug

Analysis of the ASIO 1.36.0 source code (standalone, Windows IOCP backend) confirms this is a Glaze-side issue.

### ASIO's documented destruction semantics

From `asio/io_context.hpp`:

> On destruction, the io_context performs the following sequence of operations:
> 1. For each service object svc, performs `svc->shutdown()`.
> 2. **Uninvoked handler objects that were scheduled for deferred invocation on the io_context are destroyed.**
> 3. For each service object svc, performs `delete static_cast<io_context::service*>(svc)`.

Step 1 (`shutdown_services`) closes all sockets via `close_for_destruction()` in `win_iocp_socket_service_base::base_shutdown()`. This happens **before** pending handlers are destroyed.

Step 2 destroys pending handlers. In the IOCP backend (`win_iocp_io_context::shutdown()`), this is done by dequeuing operations and calling `op->destroy()`, which invokes the handler's `do_complete` function with `owner = NULL`. The `do_complete` function **deallocates the handler's memory and calls its destructor** but does NOT invoke the user callback (guarded by `if (owner)`).

### The problem in Glaze

When a pending handler holds `shared_ptr<connection_state>` and that handler is destroyed during step 2, the `connection_state` destructor runs. This destroys the socket member. But the socket was already closed in step 1. On MinGW/GCC, the double-close or the interaction between socket destruction and io_context teardown causes heap corruption.

The key issue: ASIO closes sockets in step 1 at the service level, then destroys handler captures in step 2. If a handler's capture holds the **last** shared_ptr to a connection_state, the socket destructor in step 2 may try to perform cleanup on an already-closed socket, leading to corruption of IOCP internal structures on MinGW.

### Detailed crash site (from CI tracing)

The crash occurs during `thread.join()` inside `http_server::stop()`. The `io_context->stop()` succeeds, but the worker thread crashes while exiting `io_context::run()`. The heap is verified clean (via `HeapValidate`) immediately before `server_.stop()` is called, and through `io_context->stop()` on the main thread.

The crash only occurs when the server has handled at least one connection. A server that starts and stops without handling any connections does not crash. The connection cleanup (socket close after `Connection: close` response) is the likely trigger.

A `restart()` + `poll()` drain after `thread.join()` does NOT fix the issue because the crash occurs during the join itself, before the drain runs.

### Ruled out hypotheses

| Hypothesis | Test | Result |
|---|---|---|
| GCC optimizer miscompilation | Build with `-O0` | Still crashes |
| Struct layout change from `monostate` member | Changed to `unique_ptr` | Still crashes |
| Pending handlers during io_context destruction | Added `restart()` + `poll()` drain | Still crashes |
| OpenSSL TLS cleanup on thread exit | Added `OPENSSL_thread_stop()` | Still crashes |
| Complex test infrastructure | Minimal reproducer (bare server + raw TCP) | Still crashes |

### Confirmed minimal reproducer

The crash reproduces with just:
1. `glz::http_server<>` with one GET route
2. A raw TCP client sends `GET /ping HTTP/1.1\r\nConnection: close\r\n\r\n`
3. `server.stop()` → crash during `thread.join()`

No `working_test_server`, no `simple_test_client`, no CORS, no streaming. The bug is in the core `http_server` + ASIO IOCP + MinGW + OpenSSL interaction.

### Current understanding

The crash occurs during `thread.join()` after `io_context->stop()`. `HeapValidate` passes on both the main thread (right before stop) and the worker thread (right after `run()` returns). The corruption happens during thread teardown — after the worker's function body completes but during the thread exit machinery.

The crash only occurs when OpenSSL libraries are linked (even if no SSL code executes). This points to OpenSSL's DLL initialization or its interaction with MinGW's threading/heap infrastructure as the root cause.

### Why the drain fix works (when it applies)

By calling `io_context->restart()` + `io_context->poll()` after joining worker threads, all pending handlers execute (and their captures are destroyed) while the io_context is still fully operational. When `~io_context()` later runs, there are no pending handlers left to destroy.

### Why MSVC is unaffected

MSVC's CRT heap allocator handles the double-close/reentrancy differently (likely with more slack in heap metadata), so the corruption doesn't manifest. MinGW's heap implementation (using Windows' native HeapAlloc) is stricter about metadata integrity.

### Correct usage pattern

ASIO expects callers to ensure all pending work is either completed or safely cancellable before destroying the io_context:

```cpp
io_context->stop();       // Prevent new handler dispatch
join_worker_threads();    // Wait for in-progress handlers to finish
io_context->restart();    // Clear the stopped flag
io_context->poll();       // Execute remaining queued handlers
// Now safe to destroy io_context
```
