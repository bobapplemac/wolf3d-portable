# Repository layout

wolf3d-portable owns host integrations and runnable packages. It consumes the
engine through the public WOLF3D.h API, without private engine include paths.

| Path | Responsibility |
| --- | --- |
| `platforms/` | GDI (win32/), SDL3 (sdl3/), KMS/fbdev (linux-console/) and VGA (dos/) hosts plus shared launcher/API compatibility code. |
| `lib/wolf3d/` | Engine submodule: recorded starting revision, optionally advanced through confirmed updates. |
| `third_party/SDL3/` | Recorded SDL3 source submodule; system SDL3 is an explicit build option. |
| `.github/` | CI and repository automation. |
| `cmake/` | Build graph helpers; legacy/ isolates CMake 3.5-era generators. |
| `docs/` | User and developer guides; README.md is the navigation and ownership index. |
| `ide/` | Version-specific native IDE descriptors; generated workspace caches stay ignored. |
| `packaging/` | Container definitions, package templates, license resources and notice fragments. |
| `scripts/` | Build entry-point implementations and packaging orchestration, grouped by host/toolchain. |
| `tests/` | Runtime, API, build and packaging regression fixtures; external game data remains external. |
| `tools/` | Focused audits and generators; scripts may invoke these during validation. |
| `build/` | Ignored intermediate binaries, logs, local diagnostics and caches. |
| `dist/` | Ignored complete redistributable packages; layout is a separate published contract. |

Root build.sh, build.ps1 and build.cmd are stable user entry points. Makefile,
CMakeLists.txt and CMakePresets.json remain at the root for their tools. README,
CHANGELOG, LICENSE and THIRD_PARTY are discoverable repository metadata.

## Placement and maintenance rules

- Keep source paths stable: native IDE descriptors, scripts and tools reference
  them. A backend vocabulary change does not require renaming its source directory.
- Put orchestration in scripts/, standalone audits/generators in tools/, and
  test fixtures in tests/. A tool used by packaging may remain in tools/.
- Put Dockerfiles, distribution text and licensing inputs in packaging/; keep
  runtime host implementation in platforms/ and CMake logic in cmake/.
- Do not move vendored dependencies or their upstream documents for visual tidiness.
- Generated outputs belong in ignored build/ or dist/, never among tracked source.
- Keep the root README short. Each topic has an owning guide in the
  [documentation index](README.md); link there instead of duplicating option tables.
- Archive completed plans and checkpoint evidence rather than presenting them as
  current support claims. Keep original source/provenance records intact.

[Build naming](BUILD-NAMING.md) defines output directory identities;
[distribution contents](DISTRIBUTION-CONTENTS.md) defines the package payload.
Source-tree organization and distribution organization serve different readers.
