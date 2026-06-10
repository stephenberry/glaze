# glaze-asio.cmake
#
# Single source of truth for selecting the Asio backend used by Glaze's
# networking headers (glaze/ext/glaze_asio.hpp, glaze/net/*).
#
# Including this module declares the glaze_USE_BUNDLED_ASIO option and provides:
#
#   glaze_setup_asio()
#       Creates the `glaze_asio` INTERFACE target and the `glaze::asio` alias,
#       wired to exactly one Asio backend. Link `glaze::asio` alongside
#       `glaze::glaze` from any networking consumer.
#
# Selection order (unchanged from the historical tests/ behavior):
#   1. Boost.Asio      - find_package(Boost CONFIG); also defines GLZ_USE_BOOST_ASIO
#   2. Standalone Asio - find_package(Asio) via the FindAsio.cmake shipped here
#   3. Bundled Asio    - FetchContent, when glaze_USE_BUNDLED_ASIO is ON
#
# Why the GLZ_USE_BOOST_ASIO define matters: glaze/ext/glaze_asio.hpp prefers
# standalone Asio whenever <asio.hpp> is includable, the opposite of the order
# above. On a system with both installed, CMake could select Boost while the
# header silently compiled against standalone Asio. Defining GLZ_USE_BOOST_ASIO
# on the target keeps the header in lockstep with the backend CMake actually
# linked, so the two can never disagree (see issue #2599).

include_guard(GLOBAL)

include(FetchContent)

option(glaze_USE_BUNDLED_ASIO
    "Download Asio via FetchContent if no system Asio/Boost is found (disable for distro builds)"
    ON
)

function(glaze_setup_asio)
    # The target is global; only build it once per configure.
    if(TARGET glaze_asio)
        return()
    endif()

    # 1. Boost.Asio
    find_package(Boost QUIET CONFIG)
    if(Boost_FOUND)
        message(STATUS "glaze: using Boost.Asio")
        add_library(glaze_asio INTERFACE)
        add_library(glaze::asio ALIAS glaze_asio)
        # Pin glaze/ext/glaze_asio.hpp to the Boost backend it would not otherwise
        # select when standalone <asio.hpp> is also visible (issue #2599).
        target_compile_definitions(glaze_asio INTERFACE GLZ_USE_BOOST_ASIO)
        # Boost.Asio is header-only; Boost::system was removed in Boost 1.89
        # (header-only since 1.69), so Boost::headers alone is sufficient.
        if(TARGET Boost::headers)
            target_link_libraries(glaze_asio INTERFACE Boost::headers)
        elseif(TARGET Boost::boost)
            target_link_libraries(glaze_asio INTERFACE Boost::boost)
        endif()
        return()
    endif()

    # 2. Standalone Asio (FindAsio.cmake ships next to this module)
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_FUNCTION_LIST_DIR}")
    find_package(Asio QUIET)
    if(Asio_FOUND)
        message(STATUS "glaze: using standalone Asio ${Asio_VERSION}")
        add_library(glaze_asio INTERFACE)
        add_library(glaze::asio ALIAS glaze_asio)
        target_link_libraries(glaze_asio INTERFACE Asio::Asio)
        return()
    endif()

    # 3. Bundled Asio via FetchContent
    if(glaze_USE_BUNDLED_ASIO)
        message(STATUS "glaze: fetching standalone Asio via FetchContent")
        FetchContent_Declare(
            asio
            GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
            GIT_TAG asio-1-36-0
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(asio)
        add_library(glaze_asio INTERFACE)
        add_library(glaze::asio ALIAS glaze_asio)
        target_include_directories(glaze_asio INTERFACE ${asio_SOURCE_DIR}/asio/include)
        return()
    endif()

    message(FATAL_ERROR
        "Asio not found and bundling is disabled (glaze_USE_BUNDLED_ASIO=OFF).\n"
        "Options:\n"
        "  - Install Boost with headers\n"
        "  - Install standalone Asio (headers in a standard location)\n"
        "  - Set Asio_INCLUDE_DIR to your Asio headers location\n"
        "  - Set glaze_USE_BUNDLED_ASIO=ON to download via FetchContent"
    )
endfunction()
