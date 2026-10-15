# Running wolf3d-portable

## Start the game

Copy one original game installation beside the selected executable, or pass
its location explicitly:

```text
wolf3d --data /path/to/WL1
wolf3d --data /path/to/WL6 --game WL6
```

Without `--data`, every launcher recursively searches its own directory for
complete supported data sets. They may therefore be kept in layouts such as
`GAMEDATA/WL1`, `GAMEDATA/WL6`, `GAMEDATA/SDM`, and `GAMEDATA/SOD` instead of
beside the executable. Symbolic links, junctions, and other reparse points are
not followed. If more than one directory contains the selected extension, the
launcher lists the conflicting paths and requires an explicit `--data PATH`.

## Launcher configuration

Optional launcher defaults can be stored beside the user-facing executable.
Windows and DOS use `.ini`; Linux uses `.conf`. The exact executable name is
checked first (`wolf3d.ini` on Windows/DOS or `wolf3d.conf` on Linux). For a development binary
or a renamed executable, it is followed by a wrapper-neutral name with a
known `-sdl3`, `-win32`, or `-console` suffix removed (`wolf3d.ini`). Only the
first existing file is loaded. Put one command-line option on each line:

```ini
# wolf3d launcher defaults
--fullscreen
--mouse
--game WL6
--opl dbopl
--data "Game Data/Wolf3D"
```

Blank lines and lines beginning with `#` or `;` are ignored. Quoted values are
supported, and relative data/device paths are resolved from the config file's
directory. Real command-line options override the corresponding config option
family. Use `--config FILE` to select another file or `--no-config` to bypass
configuration entirely. Config files are never created automatically and are
unrelated to the original game's `CONFIG.WL1`, `CONFIG.WL6`, and similar files.

## Fullscreen modes (GDI, SDL1 and SDL3)

`--fullscreen` starts fullscreen; `--windowed` starts windowed. F11 toggles
between windowed and the selected fullscreen mode. Fullscreen windows stay
above other windows while focused, releasing that priority on focus loss:

- `--fullscreen-mode borderless`: cover the desktop without changing its
  resolution (the default).
- `--fullscreen-mode display`: use a display mode. Add
  `--fullscreen-resolution 640x480` to request a specific resolution; otherwise
  use the current desktop resolution. Unsupported resolutions fail explicitly.

For compatibility, specifying a resolution without a mode implies `display`.
An explicit `borderless` mode ignores the resolution setting regardless of
argument order. These options also work in launcher config files; selecting a
mode does not itself start fullscreen.

GDI and Windows SDL1 use a borderless window with a temporary display-mode
change. Windows SDL1 retains its normal-cooperative-level DirectDraw hardware
scaler; it does not enter SDL's conflicting exclusive DirectDraw mode. SDL3 uses
its exclusive fullscreen API for `display`; Linux SDL1 uses SDL fullscreen.
Desktop modes are restored when leaving display fullscreen or shutting down.
Windows SDL1/GDI also restore the mode and minimize on focus loss, then reapply
it on activation. Physical validation of SDL1 display-mode acceleration is pending.

```text
wolf3d.exe --fullscreen --fullscreen-mode display --fullscreen-resolution 640x480
wolf3d.exe --fullscreen --fullscreen-mode borderless
```

## Windows PCM audio output

`--audio-output auto|sdl|waveout` chooses PCM output independently of Windows
video. It also works in `wolf3d.ini`; command-line values override the config.
It does not select an OPL synthesizer (`--opl` does that).

| Choice | With SDL1 or SDL3 video | With GDI video |
| --- | --- | --- |
| `auto` (default) | Try SDL audio; use shared waveOut if SDL PCM initialization fails | Use shared waveOut |
| `sdl` | Require SDL audio; no waveOut fallback | Error: unavailable with GDI |
| `waveout` | Require the shared GDI waveOut implementation | Require shared waveOut |

Auto fallback is initialization-only: it does not benchmark playback or switch
outputs during a game. Explicit choices never fall back. SDL1 normally selects
DirectSound on Windows, but `SDL_AUDIODRIVER` can override that. SDL3 selects its
own platform driver (often WASAPI on modern Windows); `sdl` does not specifically
mean DirectSound. `--diag` shows requested/selected outputs and the actual driver.

Shared waveOut uses four 512-frame buffers: about 93 ms of queued PCM at
22050 Hz, excluding device/OS latency. This is distinct from SDL1's internal
waveOut driver, which may use quarter-second callbacks and add much more latency.
It works with SDL rendering without using SDL's audio subsystem. Native AdLib
still uses the OPL chip; Windows SDL hosts schedule writes against the selected
PCM output. No additional DLL is required for shared waveOut.

```text
WOLF3D.EXE --backend sdl1 --audio-output waveout --opl adlib
wolf3d.exe --backend sdl3 --audio-output auto
```

