#!/bin/sh
set -eu

if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
    echo "usage: $0 BUNDLE_DIR [sdl3|kms-fbdev]" >&2
    exit 2
fi

bundle=$1
backend=${2:-sdl3}
test -f "$bundle/README.TXT"
test -f "$bundle/DOCS/BUILD.TXT"
test -f "$bundle/DOCS/NOTICES.TXT"
test -f "$bundle/DOCS/LICENSES/GPL-2.TXT"
test ! -d "$bundle/DOCS/DOCS"
test ! -d "$bundle/LICENSES"
case "$backend" in
    sdl3) binary=wolf3d ;;
    kms-fbdev) binary=wolf3d ;;
    *) echo "Unsupported musl backend: $backend" >&2; exit 2 ;;
esac
if [ ! -x "$bundle/wolf3d" ] || [ ! -x "$bundle/bin/$binary" ]; then
    echo "incomplete musl application bundle: $bundle" >&2
    exit 2
fi

found_elf=0
for candidate in "$bundle/bin/$binary" "$bundle"/lib/*; do
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

bundle_absolute=$(CDPATH= cd -- "$bundle" && pwd)
help_output=$("$bundle/wolf3d" --help)
if ! printf '%s\n' "$help_output" \
        | grep -F "Usage: $bundle_absolute/wolf3d [options]" >/dev/null; then
    echo "musl launcher did not preserve its top-level argv[0]" >&2
    exit 1
fi

if [ "$backend" = kms-fbdev ]; then
    # Validate relocation and audio configuration without touching a real VT,
    # DRM master, input device, or sound card.
    test -f "$bundle/share/alsa/alsa.conf"
    test -f "$bundle/DOCS/BUILD.TXT"
    temp=$(mktemp -d)
    trap 'rm -rf "$temp"' EXIT INT TERM
    mkdir -p "$temp/relocated bundle"
    cp -a "$bundle/." "$temp/relocated bundle/"
    moved="$temp/relocated bundle"
    (cd / && "$moved/wolf3d" --help) | grep -F "Usage: $moved/wolf3d [options]" >/dev/null
    ln -s wolf3d "$moved/spear"
    "$moved/spear" --help | grep -F "Usage: $moved/spear [options]" >/dev/null
    ${CC:-cc} tools/W3P_ALSA_SMOKE.c -o "$temp/alsa-smoke" -lasound
    ALSA_CONFIG_DIR="$moved/share/alsa" ALSA_CONFIG_PATH=/proc/self/fd/9 \
        ALSA_PLUGIN_DIR="$moved/lib/alsa-lib" \
        "$moved/lib/ld-musl-x86_64.so.1" --library-path "$moved/lib" "$temp/alsa-smoke" 9< "$moved/share/alsa/alsa.conf"
    # Prove the audit rejects a missing dependency instead of finding a host copy.
    rm "$moved/lib/libdrm.so.2"
    if sh "$0" "$moved" kms-fbdev > "$temp/missing-dependency.log" 2>&1; then
        echo 'audit accepted an incomplete bundle' >&2; exit 1
    fi
    grep -F "unbundled ELF dependency 'libdrm.so.2'" "$temp/missing-dependency.log" >/dev/null
    echo 'musl KMS/fbdev audit: dependency closure, relocation, argv[0], and ALSA null PCM passed'
    echo 'Real display/input/audio and VT restoration require a local-console test.'
    exit 0
fi

Xvfb :99 -screen 0 640x480x24 >/tmp/wolf3d-xvfb.log 2>&1 &
xvfb_pid=$!
trap 'kill "$xvfb_pid" >/dev/null 2>&1 || true' EXIT INT TERM
sleep 1
DISPLAY=:99 SDL_VIDEODRIVER=x11 SDL_AUDIODRIVER=dummy \
    "$bundle/wolf3d" --sdl3-video-smoke

echo "musl bundle audit: dependency closure, top-level argv[0], and X11 video smoke tests passed"
