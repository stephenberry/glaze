// Glaze Library
// For the license information refer to glaze.hpp

#include <array>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "glaze/bson.hpp"
#include "glaze/cbor.hpp"
#include "glaze/core/istream_buffer.hpp"
#include "glaze/glaze.hpp"
#include "glaze/jsonb.hpp"
#include "glaze/msgpack.hpp"
#include "glaze/toml.hpp"
#include "glaze/yaml.hpp"
#include "ut/ut.hpp"

using namespace ut;

// ============================================================================
// Sparse enum tests (Issues #2246 and #2262)
// ============================================================================

enum class SparseEnum : int { Zero = 0, FourHundredMillion = 400000000 };

template <>
struct glz::meta<SparseEnum>
{
   using enum SparseEnum;
   static constexpr auto value = enumerate(Zero, FourHundredMillion);
};

enum class SparseEnumMillions : int { A = 0, B = 1000000, C = 2000000 };

template <>
struct glz::meta<SparseEnumMillions>
{
   using enum SparseEnumMillions;
   static constexpr auto value = enumerate(A, B, C);
};

enum class SparseEnumPow2 : int { X = 1, Y = 1024, Z = 65536 };

template <>
struct glz::meta<SparseEnumPow2>
{
   using enum SparseEnumPow2;
   static constexpr auto value = enumerate(X, Y, Z);
};

// Issue #2262: Sparse enum with adjacent values where shift doesn't help
enum class SparseEnumXor : uint32_t { no_error = 0, invalid_version = 400000000, unsupported_version = 400000001 };

template <>
struct glz::meta<SparseEnumXor>
{
   using enum SparseEnumXor;
   static constexpr auto value =
      enumerate("NO_ERROR", no_error, "INVALID_VERSION", invalid_version, "UNSUPPORTED_VERSION", unsupported_version);
};

// Test struct for sparse enum in object
struct SparseEnumTestStruct
{
   SparseEnum e1{SparseEnum::Zero};
   SparseEnum e2{SparseEnum::FourHundredMillion};
};

suite sparse_enum_tests = [] {
   "sparse_enum_serialization"_test = [] {
      SparseEnum e = SparseEnum::Zero;
      std::string json;
      expect(not glz::write_json(e, json));
      expect(json == "\"Zero\"") << json;

      e = SparseEnum::FourHundredMillion;
      json.clear();
      expect(not glz::write_json(e, json));
      expect(json == "\"FourHundredMillion\"") << json;
   };

   "sparse_enum_deserialization"_test = [] {
      SparseEnum e{};
      expect(not glz::read_json(e, R"("Zero")"));
      expect(e == SparseEnum::Zero);

      expect(not glz::read_json(e, R"("FourHundredMillion")"));
      expect(e == SparseEnum::FourHundredMillion);
   };

   "sparse_enum_roundtrip"_test = [] {
      for (auto val : {SparseEnum::Zero, SparseEnum::FourHundredMillion}) {
         std::string json;
         expect(not glz::write_json(val, json));

         SparseEnum parsed{};
         expect(not glz::read_json(parsed, json));
         expect(parsed == val);
      }
   };

   "sparse_enum_millions_roundtrip"_test = [] {
      for (auto val : {SparseEnumMillions::A, SparseEnumMillions::B, SparseEnumMillions::C}) {
         std::string json;
         expect(not glz::write_json(val, json));

         SparseEnumMillions parsed{};
         expect(not glz::read_json(parsed, json));
         expect(parsed == val);
      }
   };

   "sparse_enum_pow2_roundtrip"_test = [] {
      for (auto val : {SparseEnumPow2::X, SparseEnumPow2::Y, SparseEnumPow2::Z}) {
         std::string json;
         expect(not glz::write_json(val, json));

         SparseEnumPow2 parsed{};
         expect(not glz::read_json(parsed, json));
         expect(parsed == val);
      }
   };

   "sparse_enum_get_name"_test = [] {
      expect(glz::get_enum_name(SparseEnum::Zero) == "Zero");
      expect(glz::get_enum_name(SparseEnum::FourHundredMillion) == "FourHundredMillion");
      expect(glz::get_enum_name(SparseEnumMillions::A) == "A");
      expect(glz::get_enum_name(SparseEnumMillions::B) == "B");
      expect(glz::get_enum_name(SparseEnumMillions::C) == "C");
   };

   "sparse_enum_invalid_value"_test = [] {
      auto invalid = static_cast<SparseEnum>(12345);
      expect(glz::get_enum_name(invalid).empty());
   };

   // Issue #2262 tests
   "sparse_enum_xor_serialization"_test = [] {
      SparseEnumXor e = SparseEnumXor::no_error;
      std::string json;
      expect(not glz::write_json(e, json));
      expect(json == "\"NO_ERROR\"") << json;

      e = SparseEnumXor::invalid_version;
      json.clear();
      expect(not glz::write_json(e, json));
      expect(json == "\"INVALID_VERSION\"") << json;

      e = SparseEnumXor::unsupported_version;
      json.clear();
      expect(not glz::write_json(e, json));
      expect(json == "\"UNSUPPORTED_VERSION\"") << json;
   };

   "sparse_enum_xor_roundtrip"_test = [] {
      for (auto val : {SparseEnumXor::no_error, SparseEnumXor::invalid_version, SparseEnumXor::unsupported_version}) {
         std::string json;
         expect(not glz::write_json(val, json));

         SparseEnumXor parsed{};
         expect(not glz::read_json(parsed, json));
         expect(parsed == val);
      }
   };

   "sparse_enum_xor_deserialization"_test = [] {
      SparseEnumXor e{};
      expect(not glz::read_json(e, R"("NO_ERROR")"));
      expect(e == SparseEnumXor::no_error);

      expect(not glz::read_json(e, R"("INVALID_VERSION")"));
      expect(e == SparseEnumXor::invalid_version);

      expect(not glz::read_json(e, R"("UNSUPPORTED_VERSION")"));
      expect(e == SparseEnumXor::unsupported_version);
   };

   "sparse_enum_xor_get_name"_test = [] {
      expect(glz::get_enum_name(SparseEnumXor::no_error) == "NO_ERROR");
      expect(glz::get_enum_name(SparseEnumXor::invalid_version) == "INVALID_VERSION");
      expect(glz::get_enum_name(SparseEnumXor::unsupported_version) == "UNSUPPORTED_VERSION");
   };

   "sparse_enum_in_struct"_test = [] {
      SparseEnumTestStruct obj;
      std::string json;
      expect(not glz::write_json(obj, json));
      expect(json == R"({"e1":"Zero","e2":"FourHundredMillion"})") << json;

      SparseEnumTestStruct parsed{};
      expect(not glz::read_json(parsed, json));
      expect(parsed.e1 == SparseEnum::Zero);
      expect(parsed.e2 == SparseEnum::FourHundredMillion);
   };
};

// ============================================================================
// Two-element enum tests (N==2 optimization)
// Tests the two_element hash type that avoids hash table for 2-value enums
// ============================================================================

// Two-element enum with small int values
enum class TwoElementSmall : int { First = 0, Second = 100 };

template <>
struct glz::meta<TwoElementSmall>
{
   using enum TwoElementSmall;
   static constexpr auto value = enumerate(First, Second);
};

// Two-element enum with large uint64_t values (tests uint64_t casting)
enum class TwoElementLargeU64 : uint64_t { Low = 0, High = 0xFFFFFFFFFFFFFFFFull };

template <>
struct glz::meta<TwoElementLargeU64>
{
   using enum TwoElementLargeU64;
   static constexpr auto value = enumerate(Low, High);
};

