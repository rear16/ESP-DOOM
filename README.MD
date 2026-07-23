# ESP32-DOOM

A port of the Doom engine (via [doomgeneric](https://github.com/ozkl/doomgeneric))
to ESP32/Arduino, with its own audio mixer (SFX plus a chiptune-style
synthesizer for MUS music, EQ and limiter tunable at runtime) and a real
shutdown/restart path (`doomgeneric_Shutdown`) so it can live alongside other
apps in a larger system.

## What it does

- Runs the full Doom engine (renderer, game logic, saving) on an ESP32-S3
  board with PSRAM.
- Ships its own audio path: mixes 16 SFX channels, synthesizes MUS music with
  pulse/noise oscillators (no OPL emulation), and applies a 4th-order
  high-pass filter plus a limiter tuned for small speakers (see
  `docs/audio.md`).
- Exposes a full lifecycle: open, close (`doomgeneric_Shutdown`), and reopen
  Doom without rebooting the ESP32, returning all the PSRAM it used.
- Talks to your hardware through three small interfaces: display, input, and
  PCM audio output. The examples show concrete implementations; you are not
  required to use the same ones.

## What it does NOT do (yet)

- **Does not run on ESP32 without PSRAM** (the original "classic" chip, or a
  C3/S2 without external PSRAM). Doom's zone asks for 6 MB today; even an
  ESP32-S3 with only 2 MB of PSRAM (the "N8R2" modules) can't fit it without
  first shrinking that number. See Roadmap.
- No on-screen configuration menu, no multi-WAD support; it boots with
  whatever WAD you pass through argv.
- No FM music emulation (OPL). Music is synthesized with simple waveforms:
  it sounds "chiptune", not Sound Blaster.
- Doesn't bundle "factory" hardware drivers: the ones in the examples (a
  generic SPI panel, plain GPIO buttons, a basic I2S DAC) are exactly that —
  examples — so anyone can get running without the exact hardware of the
  original project.

## Minimum requirements today

- ESP32-S3 (or equivalent) with **at least ~6.5 MB of free PSRAM**.
- A display (any; see `IDoomDisplay` in the examples).
- A source of buttons (4 directions + 4 buttons; see `IDoomInput`).
- I2S audio output (optional: it can run silent).
- A `doom1.wad` (shareware) or another compatible IWAD on accessible storage
  (SD card, LittleFS, etc.)

## Examples

- `examples/Minimal/` — the smallest possible starting point: screen,
  buttons, WAD embedded in flash (LittleFS). No SD card, no audio, nothing
  else to decide. Start here.
- `examples/CanvasDisplay/` — same intermediate-framebuffer display
  technique as Minimal, but loads the WAD from an SD card instead, so you
  can swap WADs without reflashing.
- `examples/DirectDisplay/` — no intermediate framebuffer: pushes Doom
  straight to the display, one scanline at a time. Cheaper on RAM; it's the
  first step toward PSRAM-less targets. (See Roadmap: it still needs the
  6 MB zone today, so by itself it doesn't solve ESP32 classic.) Also loads
  the WAD from SD.

All three read pins and resolution from a `hw_conf.h` in their own folder —
to port to your hardware, that should be the only file you need to touch.

## Roadmap

1. **Shrink the zone.** 6 MB is the number doomgeneric ships by default, not
   a real minimum: vanilla Doom ran in 4 MB of DOS RAM *including* the
   executable. Near-term goal: get `doom1.wad` running stably with ~1.5-1.8 MB
   of zone, which is what it would take to fit inside a module with 2 MB of
   PSRAM (the "N8R2" boards). Not done yet; the exact number will come from
   testing how much the shareware WAD tolerates without reproducing the
   visplane crash we already fixed once by reducing it too far.
2. **"Classic" ESP32 (no on-chip PSRAM).** Ambitious, not promised. Even with
   a shrunk zone, the next wall is that classic ESP32 only has ~320 KB of
   internal SRAM total; it would need external PSRAM wired by hand (like the
   WROVER modules) to even attempt it. This is not the same project as
   point 1.
3. **A no-RAM variant for C3/S2 (idea, not a design).** Explore whether
   smaller emulators (NES) could live in the unused OTA partition and run
   from there, avoiding loading full state into RAM. This would apply first
   to Nofrendo, not Doom; it's mentioned here only as the north star of the
   same "how much can run with how little" pattern.

Nothing in the roadmap blocks using the library today on an ESP32-S3 with
generous PSRAM (4-8 MB free). Points 1-3 are about lowering that floor, not
about whether it works right now.