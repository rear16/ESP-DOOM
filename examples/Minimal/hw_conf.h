#pragma once

// El ejemplo mas chico posible: pantalla, botones, WAD en flash. Nada
// mas. Ni SD, ni audio, ni nada que soldar aparte de un panel SPI
// generico y 8 botones.
//
// Este es el unico archivo que deberias tocar para portarlo a tu
// cableado.

#define DOOM_PIN_LCD_CS    10
#define DOOM_PIN_LCD_DC    9
#define DOOM_PIN_LCD_RST   8
#define DOOM_PIN_LCD_SCK   12
#define DOOM_PIN_LCD_MOSI  11
#define DOOM_PIN_LCD_BLK   -1   // -1 si tu panel no tiene control de brillo

#define DOOM_PANEL_W       240
#define DOOM_PANEL_H       240

// Offset donde cae el area de 320x200 de Doom dentro de tu panel.
#define DOOM_VIEWPORT_X    0
#define DOOM_VIEWPORT_Y    0

// --- Entrada: 8 botones sueltos, INPUT_PULLUP (activo en bajo) ---
#define DOOM_PIN_BTN_UP     4
#define DOOM_PIN_BTN_DOWN   7
#define DOOM_PIN_BTN_LEFT   5
#define DOOM_PIN_BTN_RIGHT  6
#define DOOM_PIN_BTN_A      2
#define DOOM_PIN_BTN_B      43
#define DOOM_PIN_BTN_START  1
#define DOOM_PIN_BTN_SELECT 18

// --- WAD: embebido en la flash, sin SD ---
// Pon tu doom1.wad en la carpeta data/ de este ejemplo y subelo con
// "Sketch > Upload LittleFS Data" (Arduino 1.8.x) o el plugin de
// LittleFS para el IDE 2.x (se instala aparte, no viene por defecto).
//
// El shareware doom1.wad pesa 4.00 MB exactos. Revisa Tools >
// Partition Scheme: necesitas un esquema que le deje a LittleFS mas de
// esos 4 MB (facil en N8/N16; en un N4 de 4 MB totales no entra ni el
// WAD solo junto con el firmware).
#define DOOM_WAD_PATH      "/doom1.wad"