// Ejemplo minimo de ESP32-DOOM: pantalla CON framebuffer intermedio
// (Arduino_Canvas), botones sueltos en GPIO, sin multiplexor, sin audio
// por default. Este .ino es deliberadamente corto: si necesitas mas
// (EQ para tu bocina, musica sintetizada, un shutdown/restart probado a
// fondo), mira el proyecto Ultravice, que usa esta misma libreria con
// una capa de glue mas elaborada.

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

    // El CS de la SD se fuerza a alto ANTES de tocar la pantalla. Sin
    // esto, mientras display.begin() ya manda trafico SPI, el CS de la
    // SD puede quedar flotando y confundir ese trafico con el suyo --
    // el glitch clasico de compartir bus sin deseleccionar a tiempo.
    pinMode(DOOM_PIN_SD_CS, OUTPUT);
    digitalWrite(DOOM_PIN_SD_CS, HIGH);

    if (!display.begin())
    {
        Serial.println("No pude iniciar la pantalla. Revisa hw_conf.h");
        while (true) delay(1000);
    }

    input.begin();

    if (!SD.begin(DOOM_PIN_SD_CS))
    {
        Serial.println("No pude montar la SD.");
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
