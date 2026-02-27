# MSVC CI Investigation Notes

## Scope
This document summarizes the MSVC CI failures investigated in this branch and what was verified.

## Issues Investigated
- `ordered_small_map` template error (`C2679`) in `insert_or_assign` with explicit constructors.
- YAML `C4702` unreachable-code warnings on MSVC.
- Windows target macro warnings (`_WIN32_WINNT` not defined).
- `getenv` deprecation warnings (`C4996`) on MSVC.
- Networking test failures in websocket/WSS tests.
- Repeated MSVC segfaults in `asio_repe` and its split test groups.

## What Was Tested
- Full MSVC CI runs with the networking test suite.
- Focused reruns of:
  - `asio_repe`
  - `asio_repe_minimal`
  - split groups (`connection_state`, `send_receive`, `async_core`, `async_error`, `custom_call`) during isolation.
- Added startup trace points to isolate the `asio_repe` crash location.

## Key Observations
- `asio_repe_minimal` consistently passed while richer `asio_repe` groups segfaulted.
- Trace output repeatedly stopped at:
  - `server.run begin`
  - `server.run listening`
  - `server.run after on_listen`
  - `server.run before listener co_spawn`
- No trace after that point, indicating the crash was triggered in or immediately around listener `co_spawn` setup on MSVC.

## Fixes That Held in CI
- `ordered_small_map`/`ordered_map` `insert_or_assign` now uses constraints aligned with `std::map`-style requirements (`is_constructible` + `is_assignable`).
- YAML warning suppressions were scoped for MSVC where constexpr-generated unreachable-code warnings occur.
- Windows target macro default added only when user/project has not already defined `_WIN32_WINNT`/`_WIN32_WINDOWS`.
- MSVC-safe env var retrieval moved to `_dupenv_s` where needed.
- `OPENSSL_Applink` handling added for MSVC WSS test path.
- `shared_context_bug_test` was hardened for dynamic loopback port selection and more reliable connect/message waits.
- `asio_repe` MSVC crash was resolved by switching Windows listener/session handling to callback-based `async_accept`/`async_read`/`async_write` flow (no listener `co_spawn` on `_WIN32`).

## What We Learned
- The `asio_repe` crash was not random test logic flakiness; it was reproducibly tied to the listener coroutine spawn path on MSVC.
- Isolating by test groups plus targeted trace points was the fastest way to locate the exact startup boundary.
- Keeping Windows-specific startup behavior separate from non-Windows coroutine flow is a pragmatic stability tradeoff for this code path.

## Follow-up Guidance
- Keep the Windows listener/session callback path unless/until a minimal MSVC coroutine reproducer is understood and resolved.
- If reintroducing coroutine listener startup on Windows later, gate it behind a targeted CI validation step for `asio_repe`.
