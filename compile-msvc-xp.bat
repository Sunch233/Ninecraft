@echo off
setlocal

cd /D "%~dp0"

set "BUILD_CONFIG=%~1"
if not defined BUILD_CONFIG set "BUILD_CONFIG=Release"

git submodule sync --recursive
if errorlevel 1 exit /b %errorlevel%
git submodule update --init --recursive
if errorlevel 1 exit /b %errorlevel%

call "%~dp0scripts\configure-msvc.bat" . build-msvc-v141xp "%~2" xp
if errorlevel 1 exit /b %errorlevel%

"%NINECRAFT_CMAKE%" --build build-msvc-v141xp --config "%BUILD_CONFIG%" --target ninecraft --parallel
if errorlevel 1 exit /b %errorlevel%

echo Built build-msvc-v141xp\ninecraft\%BUILD_CONFIG%\ninecraft.exe