// Two-element enum with negative int64_t values
enum class TwoElementNegative : int64_t { Negative = -9223372036854775807ll, Positive = 9223372036854775807ll };

template <>
struct glz::meta<TwoElementNegative>
{
   using enum TwoElementNegative;
   static constexpr auto value = enumerate(Negative, Positive);
};

// Two-element enum with sequential values (0, 1)
enum class TwoElementSequential : int { Zero = 0, One = 1 };

template <>
struct glz::meta<TwoElementSequential>
{
   using enum TwoElementSequential;
   static constexpr auto value = enumerate(Zero, One);
};

// Two-element enum with custom names
enum class TwoElementNamed : int { Off = 0, On = 1 };

template <>
struct glz::meta<TwoElementNamed>
{
   using enum TwoElementNamed;
   static constexpr auto value = enumerate("OFF", Off, "ON", On);
};

suite two_element_enum_tests = [] {
   "two_element_small_serialization"_test = [] {
      TwoElementSmall e = TwoElementSmall::First;
      std::string json;
      expect(not glz::write_json(e, json));
      expect(json == "\"First\"") << json;

      e = TwoElementSmall::Second;
      json.clear();
      expect(not glz::write_json(e, json));
      expect(json == "\"Second\"") << json;
   };

   "two_element_small_deserialization"_test = [] {
      TwoElementSmall e{};
      expect(not glz::read_json(e, R"("First")"));
      expect(e == TwoElementSmall::First);

      expect(not glz::read_json(e, R"("Second")"));
      expect(e == TwoElementSmall::Second);
   };

   "two_element_small_roundtrip"_test = [] {
      for (auto val : {TwoElementSmall::First, TwoElementSmall::Second}) {
         std::string json;
         expect(not glz::write_json(val, json));

         TwoElementSmall parsed{};
         expect(not glz::read_json(parsed, json));
         expect(parsed == val);
      }
   };

   "two_element_large_u64_roundtrip"_test = [] {
      // Test with max uint64_t value to verify casting works correctly
      for (auto val : {TwoElementLargeU64::Low, TwoElementLargeU64::High}) {
         std::string json;
         expect(not glz::write_json(val, json));

         TwoElementLargeU64 parsed{};
         expect(not glz::read_json(parsed, json));
         expect(parsed == val);
      }
   };

   "two_element_negative_roundtrip"_test = [] {
      // Test with extreme int64_t values (near min/max)
      for (auto val : {TwoElementNegative::Negative, TwoElementNegative::Positive}) {
         std::string json;
         expect(not glz::write_json(val, json));

         TwoElementNegative parsed{};
         expect(not glz::read_json(parsed, json));
         expect(parsed == val);
      }
   };

   "two_element_sequential_roundtrip"_test = [] {
      for (auto val : {TwoElementSequential::Zero, TwoElementSequential::One}) {
         std::string json;
         expect(not glz::write_json(val, json));

         TwoElementSequential parsed{};
         expect(not glz::read_json(parsed, json));
         expect(parsed == val);
      }
   };

   "two_element_named_serialization"_test = [] {
      TwoElementNamed e = TwoElementNamed::Off;
      std::string json;
      expect(not glz::write_json(e, json));
      expect(json == "\"OFF\"") << json;

      e = TwoElementNamed::On;
      json.clear();
      expect(not glz::write_json(e, json));
      expect(json == "\"ON\"") << json;
   };

   "two_element_named_deserialization"_test = [] {
      TwoElementNamed e{};
      expect(not glz::read_json(e, R"("OFF")"));
      expect(e == TwoElementNamed::Off);

      expect(not glz::read_json(e, R"("ON")"));
      expect(e == TwoElementNamed::On);
   };

   "two_element_get_name"_test = [] {
      expect(glz::get_enum_name(TwoElementSmall::First) == "First");
      expect(glz::get_enum_name(TwoElementSmall::Second) == "Second");
      expect(glz::get_enum_name(TwoElementLargeU64::Low) == "Low");
      expect(glz::get_enum_name(TwoElementLargeU64::High) == "High");
      expect(glz::get_enum_name(TwoElementNegative::Negative) == "Negative");
      expect(glz::get_enum_name(TwoElementNegative::Positive) == "Positive");
      expect(glz::get_enum_name(TwoElementNamed::Off) == "OFF");
      expect(glz::get_enum_name(TwoElementNamed::On) == "ON");
   };

   "two_element_invalid_value"_test = [] {
      // Test that invalid values return empty name
      auto invalid = static_cast<TwoElementSmall>(999);
      expect(glz::get_enum_name(invalid).empty());

      auto invalid2 = static_cast<TwoElementLargeU64>(123);
      expect(glz::get_enum_name(invalid2).empty());
   };
};

// ============================================================================
// Random enum hash stress tests
// Tests that the hash algorithm finds seeds for various enum configurations
// ============================================================================

// uint8_t enums (should use small_range strategy since range <= 256)
enum class RandomU8Enum1 : uint8_t {
   v0 = 1,
   v1 = 6,
   v2 = 7,
   v3 = 8,
   v4 = 11,
   v5 = 20,
   v6 = 22,
   v7 = 23,
   v8 = 24,
   v9 = 26,
   v10 = 31,
   v11 = 35,
   v12 = 39,
   v13 = 40,
   v14 = 50,
   v15 = 55,
   v16 = 56,
   v17 = 57,
   v18 = 59,
   v19 = 62,
   v20 = 67,
   v21 = 70,
   v22 = 71,
   v23 = 86,
   v24 = 87,
   v25 = 88,
   v26 = 91,
   v27 = 96,
   v28 = 97,
   v29 = 107,
   v30 = 108,
   v31 = 114,
   v32 = 117,
   v33 = 129,
   v34 = 137,
   v35 = 139,
   v36 = 141,
   v37 = 143,
   v38 = 150,
   v39 = 151,
   v40 = 154,
   v41 = 166,
   v42 = 173,
   v43 = 178,
   v44 = 179,
   v45 = 183,
   v46 = 186,
   v47 = 188,
   v48 = 189,
   v49 = 194,
   v50 = 195,
   v51 = 206,
   v52 = 207,
   v53 = 221,
   v54 = 228,
   v55 = 233,
   v56 = 236,
   v57 = 237,
   v58 = 240,
   v59 = 243,
   v60 = 247,
   v61 = 253,
   v62 = 254,
   v63 = 255,
};

template <>
struct glz::meta<RandomU8Enum1>
{
   using enum RandomU8Enum1;
   static constexpr auto value =
      enumerate(v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22,
                v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43,
                v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61, v62, v63);
};

enum class RandomU8Enum2 : uint8_t {
   v0 = 8,
   v1 = 11,
   v2 = 14,
   v3 = 16,
   v4 = 17,
   v5 = 18,
   v6 = 20,
   v7 = 25,
   v8 = 41,
   v9 = 43,
   v10 = 49,
   v11 = 53,
   v12 = 54,
   v13 = 56,
   v14 = 58,
   v15 = 59,
   v16 = 62,
   v17 = 68,
   v18 = 69,
   v19 = 71,
   v20 = 74,
   v21 = 80,
   v22 = 83,
   v23 = 90,
   v24 = 92,
   v25 = 93,
   v26 = 94,
   v27 = 97,
   v28 = 102,
   v29 = 116,
   v30 = 118,
   v31 = 136,
   v32 = 142,
   v33 = 145,
   v34 = 147,
   v35 = 150,
   v36 = 155,
   v37 = 158,
   v38 = 160,
   v39 = 162,
   v40 = 163,
   v41 = 165,
   v42 = 169,
   v43 = 171,
   v44 = 174,
   v45 = 175,
   v46 = 176,
   v47 = 179,
   v48 = 180,
   v49 = 186,
   v50 = 196,
   v51 = 197,
   v52 = 198,
   v53 = 212,
   v54 = 213,
   v55 = 218,
   v56 = 220,
   v57 = 221,
   v58 = 223,
   v59 = 226,
   v60 = 228,
   v61 = 231,
   v62 = 234,
   v63 = 242,
};

