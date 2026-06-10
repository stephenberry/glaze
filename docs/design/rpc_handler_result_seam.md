# RPC Writer Seam: "Handler Result" vs "Stored Value"

## Overview

The `glz::registry` RPC layer serves two structurally different kinds of values back to a caller, and they must be serialized with different semantics:

- A **handler result** is whatever a registered callable returns. When that return type is `glz::expected<T, E>`, the value is a *success/error channel*: a present value is unwrapped and serialized, and an error is translated into the protocol's native error representation (a REPE error header, a JSON-RPC `error` object, or a non-2xx HTTP status).
- A **stored value** is data reflected from a registered object, data member, or merged view. It is serialized *verbatim*. A `glz::expected<T, E>` here is ordinary data and serializes as its JSON shape (`{"unexpected": ...}` on the error state); it is **not** a failure of the read.

These two cases share almost all of their machinery, and the only thing distinguishing them is *where the value came from* — a fact known at compile time by which endpoint-registration function was selected.

This document proposes a single, enforced seam between the two so that the distinction cannot be accidentally collapsed in one protocol but not another.

### Related issue

[#2265](https://github.com/stephenberry/glaze/issues/2265) — registry/router under `-fno-exceptions`. During review, the `glz::expected`-as-error-channel logic was found living inside the *shared* `write_response` in REPE and JSON-RPC, which meant reading a registered data member of type `glz::expected` was silently reinterpreted as a protocol error. REST never had the bug because it kept the handler-result path (`write_result`) separate from stored-value serialization (`response::body`). The fix brought REPE and JSON-RPC into line, but nothing *structurally* prevents the asymmetry from returning. This note addresses that.

### Goals

1. **One vocabulary across all three protocols** (REPE, JSON-RPC, REST) for "write a handler result" vs "write a stored value", with identical names and a documented contract.
2. **Compile-time enforcement** that every protocol provides both operations — a new protocol cannot ship with only a plain serializer and silently route handler results through it.
3. **A single point of categorization** so the choice "this value is a handler result" vs "this is stored data" is made once, in shared code, rather than re-decided inside each protocol.
4. **Zero runtime overhead** — the distinction is compile-time; no extra branches or allocations in the hot path. See [Performance and compile-time](#performance-and-compile-time) for how each layer preserves this.
5. **Incremental adoption** — each layer is independently valuable and shippable.

### Non-Goals

1. **Unifying the output sinks.** REPE writes a binary message buffer (`repe::state_view`), JSON-RPC builds a JSON string (`jsonrpc::state`), REST mutates an HTTP response (`glz::response`). These stay distinct.
2. **Unifying the error vocabularies.** Each protocol's set of acceptable `E` types is intentionally different (REPE: `error_code`/`error_ctx`/string; JSON-RPC: `rpc::error`/`rpc::error_e`/`error_ctx`/string; REST: `http_error`/`error_code`/`error_ctx`/string). The seam must accommodate per-protocol error sets, not flatten them.
3. **A speculative full rewrite** of the per-protocol endpoint-registration bodies. See the layered plan; the heaviest layer is explicitly optional.

## Background: the two value categories

The generic registration entry point lives in `include/glaze/rpc/registry.hpp` (`registry<Opts, Proto>::on()` → `register_members`). For each reflected member it decides a *category* and dispatches to a same-named static function on the protocol's `registry_impl<Opts, Proto>` specialization:

| Category | Generic dispatch target | `glz::expected` return means |
| --- | --- | --- |
| **Handler result** | `register_function_endpoint`, `register_param_function_endpoint`, `register_member_function_endpoint`, `register_member_function_with_params_endpoint` | success/error channel → translate error |
| **Stored value** | `register_endpoint`, `register_object_endpoint`, `register_value_endpoint`, `register_variable_endpoint`, `register_merge_endpoint` | ordinary data → serialize `{"unexpected": ...}` |

The category is therefore already a compile-time property of *which* `register_*` function the generic layer chose. The seam we want is the guarantee that each of those functions writes its result through the matching path.

Note that REST is a strict subset of this table: it does not implement `register_merge_endpoint`, so registering a `glz::merge<...>` against a REST registry is a hard compile error (the generic layer calls `impl::register_merge_endpoint` unconditionally at `registry.hpp:303`). The seam itself is unaffected, because it concerns the two *writers* rather than which endpoint kinds a protocol offers, but the asymmetry tempers the "the bodies are ~80% identical" claim in Layer 3b.

## Current state

Each protocol independently implements the writers, and the naming has drifted:

| Protocol | Source | Output sink | Stored-value writer | Handler-result writer | Error translator |
| --- | --- | --- | --- | --- | --- |
| REPE | `rpc/repe/repe.hpp`, `repe_registry_impl.hpp` | `repe::state_view` | `write_response<Opts>(value, state)` | `write_handler_result<Opts>(value, state)` | `set_handler_error(state, e)` |
| JSON-RPC | `rpc/jsonrpc_registry_impl.hpp` | `jsonrpc::state` | `write_response<Opts>(value, state)` | `write_handler_result<Opts>(value, state)` | `set_handler_error(state, e)` |
| REST | `net/rest_registry_impl.hpp` | `glz::response` | `response::body<Opts>(value)` *(called inline)* | `write_result<R>(res, result)` | `set_handler_error(res, e)` |

Observations:

- REPE and JSON-RPC now agree (`write_response` / `write_handler_result`) after #2265; REST uses a third name (`write_result`) and has **no named stored-value writer** — it calls `res.body<Opts>(...)` directly at four stored-value GET sites (`rest_registry_impl.hpp` register_endpoint/object/value/variable). The other `res.body<Opts>(...)` uses live inside `write_result` and `set_handler_error`, which are the handler-result and error paths.
- The three writers do not even share an argument order: REPE/JSON-RPC are value-first (`write_handler_result(value, state)`), REST is sink-first (`write_result(res, result)`). Converging on one vocabulary therefore also means converging on one signature shape (see Layer 1).
- There is no contract a `registry_impl` must satisfy. The mapping from "category" to "writer" is re-established by hand inside each protocol's `register_*_endpoint` bodies.

## The gap

The correctness of the seam currently rests entirely on each protocol author calling the right writer inside each registration function. Nothing detects a miswire:

- Putting the `expected` translation back into a shared `write_response` (the original #2265 bug) compiles and passes most tests, because the data-member case is rare and untested by default.
- A new protocol added tomorrow could implement a single serializer and reuse it for both categories, reproducing the exact asymmetry.
- The inconsistent naming (`write_result` vs `write_handler_result`, and an unnamed stored-value path in REST) makes it harder to grep for "is every handler site using the translating writer?" during review.

## Proposed design

### The seam: one vocabulary, two operations

Standardize on two operations, named identically across all protocols, whose *names encode intent*:

```cpp
// Stored data: serialize verbatim. A glz::expected is ordinary data.
template <auto Opts, class V> void write_stored_value(Sink& sink, V&& value);

// Handler result: a glz::expected is a success/error channel; an error is
// translated into this protocol's error representation (see set_handler_error).
template <auto Opts, class V> void write_handler_result(Sink& sink, V&& value);
```

The signatures above are written **sink-first**, matching REST's existing `write_result(response&, R&&)`. That is a deliberate choice the seam must make and apply uniformly, because the protocols disagree today: `write_handler_result` already exists for REPE/JSON-RPC but is *value-first* (`write_handler_result(value, state)`), so adopting the shared signature reorders its parameters. `write_stored_value` is `write_response`/`response::body` given a name. (`Opts` is shown as an explicit template parameter for illustration; in practice each `registry_impl<Opts, Proto>` already carries `Opts` from its enclosing scope.)

The design is delivered in layers. Layers 1–2 are cheap and capture most of the benefit; Layer 3 is the structural guarantee.

### Layer 1 — Converge naming and document the contract

First pick the canonical signature shape and apply it to all three protocols. This note adopts **sink-first** (`(Sink& sink, V&& value)`); REST is already sink-first, REPE and JSON-RPC are not.

- **REST**: rename `write_result` → `write_handler_result` (already sink-first, so a pure rename). Give the stored-value path a name (`write_stored_value`) wrapping `response::body<Opts>(...)`, and replace the four inline `res.body<Opts>(...)` GET sites with it.
- **REPE / JSON-RPC**: rename `write_response` → `write_stored_value` (this touches the value overload, its no-value overload, and every call site), and reorder the handler/stored writers from value-first to sink-first so all three protocols share one signature.
- Add a short doc comment defining the two operations and the `glz::expected` contract at each definition site, cross-referencing this note.

Cost: mostly mechanical, but **not** a pure rename — REPE/JSON-RPC change argument order, so their call sites move with them. No change to *runtime* behavior. Benefit: a single greppable vocabulary with one signature shape; review can verify at a glance that every handler site uses `write_handler_result` and every data site uses `write_stored_value`.

### Layer 2 — A `protocol_writer` concept enforced at the boundary

Define a concept that every `registry_impl<Opts, Proto>` must satisfy, and a per-protocol sink trait alongside the existing `protocol_storage<Proto>`:

```cpp
// The sink type a protocol writes responses to.
template <uint32_t Proto> struct protocol_sink;          // specialized per protocol
template <uint32_t Proto> using protocol_sink_t = typename protocol_sink<Proto>::type;

// A protocol must expose both halves of the writer vocabulary over its sink.
template <class Impl, auto Opts, class Sink>
concept protocol_writer = requires(Sink& sink, int probe) {
   Impl::template write_stored_value<Opts>(sink, probe);
   Impl::template write_handler_result<Opts>(sink, probe);
};
```

Assert it where the registry binds a protocol (e.g. in `registry<Opts, Proto>`):

```cpp
static_assert(protocol_writer<registry_impl<Opts, Proto>, Opts, protocol_sink_t<Proto>>,
              "a registry protocol must provide both write_stored_value and "
              "write_handler_result (see docs/design/rpc_handler_result_seam.md)");
```

Cost: low. Benefit: a new protocol that provides only one writer fails to compile with a pointed message. This does not yet prevent *calling the wrong one*, but it guarantees both exist and are named correctly — the precondition for Layer 3.

Two caveats follow from the recommended order (Layer 2 now, Layer 3a later):

- **The probe type must track Layer 3a.** The `int probe` works only while `write_handler_result` accepts a plain value. Once Layer 3a makes `write_handler_result` accept *only* `handler_result<V>`, probing it with a bare `int` stops compiling and the concept rejects every protocol. The handler-path probe must then become `handler_result<int>`. Treat that one-line change as part of the Layer 3a PR.
- **The void path is a third operation the vocabulary omits.** Handlers that return `void` (and notifications) produce no body. Today that path is the `write_response(state)` overload in REPE/JSON-RPC and `res.status(204)` in REST; Layer 3a names it `write_empty`. If you want the concept to guarantee the *whole* surface a protocol must provide, add `Impl::write_empty(sink);` to the `requires` block (and to each protocol) at the same time `write_empty` is introduced.

### Layer 3 — Centralize the categorization (the structural guarantee)

To make a miswire *impossible* rather than merely *visible*, the choice of writer must be made once, in shared code, rather than inside each protocol's registration functions.

#### Option 3a — Typed intent wrapper (recommended)

Introduce a compile-time tag, produced only by the generic layer, that marks a value as a handler result. A bare value is, by definition, stored data.

```cpp
namespace glz::detail {
   // Tags a value as the result of invoking a registered callable. The generic
   // registration layer is the single producer; protocol writers dispatch on the type.
   template <class T> struct handler_result { T value; };
   template <class T> handler_result(T&&) -> handler_result<std::remove_cvref_t<T>>;
}
```

Provide a single shared invocation primitive — the *only* sanctioned way to call a registered callable — that always wraps:

```cpp
// Shared, in the generic layer. Used by every function-endpoint builder.
template <auto Opts, uint32_t Proto, class Sink, class F, class... Args>
void invoke_to_sink(Sink& sink, F&& f, Args&&... args)
{
   using Impl = registry_impl<Opts, Proto>;
   using R = std::invoke_result_t<F, Args...>;
   // std::invoke (not f(args...)) so the same primitive serves both free callables
   // and the member-function endpoints, which dispatch as (value.*func)(...). It
   // lowers to a direct call, so there is no runtime cost.
   if constexpr (std::is_void_v<R>) {
      std::invoke(std::forward<F>(f), std::forward<Args>(args)...);
      Impl::write_empty(sink); // success, no body
   }
   else {
      // The prvalue result initializes handler_result::value directly (guaranteed
      // copy elision), so the return object is never moved or copied into the wrapper.
      Impl::template write_handler_result<Opts>(
         sink, detail::handler_result{std::invoke(std::forward<F>(f), std::forward<Args>(args)...)});
   }
}
```

Then `write_handler_result` accepts `handler_result<V>` (unwrapping `value` before applying the `expected` translation), while `write_stored_value` accepts only bare values. The type system now enforces:

- A handler result is *always* wrapped (it can only be produced by `invoke_to_sink`), so it cannot reach `write_stored_value`.
- A stored value is never wrapped, so it cannot reach the translating path.

The wrapper is a compile-time marker only: the return object is reference-forwarded through it, never moved or copied, and the wrapper itself never reaches the serializer. See [Performance and compile-time](#performance-and-compile-time) for the forwarding and instantiation rules that keep this free.

Two things to settle when this lands:

- **`write_empty` is introduced here.** It is the named form of REPE/JSON-RPC's `write_response(state)` overload and REST's `res.status(204)`. Adding it to each protocol is part of this step (and, if desired, to the Layer 2 concept; see above).
- **Notifications stay the caller's decision.** `invoke_to_sink` as written always emits — a result or `write_empty`. REPE and JSON-RPC short-circuit on notify / `has_body` *before* producing any body, so `invoke_to_sink` must be called only on the body-expected branch, or `is_notification` must be threaded into it (Layer 3b's policy list already carries it). The "single sanctioned producer" guarantee is about *wrapping* the result, not about owning the notify decision.

#### Option 3b — Shared endpoint scaffolding (eventual end-state)

The bodies of `register_function_endpoint`, `register_variable_endpoint`, etc. are ~80% identical across protocols (read params → check notify → invoke or read → write). Factoring the control flow into shared `make_function_endpoint` / `make_variable_endpoint` templates parameterized by a protocol *policy* (the sink type plus `read_params`, `is_notification`, `write_stored_value`, `write_handler_result`, `write_empty`, `set_handler_error`) would leave exactly one copy of the wiring: the function builder calls `write_handler_result`, the variable builder calls `write_stored_value`, and a new protocol supplies only policy primitives.

This is the strongest guarantee but the largest change: the per-protocol bodies differ in real ways (REPE's `thread_local` param buffers and `has_body()`/`notify()` semantics, JSON-RPC's string assembly, REST's radix-route registration with HTTP method, path spec, and OpenAPI metadata, and REST's endpoint set being a strict subset — it has no merge endpoint at all). Unifying them risks regressions and has a compile-time cost of its own (see [Performance and compile-time](#performance-and-compile-time)), so it should be undertaken only if those bodies are being reworked for other reasons. It is listed here as the target shape, not a near-term task (YAGNI).

### Recommendation

Adopt **Layer 1 + Layer 2 now** (cheap, removes the naming asymmetry, and makes "both writers exist" a compile-time invariant), and **Layer 3a** the next time the RPC writers are touched (the typed wrapper makes the seam type-enforced with no runtime cost and a small, local diff). Treat **Layer 3b** as the documented end-state to converge toward opportunistically, not a standalone project.

## Migration plan

1. **Layer 1**: pick the sink-first signature shape. Rename REST `write_result` → `write_handler_result`; add `write_stored_value` in REST and replace the four inline `res.body` GET sites. Rename REPE/JSON-RPC `write_response` → `write_stored_value` and reorder their writers to sink-first. Add contract doc comments in all three protocols. No *runtime* behavior change; existing tests must stay green both with and without exceptions.
2. **Layer 2**: add `protocol_sink<Proto>` specializations and the `protocol_writer` concept; add the `static_assert`. Confirm all three protocols satisfy it.
3. **Layer 3a**: add `detail::handler_result` and `invoke_to_sink`; route the four handler-endpoint builders through it; change each `write_handler_result` to take the wrapper. Each protocol migrates independently behind the shared invocation primitive.

Each step is a self-contained PR.

## Testing strategy

The dual-compilation harness (`tests/networking_tests/registry_no_exceptions_test`, built with and without exceptions) already proves identical behavior across configurations. Generalize the #2265 coverage into a **cross-protocol matrix** so every protocol must demonstrate both rows of the seam:

| Case | Fixture | Expectation |
| --- | --- | --- |
| Handler returns `expected<T, E>` in error state | a registered callable | translated to the protocol's error (REPE header `ec`, JSON-RPC `error`, HTTP non-2xx) |
| Data member of type `expected<T, E>` in error state | a registered data member | serialized as `{"unexpected": ...}` with a *success* status |

Today REPE and JSON-RPC each have both rows (`repe_expected_data_member_is_serialized_not_intercepted`, `jsonrpc_expected_data_member_is_serialized_not_intercepted`) and REST has the handler row; adding the REST data-member row completes the matrix. Once Layer 3a lands, a new protocol that fails to wire a writer correctly fails to compile, and the matrix catches any remaining semantic gap.

## Performance and compile-time

Goal #4 is zero runtime overhead. The hot path is already tight — both categories bottom out in a zero-copy serialize into a reused buffer (`set_body<Opts>(forward(value))` for REPE/JSON-RPC, `res.body<Opts>(...)` for REST), and today the prvalue from a handler call is reference-forwarded all the way to the serializer with no moves or allocations. The seam is a compile-time dispatch change, so there are no cycles to reclaim; the work is to *keep* the path free and to avoid paying for the abstraction at compile time. The rules below are what make that hold.

1. **The wrapper is reference-forwarded, never moved.** Construct `handler_result` from the prvalue result (`handler_result{std::invoke(...)}`) so the return object initializes the member directly under guaranteed copy elision; pass the wrapper by value-from-prvalue (elided) or by `&&`; and forward the inner value out with `*std::move(hr.value)`. The serializer reads it in place. Net moves of the return type: zero, matching the status quo. This matters for large or move-only return types, where an accidental extra move (or worse, a copy) would be a real regression.

2. **The wrapper never reaches the serializer.** `write_handler_result` unwraps to the bare `V` before calling `glz::write`/`set_body`, so reflection instantiates on `V` exactly as it does today and never on `handler_result<V>`. If the wrapper leaked into the serializer it would be both a hard error (glaze cannot reflect it) and, if "fixed," a duplicate serializer instantiation per wrapped type. Unwrap-first is load-bearing for binary size, not just correctness.

3. **The sole-producer invariant rules out a hidden copy.** The deduction guide uses `remove_cvref_t<T>`, so an *lvalue* input would copy into the wrapper. Because `invoke_to_sink` is the only producer and always feeds the prvalue `std::invoke(f, args...)`, that copy path is unreachable. The same centralization that makes the seam safe also structurally guarantees the return value is never copied.

4. **The non-`expected` handler path delegates to the stored-value writer.** Today `write_handler_result` for a non-`expected` type is a straight forward to the verbatim serializer (`repe.hpp:644`), so a handler returning `int` and a stored `int` share one `set_body<Opts>(int)` instantiation. The renamed seam must preserve this delegation; forking the two writers' serialization would double the `glz::write` instantiations per type for no benefit. This is the main code-size lever, and it is already correct — the note is "do not regress it."

5. **Compile-time per layer.** Layer 2's concept and `static_assert` cost one `requires` check per protocol binding — negligible. Layer 3a is a small *win*: `invoke_to_sink` collapses the repeated `is_void`/invoke/write dispatch across the four function builders into one primitive per protocol (the heavy serialization was already shared, so the saving is on dispatch logic). Layer 3b is a compile-time *cost*: parameterizing `make_*_endpoint` by a policy struct instantiates those templates per (protocol × endpoint-kind × member type), which can grow instantiation count and build time versus the current flatter inline bodies. In a header-only library that is a first-class concern and a second reason — beyond regression risk — to defer it.

A property worth banking explicitly: pre-#2265, a stored `expected` data member paid the handler path's `has_value()` branch and dragged in the error-translation instantiation. The fix routes stored values straight to the verbatim serializer, and Layer 3a's type separation makes "stored values pay nothing for the `expected` channel" a structural guarantee rather than a convention a future edit can quietly undo.

## Risks and trade-offs

- **Layer 3b is invasive.** Mitigated by making it optional and deferring it until the per-protocol bodies are touched anyway.
- **Layer 3b also costs compile time.** Policy-parameterized endpoint templates instantiate per (protocol × endpoint-kind × member type); see [Performance and compile-time](#performance-and-compile-time). Another reason to defer it in a header-only library.
- **Concept false sense of safety.** Layer 2 proves the writers *exist*, not that they are *called correctly*; that is why Layer 3a (or 3b) is needed for the real guarantee. The layers are complementary, not alternatives.
- **Wrapper ergonomics.** `handler_result<T>` adds a type to read in stack traces and error messages. Kept in `glz::detail` and produced only by `invoke_to_sink`, so it does not leak into user-facing APIs.

## Appendix: symbol map

The names and argument orders below are the **current** (pre-Layer-1) state: REPE/JSON-RPC are value-first (`(value, state)`), REST is sink-first (`(res, result)`). Layer 1 renames them to the shared `write_stored_value` / `write_handler_result` vocabulary and converges on a single sink-first order.

| Concept | REPE | JSON-RPC | REST |
| --- | --- | --- | --- |
| Sink | `repe::state_view` | `jsonrpc::state` | `glz::response` |
| Storage | `protocol_storage<REPE>` | `protocol_storage<JSONRPC>` | `protocol_storage<REST>` (`http_router`) |
| Impl | `registry_impl<Opts, REPE>` | `registry_impl<Opts, JSONRPC>` | `registry_impl<Opts, REST>` |
| Stored-value writer | `write_response(value, state)` | `write_response(value, state)` | `response::body<Opts>(value)` (inline) |
| Empty/null response | `write_response(state)` | `write_response(state)` | `res.status(204)` |
| Handler-result writer | `write_handler_result(value, state)` | `write_handler_result(value, state)` | `write_result(res, result)` |
| Error translator | `set_handler_error(state, e)` | `set_handler_error(state, e)` | `set_handler_error(res, e)` |
| Error vocabulary (`E`) | `error_code`, `error_ctx`, string | `rpc::error`, `rpc::error_e`, `error_ctx`, string | `http_error`, `error_code`, `error_ctx`, string |
