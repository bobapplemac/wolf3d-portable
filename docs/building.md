# Building wolf3d-portable

The [build and compatibility matrix](support-matrix.md) is the concise record
of supported compilers, wrappers, produced artifacts, and runtime-validated
destination operating systems. This document supplies the detailed commands,
dependencies, and package behavior.

## Dependencies

Initialize both submodules before configuring:

```text
git submodule update --init --recursive
```

The `lib/wolf3d` gitlink records the exact engine revision. `third_party/SDL3`
records the default GUI dependency. A system SDL 3.2 or newer is also
supported.

## Windows

Visual Studio, MinGW, and CLI builds share the CMake trees under `build/`:

```text
cmake --preset windows-dev-x64
cmake --build --preset windows-dev-x64
```

The established `windows-*` preset names explicitly select Visual Studio 2019
and v142. Add `vs2022` or `vs2026` after `windows` for the parallel v143 or
v145 presets, for example `windows-vs2026-dev-x64`,
`windows-vs2026-release-x64`, or `windows-vs2026-sdl3-x64`. Their build trees
and staged package names identify the compiler generation so all sets coexist.

The Win32/GDI host additionally provides compiler-qualified presets for
VS2017/v141, VS2015/v140, VS2013/v120, VS2012/v110, VS2010/v100, and
VS2008/v90. All cover x86/x64 and static/dynamic CRT builds. SDL3 remains a
modern-compiler target and is deliberately excluded from these profiles; no
SDL backport is part of the legacy Windows support policy.
The parallel `windows-vs2015-xp-*` presets select `v140_xp`; their PE minimums
are Windows 5.01 for x86 and 5.02 for x64.

### Windows XP-era native compilers

VC6 through VS2005 use an intentionally separate CMake 3.5 definition so the
modern project does not inherit obsolete generator constraints. From an x86
Windows build host with the selected compiler installed:

```text
scripts\windows\legacy\build.cmd COMPILER [CONFIG] [DRIVERS] [DEFAULT_OPL] [RUNTIME] [ACTION]
```

The accepted values are:

- `COMPILER`: `vc6`, `vs2002`, `vs2003`, or `vs2005`;
- `CONFIG`: `Release` (default) or `Debug`;
- `DRIVERS`: `all` (default), one driver, or a hyphenated driver pair;
- `DEFAULT_OPL`: `nuked` (default), `dbopl`, or `silent`, and must be included;
- `RUNTIME`: `static` (default) or `dynamic`;
- `ACTION`: `package` (default) or `build`.

For example, this creates a ready-to-copy VC6 package:

```text
scripts\windows\legacy\build.cmd vc6 Release all nuked static package
```

The legacy definition always compiles the pinned library submodule as part of
the wrapper build. It supports x86 only and never configures SDL3. Old Windows
SDKs lack raw-input declarations, so these builds use ordinary Win32 mouse
messages; keyboard, GDI presentation, WinMM audio, fullscreen toggling, and
dynamically discovered XInput remain available. Fullscreen covers the primary
display on these profiles, matching the single-monitor assumptions of their
target era.

The current compatibility baseline has been compile-and-package validated with
VC6 SP6, VS2002 SP1, VS2003 SP1, and VS2005 SP1. The VC6 package has also been
launched on Windows XP SP3 x86 with real WL1 shareware data and manually
validated on Windows 11 x64 under WOW64.

The development preset builds both GUI hosts. Dedicated distribution presets
are `windows-release-{x64,x86}` for Win32 and `windows-sdl3-{x64,x86}` for
SDL3. The MSVC runtime is statically linked by default; set
`WG_STATIC_MSVC_RUNTIME=OFF` in a separate build tree to use the matching
Visual C++ Redistributable.

The checked-in VS2019, VS2022, and VS2026 solutions under
`ide/visual-studio/` invoke these same configurations through v142, v143, and
v145 respectively. Select one of the following solution configurations and
build:

- `Publish Win32` stages only the dependency-free Win32/GDI host.
- `Publish SDL3` stages only the SDL3 host.
- `Publish All` stages both hosts.

Choose `x64` or `Win32` independently in the platform selector. Publish
configurations are Release builds with the static MSVC runtime and produce
Visual Studio 2017's bundled CMake predates preset support, so its dispatcher
and native project locate CMake 3.20 or newer from `PATH` or a newer installed
Visual Studio while retaining the native VS15 generator and v141 compiler.

