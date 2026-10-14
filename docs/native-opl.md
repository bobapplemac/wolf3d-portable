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
on Windows. Hardware playback on win2k/XP remains pending physical testing.

For an integration check, cross-compile `tests/WG_INPOUT32_FAKE.c` with
`i686-w64-mingw32-gcc -shared -Wl,--kill-at` and pass that fake DLL to
`python tests/WG_INPOUT32_INTEGRATION_TEST.py PACKAGE FAKE_DLL` on Windows.
The fixture performs no port I/O and must never be distributed. This checks
real DLL loading/calling conventions and verifies that help, ordinary
diagnostics and DBOPL do not load the DLL. Never use the genuine driver DLL
with this test.
