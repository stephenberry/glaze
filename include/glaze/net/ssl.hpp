// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

// TLS/SSL support shared by the Glaze networking clients: the error category, SNI and
// hostname verification, and everything to do with locating and loading CA trust anchors.
//
// This is where the platform- and OpenSSL-specific warts are deliberately concentrated.
// Trust-anchor discovery differs per operating system and per OpenSSL packaging, so keeping
// it in one auditable place matters more than usual.
//
// On what CI covers, because the glaze_ENABLE_SSL option is misleading: no workflow sets
// that option, but the SSL test targets define GLZ_ENABLE_SSL directly and glaze_BUILD_SSL_TESTS
// defaults to ON, so this header is compiled, linked and run on every job that builds the
// networking tests - Windows included, where the ROOT-store read below executes against a
// live certificate store. Jobs that pass -Dglaze_BUILD_SSL_TESTS=OFF are the exception.
//
// Included by http_client.hpp and websocket_client.hpp. Everything here is inert unless
// GLZ_ENABLE_SSL is defined, apart from the ssl_error category, which stays available so
// callers can report "SSL support was not compiled in" without conditional compilation.

#include <concepts>
#include <cstring>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "glaze/ext/glaze_asio.hpp"
#include "glaze/util/env.hpp"

#ifdef GLZ_ENABLE_SSL
// Windows is the one platform where OpenSSL cannot find the system trust anchors on its
// own, so we read them out of the OS certificate store directly (see
// detail::load_os_ca_certificates). Define GLZ_DISABLE_WINDOWS_CERT_STORE to compile that
// path out.
//
// wincrypt.h is pulled in and sanitized *before* the OpenSSL headers. It defines
// object-identifier macros whose names collide with OpenSSL type names, so leaving them
// defined breaks the OpenSSL declarations below. Doing it in this order also makes this
// header immune to include order in the consuming translation unit: if wincrypt.h was
// already included (directly, or by a <windows.h> without WIN32_LEAN_AND_MEAN), the
// re-include here is a no-op thanks to its include guard, but the #undefs still run.
// Only the colliding names are removed; the Windows constants used below
// (X509_ASN_ENCODING, CERT_FIND_PROP_ONLY_ENHKEY_USAGE_FLAG, szOID_PKIX_KP_SERVER_AUTH)
// are deliberately left intact.
//
// These #undefs are a deliberate, narrow exception to the rule that Glaze does not disturb
// the consumer's macro environment (which cmake/ci/windows-header-compatibility enforces).
// The collision is unavoidable: wincrypt.h and OpenSSL both claim these names, and a
// translation unit cannot use OpenSSL with wincrypt's versions in scope. Note that the
// compatibility job never defines GLZ_ENABLE_SSL, so it does not police this block.
//
// clang-format is switched off across this block on purpose. The config uses
// IncludeBlocks: Regroup with SortIncludes, which would sort <wincrypt.h> ahead of
// <windows.h> (and the OpenSSL headers ahead of both). wincrypt.h does not include
// <windows.h> itself and relies on its types, so the sorted order does not compile.
// clang-format off
#if defined(_WIN32) && !defined(GLZ_DISABLE_WINDOWS_CERT_STORE)
#include <windows.h>
#include <wincrypt.h>
#undef X509_NAME
#undef X509_EXTENSIONS
#undef PKCS7_ISSUER_AND_SERIAL
#undef PKCS7_SIGNER_INFO
#undef OCSP_REQUEST
#undef OCSP_RESPONSE
// The Cert* calls below live in crypt32, so this header - not the build system - is what
// creates that link dependency. Declaring it here keeps the requirement attached to the
// code that causes it, which matters because GLZ_ENABLE_SSL can be defined without going
// through Glaze's CMake at all: the SSL test targets set it directly on the target, and so
// can any consumer. Relying on the glaze_ENABLE_SSL option to name crypt32 leaves every one
// of those routes with an unlinkable header.
//
// MSVC and clang-cl honor this; MinGW ignores it, so a MinGW build that defines
// GLZ_ENABLE_SSL by hand must also link crypt32 by hand, exactly as it already must for
// OpenSSL.
#ifdef _MSC_VER
#pragma comment(lib, "crypt32.lib")
#endif
#endif

#include <openssl/err.h>
#include <openssl/ssl.h> // For SSL_set_tlsext_host_name
#include <openssl/x509.h>
// clang-format on
#endif

