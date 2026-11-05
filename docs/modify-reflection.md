# Extending Pure Reflection with `modify`

Glaze lets you _augment_ pure reflection without replacing it. Specialize `glz::meta<T>` with a `static constexpr auto modify = glz::object(...);` to rename reflected members, add aliases, or append entirely new key-value pairs while the remaining members still come from pure reflection.

## When to reach for `modify`

| Scenario | Recommendation |
| --- | --- |
| Rename a couple of fields while keeping the rest of the struct on pure reflection | Use `modify` |
| Add alternate JSON field names/aliases without duplicating your struct definition | Use `modify` |
| Expose derived data or helper views alongside reflected members | Use `modify` |
| Fully control the serialized shape (omit members, reorder everything, mix formats) | Continue to use a full `value` specialization |

Using `modify` keeps the zero-maintenance benefits of pure reflection: if you add a new data member to an aggregate type it will still be serialized automatically unless you override it explicitly inside `modify`.

## Basic usage

```cpp
struct point
{
   double x{};
   double y{};
};

// Rename `x`, add an alias for `y`, and expose a derived magnitude key

template <> struct glz::meta<point>
{
   static constexpr auto modify = glz::object(
      "real", &point::x,              // rename default key x -> real
      "imag", &point::y,              // rename default key y -> imag
      "radius", [](auto& self) {      // append a computed key
         return std::hypot(self.x, self.y);
      }
   );
};
```

Serializing `point{3, 4}` produces:

```json
{"real":3,"imag":4,"radius":5}
```

Notice that the original member list still participates: only the names you override change. Any members you do not mention retain their automatically generated names and order.

## Override a few keys

Often you just need to tweak a couple of fields while the bulk of the struct continues to rely on pure reflection. You can still do that without touching the entire definition:

```cpp
struct server_status
{
   std::string name;          // pure reflection keeps this key
   std::string region;        // …and this one
   uint64_t active_sessions;  // and all the others
   std::optional<std::string> maintenance;
   double cpu_percent{};
};

template <> struct glz::meta<server_status>
{
   static constexpr auto modify = glz::object(
      "maintenance_alias", [](auto& self) -> auto& { return self.maintenance; },
      "cpuPercent", &server_status::cpu_percent
   );
};
```

Serialising a value keeps the original fields while appending the two tweaks:

```json
{
  "name": "edge-01",
  "region": "us-east",
  "active_sessions": 2412,
  "maintenance": "scheduled",
  "cpuPercent": 73.5,
  "maintenance_alias": "scheduled"
}
```

Only the keys you touched change (`maintenance_alias`, `cpuPercent`). Everything else—`name`, `region`, `active_sessions`, `maintenance`—comes straight from pure reflection, so adding or removing members later still “just works”.

## Renaming reflected members

Pass a string literal before the member pointer to override the emitted key:

```cpp
static constexpr auto modify = glz::object(
   "first_name", &person::first,
   "last_name", &person::last
);
```

On read, Glaze respects the new key names. The original names (`first`, `last`) are not accepted unless you also add explicit aliases.

## Adding aliases

Aliases are just lambdas (or other invocables) that forward to the desired field:

```cpp
static constexpr auto modify = glz::object(
   "id", &widget::identifier,
   "legacy_id", [](auto& self) -> auto& { return self.identifier; }
);
```

Both `"id"` and `"legacy_id"` will now read and write the same member. When reading, aliases win over the base name whenever both appear in the payload (the last encountered value is used).

## Extending with additional keys

If a key in `modify` does not correspond to a reflected member it is treated as an _extension_. This is perfect for exposing alternative views or derived data:

```cpp
static constexpr auto modify = glz::object(
   "values", &complex_modify::records,
   "summary", [](auto& self) {
      return std::make_pair("count", self.records.size()); // example of a helper view
   }
);
```

Extensions are emitted in addition to all purely reflected members. During parsing Glaze invokes the callable so you can populate internal fields however you like.

## Mixing containers and optionals

`modify` works seamlessly with nested containers and optional fields because it composes on top of the existing reflection logic:

```cpp
struct dashboard
{
   std::vector<entry> items;
   std::map<std::string, double> metrics;
   std::optional<int> flag;
};

template <> struct glz::meta<dashboard>
{
   static constexpr auto modify = glz::object(
      "items", &dashboard::items,
      "items_alias", [](auto& self) -> auto& { return self.items; },
      "stats", &dashboard::metrics,
      "flag_status", &dashboard::flag
   );
};
```

No additional boilerplate is required—Glaze reuses the existing container serializers for every member you expose.

## Guidelines and caveats

- `modify` is only considered for aggregate types that would otherwise satisfy `glz::reflectable<T>`.
- Lambdas or function objects used inside `modify` must return an lvalue reference to participate in assignment, or a value if they are write-only extras.
- If a callable entry does not reference an existing member you **must** provide a key string (member pointers infer their key automatically).
- `modify` complements other `glz::meta` hooks: `skip`, `requires_key`, `unknown_read`, etc. continue to work as before.
- You can still provide a full `value` specialization later; it will override pure reflection and `modify` entirely.

## Choosing between `modify` and `value`

- Reach for `modify` when you only need small mutations to the default reflected shape (rename a few fields, add derived values, expose aliases).
- Fall back to `value` when you want to **remove** members, when the output order is critical, or when you need to interleave unrelated data in the serialized object.
