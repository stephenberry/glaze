// Glaze Library
// For the license information refer to glaze.hpp

#include "glaze/rpc/registry.hpp"
#include "ut/ut.hpp"

using namespace ut;

struct rest_noexceptions_api_t
{
   int value{};

   int get_value() const { return value; }
   void set_value(const int v) { value = v; }

   struct glaze
   {
      using T = rest_noexceptions_api_t;
      static constexpr auto value = glz::object("value", &T::value, "get_value", &T::get_value, "set_value", &T::set_value);
   };
};

suite rest_registry_noexceptions_tests = [] {
   "rest_registry_registers_and_dispatches_without_exceptions"_test = [] {
      glz::registry<glz::opts{}, glz::REST> registry{};
      rest_noexceptions_api_t api{};
      registry.on(api);

      auto [put_handler, put_params] = registry.endpoints.match(glz::http_method::PUT, "/value");
      expect(put_handler != nullptr);

      glz::request put_req{
         .method = glz::http_method::PUT, .target = "/value", .path = "/value", .params = put_params, .body = "7"};
      glz::response put_res{};
      put_handler(put_req, put_res);

      expect(api.value == 7);
      expect(put_res.status_code == 204);

      auto [get_handler, get_params] = registry.endpoints.match(glz::http_method::GET, "/value");
      expect(get_handler != nullptr);

      glz::request get_req{
         .method = glz::http_method::GET, .target = "/value", .path = "/value", .params = get_params};
      glz::response get_res{};
      get_handler(get_req, get_res);

      expect(get_res.response_body.find("7") != std::string::npos) << get_res.response_body;
   };

   "registry_try_on_reports_route_conflicts_without_exceptions"_test = [] {
      glz::registry<glz::opts{}, glz::REST> registry{};
      rest_noexceptions_api_t first{};
      rest_noexceptions_api_t second{};

      std::string error{};
      auto ok = registry.try_on(first, &error);
      expect(ok);
      expect(error.empty());

      ok = registry.try_on(second, &error);
      expect(!ok);
      expect(error.find("Route conflict") != std::string::npos) << error;
   };

   "route_registration_errors_are_observable_without_exceptions"_test = [] {
      glz::http_router router{};

      std::string first_error{};
      auto ok = router.try_route(glz::http_method::GET, "/dup", [](const glz::request&, glz::response&) {}, {},
                                 &first_error);
      expect(ok);
      expect(first_error.empty());

      std::string second_error{};
      ok = router.try_route(glz::http_method::GET, "/dup", [](const glz::request&, glz::response&) {}, {},
                            &second_error);
      expect(!ok);
      expect(!second_error.empty());
      expect(second_error.find("Route conflict") != std::string::npos) << second_error;
      expect(router.has_route_error());
      expect(router.route_error().find("Route conflict") != std::string_view::npos);
   };
};

int main() { return 0; }
