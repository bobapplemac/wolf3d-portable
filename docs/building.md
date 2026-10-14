# Building wolf3d-portable

The [build and compatibility matrix](support-matrix.md) is the concise record
of supported compilers, wrappers, produced artifacts, and runtime-validated
destination operating systems. This document supplies the detailed commands,
dependencies, and package behavior.

For cloning and confirmed updates, see [source updates](source-updates.md).

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

The legacy definition always compiles the selected library submodule as part of
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
configurations are Release builds with the static MSVC runtime and stage
ready-to-copy packages under `dist/`.

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
make dos
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

The console host requires libdrm and ALSA development packages. At runtime it
prefers a connected DRM/KMS display and falls back to the kernel framebuffer
interface (`$FRAMEBUFFER` or `/dev/fb0`); both paths retain evdev input and ALSA
audio. SDL3 builds do not require those packages directly. The pinned build
dynamically discovers available X11, Wayland, KMS/DRM, and audio backends.

## Windows cross-compilation from Linux

The Linux/Docker dispatcher can emit native Windows packages without Wine:

```text
make windows-win9x       # Open Watcom x86 Win32/GDI
make windows-xp          # MinGW-w64/MSVCRT x86 Win32/GDI
make windows-win7        # MinGW-w64/MSVCRT x86+x64 Win32/GDI and SDL3
make windows-llvm-win7   # LLVM-MinGW/MSVCRT x86+x64 Win32/GDI and SDL3
make windows-win10       # LLVM-MinGW/UCRT x64 Win32/GDI and SDL3
make windows-cross       # complete matrix
```

SDL3 deliberately starts at the Windows 7 cross profile. The Win9x host uses
ANSI Win32 APIs and the legacy cursor-warp input path; the XP profile keeps the
dependency-free Win32/GDI host. Every package compiles the recorded
`wolf3d-lib` submodule and includes its runtime DLLs. Automated PE audits
verify architecture and reject accidental GNU support DLLs, UCRT leakage into
MSVCRT profiles, and known post-Win9x imports. Runtime certification on the
named Windows versions is recorded separately in the support matrix.

## 32-bit DOS cross-build

The DOS wrapper is built on Linux through Docker; no native Open Watcom
installation is required. The library submodule pins the exact official Open
Watcom v2 snapshot and verifies its installer before creating the builder
image. Run:

```text
make dos
```

The guided `./build.sh` configurator also offers **DOS32 / Open Watcom** when
Docker is usable. The result is staged at
`dist/wolf3d-portable_1.4.REVISION_dos32_x86_vga_openwatcom<major>/` and contains:

- `WOLF3D.EXE`, the 32-bit LE protected-mode game;
- `DOS4GW.EXE`, which is the bundled DOS/32A drop-in loader rather than the
  original proprietary DOS/4GW program;
- project, engine, third-party, and DOS/32A notices.

Copy a legally obtained WL1, WL6, SDM, SOD, SD1, SD2, or SD3 data set beside
the two executable files, then run `WOLF3D`. The supported baseline is a
Pentium-class or newer x86 DOS system with VGA. The initial host uses direct
mode 13h presentation, IRQ 1 keyboard input, a 700 Hz PIT IRQ 0 clock, and
SB16 44.1 kHz 16-bit signed stereo auto-initialize DMA. Sound Blaster settings
come from the standard `BLASTER` environment variable and require an SB16-
compatible DSP plus a high-DMA channel. If detection fails, a timed null sink
keeps audio rendering and completion clocks alive without producing samples.
The default package compiles DBOPL, timing-preserving silence, and native AdLib
hardware output; select them at runtime with `--opl dbopl`, `--opl silent`, or
`--opl adlib`. Native AdLib is the DOS default, sends the same engine OPL2
register writes directly to port 388h, and retains SB16 PCM for digitized
sound. DBOPL is the practical software fallback. Nuked remains the preservation
reference on modern hosts but is opt-in under DOS because running it inside a
virtualized DOS CPU is substantially more expensive.

When Nuked is included, the package's `RELINK/` directory carries the host
objects, engine library, separately replaceable `NUKEDOPL.LIB`, the native
adapter `WGADLIB.LIB`, response file,
and plain-text instructions needed to relink with Open Watcom. This is part of
the DOS package's LGPL compliance and should remain with redistributed builds.
Custom builds can still set `DOS_OPL_DRIVERS` and
`DOS_OPL_DEFAULT`, for example:

