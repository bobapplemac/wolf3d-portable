#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)
profile=${W3P_WINDOWS_CROSS_PROFILE:?set W3P_WINDOWS_CROSS_PROFILE}
jobs=${W3P_WINDOWS_CROSS_JOBS:-}
drivers=${W3P_WINDOWS_CROSS_OPL_DRIVERS:-nuked,dbopl,silent}
default_driver=${W3P_WINDOWS_CROSS_DEFAULT_OPL:-auto}
if [ "$profile" = mingw-win2k-x86 ] || [ "$profile" = mingw-xp-x86 ]; then
    drivers=${W3P_WINDOWS_CROSS_OPL_DRIVERS:-dbopl,silent,adlib}
    default_driver=${W3P_WINDOWS_CROSS_DEFAULT_OPL:-auto}
fi
case ",$drivers," in *,silent,*) ;; *) drivers="$drivers,silent" ;; esac
version=$(sed -n '1p' "$root/lib/wolf3d/VERSION")

case "$profile" in
    mingw-win2k-x86)
        toolchain=mingw-gcc-i686.cmake
        compiler_label=mingw-gcc-win2k
        minimum_windows=0x0500
        arch=x86
        build_sdl=OFF
        release_targets=win32-release
        ;;
    mingw-xp-x86)
        toolchain=mingw-gcc-i686.cmake
        compiler_label=mingw-gcc-xp
        minimum_windows=0x0501
        build_sdl=OFF
        release_targets=win32-release
        arch=x86
        ;;
    mingw-win7-x86)
        toolchain=mingw-gcc-i686.cmake
        compiler_label=mingw-gcc-win7
        minimum_windows=0x0601
        build_sdl=ON
        release_targets="win32-release sdl3-release"
        arch=x86
        ;;
    mingw-win7-x64)
        toolchain=mingw-gcc-x86_64.cmake
        compiler_label=mingw-gcc-win7
        minimum_windows=0x0601
        build_sdl=ON
        release_targets="win32-release sdl3-release"
        arch=x64
        ;;
    llvm-mingw-win7-x86)
        toolchain=llvm-mingw-i686.cmake
        compiler_label=llvm-mingw-msvcrt-win7
        minimum_windows=0x0601
        build_sdl=ON
        release_targets="win32-release sdl3-release"
        arch=x86
        ;;
    llvm-mingw-win7-x64)
        toolchain=llvm-mingw-x86_64.cmake
        compiler_label=llvm-mingw-msvcrt-win7
        minimum_windows=0x0601
        build_sdl=ON
        release_targets="win32-release sdl3-release"
        arch=x64
        ;;
    llvm-mingw-win10-x64)
        toolchain=llvm-mingw-x86_64.cmake
        compiler_label=llvm-mingw-ucrt-win10
        minimum_windows=0x0a00
        build_sdl=ON
        release_targets="win32-release sdl3-release"
        arch=x64
        ;;
    *)
        echo "Unknown Windows cross profile: $profile" >&2
        exit 2
        ;;
esac

case "$profile" in
    *-win2k-*) dist_platform=win2k ;;
    *-xp-*) dist_platform=winxp ;;
    *-win7-*) dist_platform=win7 ;;
    *-win10-*) dist_platform=win10 ;;
esac
case "$profile" in
    llvm-mingw-win10-*) dist_crt=ucrt ;;
    *) dist_crt=msvcrt ;;
esac

native_opl=OFF
case ",$drivers," in
    *,adlib,*)
        native_opl=ON ;;
esac
build_dir="$root/build/windows-cross-$profile"
cmake -S "$root" -B "$build_dir" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DWG_DIST_PLATFORM="$dist_platform" -DWG_DIST_CRT="$dist_crt" \
    -DCMAKE_TOOLCHAIN_FILE="$root/lib/wolf3d/cmake/toolchains/$toolchain" \
    -DCMAKE_C_FLAGS="-D_WIN32_WINNT=$minimum_windows -DWINVER=$minimum_windows" \
    -DW3P_BUILD_WIN32=ON \
    -DW3P_BUILD_SDL3="$build_sdl" \
    -DW3P_USE_SYSTEM_SDL3=OFF \
    -DW3P_WARNINGS_AS_ERRORS=ON \
    -DW3P_COMPILER_LABEL="$compiler_label" \
    -DWG_STATIC_GNU_RUNTIME=ON \
    -DW3P_ENABLE_INPOUT32="$native_opl" \
    -DWG_ENABLE_OPL_ADLIB="$native_opl" \
    -DWG_ENABLE_OPL_NUKED=$(case ",$drivers," in *,nuked,*) echo ON;; *) echo OFF;; esac) \
    -DWG_ENABLE_OPL_DBOPL=$(case ",$drivers," in *,dbopl,*) echo ON;; *) echo OFF;; esac) \
    -DWG_ENABLE_OPL_SILENT=$(case ",$drivers," in *,silent,*) echo ON;; *) echo OFF;; esac) \
    -DWG_DEFAULT_OPL_DRIVER="$default_driver"

for target in $release_targets; do
    if [ -n "$jobs" ]; then
        cmake --build "$build_dir" --target "$target" --parallel "$jobs"
    else
        cmake --build "$build_dir" --target "$target" --parallel
    fi
done

for target in $release_targets; do
    case "$target" in
        win32-release) manifest=W3P_WIN32_RELEASE_DIR ;;
        sdl3-release) manifest=W3P_SDL3_RELEASE_DIR ;;
    esac
    directory="$root/$(cat "$build_dir/$manifest-Release.path")"
    sh "$root/lib/wolf3d/tools/WG_WINDOWS_PE_AUDIT.sh" "$profile" "$directory"
done

echo "Windows portable profile staged: $profile"
