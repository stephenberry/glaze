// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/glaze.hpp"
#include "glaze/rest/rest.hpp"
#include "glaze/rpc/repe/header.hpp"

namespace glz
{
   // Define the protocol enum to differentiate between REPE and REST
   enum struct protocol : uint32_t { REPE, REST };
}

namespace glz::repe
{
   struct state final
   {
      repe::message& in;
      repe::message& out;

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

   template <auto Opts, class Value>
   void write_response(Value&& value, is_state auto&& state)
   {
      auto& in = state.in;
      auto& out = state.out;
      out.header.id = in.header.id;
      if (bool(out.header.ec)) {
         out.header.query_length = out.query.size();
         out.header.body_length = out.body.size();
         out.header.length = sizeof(repe::header) + out.query.size() + out.body.size();
      }
      else {
         const auto ec = write<Opts>(std::forward<Value>(value), out.body);
         if (bool(ec)) [[unlikely]] {
            out.header.ec = ec.ec;
         }
         out.header.query_length = out.query.size();
         out.header.body_length = out.body.size();
         out.header.length = sizeof(repe::header) + out.query.size() + out.body.size();
      }
   }

   template <auto Opts>
   void write_response(is_state auto&& state)
   {
      auto& in = state.in;
      auto& out = state.out;
      out.header.id = in.header.id;
      if (bool(out.header.ec)) {
         out.header.query_length = out.query.size();
         out.header.body_length = out.body.size();
         out.header.length = sizeof(repe::header) + out.query.size() + out.body.size();
      }
      else {
         const auto ec = write<Opts>(nullptr, out.body);
         if (bool(ec)) [[unlikely]] {
            out.header.ec = ec.ec;
         }
         out.query.clear();
         out.header.query_length = out.query.size();
         out.header.body_length = out.body.size();
         out.header.length = sizeof(repe::header) + out.query.size() + out.body.size();
      }
   }

   // returns 0 on error
   template <auto Opts, class Value>
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

      glz::parse<Opts.format>::template op<Opts>(std::forward<Value>(value), ctx, b, e);

      if (bool(ctx.error)) {
         state.out.header.ec = ctx.error;
         error_ctx ec{ctx.error, ctx.custom_error_message, size_t(b - start), ctx.includer_error};

         auto& in = state.in;
         auto& out = state.out;

         std::string error_message = format_error(ec, in.body);
         const uint32_t n = uint32_t(error_message.size());
         out.header.body_length = 4 + n;
         out.body.resize(out.header.body_length);
         std::memcpy(out.body.data(), &n, 4);
         std::memcpy(out.body.data() + 4, error_message.data(), n);

         write_response<Opts>(state);
         return 0;
      }

      return size_t(b - start);
   }

   namespace detail
   {
      template <auto Opts>
      struct request_impl
      {
         message operator()(const user_header& h) const
         {
            message msg{};
            msg.header = encode(h);
            msg.header.read(true); // because no value provided
            msg.query = std::string{h.query};
            msg.header.body_length = msg.body.size();
            msg.header.length = sizeof(repe::header) + msg.query.size() + msg.body.size();
            return msg;
         }

         template <class Value>
         message operator()(const user_header& h, Value&& value) const
         {
            message msg{};
            msg.header = encode(h);
            msg.query = std::string{h.query};
            msg.header.write(true);
            // TODO: Handle potential write errors and put in msg
            std::ignore = glz::write<Opts>(std::forward<Value>(value), msg.body);
            msg.header.body_length = msg.body.size();
            msg.header.length = sizeof(repe::header) + msg.query.size() + msg.body.size();
            return msg;
         }

         void operator()(const user_header& h, message& msg) const
         {
            msg.header = encode(h);
            msg.header.read(true); // because no value provided
            msg.query = std::string{h.query};
            msg.header.body_length = msg.body.size();
            msg.header.length = sizeof(repe::header) + msg.query.size() + msg.body.size();
         }

         template <class Value>
         void operator()(const user_header& h, message& msg, Value&& value) const
         {
            msg.header = encode(h);
            msg.query = std::string{h.query};
            msg.header.write(true);
            // TODO: Handle potential write errors and put in msg
            std::ignore = glz::write<Opts>(std::forward<Value>(value), msg.body);
            msg.header.body_length = msg.body.size();
            msg.header.length = sizeof(repe::header) + msg.query.size() + msg.body.size();
         }
      };
   }

