// Glaze Library
// For the license information refer to glaze.hpp

#include "glaze/net/http_router.hpp"

#include <cassert>
#include <regex>

#include "ut/ut.hpp"

using namespace ut;

suite http_router_constraints_tests = [] {
   "numeric_id_validation"_test = [] {
      glz::http_router router;
      std::unordered_map<std::string, glz::param_constraint> constraints;
      constraints["id"] = glz::param_constraint{.description = "numeric ID", .validation = [](std::string_view value) {
                                                   if (value.empty()) return false;
                                                   for (char c : value) {
                                                      if (!std::isdigit(c)) return false;
                                                   }
                                                   return true;
                                                }};

      bool handler_called = false;
      router.get("/users/:id",
                 [&](const glz::request& req, glz::response& res) {
                    handler_called = true;
                    auto it = req.params.find("id");
                    if (it != req.params.end()) {
                       res.body("User ID: " + it->second);
                    }
                    else {
                       res.body("Error: ID not found");
                       res.status(400);
                    }
                 },
                 {.constraints = constraints});

      // Test with valid ID
      auto [handler_valid, params_valid] = router.match(glz::http_method::GET, "/users/123");
      expect(handler_valid != nullptr) << "Handler should match for valid numeric ID\n";
      expect(params_valid.find("id") != params_valid.end()) << "ID parameter should be present\n";
      expect(params_valid.at("id") == "123") << "ID parameter should be '123'\n";
      handler_called = false;
      glz::request req_valid{.method = glz::http_method::GET, .target = "/users/123", .params = params_valid};
      glz::response res_valid;
      handler_valid(req_valid, res_valid);
      expect(handler_called) << "Handler should be called for valid ID\n";
      expect(res_valid.response_body == "User ID: 123") << "Response body should contain user ID\n";

      // Test with invalid ID
      auto [handler_invalid, params_invalid] = router.match(glz::http_method::GET, "/users/abc");
      expect(handler_invalid == nullptr) << "Handler should not match for non-numeric ID\n";
   };

   "email_validation_with_regex"_test = [] {
      glz::http_router router;
      std::unordered_map<std::string, glz::param_constraint> constraints;
      constraints["email"] =
         glz::param_constraint{.description = "valid email address", .validation = [](std::string_view value) {
                                  std::regex email_regex(R"(^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$)");
                                  return std::regex_match(std::string(value), email_regex);
                               }};

      bool handler_called = false;
      router.get("/contacts/:email",
                 [&](const glz::request& req, glz::response& res) {
                    handler_called = true;
                    auto it = req.params.find("email");
                    if (it != req.params.end()) {
                       res.body("Contact Email: " + it->second);
                    }
                    else {
                       res.body("Error: Email not found");
                       res.status(400);
                    }
                 },
                 {.constraints = constraints});

      // Test with valid email
      auto [handler_valid, params_valid] = router.match(glz::http_method::GET, "/contacts/test@example.com");
      expect(handler_valid != nullptr) << "Handler should match for valid email\n";
      expect(params_valid.find("email") != params_valid.end()) << "Email parameter should be present\n";
      expect(params_valid.at("email") == "test@example.com") << "Email parameter should be 'test@example.com'\n";
      handler_called = false;
      glz::request req_valid{
         .method = glz::http_method::GET, .target = "/contacts/test@example.com", .params = params_valid};
      glz::response res_valid;
      handler_valid(req_valid, res_valid);
      expect(handler_called) << "Handler should be called for valid email\n";
      expect(res_valid.response_body == "Contact Email: test@example.com") << "Response body should contain email\n";

      // Test with invalid email
      auto [handler_invalid, params_invalid] = router.match(glz::http_method::GET, "/contacts/invalid-email");
      expect(handler_invalid == nullptr) << "Handler should not match for invalid email\n";
   };

   "four_digit_code_validation"_test = [] {
      glz::http_router router;
      std::unordered_map<std::string, glz::param_constraint> constraints;
      constraints["code"] =
         glz::param_constraint{.description = "4-digit code", .validation = [](std::string_view value) {
                                  if (value.size() != 4) return false;
                                  for (char c : value) {
                                     if (!std::isdigit(c)) return false;
                                  }
                                  return true;
                               }};

      bool handler_called = false;
      router.get("/verify/:code",
                 [&](const glz::request& req, glz::response& res) {
                    handler_called = true;
                    auto it = req.params.find("code");
                    if (it != req.params.end()) {
                       res.body("Verification Code: " + it->second);
                    }
                    else {
                       res.body("Error: Code not found");
                       res.status(400);
                    }
                 },
                 {.constraints = constraints});

      // Test with valid code
      auto [handler_valid, params_valid] = router.match(glz::http_method::GET, "/verify/1234");
      expect(handler_valid != nullptr) << "Handler should match for valid 4-digit code\n";
      expect(params_valid.find("code") != params_valid.end()) << "Code parameter should be present\n";
      expect(params_valid.at("code") == "1234") << "Code parameter should be '1234'\n";
      handler_called = false;
      glz::request req_valid{.method = glz::http_method::GET, .target = "/verify/1234", .params = params_valid};
      glz::response res_valid;
      handler_valid(req_valid, res_valid);
      expect(handler_called) << "Handler should be called for valid code\n";
      expect(res_valid.response_body == "Verification Code: 1234")
         << "Response body should contain verification code\n";

      // Test with invalid code (wrong length)
      auto [handler_invalid1, params_invalid1] = router.match(glz::http_method::GET, "/verify/123");
      expect(handler_invalid1 == nullptr) << "Handler should not match for code of wrong length\n";

      // Test with invalid code (non-numeric)
      auto [handler_invalid2, params_invalid2] = router.match(glz::http_method::GET, "/verify/abcd");
      expect(handler_invalid2 == nullptr) << "Handler should not match for non-numeric code\n";
   };
};

