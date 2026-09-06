@echo off
setlocal

cd /D "%~dp0"

set "BUILD_CONFIG=%~1"
if not defined BUILD_CONFIG set "BUILD_CONFIG=Release"

git submodule sync --recursive
if errorlevel 1 exit /b %errorlevel%
git submodule update --init --recursive
if errorlevel 1 exit /b %errorlevel%

call "%~dp0scripts\configure-msvc.bat" . build-msvc-win32 "%~2"
if errorlevel 1 exit /b %errorlevel%

"%NINECRAFT_CMAKE%" --build build-msvc-win32 --config "%BUILD_CONFIG%" --parallel
if errorlevel 1 exit /b %errorlevel%

echo Built build-msvc-win32\ninecraft\%BUILD_CONFIG%\ninecraft.exe
