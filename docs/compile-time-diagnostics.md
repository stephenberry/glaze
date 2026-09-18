# Compile Time Diagnostics

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
include/glaze/core/reflect.hpp:827:76: error: static assertion failed: glz::to<Format, T>: one of this
object's members has no writer for this format. The glz::detail::unsupported_writer specialization in
the trace above names it: after the object come the format id and the member's index in
glz::reflect<T>, then the member's key and the name of its type. Give that type a glz::meta
specialization, make it reflectable, or exclude the member with glz::skip.
```

The instantiation trace carries the identification, because a compile time diagnostic cannot hold a string value:

| trace argument | meaning |
| --- | --- |
| `Outer` | the object being written |
| `10` | the format id, `glz::JSON` |
| `1` | the member's index in `glz::reflect<Outer>` |
| `glz::string_literal<2>{"o"}` | the key the output uses, so a key renamed through `glz::meta` is reported under the name it has in the document |
| `glz::string_literal<7>{"Opaque"}` | the member's type |

Without the check the same struct fails from inside the member loop, where the only clue is `error: incomplete type 'glz::to<10, Opaque>' used in nested name specifier`: a format id, no member, and a stack of instantiation frames around it.

## Which formats report

JSON, JSONB, BEVE (both layouts), CBOR, MSGPACK (both its `glaze_object_t` and its tie-based path), BSON, YAML, TOML and CSV, in both directions where the format has that direction. NDJSON needs nothing of its own: records are written through the JSON writer and reported under the JSON id (10). EETF reads members through the same check; its write side is the one format that cannot report, because its least specialized `to<EETF, T>` matches every type — that specialization now carries a message of its own instead of leaving the dispatcher to report a missing `op`.

## What is not reported

- **A member whose type contains an unsupported type.** `std::vector<Opaque>`, `std::optional<Opaque>` and `std::map<K, Opaque>` all answer as supported, because `to<Format, std::vector<T>>` and friends exist as specializations; the failure then comes from inside the container's own writer, with the original message this check exists to replace. Catching it would mean instantiating the container's writer to find out, which is the cost the check avoids.
- **A type with no linkage.** The key comes from the member names, which the pre-C++26 reflection passes through `external<T>`, and that cannot be defined for a type declared inside a function. A report about such a type surfaces the compiler's error about `external<T>` instead. This is the narrow case MSGPACK's tie-based path reaches.
- **Members that are skipped by design.** `glz::skip`, `hidden`, includers and `meta::skip` for the operation being compiled are all passed over, and so are function pointers — both free and member function pointers, in every format. Which formats drop function pointers is not uniform: JSON, BSON, JSONB, BEVE and CBOR drop both kinds, TOML drops member function pointers, and YAML, MSGPACK and CSV drop neither. The exemption is the lenient direction: such a member is never *reported*, which is not the same as being writable everywhere.

## `write_supported` and `read_supported` are unchanged

The check lives inside `op()`, not at class scope on `to<Format, T>`, and it has to: `write_supported<T, Format>` is `requires { to<Format, T>{}; }`, so a `static_assert` in the class body would fire while that feature probe is being evaluated and turn a query generic code depends on into a hard error. `op` is a member template, so an assert in its body is invisible to the probe.

The consequence is that the concepts still answer `true` for a struct whose member has no writer — the class exists, its member does not. Code that gates on `write_supported` and then writes still gets the compile error described above; the concept answers the question it always answered.

## Adding a format

Nothing enforces the check: a format's object writer and reader ask for it by hand, as the first statement of `op()`.

```cpp
template <auto Opts, class B>
static void op(auto&& value, is_context auto&& ctx, B&& b, auto& ix)
{
   static_assert(detail::writable_members<MyFormat, T>,
                 "One of this object's members has no writer for MyFormat.");
   ...
}
```

A format that omits it keeps the old error from inside its member loop. A format whose object writer emits some members itself, instead of dispatching to `to<Format, M>`, declares that with `detail::writer_emits_member_inline<Format, M>`: BSON does, for nullable, null and variant members, because a BSON element carries its type next to its key, and TOML does for members that are always null, which its writer emits as nothing. The declaration is per direction, and only for what the writer really handles: TOML's reader has no reader for those members either, so nothing is exempted on that side and the diagnostic still reports them.
