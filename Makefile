# GNU Make conveniences; CMake remains the authoritative build description.

ifeq ($(origin CC),default)
CC := gcc
endif

COMPILER_NAME := $(notdir $(firstword $(CC)))
WOLF3D_VERSION := $(shell awk 'NR == 1 { print; exit }' lib/wolf3d/VERSION 2>/dev/null)
CMAKE ?= cmake
GIT ?= git
DOCKER ?= docker
CMAKE_GENERATOR ?= Unix Makefiles
BUILD_TYPE ?= Release
CMAKE_ARGS ?=
JOBS ?=
USE_SYSTEM_SDL3 ?= OFF
OPL_DRIVERS ?= nuked,dbopl,silent
OPL_DEFAULT ?= nuked
SAMPLE_RATE ?= 48000

comma := ,
AUDIO_SUFFIX :=
ifneq ("$(OPL_DRIVERS)","nuked,dbopl,silent")
AUDIO_SUFFIX := $(AUDIO_SUFFIX)-opl-$(subst $(comma),-,$(OPL_DRIVERS))
endif
ifneq ("$(OPL_DEFAULT)","nuked")
AUDIO_SUFFIX := $(AUDIO_SUFFIX)-default-$(OPL_DEFAULT)
endif
ifneq ("$(SAMPLE_RATE)","48000")
AUDIO_SUFFIX := $(AUDIO_SUFFIX)-$(SAMPLE_RATE)hz
endif

CONSOLE_BUILD_DIR ?= build/linux-console-$(COMPILER_NAME)
SDL3_BUILD_DIR ?= build/linux-sdl3-$(COMPILER_NAME)
PORTABLE_CONSOLE_BUILD_DIR ?= build/linux-console-portable-debian10-gcc
PORTABLE_SDL3_BUILD_DIR ?= build/linux-sdl3-portable-debian10-gcc
PORTABLE_CONSOLE_DIST_DIR ?= dist/wolf3d-portable-$(WOLF3D_VERSION)-console-x64$(AUDIO_SUFFIX)
PORTABLE_SDL3_DIST_DIR ?= dist/wolf3d-portable-$(WOLF3D_VERSION)-sdl3-linux-x64$(AUDIO_SUFFIX)
PORTABLE_BUILD_IMAGE ?= wolf3d-portable-build-debian10
PORTABLE_GLIBC_MAX ?= 2.28
MUSL_SDL3_BUILD_DIR ?= build/linux-sdl3-musl-$(COMPILER_NAME)
MUSL_STAGE_ROOT ?= build/linux-sdl3-musl-stage
MUSL_STAGE_DIR ?= $(MUSL_STAGE_ROOT)/wolf3d-portable-$(WOLF3D_VERSION)-sdl3-linux-x64$(AUDIO_SUFFIX)
MUSL_SDL3_DIST_DIR ?= dist/wolf3d-portable-$(WOLF3D_VERSION)-sdl3-linux-musl-x64$(AUDIO_SUFFIX)
MUSL_BUILD_IMAGE ?= wolf3d-portable-build-alpine-musl
DOS_BUILD_DIR ?= build/openwatcom-dos32
DOS_DIST_DIR ?= dist/wolf3d-portable-$(WOLF3D_VERSION)-dos32-x86
DOS_BUILD_IMAGE ?= wolf3d-portable-build-openwatcom-20261001
DOS_OPL_DRIVERS ?= nuked,dbopl,silent,adlib
DOS_OPL_DEFAULT ?= nuked
DOS_SAMPLE_RATE ?= 44100
MUSL_SDL3_CMAKE_ARGS ?= -DW3P_DIST_ROOT=/src/$(MUSL_STAGE_ROOT) \
	-DWG_LINUX_LIBC=musl -DSDL_KMSDRM=OFF -DSDL_OPENGL=OFF \
	-DSDL_OPENGLES=OFF -DSDL_RENDER_GPU=OFF -DSDL_VULKAN=OFF \
	-DSDL_WAYLAND_LIBDECOR=OFF -DSDL_PIPEWIRE=OFF
