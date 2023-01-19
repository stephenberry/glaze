// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/binary/read.hpp"
#include "glaze/binary/write.hpp"
#include "glaze/core/common.hpp"
#include "glaze/json/json_ptr.hpp"
#include "glaze/json/read.hpp"
#include "glaze/json/write.hpp"
#include "glaze/api/name.hpp"
#include "glaze/api/std/array.hpp"

#include <span>
#include <Eigen/Core> //Note: You are expected to provide eigen if including this header

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
      } &&!range<T>;
      
      template <matrix_t T>
      struct from_binary<T>
      {
         static_assert(T::RowsAtCompileTime >= 0 && T::ColsAtCompileTime >= 0,
                       "Does not handle dynamic matrices");

         template <auto Opts>
         static void op(auto &value, is_context auto&& ctx, auto&& it, auto&& end)
         {
            std::span<typename T::Scalar, T::RowsAtCompileTime * T::ColsAtCompileTime> view(value.data(), value.size());
            detail::read<binary>::op<Opts>(view, ctx, it, end);
         }
      };
      
      template <matrix_t T>
      struct to_binary<T>
      {
         static_assert(T::RowsAtCompileTime >= 0 && T::ColsAtCompileTime >= 0,
                       "Does not handle dynamic matrices");
         
         template <auto Opts, class... Args>
         static void op(auto&& value,  is_context auto&& ctx, Args&&... args) noexcept
         {
            std::span<typename T::Scalar, T::RowsAtCompileTime * T::ColsAtCompileTime> view(value.data(), value.size());
            detail::write<binary>::op<Opts>(view, ctx, std::forward<Args>(args)...);
         }
      };
      
      template <matrix_t T>
      struct from_json<T>
      {
         static_assert(T::RowsAtCompileTime >= 0 && T::ColsAtCompileTime >= 0,
                       "Does not handle dynamic matrices");

         template <auto Opts>
         static void op(auto &value, is_context auto&& ctx, auto&& it, auto&& end)
         {
            std::span<typename T::Scalar, T::RowsAtCompileTime * T::ColsAtCompileTime> view(value.data(), value.size());
            detail::read<json>::op<Opts>(view, ctx, it, end);
         }
      };

      template <matrix_t T>
      struct to_json<T>
      {
         static_assert(T::RowsAtCompileTime >= 0 && T::ColsAtCompileTime >= 0,
                       "Does not handle dynamic matrices");
         
         template <auto Opts>
         static void op(auto &&value, is_context auto&& ctx, auto &&b, auto&& ix) noexcept
         {
            std::span<typename T::Scalar, T::RowsAtCompileTime * T::ColsAtCompileTime> view(value.data(), value.size());
            detail::write<json>::op<Opts>(view, ctx, b, ix);
         }
      };
   }  // namespace detail
}  // namespace glaze

template <class Scalar, int Rows, int Cols>
struct glz::meta<Eigen::Matrix<Scalar, Rows, Cols>>
{
   static constexpr std::string_view name = detail::join_v<chars<"Eigen::Matrix<">, name_v<Scalar>, chars<",">,  //
                                                         chars<num_to_string<Rows>::value>, chars<",">,        //
                                                         chars<num_to_string<Cols>::value>, chars<",">,        //
                                                         chars<">">>;
};
