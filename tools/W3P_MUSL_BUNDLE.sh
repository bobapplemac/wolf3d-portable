#!/bin/sh
set -eu

if [ "$#" -ne 2 ]; then
    echo "usage: $0 STAGED_SDL3_DIR BUNDLE_DIR" >&2
    exit 2
fi

stage=$1
bundle=$2

case "$bundle" in
    ""|/|.|..)
        echo "refusing unsafe bundle directory: '$bundle'" >&2
        exit 2
        ;;
esac

if [ ! -x "$stage/wolf3d-sdl3" ]; then
    echo "staged SDL3 executable is missing: $stage/wolf3d-sdl3" >&2
    exit 2
fi
if [ "$(uname -m)" != "x86_64" ]; then
    echo "the current musl bundle target supports x86_64 only" >&2
    exit 2
fi

rm -rf -- "$bundle"
mkdir -p "$bundle/bin" "$bundle/lib"

cp "$stage/wolf3d-sdl3" "$bundle/bin/wolf3d-sdl3"
for item in "$stage"/*.so*; do
    [ -e "$item" ] || [ -L "$item" ] || continue
    cp -a "$item" "$bundle/lib/"
done

for item in README.txt COPYING.txt COPYING.Nuked-OPL3.txt \
        COPYING.SDL3.txt THIRD_PARTY.md WOLF3D-LIB.txt; do
    if [ -f "$stage/$item" ]; then
        cp "$stage/$item" "$bundle/$item"
    fi
done

# The loader and libc are the same musl ELF under two runtime names. Follow
# Alpine's symlink so the bundle remains intact when copied or archived.
cp -L /lib/ld-musl-x86_64.so.1 "$bundle/lib/ld-musl-x86_64.so.1"
cp -L /lib/ld-musl-x86_64.so.1 "$bundle/lib/libc.musl-x86_64.so.1"
cp packaging/COPYING.musl.txt "$bundle/COPYING.musl.txt"
cat packaging/SDL3-MUSL-NOTES.txt >> "$bundle/README.txt"
cp packaging/linux-musl/wolf3d "$bundle/wolf3d"
chmod 0755 "$bundle/wolf3d" "$bundle/bin/wolf3d-sdl3" \
    "$bundle/lib/ld-musl-x86_64.so.1"

for item in "$bundle/bin/wolf3d-sdl3" "$bundle"/lib/*; do
    [ -f "$item" ] || continue
    if file "$item" | grep -q 'ELF'; then
        strip --strip-unneeded "$item"
    fi
done

printf '%s\n' "musl bundle staged: $bundle"
