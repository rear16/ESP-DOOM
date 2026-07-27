#pragma once

// The smallest possible example: display, buttons, WAD in flash.
// Nothing else. No SD, no audio, nothing to solder besides a generic
// SPI panel and 8 buttons.
//
// This is the only file you should need to touch to port it to your
// wiring.

#define DOOM_PIN_LCD_CS    10
#define DOOM_PIN_LCD_DC    9
#define DOOM_PIN_LCD_RST   8
#define DOOM_PIN_LCD_SCK   12
#define DOOM_PIN_LCD_MOSI  11
#define DOOM_PIN_LCD_BLK   -1   // -1 if your panel has no backlight control

#define DOOM_PANEL_W       240
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
#define DOOM_VIEWPORT_W    240
#define DOOM_VIEWPORT_H    240
#define DOOM_VIEWPORT_X    0
#define DOOM_VIEWPORT_Y    0

// --- Input: 8 plain buttons, INPUT_PULLUP (active low) ---
#define DOOM_PIN_BTN_UP     4
#define DOOM_PIN_BTN_DOWN   7
#define DOOM_PIN_BTN_LEFT   5
#define DOOM_PIN_BTN_RIGHT  6
#define DOOM_PIN_BTN_A      2
#define DOOM_PIN_BTN_B      43
#define DOOM_PIN_BTN_START  1
#define DOOM_PIN_BTN_SELECT 18

// --- WAD: bundled in flash, no SD ---
// Put your doom1.wad in this example's data/ folder and upload it with
// "Sketch > Upload LittleFS Data" (Arduino 1.8.x) or the LittleFS
// plugin for the 2.x IDE (installed separately, not included by
// default).
//
// The shareware doom1.wad weighs 4.00 MB exactly. Check Tools >
// Partition Scheme: you need a scheme that leaves LittleFS more than
// those 4 MB (easy on N8/N16; on a 4 MB total N4, not even the WAD by
// itself fits alongside the firmware). See partitions.csv and the
// README's "Flash size and partitions" section.
#define DOOM_WAD_PATH      "/doom1.wad"
