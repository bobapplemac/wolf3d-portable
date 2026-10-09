cmake_minimum_required(VERSION 3.5)

# Keep the Visual Studio 6 through 2005 generators isolated from the modern
# wrapper project.  The engine is still built from the pinned submodule by its
# own canonical legacy definition.
project(wolf3d_portable_legacy C)

get_filename_component(W3P_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(W3P_WOLF3D_ROOT "${W3P_ROOT}/lib/wolf3d")
file(READ "${W3P_WOLF3D_ROOT}/VERSION" W3P_PROJECT_VERSION)
string(STRIP "${W3P_PROJECT_VERSION}" W3P_PROJECT_VERSION)
set(PROJECT_VERSION "${W3P_PROJECT_VERSION}")
include("${W3P_ROOT}/cmake/W3PCheckEngineVersion.cmake")
w3p_check_engine_version("${W3P_PROJECT_VERSION}")

option(WG_STATIC_MSVC_RUNTIME "Link the legacy MSVC runtime statically" ON)
set(W3P_DIST_ROOT "${W3P_ROOT}/dist" CACHE PATH
    "Directory used by legacy release staging targets")

if(NOT MSVC)
    message(FATAL_ERROR "The legacy build is only for Microsoft Visual C++")
endif()
if(MSVC_VERSION GREATER 1400)
    message(FATAL_ERROR
        "Use the root CMake project for Visual Studio 2008 and newer")
endif()

set(WG_DIST_ROOT "${CMAKE_BINARY_DIR}/wolf3d-lib-dist" CACHE PATH "" FORCE)
add_subdirectory("${W3P_WOLF3D_ROOT}/cmake/legacy" wolf3d-lib)

add_executable(wolf3d-win32 WIN32
    "${W3P_ROOT}/platforms/WG_HELP.c"
    "${W3P_ROOT}/platforms/WG_TEXT_OUTPUT.c"
    "${W3P_ROOT}/platforms/win32/WG_WIN32.c")
target_link_libraries(wolf3d-win32 wolf3d user32 gdi32 winmm shell32)
target_include_directories(wolf3d-win32 PRIVATE
    "${W3P_WOLF3D_ROOT}/include"
    "${W3P_WOLF3D_ROOT}/compat/msvc")
target_compile_definitions(wolf3d-win32 PRIVATE
    WIN32_LEAN_AND_MEAN UNICODE _UNICODE inline=__inline
    WG_LEGACY_WIN32=1)
set_target_properties(wolf3d-win32 PROPERTIES OUTPUT_NAME "wolf3d")

if(MSVC_VERSION LESS 1300)
    target_compile_options(wolf3d-win32 PRIVATE /W3)
else()
    target_compile_options(wolf3d-win32 PRIVATE /W4)
endif()

if(MSVC_VERSION EQUAL 1200)
    set(W3P_COMPILER_LABEL vc6)
elseif(MSVC_VERSION EQUAL 1300)
    set(W3P_COMPILER_LABEL vs2002)
elseif(MSVC_VERSION EQUAL 1310)
    set(W3P_COMPILER_LABEL vs2003)
elseif(MSVC_VERSION EQUAL 1400)
    set(W3P_COMPILER_LABEL vs2005)
else()
    set(W3P_COMPILER_LABEL msvc-legacy)
endif()

set(W3P_RUNTIME_SUFFIX "")
if(NOT WG_STATIC_MSVC_RUNTIME)
    set(W3P_RUNTIME_SUFFIX "-dynamic-crt")
endif()
set(W3P_AUDIO_SUFFIX "")
if(NOT WG_ENABLE_OPL_NUKED OR NOT WG_ENABLE_OPL_DBOPL
   OR NOT WG_ENABLE_OPL_SILENT)
    set(W3P_ENABLED_OPL_LABEL "")
    if(WG_ENABLE_OPL_NUKED)
        set(W3P_ENABLED_OPL_LABEL nuked)
    endif()
    if(WG_ENABLE_OPL_DBOPL)
        if(W3P_ENABLED_OPL_LABEL)
            set(W3P_ENABLED_OPL_LABEL "${W3P_ENABLED_OPL_LABEL}-dbopl")
        else()
            set(W3P_ENABLED_OPL_LABEL dbopl)
        endif()
    endif()
    if(WG_ENABLE_OPL_SILENT)
        if(W3P_ENABLED_OPL_LABEL)
            set(W3P_ENABLED_OPL_LABEL "${W3P_ENABLED_OPL_LABEL}-silent")
        else()
            set(W3P_ENABLED_OPL_LABEL silent)
        endif()
    endif()
    set(W3P_AUDIO_SUFFIX "-opl-${W3P_ENABLED_OPL_LABEL}")
endif()
if(NOT WG_DEFAULT_OPL_DRIVER STREQUAL "nuked")
    set(W3P_AUDIO_SUFFIX
        "${W3P_AUDIO_SUFFIX}-default-${WG_DEFAULT_OPL_DRIVER}")
endif()
if(NOT WG_DEFAULT_SAMPLE_RATE STREQUAL "48000")
    set(W3P_AUDIO_SUFFIX
        "${W3P_AUDIO_SUFFIX}-${WG_DEFAULT_SAMPLE_RATE}hz")
endif()
set(W3P_RELEASE_DIR
    "${W3P_DIST_ROOT}/wolf3d-portable-${W3P_PROJECT_VERSION}-win32-x86-${W3P_COMPILER_LABEL}${W3P_RUNTIME_SUFFIX}${W3P_AUDIO_SUFFIX}")

execute_process(
    COMMAND git -C "${W3P_ROOT}" rev-parse HEAD
    OUTPUT_VARIABLE W3P_PORTABLE_COMMIT
    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT W3P_PORTABLE_COMMIT)
    set(W3P_PORTABLE_COMMIT unknown)
endif()
execute_process(
    COMMAND git -C "${W3P_WOLF3D_ROOT}" rev-parse HEAD
    OUTPUT_VARIABLE W3P_WOLF3D_COMMIT
    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT W3P_WOLF3D_COMMIT)
    set(W3P_WOLF3D_COMMIT unknown)
endif()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/WOLF3D-LIB.txt"
    "wolf3d-portable commit: ${W3P_PORTABLE_COMMIT}\n"
    "wolf3d-lib version: ${W3P_PROJECT_VERSION}\n"
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
configure_file("${W3P_ROOT}/packaging/WIN32-README.txt.in"
               "${CMAKE_CURRENT_BINARY_DIR}/WIN32-README.txt" @ONLY)

set(W3P_RELEASE_COMMANDS
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${W3P_RELEASE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${W3P_RELEASE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${W3P_RELEASE_DIR}/LICENSES"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:wolf3d-win32>" "${W3P_RELEASE_DIR}/wolf3d.exe"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:wolf3d>" "${W3P_RELEASE_DIR}/wolf3d.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${W3P_ROOT}/LICENSE" "${W3P_RELEASE_DIR}/LICENSE.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${W3P_WOLF3D_ROOT}/LICENSE"
        "${W3P_RELEASE_DIR}/LICENSES/wolf3d-lib-GPL-2.0.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${W3P_ROOT}/THIRD_PARTY.md"
        "${W3P_RELEASE_DIR}/THIRD_PARTY_NOTICES.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_BINARY_DIR}/WIN32-README.txt"
        "${W3P_RELEASE_DIR}/README.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_BINARY_DIR}/WOLF3D-LIB.txt"
        "${W3P_RELEASE_DIR}/WOLF3D-LIB.txt")
set(W3P_RELEASE_DEPENDS wolf3d-win32 wolf3d)

if(TARGET nuked-opl3)
    list(APPEND W3P_RELEASE_COMMANDS
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:nuked-opl3>"
            "${W3P_RELEASE_DIR}/Nuked-OPL3.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${W3P_WOLF3D_ROOT}/third_party/Nuked-OPL3/LICENSE"
            "${W3P_RELEASE_DIR}/LICENSES/Nuked-OPL3-LGPL-2.1.txt")
    list(APPEND W3P_RELEASE_DEPENDS nuked-opl3)
endif()
if(WG_ENABLE_OPL_DBOPL)
    list(APPEND W3P_RELEASE_COMMANDS
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${W3P_WOLF3D_ROOT}/third_party/DBOPL/README.wolf3d-lib.md"
            "${W3P_RELEASE_DIR}/README.DBOPL.txt")
endif()

add_custom_target(win32_release
    ${W3P_RELEASE_COMMANDS}
    DEPENDS ${W3P_RELEASE_DEPENDS})
