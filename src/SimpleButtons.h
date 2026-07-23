#pragma once

#include <Arduino.h>
#include <IDoomInput.h>

// 8 botones sueltos en GPIO con INPUT_PULLUP (activo en bajo). Sin
// shift register, sin I2C, sin nada que soldar aparte de los propios
// botones. Los pines salen de hw_conf.h.
class SimpleButtons : public IDoomInput
{
public:

    SimpleButtons(
        uint8_t pinUp, uint8_t pinDown, uint8_t pinLeft, uint8_t pinRight,
        uint8_t pinA, uint8_t pinB, uint8_t pinStart, uint8_t pinSelect)
    :   _pins{pinUp, pinDown, pinLeft, pinRight, pinA, pinB, pinStart, pinSelect}
    {
    }

    void begin() override
    {
        for (uint8_t p : _pins)
            pinMode(p, INPUT_PULLUP);
    }

    void update() override
    {
        _previous = _current;

        for (int i = 0; i < 8; i++)
        {
            // INPUT_PULLUP: presionado = LOW.
            bool held = (digitalRead(_pins[i]) == LOW);

            if (held) _current |= (1 << i);
            else      _current &= ~(1 << i);
        }
    }

    bool down(DoomButton b) override
    {
        return (_current & maskFor(b)) != 0;
    }

    bool pressed(DoomButton b) override
    {
        return (_current & maskFor(b)) && !(_previous & maskFor(b));
    }

    bool released(DoomButton b) override
    {
        return !(_current & maskFor(b)) && (_previous & maskFor(b));
    }

private:

    // OJO: NUNCA llames a este helper 'bit'. Arduino.h define
    // '#define bit(b) (1UL << (b))' como macro de preprocesador, asi
    // que cualquier funcion con ese nombre se pisa con ella antes de
    // que el compilador vea C++ (mismo problema con 'byte'/'word').
    static int maskFor(DoomButton b) { return 1 << (int)b; }

    uint8_t _pins[8];
    uint8_t _current = 0;
    uint8_t _previous = 0;
};