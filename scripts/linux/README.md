# Linux build scripts

Root `build.sh` launches `configure-build.sh` without arguments and forwards
explicit arguments through `invoke-build.sh` to GNU Make. The configurator can
initialize recorded submodules with confirmation, but it never installs
external tools. CMake remains the build graph and direct Make use is supported.

The planned Open Watcom/DOS wrapper cross-build will run from Linux/Docker and
therefore belongs under this directory even though it will emit a 32-bit DOS
application.
