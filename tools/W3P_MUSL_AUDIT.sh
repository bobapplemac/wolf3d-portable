#!/bin/sh
set -eu

if [ "$#" -ne 1 ]; then
    echo "usage: $0 BUNDLE_DIR" >&2
    exit 2
fi

bundle=$1
if [ ! -x "$bundle/wolf3d" ] || [ ! -x "$bundle/bin/wolf3d-sdl3" ]; then
    echo "incomplete musl application bundle: $bundle" >&2
    exit 2
fi

found_elf=0
for candidate in "$bundle/bin/wolf3d-sdl3" "$bundle"/lib/*; do
    [ -f "$candidate" ] || continue
    if ! file "$candidate" | grep -q 'ELF'; then
        continue
    fi
    found_elf=1
    echo "musl bundle audit: $candidate"
    readelf -d "$candidate" 2>/dev/null | grep '(NEEDED)' || true
    if readelf --version-info "$candidate" 2>/dev/null | grep -q 'GLIBC_'; then
        echo "glibc symbol version found in $candidate" >&2
        exit 1
    fi
    for needed in $(readelf -d "$candidate" 2>/dev/null \
            | sed -n 's/.*Shared library: \[\([^]]*\)\].*/\1/p'); do
        if [ ! -e "$bundle/lib/$needed" ] && [ ! -L "$bundle/lib/$needed" ]; then
            echo "unbundled ELF dependency '$needed' required by $candidate" >&2
            exit 1
        fi
    done
done

if [ "$found_elf" -ne 1 ]; then
    echo "no ELF files found in $bundle" >&2
    exit 1
fi

"$bundle/wolf3d" --sdl3-help >/dev/null

Xvfb :99 -screen 0 640x480x24 >/tmp/wolf3d-xvfb.log 2>&1 &
xvfb_pid=$!
trap 'kill "$xvfb_pid" >/dev/null 2>&1 || true' EXIT INT TERM
sleep 1
DISPLAY=:99 SDL_VIDEODRIVER=x11 SDL_AUDIODRIVER=dummy \
    "$bundle/wolf3d" --sdl3-video-smoke

echo "musl bundle audit: closed dependency set, launcher, and X11 video smoke tests passed"
