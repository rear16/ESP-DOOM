// El ejemplo mas chico posible de ESP32-DOOM: pantalla, botones, WAD
// en flash. Sin audio, sin SD, sin nada mas que decidir.
//
// Si necesitas mas -- comparar tecnicas de pantalla (con/sin
// framebuffer), cargar WADs por SD, tener sonido -- mira
// examples/CanvasDisplay/ y examples/DirectDisplay/, que parten de
// este mismo esqueleto y le agregan una cosa a la vez.
//#define USE_DMA
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
        Serial.println("No pude iniciar la pantalla. Revisa hw_conf.h");
        while (true) delay(1000);
    }

    input.begin();

    if (!LittleFS.begin(true))
    {
        Serial.println("No pude montar LittleFS. Subiste data/doom1.wad?");
        while (true) delay(1000);
    }

    DG_FS = &LittleFS;
    DG_Display = &display;
    DG_Input = &input;

    display.getCanvas()->fillScreen(0x0000);
    display.getCanvas()->flush();

    // Nada de audio que configurar: DG_PCM se queda en nullptr por
    // default (lo define DoomSound.cpp, parte de la libreria) y el
    // juego corre mudo. No hace falta ni un #include para lograrlo.

    DoomGlue_Begin(DOOM_WAD_PATH);
}

void loop()
{
    DoomGlue_Tick();
}
