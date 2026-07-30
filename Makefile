CC ?= cc
CXX ?= c++
CROSS_COMPILE ?= /tmp/sf2000-linux-next-buildroot/buildroot-sf2000/host/bin/mipsel-buildroot-linux-uclibc-
SF2000_CC ?= $(CROSS_COMPILE)gcc
SF2000_CXX ?= $(CROSS_COMPILE)g++
CORE ?=
FROGUI_CORE ?= ../mufrog-commandc/cores/output/frogui_libretro_sf2000.a
GAMBATTE_REV := 9b3b5e3cc18ec92f460d37dd551eaf90c55bfcea
GPSP_REV := 5b6e751f4abf368509146cd143c949c1946ac1ae
FCEUMM_REV := b5e3566515c27dc66c9c20572171673126532e06
QUICKNES_REV := 7848e1ac22b1c69d056ae4cb57710651ff1dd169
PROSYSTEM_REV := 4202ac5bdb2ce1a21f84efc0e26d75bb5aa7e248
SNES9X2005_REV := b60356971fc9caae02cd0853676dced886a08be7
SNES9X2002_REV := 39e0d8c6daf4b1b1302eeecfee8309570aeb6a82
COMMON_REV := 9e2af2c23ff2595f096e2f591ea49a9bcb65401d
STB_REV := 31c1ad37456438565541f4919958214b6e762fb4
GAMBATTE_DIR := .deps/gambatte
GPSP_DIR := .deps/gpsp
FCEUMM_DIR := .deps/fceumm
QUICKNES_DIR := .deps/quicknes
PROSYSTEM_DIR := .deps/prosystem
SNES9X2005_DIR := .deps/snes9x2005
SNES9X2002_DIR := .deps/snes9x2002
QUICKNES_SOURCE_STAMP := $(QUICKNES_DIR)/.sf2000-source
COMMON_DIR := .deps/libretro-common
STB_DIR := .deps/stb
SF2000_LINUX_DIR ?= ../sf2000_linux
GE_DIR := $(SF2000_LINUX_DIR)/ge
GE_SOURCES := $(GE_DIR)/hcge_linux.c $(GE_DIR)/hcge_node.c
AUDIO_DIR := $(SF2000_LINUX_DIR)/audio
AUDIO_SOURCES := $(AUDIO_DIR)/hc15xx_resampler.c
PLATFORM_DIR := $(SF2000_LINUX_DIR)/platform
PLATFORM_SOURCES := $(PLATFORM_DIR)/hc15xx_retained.c
FRONTEND_SOURCES := src/main.c src/sf2000_input.c src/sf2000_pacer.c \
	src/sf2000_browser_ui.c
SF2000_HOST_OBJECTS := build/host-main.o build/host-input.o \
	build/host-pacer.o \
	build/host-ui.o \
	build/host-ge-linux.o build/host-ge-node.o build/host-audio.o \
	build/host-retained.o build/host-nommu-new.o build/host-content.o
