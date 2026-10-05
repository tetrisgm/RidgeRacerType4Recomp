cmake_minimum_required(VERSION 3.20)

set(PSXRECOMP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/psxrecomp" CACHE PATH
    "Path to psxrecomp")
if(NOT EXISTS "${PSXRECOMP_ROOT}/runtime/runtime.cmake")
    message(FATAL_ERROR
        "PSXRecomp runtime is missing: ${PSXRECOMP_ROOT}/runtime/runtime.cmake\n"
        "Run: git submodule update --init --recursive\n"
        "or set -DPSXRECOMP_ROOT=<path-to-psxrecomp>.")
endif()

# macOS: portable binaries by default, for release builds
# (tools/package_release.sh) and local ones. Use the static SDL3 psxrecomp
# pins rather than a Homebrew dylib, skip recomp-ui's optional
# FreeType/HarfBuzz (emoji stay outline glyphs), and target a macOS older than
# the build machine.
# Cache defaults, so a local -D still overrides them.
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
    set(CMAKE_DISABLE_FIND_PACKAGE_SDL3 ON CACHE BOOL
        "Use psxrecomp's pinned static SDL3, not a system dylib")
    set(CMAKE_DISABLE_FIND_PACKAGE_Freetype ON CACHE BOOL
        "Skip optional FreeType/HarfBuzz (Homebrew dylibs)")
endif()

project(RidgeRacerType4Recomp LANGUAGES C CXX)

# Developer tests. OFF by default: they need tests/, tools/ and the local disc,
# and release builds never run them. Configure with -DR4_BUILD_TESTS=ON to get
# them.
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
    "First-run setup wizard in recomp-ui (the disc picker of a full build)" FORCE)
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
# Every path is behind its own if(EXISTS ...), so a checkout without tests/ or
# tools/ still configures (none of them ship).
if(R4_BUILD_TESTS AND BUILD_TESTING)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_r4_hide_rear_view_mirror.c")
        add_executable(r4-hide-rear-view-mirror-tests EXCLUDE_FROM_ALL
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_r4_hide_rear_view_mirror.c")
        target_include_directories(r4-hide-rear-view-mirror-tests PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src/mods")
        add_test(NAME r4_hide_rear_view_mirror_build
                 COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
                         --target r4-hide-rear-view-mirror-tests)
        add_test(NAME r4_hide_rear_view_mirror COMMAND r4-hide-rear-view-mirror-tests)
        set_tests_properties(r4_hide_rear_view_mirror PROPERTIES
                             DEPENDS r4_hide_rear_view_mirror_build)
    endif()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_r4_camera_look.c")
        add_executable(r4-camera-look-tests
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_r4_camera_look.c")
        target_include_directories(r4-camera-look-tests PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src/mods")
        if(NOT MSVC)
            target_link_libraries(r4-camera-look-tests PRIVATE m)
        endif()
        add_test(NAME r4_camera_look_helpers COMMAND r4-camera-look-tests)
    endif()
    # Pure widescreen helpers, no game data needed.
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_r4_widescreen.c")
        add_executable(r4-widescreen-tests
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_r4_widescreen.c")
        target_include_directories(r4-widescreen-tests PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src/mods")
        add_test(NAME r4_widescreen_helpers COMMAND r4-widescreen-tests)
    endif()
    # R4 Frame Rate, pure C (no runtime or disc needed). The executables are
    # built on demand by the *_build tests, so a plain
    #   ctest --test-dir build -R r4_ --output-on-failure
    # builds and runs them.
    function(r4_frame_rate_test name)
        add_executable(test_${name} EXCLUDE_FROM_ALL ${ARGN})
        target_include_directories(test_${name} PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src/mods" "${PSXRECOMP_ROOT}/runtime/include")
        add_test(NAME ${name}_build
                 COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
                         --target test_${name})
        add_test(NAME ${name} COMMAND test_${name})
        set_tests_properties(${name} PROPERTIES DEPENDS ${name}_build)
    endfunction()
    # Option parsing.
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_r4_frame_rate.c")
        r4_frame_rate_test(r4_frame_rate
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_r4_frame_rate.c")
    endif()
    # Interpolation math and the field table.
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_r4_interp.c")
        r4_frame_rate_test(r4_interp
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_r4_interp.c")
    endif()
    # The interpolation plugin itself against a mock mod API (rolled-back
    # passes, fallback to frame blend).
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_r4_interp_plugin.c")
        r4_frame_rate_test(r4_interp_plugin
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_r4_interp_plugin.c"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/mods/r4_interp.c")
    endif()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_r4_jogcon_compat.c")
        r4_frame_rate_test(r4_jogcon_compat
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_r4_jogcon_compat.c"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/mods/r4_jogcon_compat.c")
    endif()
    find_package(Python3 QUIET COMPONENTS Interpreter)
    if(Python3_FOUND)
        # game.toml's [widescreen.cull] lists must match a fresh scan of the
        # local disc. The scan reads the boot EXE and R4.BIN from disc/*.bin
        # (no regen or extracted EXE needed), so it is registered whenever the
        # disc is there, and says so when it is not.
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tools/r4_ws_scan.py")
            file(GLOB _R4_DISC_BINS "${CMAKE_CURRENT_SOURCE_DIR}/disc/*.bin")
            if(_R4_DISC_BINS)
                if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/generated/SLUS_007.97_full.ranges")
                    add_test(NAME r4_widescreen_cull_sites
                        COMMAND "${Python3_EXECUTABLE}"
                            "${CMAKE_CURRENT_SOURCE_DIR}/tools/r4_ws_scan.py"
                            --check "${CMAKE_CURRENT_SOURCE_DIR}/game.toml"
                        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
                endif()
            else()
                message(WARNING "R4_BUILD_TESTS: no disc/*.bin, so r4_widescreen_cull_sites "
                    "(game.toml [widescreen.cull] vs the disc) is not registered")
            endif()
        endif()
        # src/mods/r4_interp_fields.h must match tools/data/r4_interp_fields.json.
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tools/gen_r4_interp_fields.py")
            add_test(NAME r4_interp_fields_current
                COMMAND "${Python3_EXECUTABLE}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/tools/gen_r4_interp_fields.py" --check
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
        endif()
    endif()
endif()
