// Glaze Library
// For the license information refer to glaze.hpp

#include "glaze/net/cors.hpp"

#include "glaze/net/http_headers.hpp"
#include "ut/ut.hpp"

using namespace ut;

namespace
{
   glz::response run_cors(const glz::cors_config& config, std::string_view origin)
   {
      auto middleware = glz::create_cors_middleware(config);
      glz::request req{};
      req.method = glz::http_method::GET;
      req.headers.set("origin", std::string(origin));
      glz::response res{};
      middleware(req, res);
      return res;
   }
}

suite cors_origin_tests = [] {
   "wildcard_without_credentials_allows_any"_test = [] {
      glz::cors_config config;
      config.allowed_origins = {"*"};
      config.allow_credentials = false;
      expect(glz::is_origin_allowed(config, "https://evil.example"));

      auto res = run_cors(config, "https://evil.example");
      expect(res.response_headers.first_value("access-control-allow-origin") == "*");
      expect(!res.response_headers.contains("access-control-allow-credentials"));
   };

   "wildcard_with_credentials_rejects_unlisted_origin"_test = [] {
      glz::cors_config config;
      config.allowed_origins = {"*"};
      config.allow_credentials = true;
      expect(not glz::is_origin_allowed(config, "https://evil.example"));

      auto res = run_cors(config, "https://evil.example");
      expect(!res.response_headers.contains("access-control-allow-origin"));
      expect(!res.response_headers.contains("access-control-allow-credentials"));
   };

   "credentials_allow_exact_listed_origin"_test = [] {
      glz::cors_config config;
      config.allowed_origins = {"https://app.example", "*"};
      config.allow_credentials = true;
      expect(glz::is_origin_allowed(config, "https://app.example"));
      expect(not glz::is_origin_allowed(config, "https://evil.example"));

      auto res = run_cors(config, "https://app.example");
      expect(res.response_headers.first_value("access-control-allow-origin") == "https://app.example");
      expect(res.response_headers.first_value("access-control-allow-credentials") == "true");
   };

   "empty_origins_with_credentials_rejects"_test = [] {
      glz::cors_config config;
      config.allowed_origins = {};
      config.allow_credentials = true;
      expect(not glz::is_origin_allowed(config, "https://evil.example"));
   };

   "validator_still_grants_with_credentials"_test = [] {
      glz::cors_config config;
      config.allowed_origins = {};
      config.allow_credentials = true;
      config.allowed_origins_validator = [](std::string_view o) { return o == "https://trusted.example"; };
      expect(glz::is_origin_allowed(config, "https://trusted.example"));
      expect(not glz::is_origin_allowed(config, "https://evil.example"));
   };
};

namespace
{
   glz::response run_preflight(const glz::cors_config& config, std::string_view origin)
   {
      auto middleware = glz::create_cors_middleware(config);
      glz::request req{};
      req.method = glz::http_method::OPTIONS;
      req.headers.set("origin", std::string(origin));
      req.headers.set("access-control-request-method", "POST");
      glz::response res{};
      middleware(req, res);
      return res;
   }
}

suite cors_preflight_tests = [] {
   // The rejection text used to be written before the origin was checked and only the
   // status was replaced afterwards, so an allowed preflight answered 204 with a body.
   "allowed_preflight_has_no_body"_test = [] {
      glz::cors_config config;
      config.allowed_origins = {"https://app.example"};

      auto res = run_preflight(config, "https://app.example");
      expect(res.status_code == 204);
      expect(res.response_body.empty()) << "got: " << res.response_body;
      expect(res.response_headers.first_value("access-control-allow-origin") == "https://app.example");
      expect(res.response_headers.contains("access-control-allow-methods"));
   };

   "allowed_preflight_with_custom_status_has_no_body"_test = [] {
      glz::cors_config config;
      config.allowed_origins = {"https://app.example"};
      config.options_success_status = 200;

      auto res = run_preflight(config, "https://app.example");
      expect(res.status_code == 200);
      expect(res.response_body.empty()) << "got: " << res.response_body;
   };

   "rejected_preflight_keeps_403_and_message"_test = [] {
      glz::cors_config config;
      config.allowed_origins = {"https://app.example"};

      auto res = run_preflight(config, "https://evil.example");
      expect(res.status_code == 403);
      expect(res.response_body == "CORS: Origin not allowed");
      expect(!res.response_headers.contains("access-control-allow-origin"));
   };
};

int main() {}
