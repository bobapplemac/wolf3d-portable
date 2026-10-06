# Build scripts

The root `build.ps1` is the stable modern Windows entry point. Its
implementation lives in `scripts/windows/build.ps1` and delegates to the
authoritative CMake presets.

Linux uses the root Makefile as its human-facing dispatcher; run `make help`.

Windows XP-era x86 compiler builds use `build-legacy.cmd`. It selects the old
Visual Studio generator, builds the pinned `wolf3d-lib` submodule, and stages a
compiler-qualified Win32/GDI package without touching SDL3. Run it without
arguments to display the accepted compiler and build options.
