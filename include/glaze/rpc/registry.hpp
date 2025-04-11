// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/glaze.hpp"
#include "glaze/rest/rest.hpp"
#include "glaze/rpc/repe/repe.hpp"

namespace glz
{
   // Define the protocol enum to differentiate between REPE and REST
   enum struct protocol : uint32_t { REPE, REST };

   namespace detail
   {
      struct string_hash
      {
         using is_transparent = void;
         [[nodiscard]] size_t operator()(const char* txt) const { return std::hash<std::string_view>{}(txt); }
         [[nodiscard]] size_t operator()(std::string_view txt) const { return std::hash<std::string_view>{}(txt); }
         [[nodiscard]] size_t operator()(const std::string& txt) const { return std::hash<std::string>{}(txt); }
      };

      static constexpr std::string_view empty_path = "";
   }

   // Forward declaration of implementation template
   template <auto Opts, protocol P>
   struct registry_impl;
}

// Include implementation files
#include "glaze/rpc/repe/repe_registry_impl.hpp"
#include "glaze/rest/rest_registry_impl.hpp"

namespace glz
{
   // This registry does not support adding methods from RPC calls or adding methods once RPC calls can be made.
   template <auto Opts = opts{}, protocol Proto = protocol::REPE>
   struct registry
   {
      // procedure for REPE protocol
      using procedure = std::function<void(repe::state&&)>; // RPC method

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

      typename protocol_storage<Proto>::type endpoints{};

      void clear() { endpoints.clear(); }

      // Register a C++ type that stores pointers to the value, so be sure to keep the registered value alive
      template <const std::string_view& root = detail::empty_path, class T, const std::string_view& parent = root>
         requires(glaze_object_t<T> || reflectable<T>)
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
         
         using impl = registry_impl<Opts, Proto>;

         if constexpr (parent == root && (glaze_object_t<T> || reflectable<T>)) {
            impl::register_endpoint(root, value, *this);
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
               impl::template register_function_endpoint<Func, Result>(full_key, func, *this);
            }
            else if constexpr (is_invocable_concrete<std::remove_cvref_t<Func>>) {
               using Tuple = invocable_args_t<std::remove_cvref_t<Func>>;
               constexpr auto N = glz::tuple_size_v<Tuple>;
               static_assert(N == 1, "Only one input is allowed for your function");

               using Params = glz::tuple_element_t<0, Tuple>;

               impl::template register_param_function_endpoint<Func, Params>(full_key, func, *this);
            }
            else if constexpr (glaze_object_t<std::remove_cvref_t<Func>> || reflectable<std::remove_cvref_t<Func>>) {
               on<root, std::remove_cvref_t<Func>, full_key>(func);

               impl::template register_object_endpoint<std::remove_cvref_t<Func>>(full_key, func, *this);
            }
            else if constexpr (not std::is_lvalue_reference_v<Func>) {
               // For glz::custom, glz::manage, etc.
               impl::template register_value_endpoint<std::remove_cvref_t<Func>>(full_key, func, *this);
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
                        impl::template register_member_function_endpoint<T, F, void>(full_key, value, func, *this);
                     }
                     else if constexpr (n_args == 1) {
                        using Input = std::decay_t<glz::tuple_element_t<0, Tuple>>;
                        impl::template register_member_function_with_params_endpoint<T, F, Input, void>(full_key, value, func, *this);
                     }
                     else {
                        static_assert(false_v<Func>, "function cannot have more than one input");
                     }
                  }
                  else {
                     // Member function pointers
                     if constexpr (n_args == 0) {
                        impl::template register_member_function_endpoint<T, F, Ret>(full_key, value, func, *this);
                     }
                     else if constexpr (n_args == 1) {
                        using Input = std::decay_t<glz::tuple_element_t<0, Tuple>>;
                        impl::template register_member_function_with_params_endpoint<T, F, Input, Ret>(full_key, value, func, *this);
                     }
                     else {
                        static_assert(false_v<Func>, "function cannot have more than one input");
                     }
                  }
               }
               else {
                  // this is a variable and not a function, so we build RPC read/write calls
                  impl::template register_variable_endpoint<std::remove_cvref_t<Func>>(full_key, func, *this);
               }
            }
         });
      }

      // Function to call methods - only available for REPE protocol
      template <class In = repe::message, class Out = repe::message>
         requires (Proto == protocol::REPE) // call method is only available for REPE protocol
      void call(In&& in, Out&& out)
      {
         if (auto it = endpoints.find(in.query); it != endpoints.end()) {
            if (bool(in.header.ec)) {
               out = in;
            }
            else {
               it->second(repe::state{in, out}); // handle the body
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

      // REST-specific functionality

      // Create a router from this registry (only for REST protocol)
      http_router create_router() const
      {
         static_assert(Proto == protocol::REST, "create_router() is only available for REST protocol");

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
         static_assert(Proto == protocol::REST, "mount_to_router() is only available for REST protocol");

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
   };

   // Convenience alias for REST registry
   template <auto Opts = opts{}>
   using rest_registry = registry<Opts, protocol::REST>;
}
