# Changelog

## 2026-10-07 - Native Open Watcom DOS workspace

- Added one Open Watcom IDE workspace that builds the pinned engine,
  independently replaceable Nuked implementation, `WGADLIB.LIB`, and the DOS
  executable from their canonical source files.
- Added a dependency-free launcher with submodule validation and shared include
  and driver configuration.
- Added deterministic descriptor generation and ignored all IDE build output.

## 2026-10-07 - Open Watcom IDE integration

- Advanced the pinned engine to wolf3d-lib 1.4.51, including its validated
  native Open Watcom workspace and clean generated-output policy.
- Kept the Docker/Open Watcom package build as the reproducible DOS release
  path while making the engine library directly browsable and buildable in the
  period-appropriate IDE.

## 2026-10-07 - Native DOS AdLib hardware output

- Advanced the pinned engine to wolf3d-lib 1.4.48 and its optional hardware
  OPL bridge.
- Added an original-style port-388h AdLib adapter with timer-based hardware
  detection, register initialization, required bus delays, and clean shutdown.
- Added `--opl adlib` to the default DOS runtime choices while retaining Nuked
  as the preservation reference and leaving non-DOS build defaults unchanged.
- Built the hardware adapter as a separate `WGADLIB.LIB` and included it in
  the DOS relink kit alongside the separately replaceable Nuked library.

## 2026-10-07 - Runtime-selectable DOS OPL emulation

- Advanced the pinned engine to wolf3d-lib 1.4.47 and kept Open Watcom
  diagnostic reports inside build output rather than the source checkout.
- Enabled Nuked-OPL3, DBOPL, and timing-preserving silence in the default DOS
  build and exposed the existing `--opl` runtime selection consistently.
- Retained Nuked as the reference default while documenting DBOPL as the
  lower-CPU option for period hardware.
- Added an Open Watcom relink kit whenever Nuked is included, keeping its
  implementation in a separately replaceable library and supplying the host
  objects and response file required for LGPL-compliant relinking.
- Retained compile-time subsets through `DOS_OPL_DRIVERS` and
  `DOS_OPL_DEFAULT` for constrained targets.

## 2026-10-07 - DOS Sound Blaster 16 PCM transport

- Added 44.1 kHz, 16-bit signed stereo SB16 output through auto-initialize
  high-DMA double buffering and shared-IRQ-safe DSP acknowledgement.
- Read the base port, IRQ, and high-DMA channel from the standard `BLASTER`
  environment variable and required a version 4.x-or-newer DSP.
- Added a wall-clocked null PCM sink when no compatible card is available so
  silent DOS systems continue advancing the same mixer and completion clocks.
- Kept the package's initial OPL selection silent pending the separate DBOPL
  and Nuked runtime validation checkpoint.
- Rebuilt warning-clean with Open Watcom and completed DOSBox startup smokes
  with WL1 data through both emulated-SB16 and null-audio paths.

## 2026-10-07 - Initial Open Watcom DOS32 host

- Advanced the pinned engine to wolf3d-lib 1.4.46 and consumed its canonical
  Open Watcom library build.
- Added a Docker-only Linux cross-build for a 32-bit protected-mode DOS host
  with direct VGA mode 13h output, IRQ 1 keyboard input, and original-style
  700 Hz PIT timing.
- Staged DOS/32A as the separately licensed `DOS4GW.EXE` drop-in loader and
  documented the Pentium-or-newer baseline and complete package contents.
- Kept the first checkpoint intentionally silent while preserving audio
  clocks; SB16 PCM and OPL backends remain subsequent milestones.
- Compile-and-package validated the distribution and completed a headless
  DOSBox startup smoke with WL1 data. Physical DOS gameplay remains pending.

## 2026-10-07 - MSVC 2008 warning-clean library build

- Advanced the pinned engine to wolf3d-lib 1.4.45.
- Restored warning-clean VS2008/v90 wrapper builds by inheriting the library's
  explicit opt-out for the legacy CRT's `_vsnprintf` deprecation diagnostic.

## 2026-10-07 - Windows runtime compatibility validation

