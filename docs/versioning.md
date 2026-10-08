# Versioning

`wolf3d-portable` and `wolf3d-lib` have independent release identities.

The root `VERSION` file contains the portable-host version using semantic
versioning:

- major: incompatible command-line, package-layout, or host-interface changes;
- minor: compatible wrapper features, new hosts, or newly supported platforms;
- patch: compatible wrapper, packaging, documentation, or build-system fixes.

The public repository begins at `1.0.0`. A commit does not need to advance the
portable version unless it represents the next releasable portable state; the
version must be advanced before publishing a package or tag containing such a
change.

The engine retains its historical `1.4.REVISION` policy independently. Every
portable package includes `WOLF3D-LIB.txt`, which records both the portable
version and the exact wolf3d-lib version and commit used to build it.

## Pinned and latest-library workflows

The committed `lib/wolf3d` gitlink is the reproducible default. A normal build
is offline with respect to the engine and always uses that exact commit.

The submodule declares `branch = main`, so maintainers can deliberately test
and adopt the newest public engine with:

```text
git submodule update --remote --merge -- lib/wolf3d
git submodule update --init --recursive
```

On Linux, `make fresh` performs those operations and then builds. If the new
engine is accepted, commit the changed gitlink. Automatically advancing the
submodule during every ordinary build is intentionally unsupported because it
would make identical portable commits produce different binaries over time.
