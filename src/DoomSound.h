#pragma once

#include <IDoomPCMOutput.h>

// DG_sound_module y DG_music_module son obligatorios: la libreria trae
// FEATURE_SOUND encendido, asi que si no los defines en ALGUN .cpp del
// proyecto, el link falla.
//
// Esta es la version "de tarde": 8 canales de SFX mezclados sin EQ ni
// limitador, musica en silencio (nadie emula OPL aqui). Si tu proyecto
// necesita mas -- EQ para una bocina chica, limitador, sintetizador de
// la musica MUS -- el ESP-DOOM del proyecto Ultravice trae esa version
// completa; este archivo es a proposito mas simple.
//
// DG_PCM puede ser nullptr (DOOM_NO_AUDIO en hw_conf.h): en ese caso
// DoomSound_Init no crea la task y todo lo demas es no-op.
extern IDoomPCMOutput* DG_PCM;

void DoomSound_Init();
