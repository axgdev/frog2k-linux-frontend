# SF2000 Linux Frontend

The module boundaries and real-time ownership rules are documented in
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md).

A small Linux/NOMMU frontend for statically linked libretro cores on the
SF2000. Hardware support belongs in `sf2000_linux`; this repository contains
only application policy, libretro hosting, and the future game menu.

The design borrows the clean host/core separation from the MIT-licensed
`mufrog-commandc` Linux frontend, but does not import its HCRTOS platform code.
Linux/NOMMU cannot rely on `dlopen`, so each deployable static PIE ELF is
linked to one core archive at build time. The browser selects the appropriate
executable while sharing ROM/menu metadata.

The integrated application provides a native framebuffer home menu, file
browser, and statically linked runners. Normal SF2000 Linux boot enters it
directly. Library opens the SD browser; browse with the DPAD, use A to open,
and B to go back. Settings identifies the active language/configuration.
Reset and Safe Shutdown are explicit system actions. A `.gb` or `.gbc` file
beneath a case-insensitive `GB` or `GBC` directory launches Gambatte.
Files below a case-insensitive `GBA` directory launch the statically linked
gpSP runner with its MIPS dynarec enabled. Files below a case-insensitive
`NES` directory launch FCEUmm.

The browser uses a small UTF-8/TrueType renderer rather than a GUI framework.
It caches rasterized glyphs, sleeps in `poll()` while idle, and renders only on
input. English, Spanish, Portuguese, Polish, Vietnamese, and Japanese UI
labels are built in. Theme colors, font path, and font size come from the
platform's single `/sf2000.conf`; RGB565 and RGB888 colors are accepted. The
Linux SD artifact stages a Unicode font separately so it does not increase ASD
size or boot time. ROM paths are passed in an `execve` argument vector, so
spaces and non-ASCII bytes never pass through shell parsing.

Validated GB/GBC, GBA, and NES runners remain in the boot image. Other systems
are routed by directory and extension to independent static-PIE packages below
`/sf2000/cores` on the card. A missing package produces a localized error
instead of a frozen selection. This keeps new cores independently deployable
and prevents the full core catalog from increasing every boot image.
`make core-packages` currently produces independently licensed QuickNES,
Snes9x 2005, Snes9x 2002, ProSystem, Stella 2014, Gearboy, and PCE Fast
executables. Put them below
`/sf2000/cores` using
their existing `sf2000-*` names. Opening a `.nes` file presents a FCEUmm /
QuickNES chooser; `/NES` initially selects FCEUmm and `/QUICKNES` initially
selects QuickNES. SNES/SFC content presents a Snes9x 2005 / Snes9x 2002
chooser; Atari 7800 directories route to ProSystem. Each archive is rebuilt from its pinned upstream
revision using MIPS o32 PIC objects; HCRTOS archives are never relinked into
Linux static PIE executables.

While a core is running, START+SELECT opens the pause menu. It exposes Resume,
Exit, a 1x/2x/3x/4x/unlimited fast-forward rate, frontend frameskip, and the
options registered by the active libretro core. Unlimited mode still presents
every video callback through GE, but suppresses audio and pacing so the
measured FPS exposes the core, host, cache, and graphics-presentation ceiling.
Mode transitions and 300-frame windows are retained in `loglinux.txt`;
`mode=uncapped`,
`fps_milli`, `suppressed`, `sampled_max_run_us`, and
`sampled_present_us` identify a valid benchmark interval and separate core
cost from display-presentation cost.

The core host implements the libretro lifecycle, ROM full-path loading,
absolute frame pacing, SF2000 evdev joypad mapping, GE-accelerated RGB565
conversion/scaling, and 32 kHz mono ALSA DMA playback. Presentation uses two
ownership-tracked GE source surfaces, allowing emulation to overlap the
previous frame's scale operation without ever modifying an in-flight surface.
Audio uses a 4096-sample circular staging queue and the platform's portable
fixed-point linear stereo-to-mono resampler; ALSA underrun recovery retains
current audio instead of replaying stale blocks. The resampler uses ALSA delay
feedback to rebuild drained lead with a bounded 0.8% correction, then returns
to the exact nominal rate. Whole-period batching halves PCM write calls, while
ALSA is primed with 224 ms of audio lead. Low-rate
metrics include interval underruns, late frames, maximum lateness, sampled
core-frame and GE-presentation runtime, evdev event-to-host latency, PCM delay,
and active resampling rate; one write appends each record to
tmpfs, and the platform logger imports it only after gameplay. The real-time
thread therefore never sends periodic telemetry through printk, UART, or FAT.
One compact cumulative health word is also written to the platform's retained
RAM ring, allowing post-reset diagnosis without placing logging in a syscall
or storage path.
Absolute frame pacing keeps its timeline across a sub-frame miss, allowing the
next inexpensive frame to recover the delay without a discontinuity. It
rebases only after a complete frame interval is lost; this prevents expensive
first-frame setup from overflowing the audio path while avoiding repeated
clock resets for harmless 2--5 ms scheduling jitter.

