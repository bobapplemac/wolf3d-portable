# wolf3d-portable

Runnable Wolfenstein 3D and Spear of Destiny applications built on the
preservation-oriented [wolf3d-lib](https://github.com/bobapplemac/wolf3d-lib) engine.
Choose GDI on Windows, SDL3 on desktop systems, KMS/fbdev on a Linux console,
or VGA on 32-bit DOS. See the [support matrix](docs/support-matrix.md) for
compiler, operating-system and runtime validation details.

Original game data is required and is not included. The [running guide](docs/running.md)
covers supported editions, shareware/demo downloads, configuration and audio.

## Build

```sh
git clone --recursive https://github.com/bobapplemac/wolf3d-portable.git
cd wolf3d-portable
```

Run the guided entry point for your build machine:

| Environment | Command |
| --- | --- |
| Linux | `./build.sh` |
| Modern Windows PowerShell | `.\build.ps1` |
| Legacy Windows Command Prompt | `build.cmd` |

The scripts detect installed tools and offer source updates for confirmation,
including the latest compatible engine. Decline to use your existing checkout.
See [building](docs/building.md) for prerequisites, IDEs, Docker cross-builds
and automation; see [source updates](docs/source-updates.md) for reproducibility.

## Run

Copy a complete package from `dist/`, put supported game data beside its
executable or in a subdirectory, and run `wolf3d` on Linux or `wolf3d.exe` on
Windows. DOS and Win9x packages use `WOLF3D.EXE`.

For example, from a Linux package directory:

```sh
./wolf3d --data /path/to/game-data
./wolf3d --help
./wolf3d --diag
```

The [running guide](docs/running.md) explains data selection, launcher defaults,
saved settings, input and audio. Keep the package's `README.TXT`, `DOCS/`, and
runtime dependencies together; Linux musl packages must use the top-level launcher.

## Documentation

The [documentation index](docs/README.md) links all maintained guides.
See [repository layout](docs/repository-layout.md), [package naming](docs/BUILD-NAMING.md),
[package contents](docs/DISTRIBUTION-CONTENTS.md), and [build identity](docs/build-identity.md)
for development and distribution details. Changes are recorded in the [changelog](CHANGELOG.md).

See [LICENSE](LICENSE), [third-party notices](THIRD_PARTY.md), and
[project history](docs/history.md) for licensing and lineage.
