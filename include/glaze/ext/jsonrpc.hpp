#pragma once

#include <algorithm>
#include <glaze/glaze.hpp>
#include <glaze/tuplet/tuple.hpp>
#include <glaze/util/expected.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#if __cpp_lib_to_underlying
namespace util
{
   using std::to_underlying;
}
#else
namespace util
{
   template <typename T>
   [[nodiscard]] constexpr std::underlying_type_t<T> to_underlying(T value) noexcept
   {
      return static_cast<std::underlying_type_t<T>>(value);
   }
}
#endif

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
   static constexpr std::array<error_e, 8> error_e_iterable{
      error_e::no_error,         error_e::server_error_lower, error_e::server_error_upper, error_e::invalid_request,
      error_e::method_not_found, error_e::invalid_params,     error_e::internal,           error_e::parse_error,
   };
}

template <>
struct glz::meta<glz::rpc::error_e>
{
   // clang-format off
   static constexpr auto value{[](auto&& enum_value) -> auto { return util::to_underlying(enum_value); }};
};
// clang-format on

// jsonrpc
namespace glz::rpc
{
   using jsonrpc_id_type = std::variant<glz::json_t::null_t, std::string, std::int64_t>;
   static constexpr std::string_view supported_version{"2.0"};

   namespace detail
   {
      inline std::string id_to_string(const jsonrpc_id_type& id)
      {
         return std::visit(
            overload{[](const json_t::null_t&) -> std::string { return "null"; },
                     [](const std::string& x) { return x; }, [](const std::int64_t& x) { return std::to_string(x); },
                     [](auto&&) -> std::string { return "unknown"; }},
            id);
      }

      template <class CharType, unsigned N>
      struct [[nodiscard]] basic_fixed_string
      {
         using char_type = CharType;

         char_type data_[N + 1]{};

         constexpr basic_fixed_string() noexcept : data_{} {}

         template <class other_char_type>
            requires std::same_as<other_char_type, char_type>
         constexpr basic_fixed_string(const other_char_type (&foo)[N + 1]) noexcept
         {
            std::copy_n(foo, N + 1, data_);
         }

         [[nodiscard]] constexpr std::basic_string_view<char_type> view() const noexcept { return {&data_[0], N}; }

         constexpr operator std::basic_string_view<char_type>() const noexcept { return {&data_[0], N}; }

         template <unsigned M>
         constexpr auto operator==(const basic_fixed_string<char_type, M>& r) const noexcept
         {
            return N == M && view() == r.view();
         }
      };

      template <class char_type, unsigned N>
      basic_fixed_string(char_type const (&str)[N]) -> basic_fixed_string<char_type, N - 1>;
   }

   struct error
   {
      static error invalid(const parse_error& pe, auto& buffer)
      {
         std::string format_err{format_error(pe, buffer)};
         return error(rpc::error_e::invalid_request, format_err.empty() ? glz::json_t{} : format_err);
      }
      static error version(std::string_view presumed_version)
      {
         return error(error_e::invalid_request, "Invalid version: " + std::string(presumed_version) +
                                                   " only supported version is " + std::string(rpc::supported_version));
      }
      static error method(std::string_view presumed_method)
      {
         return error(error_e::method_not_found, "Method: \"" + std::string(presumed_method) + "\" not found");
      }

      error() = default;

      explicit error(error_e code, glz::json_t&& data = {})
         : code(util::to_underlying(code)), message(code_as_string(code)), data(std::move(data))
      {}

      [[nodiscard]] auto get_code() const noexcept -> std::optional<error_e>
      {
         const auto it{std::find_if(std::cbegin(error_e_iterable), std::cend(error_e_iterable),
                                    [this](error_e err_val) { return util::to_underlying(err_val) == this->code; })};
         if (it == std::cend(error_e_iterable)) {
            return std::nullopt;
         }
         return *it;
      }
      [[nodiscard]] auto get_raw_code() const noexcept -> int { return code; }
      [[nodiscard]] auto get_message() const noexcept -> const std::string& { return message; }
      [[nodiscard]] auto get_data() const noexcept -> const glz::json_t& { return data; }

      operator bool() const noexcept
      {
         const auto my_code{get_code()};
         return my_code.has_value() && my_code.value() != rpc::error_e::no_error;
      }

