# Linux build scripts

Root `build.sh` launches `configure-build.sh` without arguments and forwards
explicit arguments through `invoke-build.sh` to GNU Make. The configurator can
initialize recorded submodules with confirmation, but it never installs
external tools. CMake remains the build graph and direct Make use is supported.

The Open Watcom/DOS wrapper cross-build runs from Linux/Docker and therefore
belongs under this directory even though it emits a 32-bit DOS application.
`openwatcom/build-dos.sh` is the deterministic executor used by `make dos`;
the root Makefile owns image construction and safe cleanup.

`openwatcom/build-windows.sh` emits the Win9x Win32/GDI package, while
`windows-cross/build-portable.sh` handles the XP, Win7, and Win10 MinGW and
LLVM-MinGW profiles. All are Linux-hosted native PE builds; none uses Wine.

`build-windows-sdl1.sh` builds the experimental Win2k SDL1 profile. Use
`make windows-win2k-sdl1`; see [Windows SDL1](../../docs/windows-sdl1.md).
