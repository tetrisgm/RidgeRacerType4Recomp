cmake_minimum_required(VERSION 3.20)

set(PSXRECOMP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/psxrecomp" CACHE PATH
    "Path to psxrecomp")
if(NOT EXISTS "${PSXRECOMP_ROOT}/runtime/runtime.cmake")
    message(FATAL_ERROR
        "PSXRecomp runtime is missing: ${PSXRECOMP_ROOT}/runtime/runtime.cmake\n"
        "Run: git submodule update --init --recursive\n"
        "or set -DPSXRECOMP_ROOT=<path-to-psxrecomp>.")
endif()

# macOS: portable binaries by default, both for the release setup host and for
# the game a player builds from it. Use the static SDL3 psxrecomp pins rather
# than a Homebrew dylib, skip recomp-ui's optional FreeType/HarfBuzz (emoji
# stay outline glyphs), and target a macOS older than the build machine.
# Cache defaults, so a local -D still overrides them.
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
    set(CMAKE_DISABLE_FIND_PACKAGE_SDL3 ON CACHE BOOL
        "Use psxrecomp's pinned static SDL3, not a system dylib")
    set(CMAKE_DISABLE_FIND_PACKAGE_Freetype ON CACHE BOOL
        "Skip optional FreeType/HarfBuzz (Homebrew dylibs)")
endif()

project(RidgeRacerType4Recomp LANGUAGES C CXX)

# Developer tests. OFF by default: tests/, tools/ and the disc are not in the
# setup-host zip, and the player's Generate & rebuild configures this file with
# no -DBUILD_TESTING=OFF, so an unconditional include(CTest) would register
# sources the zip cannot build. Configure with -DR4_BUILD_TESTS=ON to get them.
option(R4_BUILD_TESTS "Register R4's developer tests (needs tests/ and tools/)" OFF)
if(R4_BUILD_TESTS)
    include(CTest)
endif()

# --- Opt-in product surfaces (OFF by default in the framework) -------------
# PSX_NETPLAY must be set BEFORE runtime.cmake is included: that is where
# recomp-net / the lobby client / rbengine are wired. ENABLE_NETPLAY_IF_PRESENT
# below is only supplemental.
if(EXISTS "${PSXRECOMP_ROOT}/lib/recomp-net/CMakeLists.txt")
    set(PSX_NETPLAY ON CACHE BOOL
        "Link recomp-net rollback/delay-sync netplay (needs recomp-net)" FORCE)
endif()
set(PSX_SETUP_WIZARD ON CACHE BOOL
    "Advertise first-run setup wizard + Generate & rebuild in recomp-ui" FORCE)
include("${PSXRECOMP_ROOT}/runtime/runtime.cmake")

# This project's own mod catalog. The FRAMEWORK stages it -- together with its
# builtin psx.* packages -- into <exe-dir>/mods/bundled. Never copy it by hand
# (see psxrecomp/docs/MOD_PACKAGES.md, "How bundled/ gets staged").
set(R4_PRELOADED_MODS "${CMAKE_CURRENT_SOURCE_DIR}/mods/preloaded")

# Trusted, statically linked mod plugins (src/mods/README.md), selected by the
# packages in mods/preloaded/packages.
file(GLOB R4_MOD_PLUGIN_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/mods/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/mods/*.cpp")

psxrecomp_add_game_runtime(psx-runtime
    PRELOADED_MODS_DIR "${R4_PRELOADED_MODS}"
    ENABLE_NETPLAY_IF_PRESENT
    NETPLAY_LOBBY_URL "ws://netplay.retcomm.net:8765"
    ENABLE_SETUP_WIZARD
    WINDOW_TITLE "Ridge Racer Type 4 Recompiled"
    EXE_NAME "r4-runtime"
    DEBUG_PORT 4797
    DEFAULT_GAME_CONFIG_PATH "game.toml"
    MAX_PLAYERS 2
    LAUNCHER_BOXART "${CMAKE_CURRENT_SOURCE_DIR}/recomp/launcher/boxart.tga"
    APP_ICON "${CMAKE_CURRENT_SOURCE_DIR}/assets/psxrecomp.ico"
    GEN_MARKER "generated/SLUS_007.97_dispatch.c"
    GEN_FULL_GLOB "generated/SLUS_007.97_full_*.c"
    CODEGEN_SETUP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/codegen_setup.c"
    EXTRAS_SOURCES ${R4_MOD_PLUGIN_SOURCES}
)

# Stage runtime configs next to the exe for direct build/ launches. Only copy
# files that exist -- never fail POST_BUILD when game_options.toml is absent.
foreach(_R4_RUNTIME_CONFIG game.toml game_options.toml)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_R4_RUNTIME_CONFIG}")
        add_custom_command(TARGET psx-runtime POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/${_R4_RUNTIME_CONFIG}"
                "$<TARGET_FILE_DIR:psx-runtime>/${_R4_RUNTIME_CONFIG}"
            COMMENT "Staging ${_R4_RUNTIME_CONFIG}"
            VERBATIM)
    endif()
endforeach()

# Developer tests (R4_BUILD_TESTS above): ctest --test-dir build -R r4_
# Every path is behind its own if(EXISTS ...) so the setup-host packager's
# CMakeLists gate treats them as optional (none of them ship).
if(R4_BUILD_TESTS AND BUILD_TESTING)
    # Pure widescreen helpers, no game data needed.
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_r4_widescreen.c")
        add_executable(r4-widescreen-tests
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_r4_widescreen.c")
        target_include_directories(r4-widescreen-tests PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src/mods")
        add_test(NAME r4_widescreen_helpers COMMAND r4-widescreen-tests)
    endif()
    # game.toml's [widescreen.cull] lists must match a fresh scan of the local
    # disc; registered only where the disc and a regen are present.
    find_package(Python3 QUIET COMPONENTS Interpreter)
    if(Python3_FOUND)
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tools/r4_ws_scan.py")
            if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/disc/SLUS_007.97")
                if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/generated/SLUS_007.97_full.ranges")
                    add_test(NAME r4_widescreen_cull_sites
                        COMMAND "${Python3_EXECUTABLE}"
                            "${CMAKE_CURRENT_SOURCE_DIR}/tools/r4_ws_scan.py"
                            --check "${CMAKE_CURRENT_SOURCE_DIR}/game.toml"
                        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
                endif()
            endif()
        endif()
    endif()
endif()
