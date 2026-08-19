CC ?= cc
CXX ?= c++
CROSS_COMPILE ?= /tmp/sf2000-linux-next-buildroot/buildroot-sf2000/host/bin/mipsel-buildroot-linux-uclibc-
JOBS ?= $(shell nproc 2>/dev/null || echo 2)
CCACHE ?= $(shell command -v ccache 2>/dev/null)
CCACHE_COMPILE := $(if $(strip $(CCACHE)),$(CCACHE) ,)
SF2000_CC ?= $(CCACHE_COMPILE)$(CROSS_COMPILE)gcc
SF2000_CXX ?= $(CCACHE_COMPILE)$(CROSS_COMPILE)g++
SF2000_OBJCOPY ?= $(CROSS_COMPILE)objcopy
SF2000_STRIP ?= $(CROSS_COMPILE)strip
SF2000_NM ?= $(CROSS_COMPILE)nm

# Compiler identity stamp.  Every cross-compiled artifact (host objects,
# browser, js2300, cores) depends on it, so switching toolchains - e.g.
# Buildroot's internal uClibc toolchain for the crosstool-ng frog-toolchain -
# rebuilds everything instead of linking objects built by the old compiler
# against the new toolchain's libraries.  This is a real failure mode:
# libstdc++ mangles std::fpos<mbstate_t> differently between toolchain
# builds, so a stale core silently fails to link (undefined reference).
TOOLCHAIN_STAMP := build/.toolchain-stamp
$(TOOLCHAIN_STAMP): FORCE
	mkdir -p build
	@set -eu; \
	tmp='$@.tmp'; \
	{ \
		printf 'SF2000_CC=%s\n' '$(SF2000_CC)'; \
		printf 'SF2000_CXX=%s\n' '$(SF2000_CXX)'; \
		cc='$(word 2,$(SF2000_CC))'; \
		[ -n "$$cc" ] || cc='$(word 1,$(SF2000_CC))'; \
		stat -c 'compiler=%n|size=%s|mtime=%y' "$$cc"; \
	} > "$$tmp"; \
	if cmp -s "$$tmp" '$@'; then rm -f "$$tmp"; else mv "$$tmp" '$@'; fi

.PHONY: FORCE
FORCE:

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
# Mufrog-family core sources are cloned from their own upstream repositories
# (see MUFROG_CORE_CLONES below) so the frontend builds without any external
# mufrog-commandc checkout.  Override MUFROG_ROOT to reuse an existing tree.
MUFROG_ROOT ?= $(abspath .deps/mufrog-commandc)
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
# The target sysroot intentionally omits zlib, so the only system-zlib
# translation unit (trans_stream_zlib.c) cannot be compiled for any core.
# Nothing in the static links references the zlib backends, and rzip_stream.o
# is only pulled in if actually referenced, so the archive excludes it too.
COMMON_SOURCES := $(filter-out streams/trans_stream_zlib.c,$(COMMON_SOURCES))
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
# These old cores predate GCC 14, which promotes several legacy warning
# classes to hard errors (implicit function declarations, incompatible
# pointer types).  Keep them as warnings for the whole core set.
MUFROG_CORE_CFLAGS += -Wno-error=implicit-function-declaration \
	-Wno-error=incompatible-pointer-types -Wno-error=implicit-int
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
# Mufrog-family core sources are cloned from their own upstream repositories at
# the exact commits the frontend patches were written against.  This mirrors the
# mufrog-commandc cores/manifest.mk pins without depending on that checkout.
# Format: id|checkout-directory|upstream-url|pinned-commit
MUFROG_CORE_CLONES := \
	gpsp-multicore|gpsp_multicore|https://github.com/tzubertowski/gpsp_multicore.git|63dd94953c27bb2664872331bbc7f212a088db4b \
	picodrive|picodrive|https://github.com/libretro/picodrive.git|f0d4a0118a9733a1f10bce5a4ac772c474f9300d \
	qpsx|sf2000-qpsx-playstation-emulator|git@github.com:axgdev/qpsx_linux_private.git|482347a610b6c41b67b3fad593d2d6ba2c87d567 \
	mame2000|libretro-mame2000|https://github.com/libretro/mame2000-libretro.git|905808fbcc3adf8c610c1c60f0e41ce4b35db1c5 \
	fbalpha2012|fbalpha2012|https://github.com/libretro/fbalpha2012.git|b7ac554c53561d41640372f23dab15cd6fc4f0c4 \
	a5200|a5200|https://github.com/libretro/a5200.git|0942c88d64cad6853b539f51b39060a9de0cbcab \
	atari800lib|libretro-atari800lib|https://github.com/nutki/libretro-atari800lib.git|c562f734f80bb47511e8321251751b8566bc1f0d \
	handy|libretro-handy|https://github.com/libretro/libretro-handy.git|65d6b865544cd441ef2bd18cde7bd834c23d0e48 \
	race|RACE|https://github.com/libretro/RACE.git|f65011e6639ccbbbb44b6ffa63ca50c070475df4 \
	beetle-cygne|libretro-beetle-wswan|https://github.com/libretro/beetle-wswan-libretro.git|32bf70a3032a138baa969c22445f4b7821632c30 \
	gearcoleco|Gearcoleco|https://github.com/drhelius/Gearcoleco.git|149d9687624f845de4f7690b145da172f87d115a \
	frodo|libretro-frodo-prosty|https://github.com/tzubertowski/libretro-frodo.git|e2de1193e420f00c3eb65a1182bb31aa58fdfebb \
	fake08|fake-08-prosty|https://github.com/tzubertowski/fake-08.git|b87983eaf7492fdd945f2897024e0bb725e1e15d \
	bluemsx|libretro-blueMSX-prosty|https://github.com/tzubertowski/libretro-blueMSX.git|0b47ea3e7370bab5766eaa7c470d21247da3764a \
	snes9x2005-prosty|snes9x2005-prosty|https://github.com/tzubertowski/snes9x2005.git|fa25aaf57a043e999f1bc3d9327a71c4cdb1d942 \
	snes9x2002-prosty|snes9x2002-prosty|https://github.com/tzubertowski/snes9x2002.git|864c7d26b6bc42f7d648d1ba68dfc37520878629 \
	gambatte-prosty|libretro-gambatte-prosty|https://github.com/tzubertowski/libretro-gambatte.git|9e8bbe6a9a5e2cb35cfe3a851aaa631a4760f2e3 \
	quicknes-prosty|QuickNES_Core-prosty|https://github.com/tzubertowski/QuickNES_Core.git|9a6852e768cbabfcaa884f2d69cd8ea8cea37b69 \
	fceumm-prosty|libretro-fceumm-prosty|https://github.com/tzubertowski/libretro-fceumm.git|e6111e684e7a7761f3f1d6c80d0a825e2c8cdc7e

