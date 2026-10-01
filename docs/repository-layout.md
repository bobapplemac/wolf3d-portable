# Repository layout

| Path | Purpose |
| --- | --- |
| `lib/wolf3d/` | Pinned, `main`-tracking `wolf3d-lib` submodule. |
| `platforms/win32/` | Native GDI/WinMM host. |
| `platforms/sdl3/` | Cross-platform SDL3 host. |
| `platforms/linux-console/` | DRM/KMS, evdev, and ALSA host. |
| `platforms/WG_*` | Host-shared text-output and command-line support. |
| `third_party/SDL3/` | Pinned SDL3 submodule. |
| `packaging/` | Runtime instructions and portable Linux builder. |
| `ide/visual-studio/` | Source-owned Visual Studio entry project. |
| `build/` | Ignored compiler output. |
| `dist/` | Ignored minimal redistributable folders. |

Production wrapper targets have no private `lib/wolf3d/src` include path.
The public CMake target supplies only `lib/wolf3d/include` transitively.
