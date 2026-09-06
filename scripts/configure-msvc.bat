@echo off
setlocal DisableDelayedExpansion

rem Usage: configure-msvc.bat source-dir build-dir [auto^|2019^|2022^|2026] [xp]
if "%~1"=="" goto :usage
if "%~2"=="" goto :usage
if not "%~4"=="" if /I not "%~4"=="xp" goto :usage
if not "%~5"=="" goto :usage
set "SOURCE_DIR=%~f1"
set "BUILD_DIR=%~f2"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VS_VERSION=%~3"
if not defined VS_VERSION set "VS_VERSION=auto"
if /I "%VS_VERSION%"=="auto" goto :read_cache
if "%VS_VERSION%"=="2019" goto :read_cache
if "%VS_VERSION%"=="2022" goto :read_cache
if "%VS_VERSION%"=="2026" goto :read_cache
>&2 echo MSVC configuration failed: Visual Studio version must be auto, 2019, 2022, or 2026.
exit /b 1

:read_cache
rem Keep a cached generator only while its Visual Studio is still installed.
set "CACHED_GENERATOR="
set "CACHED_SOURCE="
set "STALE_YEAR="
set "SELECTED_YEAR="
if exist "%BUILD_DIR%\CMakeCache.txt" (
    for /f "usebackq tokens=1,* delims==" %%A in ("%BUILD_DIR%\CMakeCache.txt") do (
        if "%%A"=="CMAKE_GENERATOR:INTERNAL" set "CACHED_GENERATOR=%%B"
        if "%%A"=="CMAKE_HOME_DIRECTORY:INTERNAL" set "CACHED_SOURCE=%%B"
    )
)
if not defined CACHED_GENERATOR goto :check_cmake
if "%CACHED_GENERATOR%"=="Visual Studio 16 2019" set "SELECTED_YEAR=2019"
if "%CACHED_GENERATOR%"=="Visual Studio 17 2022" set "SELECTED_YEAR=2022"
if "%CACHED_GENERATOR%"=="Visual Studio 18 2026" set "SELECTED_YEAR=2026"
if not defined SELECTED_YEAR (
    >&2 echo MSVC configuration failed: Unsupported cached generator "%CACHED_GENERATOR%" in "%BUILD_DIR%". Use a fresh build directory.
    exit /b 1
)
if /I "%VS_VERSION%"=="auto" goto :check_cached_installation
if "%VS_VERSION%"=="%SELECTED_YEAR%" goto :check_cmake
>&2 echo MSVC configuration failed: "%BUILD_DIR%" already uses VS%SELECTED_YEAR%. Remove its CMakeCache.txt and CMakeFiles before selecting VS%VS_VERSION%.
exit /b 1

:check_cached_installation
if not exist "%VSWHERE%" goto :check_cmake
call :version_info %SELECTED_YEAR%
call :find_installation
if defined VS_INSTALL goto :check_cmake
echo Cached VS%SELECTED_YEAR% is no longer installed; selecting an available Visual Studio.
set "STALE_YEAR=%SELECTED_YEAR%"
set "SELECTED_YEAR="

:check_cmake
set "PATH_CMAKE="
for /f "delims=" %%C in ('where.exe cmake.exe 2^>nul') do if not defined PATH_CMAKE set "PATH_CMAKE=%%C"
if defined SELECTED_YEAR goto :configure
if /I not "%VS_VERSION%"=="auto" (
    set "SELECTED_YEAR=%VS_VERSION%"
    goto :configure
)

if not exist "%VSWHERE%" (
    >&2 echo MSVC configuration failed: vswhere.exe was not found. Install Visual Studio C++ Build Tools or select a version explicitly.
    exit /b 1
)
set "UNSUPPORTED_YEAR="
set "UNSUPPORTED_MINIMUM="
call :try_version 2026
call :try_version 2022
call :try_version 2019
if defined SELECTED_YEAR goto :configure
if defined UNSUPPORTED_YEAR (
    >&2 echo MSVC configuration failed: Installed VS%UNSUPPORTED_YEAR% requires CMake %UNSUPPORTED_MINIMUM% or newer. Upgrade CMake on PATH or install C++ CMake tools for Windows in Visual Studio Installer.
    exit /b 1
)
>&2 echo MSVC configuration failed: Install Visual Studio 2019, 2022, or 2026 with the Desktop development with C++ workload and a Windows SDK.
exit /b 1

