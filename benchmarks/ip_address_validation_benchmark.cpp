#include <array>
#include <bencher/bar_chart.hpp>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <string_view>

#include "bencher/bencher.hpp"
#include "bencher/diagnostics.hpp"
#include "glaze/net/http_server.hpp"

#if defined(GLZ_USING_BOOST_ASIO)
namespace asio
{
   using namespace boost::asio;
   using error_code = boost::system::error_code;
}
#endif

struct address_case
{
   std::string_view input;
   bool valid{};
};

constexpr std::array ipv4_valid_cases{
   address_case{"0.0.0.0", true},   address_case{"1.2.3.4", true},     address_case{"10.0.0.1", true},
   address_case{"127.0.0.1", true}, address_case{"192.168.0.1", true}, address_case{"255.255.255.255", true},
};

constexpr std::array ipv4_invalid_cases{
   address_case{"", false},          address_case{"1.2.3.4.5", false}, address_case{"256.1.2.3", false},
   address_case{"1.2.3.999", false}, address_case{"1.2.3.a", false},   address_case{"1..2.3", false},
   address_case{"1.2.3.", false},    address_case{"1.2.3.-1", false},
};

constexpr std::array ipv4_mixed_cases{
   address_case{"8.8.8.8", true},         address_case{"172.16.254.1", true},   address_case{"203.0.113.42", true},
   address_case{"255.255.255.255", true}, address_case{"256.100.50.25", false}, address_case{"192.168.1.256", false},
   address_case{"192.168..1", false},     address_case{"192.168.1.1 ", false},
};

constexpr std::array ipv6_valid_cases{
   address_case{"::", true},
   address_case{"::1", true},
   address_case{"2001:db8::1", true},
   address_case{"fe80::1234:5678:9abc:def0", true},
   address_case{"2001:db8:85a3:0:0:8a2e:370:7334", true},
   address_case{"::ffff:192.0.2.128", true},
};

constexpr std::array ipv6_invalid_cases{
   address_case{"", false},
   address_case{":", false},
   address_case{"2001:db8:::1", false},
   address_case{"2001:db8::1::1", false},
   address_case{"2001:db8:85a3::8a2e:37023:7334", false},
   address_case{"2001:db8::g", false},
   address_case{"1:2:3:4:5:6:7", false},
   address_case{"::ffff:999.0.2.128", false},
};

constexpr std::array ipv6_mixed_cases{
   address_case{"::1", true},
   address_case{"2001:db8::dead:beef", true},
   address_case{"2001:db8:0:0:0:0:2:1", true},
   address_case{"::ffff:203.0.113.9", true},
   address_case{"2001:db8::1::2", false},
   address_case{"2001:db8:85a3:0:0:8a2e:370:7334:1234", false},
   address_case{"fe80::1::", false},
   address_case{"::ffff:192.168.1.256", false},
};

bool glaze_validate_ipv4(const std::string_view input) noexcept { return glz::detail::validate_ipv4_address(input); }

bool glaze_validate_ipv6(const std::string_view input) noexcept { return glz::detail::validate_ipv6_address(input); }

bool asio_validate_ipv4(const std::string_view input) noexcept
{
   asio::error_code ec{};
   const auto address = asio::ip::make_address_v4(input, ec);
   bencher::do_not_optimize(address);
   return !ec;
}

bool asio_validate_ipv6(const std::string_view input) noexcept
{
   asio::error_code ec{};
   const auto address = asio::ip::make_address_v6(input, ec);
   bencher::do_not_optimize(address);
   return !ec;
}

template <size_t N, typename Validator>
size_t run_case_set(const std::array<address_case, N>& cases, Validator&& validator, const size_t repetitions)
{
   size_t bytes_processed = 0;
   size_t valid_count = 0;

   for (size_t i = 0; i < repetitions; ++i) {
      for (const auto& test_case : cases) {
         const bool valid = validator(test_case.input);
         bencher::do_not_optimize(valid);
         bytes_processed += test_case.input.size();
         valid_count += valid;
      }
   }

   bencher::do_not_optimize(valid_count);
   return bytes_processed;
}