      bool operator==(const rpc::error_e err) const noexcept
      {
         const auto my_code{get_code()};
         return my_code.has_value() && my_code.value() == err;
      }

     private:
      std::underlying_type_t<error_e> code{util::to_underlying(error_e::no_error)};
      std::string message{code_as_string(error_e::no_error)}; // string reflection of member variable code
      glz::json_t data{}; // Optional detailed error information

      static std::string_view code_as_string(error_e error_code)
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

     public:
      struct glaze
      {
         using T = error;
         static constexpr auto value{glz::object("code", &T::code, "message", &T::message, "data", &T::data)};
      };
   };

   template <class params_type>
   struct request_t
   {
      using params_t = params_type;
      using jsonrpc_id_t = jsonrpc_id_type;

      request_t() = default;
      request_t(jsonrpc_id_t&& id, std::string_view&& method, params_t&& params)
         : id(std::move(id)), version(rpc::supported_version), method(method), params(std::move(params))
      {}

      jsonrpc_id_t id{};
      std::string version{};
      std::string method{};
      params_t params{};

      struct glaze
      {
         using T = request_t;
         static constexpr auto value{
            glz::object("jsonrpc", &T::version, "method", &T::method, "params", &T::params, "id", &T::id)};
      };
   };
   using generic_request_t = request_t<glz::raw_json_view>;

   template <class result_type>
   struct response_t
   {
      using result_t = result_type;
      using jsonrpc_id_t = jsonrpc_id_type;

      response_t() = default;
      explicit response_t(rpc::error&& err) : version(rpc::supported_version), error(std::move(err)) {}
      response_t(jsonrpc_id_t&& id, result_t&& result)
         : id(std::move(id)), version(rpc::supported_version), result(std::move(result))
      {}
      response_t(jsonrpc_id_t&& id, rpc::error&& err)
         : id(std::move(id)), version(rpc::supported_version), error(std::move(err))
      {}

      jsonrpc_id_t id{};
      std::string version{};
      std::optional<result_t> result{}; // todo can this be instead expected<result_t, error>
      std::optional<rpc::error> error{};
      struct glaze
      {
         using T = response_t;
         static constexpr auto value{
            glz::object("jsonrpc", &T::version, "result", &T::result, "error", &T::error, "id", &T::id)};
      };
   };
   using generic_response_t = response_t<glz::raw_json_view>;

   template <detail::basic_fixed_string name, class params_type, class result_type>
   struct server_method_t
   {
      static constexpr std::string_view name_v{name};
      using params_t = params_type;
      using result_t = result_type;
      using jsonrpc_id_t = jsonrpc_id_type;
      using request_t = rpc::request_t<params_t>;
      using response_t = rpc::response_t<result_t>;
      std::function<expected<result_t, rpc::error>(const params_t&)> callback{
         [](const auto&) { return glz::unexpected{rpc::error(rpc::error_e::internal, "Not implemented")}; }};
   };

   template <detail::basic_fixed_string name, class params_type, class result_type>
   struct client_method_t
   {
      static constexpr std::string_view name_v{name};
      using params_t = params_type;
      using result_t = result_type;
      using jsonrpc_id_t = jsonrpc_id_type;
      using request_t = rpc::request_t<params_t>;
      using response_t = rpc::response_t<result_t>;
      using callback_t = std::function<void(const glz::expected<result_t, rpc::error>&, const jsonrpc_id_type&)>;
      std::unordered_map<jsonrpc_id_type, callback_t> pending_requests;
   };
   namespace concepts
   {
      template <typename method_t>
      concept method_type = requires(method_t meth) {
                               method_t::name_v;
                               {
                                  std::same_as<decltype(method_t::name_v), std::string_view>
                               };
                               typename method_t::params_t;
                               typename method_t::jsonrpc_id_t;
                               typename method_t::request_t;
                               typename method_t::response_t;
                            };

      template <class method_t>
      concept server_method_type =
         requires(method_t meth) {
            requires method_type<method_t>;
            {
               meth.callback
            };
            requires std::same_as<
               decltype(meth.callback),
               std::function<expected<typename method_t::result_t, rpc::error>(typename method_t::params_t const&)>>;
         };