:configure
call :version_info %SELECTED_YEAR%
call :find_installation
call :select_cmake
if errorlevel 1 (
    >&2 echo MSVC configuration failed: VS%SELECTED_YEAR% requires CMake %VS_MINIMUM% or newer. Upgrade CMake on PATH or install C++ CMake tools for Windows in Visual Studio Installer.
    exit /b 1
)
if defined STALE_YEAR (
    call :clear_stale_cache
    if errorlevel 1 exit /b 1
)
echo Using CMake: "%CMAKE_EXE%"
echo Configuring with %VS_GENERATOR% (Win32)
if /I "%~4"=="xp" (
    "%CMAKE_EXE%" -S "%SOURCE_DIR%" -B "%BUILD_DIR%" -G "%VS_GENERATOR%" -A Win32 -T v141_xp -DNINECRAFT_WINDOWS_XP=ON
) else (
    "%CMAKE_EXE%" -S "%SOURCE_DIR%" -B "%BUILD_DIR%" -G "%VS_GENERATOR%" -A Win32
)
if errorlevel 1 exit /b %errorlevel%
rem The caller must build with the same CMake that generated the project.
endlocal & set "NINECRAFT_CMAKE=%CMAKE_EXE%" & exit /b 0

:try_version
if defined SELECTED_YEAR exit /b 0
call :version_info %1
call :find_installation
if not defined VS_INSTALL exit /b 0
call :select_cmake
if not errorlevel 1 (
    set "SELECTED_YEAR=%1"
    exit /b 0
)
if not defined UNSUPPORTED_YEAR (
    set "UNSUPPORTED_YEAR=%1"
    set "UNSUPPORTED_MINIMUM=%VS_MINIMUM%"
)
exit /b 0

:find_installation
set "VS_INSTALL="
if not exist "%VSWHERE%" exit /b 0
for /f "delims=" %%I in ('call "%VSWHERE%" -latest -products * -version "%VS_RANGE%" -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath') do set "VS_INSTALL=%%I"
exit /b 0

:select_cmake
set "CMAKE_EXE=%PATH_CMAKE%"
if not defined CMAKE_EXE goto :bundled_cmake
call :generator_supported
if not errorlevel 1 exit /b 0
:bundled_cmake
if not defined VS_INSTALL exit /b 1
set "CMAKE_EXE=%VS_INSTALL%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if not exist "%CMAKE_EXE%" exit /b 1
call :generator_supported
exit /b %errorlevel%

:generator_supported
call "%CMAKE_EXE%" --help | "%SystemRoot%\System32\findstr.exe" /L /C:"%VS_GENERATOR%" >nul
exit /b %errorlevel%

:clear_stale_cache
rem BUILD_DIR is absolute. Only clear CMake's fixed state paths inside this
rem explicitly requested out-of-source build belonging to SOURCE_DIR.
if /I "%BUILD_DIR%"=="%SOURCE_DIR%" goto :unsafe_cleanup
if not defined CACHED_SOURCE goto :unsafe_cleanup
for %%D in ("%CACHED_SOURCE%") do if /I not "%%~fD"=="%SOURCE_DIR%" goto :unsafe_cleanup
for %%D in ("%BUILD_DIR%") do if "%%~nxD"=="" goto :unsafe_cleanup
echo Clearing stale CMake state in "%BUILD_DIR%".
if exist "%BUILD_DIR%\CMakeFiles\" rd /s /q "%BUILD_DIR%\CMakeFiles"
if exist "%BUILD_DIR%\CMakeFiles\" goto :cleanup_failed
if exist "%BUILD_DIR%\CMakeCache.txt" del /q "%BUILD_DIR%\CMakeCache.txt"
if exist "%BUILD_DIR%\CMakeCache.txt" goto :cleanup_failed
exit /b 0

:cleanup_failed
>&2 echo MSVC configuration failed: Could not clear CMake state in "%BUILD_DIR%".
exit /b 1

:unsafe_cleanup
>&2 echo MSVC configuration failed: Cannot automatically clear CMake state in "%BUILD_DIR%". Use a fresh out-of-source build directory.
exit /b 1

:version_info
if "%1"=="2026" (
    set "VS_GENERATOR=Visual Studio 18 2026"
    set "VS_MINIMUM=4.2"
    set "VS_RANGE=[18.0,19.0)"
)
if "%1"=="2022" (
    set "VS_GENERATOR=Visual Studio 17 2022"
    set "VS_MINIMUM=3.21"
    set "VS_RANGE=[17.0,18.0)"
)
if "%1"=="2019" (
    set "VS_GENERATOR=Visual Studio 16 2019"
    set "VS_MINIMUM=3.14"
    set "VS_RANGE=[16.0,17.0)"
)
exit /b 0

:usage
>&2 echo Usage: %~nx0 source-dir build-dir [auto^|2019^|2022^|2026] [xp]
exit /b 2
