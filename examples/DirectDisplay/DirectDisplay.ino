// Minimal ESP-DOOM example: display WITHOUT an intermediate
// framebuffer (direct row-by-row push), plain GPIO buttons, no audio
// by default.
//
// Compare it with CanvasDisplay/: the only real difference between the
// two examples is Display.h's contents (canvas vs. direct push).
// Everything else -- buttons, lifecycle, audio -- is deliberately
// identical, so the comparison is fair.
//
// This is the first step toward low-RAM targets: it saves the panel's
// framebuffer (150 KB), not Doom's zone (6 MB), which is still the
// real limit. See the Roadmap in the library's README.

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

static DirectDisplay display;

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
