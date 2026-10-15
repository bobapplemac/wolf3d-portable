# Win2k modular backend pilot

One `wolf3d.exe` can select GDI or SDL 1.2 at startup. This first implementation
is Win2k x86/MinGW only; other OS/toolchain profiles retain their existing builds.
No engine changes or Python build dependency are required.

## Build

On Linux with Docker:

```sh
make windows-win2k-modular JOBS=8
make windows-win2k-modular WIN2K_BACKENDS=gdi JOBS=8
make windows-win2k-modular WIN2K_BACKENDS=sdl1 JOBS=8
```

The default `WIN2K_BACKENDS=gdi,sdl1` compiles both. GDI-only uses the regular
MinGW Docker image, does not build/find SDL, and bundles neither SDL.dll nor
wolf3d-sdl1.dll. SDL1-containing builds use the existing SDL1 Docker image.
`WIN2K_OPL_DRIVERS` and `WIN2K_OPL_DEFAULT` have the same meaning as the standalone
Win2k builds. The initial CMake entry is `cmake/windows-modular`, with
`WG_WINDOWS_BACKENDS=gdi,sdl1`, `gdi`, or `sdl1`. Build settings and revisions are
recorded in `DOCS/BUILD.TXT`.

Pilot outputs deliberately coexist with established distributions:

```text
wolf3d-portable_<version>_win2k_x86_gdi-sdl1_mingw-gcc12-msvcrt_modular
wolf3d-portable_<version>_win2k_x86_gdi_mingw-gcc12-msvcrt_modular
wolf3d-portable_<version>_win2k_x86_sdl1_mingw-gcc12-msvcrt_modular
```

The combined backend field and `_modular` marker are pilot identities, not a
matrix-wide naming migration. Removing backend fields and consolidating the
other distributions remain follow-up work after hardware validation.

## Select at runtime

```text
wolf3d.exe --backend auto
wolf3d.exe --backend gdi
wolf3d.exe --backend sdl1 --sdl-video directx --sdl-scale hardware
wolf3d.exe --backend sdl1 --fullscreen --fullscreen-mode display --fullscreen-resolution 640x480
wolf3d.exe --diag
```

`auto` is the default: prefer compiled SDL1, then compiled GDI if the optional
SDL dependencies cannot load or SDL video initialization fails. Explicit choices
never switch backends. A backend not included in the build produces a clear
error. Game-data failures, runtime errors and OPL selection failures do not
trigger a backend switch. Selection happens at startup, not during gameplay.

`--backend` also works in `wolf3d.ini`; the command line overrides the config.
The shared launcher merges configuration once and passes the resulting arguments
to the chosen host. `--help` shows compiled choices and the selected host's help.
`--diag` reports compiled choices, auto priority, SDL module/dependency loading
and ABI availability, selection, and the chosen host's diagnostics. Dependency
availability does not promise that a video driver can initialize on that machine.

GDI and SDL1 each still own video, input and PCM output. Selecting SDL1 does not
mix SDL video with GDI input/audio. Native OPL remains separate from PCM output.
Fullscreen, scaling, input and audio options retain their existing host meanings;
see [fullscreen modes](running.md#fullscreen-modes-gdi-sdl1-and-sdl3) and
[SDL1 options](windows-sdl1.md).

## Optional SDL loading

GDI is compiled into the executable. SDL1 is in `wolf3d-sdl1.dll`, which links
against SDL.dll; neither is an executable import. The launcher explicitly loads
both files from its own directory, suppresses Windows loader error dialogs,
checks private backend ABI version 1, and unloads after the host shuts down.
Keep the backend module from the same package as the executable. It is an
internal host boundary, not a supported third-party plugin interface.

A combined package contains `wolf3d.exe`, `wolf3d.dll`, `wolf3d-sdl1.dll`, SDL.dll,
and the existing optional OPL DLLs. SDL LGPL text/source information and InpOut32
checksums/licenses are packaged as before. GDI remains usable if either SDL file
is missing, damaged, incompatible, or unavailable to the loader.

## Validation

All three backend combinations build and pass the Win2k PE audit. GDI-only is
built in the non-SDL image. Black-box Windows tests cover actual SDL loading,
missing DLL/module, mismatched module ABI, config precedence, uncompiled/invalid
choices, strict explicit selection, actual SDL video-init failure, and refusing
to fall back on general game/runtime errors. Existing standalone SDL/GDI builds
remain available. Combined-backend gameplay on physical Win2k hardware is pending.

Maintainer tests (not required for building):

```text
cmake --build build/windows-modular-win2k-gdi-sdl1 --target wg-backend-test wg-backend-good wg-backend-bad
python tests/WG_BACKEND_TEST.py --package <combined-dist> --fixtures <build-dir> --gdi-package <gdi-only-dist> --sdl-package <sdl-only-dist>
```

For hardware validation, compare explicit GDI and SDL1, check fullscreen/F11 and
native OPL, then temporarily rename SDL.dll and verify auto/GDI still launch while
explicit SDL1 prints an error. Restore the DLL afterward.