- Advanced the pinned engine to wolf3d-lib 1.4.44.
- Validated both MinGW UCRT64 wrappers with WL1 data on Windows 11 x64 after
  package dependency audits confirmed no MSYS2/Cygwin runtime leakage.
- Validated the v140_xp x86 Win32/GDI package with WL1 data on Windows XP SP3;
  Windows XP x64 remains intentionally outside the destination test plan.

## 2026-10-07 - MSYS2 bootstrap documentation

- Documented the complete fresh-install MSYS2 UCRT64 update and package setup
  needed by the MinGW build profile, including its PowerShell detection check.
- Clarified that the Windows MinGW build compiles the repository's pinned SDL3
  submodule and does not require MSYS2's SDL3 package.

## 2026-10-07 - Guided Windows build execution

- Advanced the pinned engine to wolf3d-lib 1.4.42.
- Corrected the interactive Windows wizard's final handoff to use named
  PowerShell parameter splatting, so accepting the displayed plan now invokes
  exactly the reproducible executor command shown above it.

## 2026-10-07 - Visual Studio 2017 and 2026 validation

- Advanced the pinned engine to wolf3d-lib 1.4.41.
- Added native Visual Studio 2026/v145 x86 and x64 presets, dispatcher
  detection, and a generation-specific solution with Win32/GDI and SDL3
  development and publish configurations.
- Gave VS2026 packages the same collision-resistant `msvc-v145` directory
  label used by every other compiler-qualified Windows distribution.
- Corrected exact VS2017 IDE discovery and made it borrow a newer CMake when
  its bundled CMake is too old for presets, while retaining the v141 compiler
  and native Visual Studio 15 generator.
- Validated the Win32 wrapper with exact VS2017 and both Windows wrappers with
  exact VS2019, VS2022, and VS2026 compilers on Windows 11.

## 2026-10-06 - Musl launcher data discovery

- Preserved the top-level launcher's path as the hosted SDL executable's
  `argv[0]`, restoring automatic discovery of game data beside `wolf3d` and
  retaining renamed `wolf*`/`spear*` family selection.
- Extended the musl audit to reject a launcher that exposes the internal
  `bin/wolf3d-sdl3` path to the engine.

## 2026-10-06 - Functional musl desktop backends

- Bundled musl-built X11, Wayland, ALSA, and PulseAudio client libraries plus
  their recursive ELF dependencies instead of attempting to load incompatible
  glibc system libraries into the private musl process.
- Added an Xvfb-backed SDL window smoke test to the musl package audit so a
  help-only launch can no longer pass with no functional video device.
- Disabled the musl-native PipeWire and libdecor plugin paths; PipeWire
  desktops remain supported through PulseAudio compatibility, while Wayland
  uses SDL's protocol-native decoration path.

## 2026-10-06 - Custom audio package auditing

- Advanced the pinned engine to wolf3d-lib 1.4.40.
- Matched GNU Make's portable glibc and musl staging/audit paths to the
  deterministic suffix emitted for non-default audio configurations.

## 2026-10-06 - Runtime-selectable audio drivers

- Advanced the pinned engine to wolf3d-lib 1.4.39 and platform API v3.
- Updated Win32, SDL3, and Linux-console hosts to report their obtained
  application-facing PCM format before the engine constructs its OPL driver.
- Included Nuked-OPL3, DBOPL, and timing-preserving silence by default, with
  `--opl` runtime selection and `--sample-rate` preferred-rate control.
- Added independent compiled-driver-set, default-driver, and sample-rate
  choices to GNU Make and the guided modern Windows, Linux, and XP build flows.
- Gave non-default audio configurations deterministic package suffixes so they
  coexist with canonical all-driver/Nuked/48 kHz releases.

## 2026-10-06 - Guided cross-platform build configuration

- Advanced the pinned engine to wolf3d-lib 1.4.37.
- Split modern Windows configuration from deterministic execution while
  retaining all existing parameter-driven root commands.
- Added dependency-free guided entry points for modern Windows, Linux, and
  Windows XP that detect supported local build paths and print reproducible
  commands before executing them.
