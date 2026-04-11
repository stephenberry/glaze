# MinGW + OpenSSL Heap Corruption

**Issue**: [#2411](https://github.com/stephenberry/glaze/pull/2411), [#2448](https://github.com/stephenberry/glaze/pull/2448)
**Date**: 2026-04-07 through 2026-04-11
**Status**: Root cause identified — OpenSSL + MinGW runtime interaction bug (not a Glaze bug)

## Summary

Heap corruption (`0xc0000374` STATUS_HEAP_CORRUPTION) occurs intermittently on MinGW/GCC 15.2.0 + Windows when OpenSSL is linked, during `http_server::stop()` → `thread.join()`. The crash happens after an ASIO worker thread has cleanly exited `io_context::run()`, during thread teardown.

**This is not a Glaze bug.** The crash occurs purely from linking OpenSSL libraries — even when `GLZ_ENABLE_SSL` is not defined, no SSL headers are included, and no SSL code is compiled. MSVC builds are unaffected.

## Root Cause

OpenSSL's DLL initialization or its interaction with MinGW's threading/heap infrastructure corrupts the heap when ASIO worker threads exit after handling connections.

### Evidence (A/B/C test)

| Test | GLZ_ENABLE_SSL | OpenSSL linked | Crashes? |
|------|---------------|----------------|----------|
| ssl-enabled | YES | dynamic | YES |
| link-only | **NO** | dynamic | **YES** |
| static-ssl | YES | static | YES |
| msys2 (no OpenSSL) | NO | **not linked** | **NO** |

The `link-only` test compiles with `glaze_ENABLE_SSL=OFF` at the CMake level — no SSL headers, no SSL types, no `GLZ_ENABLE_SSL` macro. The only difference from the passing `msys2` workflow is that OpenSSL libraries are linked. This is sufficient to cause the crash.

### Crash characteristics

- Occurs during `std::thread::join()` in `http_server::stop()` 
- `HeapValidate(GetProcessHeap(), 0, NULL)` passes on the main thread immediately before `stop()`
- `HeapValidate` passes on the worker thread right after `io_context::run()` returns
- The corruption happens between the worker's function body returning and `join()` completing — during thread teardown (TLS destructors, CRT cleanup)
- Only occurs when the server has handled at least one HTTP connection
- Intermittent: ~50-70% reproduction rate per CI run
- GDB and Page Heap both mask the issue by changing timing/memory layout
- Build optimization level irrelevant (`-O0` still crashes)

## Reproduction

- **Platform**: Windows (GitHub Actions `windows-latest`), MSYS2 MINGW64
- **Compiler**: GCC 15.2.0 (mingw-w64)
- **OpenSSL**: 3.6.2 (both dynamic and static)
- **ASIO**: 1.36.0 (standalone)

### Minimal reproducer

```cpp
// Link against OpenSSL (even without GLZ_ENABLE_SSL defined)
#include "glaze/net/http_server.hpp"

int main() {
    for (int round = 0; round < 20; ++round) {
        glz::http_server<> server;
        server.get("/ping", [](const glz::request&, glz::response& res) {
            res.status(200).body("pong");
        });
        server.bind("127.0.0.1", 19300);
        std::thread t([&]() { server.start(1); });

        // Wait for server, then send one GET request with Connection: close
        // ...

        server.stop();   // crash during thread.join() inside stop()
        t.join();
    }
}
```

## Ruled out hypotheses

| Hypothesis | Test | Result |
|---|---|---|
| `GLZ_ENABLE_SSL` macro / template changes | Built with `glaze_ENABLE_SSL=OFF` + OpenSSL linked | Still crashes |
| DLL boundary / CRT heap mismatch | Static OpenSSL linking | Still crashes |
| GCC optimizer miscompilation | Build with `-O0` | Still crashes |
| `http_server` struct layout change | Removed `ssl_context` member via base class | Still crashes |
| Pending handlers during io_context destruction | Added `restart()` + `poll()` drain | Still crashes |
| OpenSSL TLS cleanup on thread exit | Added `OPENSSL_thread_stop()` | Still crashes |
| Complex test infrastructure | Minimal reproducer (bare server + raw TCP) | Still crashes |
| ASIO misuse | Analyzed ASIO source — destruction semantics are correct | N/A |

## Recommendations

1. **Do not use OpenSSL with MinGW/GCC on Windows for production** until this is resolved upstream
2. **MSVC builds are unaffected** and should be used for Windows + SSL deployments  
3. The MinGW SSL CI workflow (`msys2-ssl.yml`) documents and tracks this issue
4. Consider reporting to [MSYS2](https://github.com/msys2/MSYS2-packages/issues) and/or [OpenSSL](https://github.com/openssl/openssl/issues)

## ASIO analysis (for reference)

Analysis of ASIO 1.36.0 source confirmed that ASIO's io_context destruction semantics are correct:

1. `shutdown_services()` closes all sockets via `close_for_destruction()`
2. Pending handlers are destroyed (not invoked) via `op->destroy()`
3. Services are deleted

The drain pattern (`restart()` + `poll()` after joining threads) is still good practice to ensure clean shutdown, even though it doesn't fix this particular issue:

```cpp
io_context->stop();
join_worker_threads();
io_context->restart();
io_context->poll();
```
