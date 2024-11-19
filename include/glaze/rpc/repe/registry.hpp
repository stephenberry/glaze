// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/glaze.hpp"
#include "glaze/rpc/repe/header.hpp"

namespace glz::repe
{
   struct error_t final
   {
      error_code code = error_code::none;
      std::string message = "";

      operator bool() const noexcept { return bool(code); }
   };

   struct state final
   {
      repe::message& in;
      repe::message& out;
      error_t& error;

      bool notify() const { return in.header.notify(); }

      bool read() const { return in.header.read(); }

      bool write() const { return in.header.write(); }
   };

   template <class T>
   concept is_state = std::same_as<std::decay_t<T>, state>;

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

   template <opts Opts, class Value>
   void write_response(Value&& value, is_state auto&& state)
   {
      auto& in = state.in;
      auto& out = state.out;
      out.header.id = in.header.id;
      if (state.error) {
         out.header.error = true;
         out.header.query_length = out.query.size();
         out.header.body_length = out.body.size();
         out.header.length = sizeof(repe::header) + out.query.size() + out.body.size();
      }
      else {
         const auto ec = write<Opts>(std::forward<Value>(value), out.body);
         if (bool(ec)) [[unlikely]] {
            out.header.error = true;
         }
         out.header.query_length = out.query.size();
         out.header.body_length = out.body.size();
         out.header.length = sizeof(repe::header) + out.query.size() + out.body.size();
      }
   }

   template <opts Opts>
   void write_response(is_state auto&& state)
   {
      auto& in = state.in;
      auto& out = state.out;
      out.header.id = in.header.id;
      if (state.error) {
         out.header.error = true;
         out.header.query_length = out.query.size();
         out.header.body_length = out.body.size();
         out.header.length = sizeof(repe::header) + out.query.size() + out.body.size();
      }
      else {
         const auto ec = write<Opts>(nullptr, out.body);
         if (bool(ec)) [[unlikely]] {
            out.header.error = true;
         }
         out.query.clear();
         out.header.query_length = out.query.size();
         out.header.body_length = out.body.size();
         out.header.length = sizeof(repe::header) + out.query.size() + out.body.size();
      }
   }

   // returns 0 on error
   template <opts Opts, class Value>
   size_t read_params(Value&& value, auto&& state)
   {
      glz::context ctx{};
      auto [b, e] = read_iterators<Opts>(state.in.body);
      if (state.in.body.empty()) [[unlikely]] {
         ctx.error = error_code::no_read_input;
      }
      if (bool(ctx.error)) [[unlikely]] {
         return 0;
      }
      auto start = b;

      glz::detail::read<Opts.format>::template op<Opts>(std::forward<Value>(value), ctx, b, e);

      if (bool(ctx.error)) {
         state.out.header.error = true;
         error_ctx ec{ctx.error, ctx.custom_error_message, size_t(b - start), ctx.includer_error};
         write_response<Opts>(error_t{error_code::parse_error, format_error(ec, state.in.body)}, state);
         return 0;
      }

      return size_t(b - start);
   }

   namespace detail
   {
      template <opts Opts>
      struct request_impl
      {
         error_t operator()(message& msg, const user_header& h) const
         {
            msg.header = encode(h);
            msg.header.read(true); // because no value provided
            msg.query = std::string{h.query};
            std::ignore = glz::write<Opts>(nullptr, msg.body);
            msg.header.body_length = msg.body.size();
            msg.header.length = sizeof(repe::header) + msg.query.size() + msg.body.size();
            return {};
         }

         template <class Value>
         error_t operator()(message& msg, const user_header& h, Value&& value) const
         {
            msg.header = encode(h);
            msg.query = std::string{h.query};
            msg.header.write(true);
            std::ignore = glz::write<Opts>(std::forward<Value>(value), msg.body);
            msg.header.body_length = msg.body.size();
            msg.header.length = sizeof(repe::header) + msg.query.size() + msg.body.size();
            return {};
         }
      };
   }

   template <opts Opts>
   inline constexpr auto request = detail::request_impl<Opts>{};

   inline constexpr auto request_beve = request<opts{BEVE}>;
   inline constexpr auto request_json = request<opts{JSON}>;

   // DESIGN NOTE: It might appear that we are locking ourselves into a poor design choice by using a runtime
   // std::unordered_map. However, we can actually improve this in the future by allowing glz::meta specialization on
   // the server to list out the method names and allow us to build a compile time map prior to function registration.
   // This is made possible by constinit, and the same approach used for compile time maps and struct reflection.
   // This will then be an opt-in performance improvement where a bit more code must be written by the user to
   // list the method names.

