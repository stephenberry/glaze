// Glaze Library
// For the license information refer to glaze.hpp

#include <span>

#include "glaze/rpc/repe/buffer.hpp"
#include "ut/ut.hpp"

using namespace ut;

namespace repe = glz::repe;

suite repe_magic_tests = [] {
   "repe_magic_constant"_test = [] {
      expect(repe::repe_magic == 0x1507);
      expect(repe::repe_magic == 5383);
   };

   "header_uses_magic_constant"_test = [] {
      repe::header hdr{};
      expect(hdr.spec == repe::repe_magic);
   };
};

suite finalize_header_tests = [] {
   "finalize_header_basic"_test = [] {
      repe::message msg{};
      msg.query = "/test/path";
      msg.body = R"({"value": 42})";

      repe::finalize_header(msg);

      expect(msg.header.query_length == msg.query.size());
      expect(msg.header.body_length == msg.body.size());
      expect(msg.header.length == sizeof(repe::header) + msg.query.size() + msg.body.size());
   };

   "finalize_header_empty"_test = [] {
      repe::message msg{};

      repe::finalize_header(msg);

      expect(msg.header.query_length == 0u);
      expect(msg.header.body_length == 0u);
      expect(msg.header.length == sizeof(repe::header));
   };
};

suite encode_error_tests = [] {
   "encode_error_simple"_test = [] {
      repe::message msg{};
      msg.body = "existing content";

      repe::encode_error(glz::error_code::parse_error, msg);

      expect(msg.header.ec == glz::error_code::parse_error);
      expect(msg.body.empty());
   };

   "encode_error_with_message"_test = [] {
      repe::message msg{};

      repe::encode_error(glz::error_code::invalid_header, msg, "Custom error message");

      expect(msg.header.ec == glz::error_code::invalid_header);
      expect(msg.body == "Custom error message");
      expect(msg.header.body_length == 20u);
   };

   "encode_error_with_string_view"_test = [] {
      repe::message msg{};
      std::string_view error_msg = "Error from string_view";

      repe::encode_error(glz::error_code::method_not_found, msg, error_msg);

      expect(msg.header.ec == glz::error_code::method_not_found);
      expect(msg.body == "Error from string_view");
   };

   "encode_error_empty_message_ignored"_test = [] {
      repe::message msg{};
      msg.body = "original";

      repe::encode_error(glz::error_code::parse_error, msg, "");

      expect(msg.header.ec == glz::error_code::parse_error);
      expect(msg.body == "original"); // Not changed because empty message
   };
};

suite decode_error_tests = [] {
   "decode_error_with_body"_test = [] {
      repe::message msg{};
      msg.header.ec = glz::error_code::parse_error;
      msg.body = "Error details here";
      msg.header.body_length = msg.body.size();

      auto result = repe::decode_error(msg);

      expect(result.find("REPE error") != std::string::npos);
      expect(result.find("Error details here") != std::string::npos);
   };

   "decode_error_no_body"_test = [] {
      repe::message msg{};
      msg.header.ec = glz::error_code::parse_error;
      msg.header.body_length = 0;

      auto result = repe::decode_error(msg);

      expect(result.find("REPE error") != std::string::npos);
   };

   "decode_error_no_error"_test = [] {
      repe::message msg{};
      msg.header.ec = glz::error_code::none;

      auto result = repe::decode_error(msg);

      expect(result == "no error");
   };
};

