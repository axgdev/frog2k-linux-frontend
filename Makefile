CC ?= cc
CROSS_COMPILE ?= /tmp/sf2000-linux-next-buildroot/buildroot-sf2000/host/bin/mipsel-buildroot-uclinux-uclibc-
SF2000_CC ?= $(CROSS_COMPILE)gcc
SF2000_CXX ?= $(CROSS_COMPILE)g++
SF2000_FLTHDR ?= $(CROSS_COMPILE)flthdr
CORE ?=
FROGUI_CORE ?= ../mufrog-commandc/cores/output/frogui_libretro_sf2000.a
GAMBATTE_REV := 9b3b5e3cc18ec92f460d37dd551eaf90c55bfcea
COMMON_REV := 9e2af2c23ff2595f096e2f591ea49a9bcb65401d
GAMBATTE_DIR := .deps/gambatte
COMMON_DIR := .deps/libretro-common
GAMBATTE_CORE := build/gambatte_libretro_linux.a
GAMBATTE_PATCHES := $(wildcard patches/gambatte/*.patch)
COMMON_SOURCES := compat/compat_posix_string.c compat/compat_snprintf.c \
	compat/compat_strcasestr.c compat/compat_strl.c compat/fopen_utf8.c \
	file/file_path.c file/file_path_io.c \
	streams/file_stream.c streams/file_stream_transforms.c \
	string/stdstring.c time/rtime.c vfs/vfs_implementation.c
COMMON_OBJECTS := $(addprefix build/common/,$(COMMON_SOURCES:.c=.o)) build/utf8_compat.o
LIBRETRO_COMMON := build/libretro-common-linux.a
CFLAGS := -Os -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -Iinclude
SF2000_CFLAGS := $(CFLAGS) -march=mips32 -mabi=32 -msoft-float
SF2000_LDFLAGS := -static -Wl,-elf2flt=-r -Wl,--no-check-sections \
	-Wl,--gc-sections

.PHONY: all clean check sf2000 demo frogui browser gambatte integrated

all: check

build/frontend-check: src/main.c tests/dummy_core.c include/libretro_min.h
	mkdir -p build
	$(CC) $(CFLAGS) -o $@ src/main.c tests/dummy_core.c

check: build/frontend-check

sf2000:
	test -n "$(CORE)" || { echo 'set CORE=/path/to/libretro_core.a' >&2; exit 2; }
	mkdir -p build
	$(SF2000_CC) $(SF2000_CFLAGS) $(SF2000_LDFLAGS) -o build/sf2000-frontend \
		src/main.c $(CORE)
	$(SF2000_FLTHDR) -s 262144 build/sf2000-frontend

demo:
	mkdir -p build
	$(SF2000_CC) $(SF2000_CFLAGS) $(SF2000_LDFLAGS) -o build/sf2000-frontend-demo \
		src/main.c tests/dummy_core.c
	$(SF2000_FLTHDR) -s 262144 build/sf2000-frontend-demo

frogui:
	test -f "$(FROGUI_CORE)"
	mkdir -p build
	$(SF2000_CC) $(SF2000_CFLAGS) $(SF2000_LDFLAGS) \
		-o build/sf2000-frontend-frogui src/main.c src/frogui_adapter.c \
		$(FROGUI_CORE) -lm -Wl,--wrap=calloc -Wl,--wrap=free
	$(SF2000_FLTHDR) -s 524288 build/sf2000-frontend-frogui

browser:
	mkdir -p build
	$(SF2000_CC) $(SF2000_CFLAGS) $(SF2000_LDFLAGS) \
		-o build/sf2000-browser src/browser.c
	$(SF2000_FLTHDR) -s 131072 build/sf2000-browser

gambatte:
	$(MAKE) $(GAMBATTE_CORE) $(LIBRETRO_COMMON)
	mkdir -p build
	$(SF2000_CC) $(SF2000_CFLAGS) -c -o build/gambatte-host.o src/main.c
	$(SF2000_CXX) $(filter-out -std=c11,$(SF2000_CFLAGS)) \
		-c -o build/nommu-new.o src/nommu_new.cpp
	$(SF2000_CC) $(SF2000_CFLAGS) -c -o build/content.o src/content.c
	$(SF2000_CXX) $(SF2000_LDFLAGS) \
		-o build/sf2000-gambatte build/gambatte-host.o build/nommu-new.o $(GAMBATTE_CORE) \
		$(LIBRETRO_COMMON) build/content.o -lm -Wl,--wrap=malloc -Wl,--wrap=calloc \
		-Wl,--wrap=realloc -Wl,--wrap=free
	$(SF2000_FLTHDR) -s 524288 build/sf2000-gambatte

$(GAMBATTE_DIR)/.git:
	mkdir -p .deps
	git clone --filter=blob:none https://github.com/libretro/gambatte-libretro.git $(GAMBATTE_DIR)
	git -C $(GAMBATTE_DIR) checkout --detach $(GAMBATTE_REV)

$(COMMON_DIR)/.git:
	mkdir -p .deps
	git clone --filter=blob:none https://github.com/libretro/libretro-common.git $(COMMON_DIR)
	git -C $(COMMON_DIR) checkout --detach $(COMMON_REV)

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

integrated: browser gambatte

clean:
	rm -rf build
