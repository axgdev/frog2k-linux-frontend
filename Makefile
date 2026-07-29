CC ?= cc
CXX ?= c++
CROSS_COMPILE ?= /tmp/sf2000-linux-next-buildroot/buildroot-sf2000/host/bin/mipsel-buildroot-uclinux-uclibc-
SF2000_CC ?= $(CROSS_COMPILE)gcc
SF2000_CXX ?= $(CROSS_COMPILE)g++
SF2000_FLTHDR ?= $(CROSS_COMPILE)flthdr
CORE ?=
FROGUI_CORE ?= ../mufrog-commandc/cores/output/frogui_libretro_sf2000.a
GAMBATTE_REV := 9b3b5e3cc18ec92f460d37dd551eaf90c55bfcea
GPSP_REV := 5b6e751f4abf368509146cd143c949c1946ac1ae
COMMON_REV := 9e2af2c23ff2595f096e2f591ea49a9bcb65401d
GAMBATTE_DIR := .deps/gambatte
GPSP_DIR := .deps/gpsp
COMMON_DIR := .deps/libretro-common
SF2000_LINUX_DIR ?= ../sf2000_linux
GE_DIR := $(SF2000_LINUX_DIR)/ge
GE_SOURCES := $(GE_DIR)/hcge_linux.c $(GE_DIR)/hcge_node.c
AUDIO_DIR := $(SF2000_LINUX_DIR)/audio
AUDIO_SOURCES := $(AUDIO_DIR)/hc15xx_resampler.c
PLATFORM_DIR := $(SF2000_LINUX_DIR)/platform
PLATFORM_SOURCES := $(PLATFORM_DIR)/hc15xx_retained.c
FRONTEND_SOURCES := src/main.c src/sf2000_input.c src/sf2000_pacer.c
SF2000_HOST_OBJECTS := build/host-main.o build/host-input.o \
	build/host-pacer.o \
	build/host-ge-linux.o build/host-ge-node.o build/host-audio.o \
	build/host-retained.o build/host-nommu-new.o build/host-content.o
