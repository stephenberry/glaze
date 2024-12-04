# Nullable Types

Glaze provides concepts to support custom nullable types. In order to serialize and parse your custom nullable type it should conform to the `nullable_t` concept:

```c++
template <class T>
concept nullable_t = !meta_value_t<T> && !str_t<T> && requires(T t) {
   bool(t);
   {
      *t
   };
};
```

> [!NOTE]
>
> If you want to read into a nullable type then you must have a `.reset()` method or your type must be assignable from empty braces: `value = {}`. This allows Glaze to reset the value if `"null"` is parsed.

## Nullable Value Types

In some cases a type may be like a `std::optional`, but also require an `operator bool()` that converts to the internal boolean. In these cases you can instead conform to the `nullable_value_t` concept:

```c++
// For optional like types that cannot overload `operator bool()`
template <class T>
concept nullable_value_t = !meta_value_t<T> && requires(T t) {
   t.value();
   {
      t.has_value()
   } -> std::convertible_to<bool>;
};
```
