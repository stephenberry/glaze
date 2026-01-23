// Glaze Library
// For the license information refer to glaze.hpp

#include <array>

#include "glaze/glaze.hpp"
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
      SparseEnum e;
      expect(not glz::read_json(e, R"("Zero")"));
      expect(e == SparseEnum::Zero);

      expect(not glz::read_json(e, R"("FourHundredMillion")"));
      expect(e == SparseEnum::FourHundredMillion);
   };

   "sparse_enum_roundtrip"_test = [] {
      for (auto val : {SparseEnum::Zero, SparseEnum::FourHundredMillion}) {
         std::string json;
         expect(not glz::write_json(val, json));

         SparseEnum parsed;
         expect(not glz::read_json(parsed, json));
         expect(parsed == val);
      }
   };

   "sparse_enum_millions_roundtrip"_test = [] {
      for (auto val : {SparseEnumMillions::A, SparseEnumMillions::B, SparseEnumMillions::C}) {
         std::string json;
         expect(not glz::write_json(val, json));

         SparseEnumMillions parsed;
         expect(not glz::read_json(parsed, json));
         expect(parsed == val);
      }
   };

   "sparse_enum_pow2_roundtrip"_test = [] {
      for (auto val : {SparseEnumPow2::X, SparseEnumPow2::Y, SparseEnumPow2::Z}) {
         std::string json;
         expect(not glz::write_json(val, json));

         SparseEnumPow2 parsed;
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

         SparseEnumXor parsed;
         expect(not glz::read_json(parsed, json));
         expect(parsed == val);
      }
   };

   "sparse_enum_xor_deserialization"_test = [] {
      SparseEnumXor e;
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

      SparseEnumTestStruct parsed;
      expect(not glz::read_json(parsed, json));
      expect(parsed.e1 == SparseEnum::Zero);
      expect(parsed.e2 == SparseEnum::FourHundredMillion);
   };
};

// ============================================================================
// Random enum hash stress tests
// Tests that the hash algorithm finds seeds for various enum configurations
// ============================================================================

// uint8_t enums (should use small_range strategy since range <= 256)
enum class RandomU8Enum1 : uint8_t {
   v0 = 1,    v1 = 6,    v2 = 7,    v3 = 8,    v4 = 11,   v5 = 20,   v6 = 22,   v7 = 23,
   v8 = 24,   v9 = 26,   v10 = 31,  v11 = 35,  v12 = 39,  v13 = 40,  v14 = 50,  v15 = 55,
   v16 = 56,  v17 = 57,  v18 = 59,  v19 = 62,  v20 = 67,  v21 = 70,  v22 = 71,  v23 = 86,
   v24 = 87,  v25 = 88,  v26 = 91,  v27 = 96,  v28 = 97,  v29 = 107, v30 = 108, v31 = 114,
   v32 = 117, v33 = 129, v34 = 137, v35 = 139, v36 = 141, v37 = 143, v38 = 150, v39 = 151,
   v40 = 154, v41 = 166, v42 = 173, v43 = 178, v44 = 179, v45 = 183, v46 = 186, v47 = 188,
   v48 = 189, v49 = 194, v50 = 195, v51 = 206, v52 = 207, v53 = 221, v54 = 228, v55 = 233,
   v56 = 236, v57 = 237, v58 = 240, v59 = 243, v60 = 247, v61 = 253, v62 = 254, v63 = 255,
};

template <>
struct glz::meta<RandomU8Enum1>
{
   using enum RandomU8Enum1;
   static constexpr auto value = enumerate(
      v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15,
      v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31,
      v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47,
      v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61, v62, v63);
};

enum class RandomU8Enum2 : uint8_t {
   v0 = 8,    v1 = 11,   v2 = 14,   v3 = 16,   v4 = 17,   v5 = 18,   v6 = 20,   v7 = 25,
   v8 = 41,   v9 = 43,   v10 = 49,  v11 = 53,  v12 = 54,  v13 = 56,  v14 = 58,  v15 = 59,
   v16 = 62,  v17 = 68,  v18 = 69,  v19 = 71,  v20 = 74,  v21 = 80,  v22 = 83,  v23 = 90,
   v24 = 92,  v25 = 93,  v26 = 94,  v27 = 97,  v28 = 102, v29 = 116, v30 = 118, v31 = 136,
   v32 = 142, v33 = 145, v34 = 147, v35 = 150, v36 = 155, v37 = 158, v38 = 160, v39 = 162,
   v40 = 163, v41 = 165, v42 = 169, v43 = 171, v44 = 174, v45 = 175, v46 = 176, v47 = 179,
   v48 = 180, v49 = 186, v50 = 196, v51 = 197, v52 = 198, v53 = 212, v54 = 213, v55 = 218,
   v56 = 220, v57 = 221, v58 = 223, v59 = 226, v60 = 228, v61 = 231, v62 = 234, v63 = 242,
};

