// Glaze Library
// For the license information refer to glaze.ixx
module;

#if defined(_WIN32) || defined(__CYGWIN__)
#ifndef GLAZE_API_ON_WINDOWS
#define GLAZE_API_ON_WINDOWS
#endif
#ifndef NOMINMAX
#define NOMINMAX
#define GLAZE_API_UNDEF_NOMINMAX
#endif
#include <windows.h>
#ifdef GLAZE_API_UNDEF_NOMINMAX
#undef NOMINMAX
#undef GLAZE_API_UNDEF_NOMINMAX
#endif
#elif defined(__APPLE__)
#include <dlfcn.h>
#elif __has_include(<dlfcn.h>)
#include <dlfcn.h>
#endif
export module glaze.api.lib;

import std;

import glaze.api.api;

namespace glz
{
#ifdef GLAZE_API_ON_WINDOWS
   inline constexpr auto shared_library_extension = ".dll";
   using lib_t = HINSTANCE;
#elif defined(__APPLE__)
   inline constexpr auto shared_library_extension = ".dylib";
   using lib_t = void*;
#else
   inline constexpr auto shared_library_extension = ".so";
   using lib_t = void*;
#endif

   export struct lib_loader final
   {
      using create = iface_fn (*)() noexcept;

      iface api_map{};
      std::vector<lib_t> loaded_libs{};

      void load(const std::string_view path)
      {
         const std::filesystem::path libpath(path);
         if (std::filesystem::is_directory(libpath)) {
            load_libs(path);
         }
         else if (libpath.extension() == shared_library_extension) {
            load_lib(libpath.string());
         }
         else {
            load_lib_by_name(libpath.string());
         }
      }

      void load_libs(const std::string_view directory)
      {
         const std::filesystem::path dir{directory};
         for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            if (entry.is_regular_file() && entry.path().extension() == shared_library_extension) {
               load_lib(entry.path().string());
            }
         }
      }

      auto& operator[](const std::string_view lib_name) { return api_map[std::string(lib_name)]; }

      lib_loader() = default;
      lib_loader(const lib_loader&) = delete;
      lib_loader(lib_loader&&) = delete;
      lib_loader& operator=(const lib_loader&) = delete;
      lib_loader& operator=(lib_loader&&) = delete;

      lib_loader(const std::string_view directory) { load(directory); }

      ~lib_loader()
      {
         api_map.clear();
         for (const auto& lib : loaded_libs) {
#ifdef GLAZE_API_ON_WINDOWS
            FreeLibrary(lib);
#else
            dlclose(lib);
#endif
         }
      }

     private:
      bool load_lib(const std::string& path) noexcept
      {
#ifdef GLAZE_API_ON_WINDOWS
         std::filesystem::path file_path(path);
         lib_t loaded_lib = LoadLibraryW(file_path.c_str());
#else
         lib_t loaded_lib = dlopen(path.c_str(), RTLD_LAZY);
#endif
         if (loaded_lib) {
            loaded_libs.emplace_back(loaded_lib);

#ifdef GLAZE_API_ON_WINDOWS
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
            const auto ptr = reinterpret_cast<create>(GetProcAddress(loaded_lib, "glz_iface"));
#pragma GCC diagnostic pop
#else
            const auto ptr = reinterpret_cast<create>(GetProcAddress(loaded_lib, "glz_iface"));
#endif
#else
            auto* ptr = reinterpret_cast<create>(dlsym(loaded_lib, "glz_iface"));
#endif
            if (ptr) {
               std::shared_ptr<glz::iface> shared_iface_ptr = ptr()();
               api_map.merge(*shared_iface_ptr);
               return true;
            }
         }

         return false;
      }

      bool load_lib_by_name(const std::string& path)
      {
#ifdef NDEBUG
         static std::string suffix = "";
#else
         static std::string suffix = "_d";
#endif
         const std::filesystem::path combined_path(path + suffix + shared_library_extension);

         return (load_lib(std::filesystem::canonical(combined_path).string()));
      }
   };

}
