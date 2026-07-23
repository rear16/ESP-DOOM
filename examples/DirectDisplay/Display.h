#pragma once

#include <Arduino_GFX_Library.h>
#include <IDoomDisplay.h>
#include "hw_conf.h"

// SIN framebuffer intermedio: cada fila que Doom termina de dibujar se
// empuja de una vez por SPI directo al panel, via
// draw16bitRGBBitmap(x, y, buffer, w, 1). No hay Arduino_Canvas, asi
// que te ahorras 320*240*2 = 150 KB de RAM -- el precio es que no hay
// nada que "flush()": si la fila x ya se mostro, mostrarla nunca fue
// opcional.
//
// AJUSTA AQUI la clase si tu panel no es ILI9341.
class DirectDisplay : public IDoomDisplay
{
public:

    bool begin() override
    {
        if (DOOM_PIN_LCD_BLK >= 0)
        {
            pinMode(DOOM_PIN_LCD_BLK, OUTPUT);
            digitalWrite(DOOM_PIN_LCD_BLK, HIGH);
        }

        _bus = new Arduino_HWSPI(
            DOOM_PIN_LCD_DC,
            DOOM_PIN_LCD_CS,
            DOOM_PIN_LCD_SCK,
            DOOM_PIN_LCD_MOSI,
            -1);

        // AJUSTA AQUI: Arduino_ILI9341 -> tu clase de panel.
        _gfx = new Arduino_ILI9341(_bus, DOOM_PIN_LCD_RST, 1);

        if (!_gfx->begin(40000000))
            return false;

        _gfx->fillScreen(0x0000);

        return true;
    }

    void beginFrame() override
    {
        // No-op: no hay nada que preparar, cada fila se manda sola.
    }

    void writeRow(int y, const uint16_t* rgb565, int count) override
    {
        _gfx->draw16bitRGBBitmap(
            DOOM_VIEWPORT_X,
            DOOM_VIEWPORT_Y + y,
            (uint16_t*)rgb565,
            count,
            1);
    }

    void endFrame() override
    {
        // No-op: ya se mostro todo, fila por fila, en writeRow.
    }

    int width()  const override { return DOOM_PANEL_W; }
    int height() const override { return DOOM_PANEL_H; }

private:

    Arduino_DataBus* _bus = nullptr;
    Arduino_GFX* _gfx = nullptr;
};
