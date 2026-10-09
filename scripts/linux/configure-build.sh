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
            printf 'Warning: a submodule differs from its recorded revision; local dependency work was left untouched.\n' >&2
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

targets=()
labels=()
if ready cmake && ready make; then
    targets+=(sdl3-release console-release releases clean)
    labels+=('native SDL3 distribution' 'native console distribution' 'both native distributions' 'clean local build outputs')
fi
if ready docker && ready make; then
    targets+=(portable portable-sdl3 portable-console musl-sdl3 dos-release windows-win9x windows-xp windows-win7 windows-llvm-win7 windows-win10 windows-cross)
    labels+=('both portable glibc distributions (Docker)' 'portable glibc SDL3 distribution (Docker)' 'portable glibc console distribution (Docker)' 'relocatable musl SDL3 distribution (Docker)' '32-bit DOS Open Watcom distribution (Docker)' 'Windows 95 x86 Open Watcom Win32 package (Docker)' 'Windows XP x86 MinGW/MSVCRT Win32 package (Docker)' 'Windows 7 x86/x64 MinGW/MSVCRT Win32+SDL3 packages (Docker)' 'Windows 7 x86/x64 LLVM/MSVCRT Win32+SDL3 packages (Docker)' 'Windows 10 x64 LLVM/UCRT Win32+SDL3 packages (Docker)' 'all Linux-hosted Windows packages (Docker)')
fi
if [ ${#targets[@]} -eq 0 ]; then
    printf '\nNo usable build path was detected. See docs/building.md for prerequisites.\n' >&2; exit 2
fi

label=$(choose 'What would you like to produce?' "${labels[@]}")
target=''
for ((i=0; i<${#labels[@]}; ++i)); do
    [ "${labels[$i]}" = "$label" ] && target=${targets[$i]}
done

compiler=gcc
if [[ $target != portable* && $target != musl-* && $target != dos-* && $target != windows-* && $target != clean ]]; then
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
drivers=all
default_opl=nuked
sample_rate=48000
if [[ $target == dos-* || $target == windows-win9x ]]; then
    drivers=dos-default
    default_opl=adlib
    sample_rate=44100
elif [[ $target != clean ]]; then
    drivers=$(choose 'Compiled OPL drivers' all nuked-dbopl nuked-silent dbopl-silent nuked dbopl silent)
    if [ "$drivers" = all ]; then
        default_opl=$(choose 'Default OPL driver' nuked dbopl silent)
    else
        IFS=- read -r -a defaults <<< "$drivers"
        default_opl=$(choose 'Default OPL driver' "${defaults[@]}")
    fi
    sample_rate=$(choose 'Preferred PCM sample rate' 48000 44100)
fi
opl_drivers=${drivers//-/,}
[ "$drivers" = dos-default ] && opl_drivers=dbopl,silent,adlib
[ "$drivers" = all ] && opl_drivers=nuked,dbopl,silent
jobs=''
if [[ $target != dos-* && $target != windows-win9x ]]; then
    read -r -p 'Parallel jobs (blank lets the build tool decide): ' jobs
fi
if [[ $target == dos-* ]]; then
    [ "$drivers" = dos-default ] && opl_drivers=dbopl,silent,adlib
    compiler='Open Watcom 2 (2026-10-01)'
    args=("$target" "DOS_OPL_DRIVERS=$opl_drivers" \
          "DOS_OPL_DEFAULT=$default_opl" "DOS_SAMPLE_RATE=$sample_rate")
elif [[ $target == windows-win9x ]]; then
    compiler='Open Watcom 2 (2026-10-01)'
    args=("$target" "OPL_DRIVERS=$opl_drivers" \
          "OPL_DEFAULT=$default_opl" "SAMPLE_RATE=$sample_rate")
else
    args=("$target" "CC=$compiler" "USE_SYSTEM_SDL3=$system_sdl" \
          "OPL_DRIVERS=$opl_drivers" "OPL_DEFAULT=$default_opl" \
          "SAMPLE_RATE=$sample_rate")
fi
if [[ $target == windows-cross ]]; then
    args+=("WIN9X_OPL_DRIVERS=dbopl,silent,adlib" "WIN9X_OPL_DEFAULT=adlib")
fi
[ -n "$jobs" ] && args+=("JOBS=$jobs")

printf '\nBuild plan:\n  Target:      %s\n  Compiler:    %s\n  SDL3:        %s\n  OPL drivers: %s (default: %s)\n  Sample rate: %s Hz\n' "$target" "$compiler" "$system_sdl" "$opl_drivers" "$default_opl" "$sample_rate"
printf '\nReproducible command:\n  ./scripts/linux/invoke-build.sh'
printf ' %q' "${args[@]}"
printf '\n\n'
if ! confirm 'Run this build now?'; then exit 0; fi
exec "$executor" "${args[@]}"
