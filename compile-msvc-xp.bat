@echo off
setlocal

cd /D "%~dp0"

set "BUILD_CONFIG=%~1"
if not defined BUILD_CONFIG set "BUILD_CONFIG=Release"

git submodule sync --recursive
if errorlevel 1 exit /b %errorlevel%
git submodule update --init --recursive
if errorlevel 1 exit /b %errorlevel%

cmake -S . -B build-msvc-v141xp -G "Visual Studio 17 2022" -A Win32 -T v141_xp -DNINECRAFT_WINDOWS_XP=ON
if errorlevel 1 exit /b %errorlevel%

cmake --build build-msvc-v141xp --config "%BUILD_CONFIG%" --target ninecraft --parallel
if errorlevel 1 exit /b %errorlevel%

echo Built build-msvc-v141xp\ninecraft\%BUILD_CONFIG%\ninecraft.exe
