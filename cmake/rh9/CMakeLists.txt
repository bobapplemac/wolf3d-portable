cmake_minimum_required(VERSION 3.16)
get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
file(STRINGS "${ROOT}/lib/wolf3d/VERSION" ENGINE_VERSION LIMIT_COUNT 1)
include("${ROOT}/cmake/W3PCheckEngineVersion.cmake")
w3p_check_engine_version("${ENGINE_VERSION}")
project(wolf3d-rh9 VERSION ${ENGINE_VERSION} LANGUAGES C)
include(CTest)
set(WG_BUILD_HEADLESS OFF CACHE BOOL "" FORCE)
option(WG_ENABLE_OPL_NUKED "Include optional heavier emulator" OFF)
option(WG_ENABLE_OPL_DBOPL "Include DBOPL" ON)
set(WG_ENABLE_OPL_ADLIB OFF CACHE BOOL "Native OPL is deferred for RH9" FORCE)
set(WG_DEFAULT_OPL_DRIVER auto CACHE STRING "Default OPL driver")
set(WG_LINUX_LIBC glibc CACHE STRING "" FORCE)
# This host targets old CPUs; use existing engine policy without changing its API.
add_compile_definitions(WG_AUDIO_LEGACY_TARGET=1)
add_subdirectory("${ROOT}/lib/wolf3d" wolf3d-lib)
find_path(SDL1_INCLUDE SDL/SDL.h REQUIRED)
find_library(SDL1_LIBRARY SDL REQUIRED)
add_executable(wolf3d-sdl1 "${ROOT}/platforms/sdl1/WG_SDL1.c" "${ROOT}/platforms/WG_HELP.c" "${ROOT}/platforms/WG_TEXT_OUTPUT.c")
target_include_directories(wolf3d-sdl1 PRIVATE ${SDL1_INCLUDE})
target_link_libraries(wolf3d-sdl1 PRIVATE wolf3d ${SDL1_LIBRARY})
target_compile_options(wolf3d-sdl1 PRIVATE -Wall -Wextra -Werror)
set_target_properties(wolf3d-sdl1 PROPERTIES OUTPUT_NAME wolf3d BUILD_RPATH "$ORIGIN" BUILD_RPATH_USE_ORIGIN ON)
if(BUILD_TESTING)
 add_executable(wg-sdl1-test "${ROOT}/tests/WG_SDL1_TEST.c" "${ROOT}/platforms/WG_HELP.c" "${ROOT}/platforms/WG_TEXT_OUTPUT.c")
 target_include_directories(wg-sdl1-test PRIVATE ${SDL1_INCLUDE})
 target_link_libraries(wg-sdl1-test PRIVATE wolf3d ${SDL1_LIBRARY})
 target_compile_options(wg-sdl1-test PRIVATE -Wall -Wextra -Werror)
 add_test(NAME sdl1-host COMMAND wg-sdl1-test)
 set_tests_properties(sdl1-host PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy")
 add_test(NAME sdl1-help COMMAND wolf3d-sdl1 --help)
endif()
# Driven only by the dedicated RH9 container profile.
string(REGEX MATCH "^[0-9]+" COMPILER_MAJOR "${CMAKE_C_COMPILER_VERSION}")
set(DIST "${ROOT}/dist/wolf3d-portable_${PROJECT_VERSION}_linux-glibc_x86_sdl1_gcc${COMPILER_MAJOR}")
file(WRITE "${CMAKE_BINARY_DIR}/release.path" "${DIST}")
add_custom_target(sdl1-release
 COMMAND ${CMAKE_COMMAND} "-DROOT=${ROOT}" "-DBUILD=${CMAKE_BINARY_DIR}" "-DDIST=${DIST}"
 "-DENGINE=$<TARGET_FILE:wolf3d>" "-DENGINE_SONAME=$<TARGET_SONAME_FILE_NAME:wolf3d>"
 "-DEXE=$<TARGET_FILE:wolf3d-sdl1>" "-DSDL=${SDL1_LIBRARY}"
 "-DNUKED=${WG_ENABLE_OPL_NUKED}" "-DDBOPL=${WG_ENABLE_OPL_DBOPL}"
 "-DVERSION=${PROJECT_VERSION}" "-DCOMPILER=${CMAKE_C_COMPILER_VERSION}"
 -P "${ROOT}/cmake/rh9/package.cmake"
 DEPENDS wolf3d-sdl1 VERBATIM)