DOCKER_RUN_ARGS ?=
CMAKE_COMPILER_ARG := -DCMAKE_C_COMPILER="$(CC)"
CMAKE_AUDIO_ARGS := \
	-DWG_ENABLE_OPL_NUKED=$(if $(findstring nuked,$(OPL_DRIVERS)),ON,OFF) \
	-DWG_ENABLE_OPL_DBOPL=$(if $(findstring dbopl,$(OPL_DRIVERS)),ON,OFF) \
	-DWG_ENABLE_OPL_SILENT=$(if $(findstring silent,$(OPL_DRIVERS)),ON,OFF) \
	-DWG_DEFAULT_OPL_DRIVER="$(OPL_DEFAULT)" \
	-DWG_DEFAULT_SAMPLE_RATE="$(SAMPLE_RATE)"
PARALLEL_ARG := --parallel $(JOBS)

.DEFAULT_GOAL := all

.PHONY: all help dependencies fresh configure-sdl3 sdl3 sdl3-build sdl3-release \
	configure-console console console-release linux-console-release releases \
	portable portable-sdl3 portable-sdl3-release portable-console \
	portable-console-release portable-image portable-glibc-audit print-config \
	musl-sdl3 musl-sdl3-release musl-image musl-audit universal-sdl3 \
	dos dos-release dos-image clean clean-sdl3 clean-console clean-portable \
	clean-musl clean-dos

all: sdl3-release

help:
	@printf '%s\n' \
		'wolf3d-portable GNU Make entry points' \
		'' \
		'Common targets:' \
		'  make | make all              Build the SDL3 distribution (default).' \
		'  make sdl3-release            Build the SDL3 distribution.' \
		'  make console-release         Build the DRM/evdev/ALSA distribution.' \
		'  make releases                Build both Linux distributions.' \
		'  make fresh                   Update wolf3d-lib main, initialize dependencies, and build.' \
		'  make dependencies            Initialize the recorded submodule revisions.' \
		'' \
		'Portable Debian 10 targets:' \
		'  make portable                Build portable SDL3 and console distributions.' \
		'  make portable-sdl3           Build the portable SDL3 distribution.' \
		'  make portable-console        Build the portable console distribution.' \
		'' \
		'Relocatable musl target:' \
		'  make musl-sdl3               Build an AppDir-style SDL3 bundle with its musl loader.' \
		'  make universal-sdl3          Alias for make musl-sdl3.' \
		'  make musl-audit              Re-audit an existing musl SDL3 bundle.' \
		'' \
		'32-bit DOS cross-build:' \
		'  make dos                     Build the Open Watcom/DOS32A distribution.' \
		'  make dos-release             Explicit form of make dos.' \
		'' \
		'Useful variables:' \
		'  CC=gcc|clang                 Compiler (default: gcc).' \
		'  JOBS=N                       Parallel job limit.' \
		'  BUILD_TYPE=Release|Debug     Build type (default: Release).' \
		'  USE_SYSTEM_SDL3=ON|OFF       Installed SDL >= 3.2 or pinned SDL (default).' \
		'  OPL_DRIVERS=nuked,dbopl,silent Drivers compiled into wolf3d (default: all).' \
		'  OPL_DEFAULT=nuked|dbopl|silent Runtime default (default: nuked).' \
		'  SAMPLE_RATE=Hz               Preferred PCM rate (default: 48000).' \
		'  CMAKE_ARGS="..."             Additional CMake definitions.' \
		'  DOCKER_RUN_ARGS="..."        Additional Docker run arguments.' \
		'  DOS_OPL_DRIVERS=...          DOS drivers (default: nuked,dbopl,silent,adlib).' \
		'  DOS_OPL_DEFAULT=...          DOS runtime default (default: nuked).' \
		'  DOS_SAMPLE_RATE=Hz           DOS preferred PCM rate (default: 44100).' \
		'' \
		'Cleanup:' \
		'  make clean                   Remove build trees and project builder images.' \
		'  make clean-sdl3              Remove the SDL3 build tree.' \
		'  make clean-console           Remove the console build tree.' \
		'  make clean-portable          Remove Debian portable trees and builder image.' \
		'  make clean-musl              Remove musl trees and builder image.' \
		'  make clean-dos               Remove DOS32 tree and builder image.'

