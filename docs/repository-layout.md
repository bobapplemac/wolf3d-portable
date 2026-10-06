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
| `cmake/legacy/` | Isolated VC6-through-VS2005 Win32/GDI build definition. |
| `ide/visual-studio/vsYYYY/` | One native, toolset-pinned solution/project pair per supported Visual Studio IDE generation. |
| `scripts/windows/` | Human-facing modern and XP-era Windows build dispatchers. |
| `scripts/linux/` | Linux-hosted helpers, including future Docker cross-build scripts. |
| `build/` | Ignored compiler output. |
| `dist/` | Ignored minimal redistributable folders. |

Production wrapper targets have no private `lib/wolf3d/src` include path.
The public CMake target supplies only `lib/wolf3d/include` transitively.
