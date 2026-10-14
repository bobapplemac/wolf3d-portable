# Windows SDL 1.2 and GDI comparison

The experimental Win2k x86 SDL1 target uses genuine SDL 1.2.15. It is built
with MinGW GCC/MSVCRT on Linux; it does not require a Windows build VM.
Both windib and directx video have run successfully on the Windows 2000 test machine.
Full validation, including the latest exit-card and cursor fixes, remains pending. Win9x and WinXP SDL1
packages, including applicable Visual Studio builds on the XP/Win7 build hosts,
are follow-up work after Win2k validation. This is separate from the existing
RH9 SDL1 target and does not change its Linux runtime requirements.

## Build

```sh
make windows-win2k-sdl1 JOBS=8
```

The target builds the existing MinGW image, then a checksum-pinned SDL image.
`WIN2K_OPL_DRIVERS` and `WIN2K_OPL_DEFAULT` select the driver set/default, as
for GDI. Defaults are `dbopl,silent,adlib` and `auto`; silent is always included.
Nuked may be explicitly added. InpOut32 is bundled only when AdLib is included.
All CMake cache options and source revisions are recorded in `DOCS/BUILD.TXT`.
The distribution name is `wolf3d-portable_<version>_win2k_x86_sdl1_mingw-gcc<major>-msvcrt`.
The dedicated CMake entry is `cmake/windows-sdl1`; `SDL1_ROOT` points to the SDL
headers, import library, DLL and COPYING file. Other toolchain packaging is not
yet supported; the initial release identity is specifically Win2k/x86/MinGW.

## Compare video drivers

```bat
wolf3d.exe --sdl-video windib --window-scale 2 --opl adlib
wolf3d.exe --sdl-video directx --window-scale 2 --opl adlib
```

`windib` uses Windows GDI and is SDL 1.2's default. `directx` uses SDL's DirectDraw
backend. Driver selection is explicit; an unavailable driver fails startup.
`SDL_VIDEODRIVER` also works; the CLI overrides it. Config-file argument lines
accept the same option. Keep the OPL choice and sample rate identical when
comparing performance. DirectDraw performance depends on the installed display
driver; it is not guaranteed to beat GDI on every machine.

The engine runs independently of the SDL video/input thread. Presentation uses
a copied, replaceable pending frame; dragging/resizing or a slow blit cannot
block engine simulation or PCM submission. Input waits for the window thread
while the OS is running its modal move/resize loop. Presentation is uncapped;
CPU percentage alone does not compare SDL and GDI frame rates.
The cursor is hidden whenever the fullscreen window has focus, independently of
mouse capture or whether mouse input is enabled. F11/`--fullscreen` uses SDL's
display-mode fullscreen, not the GDI host's borderless fullscreen.

## DirectDraw hardware scaling

With `--sdl-video directx`, the host tries a hardware-only DirectDraw device and
a 320x200 video-memory upload surface. The CPU converts only the original indexed
frame to the display's 16/24/32-bit RGB format. DirectDraw stretches that surface
to the 4:3 client rectangle, using a window clipper. No smoothing filter is
requested. Successful presentation reports:

```text
SDL1 scaling: DirectDraw hardware stretch (320x200 upload)
```

If capability checks, video-memory allocation, surface locking or the blit fail,
the same frame falls back to SDL software presentation, with a reason printed.
Surface loss also selects fallback; a resize or mode change permits a new attempt.
The software path uses SDL's `SDL_SoftStretch`, not a custom scaling loop.
`windib` always uses this software path. Actual hardware stretch behavior on the
Win2k/Nvidia test system remains pending physical confirmation.

To compare with software scaling while keeping the DirectDraw video driver:

```bat
set WOLF3D_SDL1_SCALE=software
wolf3d.exe --sdl-video directx --window-scale 2 --opl adlib
set WOLF3D_SDL1_SCALE=
```

Console output and the text exit card use the existing Windows text helpers.
Double-click launch pauses at exit; shell launch does not. `--no-pause` disables
the pause. SDL.dll remains a separate, replaceable LGPL library; its license,
source URL/checksum and build recipe are included in the distribution notices.

## GDI presentation

All Windows GDI targets share the asynchronous presenter, not only Win2k.
Scaling is delegated to `StretchDIBits` with `COLORONCOLOR`; the worker submits
the original 320x200 indexed framebuffer and palette. Custom software scaling
and the experimental DIB/DDB comparison paths were removed because testing had
not demonstrated a benefit. `WOLF3D_GDI_BITMAP` and `WOLF3D_GDI_FORMAT` are no
longer used. Audio/engine isolation and `WOLF3D_OPL_TIMING=1` timing logs remain.

## Validation

Maintainer tests cover native-size DirectDraw uploads, pitch, 16/24/32-bit pixel
formats, destination rectangles, failed-blit/surface-loss fallback, blocked GDI
and SDL presentation, immutable frames and worker shutdown. The shared SDL tests
cover palette changes, software stretch, cursor/focus and Windows text routing.
Windows SDK syntax checks, Win2k/Win9x PE import audits and RH9 tests also pass.
Real hardware is still needed to confirm acceleration and measure performance.
