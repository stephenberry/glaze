#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <glaze/glaze.hpp>
#include <string>
#include <string_view>

struct Person
{
   std::string first_name{};
   std::string last_name{};
   uint16_t age{};
};

namespace
{
   template <bool null_terminated>
   void impl(const std::size_t pathsize, std::string_view all_buffer)
   {
      // make separate allocations to make it easier for address sanitizer to catch
      // out of bounds access. also, make the size known up front so the allocation is
      // as tight as possible.
      std::vector<char> path(pathsize + +null_terminated);
      std::copy(all_buffer.begin(), all_buffer.begin() + pathsize, path.begin());

      std::vector<char> buffer(all_buffer.size() - pathsize + +null_terminated);
      std::copy(all_buffer.begin() + pathsize, all_buffer.end(), buffer.begin());

      Person child{};
      static constexpr glz::opts options{.null_terminated = null_terminated};
      [[maybe_unused]] auto result =
         glz::read_jmespath<options>(std::string_view(path.data(), path.size() - +null_terminated), child,
                                     std::string_view(buffer.data(), buffer.size() - +null_terminated));
   }
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
   constexpr auto bytes_used_for_nullterm = 1;
   constexpr auto bytes_used_for_size = 2;
   if (Size < bytes_used_for_size + bytes_used_for_nullterm) return 0;

   const bool null_terminated = Data[0];
   Data += bytes_used_for_nullterm;
   Size -= bytes_used_for_nullterm;

   const auto pathsize = std::min(Size - bytes_used_for_size, size_t{Data[0]} + (Data[1] << 8));
   Data += bytes_used_for_size;
   Size -= bytes_used_for_size;

   const std::string_view all_buffer{(const char*)Data, Size};

   if (null_terminated) {
      impl<true>(pathsize, all_buffer);
   }
   else {
      impl<false>(pathsize, all_buffer);
   }

   return 0;
}