template <>
struct glz::meta<RandomU8Enum2>
{
   using enum RandomU8Enum2;
   static constexpr auto value = enumerate(
      v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15,
      v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31,
      v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47,
      v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61, v62, v63);
};

enum class RandomU8Enum3 : uint8_t {
   v0 = 2,    v1 = 12,   v2 = 16,   v3 = 23,   v4 = 28,   v5 = 29,   v6 = 35,   v7 = 36,
   v8 = 39,   v9 = 40,   v10 = 54,  v11 = 56,  v12 = 63,  v13 = 64,  v14 = 67,  v15 = 68,
   v16 = 75,  v17 = 87,  v18 = 92,  v19 = 97,  v20 = 98,  v21 = 101, v22 = 102, v23 = 108,
   v24 = 109, v25 = 111, v26 = 116, v27 = 117, v28 = 119, v29 = 126, v30 = 127, v31 = 130,
   v32 = 135, v33 = 137, v34 = 141, v35 = 143, v36 = 149, v37 = 152, v38 = 160, v39 = 161,
   v40 = 164, v41 = 167, v42 = 174, v43 = 184, v44 = 190, v45 = 191, v46 = 192, v47 = 193,
   v48 = 196, v49 = 202, v50 = 206, v51 = 216, v52 = 218, v53 = 220, v54 = 223, v55 = 224,
   v56 = 226, v57 = 229, v58 = 234, v59 = 237, v60 = 240, v61 = 244, v62 = 245, v63 = 248,
};

template <>
struct glz::meta<RandomU8Enum3>
{
   using enum RandomU8Enum3;
   static constexpr auto value = enumerate(
      v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15,
      v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31,
      v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47,
      v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61, v62, v63);
};

// int8_t enums (signed, range -128 to 127)
enum class RandomI8Enum1 : int8_t {
   v0 = -128,  v1 = -127,  v2 = -124,  v3 = -114,  v4 = -111,  v5 = -108,  v6 = -107,  v7 = -101,
   v8 = -100,  v9 = -96,   v10 = -89,  v11 = -87,  v12 = -86,  v13 = -83,  v14 = -78,  v15 = -74,
   v16 = -67,  v17 = -61,  v18 = -52,  v19 = -50,  v20 = -46,  v21 = -36,  v22 = -33,  v23 = -20,
   v24 = -7,   v25 = -4,   v26 = -3,   v27 = 0,    v28 = 1,    v29 = 7,    v30 = 8,    v31 = 10,
   v32 = 12,   v33 = 17,   v34 = 25,   v35 = 27,   v36 = 32,   v37 = 35,   v38 = 40,   v39 = 56,
   v40 = 58,   v41 = 59,   v42 = 66,   v43 = 67,   v44 = 68,   v45 = 71,   v46 = 76,   v47 = 78,
   v48 = 80,   v49 = 84,   v50 = 87,   v51 = 89,   v52 = 94,   v53 = 96,   v54 = 100,  v55 = 102,
   v56 = 105,  v57 = 108,  v58 = 116,  v59 = 117,  v60 = 119,  v61 = 120,  v62 = 122,  v63 = 125,
};

template <>
struct glz::meta<RandomI8Enum1>
{
   using enum RandomI8Enum1;
   static constexpr auto value = enumerate(
      v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15,
      v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31,
      v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47,
      v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61, v62, v63);
};

