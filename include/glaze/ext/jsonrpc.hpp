// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <glaze/glaze.hpp>
#include <glaze/tuplet/tuple.hpp>
#include <glaze/util/expected.hpp>
#include <unordered_map>
#include <utility>

namespace glz::rpc
{
   enum struct error_e : int {
      no_error = 0,
      server_error_lower = -32000,
      server_error_upper = -32099,
      invalid_request = -32600,
      method_not_found = -32601,
      invalid_params = -32602,
      internal = -32603,
      parse_error = -32700,
   };

   inline constexpr std::string_view code_as_sv(const error_e error_code) noexcept
   {
      switch (error_code) {
      case error_e::no_error:
         return "No error";
      case error_e::parse_error:
         return "Parse error";
      case error_e::server_error_lower:
      case error_e::server_error_upper:
         return "Server error";
      case error_e::invalid_request:
         return "Invalid request";
      case error_e::method_not_found:
         return "Method not found";
      case error_e::invalid_params:
         return "Invalid params";
      case error_e::internal:
         return "Internal error";
      }
      return "Unknown";
   }
}

// jsonrpc
namespace glz::rpc
{
   using id_t = std::variant<glz::json_t::null_t, std::string_view, std::int64_t>;
   static constexpr std::string_view supported_version{"2.0"};

   struct error final
   {
      error_e code{error_e::no_error};
      std::optional<std::string> data{}; // Optional detailed error information
      std::string message{code_as_sv(code)}; // string reflection of member variable code

      // TODO: remove all these constructors when MSVC is fixed
      error() = default;
      error(error_e code) : code(code) {}
      error(error_e code, const std::optional<std::string>& data) : code(code), data(data) {}
      error(error_e code, const std::optional<std::string>& data, const std::string& message)
         : code(code), data(data), message(message)
      {}
      error(const error&) = default;
      error(error&&) = default;
      error& operator=(const error&) = default;
      error& operator=(error&&) = default;

      static error invalid(const parse_error& pe, auto& buffer)
      {
         std::string format_err{format_error(pe, buffer)};
         return {error_e::invalid_request, format_err.empty() ? std::nullopt : std::optional{format_err},
                 std::string(code_as_sv(error_e::invalid_request))};
      }
      static error version(std::string_view presumed_version)
      {
         return {error_e::invalid_request,
                 "Invalid version: " + std::string(presumed_version) + " only supported version is " +
                    std::string(rpc::supported_version),
                 std::string(code_as_sv(error_e::invalid_request))};
      }
      static error method(std::string_view presumed_method)
      {
         return {error_e::method_not_found, "Method: '" + std::string(presumed_method) + "' not found",
                 std::string(code_as_sv(error_e::method_not_found))};
      }

      operator bool() const noexcept { return code != rpc::error_e::no_error; }

      bool operator==(const rpc::error_e err) const noexcept { return code == err; }

      struct glaze
      {
         using T = error;
         static constexpr auto value = glz::object("code", &T::code, "message", &T::message, "data", &T::data);
      };
   };

   template <class params_type>
   struct request_t
   {
      id_t id{};
      std::string_view method{};
      params_type params{};
      std::string_view version{rpc::supported_version};

      struct glaze
      {
         using T = request_t;
         static constexpr auto value = glz::object("jsonrpc", &T::version, //
                                                   "method", &T::method, //
                                                   "params", &T::params, //
                                                   "id", &T::id);
      };
   };

   template <class params_type>
   request_t(id_t&&, std::string_view, params_type&&) -> request_t<std::decay_t<params_type>>;

   using generic_request_t = request_t<glz::raw_json_view>;

   template <class result_type>
   struct response_t
   {
      using result_t = result_type;

      response_t() = default;
      explicit response_t(rpc::error&& err) : error(std::move(err)) {}
      response_t(id_t&& id, result_t&& result) : id(std::move(id)), result(std::move(result)) {}
      response_t(id_t&& id, rpc::error&& err) : id(std::move(id)), error(std::move(err)) {}

      id_t id{};
      std::optional<result_t> result{}; // todo can this be instead expected<result_t, error>
      std::optional<rpc::error> error{};
      std::string version{rpc::supported_version};

      struct glaze
      {
         using T = response_t;
         static constexpr auto value{
            glz::object("jsonrpc", &T::version, "result", &T::result, "error", &T::error, "id", &T::id)};
      };
   };
   using generic_response_t = response_t<glz::raw_json_view>;

