# Frontend architecture

<!-- SPDX-License-Identifier: MIT -->

The frontend is a small static libretro host, not an operating-system service.
Its public boundaries are deliberately narrow:

- `src/browser.c` owns the home menu, content discovery, core selection,
  process launch, and the application performance-journal boundary. It does
  not link a core or touch GE/audio devices.
- `src/content.c` owns ROM loading and the NOMMU-safe content allocation
  contract.
- `src/sf2000_input.c` owns Linux evdev decoding, chord edge detection and the
  libretro joypad state. It reports actions to the host instead of changing
  emulation policy itself.
- `src/sf2000_pacer.c` owns the absolute frame timeline and late-frame
  recovery policy. It has no Linux device, libretro, audio, or GE dependency.
- `src/main.c` owns one libretro session and coordinates presentation, PCM
  delivery and lifecycle. The GE and resampler implementations remain reusable
  libraries in the sibling `sf2000_linux` repository.
- `src/nommu_new.cpp` supplies the bounded allocation policy required by
  C++ cores on uClinux.

Core code must not know framebuffer physical addresses, ALSA ioctls, retained
RAM, or SF2000 key codes. Platform modules must not call `retro_run()` or
select core options. This allows input, storage/browser, hardware libraries,
and individual cores to be developed and tested independently.

`SF2000_HOST_OBJECTS` in the Makefile is the single definition of the platform
host. Gambatte and gpSP link those same objects, so a platform change cannot
silently produce two different hosts and common sources compile only once.

The browser draws the loading card before it begins the synchronized
performance session and calls `execve()`. The core host ignores video
callbacks made from inside `retro_load_game()`, so a core cannot replace that
card with an incomplete or blank setup frame. The first callback after
`retro_load_game()` returns becomes the first owned game frame.

START+SELECT is decoded once in `sf2000_input.c` and withheld from the core.
`main.c` owns pause policy, registered libretro variables, fast-forward,
frameskip, audio drop/re-prime, and clean core teardown. The kernel input
driver never implements application chords or reboots.

## Frame ownership

Libretro owns a video callback pointer only until the callback returns. For a
KSEG0 RGB565 surface, GE copies it into a frontend-managed source and the host
fences that short copy before returning. Scaling from the managed snapshot is
asynchronous and overlaps the next core frame. CPU staging exists only for
non-addressable or converted input. Metrics distinguish these paths with
`ge_stage_frames` and `buffered_frames`.

A core requesting `GET_CURRENT_SOFTWARE_FRAMEBUFFER` writes directly into a
managed source. The request handler must fence before returning a source still
owned by queued GE work; fencing later in the video callback is already too
late because the core has overwritten it. Validate requested dimensions,
RGB565 format, write access, and allocation bounds before exposing a surface.

## Real-time rules

The emulation thread may append one telemetry record to tmpfs every 300 frames.
It must not write the SD card or synchronously print periodic metrics. The
supervisor imports the tmpfs journal after the core exits. Audio and video
callbacks allocate no memory after initialization.

Any new backend should provide an explicit state object, open/close ownership,
and host-side tests. Avoid backend globals and avoid exposing Linux or HC15xx
details through the libretro callback interface.

The pacer preserves its absolute timeline after a sub-frame miss and omits
that frame's sleep. A later inexpensive frame therefore catches up without a
clock discontinuity. It rebases only after a complete frame interval has
already been lost, when replaying stale emulation would overfill audio.
