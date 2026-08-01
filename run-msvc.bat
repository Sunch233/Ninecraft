@echo off
setlocal

cd /D "%~dp0"

if "%~1"=="" (
    echo Usage: %~nx0 ^<extracted-mcpe-root^> [user-data-root]
    echo Example: %~nx0 "C:\Games\MCPE-0.14.3-x86"
    exit /b 2
)

set "GAME_ROOT=%~f1"
set "USER_DATA_ROOT=%~2"
if not defined USER_DATA_ROOT set "USER_DATA_ROOT=%LOCALAPPDATA%\Ninecraft\MCPE-0.14.3-x86"

if not exist "%GAME_ROOT%\assets\" goto :missing_game_data
if not exist "%GAME_ROOT%\res\" goto :missing_game_data
if not exist "%GAME_ROOT%\lib\x86\libminecraftpe.so" goto :missing_game_data
if not exist "%GAME_ROOT%\lib\x86\libgnustl_shared.so" goto :missing_game_data
if not exist "%GAME_ROOT%\lib\x86\libfmod.so" goto :missing_game_data

if exist "build-msvc-win32\CMakeCache.txt" (
    cmake --build build-msvc-win32 --config Release --target ninecraft --parallel
    if errorlevel 1 exit /b 1
) else (
    call compile-msvc.bat Release
    if errorlevel 1 exit /b 1
)

if not exist "%USER_DATA_ROOT%\" mkdir "%USER_DATA_ROOT%"
if errorlevel 1 exit /b %errorlevel%

"build-msvc-win32\ninecraft\Release\ninecraft.exe" --game "%GAME_ROOT%" --home "%USER_DATA_ROOT%"
exit /b %errorlevel%

:missing_game_data
echo Invalid game-data root: "%GAME_ROOT%"
echo Expected assets, res, and lib\x86 containing libminecraftpe.so,
echo libgnustl_shared.so, and libfmod.so from a legally obtained x86 APK.
exit /b 2
