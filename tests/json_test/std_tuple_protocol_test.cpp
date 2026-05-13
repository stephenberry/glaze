// Glaze Library
// Test that user-defined types implementing the standard C++ tuple protocol
// (std::tuple_size, std::tuple_element, ADL get<I>) are treated as tuples
// without requiring duplicate glz:: specializations.
// See https://github.com/stephenberry/glaze/discussions/2530

#include <cstddef>
#include <string>
#include <tuple>
#include <utility>

#include "glaze/glaze.hpp"
#include "glaze/cbor.hpp"
#include "glaze/msgpack.hpp"
#include "ut/ut.hpp"

using namespace ut;

namespace user_ns
{
   class MyTuple
   {
     public:
      MyTuple() = default;
      MyTuple(int x, double y, std::string z) : x_(x), y_(y), z_(std::move(z)) {}

      int& x() & { return x_; }
      double& y() & { return y_; }
      std::string& z() & { return z_; }

      const int& x() const& { return x_; }
      const double& y() const& { return y_; }
      const std::string& z() const& { return z_; }

      bool operator==(const MyTuple&) const = default;

     private:
      int x_{};
      double y_{};
      std::string z_{};
   };

   // ADL get — found by unqualified name lookup in the same namespace as MyTuple.
   template <std::size_t I>
   decltype(auto) get(MyTuple& t)
   {
      if constexpr (I == 0)
         return t.x();
      else if constexpr (I == 1)
         return t.y();
      else
         return t.z();
   }

   template <std::size_t I>
   decltype(auto) get(const MyTuple& t)
   {
      if constexpr (I == 0)
         return t.x();
      else if constexpr (I == 1)
         return t.y();
      else
         return t.z();
   }

   template <std::size_t I>
   decltype(auto) get(MyTuple&& t)
   {
      if constexpr (I == 0)
         return std::move(t).x();
      else if constexpr (I == 1)
         return std::move(t).y();
      else
         return std::move(t).z();
   }
}

// Standard tuple protocol customization: specializing these in namespace std
// is explicitly permitted by the standard for program-defined types.
template <>
struct std::tuple_size<user_ns::MyTuple> : std::integral_constant<std::size_t, 3>
{};

template <>
struct std::tuple_element<0, user_ns::MyTuple>
{
   using type = int;
};

template <>
struct std::tuple_element<1, user_ns::MyTuple>
{
   using type = double;
};

template <>
struct std::tuple_element<2, user_ns::MyTuple>
{
   using type = std::string;
};

suite std_tuple_protocol = [] {
   "concept detects std-protocol types"_test = [] {
      static_assert(glz::std_tuple_protocol<user_ns::MyTuple>);
      static_assert(glz::std_tuple_protocol<std::tuple<int, double>>);
      static_assert(not glz::std_tuple_protocol<std::pair<int, double>>); // pair has its own path
      static_assert(not glz::std_tuple_protocol<std::array<int, 3>>); // array goes through range path
      static_assert(not glz::std_tuple_protocol<int>);
   };

   "json roundtrip via std tuple protocol"_test = [] {
      user_ns::MyTuple t{42, 3.14, "answer"};
      std::string buffer;
      expect(not glz::write_json(t, buffer));
      expect(buffer == R"([42,3.14,"answer"])");

      user_ns::MyTuple t2;
      expect(glz::read_json(t2, buffer) == glz::error_code::none);
      expect(t == t2);
   };

   "std::tuple still roundtrips"_test = [] {
      auto t = std::make_tuple(1, 2.5, std::string("ok"));
      decltype(t) t2;
      std::string buffer;
      expect(not glz::write_json(t, buffer));
      expect(glz::read_json(t2, buffer) == glz::error_code::none);
      expect(t == t2);
   };

   "std::pair stays a pair (object), not a tuple (array)"_test = [] {
      std::pair<std::string, int> p{"key", 9};
      std::string buffer;
      expect(not glz::write_json(p, buffer));
      // pair_t serializer writes {"key":9}, not ["key",9]
      expect(buffer == R"({"key":9})");
   };

   "beve roundtrip"_test = [] {
      user_ns::MyTuple t{42, 3.14, "answer"};
      auto buffer = glz::write_beve(t).value_or(std::string{});
      user_ns::MyTuple t2;
      expect(glz::read_beve(t2, buffer) == glz::error_code::none);
      expect(t == t2);
   };

   "cbor roundtrip"_test = [] {
      user_ns::MyTuple t{42, 3.14, "answer"};
      auto buffer = glz::write_cbor(t).value_or(std::string{});
      user_ns::MyTuple t2;
      expect(glz::read_cbor(t2, buffer) == glz::error_code::none);
      expect(t == t2);
   };

   "msgpack roundtrip"_test = [] {
      user_ns::MyTuple t{42, 3.14, "answer"};
      auto buffer = glz::write_msgpack(t).value_or(std::string{});
      user_ns::MyTuple t2;
      expect(glz::read_msgpack(t2, buffer) == glz::error_code::none);
      expect(t == t2);
   };
};

int main() { return 0; }
