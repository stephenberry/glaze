#include "glaze/glaze.hpp"
#include <iostream>
#include <vector>
#include <cstring>
#include <cassert>

// Access internal functions for testing if needed, or just test public API heavily.
// skip_ws is in glz namespace.

void test_countr_zero() {
    // Basic sanity checks for std::countr_zero wrapper
    assert(glz::countr_zero(uint32_t(1)) == 0);
    assert(glz::countr_zero(uint32_t(2)) == 1);
    assert(glz::countr_zero(uint32_t(8)) == 3);
    assert(glz::countr_zero(uint64_t(1)) == 0);
    assert(glz::countr_zero(uint64_t(1) << 63) == 63);
    // std::countr_zero(0) is defined as type width.
    assert(glz::countr_zero(uint32_t(0)) == 32);
    assert(glz::countr_zero(uint64_t(0)) == 64);
}

void test_skip_ws() {
    std::string buffer = "   \t\n\r   {\"key\": \"value\"}";
    glz::context ctx{};
    const char* it = buffer.data();
    const char* end = buffer.data() + buffer.size();

    // Default opts
    bool err = glz::skip_ws<glz::opts{}>(ctx, it, end);
    assert(!err);
    assert(*it == '{');
    (void)err; // Suppress unused warning in Release
}

void test_skip_ws_alignment() {
    // Create a buffer with misalignment
    // We want enough whitespace to trigger the SWAR path (>= 8 bytes)
    std::vector<char> large_buffer(128, ' ');
    large_buffer.back() = '{'; // terminator

    // Try different start offsets to test unaligned loads
    for (int offset = 0; offset < 16; ++offset) {
        glz::context ctx{};
        const char* it = large_buffer.data() + offset;
        const char* end = large_buffer.data() + large_buffer.size();

        bool err = glz::skip_ws<glz::opts{}>(ctx, it, end);
        assert(!err);
        assert(*it == '{');
        (void)err; // Suppress unused warning in Release
    }
}

void test_prettify_output() {
    std::vector<int> v(1000, 1);
    std::string buffer;
    (void)glz::write<glz::opts{.prettify = true}>(v, buffer);

    // Check if we have suspicious 0s (nulls printed as 0?)
    // In valid JSON here (array of ints), we should have spaces, newlines, brackets, commas, and '1'.
    // No '0' (unless 10, but we used 1).

    if (buffer.find('\0') != std::string::npos) {
        std::cout << "FAIL: Found null byte in output!" << std::endl;
        exit(1);
    }

    // Check if indentation is correct (should be spaces)
    // First few chars: [\n   1
    if (buffer.size() > 10) {
        // [ is at 0
        // \n at 1
        // space at 2, 3, 4
        if (buffer[2] != ' ' || buffer[3] != ' ' || buffer[4] != ' ') {
             std::cout << "FAIL: Indentation is not spaces!" << std::endl;
             printf("Bytes: %02X %02X %02X\n", buffer[2], buffer[3], buffer[4]);
             exit(1);
        }
    }
}

int main() {
    test_countr_zero();
    test_skip_ws();
    test_skip_ws_alignment();
    test_prettify_output();
    std::cout << "SWAR Tests Passed" << std::endl;
    return 0;
}
