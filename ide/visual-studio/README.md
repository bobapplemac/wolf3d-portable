# Visual Studio compatibility bands

Each directory contains solution/project files known to share one Visual
Studio file format and CMake-backed workflow. All bands compile the same root
wrapper sources and the pinned `wolf3d-lib` submodule.

| Directory | Supported IDEs/toolsets | Wrappers |
| --- | --- | --- |
| `vs2019-vs2022/` | Visual Studio 2019/v142 and 2022/v143 | Win32/GDI and SDL3 |
| `vs2015/` | Visual Studio 2015/v140 and v140_xp | Win32/GDI only |

VS2008--VS2013 are currently supported through the root PowerShell dispatcher
and CMake-generated solutions. VC6--VS2005 use the XP-native dispatcher under
`scripts/windows/legacy/`. A checked-in IDE band is added only after testing
the relevant period IDE and project-file boundary.
