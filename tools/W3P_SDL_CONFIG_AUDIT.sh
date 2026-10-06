#!/bin/sh
set -eu

if [ "$#" -ne 1 ]; then
    echo "usage: $0 SDL_BUILD_DIR" >&2
    exit 2
fi

header=$(find "$1/third_party/SDL3" -path '*/build_config/SDL_build_config.h' \
    -type f -print -quit)
if [ -z "$header" ]; then
    echo "generated SDL build configuration not found under $1" >&2
    exit 1
fi

required='SDL_VIDEO_DRIVER_X11
SDL_VIDEO_DRIVER_X11_DYNAMIC
SDL_VIDEO_DRIVER_WAYLAND
SDL_VIDEO_DRIVER_WAYLAND_DYNAMIC
SDL_VIDEO_DRIVER_WAYLAND_DYNAMIC_CURSOR
SDL_VIDEO_DRIVER_WAYLAND_DYNAMIC_EGL
SDL_VIDEO_DRIVER_WAYLAND_DYNAMIC_XKBCOMMON
SDL_AUDIO_DRIVER_ALSA
SDL_AUDIO_DRIVER_ALSA_DYNAMIC
SDL_AUDIO_DRIVER_PULSEAUDIO
SDL_AUDIO_DRIVER_PULSEAUDIO_DYNAMIC'

for macro in $required; do
    if ! grep -q "^#define $macro\([[:space:]]\|$\)" "$header"; then
        echo "required SDL backend macro is missing: $macro" >&2
        exit 1
    fi
done

echo "SDL configuration audit: dynamic X11, Wayland, ALSA, and PulseAudio enabled"
