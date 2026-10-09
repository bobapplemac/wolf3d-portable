# wolf3d-portable

`wolf3d-portable` provides runnable host integrations for the preservation-
oriented [`wolf3d-lib`](https://github.com/bobapplemac/wolf3d-lib) engine.
Every wrapper builds the library from its selected Git submodule checkout
and links only the public `WOLF3D.h`
API through the `wolf3d::wolf3d` CMake target.

The repository currently contains:

- a dependency-free Win32/GDI host;
- a cross-platform SDL3 host for Windows, X11, Wayland, and other SDL targets;
- a Linux virtual-console host using DRM/KMS or fbdev, evdev, and ALSA;
- an initial 32-bit protected-mode DOS host using VGA, the keyboard controller,
  PIT timing, Open Watcom, and DOS/32A.

No commercial game data is included. Supply files from a supported original
Wolfenstein 3D or Spear of Destiny installation: WL1, WL6, SDM, SOD, SD1,
SD2, or SD3.

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

Default modern packages compile all three software audio drivers. Select one at launch with
`--opl nuked`, `--opl dbopl`, or `--opl silent`; silence preserves all original
audio clocks and completion behavior. `--sample-rate HZ` changes the preferred
PCM rate from its 48 kHz default (44.1 kHz is used by the DOS SB16 host).
Build frontends can omit drivers for constrained targets. DOS defaults to
native AdLib, DBOPL, and timing-preserving silence; Nuked is an opt-in DOS
build because emulating it inside a DOS virtual machine is expensive.

Emulated hardware is a separate runtime choice. The default exposes Sound
Blaster and its AdLib-compatible OPL; `--adlib` or original `-nosb` exposes
AdLib only; `--pc-speaker` or original `-noal` exposes no sound card and
defaults to PC-speaker effects without CONFIG; `--no-sound` exposes no sound card with all
in-game sound initially off. Host-only `--no-audio` suppresses physical output
without changing what hardware the game detects. All modes retain the internal
audio clock.

## Clone and initialize

The root `build.sh`, `build.ps1`, and `build.cmd` scripts check for newer
published source before building. For a clean older copy, answer **Yes** to
update the source and its required components together, or **No** (the default)
to build your current copy. Local edits, local development commits, and
explicitly selected source versions are preserved. The check follows your
configured branch (normally `main`); it never switches branches.

Portable normally offers to build against the latest compatible `wolf3d-lib`
`main`, including engine fixes published without an application update. The
same confirmation covers application updates and engine updates. The host
independently declares its supported API in `platforms/WG_ENGINE_COMPAT.h`;
an incompatible engine update is skipped with an explanation, and compilation
also rejects an incompatible engine selected manually. Breaking contract
changes must advance the engine API version.

The recorded engine commit remains a reproducible/offline starting point;
third-party components such as SDL3 stay at their recorded versions. An engine
update accepted through these scripts is remembered locally so later checks
can continue updating it. Edits or other custom component selections are
preserved. Build/package versions identify the engine actually selected.

The check needs Git (and Git for Windows or MSYS2 Bash on Windows). Source archives,
unavailable tools, and failed network checks continue with existing sources.
An accepted update that fails stops the build so incomplete dependencies are
not used. Unattended runs never accept updates automatically. Set
`WOLF3D_GIT_CHECK=0` to skip the check entirely, or
`WOLF3D_GIT_INTERACTIVE=0` to check without prompting; PowerShell's
`-NonInteractive` also disables update prompts. Direct Make/CMake and executor
scripts retain their existing behavior. Regression coverage can be run with
`python tests/WG_GIT_PREFLIGHT_TEST.py` (Python 3 and Git/Bash required).


Clone recursively so both `wolf3d-lib` and the pinned SDL3 checkout are
present:

```text
git clone --recursive https://github.com/bobapplemac/wolf3d-portable.git
cd wolf3d-portable
```

For an existing checkout:

```text
git submodule update --init --recursive
```

The committed engine revision is a reproducible starting point. The root build
scripts offer the latest compatible engine independently of that revision. On
Linux, `make fresh` uses the same confirmed update check and then builds the
default SDL3 package. Packages record both application and engine commit IDs;
engine-only fixes do not require updating the application repository.

The repository's development lineage and its relationship to the companion
library's reconstructed public history are recorded in
[`docs/history.md`](docs/history.md). Package naming and exact source identity
are described in [`docs/build-identity.md`](docs/build-identity.md).

## Windows

Open `ide/visual-studio/vs2019/wolf3d-portable.sln` in Visual Studio 2019 or
the matching generation-specific solution under `ide/visual-studio/` for
Visual Studio 2022 or 2026, or use CMake. The established names below select
VS2019/v142:

```text
cmake --preset windows-dev-x64
cmake --build --preset windows-dev-x64
cmake --preset windows-release-x64
cmake --build --preset windows-release-x64
cmake --preset windows-sdl3-x64
cmake --build --preset windows-sdl3-x64
```

For VS2022/v143, add `vs2022` after `windows`, for example
`windows-vs2022-dev-x64`, `windows-vs2022-release-x64`, or
`windows-vs2022-sdl3-x64`. Substitute `vs2026` for the corresponding
VS2026/v145 presets.

From an MSYS2 UCRT64 shell, use the native-GCC x64 presets:

```text
cmake --preset windows-mingw-ucrt64-dev-x64
cmake --build --preset windows-mingw-ucrt64-dev-x64
cmake --preset windows-mingw-ucrt64-release-x64
cmake --build --preset windows-mingw-ucrt64-release-x64
cmake --preset windows-mingw-ucrt64-sdl3-x64
cmake --build --preset windows-mingw-ucrt64-sdl3-x64
```

The native Win32 wrapper also supports VS2017/v141, VS2015/v140, VS2013/v120,
VS2012/v110, VS2010/v100, and VS2008/v90 through matching `windows-vs20xx-*`
presets. Each supported IDE has its own native project under
`ide/visual-studio/`; VC6 uses `vc6/wolf3d-portable.dsw`. SDL3 is intentionally
excluded from every legacy compiler profile.

Windows XP-era x86 builds use the separate CMake definition and native CMD
dispatcher validated with VC6 SP6 and Visual Studio 2002 SP1, 2003 SP1, and
2005 SP1:

```text
build.cmd
scripts\windows\legacy\build.cmd vc6
scripts\windows\legacy\build.cmd vs2002
scripts\windows\legacy\build.cmd vs2003
scripts\windows\legacy\build.cmd vs2005
```

These commands default to a Release package with all audio drivers, Nuked as
the runtime default, and a static CRT.
Run the script without arguments for the complete option list. The resulting
compiler-qualified x86 game folders are staged under `dist/`. This path builds
only the Win32/GDI wrapper; SDL3 deliberately has no XP-era build profile.

The parallel `windows-vs2015-xp-*` profiles provide a newer XP-targeting
alternative using the v140_xp toolset.

For a guided command line, `build.ps1` detects installed supported Visual
Studio versions and MSYS2 UCRT64 MinGW. It defaults to publishing both x64
wrappers with the newest Visual Studio found. With no arguments it
interactively walks through the choices:

```powershell
.\build.ps1 -List
.\build.ps1
.\build.ps1 -Compiler vs2026 -Architecture x64 -Wrapper all
.\build.ps1 -Compiler vs2019 -Architecture x86 -Wrapper win32
.\build.ps1 -Compiler vs2015 -Architecture x86 -Wrapper win32
.\build.ps1 -Compiler vs2015-xp -Architecture x86 -Wrapper win32
.\build.ps1 -Compiler vs2008 -Architecture x86 -Wrapper win32
.\build.ps1 -Compiler mingw-ucrt64 -Wrapper win32
.\build.ps1 -Compiler mingw-ucrt64 -Wrapper sdl3
.\build.ps1 -Compiler mingw-ucrt64 -Wrapper all -Publish
.\build.ps1 -Action build -Configuration Debug -Wrapper sdl3
```

Run `Get-Help .\scripts\windows\invoke-build.ps1 -Detailed` for every
automation option. With no arguments, the root launcher verifies recorded Git
submodules, offers to initialize missing revisions, and then guides the user
through valid detected choices. It never installs external build tools.
Pass `-Msys2Root C:\path\to\msys64` for a portable or non-default MSYS2
installation. MinGW release packages target x64 UCRT and statically link GCC
support code; publishing audits every EXE/DLL to prevent accidental MSYS,
Cygwin, libgcc, libstdc++, or winpthread runtime dependencies.

In Visual Studio, select `Publish Win32`, `Publish SDL3`, or `Publish All`
with the `x64` or `Win32` platform and build the solution. These publish
configurations invoke the same CMake release targets and stage the same
ready-to-copy folders under `dist/` as the command-line presets. Ordinary
Debug and Release configurations remain development builds under `build/`.

Use the corresponding `x86` presets for 32-bit builds. Release folders appear
under `dist/` and include `wolf3d.exe`, `wolf3d.dll`,
the replaceable `Nuked-OPL3.dll`, all required host DLLs, notices, and a
`DOCS/BUILD.TXT` file recording the exact portable commit plus the engine
version and commit. The numeric component in a package-folder name identifies
the bundled engine version; it is not a portable release version.

## Linux

Run `./build.sh` for a dependency-aware guided scan of native, portable-glibc,
and musl choices. Explicit arguments are forwarded to Make. `make help`
remains the complete command and variable reference; the usual paths are:

```text
make                         # pinned-SDL3 distribution
make console-release         # DRM/fbdev/evdev/ALSA distribution
make releases                # both distributions
make portable                # both in the Debian 10 compatibility container
make musl-sdl3               # relocatable bundle with its own musl userspace
make dos                     # Open Watcom/DOS32A x86 distribution
make windows-cross           # Win9x/XP/Win7/Win10 native PE packages
make sdl3-release CC=clang   # use Clang
make sdl3-release USE_SYSTEM_SDL3=ON
make sdl3-release OPL_DEFAULT=dbopl
make console-release OPL_DRIVERS=silent OPL_DEFAULT=silent
```

Pinned SDL3 is the redistribution default. An installed SDL 3.2 or newer can
be selected for distribution-integrated builds. The Debian 10 targets retain
the established glibc 2.28 build baseline and audit every staged ELF. The
musl target instead packages a private loader and closed shared-library set,
so it has no dependency on the destination's glibc version.

The DOS target is also a Linux-hosted Docker cross-build; Open Watcom does not
need to be installed on the host. It stages `WOLF3D.EXE` and the DOS/32A
drop-in `DOS4GW.EXE` loader under
`dist/wolf3d-portable_<version>_dos32_x86_vga_openwatcom<major>/`. Copy original game data beside
both files and run `WOLF3D` on a Pentium-class or newer DOS system. This first
checkpoint has VGA mode 13h output, keyboard input, the original-style 700 Hz
PIT clock, and SB16 44.1 kHz 16-bit stereo PCM. Native AdLib, DBOPL, and silent
drivers are compiled by default and selected with the same `--opl` option used
by other wrappers. Native AdLib is the DOS default and sends the original
register stream to compatible hardware at port 388h; DBOPL is the software
fallback. Nuked remains available through an explicit `DOS_OPL_DRIVERS` build.

Windows cross-builds are similarly Docker-contained and require no Wine.
Open Watcom emits the Win9x x86 Win32/GDI package; MinGW-w64/MSVCRT emits the
XP x86 and Win7 x86/x64 packages; LLVM-MinGW independently covers Win7 with
MSVCRT and Win10 x64 with UCRT. SDL3 is included only in the Win7+ profiles.
Use the individual `make windows-*` targets listed by `make help` when the
complete matrix is unnecessary.

For native IDE development, open `ide/open-watcom/wolf3d-portable.wpj` through
the adjacent `open-ide.cmd`. That workspace builds the engine and all DOS host
libraries as separate targets before linking the executable. `make dos`
remains the canonical package-producing path.

## Running

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

Optional launcher defaults can be stored beside the user-facing executable.
Windows and DOS use `.ini`; Linux uses `.conf`. The exact executable name is
checked first (`wolf3d-sdl3.ini`), followed by a wrapper-neutral name with a
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

The Linux console host needs no X11 or Wayland. Its default `--video auto`
mode prefers a connected DRM/KMS display and falls back to `$FRAMEBUFFER` or
`/dev/fb0`. Use `--video drm` or `--video fbdev` to require one backend, with
`--drm-device PATH` or `--fb-device PATH` to select a device explicitly.

For DOS, copy a data set into the distribution directory and run:

```text
WOLF3D.EXE --game WL1
WOLF3D.EXE --game WL1 --opl dbopl
```

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

See the [build and compatibility matrix](docs/support-matrix.md) for every
supported compiler, wrapper, build entry point, artifact, and validated
destination operating system. [docs/building.md](docs/building.md) contains the
complete commands and option details.

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

Distribution folders and generated build metadata follow the shared
[build/dist naming convention](docs/BUILD-NAMING.md).

`make musl-console` builds the relocatable Linux musl KMS/fbdev package;
see [console test instructions](docs/building.md#relocatable-musl-kmsfbdev-package).
