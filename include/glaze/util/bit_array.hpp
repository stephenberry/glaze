// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace glz
{
   // Basically std::bitset but exposes things normally not available like the bitscan functions
   //
   // Bit `pos` lives in chunk `pos / n_chunk_bits`, so data[0] holds the least significant bits. The bit
   // scans treat the array as a single N-bit unsigned integer: countr_zero is the index of the lowest set
   // bit and countl_zero the number of zeros above the highest one, and both return N when no bit is set.
   //
   // The bits of the top chunk at and above N are padding and are kept zero (flip masks them), so the
   // counts, popcount, and equality never see them. Writing through operator[] requires pos < N.
   template <size_t N, std::unsigned_integral Chunk = uint64_t>
   struct bit_array
   {
      static constexpr size_t n_chunk_bits = std::numeric_limits<Chunk>::digits;
      static constexpr size_t n_chunks = (N == 0) ? 0 : (N - 1) / n_chunk_bits + 1;
      static constexpr size_t n_padding_bits = n_chunks * n_chunk_bits - N;
      // The valid (non-padding) bits of the top chunk
      static constexpr Chunk top_chunk_mask = Chunk(~Chunk{}) >> n_padding_bits;

      struct reference
      {
         Chunk* data{};
         Chunk maskbit{};

         constexpr reference& operator=(bool other) noexcept
         {
            if (other)
               *data |= maskbit;
            else
               *data &= Chunk(~maskbit);
            return *this;
         }

         constexpr operator bool() const noexcept { return (*data & maskbit) != 0; }

         constexpr bool operator~() const noexcept { return (*data & maskbit) == 0; }
      };

      std::array<Chunk, n_chunks> data{};

      constexpr reference operator[](size_t pos) { return reference{&data[pos / n_chunk_bits], mask_of(pos)}; }

      constexpr bool operator[](size_t pos) const { return (data[pos / n_chunk_bits] & mask_of(pos)) != 0; }

      constexpr int popcount() const noexcept
      {
         int res{};
         for (const auto item : data) {
            res += std::popcount(item);
         }
         return res;
      }

      constexpr int countl_zero() const noexcept
      {
         for (size_t i = n_chunks; i-- > 0;) {
            if (data[i]) {
               // The top chunk's padding bits are zero and are counted by std::countl_zero, so remove them
               return int((n_chunks - 1 - i) * n_chunk_bits + size_t(std::countl_zero(data[i])) - n_padding_bits);
            }
         }
         return int(N);
      }

      constexpr int countr_zero() const noexcept
      {
         for (size_t i = 0; i < n_chunks; ++i) {
            if (data[i]) {
               return int(i * n_chunk_bits + size_t(std::countr_zero(data[i])));
            }
         }
         return int(N);
      }

      constexpr bool has_single_bit() const noexcept
      {
         if constexpr (n_chunks == 1) {
            return std::has_single_bit(data[0]);
         }
         else {
            return popcount() == 1;
         }
      }

      constexpr bit_array& flip() noexcept
      {
         for (auto& item : data) {
            item = Chunk(~item);
         }
         if constexpr (n_chunks > 0) {
            data[n_chunks - 1] &= top_chunk_mask;
         }
         return *this;
      }

      constexpr bit_array& operator&=(const bit_array& rhs) noexcept
      {
         for (size_t i{}; i < n_chunks; ++i) {
            data[i] &= rhs.data[i];
         }
         return *this;
      }

      constexpr bit_array operator&(const bit_array& rhs) const noexcept
      {
         auto ret = *this;
         return ret &= rhs;
      }

      constexpr bit_array& operator|=(const bit_array& rhs) noexcept
      {
         for (size_t i{}; i < n_chunks; ++i) {
            data[i] |= rhs.data[i];
         }
         return *this;
      }

      constexpr bit_array operator|(const bit_array& rhs) const noexcept
      {
         auto ret = *this;
         return ret |= rhs;
      }

      constexpr bool operator==(const bit_array& rhs) const noexcept { return data == rhs.data; }

     private:
      static constexpr Chunk mask_of(size_t pos) noexcept { return Chunk(Chunk{1} << (pos % n_chunk_bits)); }
   };
}