print-config:
	@printf '%s\n' \
		'CC=$(CC)' 'WOLF3D_VERSION=$(WOLF3D_VERSION)' \
		'SDL3_BUILD_DIR=$(SDL3_BUILD_DIR)' 'CONSOLE_BUILD_DIR=$(CONSOLE_BUILD_DIR)' \
		'MUSL_SDL3_BUILD_DIR=$(MUSL_SDL3_BUILD_DIR)' \
		'DOS_BUILD_DIR=$(DOS_BUILD_DIR)' 'DOS_DIST_DIR=$(DOS_DIST_DIR)' \
		'USE_SYSTEM_SDL3=$(USE_SYSTEM_SDL3)' 'OPL_DRIVERS=$(OPL_DRIVERS)' \
		'OPL_DEFAULT=$(OPL_DEFAULT)' 'SAMPLE_RATE=$(SAMPLE_RATE)' \
		'JOBS=$(JOBS)' 'CMAKE_ARGS=$(CMAKE_ARGS)'

dependencies:
	$(GIT) submodule update --init --recursive

fresh:
	$(GIT) submodule update --remote --merge -- lib/wolf3d
	$(GIT) submodule update --init --recursive
	$(MAKE) all

configure-sdl3:
	@if [ ! -f lib/wolf3d/CMakeLists.txt ]; then \
		printf '%s\n' 'wolf3d-lib is missing; run: make dependencies'; exit 2; fi
	@if [ "$(USE_SYSTEM_SDL3)" != "ON" ] && [ ! -f third_party/SDL3/CMakeLists.txt ]; then \
		printf '%s\n' 'Pinned SDL3 is missing; run: make dependencies'; exit 2; fi
	$(CMAKE) -S . -B "$(SDL3_BUILD_DIR)" -G "$(CMAKE_GENERATOR)" \
		-DCMAKE_BUILD_TYPE="$(BUILD_TYPE)" -DW3P_BUILD_SDL3=ON \
		-DW3P_BUILD_LINUX_CONSOLE=OFF -DW3P_USE_SYSTEM_SDL3=$(USE_SYSTEM_SDL3) \
		-DW3P_WARNINGS_AS_ERRORS=ON $(CMAKE_COMPILER_ARG) $(CMAKE_AUDIO_ARGS) $(CMAKE_ARGS)

sdl3-build: configure-sdl3
	$(CMAKE) --build "$(SDL3_BUILD_DIR)" $(PARALLEL_ARG)

sdl3-release: configure-sdl3
	$(CMAKE) --build "$(SDL3_BUILD_DIR)" --target sdl3-release $(PARALLEL_ARG)

configure-console:
	@if [ ! -f lib/wolf3d/CMakeLists.txt ]; then \
		printf '%s\n' 'wolf3d-lib is missing; run: make dependencies'; exit 2; fi
	$(CMAKE) -S . -B "$(CONSOLE_BUILD_DIR)" -G "$(CMAKE_GENERATOR)" \
		-DCMAKE_BUILD_TYPE="$(BUILD_TYPE)" -DW3P_BUILD_SDL3=OFF \
		-DW3P_BUILD_LINUX_CONSOLE=ON -DW3P_WARNINGS_AS_ERRORS=ON \
		$(CMAKE_COMPILER_ARG) $(CMAKE_AUDIO_ARGS) $(CMAKE_ARGS)

console-release: configure-console
	$(CMAKE) --build "$(CONSOLE_BUILD_DIR)" --target console-release $(PARALLEL_ARG)

linux-console-release: console-release
sdl3: sdl3-release
console: console-release
releases: sdl3-release console-release

