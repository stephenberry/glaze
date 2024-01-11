// Glaze Library
// For the license information refer to glaze.hpp

#ifndef BOOST_UT_DISABLE_MODULE
#define BOOST_UT_DISABLE_MODULE
#endif

#include "glaze/glaze.hpp"

#include "boost/ut.hpp"

using namespace boost::ut;

namespace glz::repe
{
   struct header {
      uint8_t version = 0; // the REPE version
      uint8_t error = 0; // 0 denotes no error
      uint8_t notification = 0; // whether this RPC is a notification (no response returned)
      uint8_t user_data = 0; // whether this RPC contains user data
      std::string method = ""; // the RPC method to call
      std::variant<std::monostate, uint64_t, std::string> id{}; // an identifier
      
      struct glaze {
         using T = header;
         static constexpr auto value = glz::array(&T::version, &T::error, &T::notification, &T::user_data, &T::method, &T::id);
      };
   };

   struct error
   {
      int32_t code = 0;
      std::string message = "";
      
      struct glaze {
         using T = error;
         static constexpr auto value = glz::array(&T::code, &T::message);
      };
   };
   
   struct procedure
   {
      std::function<void(const sv)> user_data{}; // handle user data
      std::function<void(const sv, std::string&)> call{}; // RPC call method
   };
   
   template <opts Opts = opts{}>
   struct server
   {
      // consumes a REPE message and returns a REPE response (empty if notification)
      std::unordered_map<sv, procedure> methods;
      
      std::string response;
      
      void on(const sv name, auto&& call_method) {
         methods[name] = procedure{.call = call_method};
      }
      
      void on(const sv name, auto&& call_method, auto& user_data) {
         methods[name] = procedure{[&](const sv msg){
            std::ignore = glz::read_json(user_data, msg);
         }, call_method};
      }
      
      void call(const sv msg)
      {
         repe::header header;
         context ctx{};
         auto[b, e] = read_iterators<Opts>(ctx, msg);
         auto start = b;
         detail::read<Opts.format>::template op<Opts>(header, ctx, b, e);
         
         if (bool(ctx.error)) {
            parse_error pe{ctx.error, size_t(std::distance(start, b))};
            response = format_error(pe, msg);
            return;
         }
         
         if (*b == '\n') {
            ++b;
         }
         else {
            ctx.error = error_code::syntax_error;
            parse_error pe{ctx.error, size_t(std::distance(start, b))};
            response = format_error(pe, msg);
            return;
         }
         
         if (auto it = methods.find(header.method); it != methods.end()) {
            if (header.user_data) {
               it->second.user_data(msg); // consume the user data
            }
            
            const sv body = msg.substr(size_t(std::distance(start, b)));
            it->second.call(body, response); // handle the body
         }
         else {
            response = "method not found";
         }
      }
   };
}

suite repe_tests = [] {
   "repe"_test = [] {
      glz::repe::server server{};
      
      server.on("summer", [](auto msg, std::string& response){
         std::vector<int> v{};
         std::ignore = glz::read_json(v, msg);
         auto result = std::reduce(std::cbegin(v), std::cend(v));
         glz::write_json(result, response);
      });
      
      std::vector<int> v = {1, 2, 3, 4, 5};
      
      glz::repe::header header{.method = "summer"};
      
      auto message = std::tie(header, v);
      
      std::string buffer;
      glz::write_ndjson(message, buffer);
      
      server.call(buffer);
      
      std::cerr << server.response << '\n';
   };
};

int main()
{
   const auto result = boost::ut::cfg<>.run({.report_errors = true});
   return result;
}
