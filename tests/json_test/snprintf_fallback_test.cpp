// Glaze Library
// For the license information refer to glaze.hpp

// Force snprintf fallback by defining this BEFORE including glaze
#define GLZ_USE_STD_FORMAT_FLOAT 0

#include <format>
#include <numbers>

#include "glaze/glaze.hpp"
#include "ut/ut.hpp"

using namespace ut;

// Verify the macro was respected
static_assert(GLZ_USE_STD_FORMAT_FLOAT == 0, "GLZ_USE_STD_FORMAT_FLOAT should be 0 for this test");

// Test structs for float_format option
struct float_format_2f : glz::opts
{
   static constexpr std::string_view float_format = "{:.2f}";
};

struct float_format_0f : glz::opts
{
   static constexpr std::string_view float_format = "{:.0f}";
};

struct float_format_6f : glz::opts
{
   static constexpr std::string_view float_format = "{:.6f}";
};

struct float_format_2e : glz::opts
{
   static constexpr std::string_view float_format = "{:.2e}";
};

struct float_format_4E : glz::opts
{
   static constexpr std::string_view float_format = "{:.4E}";
};

struct float_format_6g : glz::opts
{
   static constexpr std::string_view float_format = "{:.6g}";
};

// Test struct for per-member float_format wrapper
struct coordinates_t
{
   double lat{16.0000000001};
   double lon{45.9999999999};
};

template <>
struct glz::meta<coordinates_t>
{
   using T = coordinates_t;
   static constexpr auto value =
      glz::object("lat", glz::float_format<&T::lat, "{:.2f}">, "lon", glz::float_format<&T::lon, "{:.2f}">);
};

suite snprintf_fallback_tests = [] {
   "snprintf_fallback_fixed_2f"_test = [] {
      double pi = std::numbers::pi_v<double>;
      std::string json = glz::write<float_format_2f{}>(pi).value_or("error");
      std::string expected = std::format("{:.2f}", pi);
      expect(json == expected) << "got: " << json << ", expected: " << expected;
   };

   "snprintf_fallback_fixed_6f"_test = [] {
      double pi = std::numbers::pi_v<double>;
      std::string json = glz::write<float_format_6f{}>(pi).value_or("error");
      std::string expected = std::format("{:.6f}", pi);
      expect(json == expected) << "got: " << json << ", expected: " << expected;
   };

   "snprintf_fallback_fixed_0f"_test = [] {
      double value = 3.7;
      std::string json = glz::write<float_format_0f{}>(value).value_or("error");
      std::string expected = std::format("{:.0f}", value);
      expect(json == expected) << "got: " << json << ", expected: " << expected;
   };

   "snprintf_fallback_scientific_e"_test = [] {
      double value = 1234567.89;
      std::string json = glz::write<float_format_2e{}>(value).value_or("error");
      std::string expected = std::format("{:.2e}", value);
      expect(json == expected) << "got: " << json << ", expected: " << expected;
   };

   "snprintf_fallback_scientific_E"_test = [] {
      double value = 1234567.89;
      std::string json = glz::write<float_format_4E{}>(value).value_or("error");
      std::string expected = std::format("{:.4E}", value);
      expect(json == expected) << "got: " << json << ", expected: " << expected;
   };

   "snprintf_fallback_general_g"_test = [] {
      double value = 0.000123456;
      std::string json = glz::write<float_format_6g{}>(value).value_or("error");
      std::string expected = std::format("{:.6g}", value);
      expect(json == expected) << "got: " << json << ", expected: " << expected;
   };

   "snprintf_fallback_negative"_test = [] {
      double value = -3.14159;
      std::string json = glz::write<float_format_2f{}>(value).value_or("error");
      std::string expected = std::format("{:.2f}", value);
      expect(json == expected) << "got: " << json << ", expected: " << expected;
   };

   "snprintf_fallback_zero"_test = [] {
      double value = 0.0;
      std::string json = glz::write<float_format_2f{}>(value).value_or("error");
      std::string expected = std::format("{:.2f}", value);
      expect(json == expected) << "got: " << json << ", expected: " << expected;
   };

   "snprintf_fallback_very_small"_test = [] {
      double value = 0.000000123;
      std::string json = glz::write<float_format_2e{}>(value).value_or("error");
      std::string expected = std::format("{:.2e}", value);
      expect(json == expected) << "got: " << json << ", expected: " << expected;
   };

   "snprintf_fallback_very_large"_test = [] {
      double value = 1.23e15;
      std::string json = glz::write<float_format_2e{}>(value).value_or("error");
      std::string expected = std::format("{:.2e}", value);
      expect(json == expected) << "got: " << json << ", expected: " << expected;
   };

   "snprintf_fallback_float32"_test = [] {
      float pi = std::numbers::pi_v<float>;
      std::string json = glz::write<float_format_2f{}>(pi).value_or("error");
      std::string expected = std::format("{:.2f}", pi);
      expect(json == expected) << "got: " << json << ", expected: " << expected;
   };

   "snprintf_fallback_vector"_test = [] {
      std::vector<double> values{3.14159, 2.71828, 1.41421};
      std::string json = glz::write<float_format_2f{}>(values).value_or("error");
      expect(json == R"([3.14,2.72,1.41])") << json;
   };

   "snprintf_fallback_object"_test = [] {
      coordinates_t point{};
      std::string json;
      auto ec = glz::write_json(point, json);
      expect(!ec) << glz::format_error(ec, json);
      expect(json == R"({"lat":16.00,"lon":46.00})") << json;
   };

   "snprintf_fallback_wrapper_roundtrip"_test = [] {
      coordinates_t point{123.456789, -45.678901};
      std::string json;
      expect(!glz::write_json(point, json));
      // Values are formatted with 2 decimal places
      expect(json == R"({"lat":123.46,"lon":-45.68})") << json;

      // Read back
      coordinates_t point2{};
      expect(!glz::read_json(point2, json));
      expect(std::abs(point2.lat - 123.46) < 0.01);
      expect(std::abs(point2.lon - (-45.68)) < 0.01);
   };
};

