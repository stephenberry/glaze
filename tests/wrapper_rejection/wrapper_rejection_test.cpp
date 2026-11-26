// Glaze Library
// For the license information refer to glaze.hpp

// A wrapper (a glaze_wrapper such as glz::quoted or glz::invoke) is a view onto a member, not a struct.
// A format with no specialization for one must not reflect it, which writes the wrapper's internals
// (often an empty object) and reads into them. These checks hold that line for the wrappers that only
// some formats support. Writing or reading glz::invoke in a format that cannot is a compile error with
// its own message, covered by the rejection cases next to this file.

#include <functional>
#include <string>

#include "glaze/beve.hpp"
#include "glaze/cbor.hpp"
#include "glaze/json.hpp"
#include "glaze/json/raw_string.hpp"
#include "glaze/msgpack.hpp"

struct invoke_host
{
   void call() {}
};

using invoke_member_function = glz::invoke_t<void (invoke_host::*)()>;
using invoke_std_function = glz::invoke_t<std::function<void(int)>>;

static_assert(!glz::reflectable<glz::quoted_t<int>>);
static_assert(!glz::reflectable<glz::escape_bytes_t<char[4]>>);
static_assert(!glz::reflectable<glz::raw_t<std::string>>);
static_assert(!glz::reflectable<glz::raw_string_t<std::string>>);
static_assert(!glz::reflectable<glz::escaped_t<std::string>>);
static_assert(!glz::reflectable<invoke_member_function>);
static_assert(!glz::reflectable<invoke_std_function>);

template <class T, uint32_t Format>
constexpr bool unsupported = !glz::write_supported<T, Format> && !glz::read_supported<T, Format>;

// JSON-only wrappers have neither a writer nor a reader in the binary formats
static_assert(unsupported<glz::quoted_t<int>, glz::BEVE>);
static_assert(unsupported<glz::quoted_t<int>, glz::CBOR>);
static_assert(unsupported<glz::quoted_t<int>, glz::MSGPACK>);
static_assert(unsupported<glz::escape_bytes_t<char[4]>, glz::BEVE>);
static_assert(unsupported<glz::raw_string_t<std::string>, glz::CBOR>);
static_assert(unsupported<glz::escaped_t<std::string>, glz::MSGPACK>);

// and JSON keeps them
static_assert(glz::write_supported<glz::quoted_t<int>, glz::JSON>);
static_assert(glz::read_supported<glz::quoted_t<int>, glz::JSON>);
static_assert(glz::write_supported<glz::raw_string_t<std::string>, glz::JSON>);
static_assert(glz::read_supported<invoke_member_function, glz::JSON>);

int main() { return 0; }
