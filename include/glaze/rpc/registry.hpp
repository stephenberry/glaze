// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/glaze.hpp"
#include "glaze/rpc/repe/repe.hpp"

namespace glz
{
   namespace detail
   {
      static constexpr std::string_view empty_path = "";
   }

   // Forward declaration of implementation template
   template <auto Opts, uint32_t Protocol>
   struct registry_impl;
}

// Include implementation files
#include "glaze/net/rest_registry_impl.hpp"
#include "glaze/rpc/jsonrpc_registry_impl.hpp"
#include "glaze/rpc/repe/repe_registry_impl.hpp"

namespace glz
{
   // This registry does not support adding methods from RPC calls or adding methods once RPC calls can be made.
   template <auto Opts = opts{}, uint32_t Proto = REPE>
   struct registry
   {
      // procedure for REPE protocol
      using procedure = std::function<void(repe::state&&)>; // RPC method

      static constexpr auto protocol = Proto;

      typename protocol_storage<Proto>::type endpoints{};

      void clear() { endpoints.clear(); }

     private:
      // Helper to register all members of a type without registering the root endpoint
      template <const std::string_view& root, class T, const std::string_view& parent>
         requires(glaze_object_t<T> || reflectable<T>)
      void register_members(T& value)
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

         for_each<N>([&]<auto I>() {
            decltype(auto) func = [&]() -> decltype(auto) {
               if constexpr (reflectable<T>) {
                  return get_member(value, get<I>(t));
               }
               else {
                  return get_member(value, get<I>(reflect<T>::values));
               }
            }();

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
            else if constexpr (std::is_pointer_v<std::remove_cvref_t<Func>> &&
                               (glaze_object_t<std::remove_pointer_t<std::remove_cvref_t<Func>>> ||
                                reflectable<std::remove_pointer_t<std::remove_cvref_t<Func>>>)) {
               // Handle pointer members explicitly for RPC traversal
               if (func) { // Only traverse if pointer is valid
                  on<root, std::remove_pointer_t<std::remove_cvref_t<Func>>, full_key>(*func);
                  impl::template register_object_endpoint<std::remove_pointer_t<std::remove_cvref_t<Func>>>(
                     full_key, *func, *this);
               }
               // else: skip registration for null pointers - no endpoints created
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
                        impl::template register_member_function_with_params_endpoint<T, F, Input, void>(full_key, value,
                                                                                                        func, *this);
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
                        impl::template register_member_function_with_params_endpoint<T, F, Input, Ret>(full_key, value,
                                                                                                       func, *this);
                     }
                     else {
                        static_assert(false_v<Func>, "function cannot have more than one input");
                     }
                  }
               }
               else {
                  // this is a variable and not a function, so we build RPC read/write calls
                  // We can't remove const here, because const fields need to be able to be written
                  impl::template register_variable_endpoint<std::remove_reference_t<Func>>(full_key, func, *this);
               }
            }
         });
      }

     public:
      // Register a C++ type that stores pointers to the value, so be sure to keep the registered value alive
      template <const std::string_view& root = detail::empty_path, class T, const std::string_view& parent = root>
         requires(glaze_object_t<T> || reflectable<T>)
      void on(T& value)
      {
         using impl = registry_impl<Opts, Proto>;

         if constexpr (parent == root && (glaze_object_t<T> || reflectable<T>)) {
            impl::register_endpoint(root, value, *this);
         }

         register_members<root, T, parent>(value);
      }

      // Register multiple C++ types merged together, allowing the root endpoint to return a combined view
      template <const std::string_view& root = detail::empty_path, class... Ts>
         requires(sizeof...(Ts) > 0 && (... && (glaze_object_t<Ts> || reflectable<Ts>)))
      void on(glz::merge<Ts...>& merged)
      {
         using impl = registry_impl<Opts, Proto>;

         // Register root endpoint with the merged object - this handles ""
         impl::register_merge_endpoint(root, merged, *this);

         // Register each merged object's member paths
         for_each<sizeof...(Ts)>([&]<size_t I>() {
            auto& obj = glz::get<I>(merged.value);
            using T = std::decay_t<decltype(obj)>;
            register_members<root, T, root>(obj);
         });
      }

      // Function to call methods - only available for REPE protocol
      template <class In = repe::message, class Out = repe::message>
         requires(Proto == REPE) // call method is only available for REPE protocol
      void call(In&& in, Out&& out)
      {
         auto write_error = [&](const std::string& body) {
            out.body = body;
            out.header.body_length = body.size();
            out.header.body_format = repe::body_format::UTF8; // Error messages are UTF-8
            out.header.length = sizeof(repe::header) + out.query.size() + out.body.size();
         };

         // REPE Header Validation

         // Version validation - REPE spec requires version 1
         if (in.header.version != 1) {
            out.header.ec = error_code::version_mismatch;
            out.header.id = in.header.id; // Echo back the original ID
            write_error("REPE version mismatch: expected 1, got " + std::to_string(in.header.version));
            return;
         }

         // Length validation - REPE spec requires length = 48 + query_length + body_length
         const uint64_t expected_length = sizeof(repe::header) + in.header.query_length + in.header.body_length;
         if (in.header.length != expected_length) {
            out.header.ec = error_code::invalid_header;
            out.header.id = in.header.id; // Echo back the original ID
            write_error("REPE length mismatch: expected " + std::to_string(expected_length) + ", got " +
                        std::to_string(in.header.length));
            return;
         }

         // Magic number validation - REPE spec requires 0x1507
         if (in.header.spec != 0x1507) {
            out.header.ec = error_code::invalid_header;
            out.header.id = in.header.id; // Echo back the original ID
            write_error("REPE magic number mismatch: expected 0x1507, got 0x" + std::to_string(in.header.spec));
            return;
         }

         if (auto it = endpoints.find(in.query); it != endpoints.end()) {
            if (bool(in.header.ec)) {
               out = in;
            }
            else {
               try {
                  it->second(repe::state{in, out}); // handle the body
               }
               catch (const std::exception& e) {
                  out = repe::message{}; // reset the output message because it could have been modified
                  out.header.id = in.header.id; // Preserve the ID from the input message
                  out.header.query_length = 0;
                  std::string body = "registry error for `" + in.query + "`: ";
                  body.append(e.what());
                  out.header.ec = error_code::parse_error;
                  write_error(body);
               }
            }
         }
         else {
            std::string body = "invalid_query: " + in.query;
            out.header.ec = error_code::method_not_found;
            out.header.id = in.header.id; // Preserve the ID from the input message
            write_error(body);
         }
      }

      // Function to call methods - only available for JSONRPC protocol
      // Returns a JSON RPC response string
      // Supports single requests, batch requests, and notifications
      std::string call(std::string_view json_request)
         requires(Proto == JSONRPC)
      {
         // Validate JSON first
         if (auto parse_err = glz::validate_json(json_request)) {
            return R"({"jsonrpc":"2.0","error":{"code":-32700,"message":"Parse error","data":")" +
                   format_error(parse_err, json_request) + R"("},"id":null})";
         }

         // Check if it's a batch request (array)
         auto batch_requests = glz::read_json<std::vector<glz::raw_json_view>>(json_request);
         if (batch_requests.has_value()) {
            if (batch_requests.value().empty()) {
               return R"({"jsonrpc":"2.0","error":{"code":-32600,"message":"Invalid Request","data":"Empty batch"},"id":null})";
            }
            return process_batch(batch_requests.value());
         }

         // Single request
         auto response = process_single_request(json_request);
         return response.value_or("");
      }

     private:
      // Process a single JSON RPC request, returns nullopt for notifications
      std::optional<std::string> process_single_request(std::string_view json_request)
         requires(Proto == JSONRPC)
      {
         auto request = glz::read_json<rpc::generic_request_t>(json_request);
         if (!request.has_value()) {
            // Try to extract the id even on parse failure
            auto id = glz::get_as_json<rpc::id_t, "/id">(json_request);
            std::string id_json = id.has_value() ? glz::write_json(id.value()).value_or("null") : "null";
            return R"({"jsonrpc":"2.0","error":{"code":-32600,"message":"Invalid Request","data":")" +
                   format_error(request.error(), json_request) + R"("},"id":)" + id_json + "}";
         }

         auto& req = request.value();

         // Validate version
         if (req.version != rpc::supported_version) {
            std::string id_json = glz::write_json(req.id).value_or("null");
            return R"({"jsonrpc":"2.0","error":{"code":-32600,"message":"Invalid Request","data":"Invalid version: )" +
                   std::string(req.version) + R"("},"id":)" + id_json + "}";
         }

         // Check if this is a notification (id is null)
         bool is_notification = std::holds_alternative<glz::generic::null_t>(req.id);

         // Convert method to path (add leading slash if not present)
         std::string method_path;
         if (req.method.empty() || req.method[0] != '/') {
            method_path = "/";
            method_path += req.method;
         }
         else {
            method_path = std::string(req.method);
         }

         // Look up the endpoint
         auto it = endpoints.find(method_path);
         if (it == endpoints.end()) {
            // Also try without the leading slash for "" endpoint
            if (method_path == "/") {
               it = endpoints.find("");
            }
            if (it == endpoints.end()) {
               if (is_notification) {
                  return std::nullopt; // No response for notifications
               }
               std::string id_json = glz::write_json(req.id).value_or("null");
               return R"({"jsonrpc":"2.0","error":{"code":-32601,"message":"Method not found","data":")" +
                      std::string(req.method) + R"("},"id":)" + id_json + "}";
            }
         }

         // Prepare state
         std::string response;
         bool has_params = !req.params.str.empty() && req.params.str != "null";
         jsonrpc::state state{req.id, response, is_notification, has_params, req.params.str};

         try {
            it->second(std::move(state));
         }
         catch (const std::exception& e) {
            if (is_notification) {
               return std::nullopt;
            }
            std::string id_json = glz::write_json(req.id).value_or("null");
            return R"({"jsonrpc":"2.0","error":{"code":-32603,"message":"Internal error","data":")" +
                   std::string(e.what()) + R"("},"id":)" + id_json + "}";
         }

         if (is_notification) {
            return std::nullopt;
         }

         return response;
      }

      // Process batch requests
      std::string process_batch(const std::vector<glz::raw_json_view>& batch)
         requires(Proto == JSONRPC)
      {
         std::vector<std::string> responses;
         responses.reserve(batch.size());

         for (const auto& req : batch) {
            auto response = process_single_request(req.str);
            if (response.has_value()) {
               responses.push_back(std::move(response.value()));
            }
         }

         // If all were notifications, return empty string
         if (responses.empty()) {
            return "";
         }

         // Build batch response array
         std::string result = "[";
         for (size_t i = 0; i < responses.size(); ++i) {
            if (i > 0) {
               result += ",";
            }
            result += responses[i];
         }
         result += "]";
         return result;
      }
   };
}
