cmake_minimum_required(VERSION 3.20)

set(PSXRECOMP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/psxrecomp" CACHE PATH
    "Path to psxrecomp")
if(NOT EXISTS "${PSXRECOMP_ROOT}/runtime/runtime.cmake")
    message(FATAL_ERROR
        "PSXRecomp runtime is missing: ${PSXRECOMP_ROOT}/runtime/runtime.cmake\n"
        "Run: git submodule update --init --recursive\n"
        "or set -DPSXRECOMP_ROOT=<path-to-psxrecomp>.")
endif()

project(RidgeRacerType4Recomp LANGUAGES C CXX)

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

# Trusted, statically linked mod plugins (none yet).
file(GLOB R4_MOD_PLUGIN_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/mods/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/mods/*.cpp")

# Generated-C directory. "generated" is the stock 4:3 build; the adaptive
# renderer build (renderer/adaptive/materialize.sh) points this at its own
# regen from the patched recompiler so the two never share codegen.
set(R4_GENERATED_DIR "generated" CACHE STRING
    "Directory (relative to source) holding the recompiled game C")

# MMX6 adaptive renderer, carried for later widescreen work. OFF = the stock
# psxrecomp renderer (base 4:3). ON requires PSXRECOMP_ROOT to be the patched
# framework copy materialize.sh produces; R4's own adaptive plugin sources go
# in renderer/adaptive/src/ (never src/mods/, which the stock build globs).
option(R4_ADAPTIVE_RENDERER "Build against the patched adaptive-renderer framework" OFF)
if(R4_ADAPTIVE_RENDERER)
    if(NOT EXISTS "${PSXRECOMP_ROOT}/runtime/include/ws_view_anchor.h")
        message(FATAL_ERROR
            "R4_ADAPTIVE_RENDERER=ON needs the patched framework from "
            "renderer/adaptive/materialize.sh (PSXRECOMP_ROOT=${PSXRECOMP_ROOT} "
            "has no ws_view_anchor.h).")
    endif()
    file(GLOB R4_ADAPTIVE_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/renderer/adaptive/src/*.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/renderer/adaptive/src/*.cpp")
    list(APPEND R4_MOD_PLUGIN_SOURCES ${R4_ADAPTIVE_SOURCES})
endif()

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
    GEN_MARKER "${R4_GENERATED_DIR}/SLUS_007.97_dispatch.c"
    GEN_FULL_GLOB "${R4_GENERATED_DIR}/SLUS_007.97_full_*.c"
    CODEGEN_SETUP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/codegen_setup.c"
    EXTRAS_SOURCES ${R4_MOD_PLUGIN_SOURCES}
)

# Stage runtime configs next to the exe for direct build/ launches. Only copy
# files that exist -- never fail POST_BUILD when game_options.toml is absent.
# On Windows the staged game.toml names the recompiler with its .exe suffix:
# the runtime checks the overlay_autocompile_cmd paths literally there.
set(R4_STAGED_GAME_TOML "${CMAKE_CURRENT_SOURCE_DIR}/game.toml")
if(WIN32)
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/game.toml" _r4_game_toml)
    string(REPLACE "build-recompiler/psxrecomp-game " "build-recompiler/psxrecomp-game.exe "
        _r4_game_toml "${_r4_game_toml}")
    set(R4_STAGED_GAME_TOML "${CMAKE_CURRENT_BINARY_DIR}/game.win.toml")
    file(WRITE "${R4_STAGED_GAME_TOML}" "${_r4_game_toml}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/game.toml")
endif()
add_custom_command(TARGET psx-runtime POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${R4_STAGED_GAME_TOML}" "$<TARGET_FILE_DIR:psx-runtime>/game.toml"
    COMMENT "Staging game.toml"
    VERBATIM)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/game_options.toml")
    add_custom_command(TARGET psx-runtime POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/game_options.toml"
            "$<TARGET_FILE_DIR:psx-runtime>/game_options.toml"
        COMMENT "Staging game_options.toml"
        VERBATIM)
endif()
