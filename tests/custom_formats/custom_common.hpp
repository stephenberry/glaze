// Glaze Library
// For the license information refer to glaze.hpp

// Shared body for the per-format glz::custom tests.
//
// Each translation unit includes exactly ONE format header and then this file. That isolation is
// the point: glz::custom is implemented in "glaze/core/custom.hpp", and the bug this guards
// (issue #2786) was that several format headers never pulled it in and only worked by accident
// when a JSON header happened to be included alongside. Do not include any other glaze header.

#pragma once

#include <string>

#include "ut/ut.hpp"

namespace custom_formats
{
   // Setter and getter that transform the value, so a silent pass-through cannot pass the test.
   struct getter_setter
   {
      int v{};
      void set_v(int in) { v = in * 2; }
      int get_v() const { return v + 1; }
   };

   // A setter taking no input at all. The incoming value is discarded and the handler invoked;
   // this is the path that used to require a format-specific skip_array.
   struct no_input
   {
      int calls{};
      int payload{9};
      void trigger() { ++calls; }
      int get() const { return payload; }
   };

   // glz::skip on the read side: written, never read back.
   struct skip_read
   {
      int a{5};
      int b{6};
   };

   // Lambda handlers, including the form that takes a glz::context to report its own error.
   struct lambda_handlers
   {
      int age{};
   };
}

template <>
struct glz::meta<custom_formats::getter_setter>
{
   using T = custom_formats::getter_setter;
   static constexpr auto value = object("v", custom<&T::set_v, &T::get_v>);
};

template <>
struct glz::meta<custom_formats::no_input>
{
   using T = custom_formats::no_input;
   static constexpr auto value = object("p", custom<&T::trigger, &T::get>);
};

template <>
struct glz::meta<custom_formats::skip_read>
{
   using T = custom_formats::skip_read;
   static constexpr auto value = object("a", custom<glz::skip{}, &T::a>, "b", &T::b);
};

template <>
struct glz::meta<custom_formats::lambda_handlers>
{
   using T = custom_formats::lambda_handlers;
   static constexpr auto read_age = [](T& s, int age, glz::context& ctx) {
      if (age < 21) {
         ctx.error = glz::error_code::constraint_violated;
         ctx.custom_error_message = "age too young";
      }
      else {
         s.age = age;
      }
   };
   static constexpr auto value = object("age", custom<read_age, &T::age>);
};

namespace custom_formats
{
   // Registers the same four checks for whichever format the including TU selected. The options
   // value is the parameter rather than the format id because EETF carries its own opts type.
   template <auto opts>
   ut::suite make_suite(const std::string& format_name)
   {
      using namespace ut;
      return [format_name] {
         test(format_name + ": member function getter and setter") = [] {
            getter_setter o{};
            o.v = 10; // getter writes 11
            std::string buf{};
            expect(not glz::write<opts>(o, buf));

            getter_setter r{};
            auto ec = glz::read<opts>(r, buf);
            expect(not ec) << glz::format_error(ec, buf);
            expect(r.v == 22) << r.v; // setter doubles what the getter wrote
         };

         test(format_name + ": setter taking no input discards the value") = [] {
            no_input o{};
            std::string buf{};
            expect(not glz::write<opts>(o, buf));

            no_input r{};
            auto ec = glz::read<opts>(r, buf);
            expect(not ec) << glz::format_error(ec, buf);
            expect(r.calls == 1) << r.calls;
         };

         test(format_name + ": glz::skip leaves the member untouched") = [] {
            skip_read o{};
            std::string buf{};
            expect(not glz::write<opts>(o, buf));

            skip_read r{};
            r.a = 0;
            r.b = 0;
            auto ec = glz::read<opts>(r, buf);
            expect(not ec) << glz::format_error(ec, buf);
            expect(r.a == 0) << r.a;
            expect(r.b == 6) << r.b;
         };

         test(format_name + ": setter reports its own error through the context") = [] {
            lambda_handlers o{};
            o.age = 18;
            std::string buf{};
            expect(not glz::write<opts>(o, buf));

            lambda_handlers r{};
            auto ec = glz::read<opts>(r, buf);
            expect(ec.ec == glz::error_code::constraint_violated);
            expect(r.age == 0) << r.age;

            o.age = 25;
            buf.clear();
            expect(not glz::write<opts>(o, buf));
            auto ec2 = glz::read<opts>(r, buf);
            expect(not ec2) << glz::format_error(ec2, buf);
            expect(r.age == 25) << r.age;
         };
      };
   }
}
