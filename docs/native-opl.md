# Native OPL on Linux and Windows

## Linux x86/x64: SDL3 and KMS/fbdev

**Status: implemented, build- and mock-tested; physical Linux OPL playback is
pending confirmation.** The chip probe and 4/24 microsecond write delays follow
our working DOS/Windows implementation. That gives a sound basis for expecting
compatible ISA hardware to work, but does not establish real-card timing,
FM/PCM synchronization, mixer routing, or distribution compatibility.

Both Linux hosts share a direct port-I/O worker for the OPL chip at 0x388-0x38b.
The KMS/fbdev host keeps ALSA PCM; SDL3 keeps its selected audio backend (ALSA,
PulseAudio, PipeWire, etc.). Native FM does not replace or bypass PCM. To select
ALSA specifically for SDL3, set `SDL_AUDIO_DRIVER=alsa` before launching.
The sound card mixes FM with PCM; software capture of PCM alone will not capture FM.

Linux x86/x64 builds include AdLib by default. `auto` tries AdLib, Nuked, DBOPL,
then silent; permission or detection failure falls back quietly. Explicit
`--opl adlib` fails with a diagnostic instead. Native OPL prefers 22050 Hz PCM;
`--sample-rate` still overrides that preference without changing FM tempo/pitch.
Non-x86 Linux does not provide this direct ISA backend. Docker builds do not need
hardware access or elevated I/O privileges; those are runtime requirements only.

### Hardware and permissions

- Initialize the card using its normal Linux sound driver and enable/unmute the
  FM mixer route. Yamaha OPL3-SA2/SA3 cards use `snd-opl3sa2`; their ISA PnP,
  port/IRQ/DMA and mixer configuration is separate from this backend.
- Leave ALSA PCM and mixer support enabled. Close other FM players and avoid
  using ALSA's OPL sequencer at the same time. Direct access does not acquire
  exclusive ownership from the kernel FM driver or other applications.
- The playback thread requests `ioperm(0x388, 4, 1)`. It needs `CAP_SYS_RAWIO`;
  kernel lockdown/security policy can still deny access. The game never changes
  privileges or permissions itself. Only the worker receives port permission,
  and it silences the chip and revokes permission when stopped.

For an administrator-controlled hardware test, util-linux `setpriv` can launch
under your normal UID with the required ambient capability, avoiding running the
whole game as root (this is still a powerful capability; use trusted binaries
and libraries):

```sh
sudo setpriv --reuid="$(id -u)" --regid="$(id -g)" --init-groups \
  --inh-caps=+sys_rawio --ambient-caps=+sys_rawio \
  ./wolf3d --no-config --diag --opl adlib
```

Use the same launcher without `--diag` for playback. Ambient capabilities require
Linux 4.3 or newer; distribution policy may restrict them. Bundled shared-library
builds should not simply be given file capabilities: secure-loader rules can
change library lookup. No permission setup is needed for software emulation.

The probe resets the OPL chip. Success should select `adlib` at the requested or
negotiated PCM rate. Then test music, AdLib effects and digitized effects together,
including window dragging, busy scenes, normal exit and restarting. Do not label
physical support confirmed until this has been checked on the target machine.

### Implementation and regression checks

The worker performs chip I/O; a monotonic clock enforces elapsed-time register
recovery waits. Timestamped musical writes are released only after corresponding
PCM is accepted. PCM queue observations correct the playback timeline; hardware
and sound-server buffering can introduce additional latency, requiring listening
tests. Shutdown joins the worker before resetting PCM. No InpOut32 DLL or custom
kernel module is used.

`linux-native-opl` in CTest exercises mocked port access with the real pthread
worker: denied permission, absent chip, OPL2/OPL3 reset, minimum delays, PCM-gated
writes, invalid rate handling, restart, silence and permission cleanup. The shared
`WG_OPL_TIMELINE_TEST.c` also covers drift, starvation and queue boundaries.

Build with `make releases`; add `OPL_DRIVERS=nuked,dbopl,silent` to omit native
OPL. Direct CMake builds use `-DWG_ENABLE_OPL_ADLIB=OFF` to omit it. Existing caches
retain explicit driver choices. glibc and musl use the same backend. Validation covers full x64 builds of both
hosts on glibc 2.28 and musl, plus full i686 glibc distributions and mocked
worker tests. Physical Linux playback remains pending. See building.md for
the i686 Docker profile and its CPU/runtime limits.

## Red Hat Linux 9 / i586 SDL 1.2

