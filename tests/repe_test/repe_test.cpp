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
      std::string method = ""; // the RPC method to call
      std::variant<std::monostate, uint64_t, std::string> id{}; // an identifier
      uint8_t version = 0; // the REPE version
      uint8_t error = 0; // 0 denotes no error
      uint8_t notification = 0; // whether this RPC is a notification (no response returned)
      uint8_t user_data = 0; // whether this RPC contains user data
      
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
   
   struct state
   {
      const sv msg{};
      const repe::header& header;
      std::string& buffer;
   };
   
   struct procedure
   {
      std::function<void(const sv)> user_data{}; // handle user data
      std::function<void(state&&)> call{}; // RPC call method
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
      
      bool read_json_params(auto&& value, auto&& state)
      {
         const auto ec = glz::read_json(value, state.msg);
         if (ec) {
            glz::write_ndjson(std::tuple(header{.error = 1}, error{1, format_error(ec, state.msg)}), response);
            return true;
         }
         return false;
      }
      
      template <class Value>
      void write_json_response(Value&& value, auto&& state) {
         glz::write_ndjson(std::forward_as_tuple(state.header, std::forward<Value>(value)), state.buffer);
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
            it->second.call(state{body, header, response}); // handle the body
         }
         else {
            response = "method not found";
         }
      }
   };
   
   template <class Value>
   inline auto request(const header& header, Value&& value) {
      return glz::write_ndjson(std::forward_as_tuple(header, std::forward<Value>(value)));
   }
}

namespace repe = glz::repe;

suite repe_tests = [] {
   "repe"_test = [] {
      repe::server server{};
      
      server.on("summer", [&](auto&& state){
         std::vector<int> v{};
         if (server.read_json_params(v, state)) {
            return;
         }
         
         auto result = std::reduce(v.begin(), v.end());
         
         server.write_json_response(result, state);
      });
      
      std::vector<int> v = {1, 2, 3, 4, 5};
      
      auto request = repe::request({"summer", 5ul}, v);
      
      server.call(request);
      
      std::cerr << server.response << '\n';
   };
};

int main()
{
   const auto result = boost::ut::cfg<>.run({.report_errors = true});
   return result;
}
