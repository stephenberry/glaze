// Glaze Library
// For the license information refer to glaze.hpp

#include <cassert>
#include <regex>
#include "glaze/net/http_router.hpp"

void test_http_router_constraints() {
    glz::http_router router;

    // Test 1: Validation function for numeric ID
    std::unordered_map<std::string, glz::http_router::param_constraint> constraints1;
    constraints1["id"] = glz::http_router::param_constraint{
        .description = "numeric ID",
        .validation = [](std::string_view value) {
            if (value.empty()) return false;
            for (char c : value) {
                if (!std::isdigit(c)) return false;
            }
            return true;
        }
    };

    bool handler1_called = false;
    router.get("/users/:id", [&](const glz::request& req, glz::response& res) {
        handler1_called = true;
        auto it = req.params.find("id");
        if (it != req.params.end()) {
            res.body("User ID: " + it->second);
        } else {
            res.body("Error: ID not found");
            res.status(400);
        }
    }, constraints1);

    // Test with valid ID
    auto [handler1_valid, params1_valid] = router.match(glz::http_method::GET, "/users/123");
    std::cout << "Handler valid: " << (handler1_valid != nullptr ? "true" : "false") << "\n";
    std::cout << "Params size: " << params1_valid.size() << "\n";
    for (const auto& [key, value] : params1_valid) {
        std::cout << "Param: " << key << " = " << value << "\n";
    }
    assert(handler1_valid != nullptr);
    assert(params1_valid.find("id") != params1_valid.end());
    assert(params1_valid.at("id") == "123");
    handler1_called = false;
    glz::request req1_valid{ .method = glz::http_method::GET, .target = "/users/123", .params = params1_valid };
    glz::response res1_valid;
    handler1_valid(req1_valid, res1_valid);
    assert(handler1_called);
    assert(res1_valid.response_body == "User ID: 123");

    // Test with invalid ID
    auto [handler1_invalid, params1_invalid] = router.match(glz::http_method::GET, "/users/abc");
    assert(handler1_invalid == nullptr); // Should not match due to validation failure

    // Test 2: Validation function using std::regex
    std::unordered_map<std::string, glz::http_router::param_constraint> constraints2;
    constraints2["email"] = glz::http_router::param_constraint{
        .description = "valid email address",
        .validation = [](std::string_view value) {
            std::regex email_regex(R"(^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$)");
            return std::regex_match(std::string(value), email_regex);
        }
    };

    bool handler2_called = false;
    router.get("/contacts/:email", [&](const glz::request& req, glz::response& res) {
        handler2_called = true;
        auto it = req.params.find("email");
        if (it != req.params.end()) {
            res.body("Contact Email: " + it->second);
        } else {
            res.body("Error: Email not found");
            res.status(400);
        }
    }, constraints2);

    // Test with valid email
    auto [handler2_valid, params2_valid] = router.match(glz::http_method::GET, "/contacts/test@example.com");
    assert(handler2_valid != nullptr);
    assert(params2_valid.find("email") != params2_valid.end());
    assert(params2_valid.at("email") == "test@example.com");
    handler2_called = false;
    glz::request req2_valid{ .method = glz::http_method::GET, .target = "/contacts/test@example.com", .params = params2_valid };
    glz::response res2_valid;
    handler2_valid(req2_valid, res2_valid);
    assert(handler2_called);
    assert(res2_valid.response_body == "Contact Email: test@example.com");

    // Test with invalid email
    auto [handler2_invalid, params2_invalid] = router.match(glz::http_method::GET, "/contacts/invalid-email");
    assert(handler2_invalid == nullptr); // Should not match due to validation failure

    // Test 3: Validation function with simple logic
    std::unordered_map<std::string, glz::http_router::param_constraint> constraints3;
    constraints3["code"] = glz::http_router::param_constraint{
        .description = "4-digit code",
        .validation = [](std::string_view value) {
            if (value.size() != 4) return false;
            for (char c : value) {
                if (!std::isdigit(c)) return false;
            }
            return true;
        }
    };

    bool handler3_called = false;
    router.get("/verify/:code", [&](const glz::request& req, glz::response& res) {
        handler3_called = true;
        auto it = req.params.find("code");
        if (it != req.params.end()) {
            res.body("Verification Code: " + it->second);
        } else {
            res.body("Error: Code not found");
            res.status(400);
        }
    }, constraints3);

    // Test with valid code
    auto [handler3_valid, params3_valid] = router.match(glz::http_method::GET, "/verify/1234");
    assert(handler3_valid != nullptr);
    assert(params3_valid.find("code") != params3_valid.end());
    assert(params3_valid.at("code") == "1234");
    handler3_called = false;
    glz::request req3_valid{ .method = glz::http_method::GET, .target = "/verify/1234", .params = params3_valid };
    glz::response res3_valid;
    handler3_valid(req3_valid, res3_valid);
    assert(handler3_called);
    assert(res3_valid.response_body == "Verification Code: 1234");

    // Test with invalid code (wrong length)
    auto [handler3_invalid1, params3_invalid1] = router.match(glz::http_method::GET, "/verify/123");
    assert(handler3_invalid1 == nullptr); // Should not match due to validation failure

    // Test with invalid code (non-numeric)
    auto [handler3_invalid2, params3_invalid2] = router.match(glz::http_method::GET, "/verify/abcd");
    assert(handler3_invalid2 == nullptr); // Should not match due to validation failure
}

int main() {
    test_http_router_constraints();
    std::cout << "All HTTP router constraint tests passed.\n";
    return 0;
}