# Shared libretro-common include root used by every mufrog core build.
MUFROG_LIBRETRO_COMMON_REV := e2e3eccfd245a04771e6a435320b42234c8cc4d7

mufrog_clone_dir = $(word 2,$(subst |, ,$(filter $(1)|%,$(MUFROG_CORE_CLONES))))
# Lookups key on the checkout-directory field (e.g. gpsp_multicore), which is
# what the clone stamps are named after.  (filter only honours a single %,
# so use findstring to locate the manifest entry instead.)
mufrog_clone_entry = $(strip $(foreach w,$(MUFROG_CORE_CLONES),$(if $(findstring |$(2)|,$(w)),$(w))))
mufrog_clone_url = $(word 3,$(subst |, ,$(call mufrog_clone_entry,x,$(2))))
mufrog_clone_rev = $(word 4,$(subst |, ,$(call mufrog_clone_entry,x,$(2))))

$(MUFROG_SOURCE_ROOT)/libretro-common/.git:
	mkdir -p '$(dir $@)'
	test -d '$(@D)/.git' || \
		git clone --filter=blob:none https://github.com/libretro/libretro-common.git '$(@D)'
	git -C '$(@D)' checkout --detach '$(MUFROG_LIBRETRO_COMMON_REV)'

$(MUFROG_SOURCE_ROOT)/libretro-common/include: $(MUFROG_SOURCE_ROOT)/libretro-common/.git

$(MUFROG_SOURCE_ROOT)/%.git: Makefile
	mkdir -p '$(dir $@)'
	test -d '$(@D)/.git' || \
		git clone --filter=blob:none '$(call mufrog_clone_url,x,$(notdir $(@D)))' '$(@D)'
	git -C '$(@D)' cat-file -e '$(call mufrog_clone_rev,x,$(notdir $(@D)))^{commit}' 2>/dev/null || \
		git -C '$(@D)' fetch --depth 1 \
			'$(call mufrog_clone_url,x,$(notdir $(@D)))' \
			'$(call mufrog_clone_rev,x,$(notdir $(@D)))'
	git -C '$(@D)' checkout --detach '$(call mufrog_clone_rev,x,$(notdir $(@D)))'
	git -C '$(@D)' submodule update --init --depth 1 --filter=blob:none --jobs '$(JOBS)'
	touch '$@'

# Any file inside a mufrog core checkout implies the clone stamp, and becomes
# buildable as soon as the clone materializes it.  The stamp rules above perform
# the actual clone; these recipes only verify that the referenced file really
# came from the checkout.  A plain -f test also rejects a directory at the path
# (which is what an accidental nested clone would leave behind).  The explicit
# libretro-common/.git and libretro-common/include rules take precedence over
# these patterns, and the $(MUFROG_SOURCE_ROOT)/%.git stamp rule has a shorter
# stem than the generic pattern below, so the stamps themselves are never
# matched here.
$(MUFROG_SOURCE_ROOT)/libretro-common/%: $(MUFROG_SOURCE_ROOT)/libretro-common/.git
	@test -f '$@'
$(MUFROG_SOURCE_ROOT)/%: $(MUFROG_SOURCE_ROOT)/%.git
	@test -f '$@'

