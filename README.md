# SF2000 Linux Frontend

A small Linux/NOMMU frontend for statically linked libretro cores on the
SF2000. Hardware support belongs in `sf2000_linux`; this repository contains
only application policy, libretro hosting, and the future game menu.

The design borrows the clean host/core separation from the MIT-licensed
`mufrog-commandc` Linux frontend, but does not import its HCRTOS platform code.
Linux bFLT cannot rely on `dlopen`, so each deployable frontend is linked to a
core archive at build time. A later core launcher can package one bFLT per core
while sharing ROM/menu metadata.

Current bring-up implements the libretro lifecycle, ROM full-path loading,
absolute frame pacing, SF2000 evdev joypad mapping, direct RGB565 framebuffer
copy, XRGB8888 conversion, and centered downscaling. Native 320x240 RGB565 is
published with one contiguous copy, and identical frames are not repeatedly
written into the live scanout. Audio is deliberately consumed without blocking until the
Linux ALSA DMA service has a stable application ABI.

Build checks:

```
make check
```

Cross-build a statically linked core:

```
make sf2000 CORE=/path/to/core_libretro.a
```

The resulting `build/sf2000-frontend` is a MIPS32r1 soft-float bFLT. Invoke it
as `sf2000-frontend /mnt/sd/roms/game.ext`.

`make demo` builds a self-contained moving RGB565 test core. The integrated
Linux image launches it with START+R and START+L returns to the Linux console.

`make frogui` links the symbol-prefixed FrogUI core produced by the read-only
`mufrog-commandc` tree through a small Linux adapter. It provides the actual
themeable ROM browser rather than the synthetic pattern. The SF2000 Linux
image embeds this bFLT and exposes the SD card at FrogUI's established
`/media/mmcblk0` path. There is no standalone frontend file to place on the SD
card when using the integrated image: it is installed as
`/usr/bin/sf2000-frontend` inside `bisrv.asd` and START+R launches it. START+X
remains reserved for the SF2000 Linux graphics performance benchmark.
Core-selection requests are the next Linux frontend
service to implement; browsing, settings, rendering, and input already run.

## Boundaries and next checkpoints

- Kernel: framebuffer/GE, evdev, ALSA PCM, MMC, clocks and power.
- QEMU: faithful registers, timing, DMA and regression gates.
- SF2000 platform: small stable userspace interfaces over those drivers.
- This application: menus, game metadata, save policy and libretro callbacks.

The next application checkpoint is ALSA ring-buffer output followed by a GE
submission API that avoids CPU conversion/scaling. Menu work should begin only
after those platform interfaces are exercised in QEMU and on hardware.
