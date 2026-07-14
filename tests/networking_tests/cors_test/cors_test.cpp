// Glaze Library
// For the license information refer to glaze.hpp

#include "glaze/net/cors.hpp"

#include "ut/ut.hpp"

using namespace ut;

namespace
{
   glz::response run_cors(const glz::cors_config& config, std::string_view origin)
   {
      auto middleware = glz::create_cors_middleware(config);
      glz::request req{};
      req.method = glz::http_method::GET;
      req.headers["origin"] = std::string(origin);
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
      expect(res.response_headers["access-control-allow-origin"] == "*");
      expect(res.response_headers.count("access-control-allow-credentials") == 0);
   };

   "wildcard_with_credentials_rejects_unlisted_origin"_test = [] {
      glz::cors_config config;
      config.allowed_origins = {"*"};
      config.allow_credentials = true;
      expect(not glz::is_origin_allowed(config, "https://evil.example"));

      auto res = run_cors(config, "https://evil.example");
      expect(res.response_headers.count("access-control-allow-origin") == 0);
      expect(res.response_headers.count("access-control-allow-credentials") == 0);
   };

   "credentials_allow_exact_listed_origin"_test = [] {
      glz::cors_config config;
      config.allowed_origins = {"https://app.example", "*"};
      config.allow_credentials = true;
      expect(glz::is_origin_allowed(config, "https://app.example"));
      expect(not glz::is_origin_allowed(config, "https://evil.example"));

      auto res = run_cors(config, "https://app.example");
      expect(res.response_headers["access-control-allow-origin"] == "https://app.example");
      expect(res.response_headers["access-control-allow-credentials"] == "true");
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

int main() {}
