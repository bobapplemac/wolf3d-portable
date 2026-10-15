# Windows SDL 1.2 and GDI comparison

The experimental Win2k x86 SDL1 target uses genuine SDL 1.2.15. It is built
with MinGW GCC/MSVCRT on Linux; it does not require a Windows build VM.
Both windib and directx video have run successfully on the Windows 2000 test machine.
DirectDraw hardware scaling has also been confirmed fast on the Win2k/Nvidia test machine.
The border-flicker fix is confirmed. Accelerated borderless and display fullscreen are confirmed on that machine. Win9x and WinXP SDL1
packages, including applicable Visual Studio builds on the XP/Win7 build hosts,
are follow-up work after Win2k validation. This is separate from the existing
RH9 SDL1 target and does not change its Linux runtime requirements.

For a single executable with runtime GDI/SDL1 selection, see the
[modular Win2k pilot](windows-backends.md).

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

The host defaults to `directx` (SDL's DirectDraw backend). If its initialization
or initial video-mode setup fails, the host retries with `windib` (Windows GDI).
This fallback also applies to explicit `--sdl-video directx`. Selecting `windib`
skips DirectDraw; failure of the selected/fallback GDI driver ends startup.
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
mouse capture or whether mouse input is enabled. F11/`--fullscreen` uses
borderless desktop-sized fullscreen on Windows, preserving the desktop resolution
and the normal-cooperative-level DirectDraw hardware path. Exclusive SDL fullscreen
previously conflicted with that path and could force slow software fallback.
`--fullscreen-mode display --fullscreen-resolution 640x480` temporarily switches
the primary display resolution but retains normal DirectDraw cooperation and
hardware stretching. It avoids SDL exclusive ownership; the desktop mode is
restored on focus loss, windowed return and exit. Without a resolution, display
mode uses the desktop dimensions. Both fullscreen modes have been confirmed accelerated on the Win2k/Nvidia test machine.
See [fullscreen options](running.md#fullscreen-modes-gdi-sdl1-and-sdl3) for the
shared GDI/SDL1/SDL3 interface.

## DirectDraw hardware scaling

With `--sdl-video directx`, the host tries a hardware-only DirectDraw device and
a 320x200 video-memory upload surface. The CPU converts only the original indexed
frame to the display's 16/24/32-bit RGB format. DirectDraw stretches that surface
to the 4:3 client rectangle, using a window clipper. No smoothing filter is
requested, but the display driver controls hardware stretch filtering; this path
does not guarantee nearest-neighbor output. Even integer window scales stretch
the original 320x200 frame to a 4:3 rectangle (for example, 640x480). Black borders
are cleared separately without blanking the game image. Successful presentation reports:

```text
SDL1 scaling: DirectDraw hardware stretch (320x200 upload)
```

If capability checks, video-memory allocation, surface locking or the blit fail,
the same frame falls back to SDL software presentation, with a reason printed.
Surface loss also selects fallback; a resize or mode change permits a new attempt.
The software path uses SDL's `SDL_SoftStretch`, not a custom scaling loop.
`windib` always uses this software path. Hardware scaling is the default;
`--sdl-scale hardware` explicitly selects the same hardware-with-fallback policy.
`--sdl-scale software` skips hardware scaling. Both options also work in the
launcher config file. The former `WOLF3D_SDL1_SCALE` variable is no longer used.

To compare with software scaling while keeping the DirectDraw video driver:

```bat
wolf3d.exe --sdl-video directx --sdl-scale software --window-scale 2 --opl adlib
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
formats, non-integer/letterbox/pillarbox rectangles, border-only clearing,
video-driver fallback and CLI selection, failed-blit/surface-loss fallback, blocked GDI
and SDL presentation, immutable frames and worker shutdown. The shared SDL tests
cover palette changes, software stretch, cursor/focus, Windows text routing,
joystick-disabled polling and borderless fullscreen flags/dimensions.
Windows SDK syntax checks, Win2k/Win9x PE import audits and RH9 tests also pass.
Physical testing confirmed a large performance improvement from hardware scaling;
filtering and acceleration still depend on the installed display driver.

## OpenGL presentation (Windows and legacy Linux)

`--sdl-render auto|opengl|surface` selects presentation independently of the
SDL video driver. It also works as an argument line in `wolf3d.ini`.

- `auto` (default): Windows retains the DirectDraw/windib behavior above.
  Legacy Linux tries OpenGL, then software surfaces if GL initialization fails.
- `opengl`: require OpenGL; initialization failure is an error. Windows uses
  SDL's windib driver to create the GL context, overriding `--sdl-video` and
  `SDL_VIDEODRIVER`. This does not mean GDI is drawing the game frames.
- `surface`: use the existing SDL surface path. On Windows, `--sdl-video` and
  `--sdl-scale` retain their existing meanings. `--sdl-scale` does not affect GL.

For the combined package, use:

```bat
wolf3d.exe --backend sdl1 --sdl-render opengl
wolf3d.exe --backend sdl1 --sdl-render opengl --diag
wolf3d.exe --backend sdl1 --sdl-render auto
```

GL uses the system driver, including software implementations when hardware
acceleration is unavailable. Startup and `--diag` print vendor, renderer,
version and a best-effort software-renderer classification; unknown names are
not proof of hardware acceleration. We do not bundle or substitute a GL driver.
The window may briefly appear during diagnostics while the renderer is probed.

Only OpenGL 1.1 functions are used, dynamically resolved through SDL: no shaders,
modern extensions, or mandatory GL DLL/shared-library imports. A 512x256 texture
stores the 320x200 RGB frame, with nearest-neighbor filtering and a centered 4:3
viewport. GPU or software GL scales it; the engine framebuffer remains unchanged.
Windows GL calls stay on the video thread, separate from engine/audio work.
Context recreation on resize/fullscreen changes reinitializes textures and state.

Automated pixel readback on Windows/NVIDIA hardware GL and the legacy Linux
executable with Mesa llvmpipe software GL confirms colors, orientation, black
borders, nearest filtering and resize/context recreation. The Linux software-GL
test runs on a modern kernel; it does not establish RH9 runtime compatibility. Win2k/FX 5900XT and
physical RH9 validation remain pending. To run the optional real-GL host test,
set `WG_TEST_GL=1` and run `wg-sdl1-dynamic-host-test.exe` (Windows) or
`wg-sdl1-test` (Linux) with a working display. The ordinary test uses SDL's dummy
video driver to cover surface fallback and PCM/input behavior without a display.
