# Build identity

`wolf3d-portable` is currently an unversioned rolling source project on
`main`. A portable release/version policy will be chosen when public binary
releases are planned; the repository does not claim a provisional semantic
version before then.

Package-folder names retain their established form:

```text
wolf3d-portable-<wolf3d-lib-version>-<host/platform/compiler>
```

The numeric component identifies the bundled engine version. It is useful for
sorting compatible host builds, but it is not a `wolf3d-portable` release
version. Every staged package includes `WOLF3D-LIB.txt`, which records the full
portable commit plus the exact wolf3d-lib version and commit.

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