MUFROG_picodrive_EXTRA_CFLAGS := -include$(SF2000_SYSROOT)/usr/include/wchar.h \
	-include$(abspath src/mufrog_picodrive_config.h) -DDR_MP3_NO_STDIO -DUSE_TREMOR \
	-DEMU_F68K -D_USE_CZ80 -DDRC_SH2
MUFROG_picodrive_EXTRA_CFLAGS += -O3
# picodrive-no-chd.patch adds a NO_CD_MEDIA switch; the SF2000 build skips the
# whole libchdr/zstd/lzma dependency stack (no CHD media on cartridge-only
# targets), which also avoids the vendored libchdr headers being shadowed by
# the shared libretro-common libchdr module on the include path.
MUFROG_picodrive_PATCHES := patches/mufrog/picodrive-no-chd.patch
MUFROG_picodrive_EXTRA_ARGS := NO_CD_MEDIA=1
QPSX_OPTIMIZE ?= -O2
MUFROG_qpsx_EXTRA_CFLAGS := -Isrc/ -Isrc/spu/spu_pcsxrearmed \
	-Isrc/gpu/gpu_unai -Isrc/gpu/gpulib -Isrc/plugin_lib \
	-Isrc/port/libretro -Ilibretro/core -Ilibretro/include \
	-DSF2000 -DGPU_UNAI -DSPU_PCSXREARMED -D__LIBRETRO__ -DHAVE_LIBRETRO \
	-DPSXREC -Dmips -DUSE_GPULIB -DHLE_BIOS -DXA_HACK -DNO_THREADS -DNO_ZLIB \
	-DQPSX_ENABLE_MIPS_DIRECT_MEM=1 \
	-DQPSX_ENABLE_MIPS_LSU_CACHING=1 \
	-DQPSX_ENABLE_MIPS_CONST_MEM=1 \
	-DQPSX_ENABLE_MIPS_PIC_ASM_DISPATCH=1 \
	-DQPSX_LINUX_CACHEFLUSH=1 \
	-DQPSX_LINUX_ALLOCATED_RAM=1 \
	-DQPSX_DISABLE_MIPS32R2_GPU_ASM=1 \
	-include$(abspath src/mufrog_qpsx_config.h) $(QPSX_OPTIMIZE) -mtune=24kc \
	-fno-semantic-interposition
# QPSX allocates psxM once during init and releases it only during deinit, so
# translated constant RAM addresses remain valid for the process lifetime.
# QPSX is linked into a static PIE, so disabling shared-library semantic
# interposition lets GCC bind internal hot-path calls directly and inline them.
# This common flag intentionally reaches both C and C++; keep it QPSX-scoped
# because the other Mufrog cores may rely on default semantics.
# QPSX's MIPS build has no C++ exceptions, RTTI, or threaded static
# initialization. Keep these C++-only switches out of the C compiler flags so
# the shared Mufrog rule remains warning-free for the C portions of the core.
MUFROG_qpsx_EXTRA_CXXFLAGS := \
	-fno-exceptions -fno-rtti -fno-threadsafe-statics -fno-use-cxa-atexit
# The upstream "sf2000" platform selects bare-metal GPU/GTE/PSX-memory
# assembly. Its GPU object explicitly uses MIPS32r2 EXT instructions, while
# HC15xx is MIPS32r1. The Linux build already supplies every required ABI and
# core flag above; the MIPS dynarec remains enabled by -DPSXREC -Dmips.
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
MUFROG_fake08_PATCHES := patches/mufrog/fake08-tostring-cxx11.patch \
	patches/mufrog/fake08-cxx17.patch \
	patches/mufrog/fake08-fix32-mips.patch
MUFROG_snes9x2005_prosty_EXTRA_CFLAGS := -I$(abspath build/mufrog/src/snes9x2005-prosty/source)
MUFROG_snes9x2002_prosty_EXTRA_CFLAGS := -I$(abspath build/mufrog/src/snes9x2002-prosty/source) -DUSE_SA1
MUFROG_snes9x2002_prosty_PATCHES := patches/mufrog/snes9x2002-rops.patch
MUFROG_bluemsx_EXTRA_CFLAGS := -Wno-error=incompatible-pointer-types
MUFROG_bluemsx_PATCHES := patches/mufrog/bluemsx-linux-compat.patch \
	patches/mufrog/bluemsx-srammapper-prototype.patch \
	patches/mufrog/bluemsx-savestate-signature.patch \
	patches/mufrog/bluemsx-zlib-always.patch
MUFROG_a5200_PATCHES := patches/mufrog/a5200-libretro-common-md5.patch
MUFROG_gambatte_prosty_EXTRA_CFLAGS := \
	-I$(abspath build/mufrog/src/gambatte-prosty/libgambatte/include) \
	-I$(abspath build/mufrog/src/gambatte-prosty/libgambatte/src) \
	-I$(abspath build/mufrog/src/gambatte-prosty/common) \
	-I$(abspath build/mufrog/src/gambatte-prosty/libgambatte/libretro) \
	-DHAVE_STDINT_H
