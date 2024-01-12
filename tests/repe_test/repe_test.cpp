// Glaze Library
// For the license information refer to glaze.hpp

#ifndef BOOST_UT_DISABLE_MODULE
#define BOOST_UT_DISABLE_MODULE
#endif

#include "glaze/glaze.hpp"

#include "boost/ut.hpp"

using namespace boost::ut;

#include <shared_mutex>

namespace glz::repe
{
   // we put the method and id at the top of the class for easier initialization
   // the order in the actual message is not the same
   struct header final {
      std::string_view method = ""; // the RPC method to call
      std::variant<std::monostate, uint64_t, std::string_view> id{}; // an identifier
      static constexpr uint8_t version = 0; // the REPE version
      uint8_t error = 0; // 0 denotes no error
      uint8_t notification = 0; // whether this RPC is a notification (no response returned)
      uint8_t user_data = 0; // whether this RPC contains user data
      
      struct glaze {
         using T = header;
         static constexpr auto value = glz::array(&T::version, &T::error, &T::notification, &T::user_data, &T::method, &T::id);
      };
   };
   
   enum struct error_e : int32_t {
      no_error = 0,
      server_error_lower = -32000,
      server_error_upper = -32099,
      invalid_request = -32600,
      method_not_found = -32601,
      invalid_params = -32602,
      internal = -32603,
      parse_error = -32700,
   };

   struct error_t final
   {
      error_e code = error_e::no_error;
      std::string message = "";
      
      operator bool() const noexcept {
         return bool(code);
      }
      
      struct glaze {
         using T = error_t;
         static constexpr auto value = glz::array(&T::code, &T::message);
      };
   };
   
   struct state final
   {
      const std::string_view message{};
      const repe::header& header;
      std::string& buffer;
      error_t& error;
   };
   
   struct procedure final
   {
      std::function<void(state&&)> call{}; // RPC call method
      std::function<void(const sv)> user_data{}; // handle user data
   };
   
   template <class T>
   constexpr auto lvalue = std::is_lvalue_reference_v<T>;
   
   namespace detail
   {
      struct string_hash {
        using is_transparent = void;
        [[nodiscard]] size_t operator()(const char *txt) const {
          return std::hash<std::string_view>{}(txt);
        }
        [[nodiscard]] size_t operator()(std::string_view txt) const {
          return std::hash<std::string_view>{}(txt);
        }
        [[nodiscard]] size_t operator()(const std::string &txt) const {
          return std::hash<std::string>{}(txt);
        }
      };
   }
   
   inline auto& get_shared_mutex() {
      static std::shared_mutex mtx{};
      return mtx;
   }
   
   template <opts Opts, class Value>
   bool read_params(Value&& value, auto&& state, auto&& response)
   {
      if constexpr (Opts.format == json) {
         const auto ec = glz::read_json(std::forward<Value>(value), state.message);
         if (ec) {
            glz::write_ndjson(std::forward_as_tuple(header{.error = true}, error_t{error_e::parse_error, format_error(ec, state.message)}), response);
            return true;
         }
         return false;
      }
      else {
         static_assert(false_v<Value>, "TODO: implement BEVE");
      }
   }
   
   template <opts Opts, class Value>
   void write_response(Value&& value, auto&& state) {
      if (state.error) {
         if constexpr (Opts.format == json) {
            write_ndjson(std::forward_as_tuple(header{.error = true}, state.error), state.buffer);
         }
         else {
            static_assert(false_v<Value>, "TODO: implement BEVE");
         }
      }
      else {
         if constexpr (Opts.format == json) {
            glz::write_ndjson(std::forward_as_tuple(state.header, std::forward<Value>(value)), state.buffer);
         }
         else {
            static_assert(false_v<Value>, "TODO: implement BEVE");
         }
      }
   }
   
   // DESIGN NOTE: It might appear that we are locking ourselves into a poor design choice by using a runtime std::unordered_map.
   // However, we can actually improve this in the future by allowing glz::meta specialization on the server to
   // list out the method names and allow us to build a compile time map prior to function registration.
   // This is made possible by constinit, and the same approach used for compile time maps and struct reflection.
   // This will then be an opt-in performance improvement where a bit more code must be written by the user to
   // list the method names.
   
   // This server is designed to be lightweight, and meant to be constructed on a per client basis
   // This server does not support adding methods from RPC calls or adding methods once RPC calls can be made
   // Each instance of this server is expected to be accessed by a single thread, so a single std::string response buffer is used
   // You can register object memory as the input parameter and the output parameter, which can improve performance or may be required for hardware interfaces or restricted memory interfaces
   // Access to this registered memory is thread safe across server instances
   // No thread locks are needed if you don't pass input and output parameters by reference
   template <opts Opts = opts{}, class UserHeader = void>
   struct server
   {
      std::unordered_map<std::string_view, procedure, detail::string_hash, std::equal_to<>> methods;
      
      std::string response;
      
      error_t error{};
            