For the native Visual Studio 2015 IDE, open
`ide/visual-studio/vs2015/wolf3d-portable.sln`. This focused
compatibility-band solution exposes Debug, Release, dynamic-CRT variants, and
`Publish Win32` without presenting unsupported SDL3 configurations. Its
`Publish Win32 XP` configuration selects the v140_xp profile; its x86 output
is XP-validated and its x64 destination remains untested. VS2015 does not
bundle CMake, so the solution uses CMake 3.20 or newer from `PATH` or
from a newer installed Visual Studio while still compiling through MSBuild 14
and MSVC 19.0.

### Windows build dispatcher

The root `build.ps1` is the human-facing entry point for selecting one build
without memorizing preset names. It detects supported Visual Studio and MSYS2
UCRT64 installations and exposes compiler, x86/x64 architecture, Win32/SDL3/both
wrappers, Debug/Release, static/dynamic CRT, compiled OPL drivers, runtime
default, preferred PCM rate, build/publish/clean actions, parallelism, and a
dry-run mode:

```powershell
.\build.ps1 -List
.\build.ps1
.\build.ps1 -Compiler vs2026 -Architecture x64 -Wrapper all
.\build.ps1 -Compiler vs2022 -Architecture x64 -Wrapper sdl3
.\build.ps1 -Compiler vs2019 -Architecture x86 -Wrapper win32
.\build.ps1 -Compiler vs2017 -Architecture x64 -Wrapper win32
.\build.ps1 -Compiler vs2015 -Architecture x86 -Wrapper win32
.\build.ps1 -Compiler vs2015-xp -Architecture x86 -Wrapper win32
.\build.ps1 -Compiler vs2013 -Architecture x86 -Wrapper win32
.\build.ps1 -Compiler vs2012 -Architecture x86 -Wrapper win32
.\build.ps1 -Compiler vs2010 -Architecture x86 -Wrapper win32
.\build.ps1 -Compiler vs2008 -Architecture x86 -Wrapper win32
.\build.ps1 -Compiler mingw-ucrt64 -Wrapper win32
.\build.ps1 -Compiler mingw-ucrt64 -Wrapper sdl3
.\build.ps1 -Compiler mingw-ucrt64 -Wrapper all
.\build.ps1 -Compiler mingw-ucrt64 -Wrapper all -Publish
.\build.ps1 -Action build -Configuration Debug -Runtime dynamic -Wrapper all
.\build.ps1 -Wrapper sdl3 -DefaultOpl dbopl -SampleRate 44100
.\build.ps1 -Wrapper win32 -Drivers silent -DefaultOpl silent
.\build.ps1 -Compiler vs2022 -Wrapper win32 -DryRun
```

With no arguments it verifies recorded submodules, offers to initialize
missing revisions, reports detected toolchains, prompts for relevant choices,
prints a reproducible build plan, and requests final confirmation. It never
installs external tools or advances a submodule beyond the recorded commit.
Explicit arguments remain suitable for automation; `-NonInteractive` applies
the defaults without prompting. `auto` prefers
VS2026/v145 and falls back through each installed compiler to VS2008/v90. All
v141-and-older and v140_xp choices accept only `-Wrapper win32`. Publishing is
a Release-only operation; Debug remains a development build. The root launcher
delegates to `scripts/windows/invoke-build.ps1`, which prints every CMake command
before running it and does not duplicate build logic.

The MinGW profile targets x64 through MSYS2 UCRT64. Install the UCRT64 GCC
toolchain together with its native CMake and Ninja packages. The dispatcher
uses `C:\msys64` by default; pass `-Msys2Root C:\path\to\msys64` for a
portable or non-default installation. It prepends that installation only to
the current process environment. Release packages statically link GCC support
code while retaining Windows' UCRT, and the publish action rejects imports of
`msys-2.0.dll`, `cygwin1.dll`, libgcc, libstdc++, or winpthread DLLs.

For a fresh MSYS2 installation, open the **MSYS2 UCRT64** terminal and update
the package database and base installation:

```sh
pacman -Syu
```

If MSYS2 asks you to close the terminal, reopen the **MSYS2 UCRT64** terminal
and run `pacman -Syu` again. Then install the required x64 UCRT toolchain and
build tools:

```sh
pacman -S --needed \
  mingw-w64-ucrt-x86_64-toolchain \
  mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-ninja
```

Return to ordinary PowerShell and run `.\build.ps1 -List` to verify that
`mingw-ucrt64` is reported as ready. Do not install MSYS2's SDL3 package for
this project: the portable build compiles the pinned SDL3 submodule.

The equivalent direct presets, run from an MSYS2 UCRT64 shell, are:

