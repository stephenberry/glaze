// UTF-8 validation tests.
//
// Validation runs on whichever SIMD backend the build selected (AVX-512BW, AVX2, SSSE3, AArch64
// NEON, WASM SIMD128, or the scalar fallback), so every test here is backend agnostic and the whole
// file is worth running on each target. Register width varies between backends, which is why so
// much of this file sweeps byte offsets: a bug in cross-register carry or in the padded tail block
// only shows up when a sequence straddles a boundary the host happens to use.

#include "glaze/simd/utf8_validation.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <map>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#include "glaze/glaze.hpp"
#include "ut/ut.hpp"

using namespace ut;

namespace
{
   bool valid(std::string_view s) noexcept { return glz::validate_utf8(s.data(), s.size()); }

   bool valid_scalar(std::string_view s) noexcept
   {
      const auto* p = reinterpret_cast<const uint8_t*>(s.data());
      return glz::validate_utf8_scalar(p, p + s.size());
   }

   // Encode a code point directly so tests can build sequences the encoder would refuse.
   std::string utf8(uint32_t cp)
   {
      std::string out;
      if (cp <= 0x7F) {
         out += char(cp);
      }
      else if (cp <= 0x7FF) {
         out += char(0xC0 | (cp >> 6));
         out += char(0x80 | (cp & 0x3F));
      }
      else if (cp <= 0xFFFF) {
         out += char(0xE0 | (cp >> 12));
         out += char(0x80 | ((cp >> 6) & 0x3F));
         out += char(0x80 | (cp & 0x3F));
      }
      else {
         out += char(0xF0 | (cp >> 18));
         out += char(0x80 | ((cp >> 12) & 0x3F));
         out += char(0x80 | ((cp >> 6) & 0x3F));
         out += char(0x80 | (cp & 0x3F));
      }
      return out;
   }

   std::string bytes(std::initializer_list<int> bs)
   {
      std::string s;
      for (int b : bs) s += char(b);
      return s;
   }

   // Every distinct way a byte sequence can be malformed UTF-8.
   struct named_bytes
   {
      const char* name;
      std::string data;
   };

   const std::vector<named_bytes>& malformed()
   {
      static const std::vector<named_bytes> v{
         {"lone continuation 0x80", bytes({0x80})},
         {"lone continuation 0xBF", bytes({0xBF})},
         {"two lone continuations", bytes({0x80, 0xBF})},
         {"overlong NUL C0 80", bytes({0xC0, 0x80})},
         {"overlong C1 BF", bytes({0xC1, 0xBF})},
         {"overlong 3 byte E0 80 80", bytes({0xE0, 0x80, 0x80})},
         {"overlong 3 byte E0 9F BF", bytes({0xE0, 0x9F, 0xBF})},
         {"overlong 4 byte F0 80 80 80", bytes({0xF0, 0x80, 0x80, 0x80})},
         {"overlong 4 byte F0 8F BF BF", bytes({0xF0, 0x8F, 0xBF, 0xBF})},
         {"surrogate D800 encoded", bytes({0xED, 0xA0, 0x80})},
         {"surrogate DFFF encoded", bytes({0xED, 0xBF, 0xBF})},
         {"above U+10FFFF F4 90 80 80", bytes({0xF4, 0x90, 0x80, 0x80})},
         {"5 byte sequence F8", bytes({0xF8, 0x88, 0x80, 0x80, 0x80})},
         {"6 byte sequence FC", bytes({0xFC, 0x84, 0x80, 0x80, 0x80, 0x80})},
         {"invalid byte FE", bytes({0xFE})},
         {"invalid byte FF", bytes({0xFF})},
         {"truncated 2 byte", bytes({0xC3})},
         {"truncated 3 byte at 1", bytes({0xE2})},
         {"truncated 3 byte at 2", bytes({0xE2, 0x82})},
         {"truncated 4 byte at 1", bytes({0xF0})},
         {"truncated 4 byte at 2", bytes({0xF0, 0x9F})},
         {"truncated 4 byte at 3", bytes({0xF0, 0x9F, 0x98})},
         {"lead then lead", bytes({0xC3, 0xC3, 0xA9})},
         {"lead then ASCII", bytes({0xC3, 0x41})},
         {"3 byte lead then ASCII", bytes({0xE2, 0x82, 0x41})},
         {"continuation after complete seq", bytes({0xC3, 0xA9, 0x80})},
      };
      return v;
   }

