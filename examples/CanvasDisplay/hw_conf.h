#pragma once

// The only file you should need to touch to port this example to your
// own wiring.
//
// Target panel: generic 4-wire SPI, 320x240 (the cheap ILI9341/ST7789
// from any store). If yours is different, adjust the Arduino_GFX class
// in CanvasDisplay.ino (look for the "ADJUST HERE" comment) in
// addition to these pins.

#define DOOM_PIN_LCD_CS    10
#define DOOM_PIN_LCD_DC    9
#define DOOM_PIN_LCD_RST   8
#define DOOM_PIN_LCD_SCK   12
#define DOOM_PIN_LCD_MOSI  11
#define DOOM_PIN_LCD_BLK   -1   // -1 if your panel has no backlight control

#define DOOM_PANEL_W       320
#define DOOM_PANEL_H       240

// Doom always renders at a fixed 320x200; this panel is 240x240. With
// both DOOM_VIEWPORT_W and DOOM_VIEWPORT_H set, this is CUSTOM mode:
// independent per-axis stretch to exactly fill the panel (240x240
// isn't Doom's 320x200 aspect ratio, so this squashes vertically --
// that's the expected tradeoff for a screen this shape). See
// DoomGlue.cpp's "Rescale / fill / crop" section for the other two
// modes (native, and width-driven fill that preserves aspect ratio),
// and the README's "Display scaling" section for a plainer walkthrough.
//
// This used to be a real bug: without an explicit viewport, writeRow
// got called with 320 pixels per row against a 240-wide canvas, an
// out-of-bounds write into the next row's memory. Declaring the
// viewport is what fixes it, not a clamp somewhere downstream.
#define DOOM_VIEWPORT_W    320
#define DOOM_VIEWPORT_H    240
#define DOOM_VIEWPORT_X    0
#define DOOM_VIEWPORT_Y    0

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
// No I2S DAC: leave this defined and the game runs silent.
#define DOOM_NO_AUDIO

// If you have an I2S DAC (MAX98357, PCM5102, etc), comment out the
// line above and define your pins:
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