template <>
struct glz::meta<RandomU8Enum2>
{
   using enum RandomU8Enum2;
   static constexpr auto value =
      enumerate(v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22,
                v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43,
                v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61, v62, v63);
};

enum class RandomU8Enum3 : uint8_t {
   v0 = 2,
   v1 = 12,
   v2 = 16,
   v3 = 23,
   v4 = 28,
   v5 = 29,
   v6 = 35,
   v7 = 36,
   v8 = 39,
   v9 = 40,
   v10 = 54,
   v11 = 56,
   v12 = 63,
   v13 = 64,
   v14 = 67,
   v15 = 68,
   v16 = 75,
   v17 = 87,
   v18 = 92,
   v19 = 97,
   v20 = 98,
   v21 = 101,
   v22 = 102,
   v23 = 108,
   v24 = 109,
   v25 = 111,
   v26 = 116,
   v27 = 117,
   v28 = 119,
   v29 = 126,
   v30 = 127,
   v31 = 130,
   v32 = 135,
   v33 = 137,
   v34 = 141,
   v35 = 143,
   v36 = 149,
   v37 = 152,
   v38 = 160,
   v39 = 161,
   v40 = 164,
   v41 = 167,
   v42 = 174,
   v43 = 184,
   v44 = 190,
   v45 = 191,
   v46 = 192,
   v47 = 193,
   v48 = 196,
   v49 = 202,
   v50 = 206,
   v51 = 216,
   v52 = 218,
   v53 = 220,
   v54 = 223,
   v55 = 224,
   v56 = 226,
   v57 = 229,
   v58 = 234,
   v59 = 237,
   v60 = 240,
   v61 = 244,
   v62 = 245,
   v63 = 248,
};

template <>
struct glz::meta<RandomU8Enum3>
{
   using enum RandomU8Enum3;
   static constexpr auto value =
      enumerate(v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22,
                v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43,
                v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61, v62, v63);
};

// int8_t enums (signed, range -128 to 127)
enum class RandomI8Enum1 : int8_t {
   v0 = -128,
   v1 = -127,
   v2 = -124,
   v3 = -114,
   v4 = -111,
   v5 = -108,
   v6 = -107,
   v7 = -101,
   v8 = -100,
   v9 = -96,
   v10 = -89,
   v11 = -87,
   v12 = -86,
   v13 = -83,
   v14 = -78,
   v15 = -74,
   v16 = -67,
   v17 = -61,
   v18 = -52,
   v19 = -50,
   v20 = -46,
   v21 = -36,
   v22 = -33,
   v23 = -20,
   v24 = -7,
   v25 = -4,
   v26 = -3,
   v27 = 0,
   v28 = 1,
   v29 = 7,
   v30 = 8,
   v31 = 10,
   v32 = 12,
   v33 = 17,
   v34 = 25,
   v35 = 27,
   v36 = 32,
   v37 = 35,
   v38 = 40,
   v39 = 56,
   v40 = 58,
   v41 = 59,
   v42 = 66,
   v43 = 67,
   v44 = 68,
   v45 = 71,
   v46 = 76,
   v47 = 78,
   v48 = 80,
   v49 = 84,
   v50 = 87,
   v51 = 89,
   v52 = 94,
   v53 = 96,
   v54 = 100,
   v55 = 102,
   v56 = 105,
   v57 = 108,
   v58 = 116,
   v59 = 117,
   v60 = 119,
   v61 = 120,
   v62 = 122,
   v63 = 125,
};

template <>
struct glz::meta<RandomI8Enum1>
{
   using enum RandomI8Enum1;
   static constexpr auto value =
      enumerate(v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22,
                v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43,
                v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61, v62, v63);
};

enum class RandomI8Enum2 : int8_t {
   v0 = -127,
   v1 = -123,
   v2 = -120,
   v3 = -115,
   v4 = -113,
   v5 = -112,
   v6 = -111,
   v7 = -110,
   v8 = -104,
   v9 = -98,
   v10 = -95,
   v11 = -80,
   v12 = -74,
   v13 = -72,
   v14 = -71,
   v15 = -70,
   v16 = -68,
   v17 = -66,
   v18 = -65,
   v19 = -57,
   v20 = -49,
   v21 = -44,
   v22 = -42,
   v23 = -38,
   v24 = -33,
   v25 = -26,
   v26 = -24,
   v27 = -23,
   v28 = -20,
   v29 = -18,
   v30 = -16,
   v31 = -13,
   v32 = -9,
   v33 = -7,
   v34 = -4,
   v35 = 3,
   v36 = 4,
   v37 = 10,
   v38 = 13,
   v39 = 18,
   v40 = 19,
   v41 = 22,
   v42 = 33,
   v43 = 37,
   v44 = 38,
   v45 = 39,
   v46 = 40,
   v47 = 43,
   v48 = 44,
   v49 = 53,
   v50 = 54,
   v51 = 57,
   v52 = 58,
   v53 = 72,
   v54 = 76,
   v55 = 78,
   v56 = 83,
   v57 = 92,
   v58 = 102,
   v59 = 104,
   v60 = 108,
   v61 = 110,
   v62 = 123,
   v63 = 127,
};

template <>
struct glz::meta<RandomI8Enum2>
{
   using enum RandomI8Enum2;
   static constexpr auto value =
      enumerate(v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22,
                v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43,
                v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61, v62, v63);
};

// uint32_t enums (sparse, needs hash)
enum class RandomU32Enum1 : uint32_t {
   v0 = 15228622,
   v1 = 41531046,
   v2 = 106456634,
   v3 = 245522987,
   v4 = 251837136,
   v5 = 311570307,
   v6 = 422701550,
   v7 = 441495235,
   v8 = 464267175,
   v9 = 547374338,
   v10 = 576775951,
   v11 = 636057975,
   v12 = 664847319,
   v13 = 676168421,
   v14 = 677517496,
   v15 = 714300770,
   v16 = 798112150,
   v17 = 829486135,
   v18 = 955345537,
   v19 = 977515194,
   v20 = 1010193046,
   v21 = 1025148381,
   v22 = 1067970820,
   v23 = 1094024844,
   v24 = 1125089309,
   v25 = 1139027119,
   v26 = 1140169349,
   v27 = 1145921803,
   v28 = 1169726681,
   v29 = 1188332531,
   v30 = 1196342297,
   v31 = 1198832728,
   v32 = 1274350418,
   v33 = 1288477634,
   v34 = 1323959527,
   v35 = 1347233823,
   v36 = 1409874348,
   v37 = 1564319318,
   v38 = 1587106770,
   v39 = 1627677155,
   v40 = 1699887270,
   v41 = 1811967841,
   v42 = 1866437132,
   v43 = 1926780541,
   v44 = 1954246074,
   v45 = 1976987348,
   v46 = 2085812759,
   v47 = 2098228320,
   v48 = 2196545325,
   v49 = 2245334677,
   v50 = 2303029031,
   v51 = 2328710672,
   v52 = 2343292475,
   v53 = 2363629219,
   v54 = 2376077463,
   v55 = 2476426797,
   v56 = 2509023674,
   v57 = 2553440342,
   v58 = 2555656321,
   v59 = 2597724331,
   v60 = 2660223333,
   v61 = 2849232839,
   v62 = 2880327491,
   v63 = 3021680963,
};