      template <class Callback>
      void on(const sv name, Callback&& callback) {
         static_assert(is_lambda_concrete<std::remove_cvref_t<Callback>>);
         using Tuple = lambda_args_t<std::remove_cvref_t<Callback>>;
         constexpr auto N = std::tuple_size_v<Tuple>;
         if constexpr (N == 2) {
            using Params = std::decay_t<std::tuple_element_t<0, Tuple>>;
            using Result = std::decay_t<std::tuple_element_t<1, Tuple>>;
            
            methods.emplace(name, procedure{[this, params = Params{}, result = Result{}, callback](auto&& state) mutable {
               // no need to lock locals
               if (read_params<Opts>(params, state, response)) {
                  return;
               }
               callback(params, result);
               write_response<Opts>(result, state);
            }});
         }
         else {
            static_assert(false_v<Callback>, "Requires params and results inputs");
         }
      }
      
      template <class Params, class Result, class Callback>// requires std::is_assignable_v<std::function<void()>, Callback>
      void on(const sv name, Params&& params, Result&& result, Callback&& callback) {
         if constexpr (lvalue<Params> && lvalue<Result>) {
            methods.emplace(name, procedure{[this, &params, &result, callback](auto&& state){
               // we must lock access to params and result as multiple clients might need to manipulate them
               std::unique_lock lock{get_shared_mutex()};
               if (read_params<Opts>(params, state, response)) {
                  return;
               }
               callback(params, result);
               write_response<Opts>(result, state);
            }});
         }
         else if constexpr (lvalue<Params> && !lvalue<Result>) {
            methods.emplace(name, procedure{[this, &params, result, callback](auto&& state) mutable {
               {
                  std::unique_lock lock{get_shared_mutex()};
                  if (read_params<Opts>(params, state, response)) {
                     return;
                  }
                  callback(params, result);
               }
               // no need to lock local result writing
               write_response<Opts>(result, state);
            }});
         }
         else if constexpr (!lvalue<Params> && lvalue<Result>) {
            methods.emplace(name, procedure{[this, params, &result, callback](auto&& state) mutable {
               // no need to lock locals
               if (read_params<Opts>(params, state, response)) {
                  return;
               }
               std::unique_lock lock{get_shared_mutex()};
               callback(params, result);
               write_response<Opts>(result, state);
            }});
         }
         else if constexpr (!lvalue<Params> && !lvalue<Result>) {
            methods.emplace(name, procedure{[this, params, result, callback](auto&& state) mutable {
               // no need to lock locals
               if (read_params<Opts>(params, state, response)) {
                  return;
               }
               callback(params, result);
               write_response<Opts>(result, state);
            }});
         }
         else {
            static_assert(false_v<Result>, "unsupported");
         }
      }
      
      // TODO: Implement JSON Pointer lock free path access when paths do not collide
      /*template <class Params, class Result, class Callback>
      void on(const sv name, Params&& params, Result&& result, Callback&& callback) {
         
      }*/
      
      void call(const sv msg)
      {
         header h;
         context ctx{};
         auto[b, e] = read_iterators<Opts>(ctx, msg);
         auto start = b;
         glz::detail::read<Opts.format>::template op<Opts>(h, ctx, b, e);
         
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
            write_ndjson(std::forward_as_tuple(header{.error = true}, error_t{error_e::parse_error, format_error(pe, msg)}), response);
            return;
         }
         
         if (auto it = methods.find(h.method); it != methods.end()) {
            if (h.user_data) {
               it->second.user_data(msg); // consume the user data
            }
            
            const sv body = msg.substr(size_t(std::distance(start, b)));
            it->second.call(state{body, h, response, error}); // handle the body
         }
         else {
            write_ndjson(std::forward_as_tuple(header{.error = true}, error_t{error_e::method_not_found}), response);
         }
      }
   };
   
   template <class Value>
   inline auto request_json(const header& header, Value&& value) {
      return glz::write_ndjson(std::forward_as_tuple(header, std::forward<Value>(value)));
   }
}

namespace repe = glz::repe;

struct my_struct
{
   std::string hello{};
   std::string world{};
};

suite repe_tests = [] {
   "references"_test = [] {
      repe::server server{};
      
      my_struct params{};
      std::string result{};
      
      server.on("concat", params, result, [](auto& params, auto& result){
         params.hello = "Aha";
         result = params.hello + " " + params.world;
      });
      
      {
         my_struct params{"Hello", "World"};
         
         auto request = repe::request_json({"concat", 5ul}, params);
         
         server.call(request);
      }
      
      expect(server.response ==
R"([0,0,0,0,"concat",5]
"Aha World")");
   };
   
   "concrete internal storage"_test = [] {
      repe::server server{};
      
      server.on("concat", [](my_struct& params, std::string& result){
         params.hello = "Aha";
         result = params.hello + " " + params.world;
      });
      
      {
         my_struct params{"Hello", "World"};
         
         auto request = repe::request_json({"concat", 5ul}, params);
         
         server.call(request);
      }
      
      expect(server.response ==
R"([0,0,0,0,"concat",5]
"Aha World")");
   };
   
   "error support"_test = [] {
      repe::server server{};
      
      server.on("concat", [&](my_struct& params, std::string& result){
         params.hello = "Aha";
         result = params.hello + " " + params.world;
         server.error = {repe::error_e::internal, "my custom error"};
      });
      
      {
         my_struct params{"Hello", "World"};
         
         auto request = repe::request_json({"concat", 5ul}, params);
         
         server.call(request);
      }
      
      expect(server.response ==
R"([0,1,0,0,"",null]
[-32603,"my custom error"])") << server.response;
   };
};

int main()
{
   const auto result = boost::ut::cfg<>.run({.report_errors = true});
   return result;
}
