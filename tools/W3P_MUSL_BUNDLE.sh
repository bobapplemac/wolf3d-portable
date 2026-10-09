#!/bin/sh
set -eu

if [ "$#" -lt 2 ] || [ "$#" -gt 3 ]; then
    echo "usage: $0 STAGED_DIR BUNDLE_DIR [sdl3|kms-fbdev]" >&2
    exit 2
fi

stage=$1
bundle=$2
backend=${3:-sdl3}
case "$backend" in
    sdl3) staged_binary=wolf3d; binary=wolf3d ;;
    kms-fbdev) staged_binary=wolf3d; binary=wolf3d ;;
    *) echo "Unsupported musl backend: $backend" >&2; exit 2 ;;
esac

case "$bundle" in
    ""|/|.|..)
        echo "refusing unsafe bundle directory: '$bundle'" >&2
        exit 2
        ;;
esac

if [ ! -x "$stage/$staged_binary" ]; then
    echo "staged executable is missing: $stage/$staged_binary" >&2
    exit 2
fi
if [ "$(uname -m)" != "x86_64" ]; then
    echo "the current musl bundle target supports x86_64 only" >&2
    exit 2
fi

rm -rf -- "$bundle"
mkdir -p "$bundle/bin" "$bundle/lib" "$bundle/DOCS/LICENSES"

cp "$stage/$staged_binary" "$bundle/bin/$binary"
for item in "$stage"/*.so*; do
    [ -e "$item" ] || [ -L "$item" ] || continue
    cp -a "$item" "$bundle/lib/"
done

cp "$stage/README.TXT" "$bundle/README.TXT"
cp -R "$stage/DOCS/." "$bundle/DOCS/"

# The loader and libc are the same musl ELF under two runtime names. Follow
# Alpine's symlink so the bundle remains intact when copied or archived.
cp -L /lib/ld-musl-x86_64.so.1 "$bundle/lib/ld-musl-x86_64.so.1"
cp -L /lib/ld-musl-x86_64.so.1 "$bundle/lib/libc.musl-x86_64.so.1"
apk info --who-owns /lib/ld-musl-x86_64.so.1 > "$bundle/DOCS/RUNTIME.TXT"

# SDL loads its desktop and audio backends with dlopen(). A musl process cannot
# load the destination system's glibc-built copies, so seed the bundle with the
# matching Alpine/musl libraries and then close their ordinary ELF dependency
# graph. Copy each object under its SONAME so SDL and the musl loader find the
# exact name they request without depending on Alpine's symlink layout.
copy_runtime_library()
{
    soname=$1
    if [ -e "$bundle/lib/$soname" ]; then
        return 1
    fi
    source_path=
    for directory in /lib /usr/lib; do
        if [ -e "$directory/$soname" ]; then
            source_path=$directory/$soname
            break
        fi
    done
    if [ -z "$source_path" ]; then
        source_path=$(find /lib /usr/lib -name "$soname" -print -quit)
    fi
    if [ -z "$source_path" ]; then
        echo "musl runtime library is missing from the builder: $soname" >&2
        exit 1
    fi
    cp -L "$source_path" "$bundle/lib/$soname"
    apk info --who-owns "$(readlink -f "$source_path")" >> "$bundle/DOCS/RUNTIME.TXT"
    return 0
}

if [ "$backend" = sdl3 ]; then
runtime_roots='libX11.so.6
libXext.so.6
libXcursor.so.1
libXi.so.6
libXfixes.so.3
libXrandr.so.2
libXrender.so.1
libXss.so.1
libXtst.so.6
libwayland-client.so.0
libwayland-cursor.so.0
libwayland-egl.so.1
libxkbcommon.so.0
libudev.so.1
libasound.so.2
libpulse.so.0'
else
    runtime_roots='libdrm.so.2 libasound.so.2'
    mkdir -p "$bundle/share" "$bundle/lib/alsa-lib"
    mkdir -p "$bundle/share/alsa"
    # Use ALSA's built-in hardware/plug/null PCMs. External desktop-server
    # plugins are deliberately not dependencies of the direct-console host.
    cp packaging/linux-musl/alsa-console.conf "$bundle/share/alsa/alsa.conf"
fi

for runtime_root in $runtime_roots; do
    copy_runtime_library "$runtime_root" || true
done

changed=1
while [ "$changed" -eq 1 ]; do
    changed=0
    for candidate in "$bundle/bin/$binary" "$bundle"/lib/*; do
        [ -f "$candidate" ] || continue
        if ! file "$candidate" | grep -q 'ELF'; then
            continue
        fi
        for needed in $(readelf -d "$candidate" 2>/dev/null \
                | sed -n 's/.*Shared library: \[\([^]]*\)\].*/\1/p'); do
            if copy_runtime_library "$needed"; then
                changed=1
            fi
        done
    done
