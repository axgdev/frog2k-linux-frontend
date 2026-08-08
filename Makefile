CC ?= cc
CXX ?= c++
CROSS_COMPILE ?= /tmp/sf2000-linux-next-buildroot/buildroot-sf2000/host/bin/mipsel-buildroot-linux-uclibc-
SF2000_CC ?= $(CROSS_COMPILE)gcc
SF2000_CXX ?= $(CROSS_COMPILE)g++
SF2000_OBJCOPY ?= $(CROSS_COMPILE)objcopy
SF2000_STRIP ?= $(CROSS_COMPILE)strip
SF2000_NM ?= $(CROSS_COMPILE)nm
CORE ?=
FROGUI_CORE ?= ../mufrog-commandc/cores/output/frogui_libretro_sf2000.a
GAMBATTE_REV := 9b3b5e3cc18ec92f460d37dd551eaf90c55bfcea
GPSP_REV := 5b6e751f4abf368509146cd143c949c1946ac1ae
FCEUMM_REV := b5e3566515c27dc66c9c20572171673126532e06
QUICKNES_REV := 7848e1ac22b1c69d056ae4cb57710651ff1dd169
PROSYSTEM_REV := 4202ac5bdb2ce1a21f84efc0e26d75bb5aa7e248
SNES9X2005_REV := b60356971fc9caae02cd0853676dced886a08be7
SNES9X2002_REV := 39e0d8c6daf4b1b1302eeecfee8309570aeb6a82
STELLA_REV := 30e01eb2acb6587bd7cf2253fe7dbbcaa496ad8e
GEARBOY_REV := 36f9faf04bcb6c023176de12dddae99ffc1ceb10
PCE_FAST_REV := 9ba79648d6ec85e833aef719d7f359117498d89c
COMMON_REV := 9e2af2c23ff2595f096e2f591ea49a9bcb65401d
STB_REV := 31c1ad37456438565541f4919958214b6e762fb4
GAMBATTE_DIR := .deps/gambatte
GPSP_DIR := .deps/gpsp
FCEUMM_DIR := .deps/fceumm
QUICKNES_DIR := .deps/quicknes
PROSYSTEM_DIR := .deps/prosystem
SNES9X2005_DIR := .deps/snes9x2005
SNES9X2002_DIR := .deps/snes9x2002
STELLA_DIR := .deps/stella2014
GEARBOY_DIR := .deps/gearboy
PCE_FAST_DIR := .deps/pce-fast
QUICKNES_SOURCE_STAMP := $(QUICKNES_DIR)/.sf2000-source
COMMON_DIR := .deps/libretro-common
STB_DIR := .deps/stb
SF2000_LINUX_DIR ?= ../sf2000_linux
MUFROG_ROOT ?= /root/host-frogdev/universal/temp/mufrog-commandc
MUFROG_SOURCE_ROOT ?= $(MUFROG_ROOT)/.deps/cores
JS2300_ROOT ?= $(MUFROG_ROOT)/js2300
JS2300_MQUICKJS_DIR ?= $(MUFROG_ROOT)/.deps/mquickjs
GE_DIR := $(SF2000_LINUX_DIR)/ge
GE_SOURCES := $(GE_DIR)/hcge_linux.c $(GE_DIR)/hcge_node.c
AUDIO_DIR := $(SF2000_LINUX_DIR)/audio
AUDIO_SOURCES := $(AUDIO_DIR)/hc15xx_resampler.c
PLATFORM_DIR := $(SF2000_LINUX_DIR)/platform
PLATFORM_SOURCES := $(PLATFORM_DIR)/hc15xx_retained.c
FRONTEND_SOURCES := src/main.c src/sf2000_input.c src/sf2000_pacer.c \
	src/sf2000_browser_ui.c src/sf2000_log.c
SF2000_HOST_OBJECTS := build/host-main.o build/host-input.o \
	build/host-pacer.o \
	build/host-ui.o \
	build/host-log.o \
	build/host-ge-linux.o build/host-ge-node.o build/host-audio.o \
	build/host-retained.o build/host-nommu-new.o build/host-content.o