- Added Git dependency verification and confirmed initialization of missing
  recorded submodule revisions without installing external build tools or
  advancing dependency versions implicitly.
- Accepted complete source-export trees without Git metadata when all recorded
  dependency contents are already present.

## 2026-10-06 - Windows publish-switch alias

- Added `-Publish` as a convenient alias for `-Action publish` in the Windows
  build dispatcher, including the documented MinGW all-wrapper command.

## 2026-10-06 - MinGW UCRT64 Windows builds

- Advanced the pinned engine to wolf3d-lib 1.4.36.
- Added first-class MSYS2 UCRT64/GCC 16.2 x64 presets and interactive
  `build.ps1` support for both the Win32/GDI and pinned-SDL3 wrappers.
- Fixed portable Win32 procedure loading, structure initialization, and
  Unicode GUI entry-point linking without weakening warnings.
- Statically linked GCC support code and added a publish-time import audit
  that rejects dependencies on MSYS, Cygwin, libgcc, libstdc++, or winpthread
  runtime DLLs.

## 2026-10-06 - VC6 Windows 11 runtime validation

- Advanced the pinned engine to wolf3d-lib 1.4.35.
- Recorded a successful manual run of the VC6-built x86 Win32/GDI package on
  Windows 11 x64 under WOW64, alongside its existing Windows XP SP3 result.

## 2026-10-06 - Native Visual Studio generation matrix

- Advanced the pinned engine to wolf3d-lib 1.4.34.
- Added one native, toolset-pinned solution/project pair per Visual Studio IDE
  from VS2002 through VS2022, plus a period-correct VC6 workspace; every build
  continues to compile the pinned engine submodule.
- Kept SDL3 confined to VS2019 and VS2022 while all historical IDE projects
  expose only the dependency-free Win32/GDI wrapper.
- Validated VS2008--VS2013 on Windows 7 and VC6--VS2005 on Windows XP through
  their exact IDE command-line hosts; native VS2017 IDE testing remains
  pending a matching installation.
- Corrected the XP-native dispatcher so inherited nonzero `ERRORLEVEL` state
  cannot falsely fail an already-created build directory.

## 2026-10-06 - Compatibility-banded build layout

- Advanced the pinned engine to wolf3d-lib 1.4.33.
- Moved the shared VS2019/VS2022 solution and project into the explicit
  `ide/visual-studio/vs2019-vs2022` band without changing its CMake-backed
  development or publish behavior.
- Moved the XP-native VC6--VS2005 dispatcher to
  `scripts/windows/legacy/build.cmd` and established `scripts/linux/` for
  Linux-hosted helpers, including the planned Open Watcom/DOS cross-build.
- Updated every documented build path while retaining `build.ps1` and the
  root Makefile as the stable human-facing dispatchers.

## 2026-10-06 - Build and compatibility matrices

- Advanced the pinned engine to wolf3d-lib 1.4.32.
- Added one authoritative matrix covering every supported Windows and Linux
  build interface, compiler/toolset band, wrapper, architecture, staged
  artifact, and validated destination operating system.
- Clearly separated compiler/package validation from actual destination-OS
  runtime validation and documented the different native-glibc, portable-
  glibc, and bundled-musl compatibility models.
- Recorded MinGW-w64 and Open Watcom/DOS32A as planned work rather than
  implying that design discussion constitutes present support.

## 2026-10-05 - Native Windows XP-era wrapper builds

- Advanced the pinned engine to wolf3d-lib 1.4.31.
- Added a separate CMake 3.5 build and CMD dispatcher for VC6 SP6, VS2002 SP1,
  VS2003 SP1, and VS2005 SP1 x86 Win32/GDI packages.
- Kept every legacy wrapper build compiling the pinned engine and selected OPL
  backend while leaving the modern CMake project and SDL3 support untouched.
- Preserved joystick support without an SDK XInput dependency and supplied an
  old-SDK mouse/fullscreen fallback for compiler generations predating raw
  input and pointer-sized window APIs.
- Compile-and-package validated all four compiler bands on Windows XP SP3 and
  launched the VC6 package there with real WL1 shareware data.