   const std::vector<named_bytes>& well_formed()
   {
      static const std::vector<named_bytes> v{
         {"empty", ""},
         {"ascii", "hello world"},
         {"U+0000", std::string(1, '\0')},
         {"U+007F", utf8(0x7F)},
         {"U+0080 first 2 byte", utf8(0x80)},
         {"U+07FF last 2 byte", utf8(0x7FF)},
         {"U+0800 first 3 byte", utf8(0x800)},
         {"U+D7FF last before surrogates", utf8(0xD7FF)},
         {"U+E000 first after surrogates", utf8(0xE000)},
         {"U+FFFF last 3 byte", utf8(0xFFFF)},
         {"U+10000 first 4 byte", utf8(0x10000)},
         {"U+10FFFF last code point", utf8(0x10FFFF)},
         {"euro sign", utf8(0x20AC)},
         {"CJK", utf8(0x65E5) + utf8(0x672C) + utf8(0x8A9E)},
         {"emoji", utf8(0x1F600)},
         {"mixed", "a" + utf8(0xE9) + "b" + utf8(0x20AC) + "c" + utf8(0x1F600) + "d"},
      };
      return v;
   }
}

// -------------------------------------------------------------------------------------------
// Backend identification
//
// glz::simd_info names what each accelerated subsystem compiled to. The assertions live here
// because this is the file the simd-backends workflow runs against every target, so each one is
// checked on every backend Glaze can select rather than only on the host.
// -------------------------------------------------------------------------------------------

namespace
{
   // Every value simd_info.detected can take, paired with what the other subsystems must report on
   // a build that detected it. This table IS the divergence: reading down the columns shows why one
   // name for "the SIMD Glaze is using" would be wrong on most builds.
   //
   // float_write is absent on purpose -- zmij runs its own detection off __SSE2__ / __ARM_NEON, so
   // it is not a function of what Glaze detected. It is checked separately below.
   struct build_shape
   {
      std::string_view detected{};
      std::string_view utf8{}; // validator: needs a byte-granular shuffle, else falls back
      std::string_view escape{}; // string escaping: no AVX-512 or WASM helper exists
   };

   inline constexpr build_shape known_builds[]{
      {"AVX512BW", "AVX512BW", "AVX2"}, // no AVX-512 escape helper
      {"AVX2", "AVX2", "AVX2"}, //
      {"SSSE3", "SSSE3", "SSE2"}, // no SSSE3 escape helper
      {"SSE2", "scalar", "SSE2"}, // SSE2 has no byte-granular shuffle
      {"NEON64", "NEON64", "NEON"}, //
      {"NEON", "scalar", "NEON"}, // 32 bit NEON has no vqtbl1q_u8
      {"WASM_SIMD128", "WASM_SIMD128", "SWAR"}, // no WASM escape helper
      {"scalar", "scalar", "SWAR"}, //
   };

   constexpr const build_shape* shape_for(std::string_view detected) noexcept
   {
      for (const auto& b : known_builds) {
         if (b.detected == detected) return &b;
      }
      return nullptr;
   }

   inline constexpr size_t unknown_width = size_t(-1);

   struct utf8_backend_name
   {
      std::string_view name{};
      size_t width{}; // register width the validator uses under this name; 0 for the scalar fallback
   };

   inline constexpr utf8_backend_name known_utf8_backends[]{
      {"AVX512BW", 64},  {"AVX2", 32},      {"SSSE3", 16},     {"NEON64", 16}, {"WASM_SIMD128", 16},
      {"generic16", 16}, {"generic32", 32}, {"generic64", 64}, {"scalar", 0},
   };