template <>
struct glz::meta<RandomU32Enum1>
{
   using enum RandomU32Enum1;
   static constexpr auto value =
      enumerate(v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22,
                v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43,
                v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61, v62, v63);
};

enum class RandomU32Enum2 : uint32_t {
   v0 = 1743499,
   v1 = 104906255,
   v2 = 116402431,
   v3 = 123265537,
   v4 = 124660666,
   v5 = 215970859,
   v6 = 275860817,
   v7 = 312794864,
   v8 = 351784589,
   v9 = 364194056,
   v10 = 372214308,
   v11 = 453285987,
   v12 = 542114964,
   v13 = 546696950,
   v14 = 594312071,
   v15 = 654477195,
   v16 = 855256552,
   v17 = 909074012,
   v18 = 936150587,
   v19 = 942408767,
   v20 = 947206476,
   v21 = 993260504,
   v22 = 1047905204,
   v23 = 1152750071,
   v24 = 1167007513,
   v25 = 1179387055,
   v26 = 1227193073,
   v27 = 1232285036,
   v28 = 1292569882,
   v29 = 1403792057,
   v30 = 1439622619,
   v31 = 1465576748,
   v32 = 1503068227,
   v33 = 1510792572,
   v34 = 1591589823,
   v35 = 1627746667,
   v36 = 1628238707,
   v37 = 1628334692,
   v38 = 1633724084,
   v39 = 1750625978,
   v40 = 1754034197,
   v41 = 1785736751,
   v42 = 1804502687,
   v43 = 1825989011,
   v44 = 1870403050,
   v45 = 1871901278,
   v46 = 1960488639,
   v47 = 1982979741,
   v48 = 1997109065,
   v49 = 2038711176,
   v50 = 2076605983,
   v51 = 2123333781,
   v52 = 2168005699,
   v53 = 2224611603,
   v54 = 2238965651,
   v55 = 2316787198,
   v56 = 2319936135,
   v57 = 2330497115,
   v58 = 2353092123,
   v59 = 2557448229,
   v60 = 2691864041,
   v61 = 2693985578,
   v62 = 2738735023,
   v63 = 2760588734,
};

template <>
struct glz::meta<RandomU32Enum2>
{
   using enum RandomU32Enum2;
   static constexpr auto value =
      enumerate(v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22,
                v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43,
                v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61, v62, v63);
};

enum class RandomU32Enum3 : uint32_t {
   v0 = 11492154,
   v1 = 220661337,
   v2 = 230249217,
   v3 = 306702926,
   v4 = 322401182,
   v5 = 367704871,
   v6 = 391883149,
   v7 = 392245997,
   v8 = 399661102,
   v9 = 534094883,
   v10 = 674555072,
   v11 = 770695571,
   v12 = 780996231,
   v13 = 795212538,
   v14 = 827143233,
   v15 = 833728665,
   v16 = 874275988,
   v17 = 906346324,
   v18 = 911047815,
   v19 = 951749308,
   v20 = 977032766,
   v21 = 994182853,
   v22 = 1075321966,
   v23 = 1079770569,
   v24 = 1163577186,
   v25 = 1178158203,
   v26 = 1239573053,
   v27 = 1250949136,
   v28 = 1304963614,
   v29 = 1350840898,
   v30 = 1388450337,
   v31 = 1416495962,
   v32 = 1420737819,
   v33 = 1431548670,
   v34 = 1461744198,
   v35 = 1494481363,
   v36 = 1721025347,
   v37 = 1774884818,
   v38 = 1825957977,
   v39 = 1891799103,
   v40 = 1901472238,
   v41 = 1950049698,
   v42 = 1953462417,
   v43 = 2016044512,
   v44 = 2051831252,
   v45 = 2079617090,
   v46 = 2108061573,
   v47 = 2108307952,
   v48 = 2120834317,
   v49 = 2244431947,
   v50 = 2280292341,
   v51 = 2294092753,
   v52 = 2330421741,
   v53 = 2338877621,
   v54 = 2358880404,
   v55 = 2362646960,
   v56 = 2376658660,
   v57 = 2465587067,
   v58 = 2497762234,
   v59 = 2513971584,
   v60 = 2550399650,
   v61 = 2667292043,
   v62 = 2686110098,
   v63 = 2781569954,
};

template <>
struct glz::meta<RandomU32Enum3>
{
   using enum RandomU32Enum3;
   static constexpr auto value =
      enumerate(v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22,
                v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43,
                v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61, v62, v63);
};

// int64_t enums (large sparse values including negatives)
enum class RandomI64Enum1 : int64_t {
   v0 = -4564365332251179056ll,
   v1 = -4462304907209975628ll,
   v2 = -4359436676892473058ll,
   v3 = -4328572801788592873ll,
   v4 = -4206971880217493968ll,
   v5 = -4079138866662619210ll,
   v6 = -4077703386449392455ll,
   v7 = -3996219075623007752ll,
   v8 = -3884467796679683055ll,
   v9 = -3576469008271012110ll,
   v10 = -3323917281637900663ll,
   v11 = -3249174438424275149ll,
   v12 = -3231348417195919473ll,
   v13 = -3222657955421737765ll,
   v14 = -3130129882875710990ll,
   v15 = -2945892039253594341ll,
   v16 = -2944481566062275424ll,
   v17 = -2819327873128276419ll,
   v18 = -2772329842111682456ll,
   v19 = -2760286004645155305ll,
   v20 = -2757654675739026852ll,
   v21 = -2644280391339112955ll,
   v22 = -2518697749076654641ll,
   v23 = -2282853393443971568ll,
   v24 = -2187566001942696445ll,
   v25 = -2093551559498824051ll,
   v26 = -2088554468305190159ll,
   v27 = -1952487946360581085ll,
   v28 = -1943512689512827584ll,
   v29 = -1805838103700118053ll,
   v30 = -1771420096149367335ll,
   v31 = -1576377127209936231ll,
   v32 = -1542797459643372830ll,
   v33 = -1382226397606179737ll,
   v34 = -1290222654944276890ll,
   v35 = -1104369838643911773ll,
   v36 = -1095545322614902406ll,
   v37 = -907675757399589003ll,
   v38 = -571765398052948788ll,
   v39 = -466690587637281025ll,
   v40 = -449829573074579746ll,
   v41 = -415304843405078997ll,
   v42 = -366750904696563494ll,
   v43 = -328565268112253710ll,
   v44 = -280939389700012627ll,
   v45 = -206501268681702946ll,
   v46 = -107038422706417696ll,
   v47 = 11562381045053758ll,
   v48 = 277121395863698081ll,
   v49 = 316779657309132979ll,
   v50 = 415556291882797130ll,
   v51 = 689520904128037487ll,
   v52 = 694181656524073260ll,
   v53 = 701513981855790678ll,
   v54 = 772209481943830730ll,
   v55 = 832712746758178733ll,
   v56 = 846196241538146301ll,
   v57 = 906735811379796324ll,
   v58 = 912488031471669098ll,
   v59 = 961475093364856474ll,
   v60 = 1120642283436996789ll,
   v61 = 1257631261282564138ll,
   v62 = 1371970467602501734ll,
   v63 = 1415218087808752604ll,
};

template <>
struct glz::meta<RandomI64Enum1>
{
   using enum RandomI64Enum1;
   static constexpr auto value =
      enumerate(v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22,
                v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43,
                v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61, v62, v63);
};

