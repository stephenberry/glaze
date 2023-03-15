#pragma once

#include <algorithm>
#include <deque>
#include <glaze/glaze.hpp>
#include <glaze/tuplet/tuple.hpp>
#include <glaze/util/expected.hpp>
#include <optional>
#include <string>
#include <string_view>
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
      template <typename CharType, unsigned N>
      struct [[nodiscard]] basic_fixed_string
      {
         using char_type = CharType;

         char_type data_[N + 1]{};

         constexpr basic_fixed_string() noexcept : data_{} {}

         template <typename other_char_type>
            requires std::same_as<other_char_type, char_type>
         constexpr basic_fixed_string(const other_char_type (&foo)[N + 1]) noexcept
         {
            std::copy_n(foo, N + 1, data_);
         }

         [[nodiscard]] constexpr std::basic_string_view<char_type> view() const noexcept { return {&data_[0], N}; }

         constexpr operator std::basic_string_view<char_type>() const noexcept { return {&data_[0], N}; }

         template <unsigned M>
         constexpr auto operator==(basic_fixed_string<char_type, M> const& r) const noexcept
         {
            return N == M && view() == r.view();
         }
      };

      template <typename char_type, unsigned N>
      basic_fixed_string(char_type const (&str)[N]) -> basic_fixed_string<char_type, N - 1>;
   }

   class error
   {
     public:
      static error invalid(parse_error const& pe, auto& buffer)
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
         auto const it{std::find_if(std::cbegin(error_e_iterable), std::cend(error_e_iterable),
                                    [this](error_e err_val) { return util::to_underlying(err_val) == this->code; })};
         if (it == std::cend(error_e_iterable)) {
            return std::nullopt;
         }
         return *it;
      }
      [[nodiscard]] auto get_raw_code() const noexcept -> int { return code; }
      [[nodiscard]] auto get_message() const noexcept -> std::string const& { return message; }
      [[nodiscard]] auto get_data() const noexcept -> glz::json_t const& { return data; }

      operator bool() const noexcept
      {
         auto const my_code{get_code()};
         return my_code.has_value() && my_code.value() != rpc::error_e::no_error;
      }

      bool operator==(const rpc::error_e err) const noexcept
      {
         auto const my_code{get_code()};
         return my_code.has_value() && my_code.value() == err;
      }

     private:
      std::underlying_type_t<error_e> code{util::to_underlying(error_e::no_error)};
      std::string message{code_as_string(error_e::no_error)};  // string reflection of member variable code
      glz::json_t data{};                                      // Optional detailed error information

      static auto code_as_string(error_e error_code) -> std::string
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

   template <typename params_type>
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

   template <typename result_type>
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
      std::optional<result_t> result{};  // todo can this be instead expected<result_t, error>
      std::optional<rpc::error> error{};
      struct glaze
      {
         using T = response_t;
         static constexpr auto value{
            glz::object("jsonrpc", &T::version, "result", &T::result, "error", &T::error, "id", &T::id)};
      };
   };
   using generic_response_t = response_t<glz::raw_json_view>;

   template <detail::basic_fixed_string name, typename params_type, typename result_type>
   struct server_method_t
   {
      static constexpr std::string_view name_v{name};
      using params_t = params_type;
      using result_t = result_type;
      using jsonrpc_id_t = jsonrpc_id_type;
      using request_t = rpc::request_t<params_t>;
      using response_t = rpc::response_t<result_t>;
      std::function<expected<result_t, rpc::error>(params_t const&)> callback{
         [](auto const&) { return glz::unexpected{rpc::error(rpc::error_e::internal, "Not implemented")}; }};
   };

   template <detail::basic_fixed_string name, typename params_type, typename result_type>
   struct client_method_t
   {
      static constexpr std::string_view name_v{name};
      using params_t = params_type;
      using result_t = result_type;
      using jsonrpc_id_t = jsonrpc_id_type;
      using request_t = rpc::request_t<params_t>;
      using response_t = rpc::response_t<result_t>;
      std::function<void(glz::expected<result_t, rpc::error> const&, jsonrpc_id_type const&)> callback{
         [](auto const&, auto const&) {}};
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
                               {
                                  meth.callback
                               };
                            };

      template <typename method_t>
      concept server_method_type =
         requires(method_t meth) {
            requires method_type<method_t>;
            requires std::same_as<
               decltype(meth.callback),
               std::function<expected<typename method_t::result_t, rpc::error>(typename method_t::params_t const&)>>;
         };

      template <typename method_t>
      concept client_method_type =
         requires(method_t meth) {
            requires method_type<method_t>;
            requires std::same_as<decltype(meth.callback),
                                  std::function<void(glz::expected<typename method_t::result_t, rpc::error> const&,
                                                     jsonrpc_id_type const&)>>;
         };

      template <typename call_return_t>
      concept call_return_type =
         requires { requires glz::is_any_of<call_return_t, std::string, std::vector<response_t<glz::raw_json>>>; };
   }

   namespace detail
   {
      template <detail::basic_fixed_string name, typename... method_type>
      inline constexpr void set_callback(glz::tuplet::tuple<method_type...>& methods, auto const& callback)
      {
         constexpr bool method_found = ((method_type::name_v == name) || ...);
         static_assert(method_found, "Method not settable in given tuple.");

         methods.any([&callback](auto&& method) -> bool {
            if constexpr (name == std::remove_reference_t<decltype(method)>::name_v) {
               method.callback = callback;
               return true;
            }
            return false;
         });
      }
   }

   template <concepts::server_method_type... method_type>
   struct server
   {
      using raw_response_t = response_t<glz::raw_json>;

      server() = default;
      glz::tuplet::tuple<method_type...> methods{};

      template <detail::basic_fixed_string name>
      constexpr void on(auto const& callback)  // std::function<expected<result_t, rpc::error>(params_t const&)>
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
         constexpr auto return_helper{
            []<typename input_type>(input_type &&
                                    response) -> auto{if constexpr (std::same_as<return_t, std::string>){
                                                 return glz::write_json(std::forward<input_type>(response));
      }
      else if constexpr (std::same_as<input_type, raw_response_t>)
      {
         return std::vector<raw_response_t>{std::forward<input_type>(response)};
      }
      else
      {
         return std::forward<input_type>(response);
      }
   }
};

if (auto parse_err{glz::validate_json(json_request)}) {
   return return_helper(raw_response_t{rpc::error(rpc::error_e::parse_error, format_error(parse_err, json_request))});
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
         expected<typename meth_t::result_t, rpc::error> result{std::invoke(method.callback, params_request->params)};
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

std::vector<raw_response_t> batch_request(std::vector<glz::raw_json_view> batch_requests)
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
}
;

template <concepts::client_method_type... method_type>
struct client
{
   // Create client with default queue size of 100
   client() = default;
   // Create client given the queue size to store request ids
   explicit client(std::size_t max_queue_size) : queue_size(max_queue_size) {}
   std::size_t const queue_size{100};
   glz::tuplet::tuple<method_type...> methods{};
   // <id, method_name>
   std::deque<std::pair<jsonrpc_id_type, std::string>> id_ring_buffer{};

   template <detail::basic_fixed_string name>
   constexpr void on(auto const& callback)  // std::function<void(glz::expected<result_t, rpc::error> const&,
                                            // jsonrpc_id_type const&)>
   {
      detail::set_callback<name>(methods, callback);
   }

   rpc::error call(std::string_view json_response)
   {
      expected<generic_response_t, glz::parse_error> response{glz::read_json<generic_response_t>(json_response)};

      if (!response.has_value()) {
         return rpc::error{rpc::error_e::parse_error, format_error(response.error(), json_response)};
      }

      auto& res{response.value()};

      auto id_it{std::find_if(std::begin(id_ring_buffer), std::end(id_ring_buffer),
                              [&res](auto&& pair) { return res.id == pair.first; })};

      if (id_it == std::end(id_ring_buffer)) {
         return rpc::error{rpc::error_e::internal, "ID not found in request buffer."};
      }

      jsonrpc_id_type id;
      std::string method_name;
      std::tie(id, method_name) = *id_it;

      id_ring_buffer.erase(id_it);

      rpc::error return_v;
      bool method_found = methods.any([&json_response, &method_name, &return_v](auto&& method) -> bool {
         using meth_t = std::remove_reference_t<decltype(method)>;
         if (method_name == meth_t::name_v) {
            expected<typename meth_t::response_t, glz::parse_error> response{
               glz::read_json<typename meth_t::response_t>(json_response)};

            if (!response.has_value()) {
               return_v = rpc::error{rpc::error_e::parse_error, format_error(response.error(), json_response)};
            }
            else {
               auto& res_typed{response.value()};
               if (res_typed.result.has_value()) {
                  std::invoke(method.callback, res_typed.result.value(), res_typed.id);
               }
               else if (res_typed.error.has_value()) {
                  std::invoke(method.callback, unexpected(res_typed.error.value()), res_typed.id);
               }
               else {
                  return_v = rpc::error{rpc::error_e::parse_error, R"(Missing key "result" or "error" in response)"};
               }
            }
            return true;
         }
         return false;
      });

      if (!method_found) [[unlikely]] {
         if (!return_v) {
            return_v = rpc::error::method(method_name);
         }
      }

      return return_v;
   }

   template <detail::basic_fixed_string method_name>
   [[nodiscard]] auto request(jsonrpc_id_type&& id, auto&& params) -> std::string
   {
      constexpr bool method_found = ((method_type::name_v == method_name) || ...);
      static_assert(method_found, "Method not declared in client.");

      using params_type = std::remove_cvref_t<decltype(params)>;
      constexpr bool params_type_found = (std::is_same_v<typename method_type::params_t, params_type> || ...);
      static_assert(params_type_found, "Input parameters do not match constructed client parameter types.");

      constexpr bool method_params_match =
         ((method_type::name_v == method_name && std::is_same_v<typename method_type::params_t, params_type>) || ...);
      static_assert(method_params_match, "Method name and given params type do not match.");

      id_ring_buffer.emplace_front(id, method_name);
      if (id_ring_buffer.size() > queue_size) {
         id_ring_buffer.pop_back();
      }

      rpc::request_t<params_type> req(std::forward<jsonrpc_id_type>(id), method_name.view(),
                                      std::forward<params_type>(params));
      return glz::write_json(std::move(req));
   }
};
}  // namespace glz::rpc
