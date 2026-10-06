# Build scripts

The root `build.ps1` is the stable modern Windows entry point. With no
arguments it launches the dependency-aware
`scripts/windows/configure-build.ps1`; explicit arguments go to
`scripts/windows/invoke-build.ps1`. Both delegate to authoritative CMake
presets and targets.

Root `build.sh` provides the equivalent Linux convention. Its configurator and
thin Make executor live under `scripts/linux/`; direct `make` remains fully
supported. Linux-hosted helpers, including the planned Docker/Open Watcom DOS
cross-build, also belong there.

Root `build.cmd` provides the XP-compatible guided scanner. Its deterministic
executor is `scripts/windows/legacy/build.cmd`; it selects the old Visual Studio generator,
builds the pinned `wolf3d-lib` submodule, and stages a compiler-qualified
Win32/GDI package without touching SDL3. Run it without arguments to display
the accepted compiler and build options.
