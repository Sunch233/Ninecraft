# Ninecraft

![alt screenshot](https://raw.githubusercontent.com/MCPI-Revival/Ninecraft/master/screenshots/0.6.1-menu.png)

## Installing compile dependencies
### Microsoft Windows:
To compile Ninecraft on Windows, you'll need either MinGW32, LLVM-MinGW, or the Visual Studio 2022 Build Tools. CMake, Python and Git are also required.
After python is installed you must run `pip install jinja2`

The Windows XP build additionally requires the Visual Studio Installer
component **C++ Windows XP Support for VS 2017 (v141) tools**.
### Debian/Ubuntu:
x86_64:
```
sudo dpkg --add-architecture i386
sudo apt update
sudo apt install git make cmake gcc g++ gcc-multilib g++-multilib libopenal-dev:i386 libx11-dev:i386 libxrandr-dev:i386 libxinerama-dev:i386 libxcursor-dev:i386 libxi-dev:i386 libgl-dev:i386 zenity unzip python3-jinja2
```
arm64:
```
sudo dpkg --add-architecture armhf
sudo apt update
sudo apt install git make cmake gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf libopenal-dev:armhf libx11-dev:armhf libxrandr-dev:armhf libxinerama-dev:armhf libxcursor-dev:armhf libxi-dev:armhf libgl-dev:armhf zenity unzip python3-jinja2
```
x86:
```
sudo apt update
sudo apt install git make cmake gcc g++ libopenal-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl-dev zenity unzip python3-jinja2
```
arm:
```
sudo apt update
sudo apt install git make cmake gcc g++ libopenal-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl-dev zenity unzip python3-jinja2
```
### ArchLinux
x86_64:
```
sudo pacman -Syu
sudo pacman -S git make cmake gcc gcc-multilib lib32-openal lib32-libx11 lib32-libxrandr lib32-libxinerama lib32-libxcursor lib32-libxi lib32-libglvnd zenity unzip python-jinja
```
arm:
```
sudo pacman -Syu
sudo pacman -S git make cmake gcc openal libx11 libxrandr libxinerama libxcursor libxi libglvnd zenity unzip python-jinja
```
### AlpineLinux
x86:
```
apk update
apk add git make cmake gcc g++ openal-soft-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev mesa-dev zenity unzip py3-jinja2
```
### Fedora
x86_64:
```
dnf update
dnf install git make cmake gcc g++ glibc-devel.i686 libstdc++-devel.i686 openal-soft-devel.i686 libX11-devel.i686 libXrandr-devel.i686 libXinerama-devel.i686 libXcursor-devel.i686 libXi-devel.i686 libglvnd-devel.i686 zenity unzip python3-jinja2
```
x86:
```
dnf update
dnf install git make cmake gcc g++ openal-soft-devel libX11-devel libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel libglvnd-devel zenity unzip python3-jinja2
```
arm:
```
dnf update
dnf install git make cmake gcc g++ openal-soft-devel libX11-devel libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel libglvnd-devel zenity unzip python3-jinja2
```

### Nix/NixOS
```
nix --extra-experimental-features "nix-command flakes" shell github:MCPI-Revival/Ninecraft --impure
```
to install globaly see here [nix/README.md].

## Compiling on Microsoft Windows
### mingw32 & llvm-mingw
```
git clone --recursive https://github.com/MCPI-Revival/Ninecraft.git
cd Ninecraft
.\compile.bat
```
### Visual Studio 2022 build tools
```
git clone --recursive https://github.com/MCPI-Revival/Ninecraft.git
cd Ninecraft
.\compile-msvc.bat
```

The MSVC script builds the `Release` configuration by default. Pass another
configuration explicitly when needed, for example `compile-msvc.bat Debug`.
It also initializes missing Git submodules automatically.

Windows MSVC builds statically link the bundled libraries and the MSVC/UCRT
runtime. The resulting `ninecraft.exe` does not require VC Redistributable,
SDL2, or zlib DLLs; normal Windows system DLLs are still used.

### Windows XP SP3 (MSVC v141_xp)

```bat
.\compile-msvc-xp.bat
```

This uses a separate `build-msvc-v141xp` directory, targets 32-bit Windows XP
SP3, and keeps the static runtime configuration. The resulting executable is
`build-msvc-v141xp\ninecraft\Release\ninecraft.exe`. A graphics driver with
OpenGL 2.0 support is still required.

To verify that the compiled launcher can start without requiring game data:
```
ctest --test-dir build-msvc-win32 -C Release --output-on-failure
```
For the XP build, replace `build-msvc-win32` with `build-msvc-v141xp`.

### Launcher runtime configuration

On the first normal launch, Ninecraft attempts to create `ninecraft.ini`
beside the executable. All settings default to `false`, preserving the
previous behavior:

```ini
force_gles_translation=false
disable_vsync=false
windows10_ui=false
fps_limit=false
```

Set `force_gles_translation=true` to ignore native GLSL ES 1.00 support such
as `GL_ARB_ES2_compatibility` and use the desktop GLSL 1.20 translator. This
can work around shader precision problems in older graphics drivers and
requires desktop GLSL 1.20 support.

Set `disable_vsync=true` to request swap interval 0 after the OpenGL context is
created. A graphics-driver control panel may still override this request.

Set `windows10_ui=true` to use the Windows 10 edition start menu, desktop
screen layouts and centered desktop HUD in Minecraft 0.14.3. Leave it `false`
to retain the original Pocket Edition interface.

Set `fps_limit=false` to leave the frame rate uncapped, or set it to an integer
from 1 to 1000, such as `fps_limit=60`. VSync, driver settings or insufficient
performance may still produce a lower frame rate.

Restart Ninecraft after changing an option. With `-debug`, the selected values,
shader path, swap-interval result and FPS-limiter state are written to
`debug.log`.

## Compiling on Linux
### x86_64 & x86:
```
git clone --recursive https://github.com/MCPI-Revival/Ninecraft.git
cd Ninecraft
make build-i686
```
### arm64 & arm:
```
git clone --recursive https://github.com/MCPI-Revival/Ninecraft.git
cd Ninecraft
make build-arm
```

## Before running for the first time
Ninecraft does not include Minecraft game data. Extract a legally obtained,
supported 32-bit Minecraft PE APK before trying to launch the game.
For Windows/i686, the APK must contain `lib/x86/libminecraftpe.so`.

MCPE `v0.14.3 alpha` x86 is the currently verified Windows target. Point
`--game` at the APK extraction root, not at its `lib/x86` directory. The root
must contain at least:

```text
assets/
res/
lib/x86/libminecraftpe.so
lib/x86/libgnustl_shared.so
lib/x86/libfmod.so
```

```
./tools/extract.sh /path/to/your/apk
```

On Windows, run the extraction command from Git Bash, which is included with
Git for Windows.
### on nix
```
ninecraft-extract
```

## Running
### mingw32 & llvm-mingw (Microsoft Windows)
```
.\build-mingw32\ninecraft\ninecraft.exe
```
### Visual Studio 2022 build tools (Microsoft Windows)
```
.\run-msvc.bat "C:\path\to\extracted-mcpe-0.14.3-x86"
```

The runner validates the extracted client, builds `Release` when necessary,
and stores worlds and options under
`%LOCALAPPDATA%\Ninecraft\MCPE-0.14.3-x86` by default. Pass a second argument
to choose another user-data directory.

The equivalent direct PowerShell launch is:

```powershell
$gameRoot = 'C:\path\to\extracted-mcpe-0.14.3-x86'
$userDataRoot = Join-Path $env:LOCALAPPDATA 'Ninecraft\MCPE-0.14.3-x86'
New-Item -ItemType Directory -Force -Path $userDataRoot | Out-Null

& '.\build-msvc-win32\ninecraft\Release\ninecraft.exe' `
    --game $gameRoot `
    --home $userDataRoot
```

Menu navigation, world creation/loading, keyboard movement, mouse look, and
save/quit have been verified with this client version. Sound currently uses a
silent fallback because the Android FMOD Java bridge is not present on the
desktop host.
### x86_64 & x86 (Linux):
```
./build-i686/ninecraft/ninecraft
```
### arm64 & arm (Linux)
```
./build-arm/ninecraft/ninecraft
```
### NixOS (eventually be both arm and x86, also Windows and Mac)
```sh
ninecraft (optional version)
```