namespace glz
{
#ifdef GLZ_ENABLE_SSL
   // The TLS stream type used by the HTTP and WebSocket clients.
   using ssl_socket = asio::ssl::stream<asio::ip::tcp::socket>;
#endif

   // SSL error codes for detailed error reporting
   enum class ssl_error {
      success = 0,
      ssl_not_supported, // HTTPS requested but SSL support not compiled in
      sni_hostname_failed, // Failed to set SNI hostname (SSL_set_tlsext_host_name)
      os_trust_store_unavailable, // The OS certificate store could not be opened, or held no usable anchors
      no_certificates_added // A trust-anchor source was empty, so nothing was added
      // Note: Handshake and certificate errors propagate as native ASIO/OpenSSL error codes
      // for more detailed error information
   };

   // SSL error category for std::error_code integration
   class ssl_error_category : public std::error_category
   {
     public:
      const char* name() const noexcept override { return "glaze.ssl"; }

      std::string message(int ev) const override
      {
         switch (static_cast<ssl_error>(ev)) {
         case ssl_error::success:
            return "Success";
         case ssl_error::ssl_not_supported:
            return "SSL/TLS not supported: GLZ_ENABLE_SSL not defined";
         case ssl_error::sni_hostname_failed:
            return "Failed to set SNI hostname for TLS connection";
         case ssl_error::os_trust_store_unavailable:
            return "Operating system certificate store unavailable or empty";
         case ssl_error::no_certificates_added:
            return "No certificates were added: the provided trust anchor source was empty";
         default:
            return "Unknown SSL error";
         }
      }

      // Map to equivalent standard error conditions where applicable
      std::error_condition default_error_condition(int ev) const noexcept override
      {
         switch (static_cast<ssl_error>(ev)) {
         case ssl_error::ssl_not_supported:
            return std::errc::protocol_not_supported;
         case ssl_error::sni_hostname_failed:
            return std::errc::protocol_error;
         case ssl_error::os_trust_store_unavailable:
            return std::errc::no_such_file_or_directory;
         case ssl_error::no_certificates_added:
            return std::errc::invalid_argument;
         default:
            return std::error_condition(ev, *this);
         }
      }
   };

   // Get the singleton instance of the SSL error category
   inline const ssl_error_category& get_ssl_error_category() noexcept
   {
      static ssl_error_category instance;
      return instance;
   }

   // Create std::error_code from ssl_error
   inline std::error_code make_error_code(ssl_error e) noexcept
   {
      return {static_cast<int>(e), get_ssl_error_category()};
   }
} // namespace glz

// Enable automatic conversion from glz::ssl_error to std::error_code
template <>
struct std::is_error_code_enum<glz::ssl_error> : std::true_type
{};

namespace glz
{
   namespace detail
   {
#ifdef GLZ_ENABLE_SSL
      // Configure SNI and hostname verification for client TLS connections.
      inline bool configure_ssl_client_hostname(ssl_socket& sock, const std::string& host)
      {
         if (!SSL_set_tlsext_host_name(sock.native_handle(), host.c_str())) {
            return false;
         }

         sock.set_verify_callback(asio::ssl::host_name_verification(host));
         return true;
      }

#if defined(_WIN32) && !defined(GLZ_DISABLE_WINDOWS_CERT_STORE)
      // Whether Windows considers this root usable for TLS server authentication.
      //
      // Windows can restrict what a root may be used for through the store's enhanced key
      // usage property, and the Microsoft Trusted Root Program relies on exactly that to
      // disable a root for particular purposes. The restriction lives in the store, not in
      // the encoded certificate, so copying the DER alone would silently promote (say) a
      // code-signing-only root into a TLS anchor.
      //
      // Every uncertain outcome answers "yes". Wrongly keeping a root leaves us no worse
      // than not consulting the property at all, whereas wrongly dropping one removes a
      // trust anchor and breaks HTTPS outright, so the bias runs firmly toward keeping.
      inline bool windows_root_allows_server_auth(PCCERT_CONTEXT entry)
      {
         DWORD size = 0;
         if (!::CertGetEnhancedKeyUsage(entry, CERT_FIND_PROP_ONLY_ENHKEY_USAGE_FLAG, nullptr, &size) ||
             size < sizeof(CERT_ENHKEY_USAGE)) {
            return true; // No usable property; Windows treats the root as unrestricted
         }

         std::vector<unsigned char> buffer(size);
         auto* usage = reinterpret_cast<CERT_ENHKEY_USAGE*>(buffer.data());
         if (!::CertGetEnhancedKeyUsage(entry, CERT_FIND_PROP_ONLY_ENHKEY_USAGE_FLAG, usage, &size)) {
            return true;
         }

         if (usage->cUsageIdentifier == 0) {
            // An empty list means "valid for all uses" only when the property is absent,
            // which CryptoAPI signals with CRYPT_E_NOT_FOUND. An empty list that is
            // actually present means the opposite: valid for nothing.
            return ::GetLastError() == static_cast<DWORD>(CRYPT_E_NOT_FOUND);
         }

         for (DWORD i = 0; i < usage->cUsageIdentifier; ++i) {
            const char* oid = usage->rgpszUsageIdentifier[i];
            if (oid && std::strcmp(oid, szOID_PKIX_KP_SERVER_AUTH) == 0) {
               return true;
            }
         }
         return false;
      }
#endif