The browser establishes a synchronized, RAM-journaled performance session
immediately before `exec` of a core or media player. The platform supervisor
ends and drains it after every clean exit or crash before relaunching the home
menu. This keeps diagnostics intact without allowing logger FAT traffic to
contend with ROM reads or real-time audio on the single SD/MMC channel, while
avoiding an indefinitely deferred journal while the idle browser is open.
START+RIGHT forces the platform logger to commit a durable checkpoint; the
daemon recognizes it independently of the frontend so it also works during an
early core hang. START+UP writes the UI source/GE/CPU diagnostic artifact.
The integrated image covers
browsing, ROM loading, emulation, video, audio, input, saves, and clean process
handoff.

Build checks:

```
make check
```

Cross-build a statically linked core:

```
make sf2000 CORE=/path/to/core_libretro.a
```

The resulting `build/sf2000-frontend` is a MIPS32r1 soft-float static PIE ELF.
Invoke it as `sf2000-frontend /mnt/sd/roms/game.ext`.

`make integrated` fetches pinned Gambatte and libretro-common revisions and
builds both `build/sf2000-browser` and `build/sf2000-gambatte`. The Linux image
embeds both, so no frontend executable belongs on the SD card. Put a ROM at,
for example, `/GB/game.gb` or `/GBC/game.gbc` on its FAT partition.

`make frogui` remains a developer compatibility target for the symbol-prefixed FrogUI core produced by the read-only
`mufrog-commandc` tree through a small Linux adapter. It provides the actual
themeable ROM browser rather than the synthetic pattern. The SF2000 Linux
image. It is not the integrated menu because the imported core does not expose
a complete Linux game-launch contract. The old diagnostic input shortcuts are
not part of the normal UI.

## Boundaries and next checkpoints

- Kernel: framebuffer/GE, evdev, ALSA PCM, MMC, clocks and power.
- QEMU: faithful registers, timing, DMA and regression gates.
- SF2000 platform: small stable userspace interfaces over those drivers.
- This application: menus, game metadata, save policy and libretro callbacks.

The application consumes the platform's source-compatible GE and audio
modules; register programming, cache ownership, DMA and interrupt policy stay
outside the cores. This keeps later emulator-specific optimization optional
and lets Linux and RTOS frontends share the same conversion and acceleration
contracts.

The presenter uses two source surfaces and fences before reuse.  This matches
the HC15xx engine's running-plus-pending queue contract.  A third outstanding
surface was tested on physical hardware and caused gpSP to continue executing
behind a blank scanout, without improving frame rate.  First-frame diagnostics
include hashes of both the core surface and the GE-written framebuffer so a
core failure and a presentation failure are distinguishable after reset.

For RGB565 cores whose framebuffer is in the NOMMU KSEG0 direct map, GE
copies the callback surface into the next managed source and the frontend
fences that short operation before returning from the callback.  Scaling from
the managed snapshot remains asynchronous.  This respects libretro's callback
lifetime while removing CPU staging copies. The same ownership-safe staging is
used in normal and uncapped modes; physical log140 shows that this is faster
than CPU-buffered delivery even without a pacing interval. Metrics report
`ge_stage_frames` and `buffered_frames`, making both contracts testable.

The NOMMU C++ runtime maps each allocation independently and returns it to the
kernel on `free`. A fixed bump arena is unsafe for switching cores in one
NOMMU process because it cannot reclaim a departed core's allocations.
`make check` includes repeated allocation/free/reallocation cycles to prevent
that lifecycle leak from returning.

## gpSP portability boundary

SF2000 cache sizing and compiler policy live in this Makefile, not in gpSP.
The host selects a 512 KiB ROM JIT cache, a 128 KiB RAM JIT cache, and a
mapped 9 KiB translator workspace. Only `cpu_threaded.c` is compiled with the
conservative switch lowering required by its generated-code control flow; the
rest of the core retains the normal optimized build.

The pinned gpSP patch contains only behavior that cannot be supplied by a
libretro frontend or Linux driver: MIPS32r1 assembly selection, a Thumb
high-register emitter correctness fix, safe handling of failed dynarec
lookups, executable-cache allocation validation, a relocatable translator
workspace, and safe memory-handler specialization. The latter publishes
modified JIT code through Linux's cache API only at a frame boundary, when no
generated code is live. Until that boundary the generic handler remains
valid. ROM hacks also skip inherited retail-game idle-loop addresses, which
may refer to unrelated instructions after a hack moves or replaces code.

An intrusive QEMU-only profile of Pokemon Unbound found 94--97% of gpSP core
time in generated guest code. GBA scanline rendering used about 1--2%, audio
less than 1%, and the frontend presentation path about 1.5 ms per frame.
Safe handler specialization improved the two measured 300-frame intervals by
roughly 1.0% and 1.8%, without frame skipping or reduced quality. The detailed
profiler is not part of release builds; the frontend's low-overhead tmpfs
metrics remain available for physical and QEMU runs.

Cache sizes and each optional dynarec policy are compile-time defines, so
another NOMMU MIPS host can reuse the fixes without inheriting SF2000
constants.

UTF conversion is excluded because this minimal uClibc profile deliberately
has no wide-character API and gpSP does not call that libretro-common unit.
Applications that need Unicode should use a wchar-enabled system profile;
there is no SF2000-specific replacement hidden in the core.
