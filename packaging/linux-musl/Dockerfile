FROM alpine:3.20.10@sha256:d9e853e87e55526f6b2917df91a2115c36dd7c696a35be12163d44e6e2a4b6bc

LABEL org.opencontainers.image.title="wolf3d-portable Alpine musl builder" \
      org.opencontainers.image.description="Relocatable x86-64 Linux SDL3 release toolchain"

RUN apk add --no-cache \
        alsa-lib-dev \
        binutils \
        build-base \
        clang \
        cmake \
        dbus-dev \
        eudev-dev \
        file \
        libdecor-dev \
        libdrm-dev \
        pkgconf \
        libx11-dev \
        libxcursor-dev \
        libxext-dev \
        libxfixes-dev \
        libxi-dev \
        libxkbcommon-dev \
        libxrandr-dev \
        libxscrnsaver-dev \
        libxtst-dev \
        make \
        mesa-dev \
        pipewire-dev \
        pulseaudio-dev \
        wayland-dev \
        wayland-protocols \
        xvfb

WORKDIR /src
