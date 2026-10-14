# Native OPL on Windows 2000 and XP

This experimental x86 GDI option uses the real ISA OPL chip at ports
388h-38Bh through InpOut32. Auto is the default; ordinary launches,
help does not load the access DLL; diagnostics probes the selected policy.
Digital effects still use Windows waveOut. This is not WSS or SB DMA support.

## Build

On the Linux Docker build host:

```sh
make windows-win2k
make windows-xp
# Optional heavier emulator, retaining DBOPL as the runtime default:
make windows-win2k WIN2K_OPL_DRIVERS=nuked,dbopl,silent,adlib
make windows-xp WINXP_OPL_DRIVERS=nuked,dbopl,silent,adlib
```

For an x86 Windows GDI CMake build, set `W3P_ENABLE_INPOUT32=ON`.
This enables the library's `WG_ENABLE_OPL_ADLIB` adapter. The win2k and winxp
Docker profiles enable it by default, compiling DBOPL, silent and AdLib with
auto selected at runtime. Generic CMake builds keep this option off by default.
Other cross profiles reject AdLib selection for now. The guided Linux build
lets DOS, Win9x, win2k and winxp users add Nuked and select their runtime default.

## Install and test

Native-AdLib-enabled **win2k/winxp x86 GDI distributions include** the verified
Highrez `inpout32.dll` beside `wolf3d.exe`. Keep it there. The DLL embeds its
kernel drivers and can install/start the appropriate driver when loaded;
initial installation requires Administrator privileges on the destination
machine. No separate `InstallDriver.exe` or `.sys` download is required.
Explicit DBOPL and help do not load it. Auto diagnostics may probe it.

Other profiles do not bundle this DLL. For a custom supported x86 GDI build,
obtain the Win32 DLL from the official
[InpOut32 1.5.0.1 binaries](https://vps.highrez.co.uk/downloads/InpOut32/default.htm).

The repository vendors the unmodified binary and upstream MIT license in
`third_party/InpOut32`. Packaging verifies its pinned SHA-256 and fails on a
mismatch. `DOCS/BUILD.TXT` records the release, DLL file version and checksum;
`DOCS/NOTICES.TXT` and `DOCS/LICENSES/INPOUT32.TXT` preserve attribution and
license terms. Release 1.5.0.1 retains a 1.5.0.0 DLL version resource.
Builds neither download nor execute the access DLL. Maintainers can run
`tests/WG_INPOUT32_PACKAGE_TEST.py` to test inclusion and tamper rejection.

Start with:

```text
wolf3d.exe --no-config --diag --opl adlib
```

The Native OPL line distinguishes DLL load failure, missing exports, an
inaccessible driver and an unresponsive chip. A successful report says
OPL detected at 0x388. The probe resets the chip and then silences it.
Do not play another application's MIDI/FM audio while using this driver:
the generic port-access driver does not coordinate with the Windows sound driver.
The Yamaha card must already be initialized and its FM mixer route enabled.

Then run `wolf3d.exe --opl adlib` and enable AdLib music/effects in the game's
Sound menu. Saved CONFIG.* sound choices still apply. `--opl dbopl` returns
to software FM without loading InpOut32. Do not use the Win9x executable for
native FM on NT; use the win2k or XP package built with this option.

## Timing and validation

Each register update makes two synchronous port-write calls. The host waits
at least 4 microseconds after the address and 24 after the data using
QueryPerformanceCounter, rounding upward with an extra counter tick. The
initial detection timer has a separate 100-microsecond wait. Only actual
chip detection reads status; no dummy reads are used for delays.
The waits begin after each driver call returns, so driver overhead is never
assumed to satisfy a minimum hardware delay. These are conservative OPL2
recovery intervals even when the hardware is OPL3.

A missing performance counter prevents initialization. A failed, backward or
stuck counter during playback uses a conservative coarse-time fallback;
that fallback may stutter and is not a normal playback strategy. Very old
multiprocessor systems with unsynchronized counters need physical testing.
DOS/Win9x retain their tested direct-I/O delay loops pending separate review.

`tests/WG_INPOUT32_TEST.c` uses mocked Windows loading, timing and port I/O;
it never loads a real access driver. Compile it with MinGW GCC and run it
on Windows. Physical Windows 2000/Yamaha testing confirms access with Highrez
1.5.0.1; the original Logix4U DLL lacks the required IsInpOutDriverOpen export.
The first implementation played notes but exposed burst scheduling. Physical testing of the
sample-positioned playback revision confirms correct-sounding music/effects.

For an integration check, cross-compile `tests/WG_INPOUT32_FAKE.c` with
`i686-w64-mingw32-gcc -shared -Wl,--kill-at` and pass that fake DLL to
`python tests/WG_INPOUT32_INTEGRATION_TEST.py PACKAGE FAKE_DLL` on Windows.
The fixture performs no port I/O and must never be distributed. This checks
real DLL loading/calling conventions and verifies that help, ordinary
diagnostics and DBOPL do not load the DLL. Never use the genuine driver DLL
with this test.

## Playback scheduling and window dragging

Platform API v7 supplies a 64-bit PCM-frame timestamp for each native register
write. The Windows host preserves event order in a bounded queue and dispatches
it from a dedicated worker, separate from rendering. The worker follows waveOut's
sample position, interpolated with QueryPerformanceCounter between device updates.
If the device cannot report sample positions, the QPC timeline remains active.
It never advances beyond accepted PCM; an underrun rebases the timeline when
new samples arrive. Queue overflow or a failed clock stops audio with an error
rather than silently discarding register writes. Shutdown joins the worker and
cancels queued events before resetting the chip or closing waveOut.

The worker requests 1-ms timer resolution and uses short waits (not continuous
CPU spinning between notes). Windows is not a hard real-time operating system;
driver latency and scheduling can still introduce jitter. The 4/24-us chip
recovery waits remain separate from musical event timestamps.

Dragging/resizing a GDI window or opening its system menu enters a Windows
modal loop. A window timer continues simulation, demo playback, audio production and
framebuffer rendering on the engine thread in that loop; the worker continues
delivering native writes. GDI presentation is skipped during the drag/menu and
resumes with the latest frame when it ends. A visually frozen dragged window
therefore does not mean the game is paused. Explicit game pauses still apply.
A blocked UI thread outside those modal loops can still exhaust the audio queue.

Maintainer tests (MinGW GCC, no native port-access DLL):

```text
gcc -std=c99 -Wall -Wextra -Werror tests/WG_OPL_TIMELINE_TEST.c -o build/opl-timeline-test.exe
gcc -std=c99 -Wall -Wextra -Werror tests/WG_OPL_PLAYBACK_TEST.c -lwinmm -o build/opl-playback-test.exe
gcc -std=c99 -Wall -Wextra -Werror tests/WG_MODAL_FRAME_TEST.c -o build/modal-frame-test.exe
```

Run the three executables on Windows. They cover event spacing/order, timestamp
wrap and clock correction, underruns, queue limits, worker cancellation/restart,
and nested modal loops. Keep DOS/Win9x timing changes as a separate follow-up.