The [RH9 profile](linux-rh9.md#native-adlib--opl) also includes native AdLib by
default, keeping SDL/OSS for PCM. It uses the shared port worker with a separate
legacy timing path; modern clock APIs and ambient capabilities are unavailable.
Auto prefers AdLib, then DBOPL, Nuked and silent. Built and mock-tested; physical
RH9 timing, audio and port access remain pending confirmation. See the linked
guide for permissions, clock limitations and the hardware test procedure.

## Windows 2000 and XP

This experimental x86 GDI option uses the real ISA OPL chip at ports
388h-38Bh through InpOut32. Auto is the default; ordinary launches may probe the chip. Help does not load
the access DLL; diagnostics probes the selected policy.
Digital effects still use Windows waveOut. This is not WSS or SB DMA support.

## Build

On the Linux Docker build host:

```sh
make windows-win2k
make windows-xp
# Optional heavier emulator, retaining auto as the runtime default:
make windows-win2k WIN2K_OPL_DRIVERS=nuked,dbopl,silent,adlib
make windows-xp WINXP_OPL_DRIVERS=nuked,dbopl,silent,adlib
```

For a Windows x86/x64 GDI or SDL3 CMake build, set `W3P_ENABLE_INPOUT32=ON`.
This enables the library's `WG_ENABLE_OPL_ADLIB` adapter. The win2k and winxp
Docker profiles enable it by default, compiling DBOPL, silent and AdLib with
auto selected at runtime. Generic CMake builds keep this option off by default.
Modern Windows profiles omit native AdLib by default. Enable it with
`make windows-win7 OPL_DRIVERS=nuked,dbopl,silent,adlib` (also supported by
`windows-llvm-win7` and `windows-win10`), or Windows
`build.ps1 -Drivers all-adlib -DefaultOpl auto`. The guided PowerShell and
Linux menus expose these choices. `build.cmd` remains the VC6--VS2005 entry point. Silent is
always included; explicitly selected unavailable drivers fail rather than fall back.

## Install and test

Native-AdLib-enabled **win2k/winxp x86 GDI distributions include** the verified
Highrez `inpout32.dll` beside `wolf3d.exe`. Keep it there. The DLL embeds its
kernel drivers and can install/start the appropriate driver when loaded;
initial installation requires Administrator privileges on the destination
machine. No separate `InstallDriver.exe` or `.sys` download is required.
Explicit DBOPL and help do not load it. Auto diagnostics may probe it.

Other profiles do not bundle an access DLL or its kernel driver. Supply
`inpout32.dll` for an **x86 application** or `inpoutx64.dll` for an **x64 application**,
including when the x86 application runs on a 64-bit OS. Obtain the matching DLL
from the official
[InpOut32 1.5.0.1 binaries](https://vps.highrez.co.uk/downloads/InpOut32/default.htm).

Modern Windows driver-signing and security policies can prevent InpOut from
loading. The wrapper reports that failure for explicit `--opl adlib`; `auto`
quietly falls back. Native ISA OPL on modern Windows/x64 is implemented and
mock-tested, but is not physically confirmed. Do not assume a PCI sound card
exposes a compatible OPL chip simply because Windows audio works.

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
it from a dedicated worker, separate from rendering. GDI follows waveOut's
sample position; SDL3 observes accepted PCM and its remaining stream queue.
Both interpolate with QueryPerformanceCounter between device updates.
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

## Windows native playback timing investigation

For a sustained-tempo comparison, use the same explicit sample rate for AdLib
and DBOPL. The engine uses the same 700 Hz music clock for both. Chip recovery
waits are separate from timestamp scheduling; they should not add to song time
unless the playback worker cannot keep up.

To collect an opt-in Win2k/XP native timing report, launch from Command Prompt
in a writable package directory:

```bat
set WOLF3D_OPL_TIMING=1
wolf3d.exe --opl adlib --sample-rate 22050
set WOLF3D_OPL_TIMING=
```

Leave the same menu song playing for about a minute, then quit normally.
`OPLTIME.TXT` is appended after each native playback session stops. It records
elapsed counter time, submitted PCM, last reported device position, late events,
port-write/query workload and starvation counts. Counters stay in memory during
playback; tracing adds counter-query overhead and is disabled unless requested.
A position slightly behind elapsed time can be ordinary buffering/report latency;
these figures are diagnostic evidence, not an automatic tempo correction.

The Windows worker polls device position at most every 20 ms (50 queries/second),
while QPC interpolation and event dispatch continue between reports. Timing logs
also report total and longest **estimated** PCM starvation durations for gaps
ending at successful refills. These derive from the current timeline anchor;
they are not hardware underrun measurements and exclude any unrefilled tail at
shutdown. A zero-duration restart can occur exactly at the published queue end.

For GDI scaling comparisons, repeat with `--windowed --window-scale 1`, `2`,
and `3`, keeping the song, sample rate and duration the same. Save each run's
`OPLTIME.TXT` separately. New reports include destination pixel dimensions,
presentation count/total/average/maximum and separate `StretchDIBits` API time.
Presentation time includes palette expansion and the surrounding GDI calls;
no forced GDI flush is added, so it is not a measurement of GPU completion.

The OPL worker also samples all waveOut header flags independently of the music
timeline, and submission checks them before refilling. Once playback has started,
no header still in flight means the driver queue was observed empty. Each transition
is counted once, with the observed empty duration until refill and the longest gap.
Durations are sampled lower bounds, not exact hardware-silence measurements;
short gaps between observations can be missed. An unrefilled shutdown tail is
marked `open` and excluded from completed durations. These counters distinguish
observed driver-queue exhaustion from the earlier timeline-derived estimates.
