// Fuzzes the JSON RPC registry over untrusted request text.
//
// registry<Opts, JSONRPC>::call takes a std::string_view, so the bytes it parses are the caller's
// and carry no null terminator. Three readers run over them: the batch split, the syntax check that
// decides -32700 against -32600, and the per-request params read. Every buffer here is heap
// allocated at exactly the size of its contents so ASAN reports a read one byte past the end.
//
// Random bytes rarely look like a JSON RPC request, so the input is also replayed as the params of
// a well formed envelope. That splits the work: the raw pass fuzzes the envelope parser, the
// wrapped pass fuzzes the params reader and the method dispatch behind it.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <glaze/glaze.hpp>
#include <glaze/rpc/registry.hpp>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

// Reflection needs external linkage, so these cannot live in an anonymous namespace.
namespace jsonrpc_fuzz
{
   struct point
   {
      double x{};
      double y{};
   };

   struct amount
   {
      double value{};
   };

   struct nested
   {
      int32_t depth{};
      std::string label{};
   };

   // Wide enough that the dispatch has somewhere to go and the reader has more than one shape to
   // resolve against. Three surfaces matter here and each is represented:
   //   - plain members, where the read target is a fixed type
   //   - std::function members, which is how a call with typed parameters is registered, and so
   //     the only way to reach the reader that parses arguments
   //   - a variant, whose resolution re-parses speculatively -- the path where a reader is most
   //     likely to walk off the end of a buffer it does not own
   struct fuzz_api
   {
      int32_t value{};
      std::string name{};
      point position{};
      nested inner{};
      std::vector<int32_t> series{};
      std::map<std::string, int32_t> table{};
      std::optional<double> maybe{};
      std::variant<std::string, amount> measure{};

      // The fuzzer supplies these arguments, so every one of them arrives at its own extremes.
      // Signed overflow in the body of a target is undefined behavior in the target, not a finding
      // about the registry, and UBSan stops the run on it either way -- so the arithmetic here goes
      // through unsigned, where wrapping is defined and the result is still a function of the input.
      std::function<int32_t(int32_t)> doubled = [](int32_t v) { return int32_t(uint32_t(v) * 2u); };
      std::function<std::string(const std::string&)> greet = [](const std::string& who) { return "hi " + who; };
      std::function<point(const point&)> midpoint = [](const point& p) -> point { return {p.x / 2, p.y / 2}; };
      std::function<double(std::vector<double>&)> total = [](std::vector<double>& v) {
         double sum{};
         for (auto d : v) sum += d;
         return sum;
      };
      std::function<void()> reset = [] {};
   };

   // Member functions listed in a glz::meta reach different endpoint registrations than the
   // std::function members above -- register_member_function_endpoint and its with_params variant.
   // The with_params one parks its argument in a `static thread_local`, which is the kind of state
   // that survives between inputs and turns a crash into one that will not reproduce, so it is
   // worth having under the fuzzer rather than only under the unit tests.
   struct member_api
   {
      int32_t counter{};

      // Wraps rather than overflows, for the reason given on `doubled` above. `counter` accumulates
      // across inputs, so it reaches its extremes on its own even when no single argument is large.
      int32_t scale(int32_t v)
      {
         counter = int32_t(uint32_t(counter) + uint32_t(v));
         return int32_t(uint32_t(v) * 3u);
      }
      void bump() { counter = int32_t(uint32_t(counter) + 1u); }
      point shift(const point& p) { return {p.x + 1, p.y + 1}; }
   };
}

template <>
struct glz::meta<jsonrpc_fuzz::member_api>
{
   using T = jsonrpc_fuzz::member_api;
   static constexpr auto value = object(&T::counter, &T::scale, &T::bump, &T::shift);
};

namespace
{
   using jsonrpc_fuzz::fuzz_api;

   // Exactly sized, so there is no slack after the request for an over-read to land in harmlessly.
   std::vector<char> exact_buffer(std::string_view bytes)
   {
      std::vector<char> buf(bytes.size());
      if (!bytes.empty()) {
         std::memcpy(buf.data(), bytes.data(), bytes.size());
      }
      return buf;
   }

   void drive(std::string_view request)
   {
      glz::registry<glz::opts{}, glz::JSONRPC> registry;
      fuzz_api api{};
      jsonrpc_fuzz::member_api members{};
      // members first, so the last registration -- and therefore the root endpoint -- is the wider
      // of the two objects. Root reads the whole object in one document, so it should be the one
      // with the nested struct, containers, optional and variant in it.
      registry.on(members);
      registry.on(api);

      const auto response = registry.call(request);
      if (!response.empty()) {
         // The response is JSON the registry itself produced, so reading it back checks that what
         // goes out is well formed even when what came in was not.
         const auto buf = exact_buffer(response);
         glz::generic parsed{};
         [[maybe_unused]] auto ec =
            glz::read<glz::opts{.null_terminated = false}>(parsed, std::string_view{buf.data(), buf.size()});
      }
   }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
   const std::string_view input{reinterpret_cast<const char*>(Data), Size};

   // Raw bytes: the batch split, the syntax check, and the envelope read.
   {
      const auto buf = exact_buffer(input);
      drive({buf.data(), buf.size()});
   }

   if (Size < 1) {
      return 0;
   }

   // Wrapped: the input becomes the params of an envelope that is otherwise well formed, so the
   // params reader is reached even when the fuzzer has not yet learned the envelope's shape. The
   // first byte picks the method, since which one is registered decides what the params must parse
   // as, and a params reader is only interesting once it has a destination type.
   // "" is the root method, which reads the whole registered object in one document -- every nested
   // struct, container, optional and variant at once, and the deepest reader nesting on offer.
   static constexpr std::string_view methods[] = {
      "",        "value", "name",     "position", "position/x", "inner",   "series", "table", "maybe", "measure",
      "doubled", "greet", "midpoint", "total",    "reset",      "counter", "scale",  "bump",  "shift", "absent"};
   const std::string_view method = methods[Data[0] % (sizeof(methods) / sizeof(methods[0]))];
   const std::string_view params = input.substr(1);

   {
      std::string request;
      request += R"({"jsonrpc":"2.0","method":")";
      request += method;
      request += R"(","params":)";
      request += params;
      request += R"(,"id":1})";

      const auto buf = exact_buffer(request);
      drive({buf.data(), buf.size()});
   }

   // The same envelope inside a batch, which takes a different path: the array is split into raw
   // views first and each element is read on its own.
   {
      std::string request;
      request += R"([{"jsonrpc":"2.0","method":")";
      request += method;
      request += R"(","params":)";
      request += params;
      request += R"(,"id":1},{"jsonrpc":"2.0","method":")";
      request += method;
      request += R"("}])"; // second element is a notification, so it takes the no-response path

      const auto buf = exact_buffer(request);
      drive({buf.data(), buf.size()});
   }

   return 0;
}
