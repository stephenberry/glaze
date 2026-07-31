// Fuzzes the REPE registry over untrusted wire bytes.
//
// The registry parses bytes it does not own: a span handed to registry::call, and the same span
// handed across an FFI boundary by repe::plugin_call. Neither carries a null terminator, and both
// are reachable by anyone who can send a message. Every buffer here is heap allocated at exactly
// the size of its contents so ASAN reports a read one byte past the end.
//
// Most random bytes fail header validation, which would leave the registry itself barely exercised,
// so the input is also replayed wrapped in a well formed header. That splits the work: the raw pass
// fuzzes the header parser, the wrapped pass fuzzes the query dispatch and body reader behind it.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <glaze/glaze.hpp>
#include <glaze/rpc/registry.hpp>
#include <glaze/rpc/repe/plugin_helper.hpp>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

// Reflection needs external linkage, so these cannot live in an anonymous namespace.
namespace repe_fuzz
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

      std::function<int32_t(int32_t)> doubled = [](int32_t v) { return v * 2; };
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

      int32_t scale(int32_t v)
      {
         counter += v;
         return v * 3;
      }
      void bump() { ++counter; }
      point shift(const point& p) { return {p.x + 1, p.y + 1}; }
      std::string tag(const nested& n) { return n.label; }
   };
}

template <>
struct glz::meta<repe_fuzz::member_api>
{
   using T = repe_fuzz::member_api;
   static constexpr auto value = object(&T::counter, &T::scale, &T::bump, &T::shift, &T::tag);
};

namespace
{
   using repe_fuzz::fuzz_api;

   // Exactly sized, so there is no slack after the message for an over-read to land in harmlessly.
   std::vector<char> exact_buffer(std::string_view bytes)
   {
      std::vector<char> buf(bytes.size());
      if (!bytes.empty()) {
         std::memcpy(buf.data(), bytes.data(), bytes.size());
      }
      return buf;
   }

   std::string framed_message(std::string_view query, std::string_view body, uint8_t flags)
   {
      glz::repe::header hdr{};
      hdr.spec = glz::repe::repe_magic;
      hdr.version = 1;
      hdr.id = 1;
      hdr.notify = (flags & 0x1) ? 1 : 0;
      hdr.query_length = query.size();
      hdr.body_length = body.size();
      hdr.length = sizeof(glz::repe::header) + query.size() + body.size();
      hdr.query_format = glz::repe::query_format::JSON_POINTER;
      hdr.body_format = glz::repe::body_format::JSON;
      // The registry echoes a request that already carries an error back to the sender without
      // dispatching it. Nothing else in this target sets ec, so without this that branch is dead.
      hdr.ec = (flags & 0x2) ? glz::error_code::invalid_query : glz::error_code::none;

      std::string msg;
      msg.resize(hdr.length);
      std::memcpy(msg.data(), &hdr, sizeof(hdr));
      std::memcpy(msg.data() + sizeof(hdr), query.data(), query.size());
      std::memcpy(msg.data() + sizeof(hdr) + query.size(), body.data(), body.size());
      return msg;
   }

   void drive(std::span<const char> request)
   {
      glz::registry<> registry;
      fuzz_api api{};
      repe_fuzz::member_api members{};
      // members first, so the last registration -- and therefore the root endpoint -- is the wider
      // of the two objects. Root reads the whole object in one document, so it should be the one
      // with the nested struct, containers, optional and variant in it.
      registry.on(members);
      registry.on(api);

      std::string response;
      registry.call(request, response);
      if (!response.empty()) {
         // The response is a message in its own right, so parsing it back exercises the header
         // reader on bytes the registry itself produced. Copy it out at exactly its own size first:
         // a std::string keeps an implicit '\0' at data()[size()] and allocator slack behind it,
         // which is the very thing this target exists to not have.
         const auto buf = exact_buffer(response);
         [[maybe_unused]] auto parsed = glz::repe::parse_request({buf.data(), buf.size()});
      }
   }

   // plugin_call forwards to the same registry::call overload drive() already uses, so running it
   // per drive doubled the work for one extra line of coverage: the pointer-and-length wrapper and
   // its thread_local response buffer. Once per input is enough to keep that line exercised.
   void drive_plugin(std::span<const char> request)
   {
      glz::registry<> registry;
      fuzz_api api{};
      registry.on(api);
      [[maybe_unused]] auto out = glz::repe::plugin_call(registry, request.data(), request.size());
   }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
   const std::string_view input{reinterpret_cast<const char*>(Data), Size};

   // Raw bytes: header validation, length arithmetic, and the query/body views it hands out.
   {
      const auto buf = exact_buffer(input);
      drive({buf.data(), buf.size()});
      drive_plugin({buf.data(), buf.size()});
   }

   if (Size < 2) {
      return 0;
   }

   // Wrapped: the first byte picks the framing, the second says how much of the rest is the query.
   // The query is what the dispatch walks and the body is what the reader parses, so splitting the
   // input between them lets a single corpus entry drive both.
   const uint8_t flags = Data[0];
   const std::string_view rest = input.substr(2);
   const size_t query_size = rest.empty() ? 0 : size_t(Data[1]) % (rest.size() + 1);
   const std::string_view query = rest.substr(0, query_size);
   const std::string_view body = rest.substr(query_size);

   {
      const auto framed = framed_message(query, body, flags);
      const auto buf = exact_buffer(framed);
      drive({buf.data(), buf.size()});
   }

   // The same body against a fixed query that is known to resolve, so the body reader is reached
   // even when the fuzzer has not yet learned what a valid JSON pointer looks like.
   // "" is the root endpoint, which reads the whole registered object in one document -- every
   // nested struct, container, optional and variant at once, and the deepest reader nesting on
   // offer. It is the widest single target here, so it must not be left out of the list.
   for (std::string_view known : {"",        "/value", "/name",    "/position", "/position/x", "/inner", "/inner/label",
                                  "/series", "/table", "/maybe",   "/measure",  "/doubled",    "/greet", "/midpoint",
                                  "/total",  "/reset", "/counter", "/scale",    "/bump",       "/shift", "/tag"}) {
      const auto framed = framed_message(known, body, flags);
      const auto buf = exact_buffer(framed);
      drive({buf.data(), buf.size()});
   }

   return 0;
}
