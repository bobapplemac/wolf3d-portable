#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
export WIN2K_BACKENDS=sdl1
exec sh "$root/scripts/linux/build-windows-modular.sh"
