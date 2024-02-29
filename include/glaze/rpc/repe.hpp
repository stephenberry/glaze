// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <shared_mutex>

#include "glaze/glaze.hpp"

namespace glz::repe
{
   constexpr uint8_t notify = 0b00000001;
   constexpr uint8_t empty = 0b00000010;
   
   // we put the method and id at the top of the class for easier initialization
   // the order in the actual message is not the same
   struct header final
   {
      std::string_view method = ""; // the RPC method to call
      std::variant<std::monostate, uint64_t, std::string_view> id{}; // an identifier
      static constexpr uint8_t version = 0; // the REPE version
      uint8_t error = 0; // 0 denotes no error
      uint8_t action = 0; // how the RPC is to be handled (supports notifications and more)

      struct glaze
      {
         using T = header;
         static constexpr sv name = "glz::repe::header"; // Hack to fix MSVC (shouldn't be needed)
         static constexpr auto value = glz::array(&T::version, &T::error, &T::action, &T::method, &T::id);
      };
   };
   
   template <class T>
   concept is_header = std::same_as<std::decay_t<T>, header>;

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

      operator bool() const noexcept { return bool(code); }

      struct glaze
      {
         using T = error_t;
         static constexpr auto value = glz::array(&T::code, &T::message);
      };
   };

   struct state final
   {
      const std::string_view message{};
      repe::header& header;
      std::string& buffer;
      error_t& error;
   };
   
   template <class T>
   concept is_state = std::same_as<std::decay_t<T>, state>;

   template <class T>
   constexpr auto lvalue = std::is_lvalue_reference_v<T>;

   namespace detail
   {
      struct string_hash
      {
         using is_transparent = void;
         [[nodiscard]] size_t operator()(const char* txt) const { return std::hash<std::string_view>{}(txt); }
         [[nodiscard]] size_t operator()(std::string_view txt) const { return std::hash<std::string_view>{}(txt); }
         [[nodiscard]] size_t operator()(const std::string& txt) const { return std::hash<std::string>{}(txt); }
      };
   }

   inline auto& get_shared_mutex()
   {
      static std::shared_mutex mtx{};
      return mtx;
   }

   // returns 0 on error
   template <opts Opts, class Value>
   size_t read_params(Value&& value, auto&& state, auto&& response)
   {
      if constexpr (Opts.format == json) {
         glz::context ctx{};
         auto [b, e] = read_iterators<Opts>(ctx, state.message);
         auto start = b;

         glz::detail::read<Opts.format>::template op<Opts>(std::forward<Value>(value), ctx, b, e);

         if (bool(ctx.error)) {
            parse_error ec{ctx.error, size_t(std::distance(start, b)), ctx.includer_error};
            write_json(std::forward_as_tuple(header{.error = true},
                                             error_t{error_e::parse_error, format_error(ec, state.message)}),
                       response);
            return 0;
         }

         return size_t(std::distance(start, b));
      }
      else {
         static_assert(false_v<Value>, "TODO: implement BEVE");
      }
   }

   template <opts Opts, class Value>
   void write_response(Value&& value, is_state auto&& state)
   {
      if (state.error) {
         if constexpr (Opts.format == json) {
            write_json(std::forward_as_tuple(header{.error = true}, state.error), state.buffer);
         }
         else {
            static_assert(false_v<Value>, "TODO: implement BEVE");
         }
      }
      else {
         if constexpr (Opts.format == json) {
            state.header.action &= ~empty; // clear empty bit because we are writing a response
            write_json(std::forward_as_tuple(state.header, std::forward<Value>(value)), state.buffer);
         }
         else {
            static_assert(false_v<Value>, "TODO: implement BEVE");
         }
      }
   }
   
   template <opts Opts>
   void write_response(is_state auto&& state)
   {
      if (state.error) {
         if constexpr (Opts.format == json) {
            write_json(std::forward_as_tuple(header{.error = true}, state.error), state.buffer);
         }
         else {
            static_assert(false_v<decltype(Opts)>, "TODO: implement BEVE");
         }
      }
      else {
         if constexpr (Opts.format == json) {
            state.header.action = empty;
            write_json(std::forward_as_tuple(state.header, nullptr), state.buffer);
         }
         else {
            static_assert(false_v<decltype(Opts)>, "TODO: implement BEVE");
         }
      }
   }
   
   struct ignore_result final {};

   template <opts Opts, class Result>
   [[nodiscard]] error_t decode_response(Result&& result, auto& buffer)
   {
      repe::header h;
      context ctx{};
      auto [b, e] = read_iterators<Opts>(ctx, buffer);
      auto start = b;

      // clang 14 won't build when capturing from structured binding
      auto handle_error = [&](auto& it) {
         ctx.error = error_code::syntax_error;
         parse_error pe{ctx.error, size_t(std::distance(start, it)), ctx.includer_error};
         return error_t{error_e::parse_error, format_error(pe, buffer)};
      };

      if (*b == '[') {
         ++b;
      }
      else {
         return handle_error(b);
      }

      glz::detail::read<Opts.format>::template op<Opts>(h, ctx, b, e);

      if (bool(ctx.error)) {
         parse_error pe{ctx.error, size_t(std::distance(start, b)), ctx.includer_error};
         return {error_e::parse_error, format_error(pe, buffer)};
      }

      if (*b == ',') {
         ++b;
      }
      else {
         return handle_error(b);
      }

      if (h.error) {
         error_t error{};
         glz::detail::read<Opts.format>::template op<Opts>(error, ctx, b, e);
         return error;
      }
      
      if constexpr (!std::same_as<std::decay_t<Result>, ignore_result>) {
         glz::detail::read<Opts.format>::template op<Opts>(result, ctx, b, e);
         
         if (bool(ctx.error)) {
            parse_error pe{ctx.error, size_t(std::distance(start, b)), ctx.includer_error};
            return {error_e::parse_error, format_error(pe, buffer)};
         }
      }
      
      return {};
   }
   
   template <opts Opts>
   [[nodiscard]] error_t decode_response(auto& buffer) {
      return decode_response<Opts>(ignore_result{}, buffer);
   }
   
   template <opts Opts, class Value>
   inline auto request(const header& header, Value&& value)
   {
      return glz::write<Opts>(std::forward_as_tuple(header, std::forward<Value>(value)));
   }

   template <opts Opts, class Value>
   inline auto request(const header& header, Value&& value, auto& buffer)
   {
      return glz::write<Opts>(std::forward_as_tuple(header, std::forward<Value>(value)), buffer);
   }
   
   inline auto request_json(header&& header) {
      header.action |= empty; // because no value provided
      return glz::write_json(std::forward_as_tuple(header, nullptr)); }
   
   inline auto request_json(const header& h) {
      repe::header copy = h;
      copy.action |= empty; // because no value provided
      return request_json(std::move(copy));
   }

   template <class Value>
   inline auto request_json(const header& header, Value&& value)
   {
      return glz::write_json(std::forward_as_tuple(header, std::forward<Value>(value)));
   }

   template <class Value>
   inline void request_json(const header& header, Value&& value, auto& buffer)
   {
      glz::write_json(std::forward_as_tuple(header, std::forward<Value>(value)), buffer);
   }

   // DESIGN NOTE: It might appear that we are locking ourselves into a poor design choice by using a runtime
   // std::unordered_map. However, we can actually improve this in the future by allowing glz::meta specialization on
   // the server to list out the method names and allow us to build a compile time map prior to function registration.
   // This is made possible by constinit, and the same approach used for compile time maps and struct reflection.
   // This will then be an opt-in performance improvement where a bit more code must be written by the user to
   // list the method names.

   // This server is designed to be lightweight, and meant to be constructed on a per client basis
   // This server does not support adding methods from RPC calls or adding methods once RPC calls can be made
   // Each instance of this server is expected to be accessed by a single thread, so a single std::string response
   // buffer is used. You can register object memory as the input parameter and the output parameter, which can improve
   // performance or may be required for hardware interfaces or restricted memory interfaces Access to this registered
   // memory is thread safe across server instances No thread locks are needed if you don't pass input and output
   // parameters by reference
   template <opts Opts = opts{}>
   struct registry
   {
      using procedure = std::function<void(state&&)>; // RPC method
      std::unordered_map<std::string_view, procedure, detail::string_hash, std::equal_to<>> methods;

      std::string response;

      error_t error{};

      static constexpr std::string_view default_parent = "";

      template <class T, const std::string_view& parent = default_parent>
         requires(glz::detail::glaze_object_t<T> || glz::detail::reflectable<T>)
      void on(T& value)
      {
         using namespace glz::detail;
         static constexpr auto N = [] {
            if constexpr (reflectable<T>) {
               return count_members<T>;
            }
            else {
               return std::tuple_size_v<meta_t<T>>;
            }
         }();

         [[maybe_unused]] decltype(auto) t = [&] {
            if constexpr (reflectable<T>) {
               return to_tuple(value);
            }
            else {
               return nullptr;
            }
         }();
         
         if constexpr (parent == "" && (glaze_object_t<T> || reflectable<T>)) {
            // build read/write calls to the top level object
            methods.emplace("", [this, &value](repe::state&& state) {
               if (!(state.header.action & empty)) {
                  if (read_params<Opts>(value, state, response) == 0) {
                     return;
                  }
               }
               
               if (state.header.action & notify) {
                  return;
               }
               
               if (state.header.action & empty) {
                  write_response<Opts>(value, state);
               }
               else {
                  write_response<Opts>(state);
               }
            });
         }

         for_each<N>([&](auto I) {
            using Element = glaze_tuple_element<I, N, T>;
            static constexpr std::string_view full_key = [&] {
               if constexpr (parent == "") {
                  return join_v<chars<"/">, key_name<I, T, Element::use_reflection>>;
               }
               else {
                  return join_v<parent, chars<"/">, key_name<I, T, Element::use_reflection>>;
               }
            }();

            using E = typename Element::type;
            decltype(auto) func = [&]() -> decltype(auto) {
               if constexpr (reflectable<T>) {
                  return std::get<I>(t);
               }
               else {
                  return get_member(value, get<Element::member_index>(get<I>(meta_v<T>)));
               }
            }();
            
            // This logic chain should match glz::cli_menu
            using Func = decltype(func);
            if constexpr (std::is_invocable_v<Func>) {
               using Result = std::invoke_result_t<Func>;
               if constexpr (std::same_as<Result, void>) {                  
                  methods.emplace(full_key,
                                  [callback = func](repe::state&& state) mutable {
                                     callback();
                                     if (state.header.action & notify) {
                                        return;
                                     }
                     write_response<Opts>(state);
                                  });
               }
               else {
                  methods.emplace(full_key,
                                  [result = Result{}, callback = func](repe::state&& state) mutable {
                                     result = callback();
                                     if (state.header.action & notify) {
                                        return;
                                     }
                                     write_response<Opts>(result, state);
                                  });
               }
            }
            else if constexpr (is_invocable_concrete<std::remove_cvref_t<Func>>) {
               using Tuple = invocable_args_t<std::remove_cvref_t<Func>>;
               constexpr auto N = std::tuple_size_v<Tuple>;
               static_assert(N == 1, "Only one input is allowed for your function");
               
               using Params = std::tuple_element_t<0, Tuple>;
               using Result = std::invoke_result_t<Func, Params>;
               
               methods.emplace(full_key, [this, params = std::decay_t<Params>{}, result = std::decay_t<Result>{}, callback = func](repe::state&& state) mutable {
                  // no need to lock locals
                  if (read_params<Opts>(params, state, response) == 0) {
                     return;
                  }
                  result = callback(params);
                  if (state.header.action & notify) {
                     return;
                  }
                  write_response<Opts>(result, state);
               });
            }
            else if constexpr (glaze_object_t<E> || reflectable<E>) {
               if constexpr (reflectable<T>) {
                  on<std::decay_t<E>, full_key>(std::get<I>(t));
               }
               else {
                  decltype(auto) member = get_member(value, get<Element::member_index>(get<I>(meta_v<T>)));
                  on<std::decay_t<E>, full_key>(member);
               }
               
               // build read/write calls to the object as a variable
               methods.emplace(full_key, [this, &func](repe::state&& state) {
                  if (!(state.header.action & empty)) {
                     if (read_params<Opts>(func, state, response) == 0) {
                        return;
                     }
                  }
                  
                  if (state.header.action & notify) {
                     return;
                  }
                  
                  if (state.header.action & empty) {
                     write_response<Opts>(func, state);
                  }
                  else {
                     write_response<Opts>(state);
                  }
               });
            }
            else {
               static_assert(std::is_lvalue_reference_v<decltype(func)>);
               // this is a variable and not a function, so we build RPC read/write calls
               methods.emplace(full_key, [this, &func](repe::state&& state) {
                  if (!(state.header.action & empty)) {
                     if (read_params<Opts>(func, state, response) == 0) {
                        return;
                     }
                  }
                  
                  if (state.header.action & notify) {
                     return;
                  }
                  
                  if (state.header.action & empty) {
                     write_response<Opts>(func, state);
                  }
                  else {
                     write_response<Opts>(state);
                  }
               });
            }
         });
      }

      template <class Callback>
      void on(const sv name, Callback&& callback)
      {
         static_assert(is_invocable_concrete<std::remove_cvref_t<Callback>>);
         using Tuple = invocable_args_t<std::remove_cvref_t<Callback>>;
         constexpr auto N = std::tuple_size_v<Tuple>;
         if constexpr (N == 2) {
            using Params = std::decay_t<std::tuple_element_t<0, Tuple>>;
            using Result = std::decay_t<std::tuple_element_t<1, Tuple>>;

            methods.emplace(name, [this, params = Params{}, result = Result{}, callback](repe::state&& state) mutable {
               // no need to lock locals
               if (read_params<Opts>(params, state, response) == 0) {
                  return;
               }
               callback(params, result);
               if (state.header.action & notify) {
                  return;
               }
               write_response<Opts>(result, state);
            });
         }
         else {
            static_assert(false_v<Callback>, "Requires params and results inputs");
         }
      }

      template <class Params, class Result, class Callback>
      void on(const sv name, Params&& params, Result&& result, Callback&& callback)
      {
         if constexpr (lvalue<Params> && lvalue<Result>) {
            methods.emplace(name, procedure{[this, &params, &result, callback](repe::state&& state) {
                               // we must lock access to params and result as multiple clients might need to manipulate
                               // them
                               std::unique_lock lock{get_shared_mutex()};
                               if (read_params<Opts>(params, state, response) == 0) {
                                  return;
                               }
                               callback(params, result);
                               if (state.header.action & notify) {
                                  return;
                               }
                               write_response<Opts>(result, state);
                            }});
         }
         else if constexpr (lvalue<Params> && !lvalue<Result>) {
            methods.emplace(name, [this, &params, result, callback](repe::state&& state) mutable {
               {
                  std::unique_lock lock{get_shared_mutex()};
                  if (read_params<Opts>(params, state, response) == 0) {
                     return;
                  }
                  callback(params, result);
               }
               // no need to lock local result writing
               if (state.header.action & notify) {
                  return;
               }
               write_response<Opts>(result, state);
            });
         }
         else if constexpr (!lvalue<Params> && lvalue<Result>) {
            methods.emplace(name, [this, params, &result, callback](repe::state&& state) mutable {
               // no need to lock locals
               if (read_params<Opts>(params, state, response) == 0) {
                  return;
               }
               std::unique_lock lock{get_shared_mutex()};
               callback(params, result);
               if (state.header.action & notify) {
                  return;
               }
               write_response<Opts>(result, state);
            });
         }
         else if constexpr (!lvalue<Params> && !lvalue<Result>) {
            methods.emplace(name, [this, params, result, callback](repe::state&& state) mutable {
               // no need to lock locals
               if (read_params<Opts>(params, state, response) == 0) {
                  return;
               }
               callback(params, result);
               if (state.header.action & notify) {
                  return;
               }
               write_response<Opts>(result, state);
            });
         }
         else {
            static_assert(false_v<Result>, "unsupported");
         }
      }

      // TODO: Implement JSON Pointer lock free path access when paths do not collide
      /*template <class Params, class Result, class Callback>
      void on(const sv name, Params&& params, Result&& result, Callback&& callback) {

      }*/
      
      template <class Value>
      bool call(const header& header)
      {
         return call(request<Opts>(header));
      }
            
      template <class Value>
      bool call(const header& header, Value&& value)
      {
         return call(request<Opts>(header, std::forward<Value>(value)));
      }

      template <class Value>
      bool call(const header& header, Value&& value, auto& buffer)
      {
         return call(request<Opts>(header, std::forward<Value>(value), buffer));
      }

      // returns true if there is a result to send (not a notification)
      bool call(const sv msg)
      {
         header h;
         context ctx{};
         auto [b, e] = read_iterators<Opts>(ctx, msg);
         auto start = b;

         auto handle_error = [&](auto& it) {
            ctx.error = error_code::syntax_error;
            parse_error pe{ctx.error, size_t(std::distance(start, it)), ctx.includer_error};
            write_json(
               std::forward_as_tuple(header{.error = true}, error_t{error_e::parse_error, format_error(pe, msg)}),
               response);
         };

         if (*b == '[') {
            ++b;
         }
         else {
            handle_error(b);
            return !(h.action & notify);
         }

         glz::detail::read<Opts.format>::template op<Opts>(h, ctx, b, e);

         if (bool(ctx.error)) {
            parse_error pe{ctx.error, size_t(std::distance(start, b)), ctx.includer_error};
            response = format_error(pe, msg);
            return !(h.action & notify);
         }

         if (*b == ',') {
            ++b;
         }
         else {
            handle_error(b);
            return !(h.action & notify);
         }

         if (auto it = methods.find(h.method); it != methods.end()) {
            const sv body = msg.substr(size_t(std::distance(start, b)));
            it->second(state{body, h, response, error}); // handle the body
         }
         else {
            write_json(std::forward_as_tuple(header{.error = true}, error_t{error_e::method_not_found}), response);
         }

         return !(h.action & notify);
      }
   };
}

/*template <>
struct glz::meta<glz::repe::error_e>
{
   using enum repe::error_e;
   static constexpr auto value = enumerate(no_error, server_error_lower, server_error_upper,
                                           invalid_request, method_not_found, invalid_params,
                                           internal, parse_error);
};*/
