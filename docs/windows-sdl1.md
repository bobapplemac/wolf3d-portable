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
while the OS is running its modal move/resize loop. Video uses nearest-neighbor
scaling to a 4:3 rectangle, with cached horizontal maps and repeated-row copies.
Presentation is uncapped; higher frame throughput can still use all available CPU.
CPU percentage alone does not compare the frame rates of SDL and GDI.
The cursor is hidden whenever the fullscreen window has focus, independently of
mouse capture or whether mouse input is enabled. F11/`--fullscreen` uses SDL's display-mode fullscreen,
not the GDI host's borderless fullscreen. `--windowed` is the default.

Console output and the text exit card use the existing Windows text helpers.
Double-click launch pauses at exit; shell launch does not. `--no-pause` disables
the pause. SDL.dll remains a separate, replaceable LGPL library; its license,
source URL/checksum and build recipe are included in the distribution notices.

## GDI comparison switches

All Windows GDI targets share the asynchronous presenter, not only Win2k.
The default path scales into a DIB and issues an unscaled BitBlt. Recognized
16-bit desktop formats use RGB555/RGB565; other desktops use 32-bit pixels.
Horizontal sampling maps are cached and borders are cleared on geometry changes.
No identical-frame detection or interpolation is performed.

For hardware comparison from Command Prompt:

```bat
set WOLF3D_OPL_TIMING=1
set WOLF3D_GDI_BITMAP=ddb
wolf3d.exe --window-scale 2 --opl adlib
set WOLF3D_GDI_BITMAP=
```

`ddb` uploads the DIB into a device-dependent compatible bitmap before BitBlt.
It is opt-in because the upload may cost more than it saves. Clearing the variable
restores the default DIB path. `WOLF3D_GDI_FORMAT=32` forces 32-bit source pixels
for comparison on a 16-bit desktop; clearing it restores automatic selection.
`OPLTIME.TXT` records path, pixel depth, worker timing and replaced pending frames.
Native OPL timing logging remains available with SDL1, but GDI-specific timings
and waveOut header exhaustion counters apply only to the GDI host.

## Validation

Maintainer checks cover 16/32-bit nearest-neighbor pixels and stride guards,
blocked GDI and SDL presentations, immutable in-flight frames, pending-frame
replacement, and worker shutdown. Windows SDK syntax checks and the Win2k PE
import audit remain required. Red Hat 9 SDL1 tests cover the shared host's
Linux path. Physical Win2k DirectDraw, windib and native OPL comparison is still
required before declaring this backend supported.
