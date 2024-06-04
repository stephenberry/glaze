# REPE RPC

> [!WARNING]
>
> **The `glz::asio_server`, `glz::asio_client`, and `glz::repe::registry`, are all under active development and should not be considered stable.**

Glaze provides support for the [REPE](https://github.com/stephenberry/repe) RPC format along with client/server implementations (currently [asio](http://think-async.com/Asio/)).

- [REPE specification](https://github.com/stephenberry/repe)

JSON Pointer syntax is used to refer to fields in C++ structures.

## REPE registry locking and thread safety

The `glz::repe::registry` allows users to expose C++ classes directly to the registry as an interface for network RPC calls and POST/GET calls.

The registry attempts to handle the thread safety required to directly manipulate C++ memory and read/write from JSON or BEVE messages. In most cases unlimited clients can read and write from your registered structures in a thread-safe manner than reduces locks, permits asynchronous reads, and non-blocking writes where valid.

Locking is performed on JSON pointer path chains to allow multiple clients to read/write from the same object at the same time.

Locking is based on depth in the object tree. A chain of shared mutexes are locked, using shared locks until the actual write point, which uses a unique lock. When reading from C++ memory, only shared locks are used.

An invoke lock is also used for invoking functions. Unlike variable access, the invocation lock also locks everything at the same depth as the function, which allows member functions to safely manipulate member variables from the same class.

> [!IMPORTANT]
>
> Pointers to parent members or functions that manipulate shallower depth members are not thread safe and safety has to be added by the developer.

## asio

> [!NOTE]
>
> `glz::asio_server` and `glz::asio_client` require the [standalone asio](https://think-async.com/Asio/AsioStandalone.html) to build. Glaze does not include this dependency within its CMake files. The developer is expected to include this header-only library.