enum class RandomI64Enum2 : int64_t {
   v0 = -4320576372119261223ll,
   v1 = -4077594026428631998ll,
   v2 = -4037058126392169589ll,
   v3 = -3925243403150937539ll,
   v4 = -3775185044658164432ll,
   v5 = -3693730133137775444ll,
   v6 = -3678677168680358231ll,
   v7 = -3662673413155623227ll,
   v8 = -3505232265117544445ll,
   v9 = -3494733982915752808ll,
   v10 = -3439247195677398151ll,
   v11 = -3328061501866390119ll,
   v12 = -3106772471931914564ll,
   v13 = -3036991454696243030ll,
   v14 = -2928885756750485998ll,
   v15 = -2920609872797475944ll,
   v16 = -2919055495889607474ll,
   v17 = -2700592915007833071ll,
   v18 = -2620141810975762971ll,
   v19 = -2583902841929476121ll,
   v20 = -2515226403550688628ll,
   v21 = -2501119015776694048ll,
   v22 = -2492426033469818076ll,
   v23 = -2469641209952557663ll,
   v24 = -2459394852865567781ll,
   v25 = -2331876246799200871ll,
   v26 = -2283862070685299439ll,
   v27 = -2273403649282931277ll,
   v28 = -2211479541987140419ll,
   v29 = -2180345572247369935ll,
   v30 = -1969162443289570675ll,
   v31 = -1875409772267085483ll,
   v32 = -1757418110287737627ll,
   v33 = -1741743284231354976ll,
   v34 = -1707997432890722922ll,
   v35 = -1265344128979294688ll,
   v36 = -826311272503431505ll,
   v37 = -699024298447522230ll,
   v38 = -603767749053051588ll,
   v39 = -524814459894787434ll,
   v40 = -439787125178091564ll,
   v41 = -313618993549911015ll,
   v42 = -280389981654904015ll,
   v43 = -256176593052396192ll,
   v44 = 90200827243037273ll,
   v45 = 224338441249130200ll,
   v46 = 266713706750028893ll,
   v47 = 283480581157381678ll,
   v48 = 429262886585483127ll,
   v49 = 477428407419566797ll,
   v50 = 503132067001390243ll,
   v51 = 526990553464315756ll,
   v52 = 548608864138226231ll,
   v53 = 694140034284583141ll,
   v54 = 828217289203684006ll,
   v55 = 952411512683484083ll,
   v56 = 1137008721084009637ll,
   v57 = 1151978886418168276ll,
   v58 = 1153356341813733364ll,
   v59 = 1153813436578630575ll,
   v60 = 1330228286192529244ll,
   v61 = 1372122100029621096ll,
   v62 = 1438961840690848996ll,
   v63 = 1540472015015337691ll,
};

template <>
struct glz::meta<RandomI64Enum2>
{
   using enum RandomI64Enum2;
   static constexpr auto value =
      enumerate(v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15, v16, v17, v18, v19, v20, v21, v22,
                v23, v24, v25, v26, v27, v28, v29, v30, v31, v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43,
                v44, v45, v46, v47, v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61, v62, v63);
};

// Helper function template for roundtrip test
template <typename E>
void test_enum_roundtrip()
{
   glz::for_each<glz::reflect<E>::size>([&]<size_t I>() {
      E val = static_cast<E>(glz::get<I>(glz::reflect<E>::values));
      std::string json;
      expect(not glz::write_json(val, json));
      E parsed{};
      expect(not glz::read_json(parsed, json));
      expect(parsed == val) << "Failed for value at index " << I;
   });
}

suite random_enum_hash_tests = [] {
   "RandomU8Enum1_roundtrip"_test = [] { test_enum_roundtrip<RandomU8Enum1>(); };
   "RandomU8Enum2_roundtrip"_test = [] { test_enum_roundtrip<RandomU8Enum2>(); };
   "RandomU8Enum3_roundtrip"_test = [] { test_enum_roundtrip<RandomU8Enum3>(); };
   "RandomI8Enum1_roundtrip"_test = [] { test_enum_roundtrip<RandomI8Enum1>(); };
   "RandomI8Enum2_roundtrip"_test = [] { test_enum_roundtrip<RandomI8Enum2>(); };
   "RandomU32Enum1_roundtrip"_test = [] { test_enum_roundtrip<RandomU32Enum1>(); };
   "RandomU32Enum2_roundtrip"_test = [] { test_enum_roundtrip<RandomU32Enum2>(); };
   "RandomU32Enum3_roundtrip"_test = [] { test_enum_roundtrip<RandomU32Enum3>(); };
   "RandomI64Enum1_roundtrip"_test = [] { test_enum_roundtrip<RandomI64Enum1>(); };
   "RandomI64Enum2_roundtrip"_test = [] { test_enum_roundtrip<RandomI64Enum2>(); };
};

// ============================================================================
// meta<E>::enum_unknown_read (Issue #2376)
// ============================================================================

namespace unknown_read_test
{
   // Every key the trait below is asked about, so a test can assert it was asked nothing, or was
   // asked only about complete names.
   inline std::vector<std::string> seen{};