portable-image:
	$(DOCKER) build --tag "$(PORTABLE_BUILD_IMAGE)" packaging/linux-portable

portable-sdl3-release: portable-image
	$(DOCKER) run --rm --user "$$(id -u):$$(id -g)" \
		--volume "$(CURDIR):/src" --workdir /src $(DOCKER_RUN_ARGS) \
		"$(PORTABLE_BUILD_IMAGE)" make sdl3-release CC=gcc \
		OPL_DRIVERS="$(OPL_DRIVERS)" OPL_DEFAULT="$(OPL_DEFAULT)" SAMPLE_RATE="$(SAMPLE_RATE)" \
		SDL3_BUILD_DIR="$(PORTABLE_SDL3_BUILD_DIR)" JOBS="$(JOBS)" USE_SYSTEM_SDL3=OFF
	$(DOCKER) run --rm --volume "$(CURDIR):/src:ro" --workdir /src \
		$(DOCKER_RUN_ARGS) "$(PORTABLE_BUILD_IMAGE)" sh tools/WG_GLIBC_AUDIT.sh \
		"$(PORTABLE_SDL3_DIST_DIR)" "$(PORTABLE_GLIBC_MAX)"

portable-console-release: portable-image
	$(DOCKER) run --rm --user "$$(id -u):$$(id -g)" \
		--volume "$(CURDIR):/src" --workdir /src $(DOCKER_RUN_ARGS) \
		"$(PORTABLE_BUILD_IMAGE)" make console-release CC=gcc \
		OPL_DRIVERS="$(OPL_DRIVERS)" OPL_DEFAULT="$(OPL_DEFAULT)" SAMPLE_RATE="$(SAMPLE_RATE)" \
		CONSOLE_BUILD_DIR="$(PORTABLE_CONSOLE_BUILD_DIR)" JOBS="$(JOBS)"
	$(DOCKER) run --rm --volume "$(CURDIR):/src:ro" --workdir /src \
		$(DOCKER_RUN_ARGS) "$(PORTABLE_BUILD_IMAGE)" sh tools/WG_GLIBC_AUDIT.sh \
		"$(PORTABLE_CONSOLE_DIST_DIR)" "$(PORTABLE_GLIBC_MAX)"

portable-sdl3: portable-sdl3-release
portable-console: portable-console-release
portable: portable-sdl3-release portable-console-release
portable-glibc-audit:
	sh tools/WG_GLIBC_AUDIT.sh "$(PORTABLE_SDL3_DIST_DIR)" "$(PORTABLE_GLIBC_MAX)"
	sh tools/WG_GLIBC_AUDIT.sh "$(PORTABLE_CONSOLE_DIST_DIR)" "$(PORTABLE_GLIBC_MAX)"

musl-image:
	$(DOCKER) build --tag "$(MUSL_BUILD_IMAGE)" packaging/linux-musl

musl-sdl3-release: musl-image
	$(DOCKER) run --rm --user "$$(id -u):$$(id -g)" \
		--volume "$(CURDIR):/src" --workdir /src $(DOCKER_RUN_ARGS) \
		"$(MUSL_BUILD_IMAGE)" make sdl3-release CC="$(CC)" \
		OPL_DRIVERS="$(OPL_DRIVERS)" OPL_DEFAULT="$(OPL_DEFAULT)" SAMPLE_RATE="$(SAMPLE_RATE)" \
		SDL3_BUILD_DIR="$(MUSL_SDL3_BUILD_DIR)" JOBS="$(JOBS)" \
		USE_SYSTEM_SDL3=OFF CMAKE_ARGS="$(MUSL_SDL3_CMAKE_ARGS)"
	$(DOCKER) run --rm --volume "$(CURDIR):/src:ro" --workdir /src \
		$(DOCKER_RUN_ARGS) "$(MUSL_BUILD_IMAGE)" sh tools/W3P_SDL_CONFIG_AUDIT.sh \
		"$(MUSL_SDL3_BUILD_DIR)"
	$(DOCKER) run --rm --user "$$(id -u):$$(id -g)" \
		--volume "$(CURDIR):/src" --workdir /src $(DOCKER_RUN_ARGS) \
		"$(MUSL_BUILD_IMAGE)" sh tools/W3P_MUSL_BUNDLE.sh \
		"$(MUSL_STAGE_DIR)" "$(MUSL_SDL3_DIST_DIR)"
	$(DOCKER) run --rm --volume "$(CURDIR):/src:ro" --workdir /src \
		$(DOCKER_RUN_ARGS) "$(MUSL_BUILD_IMAGE)" sh tools/W3P_MUSL_AUDIT.sh \
		"$(MUSL_SDL3_DIST_DIR)"

