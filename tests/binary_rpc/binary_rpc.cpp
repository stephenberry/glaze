// Glaze Library
// For the license information refer to glaze.hpp

#ifndef BOOST_UT_DISABLE_MODULE
#define BOOST_UT_DISABLE_MODULE
#endif

#include "boost/ut.hpp"
#include "glaze/binary/read.hpp"
#include "glaze/binary/write.hpp"
#include "glaze/binary/rpc.hpp"

using namespace boost::ut;

struct my_request_data
{
   uint32_t integer{};
   std::string string{};
   
   struct glaze {
      using T = my_request_data;
      static constexpr auto value = glz::object(&T::integer, &T::string);
   };
};

suite repe_write_read = [] {
   "repe_write_read"_test = [] {
      // This syntax works, but we can wrap this into a cleaner interface. Let this be the low level syntax.
      my_request_data params{55, "hello"};
      glz::repe::message<my_request_data> msg{glz::repe::header{.method = "func"}, params};
      
      std::string buffer;
      glz::write_binary(msg, buffer);
      
      params = {};
      msg.header = {};
      expect(!glz::read_binary(msg, buffer));
      
      expect(msg.header.method == "func");
      expect(params.integer == 55);
      expect(params.string == "hello");
   };
};

int main()
{
   return boost::ut::cfg<>.run({.report_errors = true});
}
