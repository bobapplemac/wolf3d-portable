# Changelog

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
