# Building wolf3d-portable

## Dependencies

Initialize both submodules before configuring:

```text
git submodule update --init --recursive
```

The `lib/wolf3d` gitlink records the exact engine revision. `third_party/SDL3`
records the default GUI dependency. A system SDL 3.2 or newer is also
supported.

## Windows

Visual Studio and CLI builds share the CMake trees under `build/`:

```text
cmake --preset windows-dev-x64
cmake --build --preset windows-dev-x64
```

The development preset builds both GUI hosts. Dedicated distribution presets
are `windows-release-{x64,x86}` for Win32 and `windows-sdl3-{x64,x86}` for
SDL3. The MSVC runtime is statically linked by default; set
`WG_STATIC_MSVC_RUNTIME=OFF` in a separate build tree to use the matching
Visual C++ Redistributable.

The checked-in `wolf3d-portable.sln` invokes these same configurations from
Visual Studio.

## Linux

`make` defaults to the pinned-SDL3 distribution. Useful targets include:

```text
make help
make dependencies
make fresh
make sdl3-release
make console-release
make releases
make portable-sdl3
make portable-console
make portable
make clean
```

GCC is the default. Select Clang with `CC=clang`. Set
`USE_SYSTEM_SDL3=ON` to use the installed SDL package. `CMAKE_ARGS` passes
additional definitions through to configuration, and `JOBS=N` limits build
parallelism.

The console host requires libdrm and ALSA development packages. SDL3 builds do
not require those packages directly. The pinned build dynamically discovers
available X11, Wayland, KMS/DRM, and audio backends.

## Reproducibility and latest-main builds

Ordinary builds are offline with respect to `wolf3d-lib`: they use the
committed gitlink. `make fresh` runs:

```text
git submodule update --remote --merge -- lib/wolf3d
git submodule update --init --recursive
make all
```

This is intentionally convenient for active integration. Commit the resulting
gitlink to make that engine selection reproducible. Every staged folder
contains `WOLF3D-LIB.txt` with the resolved `1.4.REVISION` and full commit SHA.

## Linux portability

The portable targets build inside the digest-pinned Debian 10 container and
audit staged ELFs against glibc 2.28. The SDL package includes its pinned SDL3
shared object and uses `$ORIGIN`; normal kernel, graphics, input, and audio
facilities remain system supplied. `make clean` removes project build trees
and the project-named Docker image without pruning unrelated Docker data.
