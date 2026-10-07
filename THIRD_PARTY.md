# Third-party software

## Nuked-OPL3

- Upstream: `nukeykt/Nuked-OPL3`
- Vendored revision: `765ec962e473aeb767e4cba74ffdc8f588ffbfe8`
- License: LGPL-2.1-or-later
- Location: `lib/wolf3d/third_party/Nuked-OPL3`

The official implementation is the project's OPL reference and default
synthesizer. It remains an independently replaceable shared library
(`Nuked-OPL3.dll` on Windows and the corresponding shared object elsewhere).
Binary distributors must satisfy the LGPL's applicable notice, source,
modification, and relinking requirements; consult the included license when
preparing a release.

wolf3d-lib also contains an optional C DBOPL implementation derived through
PrBoom+ from DOSBox. The portable wrappers continue to select Nuked-OPL3 by
default; see `lib/wolf3d/third_party/DBOPL/README.wolf3d-lib.md` for its exact
provenance and GPL-2.0-or-later terms. No MAME OPL implementation is included.

## SDL3

- Upstream: `libsdl-org/SDL`
- Vendored release: `3.4.16`
- License: Zlib
- Location: `third_party/SDL3` (Git submodule)

SDL3 is an optional host-layer dependency used only by the portable GUI
wrapper. It remains a separate shared library in staged SDL3 packages. The
native Win32 and Linux direct-console targets do not require it. Configure
with `W3P_USE_SYSTEM_SDL3=ON` to use an installed
SDL 3.2-or-newer package instead of the pinned submodule. SDL release packages
include the zlib notice; system-SDL packaging uses the vendored copy at
`packaging/COPYING.SDL3.txt` so the source submodule need not be initialized.
Staged packages name the notice `LICENSES/SDL3-Zlib.txt`.

## musl libc

The optional relocatable Linux SDL3 package redistributes the musl 1.2.5
runtime loader/libc from Alpine Linux 3.20.10. musl is distributed under the
MIT license with additional permissive notices; its notice is included as
`packaging/COPYING.musl.txt` and copied into that distribution as
`LICENSES/musl-MIT.txt`.

### Bundled musl desktop clients

The relocatable musl package also carries the Alpine 3.20 runtime objects
needed by SDL's dynamically loaded X11, Wayland, eudev, ALSA, and PulseAudio
paths, including their ELF dependency closure. These are unmodified shared
libraries and remain independently replaceable. The principal upstream
projects and licenses are X.Org libraries (MIT/X11), Wayland (MIT), xkbcommon
(MIT), eudev (LGPL-2.1-or-later), ALSA lib (LGPL-2.1-or-later), PulseAudio
(LGPL-2.1-or-later), and PulseAudio's codec/runtime dependencies under their
respective permissive or LGPL licenses. The package includes the LGPL 2.1
license as `LICENSES/LGPL-2.1.txt`; exact pinned binary provenance is the
digest-pinned Alpine 3.20.10 builder in `packaging/linux-musl/Dockerfile`.

## Wayland 1.18 build toolchain

The optional Debian 10 portable-release container downloads the official
Wayland 1.18.0 source archive by pinned SHA-256 and builds its scanner and
development files under an isolated prefix. This supplies SDL's minimum
native-Wayland build interface without raising the Debian 10 glibc baseline.
Wayland uses the MIT license. The glibc portable package dynamically uses the
destination system's Wayland runtime. The relocatable musl package instead
ships the matching musl-built Wayland client objects as described above.

## DOS/32A

- Upstream: `dos32a.sourceforge.net`
- Bundled component: DOS/32A 9.1.2 from the pinned Open Watcom v2 toolchain
- License: permissive DOS/32A license; see `LICENSES/DOS32A.txt` in
  the DOS distribution

The 32-bit DOS package uses DOS/32 Advanced DOS Extender technology. The file
staged as `DOS4GW.EXE` is DOS/32A's compatible drop-in loader, not the original
proprietary DOS/4GW binary. It remains a separate, replaceable executable and
is distributed with its required license text.