suite to_buffer_tests = [] {
   "to_buffer_basic"_test = [] {
      repe::message msg{};
      msg.query = "/api/test";
      msg.body = R"({"data": 123})";
      repe::finalize_header(msg);

      std::string buffer = repe::to_buffer(msg);

      expect(buffer.size() == sizeof(repe::header) + msg.query.size() + msg.body.size());

      // Verify header is at the beginning
      repe::header extracted_header;
      std::memcpy(&extracted_header, buffer.data(), sizeof(repe::header));
      expect(extracted_header.spec == repe::repe_magic);
      expect(extracted_header.query_length == msg.query.size());
      expect(extracted_header.body_length == msg.body.size());
   };

   "to_buffer_existing_buffer"_test = [] {
      repe::message msg{};
      msg.query = "/test";
      msg.body = "body content";
      repe::finalize_header(msg);

      std::string buffer;
      repe::to_buffer(msg, buffer);

      expect(buffer.size() == sizeof(repe::header) + msg.query.size() + msg.body.size());
   };

   "to_buffer_empty_message"_test = [] {
      repe::message msg{};
      repe::finalize_header(msg);

      std::string buffer = repe::to_buffer(msg);

      expect(buffer.size() == sizeof(repe::header));
   };
};

suite from_buffer_tests = [] {
   "from_buffer_basic"_test = [] {
      // Create original message
      repe::message original{};
      original.query = "/api/endpoint";
      original.body = R"({"key": "value"})";
      original.header.id = 12345;
      original.header.body_format = repe::body_format::JSON;
      repe::finalize_header(original);

      // Serialize to buffer
      std::string buffer = repe::to_buffer(original);

      // Deserialize
      repe::message restored{};
      auto ec = repe::from_buffer(buffer, restored);

      expect(ec == glz::error_code::none);
      expect(restored.query == original.query);
      expect(restored.body == original.body);
      expect(restored.header.id == original.header.id);
      expect(restored.header.body_format == original.header.body_format);
   };

   "from_buffer_too_small"_test = [] {
      std::string small_buffer(10, '\0'); // Too small to contain header

      repe::message msg{};
      auto ec = repe::from_buffer(small_buffer, msg);

      expect(ec == glz::error_code::invalid_header);
   };

   "from_buffer_invalid_magic"_test = [] {
      repe::message original{};
      original.query = "/test";
      repe::finalize_header(original);

      std::string buffer = repe::to_buffer(original);

      // Corrupt magic bytes
      buffer[8] = 0xFF;
      buffer[9] = 0xFF;

      repe::message msg{};
      auto ec = repe::from_buffer(buffer, msg);

      expect(ec == glz::error_code::invalid_header);
   };

   "from_buffer_invalid_version"_test = [] {
      repe::message original{};
      original.query = "/test";
      repe::finalize_header(original);

      std::string buffer = repe::to_buffer(original);

      // Corrupt version (byte 10)
      buffer[10] = 99;

      repe::message msg{};
      auto ec = repe::from_buffer(buffer, msg);

      expect(ec == glz::error_code::version_mismatch);
   };

   "from_buffer_truncated_body"_test = [] {
      repe::message original{};
      original.query = "/test";
      original.body = "This is a longer body content";
      repe::finalize_header(original);

      std::string buffer = repe::to_buffer(original);

      // Truncate the buffer
      buffer.resize(sizeof(repe::header) + 5);

      repe::message msg{};
      auto ec = repe::from_buffer(buffer, msg);

      expect(ec == glz::error_code::invalid_body);
   };

   "from_buffer_char_pointer"_test = [] {
      repe::message original{};
      original.query = "/endpoint";
      original.body = "payload";
      repe::finalize_header(original);

      std::string buffer = repe::to_buffer(original);

      repe::message restored{};
      auto ec = repe::from_buffer(buffer.data(), buffer.size(), restored);

      expect(ec == glz::error_code::none);
      expect(restored.query == original.query);
      expect(restored.body == original.body);
   };
};