   constexpr size_t width_for(std::string_view backend) noexcept
   {
      for (const auto& b : known_utf8_backends) {
         if (b.name == backend) return b.width;
      }
      return unknown_width;
   }

   inline constexpr std::string_view known_float_write[]{"NEON", "SSE4.1", "SSE2", "scalar"};

   constexpr bool is_known_float_write(std::string_view name) noexcept
   {
      for (const auto& known : known_float_write) {
         if (known == name) return true;
      }
      return false;
   }
}

// These spellings are public API, so a new one should be added here and to
// docs/optimizing-performance.md deliberately rather than arriving as a side effect of adding a
// backend.
static_assert(shape_for(glz::simd_info.detected) != nullptr,
              "simd_info.detected reports a value this test does not know");
static_assert(width_for(glz::simd_info.utf8_validation) != unknown_width,
              "simd_info.utf8_validation reports a value this test does not know");
static_assert(is_known_float_write(glz::simd_info.float_write),
              "simd_info.float_write reports a value this test does not know");

// String escaping is a pure function of what was detected, with no testing hook to fence against.
static_assert(shape_for(glz::simd_info.detected)->escape == glz::simd_info.string_escape,
              "string escaping does not match the helper this build's instruction set selects");

#if !defined(GLZ_UTF8_GENERIC_WIDTH)
// GLZ_UTF8_GENERIC_WIDTH overrides the validator with the portable path, so the mapping only holds
// when that hook is off. Without this fence the row for a target with no vector path (s390x, armv7
// without NEON) would fire, since it expects "scalar" and would get "generic64".
static_assert(shape_for(glz::simd_info.detected)->utf8 == glz::simd_info.utf8_validation,
              "the UTF-8 validator does not match what this build's instruction set selects");
#endif

#if defined(GLZ_UTF8_SIMD)
// Ties the reported name to the register width its own branch defines. The table above pins the
// name, so what this adds is the width: a branch that declares the wrong width for its register
// type would otherwise only surface as wrong validation results at runtime.
static_assert(width_for(glz::simd_info.utf8_validation) == glz::detail::utf8_simd::width,
              "a UTF-8 backend named itself as one register width and then defined another");
#else
static_assert(glz::simd_info.utf8_validation == "scalar");
#endif

#if defined(GLZ_DISABLE_SIMD)
// The one promise that spans all four fields: asking for no vector path gets none anywhere,
// including from zmij, which otherwise ignores Glaze's detection entirely.
static_assert(glz::simd_info.detected == "scalar" && glz::simd_info.utf8_validation == "scalar" &&
                 glz::simd_info.string_escape == "SWAR" && glz::simd_info.float_write == "scalar",
              "GLZ_DISABLE_SIMD left a vector path enabled");
#endif

suite backend_identification = [] {
   // Echoed so a failing run in any of the backend jobs says which build it was, without needing a
   // separate probe program to establish it. Written as JSON because that is the form a benchmark
   // report wants, and it exercises the reflection users will actually use.
   "reports what this build compiled"_test = [] {
      std::string report;
      expect(not glz::write_json(glz::simd_info, report));
      std::cout << report << '\n';
   };
};

// -------------------------------------------------------------------------------------------
// The validator itself
// -------------------------------------------------------------------------------------------