done

cp packaging/COPYING.musl.txt "$bundle/DOCS/LICENSES/MUSL.TXT"
cp packaging/COPYING.LGPL-2.1.txt "$bundle/DOCS/LICENSES/LGPL-21.TXT"
cp packaging/COPYING.ALSA.txt "$bundle/DOCS/LICENSES/ALSA.TXT"
if [ "$backend" = sdl3 ]; then
    cat packaging/SDL3-MUSL-NOTES.txt >> "$bundle/README.TXT"
else
    cp packaging/KMS-FBDEV-MUSL-NOTES.txt "$bundle/README.TXT"
    cp packaging/COPYING.libdrm.txt "$bundle/DOCS/LICENSES/LIBDRM.TXT"
fi
sed "s/@BINARY@/$binary/g" packaging/linux-musl/wolf3d > "$bundle/wolf3d"
chmod 0755 "$bundle/wolf3d" "$bundle/bin/$binary" \
    "$bundle/lib/ld-musl-x86_64.so.1"

for item in "$bundle/bin/$binary" "$bundle"/lib/*; do
    [ -f "$item" ] || continue
    if file "$item" | grep -q 'ELF'; then
        strip --strip-unneeded "$item"
    fi
done

printf '%s\n' "musl bundle staged: $bundle"

{
    printf '\n===== Musl bundle recipe =====\n'
    printf 'Stage: %s\nBundle: %s\nBackend: %s\n' "$stage" "$bundle" "$backend"
    cat "$0"
    printf '\n===== Alpine build packages =====\n'
    apk info -v
    printf '\n===== Launcher recipe =====\n'
    cat "$bundle/wolf3d"
    if [ "$backend" = kms-fbdev ]; then
        printf '\n===== ALSA configuration =====\n'
        cat "$bundle/share/alsa/alsa.conf"
    fi
} >> "$bundle/DOCS/BUILD.TXT"

# Record only runtime packages actually copied, not every build dependency.
{
    printf '\nBundled Alpine musl runtime components\n=====================================\n'
    printf 'These shared libraries remain independently replaceable.\n'
    printf 'musl terms: LICENSES/MUSL.TXT; ALSA notices: LICENSES/ALSA.TXT.\n'
    printf 'LGPL runtime terms: LICENSES/LGPL-21.TXT.\n'
    if [ "$backend" = kms-fbdev ]; then
        printf 'libdrm notices: LICENSES/LIBDRM.TXT.\n'
    fi
    printf 'Corresponding sources and build recipes: https://gitlab.alpinelinux.org/alpine/aports/-/tree/3.20-stable\n'
    printf 'Exact library ownership:\n'
    sort -u "$bundle/DOCS/RUNTIME.TXT"
    printf '\nPackage versions, licenses and upstream source websites:\n'
    awk 'BEGIN { RS=""; FS="\n" }
        NR==FNR { for(i=1;i<=NF;i++) { n=split($i,a," "); wanted[a[n]]=1 } next }
        { p="";v="";l="";u="";o="";
          for(i=1;i<=NF;i++) { key=substr($i,1,2); val=substr($i,3);
            if(key=="P:")p=val; if(key=="V:")v=val; if(key=="L:")l=val;
            if(key=="U:")u=val; if(key=="o:")o=val }
          if(wanted[p "-" v]) printf "%s %s | %s | %s | Alpine source package: %s\n",p,v,l,u,o
        }' "$bundle/DOCS/RUNTIME.TXT" /lib/apk/db/installed
} >> "$bundle/DOCS/NOTICES.TXT"
rm "$bundle/DOCS/RUNTIME.TXT"
