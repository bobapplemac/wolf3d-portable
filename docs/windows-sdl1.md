# Windows SDL 1.2 and GDI comparison

The experimental Win2k x86 SDL1 target uses genuine SDL 1.2.15. It is built
with MinGW GCC/MSVCRT on Linux; it does not require a Windows build VM.
Both windib and directx video have run successfully on the Windows 2000 test machine.
DirectDraw hardware scaling has also been confirmed fast on the Win2k/Nvidia test machine.
The border-flicker fix is confirmed. Accelerated borderless and display fullscreen are confirmed on that machine. Win9x/XP and legacy Microsoft builds can now include SDL1 alongside GDI.
New target combinations still need destination gameplay confirmation. The RH9
SDL1 target retains its separate historical Linux runtime requirements.

See the [runtime backend contract](windows-backends.md) for shared build and
selection behavior.

## Build

```sh
make windows-win2k-sdl1 JOBS=8
```

The target builds the existing MinGW image, then a checksum-pinned SDL image.
`WIN2K_OPL_DRIVERS` and `WIN2K_OPL_DEFAULT` select the driver set/default, as
for GDI. Defaults are `dbopl,silent,adlib` and `auto`; silent is always included.
Nuked may be explicitly added. InpOut32 is bundled only when AdLib is included.
All CMake cache options and source revisions are recorded in `DOCS/BUILD.TXT`.
The distribution name is `wolf3d-portable_<version>_win2k_x86_mingw-gcc<major>-msvcrt`.
The common root CMake entry accepts `W3P_VIDEO_BACKENDS=gdi,sdl1` or `sdl1`.
`SDL1_ROOT` can supply headers, DLL and COPYING; an import library is unnecessary.
Native builds otherwise download the checksum-pinned official SDL 1.2 SDK.

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
are cleared separately without blanking the game image. `--diag` reports the selected path:

```text
Presentation: DirectDraw hardware stretch
```

If capability checks, video-memory allocation, surface locking or the blit fail,
the same frame falls back to SDL software presentation, with the reason available through `--diag`.
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
longer used. Audio/engine isolation remains; temporary timing-file instrumentation has been removed.

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
acceleration is unavailable. `--diag` prints vendor, renderer,
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
test runs on a modern kernel; it does not establish RH9 runtime compatibility. OpenGL has also been confirmed working on the Win2k test system;
physical RH9 validation remains pending. To run the optional real-GL host test,
set `WG_TEST_GL=1` and run `wg-sdl1-dynamic-host-test.exe` (Windows) or
`wg-sdl1-test` (Linux) with a working display. The ordinary test uses SDL's dummy
video driver to cover surface fallback and PCM/input behavior without a display.

## OpenGL implementation and acceleration selection

`--gl-acceleration auto|hardware|software` requires OpenGL and chooses an
acceleration policy. Auto lets SDL/the driver prefer hardware and accept
software fallback. Explicit hardware/software must match the resulting context,
otherwise startup fails. On Linux, explicit requests also set Mesa's
`LIBGL_ALWAYS_SOFTWARE` policy before SDL initializes; this is not a Mesa install.
On Windows, software may select Microsoft's built-in GDI Generic OpenGL 1.1.
Both Microsoft software GL and NVIDIA hardware GL pass the pixel/lifecycle tests.

`--gl-library FILE` requires OpenGL and loads a particular implementation using
SDL_GL_LoadLibrary. Use a full path to avoid ambiguous library searches. The
library and its dependencies must match the executable's architecture, OS and
CPU requirements. No Mesa library is bundled; current Mesa builds must not be
assumed compatible with Win2k or Pentium III. SDL1 does not enumerate every
installed vendor implementation; this is explicit selection of a known library.
Combining either option with `--sdl-render surface`/`auto` is rejected; use
`--sdl-render opengl` or omit that argument. All options work in the config file.

```bat
wolf3d.exe --backend sdl1 --gl-acceleration software
wolf3d.exe --backend sdl1 --gl-acceleration hardware --diag
wolf3d.exe --backend sdl1 --gl-library "C:\GL\opengl32.dll" --diag
```

Normal SDL startup, resize and fallback announcements are quiet. `--diag` groups
compiled video/audio capabilities, video runtime (including actual GL library,
renderer and acceleration), host hardware, audio runtime and game data.
Real errors remain visible. The shared Windows B800 renderer clears the console
buffer and attributes, resets the viewport, draws the card, and leaves a clean
cursor below it; it no longer guesses and redraws an existing shell prompt.

## PCM timing diagnostics

`WOLF3D.EXE --backend sdl1 --diag --no-pause` includes a two-second silent PCM
probe at 22050 Hz. It reports the actual SDL audio driver and negotiated format,
callback size, elapsed wall/SDL time, requested/consumed frames and underruns.
The requested frame rate should be approximately the negotiated sample rate;
short probes have callback-boundary and timer-rounding variation. This measures
device callback pacing without game rendering or OPL synthesis, so a healthy
probe does not rule out starvation during gameplay. Both emulated and native
OPL music follow the engine's PCM production schedule on Win9x.

For a comparison using SDL's waveOut driver, set `SDL_AUDIODRIVER=waveout` in
the command prompt before launching. The diagnostic driver line confirms the
selection. Clear the override afterward with `set SDL_AUDIODRIVER=`.

