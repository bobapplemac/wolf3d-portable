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
    if(NOT WIN32 OR (NOT W3P_BUILD_WIN32 AND NOT W3P_BUILD_SDL3))
        message(FATAL_ERROR "InpOut requires a Windows GDI or SDL3 host")
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
    add_executable(wg-pcm-queue-trace-test tests/WG_PCM_QUEUE_TRACE_TEST.c)
    add_test(NAME pcm-queue-trace COMMAND wg-pcm-queue-trace-test)
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

if(W3P_BUILD_WIN32)
    add_executable(wolf3d-win32 WIN32
        platforms/WG_HELP.c platforms/WG_TEXT_OUTPUT.c
        platforms/win32/WG_WIN32.c)
    w3p_windows_opl(wolf3d-win32)
    w3p_configure_host(wolf3d-win32)
    # Windows 2000 cannot attach a GUI process to its parent's console.
    # Inherit it at process creation for CLI output and the B800 exit card.
    if(WG_DIST_PLATFORM STREQUAL "win2k")
        set_target_properties(wolf3d-win32 PROPERTIES WIN32_EXECUTABLE FALSE)
        target_compile_definitions(wolf3d-win32 PRIVATE WG_CONSOLE_WIN32=1)
    endif()
    target_compile_definitions(wolf3d-win32 PRIVATE
        WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
    if(MINGW)
        target_link_options(wolf3d-win32 PRIVATE -municode)
    endif()
    target_link_libraries(wolf3d-win32 PRIVATE user32 gdi32 winmm shell32)
    set_target_properties(wolf3d-win32 PROPERTIES OUTPUT_NAME "wolf3d")
    w3p_copy_engine_runtime(wolf3d-win32)

    wg_distribution(W3P_WIN32_RELEASE_DIR wolf3d-portable "${PROJECT_VERSION}" "gdi" "${CMAKE_CURRENT_SOURCE_DIR}" "${W3P_DIST_ROOT}")
    set(W3P_WIN32_OPL_COMMANDS "")
    set(W3P_WIN32_RELEASE_DEPENDS wolf3d-win32 wolf3d)
    if(TARGET nuked-opl3)
        list(APPEND W3P_WIN32_OPL_COMMANDS
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:nuked-opl3>"
                "${W3P_WIN32_RELEASE_DIR}/$<TARGET_FILE_NAME:nuked-opl3>"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${W3P_WOLF3D_SOURCE_DIR}/third_party/Nuked-OPL3/LICENSE"
                "${W3P_WIN32_RELEASE_DIR}/DOCS/LICENSES/LGPL-21.TXT")
        list(APPEND W3P_WIN32_RELEASE_DEPENDS nuked-opl3)
    endif()
    if(WG_ENABLE_OPL_DBOPL)
        list(APPEND W3P_WIN32_OPL_COMMANDS
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${W3P_WOLF3D_SOURCE_DIR}/third_party/DBOPL/README.wolf3d-lib.md"
                "${W3P_WIN32_RELEASE_DIR}/DOCS/DBOPL.TXT")
    endif()
    add_custom_target(win32-release
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${W3P_WIN32_RELEASE_DIR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${W3P_WIN32_RELEASE_DIR}/DOCS/LICENSES"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${W3P_WIN32_RELEASE_DIR_INFO}" "${W3P_WIN32_RELEASE_DIR}/DOCS/BUILD.TXT"
        COMMAND ${CMAKE_COMMAND} "-DINFO=${W3P_WIN32_RELEASE_DIR}/DOCS/BUILD.TXT"
            "-DROOT=${CMAKE_CURRENT_SOURCE_DIR}" "-DENGINE=${CMAKE_CURRENT_SOURCE_DIR}/lib/wolf3d"
            "-DCACHE=${CMAKE_BINARY_DIR}/CMakeCache.txt"
            "-DSETTINGS=${CMAKE_CURRENT_BINARY_DIR}/distribution-target-settings.txt"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/WGStampBuildInfo.cmake"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${W3P_WIN32_RELEASE_DIR}/DOCS/LICENSES"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:wolf3d-win32>" "${W3P_WIN32_RELEASE_DIR}/wolf3d.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:wolf3d>" "${W3P_WIN32_RELEASE_DIR}/$<TARGET_FILE_NAME:wolf3d>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/LICENSE" "${W3P_WIN32_RELEASE_DIR}/DOCS/LICENSES/GPL-2.TXT"
        ${W3P_WIN32_OPL_COMMANDS}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_BINARY_DIR}/WIN32-README.txt" "${W3P_WIN32_RELEASE_DIR}/README.TXT"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_BINARY_DIR}/WOLF3D-LIB.txt" "${W3P_WIN32_RELEASE_DIR}/DOCS/ENGINE.TXT"
        COMMAND ${CMAKE_COMMAND} "-DDIST=${W3P_WIN32_RELEASE_DIR}" "-DROOT=${CMAKE_SOURCE_DIR}" "-DPRODUCT=wolf3d-portable"
            -P "${CMAKE_SOURCE_DIR}/cmake/WGPackageDocs.cmake"
        COMMAND ${CMAKE_COMMAND} "-DDIST=${W3P_WIN32_RELEASE_DIR}"
            "-DROOT=${CMAKE_SOURCE_DIR}" "-DENABLED=${W3P_ENABLE_INPOUT32}"
            "-DADLIB=${WG_ENABLE_OPL_ADLIB}"
            -P "${CMAKE_SOURCE_DIR}/cmake/W3PPackageInpOut32.cmake"
        DEPENDS ${W3P_WIN32_RELEASE_DEPENDS}
        COMMENT "Staging ${W3P_WIN32_RELEASE_DIR_ARCH} GDI release" VERBATIM)
endif()

if(W3P_BUILD_SDL3)
    if(WIN32)
        add_executable(wolf3d-sdl3 WIN32
            platforms/WG_HELP.c platforms/WG_TEXT_OUTPUT.c
            platforms/sdl3/WG_SDL3.c)
    else()
        add_executable(wolf3d-sdl3
            platforms/WG_HELP.c platforms/WG_TEXT_OUTPUT.c
            platforms/sdl3/WG_SDL3.c)
    endif()
    w3p_configure_host(wolf3d-sdl3)
    w3p_linux_opl(wolf3d-sdl3)
    w3p_windows_opl(wolf3d-sdl3)
    target_link_libraries(wolf3d-sdl3 PRIVATE SDL3::SDL3-shared)
    set_target_properties(wolf3d-sdl3 PROPERTIES OUTPUT_NAME "wolf3d-sdl3"
        BUILD_RPATH "$ORIGIN" BUILD_RPATH_USE_ORIGIN ON INSTALL_RPATH "$ORIGIN")
    if(WIN32)
        target_compile_definitions(wolf3d-sdl3 PRIVATE
            WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
        if(MINGW)
            target_link_options(wolf3d-sdl3 PRIVATE -municode)
        endif()
        w3p_copy_engine_runtime(wolf3d-sdl3)
        add_custom_command(TARGET wolf3d-sdl3 POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:SDL3::SDL3-shared>"
                "$<TARGET_FILE_DIR:wolf3d-sdl3>/$<TARGET_FILE_NAME:SDL3::SDL3-shared>"
            COMMENT "Copying SDL3 beside wolf3d-sdl3" VERBATIM)
    endif()

    wg_distribution(W3P_SDL3_RELEASE_DIR wolf3d-portable "${PROJECT_VERSION}" "sdl3" "${CMAKE_CURRENT_SOURCE_DIR}" "${W3P_DIST_ROOT}")
    set(W3P_SDL3_RELEASE_DEPENDS wolf3d-sdl3 wolf3d SDL3::SDL3-shared)
    set(W3P_SDL3_RELEASE_COMMANDS
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${W3P_SDL3_RELEASE_DIR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${W3P_SDL3_RELEASE_DIR}/DOCS/LICENSES"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${W3P_SDL3_RELEASE_DIR_INFO}" "${W3P_SDL3_RELEASE_DIR}/DOCS/BUILD.TXT"
        COMMAND ${CMAKE_COMMAND} "-DINFO=${W3P_SDL3_RELEASE_DIR}/DOCS/BUILD.TXT"
            "-DROOT=${CMAKE_CURRENT_SOURCE_DIR}" "-DENGINE=${CMAKE_CURRENT_SOURCE_DIR}/lib/wolf3d"
            "-DCACHE=${CMAKE_BINARY_DIR}/CMakeCache.txt"
            "-DSETTINGS=${CMAKE_CURRENT_BINARY_DIR}/distribution-target-settings.txt"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/WGStampBuildInfo.cmake"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${W3P_SDL3_RELEASE_DIR}/DOCS/LICENSES"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:wolf3d-sdl3>" "${W3P_SDL3_RELEASE_DIR}/wolf3d${CMAKE_EXECUTABLE_SUFFIX}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:wolf3d>" "${W3P_SDL3_RELEASE_DIR}/$<TARGET_FILE_NAME:wolf3d>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:SDL3::SDL3-shared>" "${W3P_SDL3_RELEASE_DIR}/$<TARGET_FILE_NAME:SDL3::SDL3-shared>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/LICENSE" "${W3P_SDL3_RELEASE_DIR}/DOCS/LICENSES/GPL-2.TXT"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${W3P_SDL3_LICENSE_FILE}"
            "${W3P_SDL3_RELEASE_DIR}/DOCS/LICENSES/SDL3.TXT"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_BINARY_DIR}/SDL3-README.txt" "${W3P_SDL3_RELEASE_DIR}/README.TXT"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_BINARY_DIR}/WOLF3D-LIB.txt" "${W3P_SDL3_RELEASE_DIR}/DOCS/ENGINE.TXT")
    if(TARGET nuked-opl3)
        list(APPEND W3P_SDL3_RELEASE_COMMANDS
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:nuked-opl3>"
                "${W3P_SDL3_RELEASE_DIR}/$<TARGET_FILE_NAME:nuked-opl3>"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${W3P_WOLF3D_SOURCE_DIR}/third_party/Nuked-OPL3/LICENSE"
                "${W3P_SDL3_RELEASE_DIR}/DOCS/LICENSES/LGPL-21.TXT")
        list(APPEND W3P_SDL3_RELEASE_DEPENDS nuked-opl3)
    endif()
    if(WG_ENABLE_OPL_DBOPL)
        list(APPEND W3P_SDL3_RELEASE_COMMANDS
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${W3P_WOLF3D_SOURCE_DIR}/third_party/DBOPL/README.wolf3d-lib.md"
                "${W3P_SDL3_RELEASE_DIR}/DOCS/DBOPL.TXT")
    endif()
    if(UNIX AND NOT APPLE)
        list(APPEND W3P_SDL3_RELEASE_COMMANDS
            COMMAND ${CMAKE_COMMAND} -E create_symlink
                "$<TARGET_FILE_NAME:wolf3d>" "${W3P_SDL3_RELEASE_DIR}/$<TARGET_SONAME_FILE_NAME:wolf3d>"
            COMMAND ${CMAKE_COMMAND} -E create_symlink
                "$<TARGET_SONAME_FILE_NAME:wolf3d>" "${W3P_SDL3_RELEASE_DIR}/$<TARGET_LINKER_FILE_NAME:wolf3d>"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_SONAME_FILE:SDL3::SDL3-shared>"
                "${W3P_SDL3_RELEASE_DIR}/$<TARGET_SONAME_FILE_NAME:SDL3::SDL3-shared>")
    endif()
    add_custom_target(sdl3-release ${W3P_SDL3_RELEASE_COMMANDS}
        COMMAND ${CMAKE_COMMAND} "-DDIST=${W3P_SDL3_RELEASE_DIR}" "-DROOT=${CMAKE_SOURCE_DIR}" "-DPRODUCT=wolf3d-portable"
            -P "${CMAKE_SOURCE_DIR}/cmake/WGPackageDocs.cmake"
        DEPENDS ${W3P_SDL3_RELEASE_DEPENDS}
        COMMENT "Staging ${W3P_SDL3_RELEASE_DIR_ARCH} SDL3 release" VERBATIM)
    if(BUILD_TESTING)
        add_test(NAME sdl3-help COMMAND wolf3d-sdl3 --help)
        add_executable(wg-sdl-modal-frame-test tests/WG_SDL_MODAL_FRAME_TEST.c)
        target_link_libraries(wg-sdl-modal-frame-test PRIVATE SDL3::SDL3-shared)
        if(WIN32)
            add_custom_command(TARGET wg-sdl-modal-frame-test POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:SDL3::SDL3-shared>"
                    "$<TARGET_FILE_DIR:wg-sdl-modal-frame-test>"
                VERBATIM)
        endif()
        add_test(NAME sdl3-modal-frame COMMAND wg-sdl-modal-frame-test)
    endif()
endif()

if(W3P_BUILD_LINUX_CONSOLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBDRM REQUIRED IMPORTED_TARGET libdrm)
    pkg_check_modules(ALSA REQUIRED IMPORTED_TARGET alsa)
    add_executable(wolf3d-console platforms/WG_HELP.c platforms/WG_TEXT_OUTPUT.c
        platforms/linux-console/MAIN.c platforms/linux-console/WG_LINUX_CONSOLE.c)
    w3p_configure_host(wolf3d-console)
    w3p_linux_opl(wolf3d-console)
    target_link_libraries(wolf3d-console PRIVATE PkgConfig::LIBDRM PkgConfig::ALSA)
    set_target_properties(wolf3d-console PROPERTIES OUTPUT_NAME "wolf3d"
        BUILD_RPATH "$ORIGIN" BUILD_RPATH_USE_ORIGIN ON INSTALL_RPATH "$ORIGIN")

    wg_distribution(W3P_CONSOLE_RELEASE_DIR wolf3d-portable "${PROJECT_VERSION}" "kms-fbdev" "${CMAKE_CURRENT_SOURCE_DIR}" "${W3P_DIST_ROOT}")
    set(W3P_CONSOLE_OPL_COMMANDS "")
    set(W3P_CONSOLE_RELEASE_DEPENDS wolf3d-console wolf3d)
    if(TARGET nuked-opl3)
        list(APPEND W3P_CONSOLE_OPL_COMMANDS
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:nuked-opl3>"
                "${W3P_CONSOLE_RELEASE_DIR}/$<TARGET_FILE_NAME:nuked-opl3>"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${W3P_WOLF3D_SOURCE_DIR}/third_party/Nuked-OPL3/LICENSE"
                "${W3P_CONSOLE_RELEASE_DIR}/DOCS/LICENSES/LGPL-21.TXT")
        list(APPEND W3P_CONSOLE_RELEASE_DEPENDS nuked-opl3)
    endif()
    if(WG_ENABLE_OPL_DBOPL)
        list(APPEND W3P_CONSOLE_OPL_COMMANDS
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${W3P_WOLF3D_SOURCE_DIR}/third_party/DBOPL/README.wolf3d-lib.md"
                "${W3P_CONSOLE_RELEASE_DIR}/DOCS/DBOPL.TXT")
    endif()
    add_custom_target(console-release
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${W3P_CONSOLE_RELEASE_DIR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${W3P_CONSOLE_RELEASE_DIR}/DOCS/LICENSES"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${W3P_CONSOLE_RELEASE_DIR_INFO}" "${W3P_CONSOLE_RELEASE_DIR}/DOCS/BUILD.TXT"
        COMMAND ${CMAKE_COMMAND} "-DINFO=${W3P_CONSOLE_RELEASE_DIR}/DOCS/BUILD.TXT"
            "-DROOT=${CMAKE_CURRENT_SOURCE_DIR}" "-DENGINE=${CMAKE_CURRENT_SOURCE_DIR}/lib/wolf3d"
            "-DCACHE=${CMAKE_BINARY_DIR}/CMakeCache.txt"
            "-DSETTINGS=${CMAKE_CURRENT_BINARY_DIR}/distribution-target-settings.txt"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/WGStampBuildInfo.cmake"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${W3P_CONSOLE_RELEASE_DIR}/DOCS/LICENSES"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:wolf3d-console>" "${W3P_CONSOLE_RELEASE_DIR}/wolf3d"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:wolf3d>" "${W3P_CONSOLE_RELEASE_DIR}/$<TARGET_FILE_NAME:wolf3d>"
        COMMAND ${CMAKE_COMMAND} -E create_symlink
            "$<TARGET_FILE_NAME:wolf3d>" "${W3P_CONSOLE_RELEASE_DIR}/$<TARGET_SONAME_FILE_NAME:wolf3d>"
        COMMAND ${CMAKE_COMMAND} -E create_symlink
            "$<TARGET_SONAME_FILE_NAME:wolf3d>" "${W3P_CONSOLE_RELEASE_DIR}/$<TARGET_LINKER_FILE_NAME:wolf3d>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/LICENSE" "${W3P_CONSOLE_RELEASE_DIR}/DOCS/LICENSES/GPL-2.TXT"
        ${W3P_CONSOLE_OPL_COMMANDS}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_BINARY_DIR}/LINUX-CONSOLE-README.txt" "${W3P_CONSOLE_RELEASE_DIR}/README.TXT"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_BINARY_DIR}/WOLF3D-LIB.txt" "${W3P_CONSOLE_RELEASE_DIR}/DOCS/ENGINE.TXT"
        COMMAND ${CMAKE_COMMAND} "-DDIST=${W3P_CONSOLE_RELEASE_DIR}" "-DROOT=${CMAKE_SOURCE_DIR}" "-DPRODUCT=wolf3d-portable"
            -P "${CMAKE_SOURCE_DIR}/cmake/WGPackageDocs.cmake"
        DEPENDS ${W3P_CONSOLE_RELEASE_DEPENDS}
        COMMENT "Staging ${W3P_CONSOLE_RELEASE_DIR_ARCH} KMS/fbdev release" VERBATIM)
    if(BUILD_TESTING)
        add_test(NAME console-help COMMAND wolf3d-console --help)
    endif()
endif()

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