suite validator_basics = [] {
   "well formed accepted"_test = [] {
      for (const auto& c : well_formed()) {
         expect(valid(c.data)) << c.name;
      }
   };

   "malformed rejected"_test = [] {
      for (const auto& c : malformed()) {
         expect(!valid(c.data)) << c.name;
      }
   };

   // The scalar path is the reference the SIMD backends are built to match, and is itself used for
   // inputs shorter than one register, so it must agree everywhere.
   "scalar and active backend agree"_test = [] {
      for (const auto& c : well_formed()) {
         expect(valid(c.data) == valid_scalar(c.data)) << c.name;
      }
      for (const auto& c : malformed()) {
         expect(valid(c.data) == valid_scalar(c.data)) << c.name;
      }
   };

   // Every corpus case above is shorter than one register, so validate() dispatches to the scalar
   // path and none of the tests above actually reach a SIMD backend. Re-run the whole corpus
   // embedded in an ASCII carrier long enough to force the vector path, at offsets that straddle
   // every register and step boundary any backend uses. ASCII is never a continuation byte, so the
   // carrier cannot complete a malformed case or break a well formed one.
   "corpus embedded in a carrier reaches the vector path"_test = [] {
      constexpr size_t offsets[]{0, 1, 7, 15, 16, 17, 31, 32, 33, 47, 63, 64, 65, 96, 127};
      for (const size_t pad : offsets) {
         for (const auto& c : well_formed()) {
            const std::string s = std::string(pad, 'a') + c.data + std::string(160, 'z');
            expect(valid(s)) << c.name << " at offset " << pad;
            expect(valid(s) == valid_scalar(s)) << c.name << " at offset " << pad;
         }
         for (const auto& c : malformed()) {
            const std::string s = std::string(pad, 'a') + c.data + std::string(160, 'z');
            expect(!valid(s)) << c.name << " at offset " << pad;
            expect(valid(s) == valid_scalar(s)) << c.name << " at offset " << pad;
         }
      }
   };

   "every code point round trips"_test = [] {
      // All of Unicode except the surrogate block, which is not encodable.
      for (uint32_t cp = 0; cp <= 0x10FFFF; ++cp) {
         if (cp >= 0xD800 && cp <= 0xDFFF) continue;
         const std::string s = utf8(cp);
         if (!valid(s)) {
            expect(false) << "rejected valid code point " << cp;
            break;
         }
      }
   };

   "surrogate code points are unencodable"_test = [] {
      for (uint32_t cp = 0xD800; cp <= 0xDFFF; ++cp) {
         const std::string s = utf8(cp); // produces the ED A0.. form
         expect(!valid(s)) << cp;
      }
   };

   "every lead byte with wrong follower"_test = [] {
      for (int lead = 0xC2; lead <= 0xF4; ++lead) {
         for (int follow = 0x00; follow <= 0xFF; ++follow) {
            if (follow >= 0x80 && follow <= 0xBF) continue; // a plausible continuation
            expect(!valid(bytes({lead, follow}))) << lead << "," << follow;
         }
      }
   };

   "all single bytes"_test = [] {
      for (int b = 0; b <= 0xFF; ++b) {
         const bool want = b < 0x80; // a lone byte is valid only if ASCII
         expect(valid(bytes({b})) == want) << b;
      }
   };
};

// -------------------------------------------------------------------------------------------
// Register boundaries. Backends use 16, 32, or 64 byte registers, and the driver steps 64 bytes
// at a time with a padded final block, so an error must be caught at every offset and length.
// -------------------------------------------------------------------------------------------