```text
make dos DOS_OPL_DRIVERS=dbopl,silent DOS_OPL_DEFAULT=dbopl
make dos DOS_OPL_DRIVERS=nuked,dbopl,silent,adlib DOS_OPL_DEFAULT=nuked
```

The current package is compile-and-package validated and has completed
headless DOSBox startup smokes with WL1 data through the Nuked, DBOPL, silent,
native AdLib, SB16, and no-device paths. Interactive DOSBox gameplay also
confirms native AdLib, DBOPL, and silent operation; an initially silent native
AdLib session was resolved by removing stale `CONFIG.WL1` state. These tests
establish that the extender, DMA/IRQ initialization, null fallback, runtime
driver selection, and engine run loop remain live. Physical DOS hardware
gameplay is not yet claimed. `make clean-dos` removes
its build tree and project-specific Docker image. `make clean` includes the
same cleanup without pruning unrelated Docker objects.

## Reproducibility and latest-main builds

The root scripts and `make fresh` use the confirmed update workflow described
in [source-updates.md](source-updates.md). Direct Make/CMake commands build the
currently selected sources. Packages record their actual source revisions and
all settings in DOCS/BUILD.TXT; see [build identity](build-identity.md).

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
`wolf3d-portable_1.4.REVISION_linux-musl_x64_sdl3_gcc<major>` containing a top-level
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
direct-console host remains available for DRM/KMS and fbdev systems.

### Win9x command-line and native OPL

The Open Watcom Win9x executable is a console-subsystem application with a GDI
game window. `wolf3d --help` and `wolf3d --diag` write to the invoking console;
redirection to a file works too. `Wolf3d.ini` is read beside the executable,
independently of the command prompt's current directory. Each line contains one
option, for example `--opl dbopl`. Command-line options override that option
family in the INI. Unknown or uncompiled driver names produce an error listing
the available names before a game window is created.

DOS and Win9x default to `dbopl,silent,adlib`, with `adlib` selected at runtime.
Win9x uses the same ISA OPL probe/register delays as DOS, without DOS interrupt
masking. It only probes ports 388h/389h on Windows 95/98/Me; NT-based Windows
requires `--opl dbopl` or `--opl silent`. Compatible physical or emulated ISA OPL
hardware must actually be exposed. The `--adlib` hardware-profile switch is
separate from the `--opl adlib` native renderer.

`make windows-win9x WIN9X_OPL_DRIVERS=nuked,dbopl,silent,adlib
WIN9X_OPL_DEFAULT=nuked` opts back into Nuked. Existing explicit `OPL_DRIVERS`
and `OPL_DEFAULT` overrides also remain accepted by that target.

Run `tests/WG_WIN9X_SMOKE.ps1 -PackagePath <package>` on Windows to exercise the
actual Open Watcom executable's console output, INI lookup, and driver errors.
This does not substitute for native AdLib playback testing on Windows 98.

## Relocatable musl KMS/fbdev package

Select **Linux musl KMS/fbdev relocatable distribution (Docker)** in `./build.sh`,
or run `make musl-console JOBS=8`. `make musl-all` produces both SDL3 and
KMS/fbdev musl packages. `make musl-console-audit` checks an existing console
bundle; the existing `musl-audit` target continues to check the SDL3 bundle.

The console output is
`dist/wolf3d-portable_<version>_linux-musl_x64_kms-fbdev_gcc<major>/` (or clang).
Docker is needed only for building/auditing. The package bundles its loader,
libdrm, ALSA, and dependencies, with a private direct-hardware ALSA configuration.
It does not load host ALSA configuration or glibc plugins. The launcher uses
`/proc/self/fd/9` to pass its private configuration safely through ALSA's path-list
parser; `/proc` must be mounted. The default PCM is
`plughw:0,0`; use `--alsa-device plughw:CARD,DEV` to choose another device.
`--alsa-device null` is useful for a silent ALSA path check.

For a physical console test, place original game data beside the top-level
launcher or supply `--data /path/to/WL1`. From an active virtual console:

```sh
./wolf3d --diag
./wolf3d --video drm --no-audio --data /path/to/WL1
./wolf3d --video fbdev --no-audio --data /path/to/WL1
./wolf3d --video auto --alsa-device plughw:0,0 --data /path/to/WL1
```

