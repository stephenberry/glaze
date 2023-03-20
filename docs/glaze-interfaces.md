# Glaze Interfaces (Generic Library API)

Glaze has been designed to work as a generic interface for shared libraries and more. This is achieved through JSON pointer syntax access to memory.

Glaze allows a single header API (`api.hpp`) to be used for every shared library interface, greatly simplifying shared library handling.

> Interfaces are simply Glaze object types. So whatever any JSON/binary interface can automatically be used as a library API.

The API is shown below. It is simple, yet incredibly powerful, allowing pretty much any C++ class to be manipulated across the API via JSON or binary, or even the class itself to be passed and safely cast on the other side.

```c++
struct api {
  /*default constructors hidden for brevity*/
  
  template <class T>
    [[nodiscard]] T* get(const sv path) noexcept;

  // Get a std::function from a member function across the API
  template <class T>
    [[nodiscard]] T get_fn(const sv path);

  template <class Ret, class... Args>
  [[nodiscard]] Ret call(const sv path, Args&&... args);

  virtual bool read(const uint32_t /*format*/, const sv /*path*/,
                    const sv /*data*/) noexcept = 0;

  virtual bool write(const uint32_t /*format*/, const sv /*path*/, std::string& /*data*/) = 0;

  [[nodiscard]] virtual const sv last_error() const noexcept {
    return error;
  }

  protected:
  /// unchecked void* access
  virtual void* get(const sv path, const sv type_hash) noexcept = 0;

  virtual bool caller(const sv path, const sv type_hash, void* ret, std::span<void*> args) noexcept = 0;

  virtual std::unique_ptr<void, void(*)(void*)> get_fn(const sv path, const sv type_hash) noexcept = 0;

  std::string error{};
};
```

## Member Functions

Member functions can be registered with the metadata, which allows the function to be called across the API.

```c++
struct my_api {
  int func() { return 5; };
};

template <>
struct glz::meta<my_api> {
   using T = my_api;
   static constexpr auto value = object("func", &T::func);
   static constexpr std::string_view name = "my_api";
};
```

In use:

```c++
std::shared_ptr<glz::iface> iface{ glz_iface()() };
auto io = (*iface)["my_api"]();

expect(5 == io->call<int>("/func"));
```

`call` invokes the function across the API. Arguments are also allowed in the call function:

```c++
auto str = io->call<std::string>("/concat", "Hello", "World");
```

`get_fn` provides a means of getting a `std::function` from a member function across the API. This can be more efficient if you intend to call the same function multiple times.

```c++
auto f = io->get_fn<std::function<int()>>("/func");
```

## std::function

Glaze allows `std::function` types to be added to objects and arrays in order to use them across Glaze APIs.

```c++
int x = 7;
double y = 5.5;
auto& f = io->get<std::function<double(int, double)>>("/f");
expect(f(x, y) == 38.5);
```

## Type Safety

A valid interface concern is binary compatibility between types. Glaze uses compile time hashing of types that is able to catch a wide range of changes to classes or types that would cause binary incompatibility. These compile time hashes are checked when accessing across the interface and provide a safeguard, much like a `std::any_cast`, but working across compilations.`std::any_cast` does not guarantee any safety between separately compiled code, whereas Glaze adds significant type checking across compilations and versions of compilers.

## Name

By default custom type names from `glz::name_v` will be `"Unnamed"`. It is best practice to give types the same name as it has in C++, including the namespace (at least the local namespace).

Concepts exist for naming `const`, pointer (`*`), and reference (`&`), versions of types as they are used. Many standard library containers are also supported.

```c++
expect(glz::name_v<std::vector<float>> == "std::vector<float>");
```

To add a name for your class, include it in the `glz::meta`:

```c++
template <>
struct glz::meta<my_api> {
  static constexpr std::string_view name = "my_api";
};
```

Or, include it via local glaze meta:

```c++
struct my_api {
  struct glaze {
    static constexpr std::string_view name = "my_api";
  };
};
```

## Version

By default all types get a version of `0.0.1`. The version tag allows the user to intentionally break API compatibility for a type when making changes that would not be caught by the compile time type checking.

```c++
template <>
struct glz::meta<my_api> {
  static constexpr glz::version_t version{ 0, 0, 2 };
};
```

Or, include it locally like `name` or `value`.

## What Is Checked?

Glaze catches the following changes:

-  `name` in meta
-  `version` in meta
-  the `sizeof` the type
-  All member variables names (for object types)
-  The compiler (clang, gcc, msvc)
-  `std::is_trivial`
-  `std::is_standard_layout`
-  `std::is_default_constructible`
-  `std::is_trivially_default_constructible`
-  `std::is_nothrow_default_constructible`
-  `std::is_trivially_copyable`
-  `std::is_move_constructible`
-  `std::is_trivially_move_constructible`
-  `std::is_nothrow_move_constructible`
-  `std::is_destructible`
-  `std::is_trivially_destructible`
-  `std::is_nothrow_destructible`
-  `std::has_unique_object_representations`
-  `std::is_polymorphic`
-  `std::has_virtual_destructor`
-  `std::is_aggregate`