// Tests for the compile-time format string translator (std::format -> printf)
suite printf_format_translator_tests = [] {
   "to_printf_fmt_basic_fixed"_test = [] {
      constexpr auto result = glz::detail::to_printf_fmt("{:.2f}");
      expect(std::string_view(result.data) == "%.2f");
   };

   "to_printf_fmt_basic_scientific"_test = [] {
      constexpr auto result = glz::detail::to_printf_fmt("{:.3e}");
      expect(std::string_view(result.data) == "%.3e");
   };

   "to_printf_fmt_basic_scientific_upper"_test = [] {
      constexpr auto result = glz::detail::to_printf_fmt("{:.4E}");
      expect(std::string_view(result.data) == "%.4E");
   };

   "to_printf_fmt_basic_general"_test = [] {
      constexpr auto result = glz::detail::to_printf_fmt("{:.6g}");
      expect(std::string_view(result.data) == "%.6g");
   };

   "to_printf_fmt_basic_general_upper"_test = [] {
      constexpr auto result = glz::detail::to_printf_fmt("{:.5G}");
      expect(std::string_view(result.data) == "%.5G");
   };

   "to_printf_fmt_empty_default"_test = [] {
      constexpr auto result = glz::detail::to_printf_fmt("{}");
      expect(std::string_view(result.data) == "%g");
   };

   "to_printf_fmt_colon_only_default"_test = [] {
      constexpr auto result = glz::detail::to_printf_fmt("{:}");
      expect(std::string_view(result.data) == "%g");
   };

   "to_printf_fmt_zero_precision"_test = [] {
      constexpr auto result = glz::detail::to_printf_fmt("{:.0f}");
      expect(std::string_view(result.data) == "%.0f");
   };

   "to_printf_fmt_high_precision"_test = [] {
      constexpr auto result = glz::detail::to_printf_fmt("{:.80f}");
      expect(std::string_view(result.data) == "%.80f");
   };

   "to_printf_fmt_type_only"_test = [] {
      constexpr auto result = glz::detail::to_printf_fmt("{:f}");
      expect(std::string_view(result.data) == "%f");
   };

   // Extra format specs (not JSON-relevant) are skipped
   "to_printf_fmt_ignores_sign"_test = [] {
      constexpr auto result = glz::detail::to_printf_fmt("{:+.2f}");
      expect(std::string_view(result.data) == "%.2f");
   };

   "to_printf_fmt_ignores_width"_test = [] {
      constexpr auto result = glz::detail::to_printf_fmt("{:10.2f}");
      expect(std::string_view(result.data) == "%.2f");
   };

   "to_printf_fmt_ignores_zero_pad"_test = [] {
      constexpr auto result = glz::detail::to_printf_fmt("{:010.2f}");
      expect(std::string_view(result.data) == "%.2f");
   };

   "to_printf_fmt_ignores_align"_test = [] {
      constexpr auto result = glz::detail::to_printf_fmt("{:<10.2f}");
      expect(std::string_view(result.data) == "%.2f");
   };

   "to_printf_fmt_ignores_alternate"_test = [] {
      constexpr auto result = glz::detail::to_printf_fmt("{:#.2f}");
      expect(std::string_view(result.data) == "%.2f");
   };

   // Test string_view overload (used by opts-based float_format)
   "to_printf_fmt_string_view"_test = [] {
      constexpr std::string_view fmt = "{:.3f}";
      constexpr auto result = glz::detail::to_printf_fmt(fmt);
      expect(std::string_view(result.data) == "%.3f");
   };
};

int main() { return 0; }