GAMBATTE_CORE := build/gambatte_libretro_linux.a
GPSP_CORE := build/gpsp_libretro_linux.a
FCEUMM_CORE := build/fceumm_libretro_linux.a
QUICKNES_CORE := build/quicknes_libretro_linux.a
PROSYSTEM_CORE := build/prosystem_libretro_linux.a
SNES9X2005_CORE := build/snes9x2005_libretro_linux.a
SNES9X2002_CORE := build/snes9x2002_libretro_linux.a
STELLA_CORE := build/stella2014_libretro_linux.a
GEARBOY_CORE := build/gearboy_libretro_linux.a
PCE_FAST_CORE := build/pce_fast_libretro_linux.a
SNES9X2002_MEMORY := build/snes9x2002-memory-stream.o
GAMBATTE_PATCHES := $(wildcard patches/gambatte/*.patch)
GPSP_PATCHES := $(wildcard patches/gpsp/*.patch)
FCEUMM_PATCHES := $(wildcard patches/fceumm/*.patch)
QUICKNES_PATCHES := $(wildcard patches/quicknes/*.patch)
PROSYSTEM_PATCHES := $(wildcard patches/prosystem/*.patch)
SNES9X2005_PATCHES := $(wildcard patches/snes9x2005/*.patch)
STELLA_PATCHES := $(wildcard patches/stella2014/*.patch)
GPSP_PATCH_ID := $(shell sha256sum $(GPSP_PATCHES) </dev/null | sha256sum | cut -c1-16)
GPSP_PATCH_STAMP := $(GPSP_DIR)/.sf2000-patched-$(GPSP_PATCH_ID)
FCEUMM_PATCH_ID := $(shell sha256sum $(FCEUMM_PATCHES) </dev/null | sha256sum | cut -c1-16)
FCEUMM_PATCH_STAMP := $(FCEUMM_DIR)/.sf2000-patched-$(FCEUMM_PATCH_ID)
QUICKNES_PATCH_ID := $(shell sha256sum $(QUICKNES_PATCHES) </dev/null | sha256sum | cut -c1-16)
QUICKNES_PATCH_STAMP := $(QUICKNES_DIR)/.sf2000-patched-$(QUICKNES_PATCH_ID)
PROSYSTEM_PATCH_ID := $(shell sha256sum $(PROSYSTEM_PATCHES) </dev/null | sha256sum | cut -c1-16)
PROSYSTEM_PATCH_STAMP := $(PROSYSTEM_DIR)/.sf2000-patched-$(PROSYSTEM_PATCH_ID)
SNES9X2005_PATCH_ID := $(shell sha256sum $(SNES9X2005_PATCHES) </dev/null | sha256sum | cut -c1-16)
SNES9X2005_PATCH_STAMP := $(SNES9X2005_DIR)/.sf2000-patched-$(SNES9X2005_PATCH_ID)
STELLA_PATCH_ID := $(shell sha256sum $(STELLA_PATCHES) </dev/null | sha256sum | cut -c1-16)
STELLA_PATCH_STAMP := $(STELLA_DIR)/.sf2000-patched-$(STELLA_PATCH_ID)
GPSP_TRANSLATOR_OPTIMIZE := -Os -DNDEBUG -fno-expensive-optimizations \
	-fno-jump-tables -fno-tree-switch-conversion
GPSP_CFLAGS := -Os -EL -march=mips32 -mtune=mips32 -mabi=32 -msoft-float \
	-G0 -mabicalls -fPIC -fomit-frame-pointer -ffast-math \
	-fsigned-char -fno-strict-aliasing -fwrapv \
	-fno-unwind-tables -fno-asynchronous-unwind-tables \
	-ffunction-sections -fdata-sections \
	-DMMAP_JIT_CACHE -DROM_BUFFER_SIZE=32 -DHAVE_STRINGS_H -DHAVE_STDINT_H \
	-DHAVE_INTTYPES_H -D__LIBRETRO__ -DINLINE=inline -DHAVE_DYNAREC \
	-DMIPS_ARCH -DGPSP_DYNAREC_SAFE_SMC_PATCH \
	-DGPSP_DYNAREC_SAFE_FALLBACK \
	-DGPSP_ROM_BUFFER_MMAP \
	-DROM_TRANSLATION_CACHE_SIZE=524288 \
	-DRAM_TRANSLATION_CACHE_SIZE=131072 \
	-DTRANSLATOR_WORKSPACE_SIZE=9216 \
	-DFRONTEND_SUPPORTS_RGB565
COMMON_SOURCES := compat/compat_posix_string.c compat/compat_snprintf.c \
	compat/compat_strcasestr.c compat/compat_strl.c compat/fopen_utf8.c \
	encodings/encoding_crc32.c encodings/encoding_deflate.c \
	file/file_path.c file/file_path_io.c \
	streams/file_stream.c streams/file_stream_transforms.c \
	streams/trans_stream.c streams/trans_stream_zlib.c \
	streams/trans_stream_deflate.c streams/trans_stream_pipe.c \
	streams/rzip_stream.c \
	formats/png/rpng.c \
	string/stdstring.c time/rtime.c vfs/vfs_implementation.c
COMMON_OBJECTS := $(addprefix build/common/,$(COMMON_SOURCES:.c=.o)) build/utf8_compat.o
LIBRETRO_COMMON := build/libretro-common-linux.a
CFLAGS := -Os -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -Iinclude
SF2000_CFLAGS := $(CFLAGS) -march=mips32 -mabi=32 -msoft-float \
	-fPIC -mabicalls \
	-I$(GE_DIR) -I$(AUDIO_DIR) -I$(SF2000_LINUX_DIR)/include
SF2000_SYSROOT ?= $(shell $(SF2000_CC) -print-sysroot)
SF2000_CRT_DIR ?= $(SF2000_SYSROOT)/usr/lib
STELLA_INCLUDES := -I$(abspath $(STELLA_DIR)) \
	-I$(abspath $(STELLA_DIR)/stella) -I$(abspath $(STELLA_DIR)/stella/src) \
	-I$(abspath $(STELLA_DIR)/stella/stubs) \
	-I$(abspath $(STELLA_DIR)/stella/src/emucore) \
	-I$(abspath $(STELLA_DIR)/stella/src/common) \
	-I$(abspath $(STELLA_DIR)/stella/src/gui) -I$(abspath $(COMMON_DIR)/include)
SF2000_STARTFILES = $(SF2000_CRT_DIR)/rcrt1.o $(SF2000_CRT_DIR)/crti.o \
	$(shell $(SF2000_CC) -print-file-name=crtbeginS.o)
SF2000_ENDFILES = $(shell $(SF2000_CC) -print-file-name=crtendS.o) \
	$(SF2000_CRT_DIR)/crtn.o
SF2000_LDFLAGS := -nostartfiles -static -Wl,-pie \
	-Wl,--no-dynamic-linker -Wl,-z,text \
	-Wl,--gc-sections
PCE_FAST_CFLAGS := -Os -EL -march=mips32 -mtune=mips32 -mabi=32 \
	-msoft-float -G0 -mabicalls -fPIC -ffast-math \
	-fomit-frame-pointer -ffunction-sections -fdata-sections \
	-fno-unwind-tables -fno-asynchronous-unwind-tables \
	-DFRONTEND_SUPPORTS_RGB565 -DNO_THREADS
PCE_FAST_CXXFLAGS := $(PCE_FAST_CFLAGS) -fno-rtti

# Mufrog's firmware archives use fixed-address HCRTOS objects.  The Linux
# NOMMU loader only accepts static-PIE executables, so these cores are rebuilt
# from the same pinned checkouts with the Linux o32 PIC ABI below.
MUFROG_CORE_CFLAGS := -EL -march=mips32 -mtune=mips32 -mabi=32 \
	-msoft-float -G0 -mabicalls -fPIC -Os -fomit-frame-pointer \
	-ffunction-sections -fdata-sections -fno-unwind-tables \
	-fno-asynchronous-unwind-tables -ffast-math -D_GNU_SOURCE \
	-D__LIBRETRO__
MUFROG_CORE_INCLUDES := \
	-I$(SF2000_SYSROOT)/usr/include \
	-I$(MUFROG_SOURCE_ROOT)/libretro-common/include
LIBRETRO_API_SYMBOLS := \
	retro_set_environment retro_set_video_refresh retro_set_audio_sample \
	retro_set_audio_sample_batch retro_set_input_poll retro_set_input_state \
	retro_init retro_deinit retro_api_version retro_get_system_info \
	retro_get_system_av_info retro_load_game retro_unload_game retro_run \
	retro_get_memory_data retro_get_memory_size retro_serialize_size \
	retro_serialize retro_unserialize

# id:source-directory:makefile:source-archive:symbol-prefix:extra-make-vars
MUFROG_CORE_SPECS := \
	gpsp-multicore:gpsp_multicore:Makefile:gpsp_multicore_libretro_sf2000.a:gpsp_multicore: \
	picodrive:picodrive:Makefile.libretro:picodrive_libretro_sf2000.a:picodrive:PLATFORM_TREMOR=1 \
	qpsx:sf2000-qpsx-playstation-emulator:Makefile.libretro:pcsx4all_libretro_sf2000.a:qpsx: \
	mame2000:libretro-mame2000:Makefile:mame2000_libretro_sf2000.a:mame2000: \
	fbalpha2012:fbalpha2012/svn-current/trunk:makefile.libretro:fbalpha2012_libretro_sf2000.a:fbalpha2012:PROFILE=performance \
	a5200:a5200:Makefile:a5200_libretro_sf2000.a:a5200: \
	atari800lib:libretro-atari800lib:Makefile:libatari800_libretro_sf2000.a:atari800lib:A5200=0 \
	handy:libretro-handy:Makefile:handy_libretro_sf2000.a:handy: \
	race:RACE:Makefile:race_libretro_sf2000.a:race: \
	beetle-cygne:libretro-beetle-wswan:Makefile:mednafen_wswan_libretro_sf2000.a:beetle_cygne: \
	gearcoleco:Gearcoleco:Makefile:gearcoleco_libretro_sf2000.a:gearcoleco: \
	frodo:libretro-frodo-prosty:Makefile.libretro:frodo_libretro_sf2000.a:frodo_prosty:EMUTYPE=frodo \
	fake08:fake-08-prosty:Makefile:fake08_libretro_sf2000.a:fake08_prosty: \
	bluemsx:libretro-blueMSX-prosty:Makefile.libretro:bluemsx_libretro_sf2000.a:bluemsx_prosty: \
	snes9x2005-prosty:snes9x2005-prosty:Makefile:snes9x2005_libretro_sf2000.a:snes9x2005_prosty:USE_BLARGG_APU=0 \
snes9x2002-prosty:snes9x2002-prosty:Makefile:snes9x2002_libretro_sf2000.a:snes9x2002_prosty: \
	gambatte-prosty:libretro-gambatte-prosty:Makefile.libretro:gambatte_libretro_sf2000.a:gambatte_prosty:HAVE_NETWORK=0 \
	quicknes-prosty:QuickNES_Core-prosty:Makefile:quicknes_libretro_sf2000.a:quicknes_prosty: \
	fceumm-prosty:libretro-fceumm-prosty:Makefile.libretro:fceumm_libretro_sf2000.a:fceumm_prosty:

mufrog_key = $(subst -,_,$(1))

define MUFROG_CORE_REGISTER
MUFROG_$(call mufrog_key,$(1))_SOURCE := $(MUFROG_SOURCE_ROOT)/$(2)
MUFROG_$(call mufrog_key,$(1))_MAKEFILE := $(3)
MUFROG_$(call mufrog_key,$(1))_ARCHIVE := $(4)
MUFROG_$(call mufrog_key,$(1))_PREFIX := $(5)
MUFROG_$(call mufrog_key,$(1))_ARGS := $(6)
endef
$(foreach spec,$(MUFROG_CORE_SPECS),$(eval $(call MUFROG_CORE_REGISTER,$(word 1,$(subst :, ,$(spec))),$(word 2,$(subst :, ,$(spec))),$(word 3,$(subst :, ,$(spec))),$(word 4,$(subst :, ,$(spec))),$(word 5,$(subst :, ,$(spec))),$(word 6,$(subst :, ,$(spec))))))
MUFROG_picodrive_EXTRA_CFLAGS := -include$(SF2000_SYSROOT)/usr/include/wchar.h \
	-include$(abspath src/mufrog_picodrive_config.h) -DDR_MP3_NO_STDIO -DUSE_TREMOR \
	-DEMU_F68K -D_USE_CZ80 -DDRC_SH2
MUFROG_picodrive_EXTRA_CFLAGS += -O3
MUFROG_picodrive_EXTRA_ARGS := NO_CD_MEDIA=1
MUFROG_qpsx_EXTRA_CFLAGS := -Isrc/ -Isrc/spu/spu_pcsxrearmed \
	-Isrc/gpu/gpu_unai -Isrc/gpu/gpulib -Isrc/plugin_lib \
	-Isrc/port/libretro -Ilibretro/core -Ilibretro/include \
	-DSF2000 -DGPU_UNAI -DSPU_PCSXREARMED -D__LIBRETRO__ -DHAVE_LIBRETRO \
	-DPSXREC -Dmips -DUSE_GPULIB -DHLE_BIOS -DXA_HACK -DNO_THREADS -DNO_ZLIB \
	-DQPSX_ENABLE_MIPS_DIRECT_MEM=1 \
	-DQPSX_ENABLE_MIPS_LSU_CACHING=1 \
	-DQPSX_LINUX_CACHEFLUSH=1 \
	-include$(abspath src/mufrog_qpsx_config.h) -O3
MUFROG_qpsx_PATCHES := patches/mufrog/qpsx-linux-paths.patch \
	patches/mufrog/qpsx-linux-cdda-asm.patch \
	patches/mufrog/qpsx-static-load-buffer.patch \
	patches/mufrog/qpsx-cue-failure.patch \
	patches/mufrog/qpsx-linux-dirent.patch \
	patches/mufrog/qpsx-nommu-recompiler.patch \
	patches/mufrog/qpsx-linux-cacheflush.patch
MUFROG_qpsx_ADAPTER_OBJECTS := build/mufrog/qpsx-adapter.o
MUFROG_handy_EXTRA_CFLAGS := -I$(abspath build/mufrog/src/handy/lynx) -DWANT_CRC32
MUFROG_fbalpha2012_EXTRA_CFLAGS := -include$(abspath src/mufrog_wchar_compat.h) \
	-D__LIBRETRO_OPTIMIZATIONS__
MUFROG_fbalpha2012_PATCHES := patches/mufrog/fbalpha-wchar.patch
MUFROG_race_EXTRA_CFLAGS := -DCZ80 -D_MAX_PATH=2048
MUFROG_beetle_cygne_EXTRA_CFLAGS := -DMEDNAFEN_VERSION_NUMERIC=931
MUFROG_gearcoleco_WORKDIR := platforms/libretro
MUFROG_gearcoleco_EXTRA_CFLAGS := \
	-I$(abspath build/mufrog/src/gearcoleco/platforms/shared/dependencies/miniz) \
	-DGEARCOLECO_DISABLE_DISASSEMBLER
MUFROG_fake08_WORKDIR := platform/libretro
MUFROG_fake08_EXTRA_CFLAGS := \
	-DSF2000 -DENABLE_AUDIO_OPTIMIZATIONS -O3 \
	-I$(abspath build/mufrog/src/fake08/libs/z8lua) \
  -I$(abspath build/mufrog/src/fake08/libs/simpleini) \
  -I$(abspath build/mufrog/src/fake08/libs/lodepng) \
  -I$(abspath build/mufrog/src/fake08/libs/miniz) \
  -include$(abspath src/mufrog_wchar_compat.h)
MUFROG_fake08_PATCHES := \
	patches/mufrog/fake08-cxx17.patch \
	patches/mufrog/fake08-fix32-mips.patch
MUFROG_snes9x2005_prosty_EXTRA_CFLAGS := -I$(abspath build/mufrog/src/snes9x2005-prosty/source)
MUFROG_snes9x2002_prosty_EXTRA_CFLAGS := -I$(abspath build/mufrog/src/snes9x2002-prosty/source) -DUSE_SA1
MUFROG_snes9x2002_prosty_PATCHES := patches/mufrog/snes9x2002-rops.patch
MUFROG_bluemsx_EXTRA_CFLAGS := -Wno-error=incompatible-pointer-types
MUFROG_bluemsx_PATCHES := patches/mufrog/bluemsx-linux-compat.patch
MUFROG_gambatte_prosty_EXTRA_CFLAGS := \
	-I$(abspath build/mufrog/src/gambatte-prosty/libgambatte/include) \
	-I$(abspath build/mufrog/src/gambatte-prosty/libgambatte/src) \
	-I$(abspath build/mufrog/src/gambatte-prosty/common) \
	-I$(abspath build/mufrog/src/gambatte-prosty/libgambatte/libretro) \
	-DHAVE_STDINT_H
MUFROG_fceumm_prosty_EXTRA_CFLAGS := -DFCEU_VERSION_NUMERIC=9813
MUFROG_mame2000_ADAPTER_OBJECTS := build/mufrog/mame2000-libco.o
MUFROG_mame2000_EXTRA_CFLAGS := -O3
MUFROG_fake08_ADAPTER_OBJECTS := build/mufrog/fake08-log.o
MUFROG_frodo_PATCHES := patches/mufrog/frodo-autoload-visibility.patch
MUFROG_frodo_EXTRA_CFLAGS := \
	-I$(abspath build/mufrog/src/frodo/libretro/core) \
	-I$(abspath build/mufrog/src/frodo/libretro/include) \
	-I$(abspath build/mufrog/src/frodo/Src/libretro-common/include) \
	-I$(abspath build/mufrog/src/frodo/Src) \
	-I$(abspath build/mufrog/src/frodo/Src/zlib) \
	-DSF2000 -DSF2000_C64_OPTIMIZED -DSF2000_FAST_CPU \
	-DSF2000_FAST_VIC -DSF2000_FAST_SID -DSF2000_FAST_MEMORY \
	-DSF2000_COMPUTED_GOTO -DSF2000_MIPS_OPTIMIZED \
	-O3 -funroll-loops
MUFROG_frodo_EXTRA_ARGS := EMUTYPE=frodo platform=sf2000 \
	MIPS=$(CROSS_COMPILE) NOLIBCO=0
MUFROG_gpsp_multicore_EXTRA_CFLAGS := -DSF2000 -DMMAP_JIT_CACHE \
	-DHAVE_DYNAREC -DMIPS_ARCH -DGPSP_DYNAREC_SAFE_SMC_PATCH \
	-DGPSP_DYNAREC_SAFE_FALLBACK -DGPSP_ROM_BUFFER_MMAP \
	-DSMALL_TRANSLATION_CACHE \
	-DROM_BUFFER_SIZE=16 \
	-DFRONTEND_SUPPORTS_RGB565 -DSF2000_OPTIMIZATION_LEVEL=2
MUFROG_gpsp_multicore_PATCHES := \
	patches/mufrog/gpsp-mips-validate.patch \
	patches/mufrog/gpsp-file-load.patch \
	patches/mufrog/gpsp-mips-pic.patch
MUFROG_gpsp_multicore_EXTRA_ARGS := HAVE_DYNAREC=1 CPU_ARCH=mips MMAP_JIT_CACHE=1 SF2000=1

MUFROG_CORE_EXECUTABLES := $(foreach spec,$(MUFROG_CORE_SPECS),build/sf2000-$(word 1,$(subst :, ,$(spec))))
MUFROG_MEMORY_STREAM := build/mufrog/libretro-memory-stream.a
JS2300_RUNTIME := build/js2300/libjs2300.a
JS2300_CORE_SOURCE := build/js2300/js2300_libretro_core.c
JS2300_CORE_OBJECT := build/js2300/js2300_libretro_core.o
JS2300_CORE_FS_OBJECT := build/js2300/js2300_core_fs.o
JS2300_CORE_EXECUTABLE := build/sf2000-js2300-core
JS2300_UI_EXECUTABLE := build/sf2000-js2300-ui
JS2300_SCRIPT := build/core-packages/js2300-cores/chip8.js

.PHONY: all clean check elf-audit gpsp-pic-audit sf2000 demo frogui browser \
	gambatte gpsp fceumm quicknes prosystem snes9x2005 snes9x2002 \
	stella2014 gearboy pce-fast mufrog-cores core-packages integrated

all: check

build/frontend-check: $(FRONTEND_SOURCES) src/content.c tests/dummy_core.c include/libretro_min.h $(GE_SOURCES) $(AUDIO_SOURCES) $(PLATFORM_SOURCES) $(STB_DIR)/.git
	mkdir -p build
	$(CC) $(CFLAGS) -I$(STB_DIR) -I$(GE_DIR) -I$(AUDIO_DIR) -I$(SF2000_LINUX_DIR)/include -o $@ \
		$(FRONTEND_SOURCES) src/content.c tests/dummy_core.c $(GE_SOURCES) $(AUDIO_SOURCES) $(PLATFORM_SOURCES) -lm

build/nommu-allocator-check: src/nommu_new.cpp tests/nommu_allocator_test.cpp
	mkdir -p build
	$(CXX) -O2 -std=c++17 -Wall -Wextra -Werror -o $@ $^

build/input-check: src/sf2000_input.c tests/input_test.c include/sf2000_input.h
	mkdir -p build
	$(CC) $(CFLAGS) -o $@ src/sf2000_input.c tests/input_test.c

build/pacer-check: src/sf2000_pacer.c tests/pacer_test.c include/sf2000_pacer.h
	mkdir -p build
	$(CC) $(CFLAGS) -o $@ src/sf2000_pacer.c tests/pacer_test.c

build/browser-ui-check: src/sf2000_browser_ui.c tests/browser_ui_test.c \
		include/sf2000_browser_ui.h $(STB_DIR)/.git
	mkdir -p build
	$(CC) $(CFLAGS) -I$(STB_DIR) -o $@ \
		src/sf2000_browser_ui.c tests/browser_ui_test.c -lm

build/qpsx-adapter-check: src/qpsx_adapter.c tests/qpsx_adapter_test.c
	mkdir -p build
	$(CC) $(CFLAGS) -o $@ src/qpsx_adapter.c tests/qpsx_adapter_test.c

check: build/frontend-check build/nommu-allocator-check build/input-check \
		build/pacer-check build/browser-ui-check build/qpsx-adapter-check
	./build/nommu-allocator-check
	./build/input-check
	./build/pacer-check
	./build/browser-ui-check
	./build/qpsx-adapter-check

elf-audit:
	@set -e; \
	for executable in build/sf2000-*; do \
		test -f "$$executable" || continue; \
		$(CROSS_COMPILE)readelf -h "$$executable" | \
			grep -Eq 'Type:[[:space:]]+DYN' || exit 1; \
		! $(CROSS_COMPILE)readelf -l "$$executable" | grep -q INTERP || exit 1; \
		$(CROSS_COMPILE)readelf -rW "$$executable" | \
			awk '/R_MIPS_/ && ($$2 !~ /^0000000[03]$$/ || \
				($$3 != "R_MIPS_REL32" && $$3 != "R_MIPS_NONE")) { exit 1 }'; \
	done
	@if test -f build/sf2000-gpsp; then $(MAKE) gpsp-pic-audit; fi

gpsp-pic-audit: build/sf2000-gpsp
	@set -e; \
	test "$$(grep -Fhxc '#if defined(PIC) || defined(__PIC__)' \
		'$(GPSP_DIR)'/mips/mips_emit.h '$(GPSP_DIR)'/mips/mips_stub.S | \
		awk '{ total += $$1 } END { print total + 0 }')" -eq 2; \
	body="$$(mktemp)"; \
	trap 'rm -f "$$body"' EXIT HUP INT TERM; \
	$(CROSS_COMPILE)objdump -dr --disassemble=execute_store_cpsr '$<' > "$$body"; \
	if grep -Eq 'lw[[:space:]]+t9,.*[(]gp[)]' "$$body"; then \
		echo 'gpSP: dynarec C call still addresses the static-PIE GOT through emulated r13/$$gp' >&2; \
		exit 1; \
	fi; \
	grep -Eq 'lw[[:space:]]+t9,.*[(]s0[)]' "$$body"; \
	grep -Eq 'lw[[:space:]]+gp,.*[(]s0[)]' "$$body"

sf2000: $(STB_DIR)/.git $(LIBRETRO_COMMON) $(SF2000_HOST_OBJECTS)
	test -n "$(CORE)" || { echo 'set CORE=/path/to/libretro_core.a' >&2; exit 2; }
	mkdir -p build
	$(SF2000_CXX) $(filter-out -std=c11,$(SF2000_CFLAGS)) \
		$(SF2000_LDFLAGS) \
		-o build/sf2000-frontend \
		$(SF2000_STARTFILES) $(SF2000_HOST_OBJECTS) \
		$(CORE) $(LIBRETRO_COMMON) -lm $(SF2000_ENDFILES)

demo: $(STB_DIR)/.git
	mkdir -p build
	$(SF2000_CC) $(SF2000_CFLAGS) -I$(STB_DIR) $(SF2000_LDFLAGS) \
		-o build/sf2000-frontend-demo \
		$(SF2000_STARTFILES) $(FRONTEND_SOURCES) $(GE_SOURCES) \
		$(AUDIO_SOURCES) $(PLATFORM_SOURCES) tests/dummy_core.c \
		$(SF2000_ENDFILES)

frogui: $(STB_DIR)/.git
	test -f "$(FROGUI_CORE)"
	mkdir -p build
	$(SF2000_CC) $(SF2000_CFLAGS) -I$(STB_DIR) $(SF2000_LDFLAGS) \
		-o build/sf2000-frontend-frogui $(SF2000_STARTFILES) \
		$(FRONTEND_SOURCES) $(GE_SOURCES) \
		$(AUDIO_SOURCES) $(PLATFORM_SOURCES) src/frogui_adapter.c \
		$(FROGUI_CORE) -lm -Wl,--wrap=calloc -Wl,--wrap=free \
		$(SF2000_ENDFILES)

browser: $(STB_DIR)/.git $(GE_SOURCES) $(GE_DIR)/ge_api.h $(GE_DIR)/hcge_node.h \
		src/browser.c src/sf2000_browser_ui.c src/sf2000_log.c \
		include/sf2000_browser_ui.h include/sf2000_log.h
	mkdir -p build
	$(SF2000_CC) $(SF2000_CFLAGS) -I$(STB_DIR) $(SF2000_LDFLAGS) \
		-o build/sf2000-browser $(SF2000_STARTFILES) src/browser.c \
		src/sf2000_browser_ui.c src/sf2000_log.c $(GE_SOURCES) -lm \
		$(SF2000_ENDFILES)

$(JS2300_RUNTIME): $(JS2300_ROOT)/Makefile \
		$(JS2300_ROOT)/src/js2300_runtime.c \
		$(JS2300_ROOT)/js2300_stdlib_gen.c \
		$(JS2300_ROOT)/include/js2300/js2300.h \
		$(JS2300_MQUICKJS_DIR)/mquickjs.c Makefile
	mkdir -p '$(@D)'
	$(MAKE) -C '$(JS2300_ROOT)' \
		BUILD='$(abspath build/js2300)' OUT='$(abspath build/js2300-out)' \
		MQUICKJS_DIR='$(JS2300_MQUICKJS_DIR)' \
		CC='$(SF2000_CC)' AR='$(CROSS_COMPILE)ar' \
		CFLAGS='$(MUFROG_CORE_CFLAGS) -I$(abspath $(JS2300_ROOT)/include) \
			-I$(abspath build/js2300) -I$(abspath build/js2300/mquickjs)' \
		all
	cp '$(abspath build/js2300-out/libjs2300.a)' '$@'
	test -s '$@'

$(JS2300_CORE_SOURCE): $(JS2300_ROOT)/src/libretro_core/js2300_libretro_core.c \
		Makefile
	mkdir -p '$(@D)'
	cp '$<' '$@'
	sed -i 's#/media/mmcblk0/unifrog_data/scripts/js2300-cores#/mnt/sd/sf2000/js2300-cores#g' '$@'
	sed -i 's/#define JS2300_CORE_HEAP_BYTES (16u \* 1024u \* 1024u)/#define JS2300_CORE_HEAP_BYTES (8u * 1024u * 1024u)/' '$@'

$(JS2300_CORE_OBJECT): $(JS2300_CORE_SOURCE) include/libretro_min.h \
		$(JS2300_ROOT)/include/js2300/js2300.h include/unifrog/abi.h Makefile
	mkdir -p '$(@D)'
	$(SF2000_CC) $(SF2000_CFLAGS) -I$(COMMON_DIR)/include \
		-I$(abspath $(JS2300_ROOT)/include) -Iinclude -c -o '$@' '$<'

$(JS2300_CORE_FS_OBJECT): src/js2300_core_fs.c Makefile
	mkdir -p '$(@D)'
	$(SF2000_CC) $(SF2000_CFLAGS) -c -o '$@' '$<'

$(JS2300_CORE_EXECUTABLE): $(JS2300_RUNTIME) $(JS2300_CORE_OBJECT) \
		$(JS2300_CORE_FS_OBJECT) $(SF2000_HOST_OBJECTS)
	$(SF2000_CC) $(SF2000_LDFLAGS) -o '$@' \
		$(SF2000_STARTFILES) $(SF2000_HOST_OBJECTS) \
		$(JS2300_CORE_OBJECT) $(JS2300_CORE_FS_OBJECT) $(JS2300_RUNTIME) \
		$(LIBRETRO_COMMON) -lm $(SF2000_ENDFILES)
	$(SF2000_STRIP) --strip-unneeded '$@'

$(JS2300_UI_EXECUTABLE): $(JS2300_RUNTIME) src/js2300_runner.c \
		$(JS2300_ROOT)/include/js2300/js2300.h $(SF2000_HOST_OBJECTS) $(GE_SOURCES) \
		src/sf2000_input.c src/sf2000_browser_ui.c src/sf2000_log.c
	$(SF2000_CC) $(SF2000_CFLAGS) -I$(COMMON_DIR)/include \
		-I$(abspath $(JS2300_ROOT)/include) -I$(STB_DIR) \
		$(SF2000_LDFLAGS) -o '$@' $(SF2000_STARTFILES) \
		src/js2300_runner.c src/sf2000_input.c src/sf2000_browser_ui.c \
		src/sf2000_log.c $(GE_SOURCES) $(JS2300_RUNTIME) -lm \
		$(SF2000_ENDFILES)
	$(SF2000_STRIP) --strip-unneeded '$@'

.PHONY: js2300-ui js2300-core
js2300-ui: $(JS2300_UI_EXECUTABLE)
js2300-core: $(JS2300_CORE_EXECUTABLE)

gambatte: $(GAMBATTE_CORE) $(LIBRETRO_COMMON) $(SF2000_HOST_OBJECTS)
	$(SF2000_CXX) $(SF2000_LDFLAGS) \
		-o build/sf2000-gambatte $(SF2000_STARTFILES) \
		$(SF2000_HOST_OBJECTS) $(GAMBATTE_CORE) \
		$(LIBRETRO_COMMON) -lm -Wl,--wrap=malloc \
		-Wl,--wrap=calloc \
		-Wl,--wrap=realloc -Wl,--wrap=free $(SF2000_ENDFILES)

gpsp: $(GPSP_CORE) $(SF2000_HOST_OBJECTS)
	$(SF2000_CXX) $(SF2000_LDFLAGS) -o build/sf2000-gpsp \
		$(SF2000_STARTFILES) $(SF2000_HOST_OBJECTS) $(GPSP_CORE) -lm \
		-Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc \
		-Wl,--wrap=free $(SF2000_ENDFILES)

fceumm: $(FCEUMM_CORE) $(LIBRETRO_COMMON) $(SF2000_HOST_OBJECTS)
	$(SF2000_CXX) $(SF2000_LDFLAGS) -o build/sf2000-fceumm \
		$(SF2000_STARTFILES) $(SF2000_HOST_OBJECTS) $(FCEUMM_CORE) \
		$(LIBRETRO_COMMON) -lm \
		-Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc \
		-Wl,--wrap=free $(SF2000_ENDFILES)

quicknes: $(QUICKNES_CORE) $(LIBRETRO_COMMON) $(SF2000_HOST_OBJECTS)
	$(SF2000_CXX) $(SF2000_LDFLAGS) -o build/sf2000-quicknes \
		$(SF2000_STARTFILES) $(SF2000_HOST_OBJECTS) $(QUICKNES_CORE) \
		$(LIBRETRO_COMMON) -lm \
		-Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc \
		-Wl,--wrap=free $(SF2000_ENDFILES)

prosystem: $(PROSYSTEM_CORE) $(LIBRETRO_COMMON) $(SF2000_HOST_OBJECTS)
	$(SF2000_CXX) $(SF2000_LDFLAGS) -o build/sf2000-prosystem \
		$(SF2000_STARTFILES) $(SF2000_HOST_OBJECTS) $(PROSYSTEM_CORE) \
		$(LIBRETRO_COMMON) -lm \
		-Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc \
		-Wl,--wrap=free $(SF2000_ENDFILES)

snes9x2005: $(SNES9X2005_CORE) $(SF2000_HOST_OBJECTS)
	$(SF2000_CXX) $(SF2000_LDFLAGS) -o build/sf2000-snes9x2005 \
		$(SF2000_STARTFILES) $(SF2000_HOST_OBJECTS) $(SNES9X2005_CORE) \
		-lm -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc \
		-Wl,--wrap=free $(SF2000_ENDFILES)

snes9x2002: $(SNES9X2002_CORE) $(SNES9X2002_MEMORY) $(LIBRETRO_COMMON) \
		$(SF2000_HOST_OBJECTS)
	$(SF2000_CXX) $(SF2000_LDFLAGS) -o build/sf2000-snes9x2002 \
		$(SF2000_STARTFILES) $(SF2000_HOST_OBJECTS) $(SNES9X2002_CORE) \
		$(SNES9X2002_MEMORY) $(LIBRETRO_COMMON) -lm \
		-Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc \
		-Wl,--wrap=free $(SF2000_ENDFILES)

stella2014: $(STELLA_CORE) $(LIBRETRO_COMMON) $(SF2000_HOST_OBJECTS)
	$(SF2000_CXX) $(SF2000_LDFLAGS) -o build/sf2000-stella2014 \
		$(SF2000_STARTFILES) $(SF2000_HOST_OBJECTS) $(STELLA_CORE) \
		$(LIBRETRO_COMMON) -lm -Wl,--wrap=malloc -Wl,--wrap=calloc \
		-Wl,--wrap=realloc -Wl,--wrap=free $(SF2000_ENDFILES)

gearboy: $(GEARBOY_CORE) $(LIBRETRO_COMMON) $(SF2000_HOST_OBJECTS)
	$(SF2000_CXX) $(SF2000_LDFLAGS) -o build/sf2000-gearboy \
		$(SF2000_STARTFILES) $(SF2000_HOST_OBJECTS) $(GEARBOY_CORE) \
		$(LIBRETRO_COMMON) -lm -Wl,--wrap=malloc -Wl,--wrap=calloc \
		-Wl,--wrap=realloc -Wl,--wrap=free $(SF2000_ENDFILES)

pce-fast: $(PCE_FAST_CORE) $(LIBRETRO_COMMON) $(SF2000_HOST_OBJECTS)
	$(SF2000_CXX) $(SF2000_LDFLAGS) -o build/sf2000-pce-fast \
		$(SF2000_STARTFILES) $(SF2000_HOST_OBJECTS) $(PCE_FAST_CORE) \
		$(LIBRETRO_COMMON) -lm -Wl,--wrap=malloc -Wl,--wrap=calloc \
		-Wl,--wrap=realloc -Wl,--wrap=free $(SF2000_ENDFILES)

core-packages: gambatte gpsp fceumm quicknes prosystem snes9x2005 snes9x2002 stella2014 gearboy pce-fast mufrog-cores \
	$(JS2300_CORE_EXECUTABLE) $(JS2300_UI_EXECUTABLE)
	mkdir -p build/core-packages/licenses
	cp build/sf2000-gambatte build/core-packages/
	cp build/sf2000-gpsp build/core-packages/
	cp build/sf2000-fceumm build/core-packages/
	cp build/sf2000-quicknes build/core-packages/
	cp build/sf2000-prosystem build/core-packages/
	cp build/sf2000-snes9x2005 build/core-packages/
	cp build/sf2000-snes9x2002 build/core-packages/
	cp build/sf2000-stella2014 build/core-packages/
	cp build/sf2000-gearboy build/core-packages/
	cp build/sf2000-pce-fast build/core-packages/
	cp $(MUFROG_CORE_EXECUTABLES) build/core-packages/
	cp $(JS2300_CORE_EXECUTABLE) build/core-packages/
	cp $(JS2300_UI_EXECUTABLE) build/core-packages/
	mkdir -p build/core-packages/js2300-cores
	cp '$(JS2300_ROOT)/scripts/js2300-cores/chip8.js' '$(JS2300_SCRIPT)'
	cp $(GAMBATTE_DIR)/COPYING build/core-packages/licenses/gambatte-COPYING
	cp $(GPSP_DIR)/COPYING build/core-packages/licenses/gpsp-COPYING
	cp $(FCEUMM_DIR)/Copying build/core-packages/licenses/fceumm-Copying
	cp $(QUICKNES_DIR)/LICENSE build/core-packages/licenses/quicknes-LICENSE
	cp $(PROSYSTEM_DIR)/License.txt build/core-packages/licenses/prosystem-LICENSE
	cp $(SNES9X2005_DIR)/copyright build/core-packages/licenses/snes9x2005-copyright
	cp $(SNES9X2002_DIR)/src/copyright.h build/core-packages/licenses/snes9x2002-copyright.h
	cp $(STELLA_DIR)/stella/license.txt build/core-packages/licenses/stella2014-license.txt
	cp $(GEARBOY_DIR)/LICENSE build/core-packages/licenses/gearboy-LICENSE
	cp $(PCE_FAST_DIR)/COPYING build/core-packages/licenses/pce-fast-COPYING
	cp $(MUFROG_SOURCE_ROOT)/gpsp_multicore/COPYING build/core-packages/licenses/gpsp-multicore-COPYING
	cp $(MUFROG_SOURCE_ROOT)/picodrive/COPYING build/core-packages/licenses/picodrive-COPYING
	cp $(MUFROG_SOURCE_ROOT)/sf2000-qpsx-playstation-emulator/LICENSE build/core-packages/licenses/qpsx-LICENSE
	cp $(MUFROG_SOURCE_ROOT)/fbalpha2012/docs/COPYING build/core-packages/licenses/fbalpha2012-COPYING
	cp $(MUFROG_SOURCE_ROOT)/a5200/License.txt build/core-packages/licenses/a5200-License.txt
	cp $(MUFROG_SOURCE_ROOT)/libretro-atari800lib/atari800/COPYING build/core-packages/licenses/atari800lib-COPYING
	cp $(MUFROG_SOURCE_ROOT)/libretro-handy/lynx/license.txt build/core-packages/licenses/handy-license.txt
	cp $(MUFROG_SOURCE_ROOT)/RACE/license.txt build/core-packages/licenses/race-license.txt
	cp $(MUFROG_SOURCE_ROOT)/libretro-beetle-wswan/COPYING build/core-packages/licenses/beetle-cygne-COPYING
	cp $(MUFROG_SOURCE_ROOT)/Gearcoleco/LICENSE build/core-packages/licenses/gearcoleco-LICENSE
	cp $(MUFROG_SOURCE_ROOT)/libretro-frodo-prosty/COPYING build/core-packages/licenses/frodo-COPYING
	cp $(MUFROG_SOURCE_ROOT)/fake-08-prosty/LICENSE.MD build/core-packages/licenses/fake08-LICENSE.MD
	cp $(MUFROG_SOURCE_ROOT)/libretro-blueMSX-prosty/license.txt build/core-packages/licenses/bluemsx-license.txt
	cp $(MUFROG_SOURCE_ROOT)/snes9x2005-prosty/copyright build/core-packages/licenses/snes9x2005-prosty-copyright
	cp $(MUFROG_SOURCE_ROOT)/snes9x2002-prosty/src/copyright.h build/core-packages/licenses/snes9x2002-prosty-copyright.h
	cp $(MUFROG_SOURCE_ROOT)/libretro-gambatte-prosty/COPYING build/core-packages/licenses/gambatte-prosty-COPYING
	cp $(MUFROG_SOURCE_ROOT)/QuickNES_Core-prosty/LICENSE build/core-packages/licenses/quicknes-prosty-LICENSE
	cp $(MUFROG_SOURCE_ROOT)/libretro-fceumm-prosty/Copying build/core-packages/licenses/fceumm-prosty-Copying

build/host-main.o: src/main.c include/libretro_min.h include/sf2000_input.h \
		include/sf2000_browser_ui.h include/sf2000_log.h
	mkdir -p build
	$(SF2000_CC) $(SF2000_CFLAGS) -c -o $@ $<

build/host-input.o: src/sf2000_input.c include/sf2000_input.h include/libretro_min.h
	mkdir -p build
	$(SF2000_CC) $(SF2000_CFLAGS) -c -o $@ $<

build/host-pacer.o: src/sf2000_pacer.c include/sf2000_pacer.h
	mkdir -p build
	$(SF2000_CC) $(SF2000_CFLAGS) -c -o $@ $<

build/host-ui.o: src/sf2000_browser_ui.c include/sf2000_browser_ui.h \
		$(STB_DIR)/.git
	mkdir -p build
	$(SF2000_CC) $(SF2000_CFLAGS) -I$(STB_DIR) -c -o $@ $<

build/host-log.o: src/sf2000_log.c include/sf2000_log.h
	mkdir -p build
	$(SF2000_CC) $(SF2000_CFLAGS) -c -o $@ $<

build/host-ge-linux.o: $(GE_DIR)/hcge_linux.c
	mkdir -p build
	$(SF2000_CC) $(SF2000_CFLAGS) -c -o $@ $<

build/host-ge-node.o: $(GE_DIR)/hcge_node.c
	mkdir -p build
	$(SF2000_CC) $(SF2000_CFLAGS) -c -o $@ $<

build/host-audio.o: $(AUDIO_SOURCES)
	mkdir -p build
	$(SF2000_CC) $(SF2000_CFLAGS) -c -o $@ $(AUDIO_SOURCES)

build/host-retained.o: $(PLATFORM_SOURCES)
	mkdir -p build
	$(SF2000_CC) $(SF2000_CFLAGS) -c -o $@ $(PLATFORM_SOURCES)

build/host-nommu-new.o: src/nommu_new.cpp
	mkdir -p build
	$(SF2000_CXX) $(filter-out -std=c11,$(SF2000_CFLAGS)) -c -o $@ $<

build/host-content.o: src/content.c
	mkdir -p build
	$(SF2000_CC) $(SF2000_CFLAGS) -c -o $@ $<

$(GAMBATTE_DIR)/.git:
	mkdir -p .deps
	git clone --filter=blob:none https://github.com/libretro/gambatte-libretro.git $(GAMBATTE_DIR)
	git -C $(GAMBATTE_DIR) checkout --detach $(GAMBATTE_REV)

$(COMMON_DIR)/.git:
	mkdir -p .deps
	git clone --filter=blob:none https://github.com/libretro/libretro-common.git $(COMMON_DIR)
	git -C $(COMMON_DIR) checkout --detach $(COMMON_REV)

$(STB_DIR)/.git:
	mkdir -p .deps
	git clone --filter=blob:none https://github.com/nothings/stb.git $(STB_DIR)
	git -C $(STB_DIR) checkout --detach $(STB_REV)

$(STELLA_DIR)/.git:
	mkdir -p .deps
	git clone --filter=blob:none https://github.com/madcock/libretro-stella2014.git $(STELLA_DIR)
	git -C $(STELLA_DIR) checkout --detach $(STELLA_REV)

$(GEARBOY_DIR)/.git:
	mkdir -p .deps
	git clone --filter=blob:none https://github.com/drhelius/Gearboy.git $(GEARBOY_DIR)
	git -C $(GEARBOY_DIR) checkout --detach $(GEARBOY_REV)

$(PCE_FAST_DIR)/.git:
	mkdir -p .deps
	git clone --filter=blob:none https://github.com/libretro/beetle-pce-fast-libretro.git $(PCE_FAST_DIR)
	git -C $(PCE_FAST_DIR) checkout --detach $(PCE_FAST_REV)

$(GPSP_DIR)/.git:
	mkdir -p .deps
	git clone --filter=blob:none https://github.com/libretro/gpsp.git $(GPSP_DIR)
	git -C $(GPSP_DIR) checkout --detach $(GPSP_REV)

$(FCEUMM_DIR)/.git:
	mkdir -p .deps
	git clone --filter=blob:none https://github.com/libretro/libretro-fceumm.git $(FCEUMM_DIR)
	git -C $(FCEUMM_DIR) checkout --detach $(FCEUMM_REV)

$(QUICKNES_SOURCE_STAMP):
	mkdir -p .deps
	test -d $(QUICKNES_DIR)/.git || \
		git clone --filter=blob:none https://github.com/libretro/QuickNES_Core.git $(QUICKNES_DIR)
	git -C $(QUICKNES_DIR) checkout --detach $(QUICKNES_REV)
	touch '$@'

$(PROSYSTEM_DIR)/.git:
	mkdir -p .deps
	git clone --filter=blob:none https://github.com/libretro/prosystem-libretro.git $(PROSYSTEM_DIR)
	git -C $(PROSYSTEM_DIR) checkout --detach $(PROSYSTEM_REV)

$(SNES9X2005_DIR)/.git:
	mkdir -p .deps
	git clone --filter=blob:none https://github.com/libretro/snes9x2005.git $(SNES9X2005_DIR)
	git -C $(SNES9X2005_DIR) checkout --detach $(SNES9X2005_REV)

$(SNES9X2002_DIR)/.git:
	mkdir -p .deps
	git clone --filter=blob:none https://github.com/libretro/snes9x2002.git $(SNES9X2002_DIR)
	git -C $(SNES9X2002_DIR) checkout --detach $(SNES9X2002_REV)

$(GPSP_PATCH_STAMP): $(GPSP_DIR)/.git $(GPSP_PATCHES)
	git -C '$(GPSP_DIR)' reset --hard '$(GPSP_REV)'
	for patch_file in $(GPSP_PATCHES); do \
		patch -d '$(GPSP_DIR)' -p1 < "$$patch_file"; \
	done
	touch '$@'

$(GPSP_CORE): $(GPSP_PATCH_STAMP) Makefile
	mkdir -p build
	$(MAKE) -C $(GPSP_DIR) clean-objs platform=rs90 STATIC_LINKING=1
	$(MAKE) -C $(GPSP_DIR) cpu_threaded.o platform=rs90 STATIC_LINKING=1 \
		CC='$(SF2000_CC)' CXX='$(SF2000_CXX)' \
		AR='$(CROSS_COMPILE)ar' CFLAGS='$(GPSP_CFLAGS)' \
		OPTIMIZE='$(GPSP_TRANSLATOR_OPTIMIZE)'
	$(MAKE) -C $(GPSP_DIR) platform=rs90 STATIC_LINKING=1 \
		TARGET='$(abspath $@)' CC='$(SF2000_CC)' CXX='$(SF2000_CXX)' \
		AR='$(CROSS_COMPILE)ar' \
		CFLAGS='$(GPSP_CFLAGS)' \
		OPTIMIZE='-Os -DNDEBUG'
	@if $(CROSS_COMPILE)objdump -dr --disassemble=translate_block_arm \
		'$(GPSP_DIR)/cpu_threaded.o' | \
		grep -Eq '[[:space:]]jr[[:space:]]+(a[0-3]|v[01]|t[0-9]|s[0-8]|gp|sp|fp)'; then \
		echo 'gpSP: computed jump remains in NOMMU ARM translator' >&2; \
		exit 1; \
	fi

$(GAMBATTE_DIR)/.sf2000-patched: $(GAMBATTE_DIR)/.git $(GAMBATTE_PATCHES)
	for patch_file in $(GAMBATTE_PATCHES); do \
		if patch -d '$(GAMBATTE_DIR)' -p1 --dry-run < "$$patch_file" >/dev/null; then \
			patch -d '$(GAMBATTE_DIR)' -p1 < "$$patch_file"; \
		elif ! grep -q 'gb_instance' '$(GAMBATTE_DIR)/libgambatte/libretro/libretro.cpp'; then \
			exit 1; \
		fi; \
	done
	touch '$@'

$(GAMBATTE_CORE): $(GAMBATTE_DIR)/.sf2000-patched Makefile
	mkdir -p build
	$(MAKE) -C $(GAMBATTE_DIR) -f Makefile.libretro clean \
		platform=unix STATIC_LINKING=1
	CFLAGS='-Os -EL -march=mips32 -msoft-float -G0 -mabicalls -fPIC -ffast-math -ffunction-sections -fdata-sections' \
	CXXFLAGS='-Os -EL -march=mips32 -msoft-float -G0 -mabicalls -fPIC -ffast-math -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti' \
	$(MAKE) -C $(GAMBATTE_DIR) -f Makefile.libretro platform=unix \
		STATIC_LINKING=1 VIDEO_RGB565=1 HAVE_NETWORK=0 \
		CC='$(SF2000_CC)' CXX='$(SF2000_CXX)' \
		AR='$(CROSS_COMPILE)ar' TARGET='$(abspath $@)'

$(FCEUMM_PATCH_STAMP): $(FCEUMM_DIR)/.git $(FCEUMM_PATCHES)
	git -C '$(FCEUMM_DIR)' reset --hard '$(FCEUMM_REV)'
	for patch_file in $(FCEUMM_PATCHES); do \
		patch -d '$(FCEUMM_DIR)' -p1 < "$$patch_file"; \
	done
	touch '$@'

$(FCEUMM_CORE): $(FCEUMM_PATCH_STAMP) Makefile
	mkdir -p build
	$(MAKE) -C $(FCEUMM_DIR) clean -f Makefile.libretro STATIC_LINKING=1 platform=unix
	CFLAGS='-O2 -EL -march=mips32 -mtune=mips32 -msoft-float -G0 -mabicalls -fPIC -ffast-math -fno-strict-aliasing -ffunction-sections -fdata-sections -DFRONTEND_SUPPORTS_RGB565' \
	CXXFLAGS='-O2 -EL -march=mips32 -mtune=mips32 -msoft-float -G0 -mabicalls -fPIC -ffast-math -fno-strict-aliasing -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti' \
	$(MAKE) -C $(FCEUMM_DIR) -f Makefile.libretro STATIC_LINKING=1 platform=unix \
		WANT_32BPP=0 HAVE_NTSC=0 HAVE_HDPACK=0 \
		CC='$(SF2000_CC)' CXX='$(SF2000_CXX)' \
		AR='$(CROSS_COMPILE)ar' TARGET='$(abspath $@)'

$(QUICKNES_PATCH_STAMP): $(QUICKNES_SOURCE_STAMP) $(QUICKNES_PATCHES)
	git -C '$(QUICKNES_DIR)' reset --hard '$(QUICKNES_REV)'
	for patch_file in $(QUICKNES_PATCHES); do \
		patch -d '$(QUICKNES_DIR)' -p1 < "$$patch_file"; \
	done
	touch '$@'

$(QUICKNES_CORE): $(QUICKNES_PATCH_STAMP) Makefile
	mkdir -p build
	$(MAKE) -C $(QUICKNES_DIR) clean platform=unix STATIC_LINKING=1
	CFLAGS='-O2 -EL -march=mips32 -mtune=mips32 -msoft-float -G0 -mabicalls -fPIC -ffast-math -fno-strict-aliasing -ffunction-sections -fdata-sections -DSF2000 -DNO_UNALIGNED_ACCESS -DFRONTEND_SUPPORTS_RGB565' \
	CXXFLAGS='-O2 -EL -march=mips32 -mtune=mips32 -msoft-float -G0 -mabicalls -fPIC -ffast-math -fno-strict-aliasing -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -DSF2000 -DNO_UNALIGNED_ACCESS -DFRONTEND_SUPPORTS_RGB565' \
	$(MAKE) -C $(QUICKNES_DIR) platform=unix STATIC_LINKING=1 \
		CC='$(SF2000_CC)' CXX='$(SF2000_CXX)' \
		AR='$(CROSS_COMPILE)ar' TARGET='$(abspath $@)'

$(PROSYSTEM_PATCH_STAMP): $(PROSYSTEM_DIR)/.git $(PROSYSTEM_PATCHES)
	git -C '$(PROSYSTEM_DIR)' reset --hard '$(PROSYSTEM_REV)'
	for patch_file in $(PROSYSTEM_PATCHES); do \
		patch -d '$(PROSYSTEM_DIR)' -p1 < "$$patch_file"; \
	done
	touch '$@'

$(PROSYSTEM_CORE): $(PROSYSTEM_PATCH_STAMP) $(COMMON_DIR)/.git Makefile
	mkdir -p build
	$(MAKE) -C $(PROSYSTEM_DIR) clean platform=unix STATIC_LINKING=1
	CFLAGS='-Os -EL -march=mips32 -mtune=mips32 -mabi=32 -msoft-float \
		-G0 -mabicalls -fPIC -fomit-frame-pointer -ffast-math \
		-ffunction-sections -fdata-sections -fsigned-char \
		-I$(abspath $(PROSYSTEM_DIR)/core) \
		-I$(abspath $(COMMON_DIR)/include)' \
	$(MAKE) -C $(PROSYSTEM_DIR) platform=unix STATIC_LINKING=1 \
		CC='$(SF2000_CC)' AR='$(CROSS_COMPILE)ar' \
		LIBRETRO_COMM_DIR='$(abspath $(COMMON_DIR))' \
		BUPBOOP_DIR='$(abspath $(PROSYSTEM_DIR)/bupboop)' \
		TARGET='$(abspath $@)'

$(SNES9X2005_PATCH_STAMP): $(SNES9X2005_DIR)/.git $(SNES9X2005_PATCHES)
	git -C '$(SNES9X2005_DIR)' reset --hard '$(SNES9X2005_REV)'
	for patch_file in $(SNES9X2005_PATCHES); do \
		patch -d '$(SNES9X2005_DIR)' -p1 < "$$patch_file"; \
	done
	touch '$@'

$(SNES9X2005_CORE): $(SNES9X2005_PATCH_STAMP) Makefile
	mkdir -p build
	$(MAKE) -C $(SNES9X2005_DIR) clean platform=unix STATIC_LINKING=1
	CFLAGS='-O2 -EL -march=mips32 -mtune=mips32 -mabi=32 -msoft-float \
		-G0 -mabicalls -fPIC -fomit-frame-pointer -ffast-math \
		-fno-strict-aliasing -ffunction-sections -fdata-sections \
		-DSF2000_NOMMU' \
	CXXFLAGS='-O2 -EL -march=mips32 -mtune=mips32 -mabi=32 -msoft-float \
		-G0 -mabicalls -fPIC -fomit-frame-pointer -ffast-math \
		-fno-strict-aliasing -ffunction-sections -fdata-sections \
		-fno-exceptions -fno-rtti -DSF2000_NOMMU' \
	$(MAKE) -C $(SNES9X2005_DIR) platform=unix STATIC_LINKING=1 \
		CC='$(SF2000_CC)' CXX='$(SF2000_CXX)' \
		AR='$(CROSS_COMPILE)ar' fpic=-fPIC TARGET='$(abspath $@)'

$(SNES9X2002_CORE): $(SNES9X2002_DIR)/.git Makefile
	mkdir -p build
	$(MAKE) -C $(SNES9X2002_DIR) clean platform=unix STATIC_LINKING=1
	CFLAGS='-Os -EL -march=mips32 -mtune=mips32 -mabi=32 -msoft-float \
		-G0 -mabicalls -fPIC -fomit-frame-pointer -ffast-math \
		-fno-strict-aliasing -ffunction-sections -fdata-sections' \
	CXXFLAGS='-Os -EL -march=mips32 -mtune=mips32 -mabi=32 -msoft-float \
		-G0 -mabicalls -fPIC -fomit-frame-pointer -ffast-math \
		-fno-strict-aliasing -ffunction-sections -fdata-sections \
		-fno-exceptions -fno-rtti' \
	$(MAKE) -C $(SNES9X2002_DIR) platform=unix STATIC_LINKING=1 \
		CC='$(SF2000_CC)' CXX='$(SF2000_CXX)' \
		AR='$(CROSS_COMPILE)ar' fpic=-fPIC TARGET='$(abspath $@)'

$(STELLA_PATCH_STAMP): $(STELLA_DIR)/.git $(STELLA_PATCHES)
	git -C '$(STELLA_DIR)' reset --hard '$(STELLA_REV)'
	for patch_file in $(STELLA_PATCHES); do \
		patch -d '$(STELLA_DIR)' -p1 < "$$patch_file"; \
	done
	touch '$@'

$(STELLA_CORE): $(STELLA_PATCH_STAMP) $(COMMON_DIR)/.git Makefile
	mkdir -p build
	$(MAKE) -C $(STELLA_DIR) clean platform=sf2000 STATIC_LINKING=1
	$(MAKE) -C $(STELLA_DIR) platform=sf2000 STATIC_LINKING=1 \
		TARGET='$(abspath $@)' CC='$(SF2000_CC)' CXX='$(SF2000_CXX)' \
		AR='$(CROSS_COMPILE)ar' \
		CFLAGS='-Os -EL -march=mips32 -mtune=mips32 -mabi=32 -msoft-float \
		-G0 -mabicalls -fPIC -ffast-math -fomit-frame-pointer \
		-ffunction-sections -fdata-sections -DSF2000 -DHAVE_STRL \
		-DFRONTEND_SUPPORTS_RGB565 $(STELLA_INCLUDES)' \
		CXXFLAGS='-Os -EL -march=mips32 -mtune=mips32 -mabi=32 -msoft-float \
		-G0 -mabicalls -fPIC -ffast-math -fomit-frame-pointer \
		-ffunction-sections -fdata-sections -fno-exceptions -fno-rtti \
		-DSF2000 -DHAVE_STRL -DFRONTEND_SUPPORTS_RGB565 \
		$(STELLA_INCLUDES)'

$(GEARBOY_CORE): $(GEARBOY_DIR)/.git Makefile
	mkdir -p build
	$(MAKE) -C $(GEARBOY_DIR)/platforms/libretro clean \
		platform=unix STATIC_LINKING=1 CC='$(SF2000_CC)' \
		CXX='$(SF2000_CXX)' AR='$(CROSS_COMPILE)ar'
	CFLAGS='-Os -EL -march=mips32 -mtune=mips32 -mabi=32 -msoft-float \
		-G0 -mabicalls -fPIC -ffast-math -fomit-frame-pointer \
		-ffunction-sections -fdata-sections' \
	CXXFLAGS='-Os -EL -march=mips32 -mtune=mips32 -mabi=32 -msoft-float \
		-G0 -mabicalls -fPIC -ffast-math -fomit-frame-pointer \
		-ffunction-sections -fdata-sections -fno-exceptions -fno-rtti \
		-DPERFORMANCE' \
	$(MAKE) -C $(GEARBOY_DIR)/platforms/libretro platform=unix \
		STATIC_LINKING=1 CC='$(SF2000_CC)' CXX='$(SF2000_CXX)' \
		AR='$(CROSS_COMPILE)ar' TARGET='$(abspath $@)'

$(PCE_FAST_CORE): $(PCE_FAST_DIR)/.git Makefile
	mkdir -p build
	$(MAKE) -C $(PCE_FAST_DIR) clean platform=unix STATIC_LINKING=1 \
		CC='$(SF2000_CC)' CXX='$(SF2000_CXX)' AR='$(CROSS_COMPILE)ar'
	CFLAGS='$(PCE_FAST_CFLAGS)' CXXFLAGS='$(PCE_FAST_CXXFLAGS)' \
	$(MAKE) -C $(PCE_FAST_DIR) platform=unix STATIC_LINKING=1 HAVE_CHD=1 \
		CC='$(SF2000_CC)' CXX='$(SF2000_CXX)' AR='$(CROSS_COMPILE)ar' \
		GIT_VERSION='$(PCE_FAST_REV)' fpic= TARGET='$(abspath $@)'

define MUFROG_CORE_RULE
build/mufrog/src/$(1)/.source: Makefile $(MUFROG_$(call mufrog_key,$(1))_PATCHES)
	rm -rf '$$(@D)'
	mkdir -p '$$(@D)'
	test -d '$(MUFROG_$(call mufrog_key,$(1))_SOURCE)'
	tar -C '$(MUFROG_$(call mufrog_key,$(1))_SOURCE)' --exclude=.git \
		-cf - . | tar -C '$$(@D)' -xf -
	set -eu; for patch_file in $(MUFROG_$(call mufrog_key,$(1))_PATCHES); do \
		patch -d '$$(@D)' -p1 < "$$$$patch_file"; \
	done
	touch '$$@'

build/mufrog/raw/$(1).a: build/mufrog/src/$(1)/.source Makefile \
		src/mufrog_picodrive_config.h
	mkdir -p '$$(@D)'
	find 'build/mufrog/src/$(1)' -type f -name '*.o' -delete
	find 'build/mufrog/src/$(1)' -type f -name '*.a' -delete
	$(MAKE) -C 'build/mufrog/src/$(1)/$(MUFROG_$(call mufrog_key,$(1))_WORKDIR)' \
		-f '$(MUFROG_$(call mufrog_key,$(1))_MAKEFILE)' \
		platform=unix STATIC_LINKING=1 STATIC_LINKING_LINK=1 fpic=-fPIC \
		TARGET='$(abspath $$@)' CC='$(SF2000_CC)' CXX='$(SF2000_CXX)' \
		AR='$(CROSS_COMPILE)ar' \
		CFLAGS='$(MUFROG_CORE_CFLAGS) \
			-I$(abspath build/mufrog/src/$(1)) \
			-I$(abspath build/mufrog/src/$(1))/src \
			-I$(abspath build/mufrog/src/$(1))/emu \
			-I$(abspath build/mufrog/src/$(1))/libretro \
			-I$(abspath build/mufrog/src/$(1))/libretro/libretro-common/include \
			-I$(abspath build/mufrog/src/$(1))/platform/common/tremor \
			-I$(abspath build/mufrog/src/$(1))/zlib \
			$(MUFROG_CORE_INCLUDES) $(MUFROG_$(call mufrog_key,$(1))_EXTRA_CFLAGS)' \
		CXXFLAGS='$(MUFROG_CORE_CFLAGS) \
			-I$(abspath build/mufrog/src/$(1)) \
			-I$(abspath build/mufrog/src/$(1))/src \
			-I$(abspath build/mufrog/src/$(1))/emu \
			-I$(abspath build/mufrog/src/$(1))/libretro \
			-I$(abspath build/mufrog/src/$(1))/libretro/libretro-common/include \
			-I$(abspath build/mufrog/src/$(1))/platform/common/tremor \
			-I$(abspath build/mufrog/src/$(1))/zlib \
			$(MUFROG_CORE_INCLUDES) $(MUFROG_$(call mufrog_key,$(1))_EXTRA_CFLAGS)' \
		$(MUFROG_$(call mufrog_key,$(1))_ARGS) \
		$(MUFROG_$(call mufrog_key,$(1))_EXTRA_ARGS)
	test -s '$$@'

build/mufrog/$(1)_libretro_linux.a: build/mufrog/raw/$(1).a Makefile
	mkdir -p '$$(@D)'
	$(SF2000_OBJCOPY) $(foreach symbol,$(LIBRETRO_API_SYMBOLS),--redefine-sym $(symbol)=$(MUFROG_$(call mufrog_key,$(1))_PREFIX)_$(symbol)) \
		'$$<' '$$@'
	set -eu; symbols='$$@.symbols'; trap 'rm -f "$$$$symbols"' EXIT HUP INT TERM; \
	$(SF2000_NM) -g --defined-only '$$@' | awk '{print $$$$3}' > "$$$$symbols"; \
	for symbol in $(LIBRETRO_API_SYMBOLS); do \
		grep -Fqx '$(MUFROG_$(call mufrog_key,$(1))_PREFIX)_'$$$$symbol "$$$$symbols" || { \
			echo "missing renamed libretro entry point: $$$$symbol" >&2; exit 1; }; \
		if grep -Fqx "$$$$symbol" "$$$$symbols"; then \
			echo "unrenamed libretro entry point remains: $$$$symbol" >&2; exit 1; \
		fi; \
	done

build/mufrog/adapter-$(1).o: src/core_adapter.c include/libretro_min.h Makefile
	mkdir -p '$$(@D)'
	$(SF2000_CC) $(SF2000_CFLAGS) -DCORE_PREFIX=$(MUFROG_$(call mufrog_key,$(1))_PREFIX) -c -o '$$@' '$$<'

build/sf2000-$(1): build/mufrog/$(1)_libretro_linux.a \
		build/mufrog/adapter-$(1).o $(SF2000_HOST_OBJECTS) \
		$(LIBRETRO_COMMON) $(MUFROG_MEMORY_STREAM) \
		$(MUFROG_$(call mufrog_key,$(1))_ADAPTER_OBJECTS)
	$(SF2000_CXX) $(SF2000_LDFLAGS) -o '$$@' \
		$(SF2000_STARTFILES) $(SF2000_HOST_OBJECTS) \
		build/mufrog/adapter-$(1).o '$$<' $(MUFROG_MEMORY_STREAM) \
		$(MUFROG_$(call mufrog_key,$(1))_ADAPTER_OBJECTS) \
		$(LIBRETRO_COMMON) -lm \
		$(SF2000_ENDFILES)
	$(SF2000_STRIP) --strip-unneeded '$$@'
endef
$(foreach spec,$(MUFROG_CORE_SPECS),$(eval $(call MUFROG_CORE_RULE,$(word 1,$(subst :, ,$(spec))))))

build/mufrog/qpsx-adapter.o: src/qpsx_adapter.c Makefile
	mkdir -p '$(@D)'
	$(SF2000_CC) $(SF2000_CFLAGS) -c -o '$@' '$<'

build/mufrog/fake08-log.o: src/fake08_log.c Makefile
	mkdir -p '$(@D)'
	$(SF2000_CC) $(SF2000_CFLAGS) -c -o '$@' '$<'

build/mufrog/mame2000-libco.o: build/mufrog/src/mame2000/.source Makefile
	mkdir -p '$(@D)'
	$(SF2000_CC) $(MUFROG_CORE_CFLAGS) \
		-I$(abspath build/mufrog/src/mame2000/src/libretro/libretro-common/include) \
		-c -o '$@' \
		'build/mufrog/src/mame2000/src/libretro/libretro-common/libco/ucontext.c'

build/mufrog/libretro-memory-stream.o: \
		$(MUFROG_SOURCE_ROOT)/libretro-common/streams/memory_stream.c Makefile
	mkdir -p '$(@D)'
	$(SF2000_CC) $(MUFROG_CORE_CFLAGS) $(MUFROG_CORE_INCLUDES) -c -o '$@' '$<'

$(MUFROG_MEMORY_STREAM): build/mufrog/libretro-memory-stream.o
	mkdir -p '$(@D)'
	$(CROSS_COMPILE)ar rcs '$@' '$<'

mufrog-cores: $(MUFROG_CORE_EXECUTABLES)

$(SNES9X2002_MEMORY): $(SNES9X2002_DIR)/.git
	mkdir -p build
	$(SF2000_CC) $(filter-out -Werror,$(SF2000_CFLAGS)) \
		-I$(SNES9X2002_DIR)/libretro/libretro-common/include \
		-c -o '$@' \
		'$(SNES9X2002_DIR)/libretro/libretro-common/streams/memory_stream.c'

build/common/%.o: $(COMMON_DIR)/.git
	mkdir -p '$(dir $@)'
	$(SF2000_CC) $(filter-out -Werror,$(SF2000_CFLAGS)) -include stdlib.h \
		-I$(COMMON_DIR)/include -I$(MUFROG_SOURCE_ROOT)/picodrive/zlib \
		-c -o '$@' '$(COMMON_DIR)/$*.c'

build/utf8_compat.o: src/utf8_compat.c
	mkdir -p build
	$(SF2000_CC) $(SF2000_CFLAGS) -c -o '$@' '$<'

$(LIBRETRO_COMMON): $(COMMON_OBJECTS)
	$(CROSS_COMPILE)ar rcs '$@' $(COMMON_OBJECTS)

integrated: browser gambatte gpsp fceumm

clean:
	rm -rf build
