# Visual Studio compatibility bands

Each directory contains the native solution/project pair for exactly one
Visual Studio IDE generation. All projects compile the same root wrapper
sources and pinned `wolf3d-lib` submodule.

| Directory | IDE/toolset | Wrappers |
| --- | --- | --- |
| `vs2022/` | Visual Studio 2022/v143 | Win32/GDI and SDL3 |
| `vs2019/` | Visual Studio 2019/v142 | Win32/GDI and SDL3 |
| `vs2017/` | Visual Studio 2017/v141 | Win32/GDI only |
| `vs2015/` | Visual Studio 2015/v140 and v140_xp | Win32/GDI only |
| `vs2013/` | Visual Studio 2013/v120 | Win32/GDI only |
| `vs2012/` | Visual Studio 2012/v110 | Win32/GDI only |
| `vs2010/` | Visual Studio 2010/v100 | Win32/GDI only |
| `vs2008/` | Visual Studio 2008/v90 | Win32/GDI only |
| `vs2005/` | Visual Studio 2005/MSVC 14.00 | Win32/GDI only |
| `vs2003/` | Visual Studio .NET 2003/MSVC 13.10 | Win32/GDI only |
| `vs2002/` | Visual Studio .NET 2002/MSVC 13.00 | Win32/GDI only |
| `vc6/` | Visual C++ 6.0/MSVC 12.00 | Win32/GDI only |

"Supported" means that the solution opens and builds in the named IDE without
an upgrade/conversion prompt. A newer IDE's ability to import an older project
is deliberately not the supported workflow. VC6 uses its period `.dsw`/`.dsp`
pair; later IDEs use their native `.sln` and project format. The pre-VS2008
projects delegate compilation to the XP-native dispatcher under
`scripts/windows/`.