MUFROG_fceumm_prosty_EXTRA_CFLAGS := -DFCEU_VERSION_NUMERIC=9813
MUFROG_mame2000_ADAPTER_OBJECTS := build/mufrog/mame2000-libco.o
MUFROG_mame2000_PATCHES := patches/mufrog/mame2000-libco-external.patch
MUFROG_mame2000_EXTRA_ARGS := LIBCO_EXTERNAL=1
MUFROG_mame2000_EXTRA_CFLAGS := -O3
MUFROG_fake08_ADAPTER_OBJECTS := build/mufrog/fake08-log.o
MUFROG_frodo_PATCHES := patches/mufrog/frodo-autoload-visibility.patch \
	patches/mufrog/frodo-sf2000-fixes.patch
MUFROG_atari800lib_PATCHES := patches/mufrog/atari800lib-implicit-decl.patch
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
	patches/mufrog/gpsp-mips-pic.patch
MUFROG_gpsp_multicore_EXTRA_ARGS := HAVE_DYNAREC=1 CPU_ARCH=mips MMAP_JIT_CACHE=1 SF2000=1

MUFROG_CORE_EXECUTABLES := $(foreach spec,$(MUFROG_CORE_SPECS),build/sf2000-$(word 1,$(subst :, ,$(spec))))
MUFROG_MEMORY_STREAM := build/mufrog/libretro-memory-stream.a
QPSX_DEV_SOURCE ?= $(abspath ../sf2000-qpsx-playstation-emulator)
QPSX_DEV_RAW := build/qpsx-dev/pcsx4all_libretro_sf2000.a
QPSX_DEV_ARCHIVE := build/qpsx-dev/qpsx_libretro_linux.a
QPSX_DEV_EXECUTABLE := build/sf2000-qpsx-dev
QPSX_DEV_FLAGS_STAMP := build/qpsx-dev/compiler.flags
JS2300_RUNTIME := build/js2300/libjs2300.a
JS2300_CORE_SOURCE := build/js2300/js2300_libretro_core.c
JS2300_CORE_OBJECT := build/js2300/js2300_libretro_core.o
JS2300_CORE_FS_OBJECT := build/js2300/js2300_core_fs.o
JS2300_CORE_EXECUTABLE := build/sf2000-js2300-core
JS2300_UI_EXECUTABLE := build/sf2000-js2300-ui
JS2300_SCRIPT := build/core-packages/js2300-cores/chip8.js
# chip8.js is vendored into resources/ (from mufrog js2300 branch v0.5.3-develop1-rebased,
# commit 5711f97): js2300-private main (pinned JS2300_REV) never shipped scripts/.

.PHONY: all clean check elf-audit gpsp-pic-audit qpsx-mips32r1-audit \
	qpsx-dev qpsx-dev-core qpsx-dev-clean qpsx-dev-mips32r1-audit qpsx-dev-package \
	sf2000 demo frogui browser \
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
	@if test -f build/sf2000-qpsx; then $(MAKE) qpsx-mips32r1-audit; fi

QPSX_AUDIT_EXECUTABLE ?= build/sf2000-qpsx

qpsx-mips32r1-audit: $(QPSX_AUDIT_EXECUTABLE)
	@set -e; \
	body="$$(mktemp)"; \
	trap 'rm -f "$$body"' EXIT HUP INT TERM; \
	$(CROSS_COMPILE)objdump -d -m mips:isa32r2 '$(QPSX_AUDIT_EXECUTABLE)' > "$$body"; \
	if grep -Eq '[[:space:]](ext|ins|rotr|rotrv|seb|seh|wsbh|rdhwr|synci|ehb|jr\.hb)[[:space:]]' "$$body"; then \
		echo 'QPSX contains MIPS32r2 instructions forbidden on HC15xx MIPS32r1' >&2; \
		exit 1; \
	fi

