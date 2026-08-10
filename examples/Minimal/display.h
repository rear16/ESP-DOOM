#pragma once

#include <Arduino_GFX_Library.h>
#include <string.h>
#include <IDoomDisplay.h>
#include "hw_conf.h"

// Uses the intermediate-framebuffer technique (same as
// examples/CanvasDisplay/): more tolerant of panel variations than
// pushing row by row, which is exactly what this minimal example needs
// to be as easy as possible to get running. If you want to save that
// 150 KB of framebuffer RAM, see examples/DirectDisplay/.
//
// ADJUST HERE the class if your panel isn't an ST7789: Arduino_GFX
// ships Arduino_ILI9341, Arduino_ILI9488, etc. with very similar
// constructors -- check the exact signature in your installed library.
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
            -1 /* MISO, not needed */);

        // ADJUST HERE: Arduino_ST7789 -> your panel class.
        _gfx = new Arduino_ST7735(_bus, DOOM_PIN_LCD_RST, 3, true, DOOM_PANEL_H, DOOM_PANEL_W, 26, 1, 26, 1);

        _canvas = new Arduino_Canvas(DOOM_PANEL_W, DOOM_PANEL_H, _gfx);

        if (!_canvas->begin(4000000))
            return false;
        _gfx->invertDisplay(false);
        _canvas->fillScreen(0x0000);
        _canvas->flush();

        return true;
    }

    void beginFrame() override
    {
        // No-op: we accumulate into the canvas, the real flush happens
        // in endFrame.
    }

    void writeRow(int x, int y, const uint16_t* rgb565, int count) override
    {
        uint16_t* fb = _canvas->getFramebuffer();
        fb += y * DOOM_PANEL_W + x;
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
