CC ?= cc
CROSS_COMPILE ?= /tmp/sf2000-linux-next-buildroot/buildroot-sf2000/host/bin/mipsel-buildroot-uclinux-uclibc-
SF2000_CC ?= $(CROSS_COMPILE)gcc
SF2000_FLTHDR ?= $(CROSS_COMPILE)flthdr
CORE ?=
FROGUI_CORE ?= ../mufrog-commandc/cores/output/frogui_libretro_sf2000.a
CFLAGS := -Os -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -Iinclude
SF2000_CFLAGS := $(CFLAGS) -march=mips32 -mabi=32 -msoft-float
SF2000_LDFLAGS := -static -Wl,-elf2flt=-r

.PHONY: all clean check sf2000 demo frogui

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

clean:
	rm -rf build