      // Load the operating system's native trust anchors into ctx, returning how many
      // certificates were added.
      //
      // Windows is the only platform where this does any work. OpenSSL there never
      // consults the OS certificate store, and packaged builds (vcpkg, Conan) bake in an
      // OPENSSLDIR pointing at the machine that built them -- a directory that generally
      // does not exist on the target. set_default_verify_paths() still reports success in
      // that case because it only registers lookup paths, so the context comes up with
      // verify_peer and zero anchors and every public HTTPS endpoint fails the handshake
      // with SSL_R_CERTIFICATE_VERIFY_FAILED.
      //
      // Elsewhere (Linux, macOS, the BSDs) OpenSSL's default verify paths already resolve
      // to the system bundle, so this returns 0 without touching the context: there,
      // set_default_verify_paths() *is* the OS store.
      inline std::expected<size_t, std::error_code> load_os_ca_certificates([[maybe_unused]] asio::ssl::context& ctx)
      {
#if defined(_WIN32) && !defined(GLZ_DISABLE_WINDOWS_CERT_STORE)
         X509_STORE* store = ::SSL_CTX_get_cert_store(ctx.native_handle());
         if (!store) {
            return std::unexpected(make_error_code(ssl_error::os_trust_store_unavailable));
         }

         HCERTSTORE os_store = ::CertOpenSystemStoreW(0, L"ROOT");
         if (!os_store) {
            return std::unexpected(std::error_code(static_cast<int>(::GetLastError()), std::system_category()));
         }

         size_t added = 0;
         PCCERT_CONTEXT entry = nullptr;
         while ((entry = ::CertEnumCertificatesInStore(os_store, entry)) != nullptr) {
            if ((entry->dwCertEncodingType & X509_ASN_ENCODING) == 0) {
               continue; // Not a DER-encoded X.509 certificate
            }
            if (!windows_root_allows_server_auth(entry)) {
               continue; // Windows does not trust this root for TLS server authentication
            }
            const unsigned char* der = entry->pbCertEncoded;
            X509* cert = ::d2i_X509(nullptr, &der, static_cast<long>(entry->cbCertEncoded));
            if (!cert) {
               // Skip anything OpenSSL cannot parse rather than failing the whole load.
               ::ERR_clear_error();
               continue;
            }
            if (::X509_STORE_add_cert(store, cert) == 1) {
               ++added;
            }
            else {
               // Drain this attempt's errors with ERR_get_error (which pops) rather than
               // peeking, so the verdict cannot be swayed by an entry left over from an
               // earlier iteration.
               bool already_present = false;
               for (unsigned long err = ::ERR_get_error(); err != 0; err = ::ERR_get_error()) {
                  // Reason codes are only unique within a library, so check both halves.
                  if (ERR_GET_LIB(err) == ERR_LIB_X509 && ERR_GET_REASON(err) == X509_R_CERT_ALREADY_IN_HASH_TABLE) {
                     // OpenSSL before 3.0 reports an already-present certificate as a
                     // failure. The anchor is in the store either way, so count it.
                     already_present = true;
                  }
               }
               if (already_present) {
                  ++added;
               }
            }
            ::X509_free(cert);
         }
         ::CertCloseStore(os_store, 0);

         // Belt and braces: the loop drains its own failures, but never let this function
         // leave anything on the error queue that a later handshake could misattribute.
         ::ERR_clear_error();

         if (added == 0) {
            return std::unexpected(make_error_code(ssl_error::os_trust_store_unavailable));
         }
         return added;
#else
         return size_t{0};
#endif
      }

      // Trust-anchor operations on a bare context. The clients own the locking, which is
      // the only thing that differs between them (http_client guards an eagerly created
      // context with a shared_mutex; websocket_client guards a lazily created one with a
      // mutex), so everything else lives here and is written once.