suite http_router_functionality_tests = [] {
   "basic_route_matching"_test = [] {
      glz::http_router router;
      bool get_handler_called = false;
      router.get("/hello", [&](const glz::request&, glz::response& res) {
         get_handler_called = true;
         res.body("Hello, World!");
      });

      // Test GET route
      auto [handler_get, params_get] = router.match(glz::http_method::GET, "/hello");
      expect(handler_get != nullptr) << "GET handler should match for /hello\n";
      get_handler_called = false;
      glz::request req_get{.method = glz::http_method::GET, .target = "/hello", .params = params_get};
      glz::response res_get;
      handler_get(req_get, res_get);
      expect(get_handler_called) << "GET handler should be called\n";
      expect(res_get.response_body == "Hello, World!") << "GET response body should be 'Hello, World!'\n";

      // Test unmatched route
      auto [handler_unmatched, params_unmatched] = router.match(glz::http_method::GET, "/not-found");
      expect(handler_unmatched == nullptr) << "Handler should not match for unmatched route\n";
   };

   "different_http_methods"_test = [] {
      glz::http_router router;
      bool get_handler_called = false;
      bool post_handler_called = false;
      bool put_handler_called = false;
      bool delete_handler_called = false;

      router.get("/resource", [&](const glz::request&, glz::response& res) {
         get_handler_called = true;
         res.body("GET Resource");
      });
      router.post("/resource", [&](const glz::request&, glz::response& res) {
         post_handler_called = true;
         res.body("POST Resource");
      });
      router.put("/resource", [&](const glz::request&, glz::response& res) {
         put_handler_called = true;
         res.body("PUT Resource");
      });
      router.del("/resource", [&](const glz::request&, glz::response& res) {
         delete_handler_called = true;
         res.body("DELETE Resource");
      });

      // Test GET
      auto [handler_get, params_get] = router.match(glz::http_method::GET, "/resource");
      expect(handler_get != nullptr) << "GET handler should match\n";
      get_handler_called = false;
      glz::request req_get{.method = glz::http_method::GET, .target = "/resource", .params = params_get};
      glz::response res_get;
      handler_get(req_get, res_get);
      expect(get_handler_called) << "GET handler should be called\n";
      expect(res_get.response_body == "GET Resource") << "GET response body should be correct\n";

      // Test POST
      auto [handler_post, params_post] = router.match(glz::http_method::POST, "/resource");
      expect(handler_post != nullptr) << "POST handler should match\n";
      post_handler_called = false;
      glz::request req_post{.method = glz::http_method::POST, .target = "/resource", .params = params_post};
      glz::response res_post;
      handler_post(req_post, res_post);
      expect(post_handler_called) << "POST handler should be called\n";
      expect(res_post.response_body == "POST Resource") << "POST response body should be correct\n";

      // Test PUT
      auto [handler_put, params_put] = router.match(glz::http_method::PUT, "/resource");
      expect(handler_put != nullptr) << "PUT handler should match\n";
      put_handler_called = false;
      glz::request req_put{.method = glz::http_method::PUT, .target = "/resource", .params = params_put};
      glz::response res_put;
      handler_put(req_put, res_put);
      expect(put_handler_called) << "PUT handler should be called\n";
      expect(res_put.response_body == "PUT Resource") << "PUT response body should be correct\n";

      // Test DELETE
      auto [handler_delete, params_delete] = router.match(glz::http_method::DELETE, "/resource");
      expect(handler_delete != nullptr) << "DELETE handler should match\n";
      delete_handler_called = false;
      glz::request req_delete{.method = glz::http_method::DELETE, .target = "/resource", .params = params_delete};
      glz::response res_delete;
      handler_delete(req_delete, res_delete);
      expect(delete_handler_called) << "DELETE handler should be called\n";
      expect(res_delete.response_body == "DELETE Resource") << "DELETE response body should be correct\n";

      // Test unmatched method
      auto [handler_patch, params_patch] = router.match(glz::http_method::PATCH, "/resource");
      expect(handler_patch == nullptr) << "PATCH handler should not match as it was not defined\n";
   };

   "wildcard_route_matching"_test = [] {
      glz::http_router router;
      bool wildcard_handler_called = false;
      router.get("/files/*path", [&](const glz::request& req, glz::response& res) {
         wildcard_handler_called = true;
         auto it = req.params.find("path");
         if (it != req.params.end()) {
            res.body("File Path: " + it->second);
         }
         else {
            res.body("Error: Path not found");
            res.status(400);
         }
      });

      // Test with wildcard path
      auto [handler_wildcard, params_wildcard] = router.match(glz::http_method::GET, "/files/documents/report.pdf");
      expect(handler_wildcard != nullptr) << "Wildcard handler should match for /files/documents/report.pdf\n";
      expect(params_wildcard.find("path") != params_wildcard.end()) << "Path parameter should be present\n";
      expect(params_wildcard.at("path") == "documents/report.pdf") << "Path parameter should capture remaining path\n";
      wildcard_handler_called = false;
      glz::request req_wildcard{
         .method = glz::http_method::GET, .target = "/files/documents/report.pdf", .params = params_wildcard};
      glz::response res_wildcard;
      handler_wildcard(req_wildcard, res_wildcard);
      expect(wildcard_handler_called) << "Wildcard handler should be called\n";
      expect(res_wildcard.response_body == "File Path: documents/report.pdf")
         << "Response body should contain captured path\n";

      // Test with minimal wildcard path
      auto [handler_minimal, params_minimal] = router.match(glz::http_method::GET, "/files/doc");
      expect(handler_minimal != nullptr) << "Wildcard handler should match for /files/doc\n";
      expect(params_minimal.find("path") != params_minimal.end())
         << "Path parameter should be present for minimal path\n";
      expect(params_minimal.at("path") == "doc") << "Path parameter should capture minimal path\n";
   };

   "route_precedence"_test = [] {
      glz::http_router router;
      bool specific_handler_called = false;
      bool general_handler_called = false;

      // More specific route
      router.get("/users/admin", [&](const glz::request&, glz::response& res) {
         specific_handler_called = true;
         res.body("Admin User");
      });

      // General route with parameter
      router.get("/users/:id", [&](const glz::request& req, glz::response& res) {
         general_handler_called = true;
         auto it = req.params.find("id");
         if (it != req.params.end()) {
            res.body("User ID: " + it->second);
         }
      });

      // Test specific route
      auto [handler_specific, params_specific] = router.match(glz::http_method::GET, "/users/admin");
      expect(handler_specific != nullptr) << "Specific handler should match for /users/admin\n";
      specific_handler_called = false;
      glz::request req_specific{.method = glz::http_method::GET, .target = "/users/admin", .params = params_specific};
      glz::response res_specific;
      handler_specific(req_specific, res_specific);
      expect(specific_handler_called) << "Specific handler should be called\n";
      expect(res_specific.response_body == "Admin User") << "Specific route response should be correct\n";
      expect(general_handler_called == false) << "General handler should not be called for specific route\n";

      // Test general route
      auto [handler_general, params_general] = router.match(glz::http_method::GET, "/users/123");
      expect(handler_general != nullptr) << "General handler should match for /users/123\n";
      general_handler_called = false;
      glz::request req_general{.method = glz::http_method::GET, .target = "/users/123", .params = params_general};
      glz::response res_general;
      handler_general(req_general, res_general);
      expect(general_handler_called) << "General handler should be called\n";
      expect(res_general.response_body == "User ID: 123") << "General route response should be correct\n";
   };
};

int main() {}
