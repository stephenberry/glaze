// CORS Support for Glaze Library
// Add this to a new file: glaze/net/cors.hpp

#pragma once

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <functional>
#include <regex>
#include <string>
#include <unordered_set>
#include <vector>

#include "glaze/net/http_router.hpp"

namespace glz
{
   inline std::string glob_to_regex(std::string_view pattern)
   {
      std::string regex;
      regex.reserve(pattern.size() * 2 + 2);
      regex.push_back('^');

      for (char ch : pattern) {
         switch (ch) {
         case '*':
            regex.append(".*");
            break;
         case '?':
            regex.push_back('.');
            break;
         case '.':
         case '+':
         case '(': 
         case ')': 
         case '[':
         case ']':
         case '{':
         case '}':
         case '^':
         case '$':
         case '|':
         case '\\':
            regex.push_back('\\');
            regex.push_back(ch);
            break;
         default:
            regex.push_back(ch);
            break;
         }
      }

      regex.push_back('$');
      return regex;
   }

   /**
    * @brief Configuration for CORS (Cross-Origin Resource Sharing) support
    */
   struct cors_config
   {
      /**
       * @brief List of allowed origins
       *
       * Use "*" to allow all origins, or specify specific origins like "https://example.com"
       * For credentials to work, you cannot use "*" - must specify exact origins
       */
      std::vector<std::string> allowed_origins = {"*"};

      /**
       * @brief List of allowed HTTP methods
       */
      std::vector<std::string> allowed_methods = {"GET", "POST", "PUT", "DELETE", "PATCH", "HEAD", "OPTIONS"};

      /**
       * @brief List of allowed request headers
       */
      std::vector<std::string> allowed_headers = {"Content-Type", "Authorization", "X-Requested-With"};

      /**
       * @brief List of headers to expose to the client
       */
      std::vector<std::string> exposed_headers = {};

      /**
       * @brief Whether to allow credentials (cookies, authorization headers)
       *
       * When true, allowed_origins cannot contain "*"
       */
      bool allow_credentials = false;

      /**
       * @brief Maximum age for preflight cache (in seconds)
       *
       * Default is 24 hours (86400 seconds)
       */
      int max_age = 86400;

      /**
       * @brief Whether to automatically handle preflight OPTIONS requests
       *
       * When true, the middleware will respond to OPTIONS requests with appropriate headers
       */
      bool handle_preflight = true;

      /**
       * @brief Optional regular expression matchers for origin validation
       */
      std::vector<std::string> allowed_origin_regexes{};

      /**
       * @brief Optional callback to dynamically validate origins
       */
      std::function<bool(std::string_view)> origin_validator{};

      /**
       * @brief Treat all methods as allowed when responding to preflight requests
       */
      bool allow_all_methods = false;

      /**
       * @brief Treat all request headers as allowed when responding to preflight requests
       */
      bool allow_all_headers = false;

      /**
       * @brief Include Access-Control-Allow-Private-Network when requested
       */
      bool allow_private_network = false;

      /**
       * @brief HTTP status code to return for successful OPTIONS preflight responses
       */
      int options_success_status = 204;

      /**
       * @brief Helper to append an allowed header without duplicating entries
       */
      cors_config& add_allowed_header(std::string header)
      {
         if (std::find(allowed_headers.begin(), allowed_headers.end(), header) == allowed_headers.end()) {
            allowed_headers.emplace_back(std::move(header));
         }
         return *this;
      }

      /**
       * @brief Helper to append an exposed header without duplicating entries
       */
      cors_config& add_exposed_header(std::string header)
      {
         if (std::find(exposed_headers.begin(), exposed_headers.end(), header) == exposed_headers.end()) {
            exposed_headers.emplace_back(std::move(header));
         }
         return *this;
      }

      /**
       * @brief Helper to append an allowed origin without duplicating entries
       */
      cors_config& add_allowed_origin(std::string origin)
      {
         if (std::find(allowed_origins.begin(), allowed_origins.end(), origin) == allowed_origins.end()) {
            allowed_origins.emplace_back(std::move(origin));
         }
         return *this;
      }

