@echo off
setlocal

cd /D "%~dp0"

git submodule sync --recursive
if errorlevel 1 exit /b %errorlevel%
git submodule update --init --recursive
if errorlevel 1 exit /b %errorlevel%

cmake -S . -B build-mingw32 -G "MinGW Makefiles"
if errorlevel 1 exit /b %errorlevel%

cmake --build build-mingw32 --parallel
if errorlevel 1 exit /b %errorlevel%

echo Built build-mingw32\ninecraft\ninecraft.exe