      template <class method_t>
      concept client_method_type =
         requires(method_t meth) {
            requires method_type<method_t>;
            {
               meth.pending_requests
            };
            requires std::same_as<
               decltype(meth.pending_requests),
               std::unordered_map<jsonrpc_id_type,
                                  std::function<void(glz::expected<typename method_t::result_t, rpc::error> const&,
                                                     jsonrpc_id_type const&)>>>;
         };

      template <class call_return_t>
      concept call_return_type =
         requires { requires glz::is_any_of<call_return_t, std::string, std::vector<response_t<glz::raw_json>>>; };
   }

   namespace detail
   {
      template <detail::basic_fixed_string name, class... method_type>
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

      template <class map_t, detail::basic_fixed_string name, class... method_type>
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
            return false;
         });

         assert(return_ptr != nullptr); // static_assert above should guarantee this

         return *return_ptr;
      }
   }

   template <concepts::server_method_type... method_type>
   struct server
   {
      using raw_response_t = response_t<glz::raw_json>;

      server() = default;
      glz::tuplet::tuple<method_type...> methods{};

      template <detail::basic_fixed_string name>
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
         constexpr auto return_helper = []<typename input_type>(input_type && response) -> auto
         {
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
               raw_response_t{rpc::error(rpc::error_e::parse_error, format_error(parse_err, json_request))});
         }

         auto batch_requests{glz::read_json<std::vector<glz::raw_json_view>>(json_request)};
         if (batch_requests.has_value() && batch_requests.value().empty()) {
            return return_helper(raw_response_t{rpc::error(rpc::error_e::invalid_request)});
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
            auto id{glz::get_as_json<jsonrpc_id_type, "/id">(json_request)};
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

   template <concepts::client_method_type... method_type>
   struct client
   {
      // Create client with default queue size of 100
      client() = default;
      // Create client given the queue size to store request ids
      explicit client(std::size_t max_queue_size) : queue_size(max_queue_size) {}
      std::size_t const queue_size{100};
      glz::tuplet::tuple<method_type...> methods{};

      rpc::error call(std::string_view json_response)
      {
         expected<generic_response_t, glz::parse_error> response{glz::read_json<generic_response_t>(json_response)};

         if (!response.has_value()) {
            return rpc::error{rpc::error_e::parse_error, format_error(response.error(), json_response)};
         }

         auto& res{response.value()};

         rpc::error return_v;
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
               return_v = rpc::error(rpc::error_e::internal, "id: " + detail::id_to_string(res.id) + " not found");
            }
         }

         return return_v;
      }

      // callback should be of type std::function<void(glz::expected<result_t, rpc::error> const&, jsonrpc_id_type
      // const&)> where result_t is the result type declared for the given method name returns the request string and
      // whether the callback was inserted into the queue if the callback was not inserted into the queue it can mean
      // that the input was a notification or more serious, conflicting id, the provided id should be unique!
      template <detail::basic_fixed_string method_name>
      [[nodiscard]] auto request(jsonrpc_id_type&& id, auto&& params, auto&& callback) -> std::pair<std::string, bool>
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

         rpc::request_t<params_type> req(std::forward<jsonrpc_id_type>(id), method_name.view(),
                                         std::forward<params_type>(params));

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

      template <detail::basic_fixed_string method_name>
      [[nodiscard]] auto notify(auto&& params) -> std::string
      {
         auto placebo{[](auto&, auto&) {}};
         return request<method_name>(glz::json_t::null_t{}, params, std::move(placebo)).first;
      }

      template <detail::basic_fixed_string method_name>
      [[nodiscard]] const auto& get_request_map() const
      {
         constexpr auto idx = detail::index_of_name<decltype(method_name), method_name, method_type...>::index;
         using method_element = std::tuple_element_t<idx, std::tuple<method_type...>>;
         using request_map_t = decltype(method_element().pending_requests);
         return detail::get_request_map<request_map_t, method_name>(methods);
      }

      template <detail::basic_fixed_string method_name>
      [[nodiscard]] auto& get_request_map()
      {
         constexpr auto idx = detail::index_of_name<decltype(method_name), method_name, method_type...>::index;
         using method_element = std::tuple_element_t<idx, std::tuple<method_type...>>;
         using request_map_t = decltype(method_element().pending_requests);
         return detail::get_request_map<request_map_t, method_name>(methods);
      }
   };
} // namespace glz::rpc