suite parse_header_tests = [] {
   "parse_header_basic"_test = [] {
      repe::message msg{};
      msg.query = "/test";
      msg.body = "body";
      msg.header.id = 999;
      repe::finalize_header(msg);

      std::string buffer = repe::to_buffer(msg);

      repe::header hdr{};
      auto ec = repe::parse_header(buffer, hdr);

      expect(ec == glz::error_code::none);
      expect(hdr.id == 999u);
      expect(hdr.query_length == 5u);
      expect(hdr.body_length == 4u);
   };

   "parse_header_invalid_magic"_test = [] {
      std::string buffer(sizeof(repe::header), '\0');
      // Don't set valid magic

      repe::header hdr{};
      auto ec = repe::parse_header(buffer, hdr);

      expect(ec == glz::error_code::invalid_header);
   };

   "parse_header_too_small"_test = [] {
      std::string buffer(10, '\0');

      repe::header hdr{};
      auto ec = repe::parse_header(buffer, hdr);

      expect(ec == glz::error_code::invalid_header);
   };
};

suite extract_query_tests = [] {
   "extract_query_basic"_test = [] {
      repe::message msg{};
      msg.query = "/api/v1/users";
      msg.body = "{}";
      repe::finalize_header(msg);

      std::string buffer = repe::to_buffer(msg);

      auto query = repe::extract_query(buffer);

      expect(query == "/api/v1/users");
   };

   "extract_query_empty"_test = [] {
      repe::message msg{};
      msg.query = "";
      repe::finalize_header(msg);

      std::string buffer = repe::to_buffer(msg);

      auto query = repe::extract_query(buffer);

      expect(query.empty());
   };

   "extract_query_invalid_buffer"_test = [] {
      std::string buffer(10, '\0');

      auto query = repe::extract_query(buffer);

      expect(query.empty());
   };

   "extract_query_invalid_magic"_test = [] {
      repe::message msg{};
      msg.query = "/test";
      repe::finalize_header(msg);

      std::string buffer = repe::to_buffer(msg);
      buffer[8] = 0xFF; // Corrupt magic

      auto query = repe::extract_query(buffer);

      expect(query.empty());
   };

   "extract_query_truncated"_test = [] {
      repe::message msg{};
      msg.query = "/very/long/query/path";
      repe::finalize_header(msg);

      std::string buffer = repe::to_buffer(msg);
      // Keep only header + partial query
      buffer.resize(sizeof(repe::header) + 5);

      auto query = repe::extract_query(buffer);

      expect(query.empty()); // Should fail because query is truncated
   };
};

suite roundtrip_tests = [] {
   "roundtrip_json_message"_test = [] {
      repe::message original{};
      original.query = "/api/data";
      original.body = R"({"name":"test","value":42,"nested":{"a":1,"b":2}})";
      original.header.id = 12345;
      original.header.body_format = repe::body_format::JSON;
      original.header.query_format = repe::query_format::JSON_POINTER;
      repe::finalize_header(original);

      std::string wire_data = repe::to_buffer(original);

      repe::message restored{};
      auto ec = repe::from_buffer(wire_data, restored);

      expect(ec == glz::error_code::none);
      expect(restored.query == original.query);
      expect(restored.body == original.body);
      expect(restored.header.id == original.header.id);
      expect(restored.header.body_format == original.header.body_format);
      expect(restored.header.query_format == original.header.query_format);
      expect(restored.header.length == original.header.length);
   };

   "roundtrip_notify_message"_test = [] {
      repe::message original{};
      original.query = "/events/notify";
      original.body = "notification payload";
      original.header.notify = 1;
      repe::finalize_header(original);

      std::string wire_data = repe::to_buffer(original);

      repe::message restored{};
      auto ec = repe::from_buffer(wire_data, restored);

      expect(ec == glz::error_code::none);
      expect(restored.header.notify == 1);
   };

   "roundtrip_error_message"_test = [] {
      repe::message original{};
      original.query = "/api/fail";
      repe::encode_error(glz::error_code::parse_error, original, "Something went wrong");
      repe::finalize_header(original);

      std::string wire_data = repe::to_buffer(original);

      repe::message restored{};
      auto ec = repe::from_buffer(wire_data, restored);

      expect(ec == glz::error_code::none);
      expect(restored.header.ec == glz::error_code::parse_error);
      expect(restored.body == "Something went wrong");
   };
};

struct Point
{
   int x{};
   int y{};