      /**
       * @brief Helper to append an allowed method without duplicating entries
       */
      cors_config& add_allowed_method(std::string method)
      {
         if (std::find(allowed_methods.begin(), allowed_methods.end(), method) == allowed_methods.end()) {
            allowed_methods.emplace_back(std::move(method));
         }
         return *this;
      }

      /**
       * @brief Helper to append a glob pattern (e.g. https://\*.example.com) for origin validation
       */
      cors_config& add_allowed_origin_pattern(std::string pattern)
      {
         allowed_origin_regexes.emplace_back(glob_to_regex(pattern));
         return *this;
      }

      /**
       * @brief Helper to append a raw regular expression for origin validation
       */
      cors_config& add_allowed_origin_regex(std::string regex)
      {
         allowed_origin_regexes.emplace_back(std::move(regex));
         return *this;
      }

      /**
       * @brief Helper to register a dynamic origin validator
       */
      cors_config& set_origin_validator(std::function<bool(std::string_view)> validator)
      {
         origin_validator = std::move(validator);
         return *this;
      }
   };

   /**
    * @brief Check if an origin is allowed based on the CORS configuration
    *
    * @param config The CORS configuration
    * @param origin The origin to check
    * @return true if the origin is allowed, false otherwise
    */
   inline bool is_origin_allowed(const cors_config& config, std::string_view origin,
                                 const std::vector<std::regex>& compiled_patterns)
   {
      if (origin.empty()) {
         return false;
      }

      if (config.origin_validator) {
         try {
            if (config.origin_validator(origin)) {
               return true;
            }
         }
         catch (...) {
            // If the validator throws, treat as not allowed.
         }
      }

      for (const auto& pattern : compiled_patterns) {
         if (std::regex_match(origin.begin(), origin.end(), pattern)) {
            return true;
         }
      }

      // If no specific origins are configured or "*" is present, allow all
      if (config.allowed_origins.empty()) {
         if (compiled_patterns.empty() && !config.origin_validator) {
            return true;
         }
      }
      if (std::find(config.allowed_origins.begin(), config.allowed_origins.end(), "*") != config.allowed_origins.end()) {
         return true;
      }

      // Check if the origin is in the allowed list
      return std::find(config.allowed_origins.begin(), config.allowed_origins.end(), origin) !=
             config.allowed_origins.end();
   }

   /**
    * @brief Join a vector of strings with a delimiter
    *
    * @param vec The vector of strings to join
    * @param delimiter The delimiter to use
    * @return The joined string
    */
   inline std::string join_strings(const std::vector<std::string>& vec, const std::string& delimiter)
   {
      if (vec.empty()) return "";

      std::string result = vec[0];
      for (size_t i = 1; i < vec.size(); ++i) {
         result += delimiter + vec[i];
      }
      return result;
   }