   template <string_literal name, class params_type, class result_type>
   struct method
   {
      static constexpr std::string_view name_v{name};
      using params_t = params_type;
      using result_t = result_type;
   };

   namespace concepts
   {
      template <class T>
      concept method_type = requires(T) {
         T::name_v;
         {
            std::same_as<decltype(T::name_v), std::string_view>
         };
         typename T::params_t;
         typename T::result_t;
      };

      template <class call_return_t>
      concept call_return_type =
         requires { requires glz::is_any_of<call_return_t, std::string, std::vector<response_t<glz::raw_json>>>; };
   }

   template <concepts::method_type Method>
   struct server_method_t
   {
      static constexpr std::string_view name_v = Method::name_v;
      using params_t = typename Method::params_t;
      using result_t = typename Method::result_t;
      using request_t = rpc::request_t<params_t>;
      using response_t = rpc::response_t<result_t>;
      std::function<expected<result_t, rpc::error>(const params_t&)> callback{[](const auto&) {
         return glz::unexpected{rpc::error{rpc::error_e::internal, "Not implemented"}};
      }};
   };

   template <concepts::method_type Method>
   struct client_method_t
   {
      static constexpr std::string_view name_v = Method::name_v;
      using params_t = typename Method::params_t;
      using result_t = typename Method::result_t;
      using request_t = rpc::request_t<params_t>;
      using response_t = rpc::response_t<result_t>;
      using callback_t = std::function<void(const glz::expected<result_t, rpc::error>&, const id_t&)>;
      std::unordered_map<id_t, callback_t> pending_requests;
   };

   namespace detail
   {
      template <string_literal name, class... method_type>
      inline constexpr void set_callback(glz::tuplet::tuple<method_type...>& methods, const auto& callback)
      {
         constexpr bool method_found = ((method_type::name_v == name) || ...);
         static_assert(method_found, "Method not settable in given tuple.");

         methods.any([&callback](auto&& method) -> bool {
            if constexpr (name == std::remove_reference_t<decltype(method)>::name_v) {
               method.callback = callback;
               return true;
            }
            else {
               return false;
            }
         });
      }

      // Utility to get index of name_v inside method_t
      template <class T, T value, class... Ts>
      struct index_of_name;

      template <class T, T value, class... Ts>
      struct index_of_name<T, value, T, Ts...>
      {
         static constexpr std::size_t index = 0;
      };

      template <class T, T value, class U, class... Ts>
      struct index_of_name<T, value, U, Ts...>
      {
         static constexpr std::size_t index = (value == U::name_v) ? 0 : 1 + index_of_name<T, value, Ts...>::index;
      };

      template <class T, T value>
      struct index_of_name<T, value>
      {
         static constexpr std::size_t index = std::numeric_limits<size_t>::max();
      };

      template <class map_t, string_literal name, class... method_type>
      auto get_request_map(glz::tuplet::tuple<method_type...>& methods) -> map_t&
      {
         constexpr bool method_found = ((method_type::name_v == name) || ...);
         static_assert(method_found, "Method not declared in client.");

         map_t* return_ptr{nullptr};

         methods.any([&return_ptr](auto&& method) -> bool {
            using meth_t = std::remove_reference_t<decltype(method)>;
            if constexpr (name == meth_t::name_v) {
               return_ptr = &method.pending_requests;
               return true; // break methods loop
            }
            else {
               return false;
            }
         });

         assert(return_ptr != nullptr); // static_assert above should guarantee this

         return *return_ptr;
      }
   }

   template <concepts::method_type... method_type>
   struct server
   {
      using raw_response_t = response_t<glz::raw_json>;

      glz::tuplet::tuple<server_method_t<method_type>...> methods{};

      template <string_literal name>
      constexpr void on(const auto& callback) // std::function<expected<result_t, rpc::error>(params_t const&)>
      {
         detail::set_callback<name>(methods, callback);
      }

