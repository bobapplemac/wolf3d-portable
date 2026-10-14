# Native OPL on Windows 2000 and XP

This experimental x86 GDI option uses the real ISA OPL chip at ports
388h-38Bh through InpOut32. DBOPL remains the default; ordinary launches,
help and diagnostics do not load the access DLL unless native AdLib is selected.
Digital effects still use Windows waveOut. This is not WSS or SB DMA support.

## Build

On the Linux Docker build host:

```sh
make windows-win2k WIN2K_OPL_DRIVERS=dbopl,silent,adlib
make windows-xp OPL_DRIVERS=dbopl,silent,adlib OPL_DEFAULT=dbopl
```

For an x86 Windows GDI CMake build, set `W3P_ENABLE_INPOUT32=ON`.
This enables the library's `WG_ENABLE_OPL_ADLIB` adapter. Both are off by
default. Other cross profiles reject AdLib selection for now.

## Install and test

Download the official [InpOut32 1.5.0.1 binaries](https://vps.highrez.co.uk/downloads/InpOut32/default.htm).
Use its **Win32** folder: put `inpout32.dll` beside `wolf3d.exe` and run the
supplied `InstallDriver.exe` as Administrator on the destination machine.
Loading this DLL can itself install/start its embedded kernel driver and
requires Administrator rights for initial installation. The game does not
bundle or download the DLL or driver. Use a trusted upstream copy.

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
The first implementation played notes but exposed burst scheduling. The
sample-positioned playback revision requires another physical listening test.

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
modal loop. A window timer continues producing audio on the engine thread in
that loop; the worker continues delivering the queued native writes. This
avoids racing the engine mixer against gameplay state on another thread.
A blocked UI thread outside those modal loops can still exhaust the audio queue.

Maintainer tests (MinGW GCC, no native port-access DLL):

```text
gcc -std=c99 -Wall -Wextra -Werror tests/WG_OPL_TIMELINE_TEST.c -o build/opl-timeline-test.exe
gcc -std=c99 -Wall -Wextra -Werror tests/WG_OPL_PLAYBACK_TEST.c -lwinmm -o build/opl-playback-test.exe
gcc -std=c99 -Wall -Wextra -Werror tests/WG_MODAL_AUDIO_TEST.c -o build/modal-audio-test.exe
```

Run the three executables on Windows. They cover event spacing/order, timestamp
wrap and clock correction, underruns, queue limits, worker cancellation/restart,
and nested modal loops. Keep DOS/Win9x timing changes as a separate follow-up.
