#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
package=${1:?usage: W3P_RH9_AUDIT.sh PACKAGE}
sh "$root/tools/WG_GLIBC_AUDIT.sh" "$package" 2.3.2
sh "$root/tools/W3P_X86_AUDIT.sh" "$package"
for file in "$package"/*; do
    readelf -h "$file" >/dev/null 2>&1 || continue
    if objdump -d "$file" | grep -Eq '[[:space:]](cmov[a-z]*|fcmov[a-z]*|fcomi[a-z]*|fucomi[a-z]*)[[:space:]]'; then
        echo "i686 instruction in i586 package: $file" >&2; exit 1
    fi
    for dependency in $(readelf -d "$file" | sed -n 's/.*Shared library: \[\(.*\)\].*/\1/p'); do
        case "$dependency" in libc.so.6|libm.so.6|libdl.so.2|libpthread.so.0|libX11.so.6|libXext.so.6|libwolf3d.so.1|libSDL-1.2.so.0|libNuked-OPL3.so) ;;
        *) echo "Unexpected RH9 dependency: $file: $dependency" >&2; exit 1 ;; esac
    done
done
readelf -l "$package/wolf3d" | grep -q '/lib/ld-linux.so.2' || { echo 'Wrong ELF interpreter' >&2; exit 1; }
echo 'RH9/i586 ABI and instruction audit passed; Linux 2.4 physical/VM runtime validation still required.'
