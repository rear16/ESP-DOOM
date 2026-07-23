#include "Palette.h"

bool Palette::load(const uint8_t *rgb)
{
    if (!rgb)
        return false;

    for (int i = 0; i < 256; i++)
    {
        uint8_t r = rgb[i * 3 + 0];
        uint8_t g = rgb[i * 3 + 1];
        uint8_t b = rgb[i * 3 + 2];

        colors[i] =
            ((r >> 3) << 11) |
            ((g >> 2) << 5)  |
            (b >> 3);
    }

    return true;
}

uint16_t Palette::color565(uint8_t index) const
{
    return colors[index];
}