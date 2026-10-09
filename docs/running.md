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
not available on NT-based Windows. The Win9x console-subsystem launcher prints
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
