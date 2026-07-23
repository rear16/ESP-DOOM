#pragma once

// Contrato minimo de entrada para los ejemplos. Deliberadamente identico
// en forma al IInput del proyecto Ultravice (Button + begin/update/
// pressed/released/down), pero sin depender de un multiplexor: la
// implementacion de ejemplo (SimpleButtons) lee GPIO directos con
// INPUT_PULLUP. Si tu hardware usa un HC165 u otra cosa, cambias solo
// SimpleButtons.*, nunca DoomGlue.

enum class DoomButton
{
    Up, Down, Left, Right,
    A, B, Start, Select
};

class IDoomInput
{
public:
    virtual void begin() = 0;

    // Llamalo una vez por poll: calcula flancos contra la lectura
    // anterior. DoomGlue ya lo hace por ti, no hace falta llamarlo
    // aparte.
    virtual void update() = 0;

    virtual bool down(DoomButton button) = 0;
    virtual bool pressed(DoomButton button) = 0;
    virtual bool released(DoomButton button) = 0;

    virtual ~IDoomInput() {}
};