## 2026-10-05 - Win32 legacy compiler span

- Advanced the pinned engine to wolf3d-lib 1.4.30.
- Added first-class Win32/GDI presets and PowerShell-dispatcher choices for
  Visual Studio 2013/v120, 2012/v110, 2010/v100, and 2008/v90.
- Built and staged clean x86 and x64 release packages with every added
  compiler, keeping project warnings as errors throughout.
- Added only the compatibility shims required by pre-C99 MSVC and suppressed
  the anonymous-union warning emitted by VS2008's own `mmsystem.h`.
- Kept SDL3 deliberately restricted to modern supported compiler profiles;
  none of the new legacy configurations downloads, configures, or builds it.
- Made root PowerShell launchers independent of the caller's working directory
  and corrected legacy-install detection under Windows PowerShell 5.1.

## 2026-10-01 - Windows XP targeting candidate

- Added v140_xp x86/x64 Win32 package profiles with static and dynamic CRT
  variants and advanced the engine to wolf3d-lib 1.4.29.
- Verified PE subsystem minimums of 5.01 for x86 and 5.02 for x64 and audited
  direct imports; runtime execution on actual XP systems remains pending.

## 2026-10-01 - Legacy MSVC Win32 wrapper checkpoints

- Added first-class v140 and v141 CMake presets and Windows dispatcher choices
  for x86/x64 Win32 builds with static or dynamic MSVC runtimes.
- Added a focused Visual Studio 2015 solution under the compatibility-banded
  IDE directory; SDL3 remains intentionally limited to modern compilers.
- Advanced the pinned engine to wolf3d-lib 1.4.28.

## 2026-10-01 - License layout and current library identity

- Standardized staged packages on `LICENSE.txt`, `THIRD_PARTY_NOTICES.txt`,
  and a component-specific `LICENSES/` directory.
- Included wolf3d-lib's GPL-2.0 license explicitly even though its text is
  currently identical to wolf3d-portable's own license.
- Advanced wolf3d-lib to 1.4.22 so wrapper titles use `wolf3d` instead of the
  obsolete pre-split `wolf3dgeneric` name.

## 2026-10-01 - Visual Studio publish configurations

- Added first-class `Publish Win32`, `Publish SDL3`, and `Publish All`
  solution configurations for x86 and x64.
- Made Visual Studio publishing invoke the same CMake release staging targets
  and produce the same ready-to-copy `dist/` folders as command-line builds.

## 2026-10-01 - wolf3d-lib 1.4.21 integration and musl application bundle

- Advanced the pinned engine to 1.4.21, including selectable Nuked-OPL3,
  DBOPL, and timing-preserving silent audio implementations in wolf3d-lib.
- Added a digest-pinned Alpine/musl SDL3 build and a relocatable application
  directory containing its own loader, libc, SDL3, wolf3d-lib, and Nuked-OPL3.
- Added a closed-dependency ELF audit, GLIBC-symbol rejection, and launcher
  smoke test for the musl package.
- Kept the Debian 10/glibc 2.28 builds as the conventional Linux alternative.

## 2026-10-01 - wolf3d-lib 1.4.20 integration

- Advanced the pinned engine to the completed library-only `wolf3d-lib`
  revision 1.4.20 and removed obsolete cache switches for hosts that now live
  exclusively in this repository.
- Added explicit line-ending policy for Linux build and audit scripts.

## Unreleased - Repository split

- Created `wolf3d-portable` from a path-filtered copy of the original
  `wolf3dgeneric` history so host work retains authorship and context.
- Moved Win32, SDL3, and direct-console ownership into this repository.
- Added `wolf3d-lib` as a reproducibly pinned, `main`-tracking submodule.
- Made every wrapper compile the submodule library and consume only its public
  `WOLF3D.h` and `wolf3d::wolf3d` interface.
- Added `make fresh` for the explicitly latest-library development workflow
  and embedded the exact library version and commit in every distribution.

Earlier implementation history remains available in Git. Engine behavior and
library revisions are documented by `lib/wolf3d/CHANGELOG.md`.
