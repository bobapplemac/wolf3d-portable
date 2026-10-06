# Changelog

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
