cmake_minimum_required(VERSION 3.16)

if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

set(W3P_WOLF3D_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lib/wolf3d")
if(NOT EXISTS "${W3P_WOLF3D_SOURCE_DIR}/VERSION")
    message(FATAL_ERROR
        "wolf3d-lib is not initialized; run git submodule update --init --recursive")
endif()
file(STRINGS "${W3P_WOLF3D_SOURCE_DIR}/VERSION" W3P_WOLF3D_VERSION
    LIMIT_COUNT 1)
if(NOT W3P_WOLF3D_VERSION MATCHES "^1\\.4\\.[0-9]+$")
    message(FATAL_ERROR "Invalid wolf3d-lib version '${W3P_WOLF3D_VERSION}'")
endif()

include(cmake/W3PCheckEngineVersion.cmake)
w3p_check_engine_version("${W3P_WOLF3D_VERSION}")

project(wolf3d-portable VERSION "${W3P_WOLF3D_VERSION}" LANGUAGES C)
include(CTest)

option(W3P_ENABLE_INPOUT32 "Enable optional x86/x64 Windows native AdLib via InpOut" OFF)
option(W3P_BUILD_WIN32 "Build the dependency-free Win32 host" ON)
option(W3P_BUILD_LINUX_CONSOLE
    "Build the direct DRM/fbdev/evdev/ALSA Linux console host" OFF)
option(W3P_BUILD_SDL3 "Build the cross-platform SDL3 GUI host" OFF)
option(W3P_BUILD_SDL1 "Include the legacy SDL 1.2 host" OFF)
set(W3P_VIDEO_BACKENDS "" CACHE STRING "Compiled video backends: comma-separated gdi,sdl1,sdl3,kms-fbdev; empty uses host switches")
if(W3P_VIDEO_BACKENDS)
    foreach(flag WIN32 SDL1 SDL3 LINUX_CONSOLE)
        set(W3P_BUILD_${flag} OFF CACHE BOOL "Compiled video host" FORCE)
    endforeach()
    string(REPLACE "," ";" requested_backends "${W3P_VIDEO_BACKENDS}")
    foreach(backend IN LISTS requested_backends)
        if(backend STREQUAL "gdi" AND WIN32)
            set(W3P_BUILD_WIN32 ON CACHE BOOL "Compiled video host" FORCE)
        elseif(backend STREQUAL "sdl1" AND WIN32)
            set(W3P_BUILD_SDL1 ON CACHE BOOL "Compiled video host" FORCE)
        elseif(backend STREQUAL "sdl3")
            set(W3P_BUILD_SDL3 ON CACHE BOOL "Compiled video host" FORCE)
        elseif(backend STREQUAL "kms-fbdev" AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
            set(W3P_BUILD_LINUX_CONSOLE ON CACHE BOOL "Compiled video host" FORCE)
        else()
            message(FATAL_ERROR "Unsupported video backend for this target: ${backend}")
        endif()
    endforeach()
endif()
if(W3P_BUILD_SDL1 AND W3P_BUILD_SDL3)
    message(FATAL_ERROR "Select SDL1 or SDL3, never both")
endif()
if(W3P_BUILD_SDL3 AND (WG_DIST_PLATFORM MATCHES "^(win9x|win2k|winxp)$" OR CMAKE_VS_PLATFORM_TOOLSET MATCHES "_xp$" OR (MSVC AND MSVC_VERSION LESS 1920)))
    message(FATAL_ERROR "Legacy Windows targets require SDL1 instead of SDL3")
endif()
if(W3P_BUILD_SDL1 AND NOT WIN32)
    message(FATAL_ERROR "Use the dedicated RH9 profile for legacy Linux SDL1")
endif()
option(W3P_USE_SYSTEM_SDL3
    "Use an installed SDL3 package instead of the pinned submodule" OFF)
option(W3P_WARNINGS_AS_ERRORS "Treat wrapper warnings as errors" ON)
option(WG_STATIC_MSVC_RUNTIME
    "Link the MSVC runtime statically for self-contained Windows releases" ON)
set(W3P_DIST_ROOT "${CMAKE_SOURCE_DIR}/dist" CACHE PATH
    "Directory used by release staging targets")
set(W3P_COMPILER_LABEL "" CACHE STRING
    "Optional compiler/toolset label appended to staged package names")

if(NOT WIN32)
    set(W3P_BUILD_WIN32 OFF CACHE BOOL "Win32 is Windows-only" FORCE)
endif()
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(W3P_BUILD_LINUX_CONSOLE OFF CACHE BOOL
        "The console host is Linux-only" FORCE)
endif()
if(MSVC AND WG_STATIC_MSVC_RUNTIME)
    set(CMAKE_MSVC_RUNTIME_LIBRARY
        "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# The submodule supplies the engine and its configured audio drivers.
# Its internal deterministic oracle is not part of production host builds.
set(WG_BUILD_HEADLESS OFF CACHE BOOL "" FORCE)
set(WG_WARNINGS_AS_ERRORS ${W3P_WARNINGS_AS_ERRORS} CACHE BOOL "" FORCE)
set(WG_DIST_ROOT "${CMAKE_BINARY_DIR}/wolf3d-lib-dist" CACHE PATH "" FORCE)
if(W3P_ENABLE_INPOUT32)
    if(NOT WIN32 OR (NOT W3P_BUILD_WIN32 AND NOT W3P_BUILD_SDL3 AND NOT W3P_BUILD_SDL1))
        message(FATAL_ERROR "InpOut requires a Windows GDI or SDL host")
    endif()
    set(WG_ENABLE_OPL_ADLIB ON CACHE BOOL "Native AdLib adapter" FORCE)
endif()
# Native ISA FM is supported on Linux x86 only. Respect explicit OFF choices.
set(W3P_LINUX_OPL_SUPPORTED OFF)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64|i[3-6]86|x86)$")
    set(W3P_LINUX_OPL_SUPPORTED ON)
    set(WG_ENABLE_OPL_ADLIB ON CACHE BOOL "Compile the native AdLib adapter")
endif()
add_subdirectory(lib/wolf3d wolf3d-lib)

function(w3p_windows_opl target)
    if(W3P_ENABLE_INPOUT32)
        target_sources(${target} PRIVATE
            platforms/dos/WG_DOS_ADLIB.c platforms/win32/WG_INPOUT32.c
            platforms/win32/WG_OPL_PLAYBACK.c)
        target_compile_definitions(${target} PRIVATE WG_INPOUT32=1)
        target_link_libraries(${target} PRIVATE winmm)
    endif()
endfunction()

function(w3p_linux_opl target)
    if(W3P_LINUX_OPL_SUPPORTED AND WG_ENABLE_OPL_ADLIB)
        find_package(Threads REQUIRED)
        target_sources(${target} PRIVATE platforms/linux/WG_LINUX_OPL.c)
        target_compile_definitions(${target} PRIVATE WG_LINUX_OPL=1)
        target_link_libraries(${target} PRIVATE Threads::Threads)
    endif()
endfunction()
if(BUILD_TESTING)
    if(WIN32)
        add_executable(wg-sdl1-winkeys-test tests/WG_SDL1_WINKEYS_TEST.c)
        add_test(NAME sdl1-win9x-keys COMMAND wg-sdl1-winkeys-test)
        add_executable(wg-console-pause-test tests/WG_CONSOLE_PAUSE_TEST.c)
        target_link_libraries(wg-console-pause-test PRIVATE user32)
        add_test(NAME console-pause COMMAND wg-console-pause-test)
        add_executable(wg-waveout-test tests/WG_WAVEOUT_TEST.c)
        add_test(NAME shared-waveout COMMAND wg-waveout-test)
        add_executable(wg-gdi-presenter-test tests/WG_GDI_PRESENTER_TEST.c)
        target_link_libraries(wg-gdi-presenter-test PRIVATE user32 gdi32)
        add_test(NAME gdi-presenter COMMAND wg-gdi-presenter-test)
        add_executable(wg-sdl1-windows-test tests/WG_SDL1_WINDOWS_TEST.c)
        add_test(NAME sdl1-windows-mailbox COMMAND wg-sdl1-windows-test)
        add_executable(wg-opl-playback-test tests/WG_OPL_PLAYBACK_TEST.c)
        target_link_libraries(wg-opl-playback-test PRIVATE winmm)
        add_test(NAME native-opl-playback COMMAND wg-opl-playback-test)
        add_executable(wg-opl-playback-win9x-test tests/WG_OPL_PLAYBACK_TEST.c)
        target_compile_definitions(wg-opl-playback-win9x-test PRIVATE WG_WIN9X=1)
        target_link_libraries(wg-opl-playback-win9x-test PRIVATE winmm)
        add_test(NAME native-opl-playback-win9x COMMAND wg-opl-playback-win9x-test)
    endif()
endif()
if(BUILD_TESTING AND W3P_LINUX_OPL_SUPPORTED)
    find_package(Threads REQUIRED)
    add_executable(wg-linux-opl-test tests/WG_LINUX_OPL_TEST.c)
    target_include_directories(wg-linux-opl-test PRIVATE lib/wolf3d/include)
    target_link_libraries(wg-linux-opl-test PRIVATE Threads::Threads)
    target_compile_options(wg-linux-opl-test PRIVATE -Wall -Wextra -Werror)
    add_test(NAME linux-native-opl COMMAND wg-linux-opl-test)
    add_executable(wg-opl-timeline-test tests/WG_OPL_TIMELINE_TEST.c)
    add_test(NAME native-opl-timeline COMMAND wg-opl-timeline-test)
endif()

find_package(Git QUIET)
set(W3P_PORTABLE_COMMIT "unknown")
set(W3P_WOLF3D_COMMIT "unknown")
if(Git_FOUND)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${CMAKE_CURRENT_SOURCE_DIR}"
            rev-parse HEAD
        OUTPUT_VARIABLE W3P_PORTABLE_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${W3P_WOLF3D_SOURCE_DIR}"
            rev-parse HEAD
        OUTPUT_VARIABLE W3P_WOLF3D_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
endif()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/WOLF3D-LIB.txt"
    "wolf3d-portable commit: ${W3P_PORTABLE_COMMIT}\n"
    "wolf3d-lib version: ${W3P_WOLF3D_VERSION}\n"
    "wolf3d-lib commit: ${W3P_WOLF3D_COMMIT}\n"
    "OPL drivers: nuked=${WG_ENABLE_OPL_NUKED}, dbopl=${WG_ENABLE_OPL_DBOPL}, silent=${WG_ENABLE_OPL_SILENT}, adlib=${WG_ENABLE_OPL_ADLIB}\n"
    "default OPL driver: ${WG_DEFAULT_OPL_DRIVER}\n"
    "sample rate: runtime auto\n")

if(TARGET nuked-opl3)
    set(W3P_OPL_FILE_LINE
        "  Nuked-OPL3.dll          Replaceable LGPL-2.1-or-later OPL emulator")
    set(W3P_OPL_KEEP_NOTE
        "wolf3d.dll and Nuked-OPL3.dll must remain beside the executable. The\nWin32 executable is only a platform host and loads the engine from the first\nDLL. Nuked-OPL3.dll may be replaced with a compatible modified build, as\nprovided for by its GNU Lesser General Public License. Corresponding source is\nidentified in the source distribution and is included in the wolf3d-lib\nsubmodule.")
else()
    set(W3P_OPL_FILE_LINE
        "  (Selected OPL drivers are compiled into wolf3d.dll)")
    set(W3P_OPL_KEEP_NOTE
        "wolf3d.dll must remain beside the executable. Audio-driver details\nare recorded in DOCS/BUILD.TXT and the source distribution.")
endif()
set(W3P_NATIVE_OPL_NOTE "")
if(W3P_ENABLE_INPOUT32)
    set(W3P_NATIVE_OPL_NOTE
        "Optional native AdLib: use --opl adlib. Auto is the default.\nThe win2k/winxp x86 GDI native-AdLib distributions bundle inpout32.dll.\nOther profiles do not bundle a driver: supply Highrez inpout32.dll for x86\nor inpoutx64.dll for x64 beside wolf3d.exe (match application bitness).\nModern Windows security may block the kernel driver. First use may install/start its embedded kernel driver;\nrun as Administrator on the destination computer for initial installation.\nInstructions: https://vps.highrez.co.uk/downloads/InpOut32/default.htm\nFirst run: wolf3d.exe --no-config --diag --opl adlib\nClose other FM/MIDI playback applications. Digital effects use the host PCM audio backend.\n")
endif()
set(W3P_LINUX_OPL_NOTE "")
if(W3P_LINUX_OPL_SUPPORTED AND WG_ENABLE_OPL_ADLIB)
    set(W3P_LINUX_OPL_NOTE "Linux native AdLib: built and mock-tested; physical playback confirmation pending.\nThe chip must be initialized at 0x388, with its FM mixer route enabled.\nPort access requires CAP_SYS_RAWIO; auto quietly falls back if unavailable.\nPCM remains on ALSA/SDL. Do not run another FM player concurrently.\nSetup: https://github.com/bobapplemac/wolf3d-portable/blob/main/docs/native-opl.md\n")
endif()
configure_file(packaging/WIN32-README.txt.in
               ${CMAKE_CURRENT_BINARY_DIR}/WIN32-README.txt @ONLY)
configure_file(packaging/LINUX-CONSOLE-README.txt.in
               ${CMAKE_CURRENT_BINARY_DIR}/LINUX-CONSOLE-README.txt @ONLY)
configure_file(packaging/SDL3-README.txt.in
               ${CMAKE_CURRENT_BINARY_DIR}/SDL3-README.txt @ONLY)

if(W3P_BUILD_SDL3)
    if(W3P_USE_SYSTEM_SDL3)
        find_package(SDL3 3.2 REQUIRED CONFIG COMPONENTS SDL3-shared)
        set(W3P_SDL3_LICENSE_FILE
            "${CMAKE_SOURCE_DIR}/packaging/COPYING.SDL3.txt")
    else()
        if(NOT EXISTS "${CMAKE_SOURCE_DIR}/third_party/SDL3/CMakeLists.txt")
            message(FATAL_ERROR
                "SDL3 is missing; initialize submodules or set W3P_USE_SYSTEM_SDL3=ON")
        endif()
        set(SDL_SHARED ON CACHE BOOL "" FORCE)
        set(SDL_STATIC OFF CACHE BOOL "" FORCE)
        set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
        set(SDL_TESTS OFF CACHE BOOL "" FORCE)
        set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
        set(SDL_RENDER_D3D11 OFF CACHE BOOL "" FORCE)
        add_subdirectory(third_party/SDL3 EXCLUDE_FROM_ALL)
        set(W3P_SDL3_LICENSE_FILE
            "${CMAKE_SOURCE_DIR}/third_party/SDL3/LICENSE.txt")
    endif()
endif()

include(cmake/WGDistribution.cmake)

function(w3p_configure_host target)
    target_link_libraries(${target} PRIVATE wolf3d::wolf3d)
    target_compile_features(${target} PRIVATE c_std_99)
    if(MINGW AND WG_STATIC_GNU_RUNTIME)
        target_link_options(${target} PRIVATE -static-libgcc)
    endif()
    if(MSVC)
        if(MSVC_VERSION LESS 1800)
            target_include_directories(${target} PRIVATE
                "${W3P_WOLF3D_SOURCE_DIR}/compat/msvc")
        endif()
        target_compile_options(${target} PRIVATE /W4)
        if(MSVC_VERSION LESS 1600)
            # Windows SDK 6.0A mmsystem.h uses anonymous unions.
            target_compile_options(${target} PRIVATE /wd4201)
        endif()
        if(MSVC_VERSION LESS 1900)
            target_compile_definitions(${target} PRIVATE inline=__inline)
        endif()
        if(MSVC_VERSION GREATER_EQUAL 1910)
            target_compile_options(${target} PRIVATE /permissive-)
        endif()
        if(W3P_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        if(W3P_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

function(w3p_copy_engine_runtime target)
    if(WIN32)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:wolf3d>"
                "$<TARGET_FILE_DIR:${target}>/$<TARGET_FILE_NAME:wolf3d>"
            COMMENT "Copying Wolf3D runtime libraries beside ${target}"
            VERBATIM)
        if(TARGET nuked-opl3)
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:nuked-opl3>"
                    "$<TARGET_FILE_DIR:${target}>/$<TARGET_FILE_NAME:nuked-opl3>"
                VERBATIM)
        endif()
    endif()
endfunction()

include(cmake/W3PBackends.cmake)

if(BUILD_TESTING)
    add_executable(wg-discovery-probe
        tests/WG_DISCOVERY_PROBE.c platforms/WG_HELP.c)
    w3p_configure_host(wg-discovery-probe)
    add_test(NAME portable-data-discovery
        COMMAND ${CMAKE_COMMAND}
            "-DPROBE=$<TARGET_FILE:wg-discovery-probe>"
            "-DROOT=${CMAKE_CURRENT_BINARY_DIR}/discovery-fixture"
            "-DCONFIG_EXTENSION=$<IF:$<BOOL:${WIN32}>,ini,conf>"
            "-DOPL_TEST_DRIVER=$<IF:$<BOOL:${WG_ENABLE_OPL_DBOPL}>,dbopl,silent>"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/WG_DISCOVERY_TEST.cmake")
endif()

wg_distribution_target_info("${CMAKE_CURRENT_SOURCE_DIR}")