GAMBATTE_CORE := build/gambatte_libretro_linux.a
GPSP_CORE := build/gpsp_libretro_linux.a
GAMBATTE_PATCHES := $(wildcard patches/gambatte/*.patch)
GPSP_PATCHES := $(wildcard patches/gpsp/*.patch)
GPSP_PATCH_ID := $(shell sha256sum $(GPSP_PATCHES) | sha256sum | cut -c1-16)
GPSP_PATCH_STAMP := $(GPSP_DIR)/.sf2000-patched-$(GPSP_PATCH_ID)
GPSP_TRANSLATOR_OPTIMIZE := -Os -DNDEBUG -fno-expensive-optimizations \
	-fno-jump-tables -fno-tree-switch-conversion
GPSP_CFLAGS := -Os -EL -march=mips32 -mtune=mips32 -mabi=32 -msoft-float \
	-G0 -mno-abicalls -fno-pic -fomit-frame-pointer -ffast-math \
	-fsigned-char -fno-strict-aliasing -fwrapv \
	-fno-unwind-tables -fno-asynchronous-unwind-tables \
	-ffunction-sections -fdata-sections \
	-DMMAP_JIT_CACHE -DROM_BUFFER_SIZE=32 -DHAVE_STRINGS_H -DHAVE_STDINT_H \
	-DHAVE_INTTYPES_H -D__LIBRETRO__ -DINLINE=inline -DHAVE_DYNAREC \
	-DMIPS_ARCH -DGPSP_DYNAREC_SAFE_SMC_PATCH \
	-DGPSP_DYNAREC_SAFE_FALLBACK \
	-DGPSP_ROM_BUFFER_MMAP -DGPSP_ROM_BUFFER_DEVICE \
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
	-I$(GE_DIR) -I$(AUDIO_DIR) -I$(SF2000_LINUX_DIR)/include
SF2000_LDFLAGS := -static -Wl,-elf2flt=-r -Wl,--no-check-sections \
	-Wl,--gc-sections

.PHONY: all clean check sf2000 demo frogui browser gambatte gpsp integrated

all: check

build/frontend-check: $(FRONTEND_SOURCES) src/content.c tests/dummy_core.c include/libretro_min.h $(GE_SOURCES) $(AUDIO_SOURCES) $(PLATFORM_SOURCES)
	mkdir -p build
	$(CC) $(CFLAGS) -I$(GE_DIR) -I$(AUDIO_DIR) -I$(SF2000_LINUX_DIR)/include -o $@ \
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

check: build/frontend-check build/nommu-allocator-check build/input-check \
		build/pacer-check
	./build/nommu-allocator-check
	./build/input-check
	./build/pacer-check

sf2000:
	test -n "$(CORE)" || { echo 'set CORE=/path/to/libretro_core.a' >&2; exit 2; }
	mkdir -p build
	$(SF2000_CC) $(SF2000_CFLAGS) $(SF2000_LDFLAGS) -o build/sf2000-frontend \
		$(FRONTEND_SOURCES) $(GE_SOURCES) $(AUDIO_SOURCES) $(PLATFORM_SOURCES) $(CORE)
	$(SF2000_FLTHDR) -s 262144 build/sf2000-frontend

demo:
	mkdir -p build
	$(SF2000_CC) $(SF2000_CFLAGS) $(SF2000_LDFLAGS) -o build/sf2000-frontend-demo \
		$(FRONTEND_SOURCES) $(GE_SOURCES) $(AUDIO_SOURCES) $(PLATFORM_SOURCES) tests/dummy_core.c
	$(SF2000_FLTHDR) -s 262144 build/sf2000-frontend-demo

frogui:
	test -f "$(FROGUI_CORE)"
	mkdir -p build
	$(SF2000_CC) $(SF2000_CFLAGS) $(SF2000_LDFLAGS) \
		-o build/sf2000-frontend-frogui $(FRONTEND_SOURCES) $(GE_SOURCES) \
		$(AUDIO_SOURCES) $(PLATFORM_SOURCES) src/frogui_adapter.c \
		$(FROGUI_CORE) -lm -Wl,--wrap=calloc -Wl,--wrap=free
	$(SF2000_FLTHDR) -s 524288 build/sf2000-frontend-frogui

browser:
	mkdir -p build
	$(SF2000_CC) $(SF2000_CFLAGS) $(SF2000_LDFLAGS) \
		-o build/sf2000-browser src/browser.c
	$(SF2000_FLTHDR) -s 131072 build/sf2000-browser

gambatte: $(SF2000_HOST_OBJECTS)
	$(MAKE) $(GAMBATTE_CORE) $(LIBRETRO_COMMON)
	$(SF2000_CXX) $(SF2000_LDFLAGS) \
		-o build/sf2000-gambatte $(SF2000_HOST_OBJECTS) $(GAMBATTE_CORE) \
		$(LIBRETRO_COMMON) -lm -Wl,--wrap=malloc \
		-Wl,--wrap=calloc \
		-Wl,--wrap=realloc -Wl,--wrap=free
	$(SF2000_FLTHDR) -s 524288 build/sf2000-gambatte

gpsp: $(GPSP_CORE) $(SF2000_HOST_OBJECTS)
	$(SF2000_CXX) $(SF2000_LDFLAGS) -o build/sf2000-gpsp \
		$(SF2000_HOST_OBJECTS) $(GPSP_CORE) -lm \
		-Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc -Wl,--wrap=free
	$(SF2000_FLTHDR) -s 524288 build/sf2000-gpsp

build/host-main.o: src/main.c include/libretro_min.h include/sf2000_input.h
	mkdir -p build
	$(SF2000_CC) $(SF2000_CFLAGS) -c -o $@ $<

build/host-input.o: src/sf2000_input.c include/sf2000_input.h include/libretro_min.h
	mkdir -p build
	$(SF2000_CC) $(SF2000_CFLAGS) -c -o $@ $<

build/host-pacer.o: src/sf2000_pacer.c include/sf2000_pacer.h
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

$(GPSP_DIR)/.git:
	mkdir -p .deps
	git clone --filter=blob:none https://github.com/libretro/gpsp.git $(GPSP_DIR)
	git -C $(GPSP_DIR) checkout --detach $(GPSP_REV)

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

$(GAMBATTE_CORE): $(GAMBATTE_DIR)/.sf2000-patched
	mkdir -p build
	CFLAGS='-Os -EL -march=mips32 -msoft-float -G0 -mno-abicalls -fno-pic -ffast-math -ffunction-sections -fdata-sections' \
	CXXFLAGS='-Os -EL -march=mips32 -msoft-float -G0 -mno-abicalls -fno-pic -ffast-math -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti' \
	$(MAKE) -C $(GAMBATTE_DIR) -f Makefile.libretro platform=unix \
		STATIC_LINKING=1 VIDEO_RGB565=1 HAVE_NETWORK=0 fpic= \
		CC='$(SF2000_CC)' CXX='$(SF2000_CXX)' \
		AR='$(CROSS_COMPILE)ar' TARGET='$(abspath $@)'

build/common/%.o: $(COMMON_DIR)/.git
	mkdir -p '$(dir $@)'
	$(SF2000_CC) $(filter-out -Werror,$(SF2000_CFLAGS)) -include stdlib.h \
		-I$(COMMON_DIR)/include -c -o '$@' '$(COMMON_DIR)/$*.c'

build/utf8_compat.o: src/utf8_compat.c
	$(SF2000_CC) $(SF2000_CFLAGS) -c -o '$@' '$<'

$(LIBRETRO_COMMON): $(COMMON_OBJECTS)
	$(CROSS_COMPILE)ar rcs '$@' $(COMMON_OBJECTS)

integrated: browser gambatte gpsp

clean:
	rm -rf build