   struct glaze
   {
      using T = Point;
      static constexpr auto value = glz::object(&T::x, &T::y);
   };
};

suite decode_message_tests = [] {
   "decode_message_success"_test = [] {
      repe::message msg{};
      msg.body = R"({"x": 10, "y": 20})";
      msg.header.ec = glz::error_code::none;

      Point pt{};
      auto result = repe::decode_message(pt, msg);

      expect(!result.has_value());
      expect(pt.x == 10);
      expect(pt.y == 20);
   };

   "decode_message_with_error"_test = [] {
      repe::message msg{};
      msg.header.ec = glz::error_code::parse_error;
      msg.body = "Error details";
      msg.header.body_length = msg.body.size();

      int value{};
      auto result = repe::decode_message(value, msg);

      expect(result.has_value());
      expect(result->find("REPE error") != std::string::npos);
   };

   "decode_message_invalid_json"_test = [] {
      repe::message msg{};
      msg.body = "not valid json";
      msg.header.ec = glz::error_code::none;

      int value{};
      auto result = repe::decode_message(value, msg);

      expect(result.has_value()); // Should have error
   };
};

// ============================================================
// Zero-copy helper function tests
// ============================================================

suite is_notify_tests = [] {
   "is_notify_true"_test = [] {
      repe::message msg{};
      msg.header.notify = 1;
      msg.query = "/test";
      repe::finalize_header(msg);

      std::string buffer = repe::to_buffer(msg);
      std::span<const char> span{buffer};

      expect(repe::is_notify(span) == true);
   };

   "is_notify_false"_test = [] {
      repe::message msg{};
      msg.header.notify = 0;
      msg.query = "/test";
      repe::finalize_header(msg);

      std::string buffer = repe::to_buffer(msg);
      std::span<const char> span{buffer};

      expect(repe::is_notify(span) == false);
   };

   "is_notify_too_small"_test = [] {
      std::string small(10, '\0');
      std::span<const char> span{small};

      expect(repe::is_notify(span) == false);
   };
};

suite extract_id_tests = [] {
   "extract_id_basic"_test = [] {
      repe::message msg{};
      msg.header.id = 42;
      msg.query = "/test";
      repe::finalize_header(msg);

      std::string buffer = repe::to_buffer(msg);
      std::span<const char> span{buffer};

      expect(repe::extract_id(span) == 42u);
   };

   "extract_id_large_value"_test = [] {
      repe::message msg{};
      msg.header.id = 0xDEADBEEFCAFE;
      msg.query = "/test";
      repe::finalize_header(msg);

      std::string buffer = repe::to_buffer(msg);
      std::span<const char> span{buffer};

      expect(repe::extract_id(span) == 0xDEADBEEFCAFEu);
   };

   "extract_id_too_small"_test = [] {
      std::string small(10, '\0');
      std::span<const char> span{small};

      expect(repe::extract_id(span) == 0u);
   };
};

suite extract_query_span_tests = [] {
   "extract_query_span_basic"_test = [] {
      repe::message msg{};
      msg.query = "/api/v1/users";
      msg.body = "{}";
      repe::finalize_header(msg);

      std::string buffer = repe::to_buffer(msg);

      // Use the pointer/size overload for span-like access
      auto query = repe::extract_query(buffer.data(), buffer.size());

      expect(query == "/api/v1/users");
   };
};