```text
cmake --preset windows-mingw-ucrt64-dev-x64
cmake --build --preset windows-mingw-ucrt64-dev-x64

cmake --preset windows-mingw-ucrt64-release-x64
cmake --build --preset windows-mingw-ucrt64-release-x64

cmake --preset windows-mingw-ucrt64-sdl3-x64
cmake --build --preset windows-mingw-ucrt64-sdl3-x64
```

## Linux

Run `./build.sh` without arguments for the guided Linux configurator. It
verifies and, with confirmation, initializes the recorded submodules; reports
GCC, Clang, CMake, Make, Docker, and Git; shows only usable build paths; and
prints the exact Make command before executing it. Arguments bypass the
wizard and are forwarded to Make.

`make` defaults to the pinned-SDL3 distribution. Useful targets include:

```text
make help
make dependencies
make fresh
make sdl3-release
make console-release
make releases
make portable-sdl3
make portable-console
make portable
make musl-sdl3
make clean
```

GCC is the default. Select Clang with `CC=clang`. Set
`USE_SYSTEM_SDL3=ON` to use the installed SDL package. `OPL_DRIVERS` is a
comma-separated subset of `nuked,dbopl,silent`; `OPL_DEFAULT` chooses one
included driver and `SAMPLE_RATE` sets the preferred application PCM rate.
`CMAKE_ARGS` passes additional definitions through to configuration, and
`JOBS=N` limits build parallelism.

The canonical all-driver/Nuked/48 kHz configuration retains the short package
name. Any reduced driver set, non-Nuked default, or non-48 kHz preferred rate
is encoded as a deterministic directory suffix so multiple configurations can
coexist under `dist/`.

The console host requires libdrm and ALSA development packages. SDL3 builds do
not require those packages directly. The pinned build dynamically discovers
available X11, Wayland, KMS/DRM, and audio backends.

## Reproducibility and latest-main builds

Ordinary builds are offline with respect to `wolf3d-lib`: they use the
committed gitlink. `make fresh` runs:

```text
git submodule update --remote --merge -- lib/wolf3d
git submodule update --init --recursive
make all
```

This is intentionally convenient for active integration. Commit the resulting
gitlink to make that engine selection reproducible. Every staged folder
contains `WOLF3D-LIB.txt` with the resolved `1.4.REVISION` and full commit SHA.

## Linux portability

The portable targets build inside the digest-pinned Debian 10 container and
audit staged ELFs against glibc 2.28. The SDL package includes its pinned SDL3
shared object and uses `$ORIGIN`; normal kernel, graphics, input, and audio
facilities remain system supplied. `make clean` removes project build trees
and the project-named Docker image without pruning unrelated Docker data.

For a distribution-independent userspace bundle, run:

```text
make musl-sdl3
```

`make universal-sdl3` is an equivalent descriptive alias. This target builds
the executable, wolf3d-lib, Nuked-OPL3, and the pinned SDL3 inside a
digest-pinned Alpine 3.20 container. It stages an AppDir-style directory named
`wolf3d-portable-1.4.REVISION-sdl3-linux-musl-x64` containing a top-level
`wolf3d` launcher, the application under `bin/`, and a closed set of shared
objects plus the musl loader under `lib/`.

Original game data belongs beside the top-level `wolf3d` launcher. The
launcher preserves that top-level path as the hosted process's executable
identity, so automatic data discovery and the `wolf*`/`spear*` executable-name
hint behave like the flat native packages. An explicit `--data PATH` still
overrides automatic discovery.

GCC is the default musl compiler. `make musl-sdl3 CC=clang` selects the Clang
toolchain in the same container and keeps its build tree separate.

The audit rejects GLIBC symbol versions, rejects unresolved direct ELF
dependencies, and executes `wolf3d --sdl3-help` through the bundled loader.
The resulting directory can be copied intact to another x86-64 Linux system;
The bundle carries musl-built X11, Wayland, eudev, ALSA, and PulseAudio client
libraries because its private musl process cannot load a destination's
glibc-built copies. It discovers the destination display, input, and audio
services at runtime without replacing their servers or device drivers. PipeWire's
native client path is omitted from this bundle because it requires a separate
plugin/configuration tree; PipeWire systems remain supported through their
PulseAudio compatibility service. OpenGL, OpenGL ES, Vulkan, SDL GPU, and
KMSDRM are deliberately disabled: wolf3d-portable already renders its
framebuffer in software, and excluding those paths avoids graphics-stack ABI
dependencies. The audit opens an SDL software-rendered window under Xvfb so a
help-only launch can no longer conceal a missing video backend. The separate
direct-console host remains available for a DRM/KMS-only system.