enum class RandomI8Enum2 : int8_t {
   v0 = -127,  v1 = -123,  v2 = -120,  v3 = -115,  v4 = -113,  v5 = -112,  v6 = -111,  v7 = -110,
   v8 = -104,  v9 = -98,   v10 = -95,  v11 = -80,  v12 = -74,  v13 = -72,  v14 = -71,  v15 = -70,
   v16 = -68,  v17 = -66,  v18 = -65,  v19 = -57,  v20 = -49,  v21 = -44,  v22 = -42,  v23 = -38,
   v24 = -33,  v25 = -26,  v26 = -24,  v27 = -23,  v28 = -20,  v29 = -18,  v30 = -16,  v31 = -13,
   v32 = -9,   v33 = -7,   v34 = -4,   v35 = 3,    v36 = 4,    v37 = 10,   v38 = 13,   v39 = 18,
   v40 = 19,   v41 = 22,   v42 = 33,   v43 = 37,   v44 = 38,   v45 = 39,   v46 = 40,   v47 = 43,
   v48 = 44,   v49 = 53,   v50 = 54,   v51 = 57,   v52 = 58,   v53 = 72,   v54 = 76,   v55 = 78,
   v56 = 83,   v57 = 92,   v58 = 102,  v59 = 104,  v60 = 108,  v61 = 110,  v62 = 123,  v63 = 127,
};

template <>
struct glz::meta<RandomI8Enum2>
{
   using enum RandomI8Enum2;
   static constexpr auto value = enumerate(
      v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15,
      v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31,
      v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47,
      v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61, v62, v63);
};

// uint32_t enums (sparse, needs hash)
enum class RandomU32Enum1 : uint32_t {
   v0 = 15228622,    v1 = 41531046,    v2 = 106456634,   v3 = 245522987,
   v4 = 251837136,   v5 = 311570307,   v6 = 422701550,   v7 = 441495235,
   v8 = 464267175,   v9 = 547374338,   v10 = 576775951,  v11 = 636057975,
   v12 = 664847319,  v13 = 676168421,  v14 = 677517496,  v15 = 714300770,
   v16 = 798112150,  v17 = 829486135,  v18 = 955345537,  v19 = 977515194,
   v20 = 1010193046, v21 = 1025148381, v22 = 1067970820, v23 = 1094024844,
   v24 = 1125089309, v25 = 1139027119, v26 = 1140169349, v27 = 1145921803,
   v28 = 1169726681, v29 = 1188332531, v30 = 1196342297, v31 = 1198832728,
   v32 = 1274350418, v33 = 1288477634, v34 = 1323959527, v35 = 1347233823,
   v36 = 1409874348, v37 = 1564319318, v38 = 1587106770, v39 = 1627677155,
   v40 = 1699887270, v41 = 1811967841, v42 = 1866437132, v43 = 1926780541,
   v44 = 1954246074, v45 = 1976987348, v46 = 2085812759, v47 = 2098228320,
   v48 = 2196545325, v49 = 2245334677, v50 = 2303029031, v51 = 2328710672,
   v52 = 2343292475, v53 = 2363629219, v54 = 2376077463, v55 = 2476426797,
   v56 = 2509023674, v57 = 2553440342, v58 = 2555656321, v59 = 2597724331,
   v60 = 2660223333, v61 = 2849232839, v62 = 2880327491, v63 = 3021680963,
};

template <>
struct glz::meta<RandomU32Enum1>
{
   using enum RandomU32Enum1;
   static constexpr auto value = enumerate(
      v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15,
      v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31,
      v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47,
      v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61, v62, v63);
};

enum class RandomU32Enum2 : uint32_t {
   v0 = 1743499,     v1 = 104906255,   v2 = 116402431,   v3 = 123265537,
   v4 = 124660666,   v5 = 215970859,   v6 = 275860817,   v7 = 312794864,
   v8 = 351784589,   v9 = 364194056,   v10 = 372214308,  v11 = 453285987,
   v12 = 542114964,  v13 = 546696950,  v14 = 594312071,  v15 = 654477195,
   v16 = 855256552,  v17 = 909074012,  v18 = 936150587,  v19 = 942408767,
   v20 = 947206476,  v21 = 993260504,  v22 = 1047905204, v23 = 1152750071,
   v24 = 1167007513, v25 = 1179387055, v26 = 1227193073, v27 = 1232285036,
   v28 = 1292569882, v29 = 1403792057, v30 = 1439622619, v31 = 1465576748,
   v32 = 1503068227, v33 = 1510792572, v34 = 1591589823, v35 = 1627746667,
   v36 = 1628238707, v37 = 1628334692, v38 = 1633724084, v39 = 1750625978,
   v40 = 1754034197, v41 = 1785736751, v42 = 1804502687, v43 = 1825989011,
   v44 = 1870403050, v45 = 1871901278, v46 = 1960488639, v47 = 1982979741,
   v48 = 1997109065, v49 = 2038711176, v50 = 2076605983, v51 = 2123333781,
   v52 = 2168005699, v53 = 2224611603, v54 = 2238965651, v55 = 2316787198,
   v56 = 2319936135, v57 = 2330497115, v58 = 2353092123, v59 = 2557448229,
   v60 = 2691864041, v61 = 2693985578, v62 = 2738735023, v63 = 2760588734,
};

