# SF2000 Linux Frontend

A small Linux/NOMMU frontend for statically linked libretro cores on the
SF2000. Hardware support belongs in `sf2000_linux`; this repository contains
only application policy, libretro hosting, and the future game menu.

The design borrows the clean host/core separation from the MIT-licensed
`mufrog-commandc` Linux frontend, but does not import its HCRTOS platform code.
Linux bFLT cannot rely on `dlopen`, so each deployable frontend is linked to a
core archive at build time. A later core launcher can package one bFLT per core
while sharing ROM/menu metadata.

The integrated application provides a native framebuffer file browser and a
statically linked Gambatte runner. Press START+R from the console, browse with
the DPAD, use A to open and B to go back. A `.gb` or `.gbc` file beneath a
case-insensitive `GB` or `GBC` directory launches Gambatte. START+L returns
from a game to the browser, then from the browser to the console.

The core host implements the libretro lifecycle, ROM full-path loading,
absolute frame pacing, SF2000 evdev joypad mapping, GE-accelerated RGB565
conversion/scaling, and 32 kHz mono ALSA DMA playback. It publishes a
PID-validated activity marker before ROM loading so low-priority system
profiling cannot contend with emulator reads or real-time audio on the single
SD/MMC channel. The integrated image covers browsing, ROM loading, emulation,
video, audio, input, saves, and clean process handoff.

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

`make integrated` fetches pinned Gambatte and libretro-common revisions and
builds both `build/sf2000-browser` and `build/sf2000-gambatte`. The Linux image
embeds both, so no frontend executable belongs on the SD card. Put a ROM at,
for example, `/GB/game.gb` or `/GBC/game.gbc` on its FAT partition.

`make frogui` remains a developer compatibility target for the symbol-prefixed FrogUI core produced by the read-only
`mufrog-commandc` tree through a small Linux adapter. It provides the actual
themeable ROM browser rather than the synthetic pattern. The SF2000 Linux
image. It is not the integrated menu because the imported core does not expose
a complete Linux game-launch contract. START+X remains reserved for the
SF2000 Linux graphics performance benchmark.

## Boundaries and next checkpoints

- Kernel: framebuffer/GE, evdev, ALSA PCM, MMC, clocks and power.
- QEMU: faithful registers, timing, DMA and regression gates.
- SF2000 platform: small stable userspace interfaces over those drivers.
- This application: menus, game metadata, save policy and libretro callbacks.

The next application checkpoint is ALSA ring-buffer output followed by a GE
submission API that avoids CPU conversion/scaling. Menu work should begin only
after those platform interfaces are exercised in QEMU and on hardware.