   namespace detail
   {
      static constexpr std::string_view empty_path = "";
   }

   // This registry does not support adding methods from RPC calls or adding methods once RPC calls can be made.
   template <opts Opts = opts{}>
   struct registry
   {
      using procedure = std::function<void(state&&)>; // RPC method
      std::unordered_map<sv, procedure, detail::string_hash, std::equal_to<>> methods;

      void clear() { methods.clear(); }

      // Register a C++ type that stores pointers to the value, so be sure to keep the registered value alive
      template <const std::string_view& root = detail::empty_path, class T, const std::string_view& parent = root>
         requires(glz::detail::glaze_object_t<T> || glz::detail::reflectable<T>)
      void on(T& value)
      {
         using namespace glz::detail;
         static constexpr auto N = reflect<T>::size;

         [[maybe_unused]] decltype(auto) t = [&]() -> decltype(auto) {
            if constexpr (reflectable<T> && requires { to_tuple(value); }) {
               return to_tuple(value);
            }
            else {
               return nullptr;
            }
         }();

         if constexpr (parent == root && (glaze_object_t<T> ||
                                          reflectable<T>)&&!std::same_as<std::decay_t<decltype(t)>, std::nullptr_t>) {
            // build read/write calls to the top level object
            methods[root] = [&value](repe::state&& state) mutable {
               if (state.write()) {
                  if (read_params<Opts>(value, state) == 0) {
                     return;
                  }
               }

               if (state.notify()) {
                  return;
               }

               if (state.read()) {
                  write_response<Opts>(value, state);
               }
               else {
                  write_response<Opts>(state);
               }
            };
         }

         for_each<N>([&](auto I) {
            decltype(auto) func = [&]<size_t I>() -> decltype(auto) {
               if constexpr (reflectable<T>) {
                  return get_member(value, get<I>(t));
               }
               else {
                  return get_member(value, get<I>(reflect<T>::values));
               }
            }.template operator()<I>();

            static constexpr auto key = reflect<T>::keys[I];

            static constexpr std::string_view full_key = [&] {
               if constexpr (parent == detail::empty_path) {
                  return join_v<chars<"/">, key>;
               }
               else {
                  return join_v<parent, chars<"/">, key>;
               }
            }();

            // static_assert(std::same_as<decltype(func), refl_t<T, I>>);
            using E = std::remove_cvref_t<decltype(func)>;

            // This logic chain should match glz::cli_menu
            using Func = decltype(func);
            if constexpr (std::is_invocable_v<Func>) {
               using Result = std::decay_t<std::invoke_result_t<Func>>;
               if constexpr (std::same_as<Result, void>) {
                  methods[full_key] = [&func](repe::state&& state) mutable {
                     func();
                     if (state.notify()) {
                        state.out.header.notify(true);
                        return;
                     }
                     write_response<Opts>(state);
                  };
               }
               else {
                  methods[full_key] = [&func](repe::state&& state) mutable {
                     if (state.notify()) {
                        std::ignore = func();
                        state.out.header.notify(true);
                        return;
                     }
                     write_response<Opts>(func(), state);
                  };
               }
            }
            else if constexpr (is_invocable_concrete<std::remove_cvref_t<Func>>) {
               using Tuple = invocable_args_t<std::remove_cvref_t<Func>>;
               constexpr auto N = glz::tuple_size_v<Tuple>;
               static_assert(N == 1, "Only one input is allowed for your function");

               using Params = glz::tuple_element_t<0, Tuple>;

               methods[full_key] = [&func](repe::state&& state) mutable {
                  static thread_local std::decay_t<Params> params{};
                  // no need lock locals
                  if (read_params<Opts>(params, state) == 0) {
                     return;
                  }

                  using Result = std::invoke_result_t<decltype(func), Params>;

                  if (state.notify()) {
                     if constexpr (std::same_as<Result, void>) {
                        func(params);
                     }
                     else {
                        std::ignore = func(params);
                     }
                     state.out.header.notify(true);
                     return;
                  }
                  if constexpr (std::same_as<Result, void>) {
                     func(params);
                     write_response<Opts>(state);
                  }
                  else {
                     auto ret = func(params);
                     write_response<Opts>(ret, state);
                  }
               };
            }
            else if constexpr (glaze_object_t<E> || reflectable<E>) {
               on<root, E, full_key>(func);

               // build read/write calls to the object as a variable
               methods[full_key] = [&func](repe::state&& state) mutable {
                  if (state.write()) {
                     if (read_params<Opts>(func, state) == 0) {
                        return;
                     }
                  }

                  if (state.notify()) {
                     return;
                  }

                  if (state.read()) {
                     write_response<Opts>(func, state);
                  }
                  else {
                     write_response<Opts>(state);
                  }
               };
            }
            else if constexpr (not std::is_lvalue_reference_v<Func>) {
               // For glz::custom, glz::manage, etc.
               methods[full_key] = [func](repe::state&& state) mutable {
                  if (state.write()) {
                     if (read_params<Opts>(func, state) == 0) {
                        return;
                     }
                  }

                  if (state.notify()) {
                     state.out.header.notify(true);
                     return;
                  }

                  if (state.read()) {
                     write_response<Opts>(func, state);
                  }
                  else {
                     write_response<Opts>(state);
                  }
               };
            }
            else {
               static_assert(std::is_lvalue_reference_v<Func>);

               if constexpr (std::is_member_function_pointer_v<std::decay_t<Func>>) {
                  using F = std::decay_t<Func>;
                  using Ret = typename return_type<F>::type;
                  using Tuple = typename inputs_as_tuple<F>::type;
                  constexpr auto n_args = glz::tuple_size_v<Tuple>;
                  if constexpr (std::is_void_v<Ret>) {
                     if constexpr (n_args == 0) {
                        methods[full_key] = [&value, &func](repe::state&& state) mutable {
                           {
                              (value.*func)();
                           }

                           if (state.notify()) {
                              state.out.header.notify(true);
                              return;
                           }

                           write_response<Opts>(state);
                        };
                     }
                     else if constexpr (n_args == 1) {
                        using Input = std::decay_t<glz::tuple_element_t<0, Tuple>>;
                        methods[full_key] = [&value, &func](repe::state&& state) mutable {
                           static thread_local Input input{};
                           if (state.write()) {
                              if (read_params<Opts>(input, state) == 0) {
                                 return;
                              }
                           }

                           (value.*func)(input);

                           if (state.notify()) {
                              state.out.header.notify(true);
                              return;
                           }

                           write_response<Opts>(state);
                        };
                     }
                     else {
                        static_assert(false_v<Func>, "function cannot have more than one input");
                     }
                  }
                  else {
                     // Member function pointers
                     if constexpr (n_args == 0) {
                        methods[full_key] = [&value, &func](repe::state&& state) mutable {
                           if (state.notify()) {
                              std::ignore = (value.*func)();
                              return;
                           }

                           write_response<Opts>((value.*func)(), state);
                        };
                     }
                     else if constexpr (n_args == 1) {
                        using Input = std::decay_t<glz::tuple_element_t<0, Tuple>>;
                        methods[full_key] = [this, &value, &func](repe::state&& state) mutable {
                           static thread_local Input input{};

                           if (state.write()) {
                              if (read_params<Opts>(input, state) == 0) {
                                 return;
                              }
                           }

                           if (state.notify()) {
                              std::ignore = (value.*func)(input);
                              state.out.header.notify(true);
                              return;
                           }

                           write_response<Opts>((value.*func)(input), state);
                        };
                     }
                     else {
                        static_assert(false_v<Func>, "function cannot have more than one input");
                     }
                  }
               }
               else {
                  // this is a variable and not a function, so we build RPC read/write calls
                  methods[full_key] = [&func](repe::state&& state) mutable {
                     if (state.write()) {
                        if (read_params<Opts>(func, state) == 0) {
                           return;
                        }
                     }

                     if (state.notify()) {
                        state.out.header.notify(true);
                        return;
                     }

                     if (state.read()) {
                        write_response<Opts>(func, state);
                     }
                     else {
                        write_response<Opts>(state);
                     }
                  };
               }
            }
         });
      }

      void call(message& in, message& out)
      {
         if (auto it = methods.find(in.query); it != methods.end()) {
            static thread_local error_t error{};
            it->second(state{in, out, error}); // handle the body
         }
         else {
            static constexpr error_code code = error_code::method_not_found;
            static constexpr sv body{"method not found"};

            const auto body_length = 8 + body.size(); // 4 bytes for code, 4 bytes for size, + message

            out.body.resize(sizeof(header) + body_length);
            header h{.error = true, .body_length = body_length};
            std::memcpy(out.body.data(), &h, sizeof(header));
            std::memcpy(out.body.data() + sizeof(header), &code, 4);
            std::memcpy(out.body.data() + sizeof(header) + 4, &body_length, 4);
            std::memcpy(out.body.data() + sizeof(header) + 8, body.data(), body.size());
         }
      }
   };
}
