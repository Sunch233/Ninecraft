@echo off
setlocal

cd /D "%~dp0"

set "BUILD_CONFIG=%~1"
if not defined BUILD_CONFIG set "BUILD_CONFIG=Debug"

call "%~dp0..\scripts\configure-msvc.bat" . build-win32 "%~2"
if errorlevel 1 exit /b %errorlevel%

"%NINECRAFT_CMAKE%" --build build-win32 --config "%BUILD_CONFIG%" --parallel
exit /b %errorlevel%
