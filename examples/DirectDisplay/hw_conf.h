#pragma once

// Unico archivo que deberias tocar para portar este ejemplo a tu propio
// cableado.
//
// Mismo tipo de panel que CanvasDisplay (SPI 4 hilos, 320x240), pero
// aqui NO se usa Arduino_Canvas: cada fila se empuja directo al panel.
// Es el primer paso hacia RAM chica -- te ahorras los 320*240*2 = 150 KB
// del framebuffer intermedio -- aunque el zone de 6 MB de Doom sigue
// siendo el limite real para correr esto en un chip sin PSRAM (ver
// Roadmap en el README de la libreria).

#define DOOM_PIN_LCD_CS    10
#define DOOM_PIN_LCD_DC    9
#define DOOM_PIN_LCD_RST   8
#define DOOM_PIN_LCD_SCK   12
#define DOOM_PIN_LCD_MOSI  11
#define DOOM_PIN_LCD_BLK   -1

#define DOOM_PANEL_W       320
#define DOOM_PANEL_H       240

#define DOOM_VIEWPORT_X    0
#define DOOM_VIEWPORT_Y    20

// --- Entrada: 8 botones sueltos, INPUT_PULLUP (activo en bajo) ---
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

// --- Tarjeta SD (modo SPI, via SD.h -- no SD_MMC) ---
// Comparte el bus SPI de la pantalla; solo el CS es distinto. El .ino
// fuerza este pin a alto ANTES de arrancar la pantalla: sin eso, el CS
// de la SD puede quedar flotando mientras la pantalla ya manda trafico
// SPI y confundir ese trafico con el suyo (el glitch clasico de
// compartir bus sin deseleccionar a tiempo).
#define DOOM_PIN_SD_CS     13

// --- WAD ---
// Puedes cambiar de WAD sin reflashear: solo reemplaza el archivo en
// la SD. Si prefieres no depender de una SD para nada, mira
// examples/Minimal/, que trae el WAD embebido en la flash.
#define DOOM_WAD_PATH      "/doom1.wad"