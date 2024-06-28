// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include <array>
#include <string_view>

// Macros for GLZ_ENUM
#define GLZ_PARENS ()

#define GLZ_EXPAND(...) \
    GLZ_EXPAND4(GLZ_EXPAND4(GLZ_EXPAND4(GLZ_EXPAND4(__VA_ARGS__))))
#define GLZ_EXPAND4(...) \
    GLZ_EXPAND3(GLZ_EXPAND3(GLZ_EXPAND3(GLZ_EXPAND3(__VA_ARGS__))))
#define GLZ_EXPAND3(...) \
    GLZ_EXPAND2(GLZ_EXPAND2(GLZ_EXPAND2(GLZ_EXPAND2(__VA_ARGS__))))
#define GLZ_EXPAND2(...) \
    GLZ_EXPAND1(GLZ_EXPAND1(GLZ_EXPAND1(GLZ_EXPAND1(__VA_ARGS__))))
#define GLZ_EXPAND1(...) __VA_ARGS__

#define GLZ_FOR_EACH(macro, ...) \
    __VA_OPT__(GLZ_EXPAND(GLZ_FOR_EACH_HELPER(macro, __VA_ARGS__)))
#define GLZ_FOR_EACH_HELPER(macro, a, ...) \
    macro(a) __VA_OPT__(, )                \
        __VA_OPT__(GLZ_FOR_EACH_AGAIN GLZ_PARENS(macro, __VA_ARGS__))
#define GLZ_FOR_EACH_AGAIN() GLZ_FOR_EACH_HELPER

#define GLZ_STRINGIFY(a) #a

#define GLZ_ENUM(EnumType, ...)                                  \
    enum struct EnumType : uint32_t { __VA_ARGS__ };                        \
    constexpr std::string_view EnumType_names[] = {              \
        GLZ_FOR_EACH(GLZ_STRINGIFY, __VA_ARGS__)};               \
    constexpr std::string_view nameof(EnumType value) noexcept { \
        return EnumType_names[size_t(value)];                    \
    };
