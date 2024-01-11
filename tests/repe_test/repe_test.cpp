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
   // we put the method and id at the top of the class for easier initialization
   // the order in the actual message is not the same
   struct header {
      std::string method = ""; // the RPC method to call
      std::variant<std::monostate, uint64_t, std::string> id{}; // an identifier
      static constexpr uint8_t version = 0; // the REPE version
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
      const sv message{};
      const repe::header& header;
      std::string& buffer;
   };
   
   struct procedure
   {
      std::function<void(state&&)> call{}; // RPC call method
      std::function<void(const sv)> user_data{}; // handle user data
   };
   
   template <opts Opts = opts{}>
   struct server
   {
      std::unordered_map<sv, procedure> methods;
      
      std::string response;
      
      template <class Params, class Callback> requires std::is_assignable_v<std::function<void()>, Callback>
      void on(const sv name, Params& params, Callback&& callback) {
         methods[name] = procedure{[this, &params, callback](auto&& state){
            if (this->read_json_params(params, state)) {
               return;
            }
            this->write_json_response(callback(), state);
         }};
      }
      
      template <class Callback> requires std::is_assignable_v<std::function<void(state&&)>, Callback>
      void on(const sv name, Callback&& callback) {
         methods[name] = procedure{callback};
      }
      
      template <class Callback> requires is_lambda_concrete<std::remove_cvref_t<Callback>>
      void on(const sv name, Callback&& callback) {
         using Tuple = lambda_args_t<std::remove_cvref_t<Callback>>;
         constexpr auto N = std::tuple_size_v<Tuple>;
         if constexpr (N == 1) {
            using Input = std::decay_t<std::tuple_element_t<0, Tuple>>;
            methods[name] = procedure{[this, callback](auto&& state){
               static thread_local Input params{};
               if (this->read_json_params(params, state)) {
                  return;
               }
               this->write_json_response(callback(params), state);
            }};
         }
         else {
            static_assert(false_v<Callback>, "Your lambda must have a single input");
         }
      }
      
      bool read_json_params(auto&& value, auto&& state)
      {
         const auto ec = glz::read_json(value, state.message);
         if (ec) {
            glz::write_ndjson(std::tuple(header{.error = 1}, error{1, format_error(ec, state.message)}), response);
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

struct my_struct
{
   std::string hello = "Hello";
   std::string world = "World";
};

suite repe_tests = [] {
   "repe"_test = [] {
      repe::server server{};
      
      my_struct s{};
      
      server.on("concat", s, [&]{
         s.hello = "Aha";
         return s.hello + " " + s.world;
      });
      
      {
         my_struct s{};
         
         auto request = repe::request({"concat", 5ul}, s);
         
         server.call(request);
      }
      
      expect(server.response ==
R"([0,0,0,0,"concat",5]
"Aha World")");
   };
   
   "repe static value"_test = [] {
      repe::server server{};
      
      server.on("concat", [](my_struct& s){
         s.hello = "Aha";
         return s.hello + " " + s.world;
      });
      
      {
         my_struct s{};
         
         auto request = repe::request({"concat", 5ul}, s);
         
         server.call(request);
      }
      
      expect(server.response ==
R"([0,0,0,0,"concat",5]
"Aha World")");
   };
   
   "repe low level handling"_test = [] {
      repe::server server{};
      
      my_struct s{};
      
      server.on("concat", [&](auto&& state){
         if (server.read_json_params(s, state)) {
            return;
         }
         
         s.hello = "Aha";
         auto result = s.hello + " " + s.world;
         
         server.write_json_response(result, state);
      });
      
      {
         my_struct s{};
         
         auto request = repe::request({"concat", 5ul}, s);
         
         server.call(request);
      }
      
      expect(server.response ==
R"([0,0,0,0,"concat",5]
"Aha World")");
   };
};

int main()
{
   const auto result = boost::ut::cfg<>.run({.report_errors = true});
   return result;
}