suite validate_header_only_tests = [] {
   "validate_header_only_valid"_test = [] {
      repe::message msg{};
      msg.query = "/test";
      repe::finalize_header(msg);

      std::string buffer = repe::to_buffer(msg);
      std::span<const char> span{buffer};

      auto ec = repe::validate_header_only(span);

      expect(ec == glz::error_code::none);
   };

   "validate_header_only_too_small"_test = [] {
      std::string small(10, '\0');
      std::span<const char> span{small};

      auto ec = repe::validate_header_only(span);

      expect(ec == glz::error_code::invalid_header);
   };

   "validate_header_only_invalid_magic"_test = [] {
      repe::message msg{};
      msg.query = "/test";
      repe::finalize_header(msg);

      std::string buffer = repe::to_buffer(msg);
      buffer[8] = 0xFF; // Corrupt magic byte
      buffer[9] = 0xFF;
      std::span<const char> span{buffer};

      auto ec = repe::validate_header_only(span);

      expect(ec == glz::error_code::invalid_header);
   };

   "validate_header_only_invalid_version"_test = [] {
      repe::message msg{};
      msg.query = "/test";
      repe::finalize_header(msg);

      std::string buffer = repe::to_buffer(msg);
      buffer[10] = 99; // Invalid version
      std::span<const char> span{buffer};

      auto ec = repe::validate_header_only(span);

      expect(ec == glz::error_code::version_mismatch);
   };
};

suite encode_error_buffer_tests = [] {
   "encode_error_buffer_basic"_test = [] {
      std::string buffer;
      repe::encode_error_buffer(glz::error_code::parse_error, buffer, "Test error message", 123);

      expect(buffer.size() == sizeof(repe::header) + 18u); // "Test error message" is 18 chars

      // Verify we can parse it back
      repe::message msg{};
      auto ec = repe::from_buffer(buffer, msg);

      expect(ec == glz::error_code::none);
      expect(msg.header.ec == glz::error_code::parse_error);
      expect(msg.header.id == 123u);
      expect(msg.body == "Test error message");
   };

   "encode_error_buffer_empty_message"_test = [] {
      std::string buffer;
      repe::encode_error_buffer(glz::error_code::invalid_header, buffer, "");

      expect(buffer.size() == sizeof(repe::header));

      repe::message msg{};
      auto ec = repe::from_buffer(buffer, msg);

      expect(ec == glz::error_code::none);
      expect(msg.header.ec == glz::error_code::invalid_header);
      expect(msg.body.empty());
   };

   "encode_error_buffer_string_view"_test = [] {
      std::string buffer;
      std::string_view error_msg = "Error from string_view";
      repe::encode_error_buffer(glz::error_code::method_not_found, buffer, error_msg, 456);

      repe::message msg{};
      auto ec = repe::from_buffer(buffer, msg);

      expect(ec == glz::error_code::none);
      expect(msg.header.ec == glz::error_code::method_not_found);
      expect(msg.header.id == 456u);
      expect(msg.body == "Error from string_view");
   };
};

suite make_error_response_tests = [] {
   "make_error_response_basic"_test = [] {
      std::string buffer = repe::make_error_response(glz::error_code::connection_failure, "Connection failed", 789);

      repe::message msg{};
      auto ec = repe::from_buffer(buffer, msg);

      expect(ec == glz::error_code::none);
      expect(msg.header.ec == glz::error_code::connection_failure);
      expect(msg.header.id == 789u);
      expect(msg.body == "Connection failed");
   };

   "make_error_response_default_id"_test = [] {
      std::string buffer = repe::make_error_response(glz::error_code::timeout, "Timeout occurred");

      repe::message msg{};
      auto ec = repe::from_buffer(buffer, msg);

      expect(ec == glz::error_code::none);
      expect(msg.header.ec == glz::error_code::timeout);
      expect(msg.header.id == 0u);
      expect(msg.body == "Timeout occurred");
   };
};

suite from_buffer_span_tests = [] {
   "from_buffer_span_basic"_test = [] {
      repe::message original{};
      original.query = "/api/endpoint";
      original.body = R"({"key": "value"})";
      original.header.id = 12345;
      repe::finalize_header(original);

      std::string buffer = repe::to_buffer(original);

      repe::message restored{};
      // Use the pointer/size overload for span-like access
      auto ec = repe::from_buffer(buffer.data(), buffer.size(), restored);

      expect(ec == glz::error_code::none);
      expect(restored.query == original.query);
      expect(restored.body == original.body);
      expect(restored.header.id == original.header.id);
   };
};

int main() { return 0; }
