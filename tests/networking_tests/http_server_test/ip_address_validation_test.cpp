#include <string_view>
#include <ut/ut.hpp>

#include "glaze/net/http_server.hpp"

using namespace ut;
using glz::detail::is_valid_ipv4_address;
using glz::detail::is_valid_ipv6_address;

suite http_server_ipv4_address_validation = [] {
   "rejects invalid IPv4 literals"_test = [] {
      expect(not is_valid_ipv4_address(""));
      expect(not is_valid_ipv4_address("256.0.0.1"));
      expect(not is_valid_ipv4_address("1.2.3.256"));
      expect(not is_valid_ipv4_address("999.0.0.1"));
      expect(not is_valid_ipv4_address("1.2.3.4:80"));
      expect(not is_valid_ipv4_address("1.2.3"));
      expect(not is_valid_ipv4_address("1.2.3.4.5"));
      expect(not is_valid_ipv4_address("1..2.3"));
      expect(not is_valid_ipv4_address("1.2..4"));
      expect(not is_valid_ipv4_address(".1.2.3.4"));
      expect(not is_valid_ipv4_address("1.2.3.4."));
      expect(not is_valid_ipv4_address("01.2.3.4"));
      expect(not is_valid_ipv4_address("00.2.3.4"));
      expect(not is_valid_ipv4_address("1.02.3.4"));
      expect(not is_valid_ipv4_address("1.2.003.4"));
      expect(not is_valid_ipv4_address("001.2.3.4"));
      expect(not is_valid_ipv4_address("1.2.3.04"));
      expect(not is_valid_ipv4_address("1.2.3.004"));
      expect(not is_valid_ipv4_address("1.2.3.1234"));
      expect(not is_valid_ipv4_address("1.2.3.-4"));
      expect(not is_valid_ipv4_address("+1.2.3.4"));
      expect(not is_valid_ipv4_address("1.+2.3.4"));
      expect(not is_valid_ipv4_address(" 1.2.3.4"));
      expect(not is_valid_ipv4_address("1.2.3.4 "));
      expect(not is_valid_ipv4_address("\t1.2.3.4"));
      expect(not is_valid_ipv4_address("1.2.3.4\t"));
      expect(not is_valid_ipv4_address("1.2.3.4\n"));
      expect(not is_valid_ipv4_address("b1.2.3.4"));
      expect(not is_valid_ipv4_address("a.e.r.o"));
      expect(not is_valid_ipv4_address("1.2.a.4"));
      expect(not is_valid_ipv4_address("1.2.3.4/32"));
      expect(not is_valid_ipv4_address("[1.2.3.4]"));
      expect(not is_valid_ipv4_address(std::string_view{"1.2.3.4\0", 8}));
   };

   "rejects legacy IPv4 forms"_test = [] {
      expect(not is_valid_ipv4_address("127.1"));
      expect(not is_valid_ipv4_address("127.0.1"));
      expect(not is_valid_ipv4_address("2130706433"));
      expect(not is_valid_ipv4_address("0177.0.0.1"));
      expect(not is_valid_ipv4_address("127.000.000.001"));
      expect(not is_valid_ipv4_address("0x7f.0.0.1"));
      expect(not is_valid_ipv4_address("0x7f000001"));
   };

   "accepts valid IPv4 literals"_test = [] {
      expect(is_valid_ipv4_address("0.0.0.0"));
      expect(is_valid_ipv4_address("9.10.99.100"));
      expect(is_valid_ipv4_address("127.0.0.1"));
      expect(is_valid_ipv4_address("1.2.3.4"));
      expect(is_valid_ipv4_address("192.168.0.1"));
      expect(is_valid_ipv4_address("199.200.249.250"));
      expect(is_valid_ipv4_address("255.255.255.255"));
   };
};