Use `--drm-device /dev/dri/cardN`, `--fb-device /dev/fbN`, or repeated
`--input-device /dev/input/eventN` if automatic selection picks the wrong device.
A VGA-connected monitor can still use KMS; fbdev requires an actual `/dev/fbN`.
The user needs display/input/audio device permissions and KMS display ownership.
An active desktop may own the display. Verify keyboard/mouse, sound, image
scaling, and restoration of the original console after normal exit.

The automatic audit checks dependency closure, no glibc references, relocation
(including paths with spaces), launcher naming, and bundled ALSA null PCM.
It does not take control of the display or claim hardware validation. Build-info
preserves all build options plus the bundle's ALSA configuration and launcher.
Custom directories/settings use `MUSL_CONSOLE_BUILD_DIR`,
`MUSL_CONSOLE_STAGE_ROOT`, `MUSL_CONSOLE_DIST_DIR`, and
`MUSL_CONSOLE_CMAKE_ARGS`. `make clean-musl` clears both musl backend build trees.

## DOS audio regression checks

Run `python tests/WG_DOS_AUDIO_TEST.py --cc gcc` with a native GCC compiler.
The test compiles the production DOS driver against simulated ISA port I/O,
DSP responses, DMA allocation and interrupt vectors. It checks SB2/Pro and
SB16 selection, sample conversion, rate negotiation, DMA commands, interrupt
acknowledgement, teardown, invalid settings, timeouts and fallback clock drift.
It does not establish physical card compatibility; follow it with `make dos`
and real-hardware music and digitized-effect testing. Python is a maintainer
test dependency only, not a build or game requirement.

`tests/WG_DOS_PCM_SMOKE.c` is the complementary DOS hardware/emulator test.
Build it with Open Watcom, linking `platforms/dos/WG_DOS_SB16.c` and using
`-bt=dos -l=dos4g -dWOLF3D_STATIC -i=lib/wolf3d/include -i=platforms/dos`.
Place DOS/32A's `DOS4GW.EXE` beside the executable. It plays a quiet test tone,
requires 32 successful DMA block submissions within ten seconds, and shuts
playback down. A failure with no sound card is expected. SB Pro 2 and SB16
DOSBox runs passed this test; this does not replace physical Yamaha testing.

DOS and Open Watcom Win9x packages default to 22050 Hz PCM to reduce CPU cost
on period hardware. `--sample-rate HZ` accepts integer requests from 8000 to
192000; the host may negotiate a lower or rounded hardware rate. DOS build
rates can be overridden with `DOS_SAMPLE_RATE`; use
`WIN9X_SAMPLE_RATE` for Win9x only. Explicit `SAMPLE_RATE` also overrides the
Win9x default for compatibility with existing build commands. Modern targets
retain their 48000 Hz default. Native OPL pitch and tempo do not depend on the
PCM rate when the audio clock is correctly paced.

Run `python tests/WG_ADLIB_INIT_TEST.py --cc gcc` to verify native AdLib register
initialization in DOS and Win9x builds. It simulates OPL3 hardware left in
extended mode, an original OPL2 chip, absent hardware and the NT I/O guard.
The companion `tests/WG_ADLIB_INIT_TEST.c` checks that only detected OPL3
hardware receives second-bank reset writes. Physical Win9x native AdLib and DBOPL playback have been confirmed on a
Pentium III with Yamaha OPL3-SAx hardware. A passing register test alone does
not establish mixer routing or exclusive access against the Windows sound driver.

Run `python tests/WG_DOS_VIDEO_TEST.py --cc gcc` for DOS exit cleanup regressions.
The companion `tests/WG_DOS_VIDEO_TEST.c` checks real-mode INT 43h save/restore,
DPMI failure handling, artwork preservation, prompt attributes, narrow screens
and full-height cards. Confirm final DOS/32A shutdown and COMMAND.COM prompt
colors on physical hardware as well.
`tests/WG_DOS_VIDEO_SMOKE.c` also exercises the production helpers under DOS/32A: BIOS graphics/text mode switches, B800 text attributes, and exact real-mode vector restoration. Its result is written to `VIDEO.TXT`.

## Windows 2000 Docker target

Run `make windows-win2k` on Linux (or choose the Windows 2000 option in
`./build.sh`). The pinned Debian/MinGW-w64 GCC image builds x86 MSVCRT binaries;
no Windows development VM is required. The target is included in
`make windows-cross`. The portable package uses GDI and legacy mouse input,
without the XP-only Raw Input imports. SDL3 is not supported on this target.

