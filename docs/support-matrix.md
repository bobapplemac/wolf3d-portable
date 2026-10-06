# Build and compatibility matrix

This document is the authoritative user-facing matrix for building runnable
`wolf3d-portable` packages. Every row compiles the pinned `wolf3d-lib`
submodule; wrappers never consume an unrelated prebuilt engine.

The status language is intentionally precise:

- **Build validated** means the compiler produced and packaged the wrapper.
- **Runtime validated** means that package was also executed on the named
  destination operating system.
- **Planned** means there is no supported build command yet.

For the engine-only SDK and its larger compiler matrix, see the companion
[`wolf3d-lib` support matrix](https://gitlab.moorenet.xyz/personal/wolf3d-lib/-/blob/main/docs/support-matrix.md)
or the local submodule's `lib/wolf3d/docs/support-matrix.md`.

## Fast paths

| Desired package | Build host | Command | Staged result |
| --- | --- | --- | --- |
| Windows Win32/GDI | Windows | `.\build.ps1 -Wrapper win32` | `dist/wolf3d-portable-<version>-win32-<arch>-<compiler>/` |
| Windows SDL3 | Windows with VS2019/2022 | `.\build.ps1 -Wrapper sdl3` | `dist/wolf3d-portable-<version>-sdl3-windows-<arch>-<compiler>/` |
| Both modern Windows wrappers | Windows with VS2019/2022 | `.\build.ps1 -Wrapper all` | Both folders above |
| Native Linux SDL3 | Linux | `make` or `make sdl3-release CC=clang` | `dist/wolf3d-portable-<version>-sdl3-linux-<arch>/` |
| Native Linux console | Linux | `make console-release` | `dist/wolf3d-portable-<version>-console-<arch>/` |
| Both native Linux wrappers | Linux | `make releases` | Both native folders |
| Portable glibc Linux wrappers | Linux + Docker | `make portable` | SDL3 and console x86-64 folders audited to glibc 2.28 |
| Bundled-musl Linux SDL3 | Linux + Docker | `make musl-sdl3` | Relocatable AppDir-style x86-64 folder |

Run `make help`, `.\build.ps1 -List`, or the XP-era dispatcher without
arguments for all selectable options. See [building.md](building.md) for exact
presets, package contents, and dependency setup.

## Windows wrapper/compiler matrix

| Compiler environment | Toolset | Architectures | Win32/GDI | SDL3 | Build entry point | Destination validation |
| --- | --- | --- | --- | --- | --- | --- |
| Visual Studio 2022 | v143 | x86, x64 | Validated | Validated | `build.ps1`, native VS2022 solution, CMake presets | Current Windows development host |
| Visual Studio 2019 | v142 | x86, x64 | Validated | Validated | `build.ps1`, native VS2019 solution, CMake presets | Current Windows development host |
| VS2017 v141 (compiler hosted by VS2019; native IDE files pending lab validation) | v141 | x86, x64 | Validated | Not supported | `build.ps1`, native VS2017 solution, CMake presets | No legacy-OS minimum claimed |
| Visual Studio 2015 | v140 | x86, x64 | Validated | Not supported | `build.ps1`, native VS2015 solution, CMake presets | No legacy-OS minimum claimed |
| Visual Studio 2015 XP SDK | v140_xp | x86, x64 | Validated | Not supported | `build.ps1`, VS2015 solution, CMake presets | Pending on actual XP; PE minimum 5.01 x86 / 5.02 x64 |
| Visual Studio 2013 Update 5 | v120 | x86, x64 | Validated | Not supported | `build.ps1`, native VS2013 solution, CMake presets | No legacy-OS minimum claimed |
| Visual Studio 2012 Update 5 | v110 | x86, x64 | Validated | Not supported | `build.ps1`, native VS2012 solution, CMake presets | No legacy-OS minimum claimed |
| Visual Studio 2010 SP1 | v100 | x86, x64 | Validated | Not supported | `build.ps1`, native VS2010 solution, CMake presets | No legacy-OS minimum claimed |
| Visual Studio 2008 SP1 | v90 | x86, x64 | Validated | Not supported | `build.ps1`, CMake presets | No legacy-OS minimum claimed |
| Visual Studio 2005 SP1 | MSVC 14.00 | x86 | Validated | Not supported | `scripts\windows\legacy\build.cmd` | Compile/package validated on XP SP3 x86 |
| Visual Studio .NET 2003 SP1 | MSVC 13.10 | x86 | Validated | Not supported | `scripts\windows\legacy\build.cmd` | Compile/package validated on XP SP3 x86 |
| Visual Studio .NET 2002 SP1 | MSVC 13.00 | x86 | Validated | Not supported | `scripts\windows\legacy\build.cmd` | Compile/package validated on XP SP3 x86 |
| Visual C++ 6.0 SP6 | MSVC 12.00 | x86 | Validated | Not supported | `scripts\windows\legacy\build.cmd` | Game runtime validated on XP SP3 x86 with WL1 and on Windows 11 x64 under WOW64 |

SDL3 is deliberately a modern wrapper. Historical Windows reach belongs to
the dependency-free Win32/GDI host; the project will not backport SDL3 or add
SDL1/SDL2 maintenance branches.

The compiler/toolset, Windows SDK, CRT selection, and imports jointly determine
the real OS floor. A compiler-labelled package is not advertised for an older
Windows version until that package has run there. Static CRT is the release
default; dynamic CRT packages require the matching Microsoft redistributable.

### Windows build interfaces

| Interface | Supported scope |
| --- | --- |
| `ide/visual-studio/vs2022/wolf3d-portable.sln` | VS2022/v143; Win32 and SDL3; x86/x64; publish configurations stage `dist/` |
| `ide/visual-studio/vs2019/wolf3d-portable.sln` | VS2019/v142; Win32 and SDL3; x86/x64; publish configurations stage `dist/` |
| `ide/visual-studio/vs2017/wolf3d-portable.sln` | VS2017/v141; Win32 only; x86/x64 |
| `ide/visual-studio/vs2015/wolf3d-portable.sln` | VS2015/v140 and v140_xp; Win32 only; x86/x64 |
| `ide/visual-studio/vs2010/`, `vs2012/`, `vs2013/` | Matching native IDE/toolset; Win32 only; x86/x64 |
| `ide/visual-studio/vs2008/wolf3d-portable.sln` | Native VS2008/v90; Win32 only; x86/x64 |
| `ide/visual-studio/vs2002/`, `vs2003/`, `vs2005/` | Matching period IDE/compiler; Win32 x86 only |
| `ide/visual-studio/vc6/wolf3d-portable.dsw` | Native VC6 workspace/project; Win32 x86 only |
| Root `build.ps1` | Interactive or scripted selection from VS2008 through VS2022; SDL3 only for VS2019/2022 |
| Direct CMake presets | Same modern compiler profiles as the dispatcher; useful for automation |
| `scripts\windows\legacy\build.cmd` | VC6, VS2002, VS2003, VS2005; Win32 x86 only; build or package |

## Linux wrapper/compiler matrix

| Profile | Compilers | Wrapper | ABI/dependencies | Build status | Intended destination |
| --- | --- | --- | --- | --- | --- |
| Native SDL3, pinned | GCC, Clang | SDL3 | Build-host glibc; bundled pinned SDL3 | Validated | Compatible x86-64 Linux with same/newer glibc and an SDL-supported display/audio stack |
| Native SDL3, system | GCC, Clang | SDL3 | Build-host glibc and system SDL 3.2+ | Validated | Distribution-integrated deployment with compatible system SDL |
| Native console | GCC, Clang | DRM/KMS + evdev + ALSA | Build-host glibc; system `libdrm` and ALSA | Validated | Linux virtual console with suitable devices, drivers, and permissions; no X11/Wayland required |
| Portable glibc SDL3 | GCC | SDL3 | Debian 10 baseline, pinned SDL3, audited maximum `GLIBC_2.28` | Validated | x86-64 Linux with glibc 2.28+, X11 or Wayland, and supported audio service |
| Portable glibc console | GCC | DRM/KMS + evdev + ALSA | Debian 10 baseline, audited maximum `GLIBC_2.28` | Validated | x86-64 glibc 2.28+ virtual-console system with required device access |
| Bundled musl SDL3 | GCC, Clang | SDL3 | Private musl loader and closed shared-library set | Validated and loader-smoke-tested | x86-64 Linux; no destination glibc dependency; X11/Wayland and audio facilities still system supplied |

The portable glibc artifacts intentionally build against an older userspace:
glibc binaries built to a 2.28 symbol ceiling are expected to work with newer
glibc releases. The musl bundle goes further by carrying its loader and user-
space libraries, but neither approach bundles a Linux kernel, graphics/input
drivers, device permissions, or a sound server.

## Package contents and executable names

| Wrapper | Executable | Required adjacent components |
| --- | --- | --- |
| Windows Win32/GDI | `wolf3d.exe` | `wolf3d.dll`; `Nuked-OPL3.dll` for the default backend |
| Windows SDL3 | `wolf3d-sdl3.exe` | `wolf3d.dll`, SDL3 DLLs; `Nuked-OPL3.dll` for the default backend |
| Linux SDL3 | `wolf3d-sdl3` (native/glibc package) or top-level `wolf3d` launcher (musl bundle) | `libwolf3d.so`, pinned SDL3 when selected, and default Nuked-OPL3 shared object |
| Linux console | `wolf3d` | `libwolf3d.so`, system DRM/ALSA libraries, and default Nuked-OPL3 shared object |

Each staged folder also contains plain-text project and third-party licenses
and `WOLF3D-LIB.txt`, which records the exact engine version and commit. Copy
the whole staged folder rather than selecting individual DLLs/shared objects.
Original game data is not included.

## Planned, not currently supported

| Target | Intended direction | Current status |
| --- | --- | --- |
| MinGW-w64 Win32/GDI | Independent Windows compiler family | Planned; no supported build command yet |
| 32-bit DOS | Open Watcom cross-build in a Linux/Docker workflow, DOS/32A extender, dedicated DOS host | Planned; not part of the current Windows or Linux matrices |

