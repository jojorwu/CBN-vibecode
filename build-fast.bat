@echo off
rem Fast local build: Ninja + ccache + PCH (preset windows-ninja-msvc in CMakeUserPresets.json).
rem Usage: build-fast.bat [target]   (default target: cataclysm-bn-tiles)
setlocal
cd /d "%~dp0"

if not defined VSCMD_VER (
    call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\Tools\VsDevCmd.bat" -no_logo -arch=amd64 -host_arch=amd64 || exit /b 1
)

set "TARGET=%~1"
if "%TARGET%"=="" set "TARGET=cataclysm-bn-tiles"

if not exist "out\build\windows-ninja-msvc\build.ninja" (
    cmake --preset windows-ninja-msvc || exit /b 1
)

cmake --build out\build\windows-ninja-msvc --config RelWithDebInfo --target %TARGET% || exit /b 1
