#pragma once

#include <stdint.h>

// Contrato minimo entre el glue de doomgeneric y la pantalla.
//
// Se disenio linea por linea (no "dame el framebuffer entero") a
// proposito: es el unico contrato que sirve tanto para una pantalla con
// framebuffer intermedio (Arduino_Canvas: begin/endFrame acumulan, el
// flush real pasa en endFrame) como para una sin el (push directo por
// SPI linea por linea, sin doble buffer).
//
// Esa segunda forma es mas barata en RAM -- es el primer paso hacia
// targets sin PSRAM -- pero hoy sigue sin resolver el zone de 6 MB de
// Doom, asi que por si sola no corre en un ESP32 classic. Ver el
// Roadmap del README.
class IDoomDisplay
{
public:
    virtual bool begin() = 0;

    // Se llama una vez por frame de Doom, antes de la primera writeRow.
    // Implementacion con canvas: no-op. Sin canvas: abre la ventana de
    // direccion (setAddrWindow) del area 320x200.
    virtual void beginFrame() = 0;

    // 'rgb565' trae 'count' pixeles ya convertidos (paleta de Doom ->
    // RGB565 la hace el glue, no la pantalla). y va de 0 a height()-1.
    virtual void writeRow(int y, const uint16_t* rgb565, int count) = 0;

    // Con canvas: aqui pasa el flush() real. Sin canvas: no-op, ya se
    // escribio todo en writeRow.
    virtual void endFrame() = 0;

    virtual int width() const = 0;
    virtual int height() const = 0;

    virtual ~IDoomDisplay() {}
};
