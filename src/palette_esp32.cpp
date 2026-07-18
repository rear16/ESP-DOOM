#include "palette_esp32.h"
#include "Palette.h"

static Palette palette;

extern "C"
{

void DG_PaletteLoad(const void *playpal)
{
    palette.load((const uint8_t *)playpal);
}

uint16_t DG_PaletteColor565(uint8_t index)
{
    return palette.color565(index);
}

}