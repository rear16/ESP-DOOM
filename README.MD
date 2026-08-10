# ESP-DOOM

A port of the Doom engine (via [doomgeneric](https://github.com/ozkl/doomgeneric))
to ESP32/Arduino, with its own audio mixer (a plain SFX-only mixer by
default, or an opt-in chiptune-style synthesizer for MUS music with EQ and
limiter tunable at runtime) and a real shutdown/restart path
(`doomgeneric_Shutdown`) so it can live alongside other apps in a larger
system.

## What it does

- Runs the full Doom engine (renderer, game logic, saving) on an ESP32-S3
  board with PSRAM.
- Ships its own audio path. By default (`src/DoomSound.cpp`): mixes up to
  16 SFX channels, music silent, no EQ, no limiter -- deliberately simple.
  Define `DOOM_MUSIC_SYNTH` in your `hw_conf.h` to switch on
  `src/mus_synth.cpp` instead: it synthesizes MUS music with pulse/noise
  oscillators (no OPL emulation), and applies a 4th-order high-pass filter
  plus a limiter tuned for small speakers. See `src/DoomSound.h` and
  `src/mus_synth.h` for what each one does.
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

### Flash size and partitions (examples/Minimal/)

`examples/Minimal/` bundles `doom1.wad` (4.00 MB exactly) into flash via
LittleFS, which means the partition table has to leave it enough room next
to the firmware. Real ESP32-S3 flash sizes only come in three tiers: 4, 8,
and 16 MB (Espressif's "N4"/"N8"/"N16" modules; there's no 6 MB or other
in-between SKU).

**4 MB (N4) genuinely can't do this**, not "it's tight": firmware
(~1.2 MB) plus the WAD (4.0 MB) is already 5.4 MB, more than the entire
chip. No partition layout fixes that. If your board is an N4, use
`CanvasDisplay` or `DirectDisplay` instead — they load the WAD from an SD
card, so nothing needs to fit in flash.

That leaves two real options, and this repo ships both:

- `examples/Minimal/partitions.csv` — **the active, default one.** Sized
  for **8 MB (N8)** boards, the smallest tier where this actually fits:
  2 MB for the app, 5.94 MB for LittleFS (1.94 MB of headroom over the
  4 MB WAD).
- `examples/Minimal/partitions_16MB.csv.alternative` — the same idea sized
  for **16 MB (N16)** boards: 3 MB for the app, 12.94 MB for LittleFS.
  **Not active as-is** — Arduino only auto-applies a sketch-local partition
  table if it's literally named `partitions.csv`. If you have a 16 MB
  board, rename this file to `partitions.csv` (replacing the 8 MB one) and
  set **Tools > Flash Size** to `16MB` in the IDE.

### Display scaling

Doom always renders at a fixed 320x200 (`DOOMGENERIC_RESX`/`RESY`). Real
panels come in every shape and orientation: 240x240, 800x800, 360x360,
80x160, whatever you're wiring up. `DoomGlue.cpp` handles fitting Doom's
image onto any of them, so you keep declaring the panel's *physical* size
(`DOOM_PANEL_W`/`H`) independently of how Doom's image gets scaled onto it
(`DOOM_VIEWPORT_W`/`H`).

There are three modes, picked by what you set `DOOM_VIEWPORT_W`/`H` to:

| `DOOM_VIEWPORT_W` | `DOOM_VIEWPORT_H` | Mode | What happens |
|---|---|---|---|
| not set / `0` | not set / `0` | **Native** | No rescale. Content is 320x200. |
| `W` | `0` | **Fill** | Content is `W` wide; height follows automatically (`H = W * 200/320`). Aspect ratio preserved. |
| `W` | `H` (both nonzero) | **Custom** | Independent per-axis stretch/squash to exactly `W` x `H`. No aspect ratio preservation. |

**Native** is what `CanvasDisplay` and `DirectDisplay` use: don't define
`DOOM_VIEWPORT_W`/`H` at all, and the panel just needs to be at least
320x200.

**Fill** takes a single number — always read as a *width* — and derives
the height for you, preserving Doom's 320:200 aspect ratio. This is the
one to reach for on an odd-shaped or rotated panel: whichever of the
panel's physical dimensions you want Doom's horizontal axis to become,
that's the number you pass. For example, on an 80x160 panel:

- `DOOM_VIEWPORT_W = 160` (treating the panel's long side as Doom's
  width): content becomes 160x100. Note this doesn't automatically
  "fit" the panel -- 160 is still wider than the panel's 80, so that
  axis ends up cropped too (see below), not placed.
- `DOOM_VIEWPORT_W = 80` (the panel's short side): content becomes
  80x50, which fits both axes and just gets placed (with vertical
  space left over, since 50 < 160).

**Custom** is what `examples/Minimal/hw_conf.h` uses: `DOOM_VIEWPORT_W =
DOOM_VIEWPORT_H = 240` stretches 320x200 to fill a 240x240 panel exactly,
accepting the distortion.

#### Placement vs. crop

Once content size is known, `DOOM_VIEWPORT_X`/`Y` (default `0,0`) means
one of two different things **per axis**, decided automatically by
comparing content size to `DOOM_PANEL_W`/`H`:

- **Content fits the panel on that axis** — X/Y *place* it (top-left
  corner inside the panel; the original meaning).
- **Content overflows the panel on that axis** — X/Y instead pick a
  **fixed crop origin**: which part of the content is visible, panned a
  fixed amount and declared once here. This is not adjustable at
  runtime; if you need that, it's a bigger change than this library
  makes today.

Worked example: an 80x160 panel showing Doom at native size (320x200, no
rescale) overflows on **both** axes (320 > 80, and 200 > 160), so both
`DOOM_VIEWPORT_X` and `DOOM_VIEWPORT_Y` become crop origins — X picks
which 80-pixel-wide slice of Doom's 320 columns you see, Y which
160-row-tall slice of its 200 rows.

Getting any of this wrong used to be a silent out-of-bounds write.
`DoomGlue.cpp` now `static_assert`s the placement-or-crop bounds on each
axis, so a mismatch fails at compile time instead of corrupting memory at
runtime.

The rescale itself is nearest-neighbor, computed once into a lookup table
(not per frame) and applied per pixel when converting Doom's palette to
RGB565. It's cheap, and it's deliberately not a quality upscaler — the
goal is fitting a panel, not making Doom look better than it did in 1993.

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