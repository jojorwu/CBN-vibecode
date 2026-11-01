@echo off
rem Launch the locally built game from the project root so it finds data/ and gfx/.
rem Picks the newest exe from the Ninja (build-fast.bat) and Visual Studio build dirs.
cd /d "%~dp0"
set "EXE="
for /f "delims=" %%F in ('powershell -NoProfile -Command "Get-Item 'out\build\windows-ninja-msvc\src\RelWithDebInfo\cataclysm-bn-tiles.exe','out\build\windows-tiles-sounds-x64-msvc\src\RelWithDebInfo\cataclysm-bn-tiles.exe' -ErrorAction SilentlyContinue | Sort-Object LastWriteTime | Select-Object -Last 1 -ExpandProperty FullName"') do set "EXE=%%F"
if not defined EXE (
    echo cataclysm-bn-tiles.exe not found. Build it with build-fast.bat first.
    exit /b 1
)
start "" "%EXE%" %*
