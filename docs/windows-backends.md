# Runtime video backends

Each package has one executable containing its selected video hosts. A host owns
video, input and PCM; native OPL remains independent. Selection happens at startup,
not during gameplay. No separate wolf3d backend DLL is needed.

## Supported combinations

| Target | Compiled choices |
| --- | --- |
| DOS32 | VGA only (`vga`) |
| Win9x, Win2k, XP; legacy Microsoft compilers | GDI, SDL 1.2, or both |
| Modern Windows (SDL3-capable toolchain) | GDI, SDL3, or both |
| Modern Linux glibc/musl | SDL3, KMS/fbdev, or both |
| Experimental Red Hat 9/i586 | SDL 1.2 only |

SDL1 and SDL3 cannot be compiled together. SDL3 requires a supported modern
compiler and Windows 7 or newer. VS2008–2017 use SDL1; VS2019+ and current
MinGW support SDL3. Legacy Microsoft and Win9x builds use the official SDL1
SDK, pinned by SHA-256; Linux cross Win2k/XP builds use the pinned SDL1 source
recipe. GDI-only builds do not download, build, link or stage SDL.

## Build selection

```sh
make releases                         # one Linux SDL3 + KMS/fbdev package
make portable LINUX_ARCH=x86           # one glibc x86 package
make musl-all                         # one relocatable musl x64 package
make windows-win2k                    # GDI + SDL1
make windows-xp VIDEO_BACKENDS=gdi     # no SDL dependency
make windows-win7 VIDEO_BACKENDS=sdl3  # SDL3 only, x86 and x64
make windows-win9x VIDEO_BACKENDS=gdi,sdl1
```

The guided build scripts expose the same choices. On Windows use
`build.ps1 -Wrapper all`, `-Wrapper win32`, or the applicable `-Wrapper sdl1`
/ `-Wrapper sdl3`. `all` means both hosts in one executable. `build.cmd` accepts
an optional seventh argument `gdi,sdl1`, `gdi`, or `sdl1` for VC6–VS2005.
Direct modern CMake supports `-DW3P_VIDEO_BACKENDS=gdi,sdl3` (or another valid
comma-separated subset), followed by `cmake --build BUILD --target release`.
Existing `win32-release`, `sdl3-release`, and `console-release` targets are aliases
for the same configured release. The old Win2k pilot shell targets also delegate
to the common build. `SDL1_ROOT` can supply an offline SDK with `include/SDL`,
`bin/SDL.dll`, and `COPYING`; otherwise native CMake downloads the verified SDK.

## Runtime selection

```text
wolf3d --backend auto
wolf3d --backend gdi
wolf3d --backend sdl1 --sdl-video directx
wolf3d --backend sdl1 --sdl-render opengl
wolf3d --backend sdl3
wolf3d --backend kms-fbdev
wolf3d --diag
```

`auto` prefers the compiled SDL host, then GDI or KMS/fbdev. A missing or invalid
SDL library permits automatic fallback. Explicit selection is strict. SDL video
initialization failures can trigger fallback; game-data errors, OPL failures and
later runtime errors cannot. Normal library fallback is quiet; `--diag` explains
unavailable choices. `--backend` works in the launcher config, with command-line
values taking precedence. `--help` lists compiled backends and the selected
host's options. Singleton DOS/RH9 builds accept `auto` or their one backend.

SDL1 and SDL3 functions are resolved at runtime. Windows loads the matching DLL
beside the executable with loader dialogs suppressed. Linux uses `libSDL3.so.0`
from its bundle/search path. Every required export is checked; partial loads are
rolled back. No SDL library is a mandatory executable import. Engine and any
compiled OPL libraries remain required. Linux KMS/fbdev retains libdrm and ALSA.

## Package identity and validation

Video backend names and `_modular` are absent from distribution folder names.
`DOCS/BUILD.TXT` records the compiled set and all build options. A subset rebuild
replaces the same package identity; stage directories are recreated to avoid
leftover SDL libraries or licenses. See [naming](BUILD-NAMING.md).

Win2k combined gameplay was confirmed on physical hardware before this matrix
expansion. Expanded combinations need their own destination testing: successful
compilation/import auditing is not a claim of physical hardware validation.
Developer tests cover runtime selection, config precedence, invalid/missing SDL,
strict explicit selection, and the existing audio/video worker behavior. Python
is only a maintainer test dependency, never a build dependency.

See [fullscreen options](running.md#fullscreen-modes-gdi-sdl1-and-sdl3),
[SDL1 rendering](windows-sdl1.md), and the [support matrix](support-matrix.md).

## Maintainer checks

`python tests/WG_VIDEO_BACKENDS_TEST.py PACKAGE --sdl sdl1` (or `sdl3`) exercises
real packaged DLL loading and fallback. `tests/WG_BACKEND_TEST.py` retains the
Win2k fault-injection fixture coverage. CTest includes
`tests/WG_SDL_MODAL_FRAME_TEST.c` to preserve engine progress in native SDL modal
loops, plus loader/export and host-lifecycle tests for MinGW SDL1 builds.
