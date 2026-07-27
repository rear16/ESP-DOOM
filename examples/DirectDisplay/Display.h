#pragma once

#include <Arduino_GFX_Library.h>
#include <IDoomDisplay.h>
#include "hw_conf.h"

// WITHOUT an intermediate framebuffer: every row Doom finishes drawing
// gets pushed straight to the panel over SPI in one shot, via
// draw16bitRGBBitmap(x, y, buffer, w, 1). There's no Arduino_Canvas,
// so you save 320*240*2 = 150 KB of RAM -- the price is that there's
// nothing to "flush()": once row y has been shown, showing it was
// never optional.
//
// ADJUST HERE the class if your panel isn't an ILI9341.
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

        // ADJUST HERE: Arduino_ILI9341 -> your panel class.
        _gfx = new Arduino_ILI9341(_bus, DOOM_PIN_LCD_RST, 1);

        if (!_gfx->begin(40000000))
            return false;

        _gfx->fillScreen(0x0000);

        return true;
    }

    void beginFrame() override
    {
        // No-op: nothing to prepare, each row is sent on its own.
    }

    void writeRow(int x, int y, const uint16_t* rgb565, int count) override
    {
        _gfx->draw16bitRGBBitmap(x, y, (uint16_t*)rgb565, count, 1);
    }

    void endFrame() override
    {
        // No-op: everything was already shown, row by row, in writeRow.
    }

    int width()  const override { return DOOM_PANEL_W; }
    int height() const override { return DOOM_PANEL_H; }

private:

    Arduino_DataBus* _bus = nullptr;
    Arduino_GFX* _gfx = nullptr;
};