template <>
struct glz::meta<RandomU32Enum2>
{
   using enum RandomU32Enum2;
   static constexpr auto value = enumerate(
      v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15,
      v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31,
      v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47,
      v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61, v62, v63);
};

enum class RandomU32Enum3 : uint32_t {
   v0 = 11492154,    v1 = 220661337,   v2 = 230249217,   v3 = 306702926,
   v4 = 322401182,   v5 = 367704871,   v6 = 391883149,   v7 = 392245997,
   v8 = 399661102,   v9 = 534094883,   v10 = 674555072,  v11 = 770695571,
   v12 = 780996231,  v13 = 795212538,  v14 = 827143233,  v15 = 833728665,
   v16 = 874275988,  v17 = 906346324,  v18 = 911047815,  v19 = 951749308,
   v20 = 977032766,  v21 = 994182853,  v22 = 1075321966, v23 = 1079770569,
   v24 = 1163577186, v25 = 1178158203, v26 = 1239573053, v27 = 1250949136,
   v28 = 1304963614, v29 = 1350840898, v30 = 1388450337, v31 = 1416495962,
   v32 = 1420737819, v33 = 1431548670, v34 = 1461744198, v35 = 1494481363,
   v36 = 1721025347, v37 = 1774884818, v38 = 1825957977, v39 = 1891799103,
   v40 = 1901472238, v41 = 1950049698, v42 = 1953462417, v43 = 2016044512,
   v44 = 2051831252, v45 = 2079617090, v46 = 2108061573, v47 = 2108307952,
   v48 = 2120834317, v49 = 2244431947, v50 = 2280292341, v51 = 2294092753,
   v52 = 2330421741, v53 = 2338877621, v54 = 2358880404, v55 = 2362646960,
   v56 = 2376658660, v57 = 2465587067, v58 = 2497762234, v59 = 2513971584,
   v60 = 2550399650, v61 = 2667292043, v62 = 2686110098, v63 = 2781569954,
};

template <>
struct glz::meta<RandomU32Enum3>
{
   using enum RandomU32Enum3;
   static constexpr auto value = enumerate(
      v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15,
      v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31,
      v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47,
      v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61, v62, v63);
};

// int64_t enums (large sparse values including negatives)
enum class RandomI64Enum1 : int64_t {
   v0 = -4564365332251179056ll, v1 = -4462304907209975628ll, v2 = -4359436676892473058ll, v3 = -4328572801788592873ll,
   v4 = -4206971880217493968ll, v5 = -4079138866662619210ll, v6 = -4077703386449392455ll, v7 = -3996219075623007752ll,
   v8 = -3884467796679683055ll, v9 = -3576469008271012110ll, v10 = -3323917281637900663ll, v11 = -3249174438424275149ll,
   v12 = -3231348417195919473ll, v13 = -3222657955421737765ll, v14 = -3130129882875710990ll, v15 = -2945892039253594341ll,
   v16 = -2944481566062275424ll, v17 = -2819327873128276419ll, v18 = -2772329842111682456ll, v19 = -2760286004645155305ll,
   v20 = -2757654675739026852ll, v21 = -2644280391339112955ll, v22 = -2518697749076654641ll, v23 = -2282853393443971568ll,
   v24 = -2187566001942696445ll, v25 = -2093551559498824051ll, v26 = -2088554468305190159ll, v27 = -1952487946360581085ll,
   v28 = -1943512689512827584ll, v29 = -1805838103700118053ll, v30 = -1771420096149367335ll, v31 = -1576377127209936231ll,
   v32 = -1542797459643372830ll, v33 = -1382226397606179737ll, v34 = -1290222654944276890ll, v35 = -1104369838643911773ll,
   v36 = -1095545322614902406ll, v37 = -907675757399589003ll, v38 = -571765398052948788ll, v39 = -466690587637281025ll,
   v40 = -449829573074579746ll, v41 = -415304843405078997ll, v42 = -366750904696563494ll, v43 = -328565268112253710ll,
   v44 = -280939389700012627ll, v45 = -206501268681702946ll, v46 = -107038422706417696ll, v47 = 11562381045053758ll,
   v48 = 277121395863698081ll, v49 = 316779657309132979ll, v50 = 415556291882797130ll, v51 = 689520904128037487ll,
   v52 = 694181656524073260ll, v53 = 701513981855790678ll, v54 = 772209481943830730ll, v55 = 832712746758178733ll,
   v56 = 846196241538146301ll, v57 = 906735811379796324ll, v58 = 912488031471669098ll, v59 = 961475093364856474ll,
   v60 = 1120642283436996789ll, v61 = 1257631261282564138ll, v62 = 1371970467602501734ll, v63 = 1415218087808752604ll,
};

