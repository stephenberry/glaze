// Glaze Library
// For the license information refer to glaze.hpp

#include <cassert>
#include <regex>
#include "glaze/net/http_router.hpp"
#include "ut/ut.hpp"

using namespace ut;

suite http_router_constraints_tests = [] {
    "numeric_id_validation"_test = [] {
        glz::http_router router;
        std::unordered_map<std::string, glz::http_router::param_constraint> constraints;
        constraints["id"] = glz::http_router::param_constraint{
            .description = "numeric ID",
            .validation = [](std::string_view value) {
                if (value.empty()) return false;
                for (char c : value) {
                    if (!std::isdigit(c)) return false;
                }
                return true;
            }
        };

        bool handler_called = false;
        router.get("/users/:id", [&](const glz::request& req, glz::response& res) {
            handler_called = true;
            auto it = req.params.find("id");
            if (it != req.params.end()) {
                res.body("User ID: " + it->second);
            } else {
                res.body("Error: ID not found");
                res.status(400);
            }
        }, constraints);

        // Test with valid ID
        auto [handler_valid, params_valid] = router.match(glz::http_method::GET, "/users/123");
        expect(handler_valid != nullptr) << "Handler should match for valid numeric ID\n";
        expect(params_valid.find("id") != params_valid.end()) << "ID parameter should be present\n";
        expect(params_valid.at("id") == "123") << "ID parameter should be '123'\n";
        handler_called = false;
        glz::request req_valid{ .method = glz::http_method::GET, .target = "/users/123", .params = params_valid };
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
        std::unordered_map<std::string, glz::http_router::param_constraint> constraints;
        constraints["email"] = glz::http_router::param_constraint{
            .description = "valid email address",
            .validation = [](std::string_view value) {
                std::regex email_regex(R"(^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$)");
                return std::regex_match(std::string(value), email_regex);
            }
        };

        bool handler_called = false;
        router.get("/contacts/:email", [&](const glz::request& req, glz::response& res) {
            handler_called = true;
            auto it = req.params.find("email");
            if (it != req.params.end()) {
                res.body("Contact Email: " + it->second);
            } else {
                res.body("Error: Email not found");
                res.status(400);
            }
        }, constraints);

        // Test with valid email
        auto [handler_valid, params_valid] = router.match(glz::http_method::GET, "/contacts/test@example.com");
        expect(handler_valid != nullptr) << "Handler should match for valid email\n";
        expect(params_valid.find("email") != params_valid.end()) << "Email parameter should be present\n";
        expect(params_valid.at("email") == "test@example.com") << "Email parameter should be 'test@example.com'\n";
        handler_called = false;
        glz::request req_valid{ .method = glz::http_method::GET, .target = "/contacts/test@example.com", .params = params_valid };
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
        std::unordered_map<std::string, glz::http_router::param_constraint> constraints;
        constraints["code"] = glz::http_router::param_constraint{
            .description = "4-digit code",
            .validation = [](std::string_view value) {
                if (value.size() != 4) return false;
                for (char c : value) {
                    if (!std::isdigit(c)) return false;
                }
                return true;
            }
        };

        bool handler_called = false;
        router.get("/verify/:code", [&](const glz::request& req, glz::response& res) {
            handler_called = true;
            auto it = req.params.find("code");
            if (it != req.params.end()) {
                res.body("Verification Code: " + it->second);
            } else {
                res.body("Error: Code not found");
                res.status(400);
            }
        }, constraints);

        // Test with valid code
        auto [handler_valid, params_valid] = router.match(glz::http_method::GET, "/verify/1234");
        expect(handler_valid != nullptr) << "Handler should match for valid 4-digit code\n";
        expect(params_valid.find("code") != params_valid.end()) << "Code parameter should be present\n";
        expect(params_valid.at("code") == "1234") << "Code parameter should be '1234'\n";
        handler_called = false;
        glz::request req_valid{ .method = glz::http_method::GET, .target = "/verify/1234", .params = params_valid };
        glz::response res_valid;
        handler_valid(req_valid, res_valid);
        expect(handler_called) << "Handler should be called for valid code\n";
        expect(res_valid.response_body == "Verification Code: 1234") << "Response body should contain verification code\n";

        // Test with invalid code (wrong length)
        auto [handler_invalid1, params_invalid1] = router.match(glz::http_method::GET, "/verify/123");
        expect(handler_invalid1 == nullptr) << "Handler should not match for code of wrong length\n";

        // Test with invalid code (non-numeric)
        auto [handler_invalid2, params_invalid2] = router.match(glz::http_method::GET, "/verify/abcd");
        expect(handler_invalid2 == nullptr) << "Handler should not match for non-numeric code\n";
    };
};

int main() {}