# Fast QPSX development path.  It compiles directly in the maintained fork,
# preserving its object files between invocations, and deliberately bypasses
# the disposable patched source tree and every unrelated core.  The recursive
# core make remains phony because it owns the fork's dependency graph.  Its
# transformed archive is installed with compare-and-replace, though, so a
# no-op recursive make does not force the much larger frontend PIE to relink.
# A compiler or flag change invalidates the QPSX objects; ordinary source edits
# remain fully incremental.  ccache makes that required clean rebuild cheap.
qpsx-dev-core:
	@test -d '$(QPSX_DEV_SOURCE)/.git' || { \
		echo 'QPSX_DEV_SOURCE must name the qpsx fork checkout' >&2; exit 2; }
	mkdir -p '$(dir $(QPSX_DEV_RAW))'
	@set -eu; \
	{ printf '%s\n' \
		'CC=$(SF2000_CC)' \
		'CXX=$(SF2000_CXX)' \
		'AR=$(CROSS_COMPILE)ar' \
		'CFLAGS=$(MUFROG_CORE_CFLAGS) $(MUFROG_CORE_INCLUDES) $(MUFROG_qpsx_EXTRA_CFLAGS)' \
		'CXXFLAGS=$(MUFROG_CORE_CFLAGS) $(MUFROG_CORE_INCLUDES) $(MUFROG_qpsx_EXTRA_CFLAGS) $(MUFROG_qpsx_EXTRA_CXXFLAGS)'; \
	} > '$(QPSX_DEV_FLAGS_STAMP).tmp'; \
	if ! cmp -s '$(QPSX_DEV_FLAGS_STAMP).tmp' '$(QPSX_DEV_FLAGS_STAMP)' 2>/dev/null; then \
		$(MAKE) -C '$(QPSX_DEV_SOURCE)' -f Makefile.libretro clean \
			platform=unix STATIC_LINKING=1 RECOMPILER=mips \
			TARGET='$(abspath $(QPSX_DEV_RAW))'; \
		mv '$(QPSX_DEV_FLAGS_STAMP).tmp' '$(QPSX_DEV_FLAGS_STAMP)'; \
	else \
		rm -f '$(QPSX_DEV_FLAGS_STAMP).tmp'; \
	fi
	$(MAKE) -C '$(QPSX_DEV_SOURCE)' -f Makefile.libretro \
		platform=unix STATIC_LINKING=1 STATIC_LINKING_LINK=1 fpic=-fPIC \
		TARGET='$(abspath $(QPSX_DEV_RAW))' \
		CC='$(SF2000_CC)' CXX='$(SF2000_CXX)' AR='$(CROSS_COMPILE)ar' \
		CFLAGS='$(MUFROG_CORE_CFLAGS) \
			-I$(QPSX_DEV_SOURCE) -I$(QPSX_DEV_SOURCE)/src \
			-I$(QPSX_DEV_SOURCE)/libretro \
			$(MUFROG_CORE_INCLUDES) $(MUFROG_qpsx_EXTRA_CFLAGS)' \
		CXXFLAGS='$(MUFROG_CORE_CFLAGS) \
			-I$(QPSX_DEV_SOURCE) -I$(QPSX_DEV_SOURCE)/src \
			-I$(QPSX_DEV_SOURCE)/libretro \
			$(MUFROG_CORE_INCLUDES) $(MUFROG_qpsx_EXTRA_CFLAGS) \
			$(MUFROG_qpsx_EXTRA_CXXFLAGS)'

$(QPSX_DEV_ARCHIVE): qpsx-dev-core
	@set -eu; \
	tmp='$@.tmp'; \
	$(SF2000_OBJCOPY) -D $(foreach symbol,$(LIBRETRO_API_SYMBOLS),--redefine-sym $(symbol)=qpsx_$(symbol)) \
		'$(QPSX_DEV_RAW)' "$$tmp"; \
	if cmp -s "$$tmp" '$@' 2>/dev/null; then \
		rm -f "$$tmp"; \
	else \
		mv "$$tmp" '$@'; \
	fi

$(QPSX_DEV_EXECUTABLE): $(SF2000_HOST_OBJECTS) $(LIBRETRO_COMMON) \
		$(MUFROG_MEMORY_STREAM) build/mufrog/adapter-qpsx.o \
		build/mufrog/qpsx-adapter.o $(QPSX_DEV_ARCHIVE) Makefile
	$(SF2000_CXX) $(SF2000_LDFLAGS) -o '$(QPSX_DEV_EXECUTABLE)' \
		$(SF2000_STARTFILES) $(SF2000_HOST_OBJECTS) \
		build/mufrog/adapter-qpsx.o '$(QPSX_DEV_ARCHIVE)' \
		$(MUFROG_MEMORY_STREAM) build/mufrog/qpsx-adapter.o \
		$(LIBRETRO_COMMON) -lm $(SF2000_ENDFILES)
	$(SF2000_STRIP) --strip-unneeded '$(QPSX_DEV_EXECUTABLE)'

qpsx-dev: $(QPSX_DEV_EXECUTABLE)

qpsx-dev-mips32r1-audit: qpsx-dev
	$(MAKE) QPSX_AUDIT_EXECUTABLE='$(QPSX_DEV_EXECUTABLE)' qpsx-mips32r1-audit

qpsx-dev-package: qpsx-dev-mips32r1-audit
	mkdir -p build/core-packages/licenses
	cp '$(QPSX_DEV_EXECUTABLE)' build/core-packages/sf2000-qpsx
	cp '$(QPSX_DEV_SOURCE)/LICENSE' build/core-packages/licenses/qpsx-LICENSE

qpsx-dev-clean:
	$(MAKE) -C '$(QPSX_DEV_SOURCE)' -f Makefile.libretro clean \
		platform=unix STATIC_LINKING=1 RECOMPILER=mips \
		TARGET='$(abspath $(QPSX_DEV_RAW))'
	rm -rf build/qpsx-dev '$(QPSX_DEV_EXECUTABLE)'

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

browser: $(STB_DIR)/.git $(GE_SOURCES) $(GE_DIR)/ge_api.h $(GE_DIR)/hcge_node.h  $(TOOLCHAIN_STAMP) \
		src/browser.c src/sf2000_browser_ui.c src/sf2000_log.c \
		include/sf2000_browser_ui.h include/sf2000_log.h
	mkdir -p build
	$(SF2000_CC) $(SF2000_CFLAGS) -I$(STB_DIR) $(SF2000_LDFLAGS) \
		-o build/sf2000-browser $(SF2000_STARTFILES) src/browser.c \
		src/sf2000_browser_ui.c src/sf2000_log.c $(GE_SOURCES) -lm \
		$(SF2000_ENDFILES)

