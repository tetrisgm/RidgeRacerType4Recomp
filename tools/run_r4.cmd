@echo off
rem Launch a local Windows build (the Windows counterpart of tools/run_r4.sh).
rem
rem   tools\run_r4.cmd [build-dir] [extra runtime args, e.g. --no-launcher]
rem
rem Puts MSYS2 MinGW64 on PATH first so the runtime's background overlay
rem compiles find python3 and gcc; without it the game still runs, overlay
rem code just stays in the interpreter.
setlocal
set "ROOT=%~dp0.."
set "BUILD=%~1"
if "%BUILD%"=="" set "BUILD=build"
if exist "C:\msys64\mingw64\bin" set "PATH=C:\msys64\mingw64\bin;%PATH%"
rem Dev overlay compiles with the local emitters (release builds use the
rem overlay_toolchain the setup host installs instead). cwd = project root.
if not defined PSX_OVERLAY_AUTOCOMPILE_CMD set "PSX_OVERLAY_AUTOCOMPILE_CMD=python3 psxrecomp/tools/compile_overlays.py --game-toml game.toml --recompiler psxrecomp/recompiler/build/psxrecomp-game.exe --runtime-include psxrecomp/runtime/include --cps"
shift
"%ROOT%\%BUILD%\r4-runtime.exe" --game "%ROOT%\%BUILD%\game.toml" --disc "%ROOT%\disc\R4 - Ridge Racer Type 4 (USA).cue" %1 %2 %3 %4 %5 %6 %7 %8 %9
