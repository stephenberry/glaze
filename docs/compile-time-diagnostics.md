# Compile-Time Diagnostics

When a struct has a member that a format cannot write or read, Glaze reports it at compile time and names the member.

## The error

```cpp
struct Outer
{
   int a{};
   Opaque o{}; // no `glz::meta`, not reflectable: no format has a writer or reader for it
   int b{};
};

std::string buffer;
glz::write_json(Outer{}, buffer);
```

```
In instantiation of 'struct glz::detail::unsupported_writer<Outer, 10, 1,
  glz::string_literal<2>{"o"}, glz::string_literal<7>{"Opaque"}>':
include/glaze/core/reflect.hpp:870:76: error: static assertion failed: glz::to<Format, T>: one of this
object's members has no writer for this format. The glz::detail::unsupported_writer specialization in
the trace above names it: after the object come the format id and the member's index in
glz::reflect<T>, then the member's key and the name of its type. Give that type a glz::meta
specialization, make it reflectable, or exclude the member with glz::skip.
```

The instantiation trace carries the identification, because a compile-time diagnostic cannot hold a string value:

| trace argument | meaning |
| --- | --- |
| `Outer` | the object being written |
| `10` | the format id, `glz::JSON` |
| `1` | the member's index in `glz::reflect<Outer>` |
| `glz::string_literal<2>{"o"}` | the key the output uses, so a key renamed through `glz::meta` is reported under the name it has in the document |
| `glz::string_literal<7>{"Opaque"}` | the member's type |

Without the check the same struct fails from inside the member loop, where the only clue is `error: incomplete type 'glz::to<10, Opaque>' used in nested name specifier`: a format id, no member, and a stack of instantiation frames around it.

## Which formats report

JSON, JSONB, BEVE (both layouts), CBOR, MSGPACK (both its `glaze_object_t` and its tie-based path), BSON, YAML, TOML and CSV, in both directions where the format has that direction. The format ids that appear in a report are:

| format | id | format | id |
| --- | --- | --- | --- |
| BEVE | 1 | MSGPACK | 30 |
| CBOR | 2 | TOML | 400 |
| JSONB | 3 | YAML | 450 |
| BSON | 4 | CSV | 10000 |
| JSON | 10 | EETF (read only) | 20000 |

NDJSON needs nothing of its own: records are written through the JSON writer and reported under the JSON id (10). EETF reads members through the same check; its write side is the one format that cannot report, because its least specialized `to<EETF, T>` matches every type — that specialization now carries a message of its own instead of leaving the dispatcher to report a missing `op`.

## What is not reported

- **A member whose type contains an unsupported type.** `std::vector<Opaque>`, `std::optional<Opaque>` and `std::map<K, Opaque>` all answer as supported, because `to<Format, std::vector<T>>` and friends exist as specializations; the failure then comes from inside the container's own writer, with the original message this check exists to replace. Catching it would mean instantiating the container's writer to find out, which is the cost the check avoids.
- **A type with no linkage.** The key comes from the member names, which the pre-C++26 reflection passes through `external<T>`, and that cannot be defined for a type declared inside a function. A report about such a type surfaces the compiler's error about `external<T>` instead. This is the narrow case MSGPACK's tie-based path reaches.
- **Members that are skipped by design.** `glz::skip`, `hidden`, includers and `meta::skip` for the operation being compiled are all passed over, and so are function pointers — both free and member function pointers, in every format. What a format would actually do with one is not uniform: with `write_function_pointers` off (the default) every object writer except CSV's omits both kinds, and MSGPACK's positional `structs_as_arrays` layout omits neither (its keyed layout does); with the option on, only JSON and BEVE have writers for both kinds, JSONB and CBOR only for member function pointers, and BSON, MSGPACK, TOML and YAML fail to compile. CSV fails either way: its `opts_csv` has no `write_function_pointers` option, so nothing skips a function pointer before its field writer is asked for one. The exemption here is the lenient direction: such a member is never *reported*, which is not the same as being writable everywhere.
- **A member whose declared type is a wrapper with its own writer.** `glz::custom` and the other wrapper types are written through the wrapper, so the check sees the wrapper type and asks about it. A value stored behind a `glz::custom` getter that has no writer of its own is not reported, and a reader could not see it anyway.

## `write_supported` and `read_supported` are unchanged

The check lives inside the writer and reader functions, not at class scope on `to<Format, T>`, and it has to: `write_supported<T, Format>` is `requires { to<Format, T>{}; } && top_level_supported<Format, T>`, so a `static_assert` in the class body would fire while that feature probe is being evaluated and turn a query generic code depends on into a hard error. `op` is a member template, so an assert in its body is invisible to the probe.

The consequence is that the concepts still answer `true` for a struct whose member has no writer — the class exists, its member does not. Code that gates on `write_supported` and then writes still gets the compile error described above; the concept answers the question it always answered.

The check's own per-member question is narrower: it asks whether `to<Format, M>` (or `from<Format, M>`) exists, without `top_level_supported`. The difference is load bearing for BSON, whose top level must be a document: `write_supported<int, BSON>` is false, but a struct's `int` member is written by the object writer all the same, so asking the public concept would reject every BSON struct with a scalar member.

## Adding a format

Nothing enforces the check: a format's object writer and reader ask for it by hand, as the first statement of `op()` — or of the member-loop function `op()` delegates to. CBOR and MSGPACK put it in `write_members` as well, because an internally tagged variant merges its discriminator into the alternative's map and calls that function directly. YAML's block and flow mapping helpers, TOML's inline-table and nested-table writers, and CSV's row readers and writers carry it for the same reason: their object writer is not on the path that emits the members.

```cpp
template <auto Opts, class B>
static void op(auto&& value, is_context auto&& ctx, B&& b, auto& ix)
{
   static_assert(detail::writable_members<MyFormat, T>,
                 "One of this object's members has no writer for MyFormat.");
   ...
}
```

A format that omits it keeps the old error from inside its member loop. A format whose object writer emits some members itself, instead of dispatching to `to<Format, M>`, declares that with `detail::writer_emits_member_inline<Format, M>`. BSON does, because a BSON element carries its type next to its key: it emits nullable, null and variant members itself, and unwraps the transparent write wrappers — a mimic and a `glz::custom` accessor — to one of those. TOML does for members that are always null, which its writer emits as nothing. The declaration is per direction, and only for what the writer really handles: TOML's reader has no reader for those members either, so nothing is exempted on that side and the diagnostic still reports them.