musl-sdl3: musl-sdl3-release
universal-sdl3: musl-sdl3-release
musl-audit: musl-image
	$(DOCKER) run --rm --volume "$(CURDIR):/src:ro" --workdir /src \
		$(DOCKER_RUN_ARGS) "$(MUSL_BUILD_IMAGE)" sh tools/W3P_MUSL_AUDIT.sh \
		"$(MUSL_SDL3_DIST_DIR)"

dos-image:
	$(DOCKER) build --tag "$(DOS_BUILD_IMAGE)" lib/wolf3d/packaging/openwatcom

dos-release: dos-image
	$(DOCKER) run --rm --user "$$(id -u):$$(id -g)" \
		--volume "$(CURDIR):/src" --workdir /src $(DOCKER_RUN_ARGS) \
		--env W3P_OPENWATCOM_BUILD_DIR="/src/$(DOS_BUILD_DIR)" \
		--env W3P_OPENWATCOM_DIST_DIR="/src/$(DOS_DIST_DIR)" \
		--env W3P_OPENWATCOM_OPL_DRIVERS="$(DOS_OPL_DRIVERS)" \
		--env W3P_OPENWATCOM_DEFAULT_OPL="$(DOS_OPL_DEFAULT)" \
		--env W3P_OPENWATCOM_SAMPLE_RATE="$(DOS_SAMPLE_RATE)" \
		"$(DOS_BUILD_IMAGE)" sh scripts/linux/openwatcom/build-dos.sh

dos: dos-release

clean-sdl3:
	$(CMAKE) -E remove_directory "$(SDL3_BUILD_DIR)"
clean-console:
	$(CMAKE) -E remove_directory "$(CONSOLE_BUILD_DIR)"
clean-portable:
	$(CMAKE) -E remove_directory "$(PORTABLE_SDL3_BUILD_DIR)"
	$(CMAKE) -E remove_directory "$(PORTABLE_CONSOLE_BUILD_DIR)"
	@if command -v "$(DOCKER)" >/dev/null 2>&1; then \
		$(DOCKER) image rm "$(PORTABLE_BUILD_IMAGE)" >/dev/null 2>&1 || true; fi
clean-musl:
	$(CMAKE) -E remove_directory "$(MUSL_SDL3_BUILD_DIR)"
	$(CMAKE) -E remove_directory "$(MUSL_STAGE_ROOT)"
	@if command -v "$(DOCKER)" >/dev/null 2>&1; then \
		$(DOCKER) image rm "$(MUSL_BUILD_IMAGE)" >/dev/null 2>&1 || true; fi
clean-dos:
	@case "$(DOS_BUILD_DIR)" in \
		build/*) ;; \
		*) printf '%s\n' 'Refusing to remove a DOS build tree outside build/:' \
			'  $(DOS_BUILD_DIR)'; exit 2 ;; \
	esac
	$(CMAKE) -E remove_directory "$(DOS_BUILD_DIR)"
	@if command -v "$(DOCKER)" >/dev/null 2>&1; then \
		$(DOCKER) image rm "$(DOS_BUILD_IMAGE)" >/dev/null 2>&1 || true; fi
clean: clean-sdl3 clean-console clean-portable clean-musl clean-dos
