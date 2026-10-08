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

project(wolf3d-portable VERSION "${W3P_WOLF3D_VERSION}" LANGUAGES C)
include(CTest)

option(W3P_BUILD_WIN32 "Build the dependency-free Win32 host" ON)
option(W3P_BUILD_LINUX_CONSOLE
    "Build the direct DRM/evdev/ALSA Linux console host" OFF)
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
add_subdirectory(lib/wolf3d wolf3d-lib)

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
    "OPL drivers: nuked=${WG_ENABLE_OPL_NUKED}, dbopl=${WG_ENABLE_OPL_DBOPL}, silent=${WG_ENABLE_OPL_SILENT}\n"
    "default OPL driver: ${WG_DEFAULT_OPL_DRIVER}\n"
    "preferred sample rate: ${WG_DEFAULT_SAMPLE_RATE} Hz\n")

if(TARGET nuked-opl3)
    set(W3P_OPL_FILE_LINE
        "  Nuked-OPL3.dll          Replaceable LGPL-2.1-or-later OPL emulator")
    set(W3P_OPL_KEEP_NOTE
        "wolf3d.dll and Nuked-OPL3.dll must remain beside the executable. The\nWin32 executable is only a platform host and loads the engine from the first\nDLL. Nuked-OPL3.dll may be replaced with a compatible modified build, as\nprovided for by its GNU Lesser General Public License. Corresponding source is\nidentified in the source distribution and is included in the wolf3d-lib\nsubmodule.")
else()
    set(W3P_OPL_FILE_LINE
        "  (Selected OPL drivers are compiled into wolf3d.dll)")
    set(W3P_OPL_KEEP_NOTE
        "wolf3d.dll must remain beside the executable. Audio-driver details\nare recorded in WOLF3D-LIB.txt and the source distribution.")
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

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(W3P_RELEASE_ARCH x64)
else()
    set(W3P_RELEASE_ARCH x86)
endif()
string(TOLOWER "${CMAKE_SYSTEM_NAME}" W3P_RELEASE_SYSTEM)
set(W3P_RELEASE_RUNTIME_SUFFIX "")
if(MSVC AND NOT WG_STATIC_MSVC_RUNTIME)
    set(W3P_RELEASE_RUNTIME_SUFFIX "-dynamic-crt")
elseif(MINGW AND NOT WG_STATIC_GNU_RUNTIME)
    set(W3P_RELEASE_RUNTIME_SUFFIX "-dynamic-gcc-runtime")
endif()
set(W3P_RELEASE_COMPILER_SUFFIX "")
if(W3P_COMPILER_LABEL)
    set(W3P_RELEASE_COMPILER_SUFFIX "-${W3P_COMPILER_LABEL}")
endif()
set(W3P_RELEASE_AUDIO_SUFFIX "")
if(NOT WG_ENABLE_OPL_NUKED OR NOT WG_ENABLE_OPL_DBOPL
   OR NOT WG_ENABLE_OPL_SILENT)
    set(W3P_ENABLED_OPL_NAMES "")
    if(WG_ENABLE_OPL_NUKED)
        list(APPEND W3P_ENABLED_OPL_NAMES nuked)
    endif()
    if(WG_ENABLE_OPL_DBOPL)
        list(APPEND W3P_ENABLED_OPL_NAMES dbopl)
    endif()
    if(WG_ENABLE_OPL_SILENT)
        list(APPEND W3P_ENABLED_OPL_NAMES silent)
    endif()
    string(REPLACE ";" "-" W3P_ENABLED_OPL_LABEL
        "${W3P_ENABLED_OPL_NAMES}")
    set(W3P_RELEASE_AUDIO_SUFFIX "-opl-${W3P_ENABLED_OPL_LABEL}")
endif()
if(NOT WG_DEFAULT_OPL_DRIVER STREQUAL "nuked")
    string(APPEND W3P_RELEASE_AUDIO_SUFFIX
        "-default-${WG_DEFAULT_OPL_DRIVER}")