suite http_server_ipv6_address_validation = [] {
   "rejects invalid IPv6 literals"_test = [] {
      expect(not is_valid_ipv6_address(""));
      expect(not is_valid_ipv6_address(":"));
      expect(not is_valid_ipv6_address(":::"));
      expect(not is_valid_ipv6_address(":1"));
      expect(not is_valid_ipv6_address("1:"));
      expect(not is_valid_ipv6_address("1:::2"));
      expect(not is_valid_ipv6_address("1::2::3"));
      expect(not is_valid_ipv6_address("1:2:3:4:5:6"));
      expect(not is_valid_ipv6_address("1:2:3:4:5:6:7"));
      expect(not is_valid_ipv6_address("1:2:3:4:5:6:7:8:9"));
      expect(not is_valid_ipv6_address("1:2:3:4:5:6:7:8::"));
      expect(not is_valid_ipv6_address("::1:2:3:4:5:6:7:8"));
      expect(not is_valid_ipv6_address("1:2:3:4::5:6:7:8"));
      expect(not is_valid_ipv6_address("1:2:3:4:5::6:7:8"));
      expect(not is_valid_ipv6_address("1:2:3:4:5:6:7::8"));
      expect(not is_valid_ipv6_address("12345::1"));
      expect(not is_valid_ipv6_address("2001:db8:0:0:0:0:0:00000"));
      expect(not is_valid_ipv6_address("gggg::1"));
      expect(not is_valid_ipv6_address("2001:db8::-1"));
      expect(not is_valid_ipv6_address("2001:db8::+1"));
      expect(not is_valid_ipv6_address("+2001:db8::1"));
      expect(not is_valid_ipv6_address("2001:db8::0x1"));
      expect(not is_valid_ipv6_address("2001:db8::1/64"));
      expect(not is_valid_ipv6_address("2001:db8::1%eth0"));
      expect(not is_valid_ipv6_address("2001:db8::1%25eth0"));
      expect(not is_valid_ipv6_address("2001:db8::1?x"));
      expect(not is_valid_ipv6_address("2001:db8::1#x"));
      expect(not is_valid_ipv6_address(" 2001:db8::1"));
      expect(not is_valid_ipv6_address("2001:db8::1 "));
      expect(not is_valid_ipv6_address("\t2001:db8::1"));
      expect(not is_valid_ipv6_address("2001:db8::1\t"));
      expect(not is_valid_ipv6_address("2001:db8::1\n"));
      expect(not is_valid_ipv6_address("[2001:db8::1]"));
      expect(not is_valid_ipv6_address("[2001:db8::1]:443"));
      expect(not is_valid_ipv6_address(std::string_view{"2001:db8::1\0", 12}));
   };

   "accepts valid IPv6 literals without compression"_test = [] {
      expect(is_valid_ipv6_address("0:0:0:0:0:0:0:0"));
      expect(is_valid_ipv6_address("0:0:0:0:0:0:0:1"));
      expect(is_valid_ipv6_address("1:2:3:4:5:6:7:8"));
      expect(is_valid_ipv6_address("2001:db8:0:0:0:0:0:1"));
      expect(is_valid_ipv6_address("2001:0DB8:0000:0000:0000:0000:0000:0001"));
      expect(is_valid_ipv6_address("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff"));
   };

   "accepts valid IPv6 literals with compression"_test = [] {
      expect(is_valid_ipv6_address("::"));
      expect(is_valid_ipv6_address("::1"));
      expect(is_valid_ipv6_address("1::"));
      expect(is_valid_ipv6_address("FE80::1"));
      expect(is_valid_ipv6_address("2001::0"));
      expect(is_valid_ipv6_address("2001:db8::"));
      expect(is_valid_ipv6_address("2001:db8::1"));
      expect(is_valid_ipv6_address("2001:db8::ff00:42:8329"));
      expect(is_valid_ipv6_address("1:2:3:4::6:7:8"));
      expect(is_valid_ipv6_address("1:2:3:4:5:6:7::"));
      expect(is_valid_ipv6_address("2001:db8::1:80"));
   };

   "rejects invalid IPv6 literals with IPv4 tail"_test = [] {
      expect(not is_valid_ipv6_address("192.0.2.1"));
      expect(not is_valid_ipv6_address("::ffff:999.0.2.1"));
      expect(not is_valid_ipv6_address("::ffff:192.0.2"));
      expect(not is_valid_ipv6_address("::ffff:192.0.2.1.5"));
      expect(not is_valid_ipv6_address("::ffff:192.168.001.1"));
      expect(not is_valid_ipv6_address("::ffff:192.0.2.01"));
      expect(not is_valid_ipv6_address("::ffff:0x7f.0.0.1"));
      expect(not is_valid_ipv6_address("::ffff:192.0.2.1."));
      expect(not is_valid_ipv6_address("::ffff:192.0.2.1:80"));
      expect(not is_valid_ipv6_address("::ffff:192.0.2.-1"));
      expect(not is_valid_ipv6_address("::ffff:+192.0.2.1"));
      expect(not is_valid_ipv6_address("::ffff:192.0.2.1 "));
      expect(not is_valid_ipv6_address("192.0.2.1::"));
      expect(not is_valid_ipv6_address("2001:db8:192.0.2.1:1"));
      expect(not is_valid_ipv6_address("1:2:3:4:5:192.0.2.1"));
      expect(not is_valid_ipv6_address("0:0:0:0:0:0:0:192.0.2.1"));
      expect(not is_valid_ipv6_address("1:2:3:4:5:6::192.0.2.1"));
      expect(not is_valid_ipv6_address("::1:2:3:4:5:6:192.0.2.1"));
      expect(not is_valid_ipv6_address("1:2:3:4:5::6:192.0.2.1"));
   };

   "accepts valid IPv6 literals with IPv4 tail"_test = [] {
      expect(is_valid_ipv6_address("::0.0.0.0"));
      expect(is_valid_ipv6_address("::192.0.2.1"));
      expect(is_valid_ipv6_address("::255.255.255.255"));
      expect(is_valid_ipv6_address("::ffff:192.0.2.1"));
      expect(is_valid_ipv6_address("2001:db8::192.0.2.1"));
      expect(is_valid_ipv6_address("0:0:0:0:0:0:192.0.2.1"));
      expect(is_valid_ipv6_address("1:2:3:4:5:6:192.0.2.1"));
      expect(is_valid_ipv6_address("1:2:3:4:5::192.0.2.1"));
      expect(is_valid_ipv6_address("ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255"));
   };
};

int main() { return 0; }
