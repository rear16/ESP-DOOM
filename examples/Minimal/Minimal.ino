// The smallest possible ESP32-DOOM example: display, buttons, WAD in
// flash. No audio, no SD, nothing else to decide.
//
// If you need more -- comparing display techniques (with/without a
// framebuffer), loading WADs from SD, having sound -- see
// examples/CanvasDisplay/ and examples/DirectDisplay/, which start
// from this same skeleton and add one thing at a time.

#include <FS.h>
#include <LittleFS.h>

#include "hw_conf.h"
#include "Display.h"

#include <DoomGlue.h>
#include <SimpleButtons.h>

extern "C"
{
#include <doomgeneric_fs.h>
}

static Display display;

static SimpleButtons input(
    DOOM_PIN_BTN_UP, DOOM_PIN_BTN_DOWN, DOOM_PIN_BTN_LEFT, DOOM_PIN_BTN_RIGHT,
    DOOM_PIN_BTN_A, DOOM_PIN_BTN_B, DOOM_PIN_BTN_START, DOOM_PIN_BTN_SELECT);

void setup()
{
    Serial.begin(115200);

    if (!display.begin())
    {
        Serial.println("Could not start the display. Check hw_conf.h");
        while (true) delay(1000);
    }

    input.begin();

    if (!LittleFS.begin(true))
    {
        Serial.println("Could not mount LittleFS. Did you upload data/doom1.wad?");
        while (true) delay(1000);
    }

    DG_FS = &LittleFS;
    DG_Display = &display;
    DG_Input = &input;

    display.getCanvas()->fillScreen(0x0000);
    display.getCanvas()->flush();

    // Nothing audio-related to configure: DG_PCM stays nullptr by
    // default (it's defined in DoomSound.cpp, part of the library) and
    // the game runs silent. Not even an #include is needed for that.

    DoomGlue_Begin(DOOM_WAD_PATH);
}

void loop()
{
    DoomGlue_Tick();
}