$(JS2300_RUNTIME): $(JS2300_ROOT)/Makefile  $(TOOLCHAIN_STAMP) \
		$(JS2300_ROOT)/src/js2300_runtime.c \
		$(JS2300_ROOT)/js2300_stdlib_gen.c \
		$(JS2300_ROOT)/include/js2300/js2300.h \
		$(JS2300_ROOT)/.git \
		$(JS2300_MQUICKJS_DIR)/mquickjs.c \
		$(JS2300_MQUICKJS_DIR)/.git Makefile
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

$(JS2300_CORE_SOURCE): src/js2300_libretro_core.c Makefile
	mkdir -p '$(@D)'
	cp '$<' '$@'

$(JS2300_CORE_OBJECT): $(JS2300_CORE_SOURCE) include/libretro_min.h  $(TOOLCHAIN_STAMP) \
		$(JS2300_ROOT)/include/js2300/js2300.h include/unifrog/abi.h Makefile
	mkdir -p '$(@D)'
	$(SF2000_CC) $(SF2000_CFLAGS) -I$(COMMON_DIR)/include \
		-I$(abspath $(JS2300_ROOT)/include) -Iinclude -c -o '$@' '$<'

$(JS2300_CORE_FS_OBJECT): src/js2300_core_fs.c Makefile $(TOOLCHAIN_STAMP)
	mkdir -p '$(@D)'
	$(SF2000_CC) $(SF2000_CFLAGS) -c -o '$@' '$<'

$(JS2300_CORE_EXECUTABLE): $(JS2300_RUNTIME) $(JS2300_CORE_OBJECT)  $(TOOLCHAIN_STAMP) \
		$(JS2300_CORE_FS_OBJECT) $(SF2000_HOST_OBJECTS)
	$(SF2000_CC) $(SF2000_LDFLAGS) -o '$@' \
		$(SF2000_STARTFILES) $(SF2000_HOST_OBJECTS) \
		$(JS2300_CORE_OBJECT) $(JS2300_CORE_FS_OBJECT) $(JS2300_RUNTIME) \
		$(LIBRETRO_COMMON) -lm $(SF2000_ENDFILES)
	$(SF2000_STRIP) --strip-unneeded '$@'

$(JS2300_UI_EXECUTABLE): $(JS2300_RUNTIME) src/js2300_runner.c  $(TOOLCHAIN_STAMP) \
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
	cp resources/js2300-cores/chip8.js '$(JS2300_SCRIPT)'
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

build/host-main.o: src/main.c include/libretro_min.h include/sf2000_input.h  $(TOOLCHAIN_STAMP) \
		include/sf2000_browser_ui.h include/sf2000_log.h
	mkdir -p build
	$(SF2000_CC) $(SF2000_CFLAGS) -c -o $@ $<

build/host-input.o: src/sf2000_input.c include/sf2000_input.h include/libretro_min.h $(TOOLCHAIN_STAMP)
	mkdir -p build
	$(SF2000_CC) $(SF2000_CFLAGS) -c -o $@ $<

build/host-pacer.o: src/sf2000_pacer.c include/sf2000_pacer.h $(TOOLCHAIN_STAMP)
	mkdir -p build
	$(SF2000_CC) $(SF2000_CFLAGS) -c -o $@ $<

build/host-ui.o: src/sf2000_browser_ui.c include/sf2000_browser_ui.h  $(TOOLCHAIN_STAMP) \
		$(STB_DIR)/.git
	mkdir -p build
	$(SF2000_CC) $(SF2000_CFLAGS) -I$(STB_DIR) -c -o $@ $<

build/host-log.o: src/sf2000_log.c include/sf2000_log.h $(TOOLCHAIN_STAMP)
	mkdir -p build
	$(SF2000_CC) $(SF2000_CFLAGS) -c -o $@ $<

build/host-ge-linux.o: $(GE_DIR)/hcge_linux.c $(TOOLCHAIN_STAMP)
	mkdir -p build
	$(SF2000_CC) $(SF2000_CFLAGS) -c -o $@ $<

build/host-ge-node.o: $(GE_DIR)/hcge_node.c $(TOOLCHAIN_STAMP)
	mkdir -p build
	$(SF2000_CC) $(SF2000_CFLAGS) -c -o $@ $<

build/host-audio.o: $(AUDIO_SOURCES) $(TOOLCHAIN_STAMP)
	mkdir -p build
	$(SF2000_CC) $(SF2000_CFLAGS) -c -o $@ $(AUDIO_SOURCES)

build/host-retained.o: $(PLATFORM_SOURCES) $(TOOLCHAIN_STAMP)
	mkdir -p build
	$(SF2000_CC) $(SF2000_CFLAGS) -c -o $@ $(PLATFORM_SOURCES)