On the Win98/Yamaha OPL3-SAx test machine, setting the system DirectSound
acceleration slider to None caused slow SDL1 playback; restoring Full fixed it.
If another game needs None, explicitly select shared waveOut here. Auto does
not detect this performance problem. The new shared SDL/waveOut combination
still needs confirmation on that physical machine.

Linux SDL hosts support `auto`/`sdl`; Windows waveOut is unavailable there.

## Diagnostics and saves

Run any launcher with `--diag` to print the loaded config, effective arguments,
all discovered game-data sets, the automatic selection, compiled OPL drivers,
and the host's available video, audio, mouse, and joystick/controller devices.
The report exits without starting the game.

The original engine configuration and save files (`CONFIG.<EXT>` and
`SAVEGAMn.<EXT>`) are stored in the selected game-data directory. This keeps
each nested WL1, WL6, SDM, SOD, or mission-pack installation self-contained.

Every launcher accepts `--help`, `-h`, or `/?` and exits after listing the
generic game options, the current host's options, and its compiled OPL
drivers. The older `--sdl3-help` and `--linux-console-help` spellings remain
available as compatibility aliases.

## Linux console

The Linux console host needs no X11 or Wayland. Its default `--video auto`
mode prefers a connected DRM/KMS display and falls back to `$FRAMEBUFFER` or
`/dev/fb0`. Use `--video drm` or `--video fbdev` to require one backend, with
`--drm-device PATH` or `--fb-device PATH` to select a device explicitly.

## DOS

For DOS, copy a data set into the distribution directory and run:

```text
WOLF3D.EXE --game WL1
WOLF3D.EXE --game WL1 --opl dbopl
```

Run `WOLF3D.EXE --diag` to probe the DSP and print the selected PCM format,
actual rate, IRQ and DMA, or the reason for silent fallback. The probe briefly
starts a silent DMA buffer and shuts it down. It uses a 22050 Hz request.

Initialize ISA/PnP cards using their DOS utility before launching. The driver
uses the actual `BLASTER` settings and DSP version:

- DSP 4.x with `H5`, `H6`, or `H7`: 16-bit stereo, up to 44100 Hz.
- DSP 2.x/3.x, or DSP 4.x without `H`: 8-bit mono using `D0`, `D1`, or `D3`,
  up to 22050 Hz. The DSP timer's rounded rate is used by the engine.
- DSP 1.x, WSS output and SB Pro stereo are not implemented. DSP 2.x/3.x
  playback requires a dedicated IRQ; shared-IRQ source detection is unavailable.

For example, `A220 I5 D1 T4` selects the 8-bit path when the DSP supports it.
A Yamaha card's 16-bit WSS interface is separate from its SB Pro interface;
do not invent an `H` setting to request SB16 playback. Native AdLib FM is
independent of PCM bit depth. If PCM initialization fails, native FM can still
play using a timed silent sink, but digitized effects and software OPL are silent.

## Game selection and input

Executable names beginning with `wolf` prefer `WL6`, then `WL1`; the exact
basename `spear` prefers `SOD`, then `SDM`. Either name falls back to the other
family if its preferred family is absent. `SD1`, `SD2`, and `SD3` are never
auto-selected. Use `--game EXT` or its short form (`-WL1`, `-WL6`, `-SDM`,
`-SOD`, `-SD1`, `-SD2`, or `-SD3`) for an exact selection. Interactive hosts
detect mouse and joystick hardware by default.
Use `--mouse` or `--joy` to force a device present, and `--nomouse` or
`--nojoy` to force it absent. Win32 and SDL3 capture an enabled mouse inside
the game window and release it while the window lacks focus. Use
`--fullscreen` to start fullscreen, and F11 or Alt+Enter to toggle fullscreen
without acknowledging an engine "any key" wait. `--windowed` overrides a
fullscreen default stored in a config file.


## Configuration precedence and constrained hardware

Startup preference precedence: launcher `.ini`/`.conf` files provide default
arguments, and real command-line arguments override matching option families.
Input and sound hardware switches then establish availability; a valid dataset
`CONFIG.<EXT>` supplies user preferences. Detected or forced presence never
re-enables a saved disabled choice. Missing hardware disables unsupported saved
choices, including a joystick whose selected port is absent. Without a valid
CONFIG file, mouse and sound defaults follow hardware and joystick control is
off. `--no-sound` explicitly mutes every sound choice. `--opl` and host audio
output settings are independent of these preferences.

DOS and Win9x defaults include `dbopl,silent,adlib` and select native `adlib`.
Nuked remains available as an explicit build choice. Use `--opl dbopl` for
software OPL (the spelling is `dbopl`, not `dbpol`) or `--opl silent`.
Win9x native AdLib requires accessible ISA OPL hardware at 388h/389h; it is
not available through that executable on NT-based Windows. The win2k/XP GDI
host has a separate optional [InpOut32 native OPL path](native-opl.md). The Win9x console-subsystem launcher prints
help/diagnostics in its invoking command prompt while the game opens a GDI window.


