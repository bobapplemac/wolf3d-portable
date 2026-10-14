# Red Hat Linux 9 / i586 SDL 1.2

This experimental target is intended for the original 2003 Red Hat Linux 9
(kernel 2.4.20, glibc 2.3.2), not Red Hat Enterprise Linux 9. The CPU baseline
is original Pentium/i586, without CMOV, MMX or SSE. Actual RH9 kernel, X11,
OSS audio and physical Pentium playback remain pending confirmation.

## Build

On a modern x64 Linux Docker host:

```sh
make rh9-sdl1 JOBS=8
# Optional heavier emulator, still with automatic runtime selection:
make rh9-sdl1 RH9_NUKED=ON
# Silent-only build:
make rh9-sdl1 RH9_DBOPL=OFF
```

The guided `./build.sh` path is Linux distributions (Docker), then Red Hat 9
i586 SDL 1.2. It offers DBOPL/silent by default and optional Nuked, with auto
as the default selection. Native OPL is deliberately deferred for this host.
The legacy engine policy prefers DBOPL over Nuked when both are present;
auto PCM defaults remain driver-dependent (DBOPL 22050 Hz, Nuked 44100 Hz).
`--sample-rate` is the runtime override. The engine source/API is unchanged.

The Dockerfile uses modern host tools and GCC 8, but compiles and links against
a separate historical sysroot. Original i386 RH9 glibc, kernel headers and
XFree86 RPMs plus SDL 1.2.15 source are downloaded from upstream archives and
checked against `packaging/linux-rh9/SHA256SUMS`. Do not replace these with host
headers or libraries. GNU89 inline semantics are needed by the old glibc headers.
SDL uses its old joystick interface; newer input-event structs are not assumed.

This is genuine SDL 1.2.15, built with software X11 and OSS support. It is not
sdl12-compat, which would bring SDL2 back into the dependency chain. ALSA,
PulseAudio, OpenGL, framebuffer-console and native OPL are not part of this
initial profile. SDL's timer/input/audio services isolate most host code so a
future legacy Windows implementation can reuse it, but Windows is not supported
by this target yet.

## Package and naming

The portable-only backend label is `sdl1`; architecture remains `x86`:

```text
wolf3d-portable_<engine-version>_linux-glibc_x86_sdl1_gcc8/
```

This extends the [fixed naming order](BUILD-NAMING.md). RH9's glibc/kernel floor
and i586 CPU requirement are recorded in `DOCS/BUILD.TXT`, along with the complete
CMake cache, source revisions and pinned dependency hashes. The executable is
`wolf3d`; `libwolf3d.so.1` and the replaceable `libSDL-1.2.so.0` stay beside it.
Optional Nuked remains a separate shared library. Runtime glibc/X11 come from
the destination OS and are not bundled. SDL's LGPL license, copyright/source
notice and build recipe are included/referenced in DOCS alongside engine notices.

## Run on Red Hat 9

Start from a terminal in a working X11 session:

```sh
./wolf3d --no-config --diag --opl dbopl
./wolf3d --data /path/to/game --window-scale 1
```

`--window-scale 1` starts at 320x240; default 2 gives 640x480. Resizing preserves
4:3 with letterboxing. F11 toggles SDL fullscreen, which can switch video modes.
Keyboard, captured mouse and startup-detected SDL joysticks are supported.
Exit text is printed to the launching terminal. Game data is not included.

PCM uses the system OSS device, normally `/dev/dsp`. The card must be initialized
by the OS and its mixer enabled. `SDL_AUDIODRIVER=dsp` selects OSS explicitly;
other programs may need to release the device. Lower `--sample-rate` if needed.
The audio thread only drains a bounded PCM queue; all engine calls stay on the
main thread. Queue underruns produce silence rather than replaying stale data.

## Validation and limitations

The build runs `tests/WG_SDL1_TEST.c` against SDL dummy video/audio, checking
input mapping, focus releases, 4:3 presentation, audio ordering/wrap/underrun,
queue overflow and audio restart. Help and the public API consumer also run.
Tests use the RH9 loader and libraries inside Docker, but still the modern host
kernel. This does **not** prove operation on Linux 2.4.

`tools/W3P_RH9_AUDIT.sh` checks ELF32, the old loader, allowed dependencies,
maximum GLIBC_2.3.2, and absence of CMOV/MMX/SSE instructions in bundled code.
`tests/WG_RH9_AUDIT_TEST.py` exercises its rejection paths. The source build
must retain `-march=pentium`; instruction scans supplement compiler settings.

A booted RH9 VM is useful for real old-kernel testing. Check help, diagnostics,
SDL/X11 presentation, PCM, configuration precedence, resizing, fullscreen and
shutdown. Repeat gameplay/music/digitized effects on the physical Pentium III.
An emulated or physical original Pentium is needed before marking i586 runtime
confirmed. VM SSH access is useful for diagnostics, not necessary for builds.

Upstream baselines: [Red Hat's original release notes](https://legacy.redhat.com/pub/redhat/linux/9/en/os/i386/RELEASE-NOTES.html),
[RH9 RPM archive](https://legacy.redhat.com/pub/redhat/linux/9/en/os/i386/RedHat/RPMS/),
[SDL 1.2 source](https://www.libsdl.org/release/SDL-1.2.15.tar.gz).
