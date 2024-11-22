#pragma once

namespace glz::detail
{
   template <class T>
   struct string_as_atom_t
   {
      T& val;
   };

   // Read and write string as atoms

   template <class T>
   struct from<ERLANG, string_as_atom_t<T>>
   {
      template <auto Opts>
      static void op(auto&& value, auto&&... args)
      {
         eetf::atom a;
         read<ERLANG>::op<Opts>(a, args...);
         value.val = a;
      }
   };

   template <class T>
   struct to<ERLANG, string_as_atom_t<T>>
   {
      template <auto Opts>
      static void op(auto&& value, is_context auto&& ctx, auto&&... args) noexcept
      {
         write<ERLANG>::op<Opts>(eetf::atom{value.val}, ctx, args...);
      }
   };

   template <auto MemPtr>
   constexpr decltype(auto) string_as_atom()
   {
      return [](auto&& val) { return string_as_atom_t<std::decay_t<decltype(val.*MemPtr)>>{val.*MemPtr}; };
   }
}
