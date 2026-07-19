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
copy, XRGB8888 conversion, and centered downscaling. RGB565 at native width is
a single row copy. Audio is deliberately consumed without blocking until the
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

## Boundaries and next checkpoints

- Kernel: framebuffer/GE, evdev, ALSA PCM, MMC, clocks and power.
- QEMU: faithful registers, timing, DMA and regression gates.
- SF2000 platform: small stable userspace interfaces over those drivers.
- This application: menus, game metadata, save policy and libretro callbacks.

The next application checkpoint is ALSA ring-buffer output followed by a GE
submission API that avoids CPU conversion/scaling. Menu work should begin only
after those platform interfaces are exercised in QEMU and on hardware.
