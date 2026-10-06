# wolf3d-portable

`wolf3d-portable` provides runnable host integrations for the preservation-
oriented [`wolf3d-lib`](lib/wolf3d) engine. Every wrapper builds the library
from its recorded Git submodule checkout and links only the public `WOLF3D.h`
API through the `wolf3d::wolf3d` CMake target.

The repository currently contains:

- a dependency-free Win32/GDI host;
- a cross-platform SDL3 host for Windows, X11, Wayland, and other SDL targets;
- a Linux virtual-console host using DRM/KMS, evdev, and ALSA.

No commercial game data is included. Supply files from a supported original
Wolfenstein 3D or Spear of Destiny installation: WL1, WL6, SDM, SOD, SD1,
SD2, or SD3.

## Shareware data

The freely distributed data-only archives below provide complete playable
shareware/demo data sets without an installer:

- [Wolfenstein 3D v1.4 shareware data (`WL1`)](https://download.sourceforge.net/wolfgl/wolfdata.zip)
  - SHA-256: `A32EE97C515B6E182597A06F2326D15CC4C343DDC70558CE5FE76C870B7A0027`
- [Spear of Destiny demo data (`SDM`)](https://download.sourceforge.net/wolfgl/sdmdata.zip)
  - SHA-256: `054590923CD35CE7C0BFAE98C23BE81AB70C28E11FD0E562B5253523FCD7B91F`

Extract one ZIP beside the executable, or place its eight files in another
directory and pass `--data <directory>`. These mirrors are linked from the
archived [WolfGL download page](https://wolfgl.sourceforge.net/files.htm); the
files are not redistributed by this project. Registered `WL6`, `SOD`, `SD1`,
`SD2`, and `SD3` data must come from a legitimately obtained game copy.

## Clone and initialize

Clone recursively so both `wolf3d-lib` and the pinned SDL3 checkout are
present:

```text
git clone --recursive ssh://git@gitlab.moorenet.xyz:2222/personal/wolf3d-portable.git
cd wolf3d-portable
```

For an existing checkout:

```text
git submodule update --init --recursive
```

The committed `wolf3d-lib` gitlink is the reproducible default. On Linux,
`make fresh` deliberately advances it to current `main`, initializes nested
dependencies, and builds the default SDL3 package. Review and commit the new
gitlink before treating that result as an official release.

## Windows

Open `ide/visual-studio/vs2019/wolf3d-portable.sln` in Visual Studio 2019 or
`ide/visual-studio/vs2022/wolf3d-portable.sln` in Visual Studio 2022, or use
CMake. The established names below select VS2019/v142:

```text
cmake --preset windows-dev-x64
cmake --build --preset windows-dev-x64
cmake --preset windows-release-x64
cmake --build --preset windows-release-x64
cmake --preset windows-sdl3-x64
cmake --build --preset windows-sdl3-x64
```

For VS2022/v143, add `vs2022` after `windows`, for example
`windows-vs2022-dev-x64`, `windows-vs2022-release-x64`, or
`windows-vs2022-sdl3-x64`.

The native Win32 wrapper also supports VS2017/v141, VS2015/v140, VS2013/v120,
VS2012/v110, VS2010/v100, and VS2008/v90 through matching `windows-vs20xx-*`
presets. Each supported IDE has its own native project under
`ide/visual-studio/`; VC6 uses `vc6/wolf3d-portable.dsw`. SDL3 is intentionally
excluded from every legacy compiler profile.

Windows XP-era x86 builds use the separate CMake definition and native CMD
dispatcher validated with VC6 SP6 and Visual Studio 2002 SP1, 2003 SP1, and
2005 SP1:

```text
scripts\windows\legacy\build.cmd vc6
scripts\windows\legacy\build.cmd vs2002
scripts\windows\legacy\build.cmd vs2003
scripts\windows\legacy\build.cmd vs2005
```

These commands default to a Release package with Nuked-OPL3 and a static CRT.
Run the script without arguments for the complete option list. The resulting
compiler-qualified x86 game folders are staged under `dist/`. This path builds
only the Win32/GDI wrapper; SDL3 deliberately has no XP-era build profile.

The parallel `windows-vs2015-xp-*` profiles provide a newer XP-targeting
alternative using the v140_xp toolset.

For a guided command line, `build.ps1` detects installed supported Visual
Studio versions and defaults to publishing both x64 wrappers with the newest
one found. With no arguments it interactively walks through the choices:

```powershell
.\build.ps1 -List
.\build.ps1
.\build.ps1 -Compiler vs2019 -Architecture x86 -Wrapper win32
.\build.ps1 -Compiler vs2015 -Architecture x86 -Wrapper win32
.\build.ps1 -Compiler vs2015-xp -Architecture x86 -Wrapper win32
.\build.ps1 -Compiler vs2008 -Architecture x86 -Wrapper win32
.\build.ps1 -Action build -Configuration Debug -Wrapper sdl3
```

Run `Get-Help .\scripts\windows\build.ps1 -Detailed` for every option. The
root launcher only dispatches to the same checked-in CMake presets and targets.

In Visual Studio, select `Publish Win32`, `Publish SDL3`, or `Publish All`
with the `x64` or `Win32` platform and build the solution. These publish
configurations invoke the same CMake release targets and stage the same
ready-to-copy folders under `dist/` as the command-line presets. Ordinary
Debug and Release configurations remain development builds under `build/`.

Use the corresponding `x86` presets for 32-bit builds. Release folders appear
under `dist/` and include `wolf3d.exe` or `wolf3d-sdl3.exe`, `wolf3d.dll`,
the replaceable `Nuked-OPL3.dll`, all required host DLLs, notices, and a
`WOLF3D-LIB.txt` file recording the exact engine version and commit.

## Linux

Run `make help` for the complete command and variable list. The usual paths
are:

```text
make                         # pinned-SDL3 distribution
make console-release         # DRM/evdev/ALSA distribution
make releases                # both distributions
make portable                # both in the Debian 10 compatibility container
make musl-sdl3               # relocatable bundle with its own musl userspace
make sdl3-release CC=clang   # use Clang
make sdl3-release USE_SYSTEM_SDL3=ON
```

Pinned SDL3 is the redistribution default. An installed SDL 3.2 or newer can
be selected for distribution-integrated builds. The Debian 10 targets retain
the established glibc 2.28 build baseline and audit every staged ELF. The
musl target instead packages a private loader and closed shared-library set,
so it has no dependency on the destination's glibc version.

## Running

Copy one original game installation beside the selected executable, or pass
its location explicitly:

```text
wolf3d-sdl3 --data /path/to/WL1
wolf3d --data /path/to/WL6 --game WL6
```

Executable names beginning with `wolf` prefer WL1/WL6 data. Names beginning
with `spear` or `sod` prefer Spear data. `--game` accepts the exact data-file
extension. Use `--mouse` to expose mouse hardware, `--fullscreen` to start
fullscreen, and F11 or Alt+Enter to toggle fullscreen without acknowledging an
engine "any key" wait.

See the [build and compatibility matrix](docs/support-matrix.md) for every
supported compiler, wrapper, build entry point, artifact, and validated
destination operating system. [docs/building.md](docs/building.md) contains the
complete commands and option details.