suite register_boundaries = [] {
   "bad byte at every offset"_test = [] {
      // Long enough to span several 64 byte steps on any backend.
      constexpr size_t n = 200;
      for (size_t off = 0; off < n; ++off) {
         std::string s(n, 'a');
         s[off] = char(0xFF);
         expect(!valid(s)) << off;
      }
   };

   "truncated sequence at every offset"_test = [] {
      constexpr size_t n = 200;
      for (size_t off = 0; off + 1 < n; ++off) {
         std::string s(n, 'a');
         s[off] = char(0xE2); // 3 byte lead followed by ASCII
         expect(!valid(s)) << off;
      }
   };

   "multibyte sequence straddling every offset"_test = [] {
      // A valid sequence placed at each offset must stay valid no matter which register it crosses.
      for (const uint32_t cp : {0xE9u, 0x20ACu, 0x1F600u}) {
         const std::string seq = utf8(cp);
         for (size_t off = 0; off + seq.size() <= 200; ++off) {
            std::string s(200, 'a');
            s.replace(off, seq.size(), seq);
            expect(valid(s)) << cp << "@" << off;
         }
      }
   };

   "sequence truncated by end of buffer at every length"_test = [] {
      // Cutting a valid document short must always be caught, including when the cut lands exactly
      // on a register boundary and the final block is the padded one.
      for (const uint32_t cp : {0xE9u, 0x20ACu, 0x1F600u}) {
         const std::string seq = utf8(cp);
         for (size_t prefix = 0; prefix <= 200; ++prefix) {
            std::string s(prefix, 'a');
            s += seq;
            for (size_t cut = 1; cut < seq.size(); ++cut) {
               expect(!valid(s.substr(0, s.size() - cut))) << cp << " prefix " << prefix << " cut " << cut;
            }
         }
      }
   };

   "all ASCII of every length"_test = [] {
      for (size_t n = 0; n <= 300; ++n) {
         expect(valid(std::string(n, 'x'))) << n;
      }
   };

   "all non-ASCII of every length"_test = [] {
      // Exercises the non-ASCII path continuously, so cross-register carry is used at every step.
      for (size_t n = 1; n <= 150; ++n) {
         std::string s;
         while (s.size() < n) s += utf8(0xE9);
         s.resize(n - (n % 2)); // keep whole 2 byte sequences
         if (s.empty()) continue;
         expect(valid(s)) << n;
      }
   };

   "error only in the final padded block"_test = [] {
      // Lands the bad byte past the last full register so only the padded tail can catch it.
      for (size_t n = 1; n <= 200; ++n) {
         std::string s(n, 'a');
         s.back() = char(0xC3); // truncated 2 byte sequence at the very end
         expect(!valid(s)) << n;
      }
   };
};

// -------------------------------------------------------------------------------------------
// Differential fuzz against the scalar reference
// -------------------------------------------------------------------------------------------

suite differential = [] {
   "random bytes match scalar"_test = [] {
      std::mt19937 rng{20240607};
      std::uniform_int_distribution<int> any_byte(0, 255);
      std::uniform_int_distribution<int> utf8ish(0x80, 0xF7);
      size_t mismatches = 0;
      for (size_t len = 0; len <= 160 && mismatches == 0; ++len) {
         for (int trial = 0; trial < 300; ++trial) {
            std::string s;
            for (size_t i = 0; i < len; ++i) {
               s += char(rng() % 3 ? utf8ish(rng) : any_byte(rng));
            }
            if (valid(s) != valid_scalar(s)) {
               ++mismatches;
               expect(false) << "len " << len;
               break;
            }
         }
      }
      expect(mismatches == 0u);
   };

   "corrupted valid text matches scalar"_test = [] {
      std::mt19937 rng{991};
      std::uniform_int_distribution<int> any_byte(0, 255);
      const std::array<uint32_t, 5> cps{'a', 0xE9, 0x20AC, 0x1F600, 0x7F};
      size_t mismatches = 0;
      for (size_t len = 1; len <= 160 && mismatches == 0; ++len) {
         std::string base;
         while (base.size() < len) base += utf8(cps[rng() % cps.size()]);
         for (int trial = 0; trial < 60; ++trial) {
            std::string s = base;
            s[rng() % s.size()] = char(any_byte(rng));
            if (valid(s) != valid_scalar(s)) {
               ++mismatches;
               expect(false) << "len " << len;
               break;
            }
         }
      }
      expect(mismatches == 0u);
   };
};

// -------------------------------------------------------------------------------------------
// Integration with the JSON reader
// -------------------------------------------------------------------------------------------

