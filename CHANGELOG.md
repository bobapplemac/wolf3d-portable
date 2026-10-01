# Changelog

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