endif()
if(NOT WG_DEFAULT_SAMPLE_RATE STREQUAL "48000")
    string(APPEND W3P_RELEASE_AUDIO_SUFFIX
        "-${WG_DEFAULT_SAMPLE_RATE}hz")
endif()

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
        platforms/WG_TEXT_OUTPUT.c platforms/win32/WG_WIN32.c)
    w3p_configure_host(wolf3d-win32)
    target_compile_definitions(wolf3d-win32 PRIVATE
        WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
    if(MINGW)
        target_link_options(wolf3d-win32 PRIVATE -municode)
    endif()
    target_link_libraries(wolf3d-win32 PRIVATE user32 gdi32 winmm shell32)
    set_target_properties(wolf3d-win32 PROPERTIES OUTPUT_NAME "wolf3d")
    w3p_copy_engine_runtime(wolf3d-win32)

    set(W3P_WIN32_RELEASE_DIR
        "${W3P_DIST_ROOT}/wolf3d-portable-${PROJECT_VERSION}-win32-${W3P_RELEASE_ARCH}${W3P_RELEASE_COMPILER_SUFFIX}${W3P_RELEASE_RUNTIME_SUFFIX}${W3P_RELEASE_AUDIO_SUFFIX}")
    set(W3P_WIN32_OPL_COMMANDS "")
    set(W3P_WIN32_RELEASE_DEPENDS wolf3d-win32 wolf3d)
    if(TARGET nuked-opl3)
        list(APPEND W3P_WIN32_OPL_COMMANDS
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:nuked-opl3>"
                "${W3P_WIN32_RELEASE_DIR}/$<TARGET_FILE_NAME:nuked-opl3>"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${W3P_WOLF3D_SOURCE_DIR}/third_party/Nuked-OPL3/LICENSE"
                "${W3P_WIN32_RELEASE_DIR}/LICENSES/Nuked-OPL3-LGPL-2.1.txt")
        list(APPEND W3P_WIN32_RELEASE_DEPENDS nuked-opl3)
    endif()
    if(WG_ENABLE_OPL_DBOPL)
        list(APPEND W3P_WIN32_OPL_COMMANDS
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${W3P_WOLF3D_SOURCE_DIR}/third_party/DBOPL/README.wolf3d-lib.md"
                "${W3P_WIN32_RELEASE_DIR}/README.DBOPL.txt")
    endif()
    add_custom_target(win32-release
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${W3P_WIN32_RELEASE_DIR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${W3P_WIN32_RELEASE_DIR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${W3P_WIN32_RELEASE_DIR}/LICENSES"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:wolf3d-win32>" "${W3P_WIN32_RELEASE_DIR}/wolf3d.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:wolf3d>" "${W3P_WIN32_RELEASE_DIR}/$<TARGET_FILE_NAME:wolf3d>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/LICENSE" "${W3P_WIN32_RELEASE_DIR}/LICENSE.txt"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${W3P_WOLF3D_SOURCE_DIR}/LICENSE"
            "${W3P_WIN32_RELEASE_DIR}/LICENSES/wolf3d-lib-GPL-2.0.txt"
        ${W3P_WIN32_OPL_COMMANDS}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/THIRD_PARTY.md"
            "${W3P_WIN32_RELEASE_DIR}/THIRD_PARTY_NOTICES.txt"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_BINARY_DIR}/WIN32-README.txt" "${W3P_WIN32_RELEASE_DIR}/README.txt"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_BINARY_DIR}/WOLF3D-LIB.txt" "${W3P_WIN32_RELEASE_DIR}/WOLF3D-LIB.txt"
        DEPENDS ${W3P_WIN32_RELEASE_DEPENDS}
        COMMENT "Staging ${W3P_RELEASE_ARCH} Win32 release" VERBATIM)
endif()

if(W3P_BUILD_SDL3)
    if(WIN32)
        add_executable(wolf3d-sdl3 WIN32
            platforms/WG_TEXT_OUTPUT.c platforms/sdl3/WG_SDL3.c)
    else()
        add_executable(wolf3d-sdl3
            platforms/WG_TEXT_OUTPUT.c platforms/sdl3/WG_SDL3.c)
    endif()
    w3p_configure_host(wolf3d-sdl3)
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

    set(W3P_SDL3_RELEASE_DIR
        "${W3P_DIST_ROOT}/wolf3d-portable-${PROJECT_VERSION}-sdl3-${W3P_RELEASE_SYSTEM}-${W3P_RELEASE_ARCH}${W3P_RELEASE_COMPILER_SUFFIX}${W3P_RELEASE_RUNTIME_SUFFIX}${W3P_RELEASE_AUDIO_SUFFIX}")
    set(W3P_SDL3_RELEASE_DEPENDS wolf3d-sdl3 wolf3d SDL3::SDL3-shared)
    set(W3P_SDL3_RELEASE_COMMANDS
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${W3P_SDL3_RELEASE_DIR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${W3P_SDL3_RELEASE_DIR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${W3P_SDL3_RELEASE_DIR}/LICENSES"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:wolf3d-sdl3>" "${W3P_SDL3_RELEASE_DIR}/$<TARGET_FILE_NAME:wolf3d-sdl3>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:wolf3d>" "${W3P_SDL3_RELEASE_DIR}/$<TARGET_FILE_NAME:wolf3d>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:SDL3::SDL3-shared>" "${W3P_SDL3_RELEASE_DIR}/$<TARGET_FILE_NAME:SDL3::SDL3-shared>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/LICENSE" "${W3P_SDL3_RELEASE_DIR}/LICENSE.txt"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${W3P_WOLF3D_SOURCE_DIR}/LICENSE"
            "${W3P_SDL3_RELEASE_DIR}/LICENSES/wolf3d-lib-GPL-2.0.txt"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${W3P_SDL3_LICENSE_FILE}"
            "${W3P_SDL3_RELEASE_DIR}/LICENSES/SDL3-Zlib.txt"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/THIRD_PARTY.md"
            "${W3P_SDL3_RELEASE_DIR}/THIRD_PARTY_NOTICES.txt"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_BINARY_DIR}/SDL3-README.txt" "${W3P_SDL3_RELEASE_DIR}/README.txt"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_BINARY_DIR}/WOLF3D-LIB.txt" "${W3P_SDL3_RELEASE_DIR}/WOLF3D-LIB.txt")
    if(TARGET nuked-opl3)
        list(APPEND W3P_SDL3_RELEASE_COMMANDS
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:nuked-opl3>"
                "${W3P_SDL3_RELEASE_DIR}/$<TARGET_FILE_NAME:nuked-opl3>"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${W3P_WOLF3D_SOURCE_DIR}/third_party/Nuked-OPL3/LICENSE"
                "${W3P_SDL3_RELEASE_DIR}/LICENSES/Nuked-OPL3-LGPL-2.1.txt")
        list(APPEND W3P_SDL3_RELEASE_DEPENDS nuked-opl3)
    endif()
    if(WG_ENABLE_OPL_DBOPL)
        list(APPEND W3P_SDL3_RELEASE_COMMANDS
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${W3P_WOLF3D_SOURCE_DIR}/third_party/DBOPL/README.wolf3d-lib.md"
                "${W3P_SDL3_RELEASE_DIR}/README.DBOPL.txt")
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
        DEPENDS ${W3P_SDL3_RELEASE_DEPENDS}
        COMMENT "Staging ${W3P_RELEASE_ARCH} SDL3 release" VERBATIM)
    if(BUILD_TESTING)
        add_test(NAME sdl3-help COMMAND wolf3d-sdl3 --sdl3-help)
    endif()
endif()

if(W3P_BUILD_LINUX_CONSOLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBDRM REQUIRED IMPORTED_TARGET libdrm)
    pkg_check_modules(ALSA REQUIRED IMPORTED_TARGET alsa)
    add_executable(wolf3d-console platforms/WG_TEXT_OUTPUT.c
        platforms/linux-console/MAIN.c platforms/linux-console/WG_LINUX_CONSOLE.c)
    w3p_configure_host(wolf3d-console)
    target_link_libraries(wolf3d-console PRIVATE PkgConfig::LIBDRM PkgConfig::ALSA)
    set_target_properties(wolf3d-console PROPERTIES OUTPUT_NAME "wolf3d"
        BUILD_WITH_INSTALL_RPATH ON INSTALL_RPATH "$ORIGIN")

    set(W3P_CONSOLE_RELEASE_DIR
        "${W3P_DIST_ROOT}/wolf3d-portable-${PROJECT_VERSION}-console-${W3P_RELEASE_ARCH}${W3P_RELEASE_AUDIO_SUFFIX}")
    set(W3P_CONSOLE_OPL_COMMANDS "")
    set(W3P_CONSOLE_RELEASE_DEPENDS wolf3d-console wolf3d)
    if(TARGET nuked-opl3)
        list(APPEND W3P_CONSOLE_OPL_COMMANDS
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:nuked-opl3>"
                "${W3P_CONSOLE_RELEASE_DIR}/$<TARGET_FILE_NAME:nuked-opl3>"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${W3P_WOLF3D_SOURCE_DIR}/third_party/Nuked-OPL3/LICENSE"
                "${W3P_CONSOLE_RELEASE_DIR}/LICENSES/Nuked-OPL3-LGPL-2.1.txt")
        list(APPEND W3P_CONSOLE_RELEASE_DEPENDS nuked-opl3)
    endif()
    if(WG_ENABLE_OPL_DBOPL)
        list(APPEND W3P_CONSOLE_OPL_COMMANDS
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${W3P_WOLF3D_SOURCE_DIR}/third_party/DBOPL/README.wolf3d-lib.md"
                "${W3P_CONSOLE_RELEASE_DIR}/README.DBOPL.txt")
    endif()
    add_custom_target(console-release
        COMMAND ${CMAKE_COMMAND} -E remove_directory "${W3P_CONSOLE_RELEASE_DIR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${W3P_CONSOLE_RELEASE_DIR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${W3P_CONSOLE_RELEASE_DIR}/LICENSES"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:wolf3d-console>" "${W3P_CONSOLE_RELEASE_DIR}/wolf3d"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:wolf3d>" "${W3P_CONSOLE_RELEASE_DIR}/$<TARGET_FILE_NAME:wolf3d>"
        COMMAND ${CMAKE_COMMAND} -E create_symlink
            "$<TARGET_FILE_NAME:wolf3d>" "${W3P_CONSOLE_RELEASE_DIR}/$<TARGET_SONAME_FILE_NAME:wolf3d>"
        COMMAND ${CMAKE_COMMAND} -E create_symlink
            "$<TARGET_SONAME_FILE_NAME:wolf3d>" "${W3P_CONSOLE_RELEASE_DIR}/$<TARGET_LINKER_FILE_NAME:wolf3d>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/LICENSE" "${W3P_CONSOLE_RELEASE_DIR}/LICENSE.txt"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${W3P_WOLF3D_SOURCE_DIR}/LICENSE"
            "${W3P_CONSOLE_RELEASE_DIR}/LICENSES/wolf3d-lib-GPL-2.0.txt"
        ${W3P_CONSOLE_OPL_COMMANDS}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/THIRD_PARTY.md"
            "${W3P_CONSOLE_RELEASE_DIR}/THIRD_PARTY_NOTICES.txt"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_BINARY_DIR}/LINUX-CONSOLE-README.txt" "${W3P_CONSOLE_RELEASE_DIR}/README.txt"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_BINARY_DIR}/WOLF3D-LIB.txt" "${W3P_CONSOLE_RELEASE_DIR}/WOLF3D-LIB.txt"
        DEPENDS ${W3P_CONSOLE_RELEASE_DEPENDS}
        COMMENT "Staging ${W3P_RELEASE_ARCH} console release" VERBATIM)
    if(BUILD_TESTING)
        add_test(NAME console-help COMMAND wolf3d-console --linux-console-help)
    endif()
endif()