template <size_t N, typename Validator>
void verify_case_set(const char* validator_name, const char* case_set_name, const std::array<address_case, N>& cases,
                     Validator&& validator)
{
   for (const auto& test_case : cases) {
      const bool actual = validator(test_case.input);
      if (actual != test_case.valid) {
         std::fprintf(stderr, "%s returned %s for %s case %.*s, expected %s\n", validator_name,
                      actual ? "valid" : "invalid", case_set_name, int(test_case.input.size()), test_case.input.data(),
                      test_case.valid ? "valid" : "invalid");
         std::abort();
      }
   }
}

template <size_t N, typename GlazeValidator, typename AsioValidator>
void bench_case_set(const char* stage_name, const char* glaze_name, const char* asio_name,
                    const std::array<address_case, N>& cases, GlazeValidator&& glaze_validator,
                    AsioValidator&& asio_validator)
{
   constexpr size_t repetitions = 4096;

   bencher::stage stage{stage_name};
   stage.min_execution_count = 100;
   stage.cold_cache = false;

   stage.run(glaze_name, [&] { return run_case_set(cases, glaze_validator, repetitions); });
   stage.run(asio_name, [&] { return run_case_set(cases, asio_validator, repetitions); });

   bencher::print_results(stage);
}

int main()
{
   verify_case_set("glz::detail::validate_ipv4_address", "IPv4 valid", ipv4_valid_cases, glaze_validate_ipv4);
   verify_case_set("asio::ip::make_address_v4", "IPv4 valid", ipv4_valid_cases, asio_validate_ipv4);
   verify_case_set("glz::detail::validate_ipv4_address", "IPv4 invalid", ipv4_invalid_cases, glaze_validate_ipv4);
   verify_case_set("asio::ip::make_address_v4", "IPv4 invalid", ipv4_invalid_cases, asio_validate_ipv4);
   verify_case_set("glz::detail::validate_ipv4_address", "IPv4 mixed", ipv4_mixed_cases, glaze_validate_ipv4);
   verify_case_set("asio::ip::make_address_v4", "IPv4 mixed", ipv4_mixed_cases, asio_validate_ipv4);

   verify_case_set("glz::detail::validate_ipv6_address", "IPv6 valid", ipv6_valid_cases, glaze_validate_ipv6);
   verify_case_set("asio::ip::make_address_v6", "IPv6 valid", ipv6_valid_cases, asio_validate_ipv6);
   verify_case_set("glz::detail::validate_ipv6_address", "IPv6 invalid", ipv6_invalid_cases, glaze_validate_ipv6);
   verify_case_set("asio::ip::make_address_v6", "IPv6 invalid", ipv6_invalid_cases, asio_validate_ipv6);
   verify_case_set("glz::detail::validate_ipv6_address", "IPv6 mixed", ipv6_mixed_cases, glaze_validate_ipv6);
   verify_case_set("asio::ip::make_address_v6", "IPv6 mixed", ipv6_mixed_cases, asio_validate_ipv6);

   std::printf("=== IPv4 Address Validation Benchmarks ===\n\n");
   bench_case_set("IPv4 valid literals", "glz::detail::validate_ipv4_address", "asio::ip::make_address_v4",
                  ipv4_valid_cases, glaze_validate_ipv4, asio_validate_ipv4);
   bench_case_set("IPv4 invalid literals", "glz::detail::validate_ipv4_address", "asio::ip::make_address_v4",
                  ipv4_invalid_cases, glaze_validate_ipv4, asio_validate_ipv4);
   bench_case_set("IPv4 mixed literals", "glz::detail::validate_ipv4_address", "asio::ip::make_address_v4",
                  ipv4_mixed_cases, glaze_validate_ipv4, asio_validate_ipv4);

   std::printf("\n=== IPv6 Address Validation Benchmarks ===\n\n");
   bench_case_set("IPv6 valid literals", "glz::detail::validate_ipv6_address", "asio::ip::make_address_v6",
                  ipv6_valid_cases, glaze_validate_ipv6, asio_validate_ipv6);
   bench_case_set("IPv6 invalid literals", "glz::detail::validate_ipv6_address", "asio::ip::make_address_v6",
                  ipv6_invalid_cases, glaze_validate_ipv6, asio_validate_ipv6);
   bench_case_set("IPv6 mixed literals", "glz::detail::validate_ipv6_address", "asio::ip::make_address_v6",
                  ipv6_mixed_cases, glaze_validate_ipv6, asio_validate_ipv6);

   return 0;
}