build/host-nommu-new.o: src/nommu_new.cpp $(TOOLCHAIN_STAMP)
	mkdir -p build
	$(SF2000_CXX) $(filter-out -std=c11,$(SF2000_CFLAGS)) -c -o $@ $<

build/host-content.o: src/content.c $(TOOLCHAIN_STAMP)
	mkdir -p build
	$(SF2000_CC) $(SF2000_CFLAGS) -c -o $@ $<

$(GAMBATTE_DIR)/.git:
	mkdir -p .deps
	git clone --filter=blob:none https://github.com/libretro/gambatte-libretro.git $(GAMBATTE_DIR)
	git -C $(GAMBATTE_DIR) checkout --detach $(GAMBATTE_REV)

JS2300_REV := 18c4718143ece724321165615019be65347a1466
MQUICKJS_REV := 203d5bb79789bc47b74855d9207415dab71661a0

# JS2300 and its embedded MQuickJS runtime are private repositories.  They are
# cloned here so the js2300-ui/js2300-core builds work from a fresh checkout
# without a separate mufrog-commandc bootstrap step.  The stamp rules below
# produce the exact files the build rules reference; make needs a rule for each
# missing prerequisite before it will run any recipe.
$(JS2300_ROOT)/.git:
	mkdir -p '$(MUFROG_ROOT)'
	test -d '$(JS2300_ROOT)/.git' || \
		git clone git@github.com:axgdev/js2300-private.git '$(JS2300_ROOT)'
	git -C '$(JS2300_ROOT)' checkout --detach '$(JS2300_REV)'
	patch -d '$(JS2300_ROOT)' -p1 < patches/mufrog/js2300-mquickjs-objects.patch

$(JS2300_ROOT)/Makefile $(JS2300_ROOT)/src/js2300_runtime.c \
$(JS2300_ROOT)/js2300_stdlib_gen.c $(JS2300_ROOT)/include/js2300/js2300.h: $(JS2300_ROOT)/.git

$(JS2300_MQUICKJS_DIR)/.git:
	mkdir -p '$(dir $(JS2300_MQUICKJS_DIR))'
	test -d '$(JS2300_MQUICKJS_DIR)/.git' || \
		git clone git@github.com:bellard/mquickjs.git '$(JS2300_MQUICKJS_DIR)'
	git -C '$(JS2300_MQUICKJS_DIR)' checkout --detach '$(MQUICKJS_REV)'
	patch -d '$(JS2300_MQUICKJS_DIR)' -p1 < patches/mufrog/mquickjs-date-constructor.patch

$(JS2300_MQUICKJS_DIR)/mquickjs.c: $(JS2300_MQUICKJS_DIR)/.git

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

$(GPSP_CORE): $(GPSP_PATCH_STAMP) Makefile $(TOOLCHAIN_STAMP)
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

$(GAMBATTE_CORE): $(GAMBATTE_DIR)/.sf2000-patched Makefile $(TOOLCHAIN_STAMP)
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

$(FCEUMM_CORE): $(FCEUMM_PATCH_STAMP) Makefile $(TOOLCHAIN_STAMP)
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

$(QUICKNES_CORE): $(QUICKNES_PATCH_STAMP) Makefile $(TOOLCHAIN_STAMP)
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

$(PROSYSTEM_CORE): $(PROSYSTEM_PATCH_STAMP) $(COMMON_DIR)/.git Makefile $(TOOLCHAIN_STAMP)
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

$(SNES9X2005_CORE): $(SNES9X2005_PATCH_STAMP) Makefile $(TOOLCHAIN_STAMP)
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

$(SNES9X2002_CORE): $(SNES9X2002_DIR)/.git Makefile $(TOOLCHAIN_STAMP)
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

$(STELLA_CORE): $(STELLA_PATCH_STAMP) $(COMMON_DIR)/.git Makefile $(TOOLCHAIN_STAMP)
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

$(GEARBOY_CORE): $(GEARBOY_DIR)/.git Makefile $(TOOLCHAIN_STAMP)
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

$(PCE_FAST_CORE): $(PCE_FAST_DIR)/.git Makefile $(TOOLCHAIN_STAMP)
	mkdir -p build
	$(MAKE) -C $(PCE_FAST_DIR) clean platform=unix STATIC_LINKING=1 \
		CC='$(SF2000_CC)' CXX='$(SF2000_CXX)' AR='$(CROSS_COMPILE)ar'
	CFLAGS='$(PCE_FAST_CFLAGS)' CXXFLAGS='$(PCE_FAST_CXXFLAGS)' \
	$(MAKE) -C $(PCE_FAST_DIR) platform=unix STATIC_LINKING=1 HAVE_CHD=1 \
		CC='$(SF2000_CC)' CXX='$(SF2000_CXX)' AR='$(CROSS_COMPILE)ar' \
		GIT_VERSION='$(PCE_FAST_REV)' fpic= TARGET='$(abspath $@)'

