#pragma once

#include <stdint.h>
#include <stddef.h>

// Contrato minimo de salida de audio para los ejemplos: PCM crudo, nada
// de reproductor MP3 encima (eso es IAudio en el proyecto Ultravice,
// pensado para su propio reproductor -- no lo necesitas para correr
// Doom).
//
// 'write' recibe samples MONO de 16 bits. Si tu DAC/I2S es estereo,
// duplica L=R en tu implementacion; es mas simple que forzar a todos a
// pensar en estereo para un juego que de por si suena mono.
//
// Si tu placa no tiene audio, define DOOM_NO_AUDIO en hw_conf.h: el
// glue deja DG_PCM en nullptr y el juego corre mudo, sin tocar este
// archivo.
class IDoomPCMOutput
{
public:
    virtual bool begin(uint32_t sampleRate) = 0;

    virtual void write(const int16_t *samples, size_t count) = 0;

    virtual void end() = 0;

    virtual ~IDoomPCMOutput() {}
};