      using raw_call_return_t = std::vector<raw_response_t>;
      // Return json stringified std::vector<response_t> meaning it can both be error and non-error and batch request
      // response. If `id` is null in json_request a response will not be generated
      // If you would like to handle errors for each request:
      // auto response_vector = server.call<decltype(server_instance)::raw_call_return_t>("...");
      // std::string response = glz::write_json(response_vector);
      template <concepts::call_return_type return_t = std::string>
      return_t call(std::string_view json_request)
      {
         constexpr auto return_helper = []<typename input_type>(input_type&& response) -> auto {
            if constexpr (std::same_as<return_t, std::string>) {
               return glz::write_json(std::forward<input_type>(response));
            }
            else if constexpr (std::same_as<input_type, raw_response_t>) {
               return std::vector<raw_response_t>{std::forward<input_type>(response)};
            }
            else {
               return std::forward<input_type>(response);
            }
         };

         if (auto parse_err{glz::validate_json(json_request)}) {
            return return_helper(
               raw_response_t{rpc::error{error_e::parse_error, format_error(parse_err, json_request)}});
         }

         auto batch_requests{glz::read_json<std::vector<glz::raw_json_view>>(json_request)};
         if (batch_requests.has_value() && batch_requests.value().empty()) {
            return return_helper(raw_response_t{rpc::error{error_e::invalid_request}});
         }
         if (batch_requests.has_value()) {
            return return_helper(batch_request(batch_requests.value()));
         }

         // At this point we are with a single request
         auto response{per_request(json_request)};
         if (response.has_value()) {
            return return_helper(std::move(response.value()));
         }
         return {};
      }

     private:
      auto per_request(std::string_view json_request) -> std::optional<response_t<glz::raw_json>>
      {
         expected<generic_request_t, glz::parse_error> request{glz::read_json<generic_request_t>(json_request)};

         if (!request.has_value()) {
            // Failed, but let's try to extract the `id`
            auto id{glz::get_as_json<id_t, "/id">(json_request)};
            if (!id.has_value()) {
               return raw_response_t{rpc::error::invalid(request.error(), json_request)};
            }
            return raw_response_t{std::move(id.value()), rpc::error::invalid(request.error(), json_request)};
         }

         auto& req{request.value()};
         if (req.version != rpc::supported_version) {
            return raw_response_t{std::move(req.id), rpc::error::version(req.version)};
         }

         std::optional<response_t<glz::raw_json>> return_v{};
         bool method_found = methods.any([&json_request, &req, &return_v](auto&& method) -> bool {
            using meth_t = std::remove_reference_t<decltype(method)>;
            if (req.method != meth_t::name_v) {
               return false;
            }
            auto const& params_request{glz::read_json<typename meth_t::request_t>(json_request)};
            if (params_request.has_value()) {
               expected<typename meth_t::result_t, rpc::error> result{
                  std::invoke(method.callback, params_request->params)};
               if (result.has_value()) {
                  return_v = raw_response_t{std::move(req.id), glz::write_json(result)};
                  if (std::holds_alternative<glz::json_t::null_t>(req.id)) {
                     // rpc notification requires no response
                     return_v = std::nullopt;
                  }
               }
               else {
                  return_v = raw_response_t{std::move(req.id), std::move(result.error())};
               }
            }
            else {
               // TODO: Finish thought, glaze does not error on missing field:
               // https://github.com/stephenberry/glaze/issues/207
               return_v = raw_response_t{std::move(req.id), rpc::error::invalid(params_request.error(), json_request)};
            }
            return true;
         });
         if (!method_found) {
            return raw_response_t{std::move(req.id), rpc::error::method(req.method)};
         }
         return return_v;
      };

      auto batch_request(const std::vector<glz::raw_json_view>& batch_requests)
      {
         std::vector<response_t<glz::raw_json>> return_vec;
         return_vec.reserve(batch_requests.size());
         for (auto&& request : batch_requests) {
            auto response{per_request(request.str)};
            if (response.has_value()) {
               return_vec.emplace_back(std::move(response.value()));
            }
         }
         return return_vec;
      }
   };

   template <concepts::method_type... method_type>
   struct client
   {
      glz::tuplet::tuple<client_method_t<method_type>...> methods{};

