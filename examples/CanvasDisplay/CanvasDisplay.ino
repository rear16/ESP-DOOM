// Minimal ESP-DOOM example: display WITH an intermediate framebuffer
// (Arduino_Canvas), plain GPIO buttons, no multiplexer, no audio by
// default. This .ino is deliberately short: if you need more (EQ for
// your speaker, synthesized music, a thoroughly-tested shutdown/
// restart cycle), see DOOM_MUSIC_SYNTH in hw_conf.h and src/DoomSound.h.

#include <SD.h>
#include <FS.h>

#include "hw_conf.h"
#include "Display.h"

#include <DoomGlue.h>
#include <SimpleButtons.h>

extern "C"
{
#include <doomgeneric_fs.h>
}

#ifndef DOOM_NO_AUDIO
#include <DoomSound.h>
#endif

static CanvasDisplay display;

static SimpleButtons input(
    DOOM_PIN_BTN_UP, DOOM_PIN_BTN_DOWN, DOOM_PIN_BTN_LEFT, DOOM_PIN_BTN_RIGHT,
    DOOM_PIN_BTN_A, DOOM_PIN_BTN_B, DOOM_PIN_BTN_START, DOOM_PIN_BTN_SELECT);

void setup()
{
    Serial.begin(115200);

    // The SD's CS is forced high BEFORE touching the display. Without
    // this, while display.begin() is already sending SPI traffic, the
    // SD's CS can float and mistake that traffic for its own -- the
    // classic glitch of sharing a bus without deselecting in time.
    pinMode(DOOM_PIN_SD_CS, OUTPUT);
    digitalWrite(DOOM_PIN_SD_CS, HIGH);

    if (!display.begin())
    {
        Serial.println("Could not start the display. Check hw_conf.h");
        while (true) delay(1000);
    }

    input.begin();

    if (!SD.begin(DOOM_PIN_SD_CS))
    {
        Serial.println("Could not mount the SD card.");
        while (true) delay(1000);
    }

    DG_FS = &SD;
    DG_Display = &display;
    DG_Input = &input;

    #ifndef DOOM_NO_AUDIO
    DoomSound_Init();
    #endif

    DoomGlue_Begin(DOOM_WAD_PATH);
}

void loop()
{
    DoomGlue_Tick();
}
