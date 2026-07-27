#pragma once

// The only file you should need to touch to port this example to your
// own wiring.
//
// Same panel type as CanvasDisplay (4-wire SPI, 320x240), but here
// Arduino_Canvas is NOT used: every row gets pushed straight to the
// panel. This is the first step toward small RAM -- you save the
// 320*240*2 = 150 KB of the intermediate framebuffer -- although
// Doom's 6 MB zone is still the real limit for running this on a chip
// without PSRAM (see the Roadmap in the library's README).

#define DOOM_PIN_LCD_CS    10
#define DOOM_PIN_LCD_DC    9
#define DOOM_PIN_LCD_RST   8
#define DOOM_PIN_LCD_SCK   12
#define DOOM_PIN_LCD_MOSI  11
#define DOOM_PIN_LCD_BLK   -1

#define DOOM_PANEL_W       320
#define DOOM_PANEL_H       240

// This example doesn't rescale: with DOOM_VIEWPORT_W/H left undefined,
// content is Doom's native 320x200 and X/Y just place it. If your
// panel is a different size, hw_conf.h can also declare
// DOOM_VIEWPORT_W/H to rescale (three modes: native / width-driven
// fill / independent-axis custom stretch) -- see
// examples/Minimal/hw_conf.h for a worked example, and DoomGlue.cpp's
// "Rescale / fill / crop" section for the full explanation. When
// content ends up bigger than the panel on some axis, X/Y switch
// meaning on that axis: instead of placing the content, they pick a
// fixed crop origin into it.
#define DOOM_VIEWPORT_X    0
#define DOOM_VIEWPORT_Y    20

// --- Input: 8 plain buttons, INPUT_PULLUP (active low) ---
#define DOOM_PIN_BTN_UP     4
#define DOOM_PIN_BTN_DOWN   5
#define DOOM_PIN_BTN_LEFT   6
#define DOOM_PIN_BTN_RIGHT  7
#define DOOM_PIN_BTN_A      15
#define DOOM_PIN_BTN_B      16
#define DOOM_PIN_BTN_START  17
#define DOOM_PIN_BTN_SELECT 18

// --- Audio ---
#define DOOM_NO_AUDIO

// #define DOOM_PIN_I2S_BCLK   43
// #define DOOM_PIN_I2S_LRC    2
// #define DOOM_PIN_I2S_DOUT   44
//
// With audio enabled, you can also opt into real synthesized music
// (instead of silence) by adding:
// #define DOOM_MUSIC_SYNTH
// See src/DoomSound.h for what that trades off.

// --- SD card (SPI mode, via SD.h -- not SD_MMC) ---
// Shares the display's SPI bus; only the CS differs. The .ino forces
// this pin high BEFORE starting the display: without that, the SD's
// CS can float while the display is already sending SPI traffic and
// mistake that traffic for its own (the classic glitch of sharing a
// bus without deselecting in time).
#define DOOM_PIN_SD_CS     13

// --- WAD ---
// You can swap WADs without reflashing: just replace the file on the
// SD card. If you'd rather not depend on an SD card at all, see
// examples/Minimal/, which bundles the WAD in flash.
#define DOOM_WAD_PATH      "/doom1.wad"