define MUFROG_CORE_RULE
build/mufrog/src/$(1)/.source: Makefile $(MUFROG_$(call mufrog_key,$(1))_PATCHES) \
		$(MUFROG_SOURCE_ROOT)/$(call mufrog_clone_dir,$(1))/.git
	rm -rf '$$(@D)'
	mkdir -p '$$(@D)'
	test -d '$(MUFROG_$(call mufrog_key,$(1))_SOURCE)'
	tar -C '$(MUFROG_$(call mufrog_key,$(1))_SOURCE)' --exclude=.git \
		-cf - . | tar -C '$$(@D)' -xf -
	set -eu; for patch_file in $(MUFROG_$(call mufrog_key,$(1))_PATCHES); do \
		patch -d '$$(@D)' -p1 < "$$$$patch_file"; \
	done
	touch '$$@'

build/mufrog/raw/$(1).a: build/mufrog/src/$(1)/.source Makefile  $(TOOLCHAIN_STAMP) \
		src/mufrog_picodrive_config.h
	mkdir -p '$$(@D)'
	find 'build/mufrog/src/$(1)' -type f -name '*.o' -delete
	find 'build/mufrog/src/$(1)' -type f -name '*.a' -delete
	rm -f '$$@'
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
			$(MUFROG_CORE_INCLUDES) $(MUFROG_$(call mufrog_key,$(1))_EXTRA_CFLAGS) \
			$(MUFROG_$(call mufrog_key,$(1))_EXTRA_CXXFLAGS)' \
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

build/mufrog/adapter-$(1).o: src/core_adapter.c include/libretro_min.h Makefile $(TOOLCHAIN_STAMP)
	mkdir -p '$$(@D)'
	$(SF2000_CC) $(SF2000_CFLAGS) -DCORE_PREFIX=$(MUFROG_$(call mufrog_key,$(1))_PREFIX) -c -o '$$@' '$$<'

build/sf2000-$(1): build/mufrog/$(1)_libretro_linux.a  $(TOOLCHAIN_STAMP) \
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

build/mufrog/qpsx-adapter.o: src/qpsx_adapter.c Makefile $(TOOLCHAIN_STAMP)
	mkdir -p '$(@D)'
	$(SF2000_CC) $(SF2000_CFLAGS) -c -o '$@' '$<'

build/mufrog/fake08-log.o: src/fake08_log.c Makefile $(TOOLCHAIN_STAMP)
	mkdir -p '$(@D)'
	$(SF2000_CC) $(SF2000_CFLAGS) -c -o '$@' '$<'

build/mufrog/mame2000-libco.o: build/mufrog/src/mame2000/.source Makefile $(TOOLCHAIN_STAMP)
	mkdir -p '$(@D)'
	$(SF2000_CC) $(MUFROG_CORE_CFLAGS) \
		-I$(abspath build/mufrog/src/mame2000/src/libretro/libretro-common/include) \
		-c -o '$@' \
		'build/mufrog/src/mame2000/src/libretro/libretro-common/libco/ucontext.c'

build/mufrog/libretro-memory-stream.o: \
		$(MUFROG_SOURCE_ROOT)/libretro-common/streams/memory_stream.c \
		$(TOOLCHAIN_STAMP) Makefile
	mkdir -p '$(@D)'
	$(SF2000_CC) $(MUFROG_CORE_CFLAGS) $(MUFROG_CORE_INCLUDES) -c -o '$@' '$<'

$(MUFROG_MEMORY_STREAM): build/mufrog/libretro-memory-stream.o $(TOOLCHAIN_STAMP)
	mkdir -p '$(@D)'
	$(CROSS_COMPILE)ar rcs '$@' '$<'

mufrog-cores: $(MUFROG_CORE_EXECUTABLES)

$(SNES9X2002_MEMORY): $(SNES9X2002_DIR)/.git $(TOOLCHAIN_STAMP)
	mkdir -p build
	$(SF2000_CC) $(filter-out -Werror,$(SF2000_CFLAGS)) \
		-I$(SNES9X2002_DIR)/libretro/libretro-common/include \
		-c -o '$@' \
		'$(SNES9X2002_DIR)/libretro/libretro-common/streams/memory_stream.c'

build/common/%.o: $(COMMON_DIR)/.git $(MUFROG_SOURCE_ROOT)/picodrive/.git $(TOOLCHAIN_STAMP)
	mkdir -p '$(dir $@)'
	$(SF2000_CC) $(filter-out -Werror,$(SF2000_CFLAGS)) -include stdlib.h \
		-I$(COMMON_DIR)/include -I$(MUFROG_SOURCE_ROOT)/picodrive/zlib \
		-c -o '$@' '$(COMMON_DIR)/$*.c'

build/utf8_compat.o: src/utf8_compat.c
	mkdir -p build
	$(SF2000_CC) $(SF2000_CFLAGS) -c -o '$@' '$<'

$(LIBRETRO_COMMON): $(COMMON_OBJECTS) $(TOOLCHAIN_STAMP)
	$(CROSS_COMPILE)ar rcs '$@' $(COMMON_OBJECTS)

integrated: browser gambatte gpsp fceumm

clean:
	rm -rf build