suite json_integration = [] {
   "malformed rejected in string values"_test = [] {
      for (const auto& c : malformed()) {
         const std::string doc = "[\"" + c.data + "\"]";
         std::vector<std::string> v;
         const auto ec = glz::read_json(v, doc);
         expect(bool(ec)) << c.name;
      }
   };

   "well formed accepted in string values"_test = [] {
      for (const auto& c : well_formed()) {
         if (c.data.find('\0') != std::string::npos) continue; // control chars need escaping
         const std::string doc = "[\"" + c.data + "\"]";
         std::vector<std::string> v;
         expect(!glz::read_json(v, doc)) << c.name;
         expect(v.size() == 1u) << c.name;
         expect(v[0] == c.data) << c.name;
      }
   };

   "malformed rejected across string target types"_test = [] {
      const std::string doc = "[\"a\xFF!b\"]";
      {
         std::vector<std::string> v;
         expect(glz::read_json(v, doc) == glz::error_code::invalid_utf8);
      }
      {
         std::vector<std::string_view> v;
         expect(glz::read_json(v, doc) == glz::error_code::invalid_utf8);
      }
      {
         std::vector<std::u8string> v;
         expect(glz::read_json(v, doc) == glz::error_code::invalid_utf8);
      }
      {
         glz::generic g;
         expect(glz::read_json(g, doc) == glz::error_code::invalid_utf8);
      }
      {
         std::array<std::string, 1> v;
         expect(bool(glz::read_json(v, doc)));
      }
   };

   "malformed rejected in object keys"_test = [] {
      const std::string doc = "{\"k\xFF!\":1}";
      {
         std::map<std::string, int> m;
         expect(glz::read_json(m, doc) == glz::error_code::invalid_utf8);
      }
      {
         glz::generic g;
         expect(glz::read_json(g, doc) == glz::error_code::invalid_utf8);
      }
      expect(bool(glz::validate_json(doc)));
   };

   "malformed rejected at many string lengths"_test = [] {
      // Reader string paths differ for short strings and the SWAR bulk loop, and the bad byte must
      // be caught at any position within either.
      for (size_t n = 1; n <= 100; ++n) {
         for (size_t pos = 0; pos < n; pos += 7) {
            std::string payload(n, 'a');
            payload[pos] = char(0xFF);
            const std::string doc = "[\"" + payload + "\"]";
            std::vector<std::string> v;
            expect(glz::read_json(v, doc) == glz::error_code::invalid_utf8) << n << "@" << pos;
         }
      }
   };

   "validate_json agrees with read_json"_test = [] {
      for (const auto& c : malformed()) {
         const std::string doc = "[\"" + c.data + "\"]";
         expect(bool(glz::validate_json(doc))) << c.name;
      }
      for (const auto& c : well_formed()) {
         if (c.data.find('\0') != std::string::npos) continue;
         const std::string doc = "[\"" + c.data + "\"]";
         expect(!glz::validate_json(doc)) << c.name;
      }
   };

   "escapes are validated independently"_test = [] {
      // A lone surrogate escape is ASCII on the wire, so only the escape decoder can reject it.
      {
         std::vector<std::string> v;
         expect(bool(glz::read_json(v, std::string(R"(["a\ud800b"])"))));
      }
      {
         std::vector<std::string> v;
         expect(bool(glz::read_json(v, std::string(R"(["\udfff"])"))));
      }
      // A well formed surrogate pair decodes to a 4 byte sequence and must be accepted.
      {
         std::vector<std::string> v;
         expect(!glz::read_json(v, std::string(R"(["😀"])")));
         expect(v.size() == 1u);
         expect(v[0] == utf8(0x1F600));
      }
   };

   "raw byte and escaped forms agree"_test = [] {
      for (const uint32_t cp : {0xE9u, 0x20ACu}) {
         std::vector<std::string> raw, escaped;
         char buf[16];
         std::snprintf(buf, sizeof(buf), R"(["\u%04X"])", cp);
         expect(!glz::read_json(raw, "[\"" + utf8(cp) + "\"]"));
         expect(!glz::read_json(escaped, std::string(buf)));
         expect(raw == escaped) << cp;
      }
   };

   "non null terminated buffers"_test = [] {
      const std::string doc = "[\"a\xFF!b\"]";
      std::string_view sv{doc.data(), doc.size()};
      std::vector<std::string> v;
      expect(bool(glz::read<glz::opts{.null_terminated = false}>(v, sv)));
   };

   "error reports a position"_test = [] {
      const std::string doc = "[\"abc\xFF\"]";
      std::vector<std::string> v;
      const auto ec = glz::read_json(v, doc);
      expect(ec == glz::error_code::invalid_utf8);
      expect(!glz::format_error(ec, doc).empty());
   };
};

int main() { return 0; }
