#!/usr/bin/env bash
set -e
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
executor="$root/scripts/linux/invoke-build.sh"

choose() {
    local prompt=$1; shift
    local options=("$@") answer i
    printf '\n%s\n' "$prompt" >&2
    for ((i=0; i<${#options[@]}; ++i)); do
        if [ "$i" -eq 0 ]; then
            printf '  %d. %s (recommended)\n' "$((i + 1))" "${options[$i]}" >&2
        else
            printf '  %d. %s\n' "$((i + 1))" "${options[$i]}" >&2
        fi
    done
    while :; do
        read -r -p 'Selection: ' answer
        answer=${answer:-1}
        if [[ $answer =~ ^[0-9]+$ ]] && ((answer >= 1 && answer <= ${#options[@]})); then
            printf '%s' "${options[$((answer - 1))]}"
            return
        fi
        printf 'Enter one of the listed numbers.\n' >&2
    done
}

ready() { command -v "$1" >/dev/null 2>&1; }
confirm() { local answer; read -r -p "$1 [Y/n] " answer; [[ -z $answer || $answer =~ ^[Yy] ]]; }

printf 'wolf3d-portable guided Linux build\n'
if ready git; then
    if git -C "$root" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
        status=$(git -C "$root" submodule status --recursive 2>&1) || {
            printf 'Git could not inspect submodules:\n%s\n' "$status" >&2; exit 2;
        }
        if [[ $status == *$'\n-'* || $status == -* ]] ||
           [ ! -f "$root/lib/wolf3d/CMakeLists.txt" ] ||
           [ ! -f "$root/third_party/SDL3/CMakeLists.txt" ]; then
            printf 'Required recorded Git dependencies are not initialized.\n'
            if confirm 'Initialize the recorded submodule revisions now?'; then
                git -C "$root" submodule update --init --recursive
            else
                printf 'Cannot build until required submodules are initialized.\n' >&2; exit 2
            fi
        else
            printf 'Git dependencies: initialized and usable.\n'
        fi
        if printf '%s\n' "$status" | grep -q '^+'; then
            printf 'Using the selected component versions (including any accepted engine update).\n' >&2
        fi
    elif [ -f "$root/lib/wolf3d/CMakeLists.txt" ] && [ -f "$root/third_party/SDL3/CMakeLists.txt" ]; then
        printf 'Source export dependencies: present (Git metadata is unavailable).\n'
    else
        printf 'Required dependencies are absent and this source export has no Git metadata.\n' >&2; exit 2
    fi
elif [ ! -f "$root/lib/wolf3d/CMakeLists.txt" ] || [ ! -f "$root/third_party/SDL3/CMakeLists.txt" ]; then
    printf 'Required submodules are absent and Git was not found.\n' >&2; exit 2
else
    printf 'Warning: Git was not found; existing dependencies cannot be verified.\n' >&2
fi

printf 'Scanning supported compilers and build tools...\n\n'
for tool in gcc clang cmake make docker git; do
    if ready "$tool"; then printf '  %-8s ready - %s\n' "$tool" "$(command -v "$tool")"
    else printf '  %-8s not found\n' "$tool"; fi
done

# Select dimensions instead of listing every platform/backend combination.
case "$(uname -m)" in x86_64|amd64) host_arch=x64 ;; i?86) host_arch=x86 ;; *) host_arch=$(uname -m) ;; esac
families=(); family_targets=()
if ready cmake && ready make; then
    families+=("Build for this computer ($host_arch detected)"); family_targets+=(local)
fi
if ready docker && ready make; then
    families+=('Linux distributions (Docker)' 'Windows distributions (Docker)' 'DOS32 x86 VGA distribution (Docker)')
    family_targets+=(linux windows dos)
fi
if ready make; then families+=('Clean local build outputs'); family_targets+=(clean); fi
if [ ${#families[@]} -eq 0 ]; then echo 'No usable build tools detected.' >&2; exit 2; fi
family=$(choose 'What would you like to produce?' "${families[@]}")
for ((i=0; i<${#families[@]}; ++i)); do [ "$family" = "${families[$i]}" ] && family=${family_targets[$i]}; done
linux_arch=x64
video_backends=
case "$family" in
    local)
        backend=$(choose 'Compiled video backends (one executable)' both SDL3 KMS/fbdev)
        case "$backend" in SDL3) target=sdl3-release ;; KMS/fbdev) target=console-release ;; both) target=releases ;; esac
        label="Linux $host_arch $backend (local toolchain)" ;;
    linux)
        profile=$(choose 'Linux compatibility profile' 'glibc 2.28 portable' 'musl relocatable (x64 only)' 'Red Hat 9 i586 SDL 1.2')
        if [[ $profile == glibc* ]]; then
            arch=$(choose 'Target architecture' 'x64 (64-bit Intel/AMD)' 'x86 legacy (Pentium Pro/i686; no SSE)')
            [[ $arch == x86* ]] && linux_arch=x86
        fi
        if [[ $profile == 'Red Hat'* ]]; then
            backend='SDL 1.2'; target=rh9-sdl1; linux_arch=x86
        else
        backend=$(choose 'Compiled video backends (one executable)' both SDL3 KMS/fbdev)
        if [[ $profile == glibc* ]]; then
            case "$backend" in SDL3) target=portable-sdl3 ;; KMS/fbdev) target=portable-console ;; both) target=portable ;; esac
        else
            case "$backend" in SDL3) target=musl-sdl3 ;; KMS/fbdev) target=musl-console ;; both) target=musl-all ;; esac
        fi
        fi
        label="Linux $profile $linux_arch $backend (Docker)" ;;
    windows)
        os=$(choose 'Windows compatibility target' win7 win10 win2k winxp win9x 'all supported profiles')
        case "$os" in
            win7)
                toolchain=$(choose 'Windows toolchain (x86 + x64; GDI + SDL3)' 'MinGW GCC/MSVCRT' 'LLVM-MinGW/MSVCRT')
                case "$toolchain" in MinGW*) target=windows-win7 ;; *) target=windows-llvm-win7 ;; esac ;;
            win10) target=windows-win10 ;;
            win2k) target=windows-win2k ;;
            winxp) target=windows-xp ;;
            win9x) target=windows-win9x ;;
            *) target=windows-cross ;;
        esac
        if [[ $os != 'all supported profiles'  ]]; then
            if [[ $os == win2k || $os == winxp || $os == win9x ]]; then sdl=sdl1; else sdl=sdl3; fi
            video_backends=$(choose 'Compiled video backends (one executable)' "gdi,$sdl" gdi "$sdl")
        fi
        label="Windows $os (Docker: $target)" ;;
    dos) target=dos-release; label='DOS32 x86 VGA Open Watcom (Docker)' ;;
    clean) target=clean; label='Clean local build outputs' ;;
