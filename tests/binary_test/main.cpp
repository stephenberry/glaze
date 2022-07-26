// Glaze Library
// For the license information refer to glaze.hpp

#ifndef BOOST_UT_DISABLE_MODULE
#define BOOST_UT_DISABLE_MODULE
#endif
#include "boost/ut.hpp"

#include <bit>

namespace glaze
{
   /*template <class T, class Buffer>
   requires std::ranges::input_range<Buffer> &&
      std::same_as<char, std::ranges::range_value_t<Buffer>>
   inline void write_binary(T&& value, Buffer& buffer) noexcept
   {
      if constexpr (std::same_as<Buffer, std::string>) {
         detail::to_buffer(std::forward<T>(value), buffer);
      }
      else {
         detail::to_buffer(std::forward<T>(value), std::back_inserter(buffer));
      }
   }*/
}

void write_tests()
{
   using namespace boost::ut;

   "binary"_test = [] {
      {
         std::string s;
         float f{0.96875f};
         s.resize(sizeof(float));
         std::memcpy(s.data(), &f, sizeof(float));
         //expect(s == "0.96875");
      }
   };
}

int main()
{
   using namespace boost::ut;

   write_tests();
}