GAMBATTE_CORE := build/gambatte_libretro_linux.a
GPSP_CORE := build/gpsp_libretro_linux.a
FCEUMM_CORE := build/fceumm_libretro_linux.a
QUICKNES_CORE := build/quicknes_libretro_linux.a
PROSYSTEM_CORE := build/prosystem_libretro_linux.a
SNES9X2005_CORE := build/snes9x2005_libretro_linux.a
SNES9X2002_CORE := build/snes9x2002_libretro_linux.a
SNES9X2002_MEMORY := build/snes9x2002-memory-stream.o
GAMBATTE_PATCHES := $(wildcard patches/gambatte/*.patch)
GPSP_PATCHES := $(wildcard patches/gpsp/*.patch)
FCEUMM_PATCHES := $(wildcard patches/fceumm/*.patch)
QUICKNES_PATCHES := $(wildcard patches/quicknes/*.patch)
PROSYSTEM_PATCHES := $(wildcard patches/prosystem/*.patch)
GPSP_PATCH_ID := $(shell sha256sum $(GPSP_PATCHES) | sha256sum | cut -c1-16)
GPSP_PATCH_STAMP := $(GPSP_DIR)/.sf2000-patched-$(GPSP_PATCH_ID)
FCEUMM_PATCH_ID := $(shell sha256sum $(FCEUMM_PATCHES) | sha256sum | cut -c1-16)
FCEUMM_PATCH_STAMP := $(FCEUMM_DIR)/.sf2000-patched-$(FCEUMM_PATCH_ID)
QUICKNES_PATCH_ID := $(shell sha256sum $(QUICKNES_PATCHES) | sha256sum | cut -c1-16)
QUICKNES_PATCH_STAMP := $(QUICKNES_DIR)/.sf2000-patched-$(QUICKNES_PATCH_ID)
PROSYSTEM_PATCH_ID := $(shell sha256sum $(PROSYSTEM_PATCHES) | sha256sum | cut -c1-16)
PROSYSTEM_PATCH_STAMP := $(PROSYSTEM_DIR)/.sf2000-patched-$(PROSYSTEM_PATCH_ID)
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
	file/file_path.c file/file_path_io.c \
	streams/file_stream.c streams/file_stream_transforms.c \
	string/stdstring.c time/rtime.c vfs/vfs_implementation.c
COMMON_OBJECTS := $(addprefix build/common/,$(COMMON_SOURCES:.c=.o)) build/utf8_compat.o
LIBRETRO_COMMON := build/libretro-common-linux.a
CFLAGS := -Os -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -Iinclude
SF2000_CFLAGS := $(CFLAGS) -march=mips32 -mabi=32 -msoft-float \
	-fPIC -mabicalls \
	-I$(GE_DIR) -I$(AUDIO_DIR) -I$(SF2000_LINUX_DIR)/include
SF2000_SYSROOT ?= $(shell $(SF2000_CC) -print-sysroot)
SF2000_CRT_DIR ?= $(SF2000_SYSROOT)/usr/lib
SF2000_STARTFILES = $(SF2000_CRT_DIR)/rcrt1.o $(SF2000_CRT_DIR)/crti.o \
	$(shell $(SF2000_CC) -print-file-name=crtbeginS.o)
SF2000_ENDFILES = $(shell $(SF2000_CC) -print-file-name=crtendS.o) \
	$(SF2000_CRT_DIR)/crtn.o
SF2000_LDFLAGS := -nostartfiles -static -Wl,-pie \
	-Wl,--no-dynamic-linker -Wl,-z,text \
	-Wl,--gc-sections

.PHONY: all clean check elf-audit gpsp-pic-audit sf2000 demo frogui browser \
	gambatte gpsp fceumm quicknes prosystem snes9x2005 snes9x2002 \
	core-packages integrated

all: check

build/frontend-check: $(FRONTEND_SOURCES) src/content.c tests/dummy_core.c include/libretro_min.h $(GE_SOURCES) $(AUDIO_SOURCES) $(PLATFORM_SOURCES) $(STB_DIR)/.git
	mkdir -p build
	$(CC) $(CFLAGS) -I$(STB_DIR) -I$(GE_DIR) -I$(AUDIO_DIR) -I$(SF2000_LINUX_DIR)/include -o $@ \
		$(FRONTEND_SOURCES) src/content.c tests/dummy_core.c $(GE_SOURCES) $(AUDIO_SOURCES) $(PLATFORM_SOURCES)

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

check: build/frontend-check build/nommu-allocator-check build/input-check \
		build/pacer-check build/browser-ui-check
	./build/nommu-allocator-check
	./build/input-check
	./build/pacer-check
	./build/browser-ui-check

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

sf2000: $(STB_DIR)/.git
	test -n "$(CORE)" || { echo 'set CORE=/path/to/libretro_core.a' >&2; exit 2; }
	mkdir -p build
	$(SF2000_CC) $(SF2000_CFLAGS) -I$(STB_DIR) $(SF2000_LDFLAGS) \
		-o build/sf2000-frontend \
		$(SF2000_STARTFILES) $(FRONTEND_SOURCES) $(GE_SOURCES) \
		$(AUDIO_SOURCES) $(PLATFORM_SOURCES) src/content.c \
		$(CORE) -lm $(SF2000_ENDFILES)

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

browser: $(STB_DIR)/.git
	mkdir -p build
	$(SF2000_CC) $(SF2000_CFLAGS) -I$(STB_DIR) $(SF2000_LDFLAGS) \
		-o build/sf2000-browser $(SF2000_STARTFILES) src/browser.c \
		src/sf2000_browser_ui.c -lm \
		$(SF2000_ENDFILES)

gambatte: $(SF2000_HOST_OBJECTS)
	$(MAKE) $(GAMBATTE_CORE) $(LIBRETRO_COMMON)
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

core-packages: quicknes prosystem snes9x2005 snes9x2002
	mkdir -p build/core-packages/licenses
	cp build/sf2000-quicknes build/core-packages/
	cp build/sf2000-prosystem build/core-packages/
	cp build/sf2000-snes9x2005 build/core-packages/
	cp build/sf2000-snes9x2002 build/core-packages/
	cp $(QUICKNES_DIR)/LICENSE build/core-packages/licenses/quicknes-LICENSE
	cp $(PROSYSTEM_DIR)/License.txt build/core-packages/licenses/prosystem-LICENSE
	cp $(SNES9X2005_DIR)/copyright build/core-packages/licenses/snes9x2005-copyright
	cp $(SNES9X2002_DIR)/src/copyright.h build/core-packages/licenses/snes9x2002-copyright.h

build/host-main.o: src/main.c include/libretro_min.h include/sf2000_input.h
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

$(SNES9X2005_CORE): $(SNES9X2005_DIR)/.git Makefile
	mkdir -p build
	$(MAKE) -C $(SNES9X2005_DIR) clean platform=unix STATIC_LINKING=1
	CFLAGS='-O2 -EL -march=mips32 -mtune=mips32 -mabi=32 -msoft-float \
		-G0 -mabicalls -fPIC -fomit-frame-pointer -ffast-math \
		-fno-strict-aliasing -ffunction-sections -fdata-sections' \
	CXXFLAGS='-O2 -EL -march=mips32 -mtune=mips32 -mabi=32 -msoft-float \
		-G0 -mabicalls -fPIC -fomit-frame-pointer -ffast-math \
		-fno-strict-aliasing -ffunction-sections -fdata-sections \
		-fno-exceptions -fno-rtti' \
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

$(SNES9X2002_MEMORY): $(SNES9X2002_DIR)/.git
	mkdir -p build
	$(SF2000_CC) $(filter-out -Werror,$(SF2000_CFLAGS)) \
		-I$(SNES9X2002_DIR)/libretro/libretro-common/include \
		-c -o '$@' \
		'$(SNES9X2002_DIR)/libretro/libretro-common/streams/memory_stream.c'

build/common/%.o: $(COMMON_DIR)/.git
	mkdir -p '$(dir $@)'
	$(SF2000_CC) $(filter-out -Werror,$(SF2000_CFLAGS)) -include stdlib.h \
		-I$(COMMON_DIR)/include -c -o '$@' '$(COMMON_DIR)/$*.c'

build/utf8_compat.o: src/utf8_compat.c
	$(SF2000_CC) $(SF2000_CFLAGS) -c -o '$@' '$<'

$(LIBRETRO_COMMON): $(COMMON_OBJECTS)
	$(CROSS_COMPILE)ar rcs '$@' $(COMMON_OBJECTS)

integrated: browser gambatte gpsp fceumm

clean:
	rm -rf build