   template <auto Opts>
   inline constexpr auto request = detail::request_impl<Opts>{};

   inline constexpr auto request_beve = request<opts{BEVE}>;
   inline constexpr auto request_json = request<opts{JSON}>;

   namespace detail
   {
      static constexpr std::string_view empty_path = "";
   }

   // This registry does not support adding methods from RPC calls or adding methods once RPC calls can be made.
   template <auto Opts = opts{}, protocol proto = protocol::REPE>
   struct registry
   {
      // Only define methods and procedure for REPE protocol
      using procedure = std::function<void(state&&)>; // RPC method

      // REST-specific structure
      struct rest_endpoint
      {
         http_method method;
         std::string path;
         handler handler;
      };

      // Single template storage for all protocol-specific storage
      template <protocol P>
      struct protocol_storage
      {};

      template <protocol P>
         requires(P == protocol::REPE)
      struct protocol_storage<P>
      {
         using type = std::unordered_map<sv, procedure, detail::string_hash, std::equal_to<>>;
      };

      template <protocol P>
         requires(P == protocol::REST)
      struct protocol_storage<P>
      {
         using type = std::vector<rest_endpoint>;
      };

      typename protocol_storage<proto>::type endpoints{};

      void clear() { endpoints.clear(); }

      // Register a C++ type that stores pointers to the value, so be sure to keep the registered value alive
      template <const std::string_view& root = detail::empty_path, class T, const std::string_view& parent = root>
         requires(glz::glaze_object_t<T> || glz::reflectable<T>)
      void on(T& value)
      {
         using namespace glz::detail;
         static constexpr auto N = reflect<T>::size;

         [[maybe_unused]] decltype(auto) t = [&]() -> decltype(auto) {
            if constexpr (reflectable<T> && requires { to_tie(value); }) {
               return to_tie(value);
            }
            else {
               return nullptr;
            }
         }();

         if constexpr (parent == root && (glaze_object_t<T> || reflectable<T>)) {
            register_endpoint<root>(value);
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

            // This logic chain should match glz::cli_menu
            using Func = decltype(func);
            if constexpr (std::is_invocable_v<Func>) {
               using Result = std::decay_t<std::invoke_result_t<Func>>;
               register_function_endpoint<full_key, Func, Result>(func);
            }
            else if constexpr (is_invocable_concrete<std::remove_cvref_t<Func>>) {
               using Tuple = invocable_args_t<std::remove_cvref_t<Func>>;
               constexpr auto N = glz::tuple_size_v<Tuple>;
               static_assert(N == 1, "Only one input is allowed for your function");

               using Params = glz::tuple_element_t<0, Tuple>;

               register_param_function_endpoint<full_key, Func, Params>(func);
            }
            else if constexpr (glaze_object_t<std::remove_cvref_t<Func>> || reflectable<std::remove_cvref_t<Func>>) {
               on<root, std::remove_cvref_t<Func>, full_key>(func);

               register_object_endpoint<full_key, std::remove_cvref_t<Func>>(func);
            }
            else if constexpr (not std::is_lvalue_reference_v<Func>) {
               // For glz::custom, glz::manage, etc.
               register_value_endpoint<full_key, std::remove_cvref_t<Func>>(func);
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
                        register_member_function_endpoint<full_key, T, F, void>(value, func);
                     }
                     else if constexpr (n_args == 1) {
                        using Input = std::decay_t<glz::tuple_element_t<0, Tuple>>;
                        register_member_function_with_params_endpoint<full_key, T, F, Input, void>(value, func);
                     }
                     else {
                        static_assert(false_v<Func>, "function cannot have more than one input");
                     }
                  }
                  else {
                     // Member function pointers
                     if constexpr (n_args == 0) {
                        register_member_function_endpoint<full_key, T, F, Ret>(value, func);
                     }
                     else if constexpr (n_args == 1) {
                        using Input = std::decay_t<glz::tuple_element_t<0, Tuple>>;
                        register_member_function_with_params_endpoint<full_key, T, F, Input, Ret>(value, func);
                     }
                     else {
                        static_assert(false_v<Func>, "function cannot have more than one input");
                     }
                  }
               }
               else {
                  // this is a variable and not a function, so we build RPC read/write calls
                  register_variable_endpoint<full_key, std::remove_cvref_t<Func>>(func);
               }
            }
         });
      }

      // Function to call methods - only available for REPE protocol
      template <class In = message, class Out = message>
      void call(In&& in, Out&& out)
      {
         if constexpr (proto == protocol::REPE) {
            if (auto it = endpoints.find(in.query); it != endpoints.end()) {
               if (bool(in.header.ec)) {
                  out = in;
               }
               else {
                  it->second(state{in, out}); // handle the body
               }
            }
            else {
               std::string body = "invalid_query: " + in.query;

               const uint32_t n = uint32_t(body.size());
               const auto body_length = 4 + n; // 4 bytes for size, + message

               out.body.resize(body_length);
               out.header.ec = error_code::method_not_found;
               out.header.body_length = body_length;
               std::memcpy(out.body.data(), &n, 4);
               std::memcpy(out.body.data() + 4, body.data(), n);
            }
         }
         else {
            static_assert(proto == protocol::REPE, "call method is only available for REPE protocol");
         }
      }

      // REST-specific functionality

      // Create a router from this registry (only for REST protocol)
      http_router create_router() const
      {
         static_assert(proto == protocol::REST, "create_router() is only available for REST protocol");

         http_router router;

         // Register all endpoints with the router
         for (const auto& endpoint : endpoints) {
            router.route(endpoint.method, endpoint.path, endpoint.handler);
         }

         return router;
      }

      // Mount this registry to an existing router (only for REST protocol)
      void mount_to_router(http_router& router, std::string_view base_path = "/") const
      {
         static_assert(proto == protocol::REST, "mount_to_router() is only available for REST protocol");

         // Register all endpoints with the router
         for (const auto& endpoint : endpoints) {
            std::string full_path = std::string(base_path);
            if (!full_path.empty() && full_path.back() == '/' && endpoint.path.front() == '/') {
               // Avoid double slash
               full_path.pop_back();
            }
            full_path += endpoint.path;

            router.route(endpoint.method, full_path, endpoint.handler);
         }
      }

     private:
      // Helper method to convert a JSON pointer path to a REST path
      // Only defined for REST protocol
      template <protocol P = proto>
      std::string convert_to_rest_path(sv json_pointer_path) const
      {
         if constexpr (P == protocol::REST) {
            // For many cases, JSON pointer and REST paths can be similar
            // This is a basic implementation - you might need to customize it

            // Make a copy of the path
            std::string rest_path(json_pointer_path);

            // Remove trailing slashes
            if (!rest_path.empty() && rest_path.back() == '/') {
               rest_path.pop_back();
            }

            return rest_path;
         }
         else {
            // This will cause a compile-time error if accessed with REPE protocol
            static_assert(P == protocol::REST, "convert_to_rest_path is only available for REST protocol");
            return std::string{}; // Dummy return to satisfy the compiler
         }
      }
      
      template <const std::string_view& path, class T>
         requires (proto == protocol::REPE)
      void register_endpoint(T& value)
      {
         endpoints[path] = [&value](repe::state&& state) mutable {
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

      // Helper for registering the root object
      template <const std::string_view& path, class T>
         requires (proto == protocol::REST)
      void register_endpoint(T& value)
      {
         std::string rest_path = convert_to_rest_path(path);
         
         // GET handler for the entire object
         endpoints.push_back(
                             {http_method::GET, rest_path, [&value](const Request& /*req*/, Response& res) { res.json(value); }});
         
         // PUT handler for updating the entire object
         endpoints.push_back({http_method::PUT, rest_path, [&value](const Request& req, Response& res) {
            // Parse the JSON request body
            auto ec = read_json(value, req.body);
            if (ec) {
               res.status(400).body("Invalid request body: " + format_error(ec, req.body));
               return;
            }
            
            res.status(204); // No Content
         }});
      }

      // Helper for registering function endpoints
      template <const std::string_view& path, class Func, class Result>
         requires (proto == protocol::REPE)
      void register_function_endpoint(Func& func)
      {
         if constexpr (std::same_as<Result, void>) {
            endpoints[path] = [&func](repe::state&& state) mutable {
               func();
               if (state.notify()) {
                  state.out.header.notify(true);
                  return;
               }
               write_response<Opts>(state);
            };
         }
         else {
            endpoints[path] = [&func](repe::state&& state) mutable {
               if (state.notify()) {
                  std::ignore = func();
                  state.out.header.notify(true);
                  return;
               }
               write_response<Opts>(func(), state);
            };
         }
      }
      
      template <const std::string_view& path, class Func, class Result>
         requires (proto == protocol::REST)
      void register_function_endpoint(Func& func)
      {
         std::string rest_path = convert_to_rest_path(path);
         
         // GET handler for functions
         endpoints.push_back({http_method::GET, rest_path, [&func](const Request& /*req*/, Response& res) {
            if constexpr (std::same_as<Result, void>) {
               func();
               res.status(204); // No Content
            }
            else {
               auto result = func();
               res.json(result);
            }
         }});
      }

      // Helper for registering function endpoints with parameters
      template <const std::string_view& path, class Func, class Params>
         requires (proto == protocol::REPE)
      void register_param_function_endpoint(Func& func)
      {
         endpoints[path] = [&func](repe::state&& state) mutable {
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
      
      template <const std::string_view& path, class Func, class Params>
         requires (proto == protocol::REST)
      void register_param_function_endpoint(Func& func)
      {
         std::string rest_path = convert_to_rest_path(path);
         
         // POST handler for functions with parameters
         endpoints.push_back({http_method::POST, rest_path, [&func](const Request& req, Response& res) {
            // Parse the JSON request body
            auto params_result = read_json<Params>(req.body);
            if (!params_result) {
               res.status(400).body("Invalid request body: " + format_error(params_result, req.body));
               return;
            }
            
            using Result = std::invoke_result_t<decltype(func), Params>;
            
            if constexpr (std::same_as<Result, void>) {
               func(params_result.value());
               res.status(204); // No Content
            }
            else {
               auto result = func(params_result.value());
               res.json(result);
            }
         }});
      }

      // Helper for registering nested object endpoints
      template <const std::string_view& path, class Obj>
         requires (proto == protocol::REPE)
      void register_object_endpoint(Obj& obj)
      {
         endpoints[path] = [&obj](repe::state&& state) mutable {
            if (state.write()) {
               if (read_params<Opts>(obj, state) == 0) {
                  return;
               }
            }
            
            if (state.notify()) {
               return;
            }
            
            if (state.read()) {
               write_response<Opts>(obj, state);
            }
            else {
               write_response<Opts>(state);
            }
         };
      }
      
      template <const std::string_view& path, class Obj>
         requires (proto == protocol::REST)
      void register_object_endpoint(Obj& obj)
      {
         std::string rest_path = convert_to_rest_path(path);
         
         // GET handler for nested objects
         endpoints.push_back(
                             {http_method::GET, rest_path, [&obj](const Request& /*req*/, Response& res) { res.json(obj); }});
         
         // PUT handler for updating nested objects
         endpoints.push_back({http_method::PUT, rest_path, [&obj](const Request& req, Response& res) {
            // Parse the JSON request body
            auto ec = read_json(obj, req.body);
            if (ec) {
               res.status(400).body("Invalid request body: " + format_error(ec, req.body));
               return;
            }
            
            // Update the object
            res.status(204); // No Content
         }});
      }

      // Helper for registering value endpoints
      template <const std::string_view& path, class Value>
         requires (proto == protocol::REPE)
      void register_value_endpoint(Value& value)
      {
         endpoints[path] = [value](repe::state&& state) mutable {
            if (state.write()) {
               if (read_params<Opts>(value, state) == 0) {
                  return;
               }
            }
            
            if (state.notify()) {
               state.out.header.notify(true);
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
      
      template <const std::string_view& path, class Value>
         requires (proto == protocol::REST)
      void register_value_endpoint(Value& value)
      {
         std::string rest_path = convert_to_rest_path(path);
         
         // GET handler for values
         endpoints.push_back(
                             {http_method::GET, rest_path, [&value](const Request& /*req*/, Response& res) { res.json(value); }});
         
         // PUT handler for updating values
         endpoints.push_back({http_method::PUT, rest_path, [&value](const Request& req, Response& res) {
            // Parse the JSON request body
            auto ec = read_json(value, req.body);
            if (!ec) {
               res.status(400).body("Invalid request body: " + format_error(ec, req.body));
               return;
            }
            
            res.status(204); // No Content
         }});
      }

      // Helper for registering variable endpoints
      template <const std::string_view& path, class Var>
         requires (proto == protocol::REPE)
      void register_variable_endpoint(Var& var)
      {
         endpoints[path] = [&var](repe::state&& state) mutable {
            if (state.write()) {
               if (read_params<Opts>(var, state) == 0) {
                  return;
               }
            }
            
            if (state.notify()) {
               state.out.header.notify(true);
               return;
            }
            
            if (state.read()) {
               write_response<Opts>(var, state);
            }
            else {
               write_response<Opts>(state);
            }
         };
      }
      
      template <const std::string_view& path, class Var>
         requires (proto == protocol::REST)
      void register_variable_endpoint(Var& var)
      {
         std::string rest_path = convert_to_rest_path(path);
         
         // GET handler for variables
         endpoints.push_back(
                             {http_method::GET, rest_path, [&var](const Request& /*req*/, Response& res) { res.json(var); }});
         
         // PUT handler for updating variables
         endpoints.push_back({http_method::PUT, rest_path, [&var](const Request& req, Response& res) {
            // Parse the JSON request body
            auto ec = read_json(var, req.body);
            if (!ec) {
               res.status(400).body("Invalid request body: " + format_error(ec, req.body));
               return;
            }
            
            res.status(204); // No Content
         }});
      }

      // Helper for registering member function endpoints
      template <const std::string_view& path, class T, class F, class Ret>
         requires (proto == protocol::REPE)
      void register_member_function_endpoint(T& value, F func)
      {
         endpoints[path] = [&value, func](repe::state&& state) mutable {
            if constexpr (std::same_as<Ret, void>) {
               (value.*func)();
               
               if (state.notify()) {
                  state.out.header.notify(true);
                  return;
               }
               
               write_response<Opts>(state);
            }
            else {
               if (state.notify()) {
                  std::ignore = (value.*func)();
                  return;
               }
               
               write_response<Opts>((value.*func)(), state);
            }
         };
      }
      
      template <const std::string_view& path, class T, class F, class Ret>
         requires (proto == protocol::REST)
      void register_member_function_endpoint(T& value, F func)
      {
         std::string rest_path = convert_to_rest_path(path);
         
         // GET handler for member functions with no args
         endpoints.push_back({http_method::GET, rest_path, [&value, func](const Request& /*req*/, Response& res) {
            if constexpr (std::same_as<Ret, void>) {
               (value.*func)();
               res.status(204); // No Content
            }
            else {
               auto result = (value.*func)();
               res.json(result);
            }
         }});
      }

      // Helper for registering member function endpoints with parameters
      template <const std::string_view& path, class T, class F, class Input, class Ret>
         requires (proto == protocol::REPE)
      void register_member_function_with_params_endpoint(T& value, F func)
      {
         endpoints[path] = [&value, func](repe::state&& state) mutable {
            static thread_local Input input{};
            if (state.write()) {
               if (read_params<Opts>(input, state) == 0) {
                  return;
               }
            }
            
            if constexpr (std::same_as<Ret, void>) {
               (value.*func)(input);
               
               if (state.notify()) {
                  state.out.header.notify(true);
                  return;
               }
               
               write_response<Opts>(state);
            }
            else {
               if (state.notify()) {
                  std::ignore = (value.*func)(input);
                  state.out.header.notify(true);
                  return;
               }
               
               write_response<Opts>((value.*func)(input), state);
            }
         };
      }
      
      template <const std::string_view& path, class T, class F, class Input, class Ret>
         requires (proto == protocol::REST)
      void register_member_function_with_params_endpoint(T& value, F func)
      {
         std::string rest_path = convert_to_rest_path(path);
         
         // POST handler for member functions with args
         endpoints.push_back({http_method::POST, rest_path, [&value, func](const Request& req, Response& res) {
            // Parse the JSON request body
            auto params_result = read_json<Input>(req.body);
            if (!params_result) {
               res.status(400).body("Invalid request body: " + format_error(params_result, req.body));
               return;
            }
            
            if constexpr (std::same_as<Ret, void>) {
               (value.*func)(params_result.value());
               res.status(204); // No Content
            }
            else {
               auto result = (value.*func)(params_result.value());
               res.json(result);
            }
         }});
      }
   };
}

namespace glz
{
   // Convenience alias for REST registry
   template <auto Opts = glz::opts{}>
   using rest_registry = glz::repe::registry<Opts, glz::protocol::REST>;
}