esac

compiler=gcc
if [[ $target != portable* && $target != musl-* && $target != rh9-* && $target != dos-* && $target != windows-* && $target != clean ]]; then
    compilers=()
    ready gcc && compilers+=(gcc)
    ready clang && compilers+=(clang)
    [ ${#compilers[@]} -gt 0 ] || { printf 'No supported C compiler was found.\n' >&2; exit 2; }
    compiler=$(choose 'C compiler' "${compilers[@]}")
fi

system_sdl=OFF
if [[ $target == sdl3-release || $target == releases ]]; then
    source=$(choose 'SDL3 source' 'pinned submodule (most reproducible)' 'system SDL3 >= 3.2')
    [ "$source" = 'system SDL3 >= 3.2' ] && system_sdl=ON
fi
default_opl=auto
if [[ $target == dos-* || $target == windows-win9x || $target == windows-win2k || $target == windows-xp || ( $target == portable* && $linux_arch == x86 ) ]]; then
    drivers=dbopl-silent-adlib
elif [[ $target == rh9-* ]]; then
    drivers=dbopl-silent-adlib
elif [[ $target == windows-* ]]; then
    drivers=nuked-dbopl-silent
else
    drivers=nuked-dbopl-silent-adlib
fi
if [[ $target != clean ]]; then
    audio=$(choose 'Audio options' "Use recommended settings ($drivers; auto sample rate)" 'Customize compiled drivers and default')
    if [[ $audio == Customize* ]]; then
        if [[ $target == rh9-* ]]; then
            drivers=$(choose 'Compiled OPL drivers' dbopl-silent-adlib nuked-dbopl-silent-adlib nuked-silent dbopl-silent adlib-silent silent)
        else
        drivers=$(choose 'Compiled OPL drivers' "$drivers" nuked-dbopl-silent-adlib nuked-dbopl-silent nuked-silent dbopl-silent adlib-silent silent)
        fi
        IFS=- read -r -a defaults <<< "$drivers"
        default_opl=$(choose 'Default OPL driver' auto "${defaults[@]}")
    fi
fi
opl_drivers=${drivers//-/,}
jobs=''
if [[ $target != dos-* && $target != windows-win9x && $target != clean ]]; then
    read -r -p 'Parallel jobs (blank lets the build tool decide): ' jobs
fi
if [[ $target == dos-* ]]; then
    compiler='Open Watcom 2 (2026-10-01)'
    args=("$target" "DOS_OPL_DRIVERS=$opl_drivers" \
          "DOS_OPL_DEFAULT=$default_opl")
elif [[ $target == windows-win9x ]]; then
    compiler='Open Watcom 2 (2026-10-01)'
    args=("$target" "OPL_DRIVERS=$opl_drivers" \
          "OPL_DEFAULT=$default_opl")
else
    args=("$target" "CC=$compiler" "USE_SYSTEM_SDL3=$system_sdl" \
          "OPL_DRIVERS=$opl_drivers" "OPL_DEFAULT=$default_opl")
fi
if [[ $target == windows-cross ]]; then
    args+=("WIN9X_OPL_DRIVERS=dbopl,silent,adlib" "WIN9X_OPL_DEFAULT=auto"
           "WIN2K_OPL_DRIVERS=dbopl,silent,adlib" "WIN2K_OPL_DEFAULT=auto"
           "WINXP_OPL_DRIVERS=dbopl,silent,adlib" "WINXP_OPL_DEFAULT=auto")
fi
if [[ $target == windows-win2k ]]; then
    args+=("WIN2K_OPL_DRIVERS=$opl_drivers" "WIN2K_OPL_DEFAULT=$default_opl")
fi
if [[ $target == windows-xp ]]; then
    args+=("WINXP_OPL_DRIVERS=$opl_drivers" "WINXP_OPL_DEFAULT=$default_opl")
fi
if [[ $target == rh9-* ]]; then
    args=("$target" "RH9_NUKED=$(case $drivers in *nuked*) echo ON;; *) echo OFF;; esac)"
          "RH9_DBOPL=$(case $drivers in *dbopl*) echo ON;; *) echo OFF;; esac)" "RH9_ADLIB=$(case $drivers in *adlib*) echo ON;; *) echo OFF;; esac)" "RH9_OPL_DEFAULT=$default_opl")
fi
[[ $target == portable* ]] && args+=("LINUX_ARCH=$linux_arch")
[ -n "$video_backends" ] && args+=("VIDEO_BACKENDS=$video_backends")
[ -n "$jobs" ] && args+=("JOBS=$jobs")

compiler_display=$compiler
if [[ $target == windows-* && $target != windows-win9x ]]; then
    compiler_display='selected Docker profile (see package above)'
fi
printf '\nBuild plan:\n  Package:     %s\n  Compiler:    %s\n  SDL3:        %s\n  OPL drivers: %s (default: %s)\n  Sample rate: runtime auto\n' "$label" "$compiler_display" "$system_sdl" "$opl_drivers" "$default_opl"
printf '\nReproducible command:\n  ./scripts/linux/invoke-build.sh'
printf ' %q' "${args[@]}"
printf '\n\n'
if ! confirm 'Run this build now?'; then exit 0; fi
exec "$executor" "${args[@]}"