      rpc::error call(std::string_view json_response)
      {
         expected<generic_response_t, glz::parse_error> response{glz::read_json<generic_response_t>(json_response)};

         if (!response.has_value()) {
            return rpc::error{rpc::error_e::parse_error, format_error(response.error(), json_response)};
         }

         auto& res{response.value()};

         rpc::error return_v{};
         bool id_found = methods.any([&json_response, &res, &return_v](auto&& method) -> bool {
            using meth_t = std::remove_reference_t<decltype(method)>;

            if (auto it{method.pending_requests.find(res.id)}; it != std::end(method.pending_requests)) {
               auto [id, callback] = *it;

               expected<typename meth_t::response_t, glz::parse_error> response{
                  glz::read_json<typename meth_t::response_t>(json_response)};

               if (!response.has_value()) {
                  return_v = rpc::error{rpc::error_e::parse_error, format_error(response.error(), json_response)};
               }
               else {
                  auto& res_typed{response.value()};
                  if (res_typed.result.has_value()) {
                     std::invoke(callback, res_typed.result.value(), res_typed.id);
                  }
                  else if (res_typed.error.has_value()) {
                     std::invoke(callback, unexpected(res_typed.error.value()), res_typed.id);
                  }
                  else {
                     return_v = rpc::error{rpc::error_e::parse_error, R"(Missing key "result" or "error" in response)"};
                  }
               }
               method.pending_requests.erase(it);
               return true;
            }
            return false;
         });

         if (!id_found) [[unlikely]] {
            if (!return_v) {
               if (std::holds_alternative<std::string_view>(res.id)) {
                  return_v = rpc::error{error_e::internal,
                                        "id: '" + std::string(std::get<std::string_view>(res.id)) + "' not found"};
               }
               else {
                  return_v = rpc::error{error_e::internal, "id: " + glz::write_json(res.id) + " not found"};
               }
            }
         }

         return return_v;
      }

      // callback should be of type std::function<void(glz::expected<result_t, rpc::error> const&, jsonrpc_id_type
      // const&)> where result_t is the result type declared for the given method name returns the request string and
      // whether the callback was inserted into the queue if the callback was not inserted into the queue it can mean
      // that the input was a notification or more serious, conflicting id, the provided id should be unique!
      template <string_literal method_name>
      [[nodiscard]] std::pair<std::string, bool> request(id_t&& id, auto&& params, auto&& callback)
      {
         constexpr bool method_found = ((method_type::name_v == method_name) || ...);
         static_assert(method_found, "Method not declared in client.");

         using params_type = std::remove_cvref_t<decltype(params)>;
         constexpr bool params_type_found = (std::is_same_v<typename method_type::params_t, params_type> || ...);
         static_assert(params_type_found, "Input parameters do not match constructed client parameter types.");

         constexpr bool method_params_match =
            ((method_type::name_v == method_name && std::is_same_v<typename method_type::params_t, params_type>) ||
             ...);
         static_assert(method_params_match, "Method name and given params type do not match.");

         rpc::request_t req{std::forward<decltype(id)>(id), method_name.sv(), std::forward<decltype(params)>(params)};

         if (std::holds_alternative<glz::json_t::null_t>(id)) {
            return {glz::write_json(std::move(req)), false};
         }

         // Some bookkeeping store the callback
         bool inserted{false};
         methods.any([&inserted, &req, cb = std::forward<decltype(callback)>(callback)](auto&& method) -> bool {
            using meth_t = std::remove_reference_t<decltype(method)>;
            if constexpr (method_name == meth_t::name_v) {
               [[maybe_unused]] decltype(method.pending_requests.begin()) unused{}; // iterator_t
               std::tie(unused, inserted) = method.pending_requests.emplace(std::make_pair(req.id, cb));
               return true; // break methods loop
            }
            else {
               return false;
            }
         });

         return {glz::write_json(std::move(req)), inserted};
      }

      template <string_literal method_name>
      [[nodiscard]] auto notify(auto&& params) -> std::string
      {
         auto placebo{[](auto&, auto&) {}};
         return request<method_name>(glz::json_t::null_t{}, params, std::move(placebo)).first;
      }

      template <string_literal method_name>
      [[nodiscard]] const auto& get_request_map() const
      {
         constexpr auto idx = detail::index_of_name<decltype(method_name), method_name, method_type...>::index;
         using method_element = std::tuple_element_t<idx, std::tuple<client_method_t<method_type>...>>;
         using request_map_t = decltype(method_element().pending_requests);
         return detail::get_request_map<request_map_t, method_name>(methods);
      }

      template <string_literal method_name>
      [[nodiscard]] auto& get_request_map()
      {
         constexpr auto idx = detail::index_of_name<decltype(method_name), method_name, method_type...>::index;
         using method_element = std::tuple_element_t<idx, std::tuple<client_method_t<method_type>...>>;
         using request_map_t = decltype(method_element().pending_requests);
         return detail::get_request_map<request_map_t, method_name>(methods);
      }
   };
} // namespace glz::rpc