## Shareware data

The freely distributed data-only archives below provide complete playable
shareware/demo data sets without an installer:

- [Wolfenstein 3D v1.4 shareware data (`WL1`)](https://download.sourceforge.net/wolfgl/wolfdata.zip)
  - SHA-256: `A32EE97C515B6E182597A06F2326D15CC4C343DDC70558CE5FE76C870B7A0027`
- [Spear of Destiny demo data (`SDM`)](https://download.sourceforge.net/wolfgl/sdmdata.zip)
  - SHA-256: `054590923CD35CE7C0BFAE98C23BE81AB70C28E11FD0E562B5253523FCD7B91F`

Extract one ZIP beside the executable, or place its eight files in another
directory and pass `--data <directory>`. These mirrors are linked from the
archived [WolfGL download page](https://wolfgl.sourceforge.net/files.htm); the
files are not redistributed by this project. Registered `WL6`, `SOD`, `SD1`,
`SD2`, and `SD3` data must come from a legitimately obtained game copy.

## Audio drivers and hardware

Default modern packages compile all three software audio drivers. Select one at launch with
`--opl nuked`, `--opl dbopl`, or `--opl silent`; silence preserves all original
audio clocks and completion behavior. `--sample-rate HZ` changes the preferred
PCM rate from its 48 kHz default (44.1 kHz is used by the DOS SB16 host).
Build frontends can omit drivers for constrained targets. DOS and Win9x default to
native AdLib, DBOPL, and timing-preserving silence; Nuked is an opt-in DOS/Win9x
build because emulating it inside a DOS virtual machine is expensive.

Emulated hardware is a separate runtime choice. The default exposes Sound
Blaster and its AdLib-compatible OPL; `--adlib` or original `-nosb` exposes
AdLib only; `--pc-speaker` or original `-noal` exposes no sound card and
defaults to PC-speaker effects without CONFIG; `--no-sound` exposes no sound card with all
in-game sound initially off. Host-only `--no-audio` suppresses physical output
without changing what hardware the game detects. All modes retain the internal
audio clock.


See [building.md](building.md) for compiling, [support-matrix.md](support-matrix.md)
for validated platforms, and [DISTRIBUTION-CONTENTS.md](DISTRIBUTION-CONTENTS.md)
for what must remain with a package.

## Moving and resizing the GDI window

Dragging/resizing or opening the window system menu keeps the engine, demo,
audio and memory framebuffer running. To avoid expensive scaling on older
machines, the host skips GDI presentation until the modal loop ends, then
shows the latest frame. The visible image can remain still during a drag;
gameplay is not paused unless an explicit game pause/menu already requires it.

### SDL3 windows

SDL3 live-resize events also keep simulation, audio and the internal framebuffer
running during native move/resize loops. The host skips presentation from these
callbacks; normal presentation resumes when event polling returns. Explicit
in-game pauses still apply. On desktops without a blocking modal loop, the
ordinary frame loop continues to render and present normally.

## Automatic audio policy

The default OPL selection is `auto`. DOS, Win9x, win2k and winxp try AdLib,
DBOPL, Nuked, then silent. Newer targets try AdLib (if compiled and supported),
Nuked, DBOPL, then silent. Missing/unavailable candidates are skipped quietly.
Silent is always compiled in; it silences FM synthesis, not digitized effects.
An explicit `--opl adlib`, `dbopl`, `nuked` or `silent` never falls back: missing
or failed drivers stop with an error. CLI overrides launcher configuration.

Sample rates are runtime-only. The default is `--sample-rate auto`; numeric
8000..192000 Hz overrides remain supported. Native AdLib prefers 22050 Hz.
Legacy targets prefer 22050 Hz for DBOPL/silent and 44100 Hz for Nuked; modern
targets prefer 48000 Hz for software drivers. Selection resolves after fallback;
PCM device negotiation may change the actual rate. OPL timestamps and mixing
use that negotiated rate. PCM-output failure is separate from OPL fallback.

`--diag` opens/closes audio to report candidate attempts, failure reasons and
preferred/negotiated rates. Auto fallback is quiet in ordinary gameplay.
On NT, auto may load InpOut32 and install/start its embedded driver. Explicit
software drivers never load it. Loader-error popups are suppressed around the
optional DLL load; explicit native failure is reported by the application.

Builds no longer offer a fixed sample rate. Old SAMPLE_RATE variables and
WG_DEFAULT_SAMPLE_RATE do not control playback; move overrides to the launcher
configuration or command line. BUILD.TXT records the runtime-auto policy.

### Native OPL on Linux

Linux x86/x64 SDL3 and KMS/fbdev builds support `--opl adlib`, keeping existing
PCM playback through ALSA or SDL's audio backend. `auto` falls back quietly if
raw port access or the chip is unavailable. See [native OPL setup](native-opl.md)
for permissions, mixer configuration and pending physical validation.