template <>
struct glz::meta<RandomI64Enum1>
{
   using enum RandomI64Enum1;
   static constexpr auto value = enumerate(
      v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15,
      v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31,
      v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47,
      v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61, v62, v63);
};

enum class RandomI64Enum2 : int64_t {
   v0 = -4320576372119261223ll, v1 = -4077594026428631998ll, v2 = -4037058126392169589ll, v3 = -3925243403150937539ll,
   v4 = -3775185044658164432ll, v5 = -3693730133137775444ll, v6 = -3678677168680358231ll, v7 = -3662673413155623227ll,
   v8 = -3505232265117544445ll, v9 = -3494733982915752808ll, v10 = -3439247195677398151ll, v11 = -3328061501866390119ll,
   v12 = -3106772471931914564ll, v13 = -3036991454696243030ll, v14 = -2928885756750485998ll, v15 = -2920609872797475944ll,
   v16 = -2919055495889607474ll, v17 = -2700592915007833071ll, v18 = -2620141810975762971ll, v19 = -2583902841929476121ll,
   v20 = -2515226403550688628ll, v21 = -2501119015776694048ll, v22 = -2492426033469818076ll, v23 = -2469641209952557663ll,
   v24 = -2459394852865567781ll, v25 = -2331876246799200871ll, v26 = -2283862070685299439ll, v27 = -2273403649282931277ll,
   v28 = -2211479541987140419ll, v29 = -2180345572247369935ll, v30 = -1969162443289570675ll, v31 = -1875409772267085483ll,
   v32 = -1757418110287737627ll, v33 = -1741743284231354976ll, v34 = -1707997432890722922ll, v35 = -1265344128979294688ll,
   v36 = -826311272503431505ll, v37 = -699024298447522230ll, v38 = -603767749053051588ll, v39 = -524814459894787434ll,
   v40 = -439787125178091564ll, v41 = -313618993549911015ll, v42 = -280389981654904015ll, v43 = -256176593052396192ll,
   v44 = 90200827243037273ll, v45 = 224338441249130200ll, v46 = 266713706750028893ll, v47 = 283480581157381678ll,
   v48 = 429262886585483127ll, v49 = 477428407419566797ll, v50 = 503132067001390243ll, v51 = 526990553464315756ll,
   v52 = 548608864138226231ll, v53 = 694140034284583141ll, v54 = 828217289203684006ll, v55 = 952411512683484083ll,
   v56 = 1137008721084009637ll, v57 = 1151978886418168276ll, v58 = 1153356341813733364ll, v59 = 1153813436578630575ll,
   v60 = 1330228286192529244ll, v61 = 1372122100029621096ll, v62 = 1438961840690848996ll, v63 = 1540472015015337691ll,
};

template <>
struct glz::meta<RandomI64Enum2>
{
   using enum RandomI64Enum2;
   static constexpr auto value = enumerate(
      v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15,
      v16, v17, v18, v19, v20, v21, v22, v23, v24, v25, v26, v27, v28, v29, v30, v31,
      v32, v33, v34, v35, v36, v37, v38, v39, v40, v41, v42, v43, v44, v45, v46, v47,
      v48, v49, v50, v51, v52, v53, v54, v55, v56, v57, v58, v59, v60, v61, v62, v63);
};

// Helper function template for roundtrip test
template <typename E>
void test_enum_roundtrip()
{
   glz::for_each<glz::reflect<E>::size>([&]<size_t I>() {
      E val = static_cast<E>(glz::get<I>(glz::reflect<E>::values));
      std::string json;
      expect(not glz::write_json(val, json));
      E parsed;
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

int main() { return 0; }