   /**
    * @brief Create a CORS middleware handler
    *
    * @param config The CORS configuration to use
    * @return A middleware handler function
    */
   inline handler create_cors_middleware(const cors_config& config = {})
   {
      std::vector<std::regex> compiled_origin_regexes;
      compiled_origin_regexes.reserve(config.allowed_origin_regexes.size());
      for (const auto& pattern : config.allowed_origin_regexes) {
         try {
            compiled_origin_regexes.emplace_back(pattern, std::regex::ECMAScript | std::regex::icase);
         }
         catch (const std::regex_error& e) {
            std::fprintf(stderr, "CORS: invalid origin regex '%s': %s\n", pattern.c_str(), e.what());
         }
      }

      const bool wildcard_with_credentials = config.allow_credentials &&
                                             std::find(config.allowed_origins.begin(), config.allowed_origins.end(),
                                                       "*") != config.allowed_origins.end();
      if (wildcard_with_credentials) {
         static std::atomic<bool> warning_emitted = false;
         bool expected = false;
         if (warning_emitted.compare_exchange_strong(expected, true)) {
            std::fprintf(stderr,
                         "glz::create_cors_middleware warning: allow_credentials=true with wildcard origin '*' "
                         "is not permitted by browsers. Falling back to echoing the request origin.\n");
         }
      }

      const bool has_dynamic_origin = config.origin_validator || !compiled_origin_regexes.empty();

      return [config, compiled_origin_regexes = std::move(compiled_origin_regexes), has_dynamic_origin](
                const request& req, response& res) {
         // Get the origin from the request headers
         std::string origin;
         auto origin_it = req.headers.find("origin");
         if (origin_it != req.headers.end()) {
            origin = origin_it->second;
         }

         // Check if this is a preflight request (OPTIONS method with specific headers)
         bool is_preflight = (req.method == http_method::OPTIONS) &&
                             (req.headers.find("access-control-request-method") != req.headers.end());

         // Always add CORS headers if origin is allowed
         if (!origin.empty() && is_origin_allowed(config, origin, compiled_origin_regexes)) {
            // Determine which origin to send back
            std::string allowed_origin = "*";
            const bool contains_wildcard =
               std::find(config.allowed_origins.begin(), config.allowed_origins.end(), "*") != config.allowed_origins.end();
            if (config.allow_credentials || !contains_wildcard || has_dynamic_origin) {
               allowed_origin = origin;
            }

            res.header("Access-Control-Allow-Origin", allowed_origin);

            // Add credentials header if enabled
            if (config.allow_credentials) {
               res.header("Access-Control-Allow-Credentials", "true");
            }

            if (config.allow_private_network) {
               auto private_network_it = req.headers.find("access-control-request-private-network");
               if (private_network_it != req.headers.end()) {
                  res.header("Access-Control-Allow-Private-Network", "true");
               }
            }

            // Add exposed headers if any
            if (!config.exposed_headers.empty()) {
               res.header("Access-Control-Expose-Headers", join_strings(config.exposed_headers, ", "));
            }

            // Handle preflight requests
            if (is_preflight && config.handle_preflight) {
               // Add allowed methods
               if (config.allow_all_methods) {
                  auto method_header = req.headers.find("access-control-request-method");
                  if (method_header != req.headers.end()) {
                     res.header("Access-Control-Allow-Methods", method_header->second);
                  }
                  else {
                     res.header("Access-Control-Allow-Methods", "*");
                  }
               }
               else {
                  res.header("Access-Control-Allow-Methods", join_strings(config.allowed_methods, ", "));
               }

               // Add allowed headers
               if (config.allow_all_headers) {
                  res.header("Access-Control-Allow-Headers", "*");
               }
               else if (!config.allowed_headers.empty()) {
                  res.header("Access-Control-Allow-Headers", join_strings(config.allowed_headers, ", "));
               }
               else if (auto requested_headers = req.headers.find("access-control-request-headers");
                        requested_headers != req.headers.end()) {
                  res.header("Access-Control-Allow-Headers", requested_headers->second);
               }

               // Add max age
               if (config.max_age > 0) {
                  res.header("Access-Control-Max-Age", std::to_string(config.max_age));
               }

               // Set status to 204 (No Content) for preflight response
               res.status(config.options_success_status);
            }
         }
         else if (is_preflight && config.handle_preflight) {
            // Origin not allowed, but it's a preflight request - respond with 403
            res.status(403).body("CORS: Origin not allowed");
         }
      };
   }

   /**
    * @brief Simple CORS middleware with default configuration
    *
    * Allows all origins, methods, and headers - suitable for development
    */
   inline handler simple_cors() { return create_cors_middleware(); }

   /**
    * @brief Restrictive CORS middleware
    *
    * Only allows specific origins - suitable for production
    *
    * @param origins Vector of allowed origins
    * @param allow_credentials Whether to allow credentials
    * @return CORS middleware handler
    */
   inline handler restrictive_cors(const std::vector<std::string>& origins, bool allow_credentials = false)
   {
      cors_config config;
      config.allowed_origins = origins;
      config.allow_credentials = allow_credentials;
      return create_cors_middleware(config);
   }
}