   inline std::string long_alias() { return std::string(100'000, 'x'); }

   // Fits a 512 byte window, though not what is left of one that has already been partly read
   inline std::string window_sized_alias() { return std::string(300, 'z'); }
}

enum class RenamedColor { Red, Green, Blue };

template <>
struct glz::meta<RenamedColor>
{
   using enum RenamedColor;
   static constexpr auto value = enumerate(Red, Green, Blue);
   static constexpr auto enum_unknown_read = +[](std::string_view key) -> std::optional<RenamedColor> {
      unknown_read_test::seen.emplace_back(key);
      if (key == "Crimson") return Red;
      if (key == "Cyan") return Blue;
      if (key == "Redder") return Red; // a known name is a prefix of this one
      if (key == "Gre") return Green; // this one is a prefix of a known name
      if (key == "") return Green;
      if (key == "Old\"Name") return Blue;
      if (key == "Old\\Name") return Green;
      if (key == unknown_read_test::long_alias()) return Blue;
      if (key == unknown_read_test::window_sized_alias()) return Green;
      return std::nullopt;
   };
};

// The same enum with no trait, for comparing what the two report
enum class PlainColor { Red, Green, Blue };

template <>
struct glz::meta<PlainColor>
{
   using enum PlainColor;
   static constexpr auto value = enumerate(Red, Green, Blue);
};

// Aliases built from the enumerator names so they land on each way a lookup can miss: past the end
// of a known name, on a name of the same length that the hash sends to a real index, and short of
// a known name. `I` keeps them distinct for enumerators that differ only in the mangled character.
inline std::vector<std::string> manglings_of(std::string_view name, size_t I)
{
   const std::string base{name};
   const char tag = char('A' + I);
   std::string last = base;
   last.back() = tag;
   std::string first = base;
   first.front() = tag;
   return {base + "_old", base + tag, last, first, base.substr(0, base.size() - 2) + tag};
}

template <class E>
std::optional<E> mangled_alias(std::string_view key)
{
   std::optional<E> found{};
   glz::for_each<glz::reflect<E>::size>([&]<size_t I>() {
      for (const auto& alias : manglings_of(glz::get<I>(glz::reflect<E>::keys), I)) {
         if (key == alias) {
            found = E(glz::get<I>(glz::reflect<E>::values));
         }
      }
   });
   return found;
}

// Between them these select each hash_type the enum reader can see
enum class Aliased1 { Only };
enum class Aliased2 { Alpha, Beta };
enum class AliasedMod4 { dog, ant, bee, cat };
enum class AliasedXorMod4 { bird, cat, dog, emu };
enum class AliasedTrio { Spring, Summer, Autumn };
enum class AliasedPerLength { xa, xaa, xaaa, xaaaa, xab, xabc };
enum class AliasedFullFlat { xaa, xab, xba, xbb, xa, xb, xc };
enum class Aliased2Late { SharedPrefixA, SharedPrefixB };
enum class Aliased3 { Apple, Banana, Cherry };
enum class Aliased4 { North, South, East, West };
enum class Aliased5 { Low, Medium, High, Urgent, Critical };
enum class Aliased8 { One, Two, Three, Four, Five, Six, Seven, Eight };
enum class AliasedSameLength { aaaa1, aaaa2, aaaa3, aaaa4, aaaa5, aaaa6, aaaa7, aaaa8, aaaa9 };
enum class AliasedWide {
   Hydrogen,
   Helium,
   Lithium,
   Beryllium,
   Boron,
   Carbon,
   Nitrogen,
   Oxygen,
   Fluorine,
   Neon,
   Sodium,
   Magnesium,
   Aluminium,
   Silicon,
   Phosphorus,
   Sulfur,
   Chlorine,
   Argon,
   Potassium,
   Calcium,
   Rutherfordium
};

#define ALIASED_META(E, ...)                                       \
   template <>                                                     \
   struct glz::meta<E>                                             \
   {                                                               \
      using enum E;                                                \
      static constexpr auto value = enumerate(__VA_ARGS__);        \
      static constexpr auto enum_unknown_read = &mangled_alias<E>; \
   }

ALIASED_META(Aliased1, Only);
ALIASED_META(Aliased2, Alpha, Beta);
ALIASED_META(AliasedMod4, dog, ant, bee, cat);
ALIASED_META(AliasedXorMod4, bird, cat, dog, emu);
ALIASED_META(AliasedTrio, Spring, Summer, Autumn);
ALIASED_META(AliasedPerLength, xa, xaa, xaaa, xaaaa, xab, xabc);
ALIASED_META(AliasedFullFlat, xaa, xab, xba, xbb, xa, xb, xc);
ALIASED_META(Aliased2Late, SharedPrefixA, SharedPrefixB);
ALIASED_META(Aliased3, Apple, Banana, Cherry);
ALIASED_META(Aliased4, North, South, East, West);
ALIASED_META(Aliased5, Low, Medium, High, Urgent, Critical);
ALIASED_META(Aliased8, One, Two, Three, Four, Five, Six, Seven, Eight);
ALIASED_META(AliasedSameLength, aaaa1, aaaa2, aaaa3, aaaa4, aaaa5, aaaa6, aaaa7, aaaa8, aaaa9);
ALIASED_META(AliasedWide, Hydrogen, Helium, Lithium, Beryllium, Boron, Carbon, Nitrogen, Oxygen, Fluorine, Neon, Sodium,
             Magnesium, Aluminium, Silicon, Phosphorus, Sulfur, Chlorine, Argon, Potassium, Calcium, Rutherfordium);
#undef ALIASED_META

// The trait as a plain static function, with the enum described by keys and value arrays
enum class KeyedColor { Red, Green };

template <>
struct glz::meta<KeyedColor>
{
   static constexpr std::array<std::string_view, 2> keys{"Red", "Green"};
   static constexpr std::array<KeyedColor, 2> value{KeyedColor::Red, KeyedColor::Green};
   static std::optional<KeyedColor> enum_unknown_read(std::string_view key)
   {
      if (key == "Vermilion") return KeyedColor::Red;
      return std::nullopt;
   }
};

struct RenamedColorHolder
{
   RenamedColor color{};
   int n{};
};

struct RenamedColorPairHolder
{
   RenamedColor first{};
   RenamedColor second{};
};

struct RenamedColorColumn
{
   std::vector<RenamedColor> colors{};
};

struct linear_enum_opts : glz::opts
{
   bool linear_search = true;
};

// Reads every enumerator and every alias of E through `read(E&, name)`
template <class E>
void check_aliases(auto&& read)
{
   glz::for_each<glz::reflect<E>::size>([&]<size_t I>() {
      const auto canonical = E(glz::get<I>(glz::reflect<E>::values));
      const std::string_view name = glz::get<I>(glz::reflect<E>::keys);

      E known = static_cast<E>(99);
      expect(not read(known, std::string{name})) << name;
      expect(known == canonical) << name;

      for (const auto& alias : manglings_of(name, I)) {
         E parsed = static_cast<E>(99);
         const auto ec = read(parsed, alias);
         expect(not ec) << alias;
         expect(parsed == canonical) << alias;
      }

      E rejected = canonical;
      const auto ec = read(rejected, std::string{name} + "_unrelated");
      expect(ec.ec == glz::error_code::unexpected_enum) << name;
      expect(rejected == canonical) << name;
   });
}

template <class E>
void check_every_format_and_path()
{
   const auto quoted = [](const std::string& s) { return "\"" + s + "\""; };

   check_aliases<E>([&](E& v, const std::string& s) { return glz::read_json(v, quoted(s)); });
   check_aliases<E>([&](E& v, const std::string& s) { return glz::read<linear_enum_opts{}>(v, quoted(s)); });
   check_aliases<E>([&](E& v, const std::string& s) {
      return glz::read<glz::opts{.null_terminated = false}>(v, std::string_view{quoted(s)});
   });
   // Inside a document, so that the alias is not at the very end of the buffer
   check_aliases<E>([&](E& v, const std::string& s) {
      std::vector<E> out{};
      const auto ec = glz::read_json(out, "[" + quoted(s) + "]");
      if (not ec) v = out.at(0);
      return ec;
   });
   check_aliases<E>([&](E& v, const std::string& s) { return glz::read_toml(v, quoted(s)); });
   check_aliases<E>([&](E& v, const std::string& s) { return glz::read_yaml(v, s); });
   check_aliases<E>([&](E& v, const std::string& s) {
      std::string buffer{};
      expect(not glz::write_msgpack(s, buffer));
      return glz::read_msgpack(v, buffer);
   });
}

static_assert(glz::hash_info<Aliased1>.type == glz::hash_type::single_element);
static_assert(glz::hash_info<Aliased2>.type == glz::hash_type::unique_index);
static_assert(glz::hash_info<AliasedMod4>.type == glz::hash_type::mod4);
static_assert(glz::hash_info<AliasedXorMod4>.type == glz::hash_type::xor_mod4);
static_assert(glz::hash_info<Aliased3>.type == glz::hash_type::minus_mod4);
static_assert(glz::hash_info<AliasedTrio>.type == glz::hash_type::three_element_unique_index);
static_assert(glz::hash_info<Aliased8>.type == glz::hash_type::front_hash);
static_assert(glz::hash_info<AliasedPerLength>.type == glz::hash_type::unique_per_length);
static_assert(glz::hash_info<AliasedFullFlat>.type == glz::hash_type::full_flat);

suite enum_unknown_read_tests = [] {
   using unknown_read_test::seen;

   "known names do not reach the trait"_test = [] {
      seen.clear();
      RenamedColor c{};
      expect(not glz::read_json(c, R"("Green")"));
      expect(c == RenamedColor::Green);
      expect(not glz::read_json(c, R"("Blue")"));
      expect(c == RenamedColor::Blue);
      expect(not glz::read_json(c, R"("Red")"));
      expect(c == RenamedColor::Red);
      expect(seen.empty());
   };

   "alias reads as the canonical value"_test = [] {
      RenamedColor c = RenamedColor::Green;
      expect(not glz::read_json(c, R"("Crimson")"));
      expect(c == RenamedColor::Red);
      expect(not glz::read_json(c, R"("Cyan")"));
      expect(c == RenamedColor::Blue);
   };

   "alias sharing a prefix with a known name"_test = [] {
      RenamedColor c = RenamedColor::Green;
      expect(not glz::read_json(c, R"("Redder")"));
      expect(c == RenamedColor::Red);
      c = RenamedColor::Blue;
      expect(not glz::read_json(c, R"("Gre")"));
      expect(c == RenamedColor::Green);
   };

   "empty string reaches the trait"_test = [] {
      RenamedColor c = RenamedColor::Red;
      expect(not glz::read_json(c, R"("")"));
      expect(c == RenamedColor::Green);
   };

   "trait returning nullopt keeps unexpected_enum"_test = [] {
      seen.clear();
      RenamedColor c = RenamedColor::Blue;
      expect(glz::read_json(c, R"("Silver")").ec == glz::error_code::unexpected_enum);
      expect(c == RenamedColor::Blue);
      expect(seen == std::vector<std::string>{"Silver"});
   };

   "a failed lookup reports what an enum with no trait reports"_test = [] {
      for (const std::string_view input : {R"("Silver")", R"("Redd")", R"("Re")", R"("Silver)", R"("Bad\qEscape")",
                                           "\"R\\u0065d\"", R"(["Silver"])"}) {
         RenamedColor with_trait{};
         PlainColor without{};
         const auto a = glz::read_json(with_trait, input);
         const auto b = glz::read_json(without, input);
         expect(a.ec == b.ec) << input;
         expect(a.count == b.count) << input;
      }
   };

   "writes keep the canonical name"_test = [] {
      RenamedColor c{};
      expect(not glz::read_json(c, R"("Crimson")"));
      std::string json{};
      expect(not glz::write_json(c, json));
      expect(json == R"("Red")");
      std::string toml{};
      expect(not glz::write_toml(c, toml));
      expect(toml == R"("Red")");
   };

   "alias with escapes is decoded before the trait sees it"_test = [] {
      RenamedColor c = RenamedColor::Red;
      expect(not glz::read_json(c, R"("Old\"Name")"));
      expect(c == RenamedColor::Blue);
      expect(not glz::read_json(c, R"("Old\\Name")"));
      expect(c == RenamedColor::Green);
      expect(not glz::read_json(c, std::string_view{"\"Cr\\u0069mson\""}));
      expect(c == RenamedColor::Red);
   };

   "an escaped quote does not end the name"_test = [] {
      std::vector<RenamedColor> out{};
      expect(not glz::read_json(out, R"(["Old\"Name","Green"])"));
      expect(out == std::vector{RenamedColor::Blue, RenamedColor::Green});
   };

   "malformed names do not reach the trait"_test = [] {
      seen.clear();
      RenamedColor c = RenamedColor::Blue;
      expect(glz::read_json(c, R"("Bad\qEscape")").ec == glz::error_code::unexpected_enum);
      expect(glz::read_json(c, R"("Crimson)").ec == glz::error_code::unexpected_enum);
      expect(glz::read<glz::opts{.null_terminated = false}>(c, std::string_view{R"("Crimson)"}).ec ==
             glz::error_code::unexpected_enum);
      expect(seen.empty());
      expect(c == RenamedColor::Blue);
   };

   "an escaped spelling of a known name is still not that name"_test = [] {
      RenamedColor c = RenamedColor::Blue;
      expect(glz::read_json(c, std::string_view{"\"R\\u0065d\""}).ec == glz::error_code::unexpected_enum);
      expect(c == RenamedColor::Blue);
   };

   "very long names"_test = [] {
      const std::string alias = "\"" + unknown_read_test::long_alias() + "\"";
      const std::string unknown = "\"" + std::string(100'000, 'y') + "\"";

      RenamedColor c = RenamedColor::Red;
      expect(not glz::read_json(c, alias));
      expect(c == RenamedColor::Blue);
      c = RenamedColor::Red;
      expect(not glz::read<linear_enum_opts{}>(c, alias));
      expect(c == RenamedColor::Blue);
      c = RenamedColor::Red;
      expect(not glz::read<glz::opts{.null_terminated = false}>(c, std::string_view{alias}));
      expect(c == RenamedColor::Blue);

      c = RenamedColor::Red;
      expect(glz::read_json(c, unknown).ec == glz::error_code::unexpected_enum);
      expect(glz::read<linear_enum_opts{}>(c, unknown).ec == glz::error_code::unexpected_enum);
      expect(glz::read<glz::opts{.null_terminated = false}>(c, std::string_view{unknown}).ec ==
             glz::error_code::unexpected_enum);
      expect(c == RenamedColor::Red);
   };

   "the read continues after an alias"_test = [] {
      RenamedColorHolder holder{};
      expect(not glz::read_json(holder, R"({"color" : "Cyan" , "n":7})"));
      expect(holder.color == RenamedColor::Blue);
      expect(holder.n == 7);

      std::vector<RenamedColor> list{};
      expect(not glz::read_json(list, R"(["Crimson", "Green" ,"Cyan","Redder"])"));
      expect(list == std::vector{RenamedColor::Red, RenamedColor::Green, RenamedColor::Blue, RenamedColor::Red});
   };

   "alias as a map key"_test = [] {
      std::map<RenamedColor, int> m{};
      expect(not glz::read_json(m, R"({"Crimson":1,"Cyan":2})"));
      expect(m == std::map<RenamedColor, int>{{RenamedColor::Red, 1}, {RenamedColor::Blue, 2}});

      std::string json{};
      expect(not glz::write_json(m, json));
      expect(json == R"({"Red":1,"Blue":2})");
   };

   "alias inside a variant"_test = [] {
      std::variant<int, RenamedColor> v{};
      expect(not glz::read_json(v, R"("Crimson")"));
      expect(std::holds_alternative<RenamedColor>(v));
      expect(std::get<RenamedColor>(v) == RenamedColor::Red);
   };

   "jsonc and ndjson go through the same reader"_test = [] {
      RenamedColor c = RenamedColor::Green;
      expect(not glz::read_jsonc(c, std::string{"/* old name */ \"Crimson\""}));
      expect(c == RenamedColor::Red);

      std::vector<RenamedColor> lines{};
      expect(not glz::read_ndjson(lines, std::string{"\"Crimson\"\n\"Green\"\n\"Cyan\""}));
      expect(lines == std::vector{RenamedColor::Red, RenamedColor::Green, RenamedColor::Blue});
   };

   "trait as a static function on a keys and value enum"_test = [] {
      KeyedColor c = KeyedColor::Green;
      expect(not glz::read_json(c, R"("Vermilion")"));
      expect(c == KeyedColor::Red);
      expect(glz::read_json(c, R"("Silver")").ec == glz::error_code::unexpected_enum);
   };

   "every hash shape and format"_test = [] {
      check_every_format_and_path<Aliased1>();
      check_every_format_and_path<Aliased2>();
      check_every_format_and_path<AliasedMod4>();
      check_every_format_and_path<AliasedXorMod4>();
      check_every_format_and_path<AliasedTrio>();
      check_every_format_and_path<AliasedPerLength>();
      check_every_format_and_path<AliasedFullFlat>();
      check_every_format_and_path<Aliased2Late>();
      check_every_format_and_path<Aliased3>();
      check_every_format_and_path<Aliased4>();
      check_every_format_and_path<Aliased5>();
      check_every_format_and_path<Aliased8>();
      check_every_format_and_path<AliasedSameLength>();
      check_every_format_and_path<AliasedWide>();
   };
};

suite enum_unknown_read_streaming_tests = [] {
   using unknown_read_test::seen;

   "aliases across refills"_test = [] {
      std::string json = "[";
      std::vector<RenamedColor> expected{};
      for (size_t i = 0; i < 2000; ++i) {
         json += (i ? "," : "");
         switch (i % 4) {
         case 0:
            json += R"("Crimson")";
            expected.push_back(RenamedColor::Red);
            break;
         case 1:
            json += R"("Green")";
            expected.push_back(RenamedColor::Green);
            break;
         case 2:
            json += R"("Old\"Name")";
            expected.push_back(RenamedColor::Blue);
            break;
         default:
            json += R"("Cyan")";
            expected.push_back(RenamedColor::Blue);
            break;
         }
      }
      json += "]";

      std::istringstream stream{json};
      glz::istream_buffer<512> buffer{stream};
      std::vector<RenamedColor> out{};
      const auto ec = glz::read_json(out, buffer);
      expect(not ec) << glz::format_error(ec, json);
      expect(out == expected);
   };

   "a name cut off by the window never reaches the trait"_test = [] {
      seen.clear();
      // No closing quote inside the window. Were the bytes it does hold handed over, the trait would
      // be asked about a prefix of the name.
      std::istringstream stream{"\"" + std::string(2000, 'x') + "\""};
      glz::istream_buffer<512> buffer{stream};
      RenamedColor c = RenamedColor::Green;
      expect(glz::read_json(c, buffer).ec == glz::error_code::unexpected_enum);
      expect(seen.empty());
      expect(c == RenamedColor::Green);
   };

   "aliases long enough to fill most of the window"_test = [] {
      const std::string alias = "\"" + unknown_read_test::window_sized_alias() + "\"";
      std::string json = "[";
      for (size_t i = 0; i < 50; ++i) {
         json += (i ? "," : "") + alias;
      }
      json += "]";

      std::istringstream stream{json};
      glz::istream_buffer<512> buffer{stream};
      std::vector<RenamedColor> out{};
      const auto ec = glz::read_json(out, buffer);
      expect(not ec) << glz::format_error(ec, json);
      expect(out == std::vector<RenamedColor>(50, RenamedColor::Green));
   };

   "an alias straddling the window edge is an error, not a prefix"_test = [] {
      seen.clear();
      // The window is refilled before an element only once half of it is used up, so stopping just short
      // of that leaves less than the long alias needs. It would fit a fresh window, and a std::string
      // element would fail at the same spot.
      std::string json = "[";
      for (size_t i = 0; i < 24; ++i) {
         json += R"("Crimson",)";
      }
      json += "\"" + unknown_read_test::window_sized_alias() + "\"]";

      std::istringstream stream{json};
      glz::istream_buffer<512> buffer{stream};
      std::vector<RenamedColor> out{};
      expect(glz::read_json(out, buffer).ec == glz::error_code::unexpected_enum);
      expect(out.size() == 25u); // the failed element was emplaced before it was read
      expect(seen == std::vector<std::string>(24, "Crimson"));
   };
};

suite enum_unknown_read_other_format_tests = [] {
   using unknown_read_test::seen;

   "toml"_test = [] {
      RenamedColorHolder holder{};
      expect(not glz::read_toml(holder, "color = \"Cyan\"\nn = 7\n"));
      expect(holder.color == RenamedColor::Blue);
      expect(holder.n == 7);

      expect(glz::read_toml(holder, "color = \"Silver\"\n").ec == glz::error_code::unexpected_enum);
      expect(holder.color == RenamedColor::Blue);

      seen.clear();
      expect(not glz::read_toml(holder, "color = \"Green\"\n"));
      expect(seen.empty());
   };

   "toml alias with escapes"_test = [] {
      RenamedColorHolder holder{};
      expect(not glz::read_toml(holder, "color = \"Old\\\"Name\"\nn = 7\n"));
      expect(holder.color == RenamedColor::Blue);
      expect(holder.n == 7);
      expect(not glz::read_toml(holder, "color = \"Cr\\u0069mson\"\n"));
      expect(holder.color == RenamedColor::Red);
   };

   "yaml"_test = [] {
      RenamedColorHolder holder{};
      expect(not glz::read_yaml(holder, std::string{"color: Cyan\nn: 7\n"}));
      expect(holder.color == RenamedColor::Blue);
      expect(holder.n == 7);
      expect(not glz::read_yaml(holder, std::string{"color: \"Old\\\"Name\"\n"}));
      expect(holder.color == RenamedColor::Blue);
      expect(not glz::read_yaml(holder, std::string{"color: 'Crimson'\n"}));
      expect(holder.color == RenamedColor::Red);

      expect(glz::read_yaml(holder, std::string{"color: Silver\n"}).ec == glz::error_code::unexpected_enum);
      expect(holder.color == RenamedColor::Red);
   };

   "yaml anchor on an alias"_test = [] {
      RenamedColorPairHolder holder{};
      const std::string input = "first: &c Cyan\nsecond: *c\n";
      const auto ec = glz::read_yaml(holder, input);
      expect(not ec) << glz::format_error(ec, input);
      expect(holder.first == RenamedColor::Blue);
      expect(holder.second == RenamedColor::Blue);
   };

   "csv"_test = [] {
      RenamedColorColumn column{};
      const std::string input = "colors\nCrimson\nGreen\nCyan\n";
      expect(not glz::read_csv<glz::colwise>(column, input));
      expect(column.colors == std::vector{RenamedColor::Red, RenamedColor::Green, RenamedColor::Blue});

      const std::string unknown = "colors\nSilver\n";
      expect(glz::read_csv<glz::colwise>(column, unknown).ec == glz::error_code::unexpected_enum);
   };

   "msgpack"_test = [] {
      std::string buffer{};
      expect(not glz::write_msgpack(std::string{"Cyan"}, buffer));
      RenamedColor c = RenamedColor::Red;
      expect(not glz::read_msgpack(c, buffer));
      expect(c == RenamedColor::Blue);

      buffer.clear();
      expect(not glz::write_msgpack(std::string{"Silver"}, buffer));
      expect(glz::read_msgpack(c, buffer).ec == glz::error_code::unexpected_enum);
      expect(c == RenamedColor::Blue);

      // The name a write produces stays the canonical one
      buffer.clear();
      expect(not glz::write_msgpack(c, buffer));
      std::string name{};
      expect(not glz::read_msgpack(name, buffer));
      expect(name == "Blue");
   };

   "formats that carry the value never reach the trait"_test = [] {
      seen.clear();
      const auto round_trips = [](auto&& write, auto&& read) {
         RenamedColorHolder in{RenamedColor::Blue, 3};
         std::string buffer{};
         expect(not write(in, buffer));
         RenamedColorHolder out{};
         expect(not read(out, buffer));
         expect(out.color == RenamedColor::Blue);
         expect(out.n == 3);
      };
      round_trips([](auto& v, auto& b) { return glz::write_beve(v, b); },
                  [](auto& v, auto& b) { return glz::read_beve(v, b); });
      round_trips([](auto& v, auto& b) { return glz::write_cbor(v, b); },
                  [](auto& v, auto& b) { return glz::read_cbor(v, b); });
      round_trips([](auto& v, auto& b) { return glz::write_bson(v, b); },
                  [](auto& v, auto& b) { return glz::read_bson(v, b); });
      round_trips([](auto& v, auto& b) { return glz::write_jsonb(v, b); },
                  [](auto& v, auto& b) { return glz::read_jsonb(v, b); });
      expect(seen.empty());
   };
};

int main() { return 0; }