Defaults include only DBOPL and silent, with DBOPL selected. The target-specific
`WIN2K_OPL_DRIVERS` and `WIN2K_OPL_DEFAULT` Make variables override these;
general OPL settings for other targets do not change them. Optional [native AdLib access](native-opl.md) is available for x86 GDI
through InpOut32, with physical testing pending. The existing `SAMPLE_RATE` override is
supported (default 48000 Hz); use the runtime `--sample-rate` option to compare
rates on slower hardware. Other platform defaults are unchanged.

The PE audit checks x86 architecture, runtime dependencies, known post-2000
imports and the subsystem version of each staged EXE/DLL. This is not a full
Windows API compatibility proof: Windows 2000 startup, input, sound and
performance still require physical testing. Use a fully updated Windows 2000
SP4 installation for the initial test baseline.

On a Windows test host, run `tests/WG_WIN9X_SMOKE.ps1 -Profile win2k` with
`-PackagePath` and `-WorkRoot` to verify the default driver, exact compiled
driver list, CLI/INI handling and rejection of unavailable native AdLib.
Running this on modern Windows does not establish Windows 2000 compatibility.

## Windows GDI window size and optional display mode

The GDI host starts at scale 3 (a 960x720 client area). Use `--window-scale N`
with an integer from 1 through 8 to choose the initial client area: 1 gives
320x240, 2 gives 640x480, 3 gives 960x720, and 4 gives 1280x960. Window borders
are additional. Manual resizing remains available; rendering preserves 4:3
with black bars when the client area has a different aspect ratio.

Plain `--fullscreen` keeps borderless fullscreen at the desktop resolution.
`--fullscreen-resolution 640x480` optionally selects a temporary primary-display
mode when entering fullscreen, including through F11 or Alt+Enter. It does not
itself start fullscreen. The current color depth is preserved and the driver
selects the refresh rate. Unsupported modes report an error; no alternate
resolution is silently selected. Both dimensions must be between 320x240 and
8192x8192, respectively; non-4:3 displays still use 4:3 rendering.

The previous desktop mode is restored on leaving fullscreen, focus loss, and
normal/error shutdown. Focus loss minimizes the mode-switched window; activation
reapplies the selected mode, falling back to windowed mode with an error if it
is no longer available. Default borderless fullscreen is unaffected. Forced
process termination cannot guarantee cleanup.

Both options work as separate lines in `Wolf3d.ini`; command-line values override
matching INI options. For example:

```text
--window-scale 2
--fullscreen-resolution 640x480
--sample-rate 11025
```

Run `python tests/WG_WIN32_DISPLAY_TEST.py --cc gcc` for parsing and mocked mode
lifecycle checks (`tests/WG_WIN32_DISPLAY_TEST.c`). The Windows package smoke
script also checks CLI/INI validation; actual display switching and focus changes
must be tested on the destination Windows/display driver.

The win2k GDI package uses a console-subsystem launcher (like Win9x), because
Windows 2000 has no `AttachConsole` API. Start it from a command prompt to keep
help, diagnostics and the colored B800 exit/error card visible after it exits.
The game itself still runs in its graphical window. An Explorer launch creates
its own console and waits for a fresh keypress before closing. Use `--no-pause`
on the command line or as a line in `Wolf3d.ini` to disable this wait. A hard crash
cannot guarantee the engine's error-screen or display-restoration cleanup.

On Windows, run `python tests/WG_WIN2K_CONSOLE_TEST.py PATH/TO/wolf3d.exe`
with MinGW GCC on PATH. The companion `tests/WG_WIN2K_CONSOLE_TEST.c` creates
an isolated hidden console and checks real console-buffer help/diagnostic
output and the shared B800 renderer, rather than testing redirected pipes alone.

Win9x and win2k console launchers pause after gameplay, help, diagnostics and
handled errors only when they detect a private interactive console. Redirected
input/output and launches from a normal command prompt do not pause. Keyboard
keys held at exit must be released and queued input is discarded before the
prompt accepts a fresh keypress. On XP and newer, detection uses the dynamically
loaded console process-list API. Win9x/2000 lack that API, so detection uses the
initial untouched (0,0) console cursor. An unusual launcher or batch file that
clears an inherited console to (0,0) without printing a prompt can resemble a
new console; use `--no-pause` for those automation cases.