      // Seed a freshly created client context with whatever anchors the platform offers.
      // Both steps are best effort: a platform whose default verify paths resolve to
      // nothing (notably Windows) still gets its OS store, and callers can add their own
      // on top either way.
      inline void seed_platform_trust_anchors(asio::ssl::context& ctx)
      {
         asio::error_code ec;
         ctx.set_default_verify_paths(ec);
         (void)load_os_ca_certificates(ctx);
      }

      inline std::expected<void, std::error_code> add_ca_file(asio::ssl::context& ctx, std::string_view path)
      {
         asio::error_code ec;
         ctx.load_verify_file(std::string(path), ec);
         if (ec) {
            return std::unexpected(std::error_code(ec));
         }
         return {};
      }

      inline std::expected<void, std::error_code> add_ca_directory(asio::ssl::context& ctx, std::string_view path)
      {
         asio::error_code ec;
         ctx.add_verify_path(std::string(path), ec);
         if (ec) {
            return std::unexpected(std::error_code(ec));
         }
         return {};
      }

      // Empty input is rejected rather than passed down: a default-constructed string_view
      // has null data, which makes OpenSSL's BIO_new_mem_buf return null, and asio then
      // reports success having added nothing. An empty bundle would otherwise look loaded
      // while leaving the caller with no anchors at all.
      inline std::expected<void, std::error_code> add_ca_pem(asio::ssl::context& ctx, std::string_view pem)
      {
         if (pem.empty()) {
            return std::unexpected(make_error_code(ssl_error::no_certificates_added));
         }
         asio::error_code ec;
         ctx.add_certificate_authority(asio::const_buffer(pem.data(), pem.size()), ec);
         if (ec) {
            return std::unexpected(std::error_code(ec));
         }
         return {};
      }

      enum class ssl_ca_source { explicit_file, env_ssl_cert_file, env_ssl_cert_dir, default_verify_paths };

      inline std::optional<std::string_view> non_empty_path(std::optional<std::string_view> value)
      {
         if (value && !value->empty()) {
            return value;
         }
         return std::nullopt;
      }

      inline std::optional<std::string_view> to_sv_opt(const std::optional<std::string>& value)
      {
         if (value && !value->empty()) {
            return std::string_view{*value};
         }
         return std::nullopt;
      }

      std::optional<std::string_view> to_sv_opt(std::optional<std::string>&&) = delete;

      template <typename Loader>
      concept ssl_ca_path_loader = requires(Loader&& loader, std::string_view path) {
         { std::forward<Loader>(loader)(path) } -> std::convertible_to<std::error_code>;
      };

      template <typename Loader>
      concept ssl_ca_default_loader = requires(Loader&& loader) {
         { std::forward<Loader>(loader)() } -> std::convertible_to<std::error_code>;
      };

      template <ssl_ca_path_loader LoadFile, ssl_ca_path_loader LoadDir, ssl_ca_default_loader LoadDefault>
      inline std::expected<ssl_ca_source, std::error_code> configure_ssl_ca_fallback(
         std::optional<std::string_view> explicit_file, std::optional<std::string_view> env_cert_file,
         std::optional<std::string_view> env_cert_dir, LoadFile&& load_file, LoadDir&& load_dir,
         LoadDefault&& load_default)
      {
         std::optional<std::error_code> last_error{};

         const auto try_file = [&](std::optional<std::string_view> path,
                                   ssl_ca_source source) -> std::optional<ssl_ca_source> {
            if (auto non_empty = non_empty_path(path)) {
               if (const std::error_code ec = load_file(*non_empty); ec) {
                  last_error = ec;
               }
               else {
                  return source;
               }
            }
            return std::nullopt;
         };

         if (auto source = try_file(explicit_file, ssl_ca_source::explicit_file)) {
            return *source;
         }

         if (auto source = try_file(env_cert_file, ssl_ca_source::env_ssl_cert_file)) {
            return *source;
         }

         if (auto non_empty = non_empty_path(env_cert_dir)) {
            if (const std::error_code ec = load_dir(*non_empty); ec) {
               last_error = ec;
            }
            else {
               return ssl_ca_source::env_ssl_cert_dir;
            }
         }

         if (const std::error_code ec = load_default(); ec) {
            if (last_error) {
               return std::unexpected(*last_error);
            }
            return std::unexpected(ec);
         }

         return ssl_ca_source::default_verify_paths;
      }
#endif
   } // namespace detail
} // namespace glz
