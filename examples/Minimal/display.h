#pragma once

#include <Arduino_GFX_Library.h>
#include <string.h>
#include <IDoomDisplay.h>
#include "hw_conf.h"

// Usa la tecnica de framebuffer intermedio (igual que
// examples/CanvasDisplay/): mas tolerante a variaciones de panel que
// empujar fila por fila, que es justo lo que este ejemplo minimo
// necesita para ser lo mas facil posible de hacer arrancar. Si
// quieres ahorrar esos 150 KB de RAM del framebuffer, mira
// examples/DirectDisplay/.
//
// AJUSTA AQUI la clase si tu panel no es ILI9341: Arduino_GFX trae
// Arduino_ST7789, Arduino_ILI9488, etc. con constructores muy
// parecidos -- revisa la firma exacta en la libreria instalada.
class Display : public IDoomDisplay
{
public:

    bool begin() override
    {
        if (DOOM_PIN_LCD_BLK >= 0)
        {
            pinMode(DOOM_PIN_LCD_BLK, OUTPUT);
            digitalWrite(DOOM_PIN_LCD_BLK, HIGH);
        }

        _bus = new Arduino_ESP32SPI(
            DOOM_PIN_LCD_DC,
            DOOM_PIN_LCD_CS,
            DOOM_PIN_LCD_SCK,
            DOOM_PIN_LCD_MOSI,
            -1 /* MISO, no hace falta */);

        // AJUSTA AQUI: Arduino_ILI9341 -> tu clase de panel.
        _gfx = new Arduino_ST7789(_bus, DOOM_PIN_LCD_RST, 1 /* rotacion landscape */);
        
        _canvas = new Arduino_Canvas(DOOM_PANEL_W, DOOM_PANEL_H, _gfx);
        
        if (!_canvas->begin(8000000))
            return false;
        _gfx->invertDisplay(true);
        _canvas->fillScreen(0x0000);
        _canvas->flush();

        return true;
    }

    void beginFrame() override
    {
        // No-op: acumulamos en el canvas, el flush real va en endFrame.
    }

    void writeRow(int y, const uint16_t* rgb565, int count) override
    {
        uint16_t* fb = _canvas->getFramebuffer();
        fb += (DOOM_VIEWPORT_Y + y) * DOOM_PANEL_W + DOOM_VIEWPORT_X;
        memcpy(fb, rgb565, count * sizeof(uint16_t));
    }

    void endFrame() override
    {
        _canvas->flush();
    }

    Arduino_Canvas* getCanvas()
    {
       return _canvas;
    }

    int width()  const override { return DOOM_PANEL_W; }
    int height() const override { return DOOM_PANEL_H; }

private:

    Arduino_DataBus* _bus = nullptr;
    Arduino_GFX* _gfx = nullptr;
    Arduino_Canvas* _canvas = nullptr;
};