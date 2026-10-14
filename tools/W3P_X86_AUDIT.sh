#!/bin/sh
# Audit project-built ELF files, not CPU-dispatched system libraries.
set -eu
package=${1:?usage: W3P_X86_AUDIT.sh PACKAGE}
count=0
for file in "$package"/*; do
    header=$(LC_ALL=C readelf -h "$file" 2>/dev/null) || continue
    count=$((count + 1))
    printf '%s\n' "$header" | grep -q 'Class:.*ELF32' || { echo "Not ELF32: $file" >&2; exit 1; }
    printf '%s\n' "$header" | grep -q 'Machine:.*Intel 80386' || { echo "Not x86: $file" >&2; exit 1; }
    # SIMD is disabled for this profile, including SDL's optimized routines.
    if objdump -d "$file" | grep -Eq '%(xmm|ymm|zmm|mm)[0-9]'; then
        echo "Unexpected SIMD instructions in legacy profile: $file" >&2
        exit 1
    fi
done
[ "$count" -gt 0 ] || { echo "No ELF files: $package" >&2; exit 1; }
echo "Legacy x86 audit passed: $count ELF32 files, no MMX/SSE/AVX register instructions."
