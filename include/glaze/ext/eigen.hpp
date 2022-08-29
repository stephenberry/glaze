// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/binary/read.hpp"
#include "glaze/binary/write.hpp"
#include "glaze/core/common.hpp"
#include "glaze/json/json_ptr.hpp"
#include "glaze/json/read.hpp"
#include "glaze/json/write.hpp"

#include <span>

namespace glz
{
   namespace detail
   {
      template <class T>
      concept matrix_t = requires(T matrix)
      {
         matrix.resize(2, 4);
         matrix.data();
         {
            matrix.rows()
            } -> std::convertible_to<size_t>;
         {
            matrix.cols()
            } -> std::convertible_to<size_t>;
         {
            matrix.size()
            } -> std::convertible_to<size_t>;
      } &&!nano::ranges::range<T>;
      
      template <matrix_t T>
      struct from_binary<T>
      {
         static_assert(T::RowsAtCompileTime >= 0 && T::ColsAtCompileTime >= 0,
                       "Does not handle dynamic matrices");

         static void op(auto &value, auto&& it, auto&& end)
         {
            std::span<typename T::Scalar, T::RowsAtCompileTime * T::ColsAtCompileTime> view(value.data(), value.size());
            detail::read<binary>::op(view, it, end);
         }
      };
      
      template <matrix_t T>
      struct to_binary<T>
      {
         static_assert(T::RowsAtCompileTime >= 0 && T::ColsAtCompileTime >= 0,
                       "Does not handle dynamic matrices");
         
         template <auto& Opts>
         static void op(auto&& value, auto&& b) noexcept
         {
            std::span<typename T::Scalar, T::RowsAtCompileTime * T::ColsAtCompileTime> view(value.data(), value.size());
            detail::write<binary>::op<Opts>(view, b);
         }
      };
      
      template <matrix_t T>
      struct from_json<T>
      {
         static_assert(T::RowsAtCompileTime >= 0 && T::ColsAtCompileTime >= 0,
                       "Does not handle dynamic matrices");

         static void op(auto &value, auto&& it, auto&& end)
         {
            std::span<typename T::Scalar, T::RowsAtCompileTime * T::ColsAtCompileTime> view(value.data(), value.size());
            detail::read<json>::op(view, it, end);
         }
      };

      template <matrix_t T>
      struct to_json<T>
      {
         static_assert(T::RowsAtCompileTime >= 0 && T::ColsAtCompileTime >= 0,
                       "Does not handle dynamic matrices");

         template <auto& Opts>
         static void op(auto &&value, auto &&b) noexcept
         {
            std::span<typename T::Scalar, T::RowsAtCompileTime * T::ColsAtCompileTime> view(value.data(), value.size());
            detail::write<json>::op<Opts>(view, b);
         }
      };
   }  // namespace detail
}  // namespace glaze
