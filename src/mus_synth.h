// Sintetizador de musica para ESP32-S3.
//
//  Lee los lumps MUS del WAD directo (el MUS es mas simple que el MIDI,
//  asi que mus2mid.c no hace falta por este camino) y los sintetiza con
//  osciladores de pulso + ruido.
//
//  Por que chiptune y no emulacion OPL: la bocina arranca en 1 kHz y el
//  bajo de Doom vive en 82-165 Hz. Una onda cuadrada a 165 Hz pone
//  armonicos en 495/825/1155/1485 Hz, y los que pasan el filtro bastan
//  para que el oido reconstruya el fundamental ausente. Los timbres FM
//  finos de un OPL se perderian en el mismo filtro.

#pragma once

#include <stdint.h>
#include <stddef.h>

bool MusSynth_Init(uint32_t sampleRate);

void MusSynth_Shutdown();

// data/len = lump MUS crudo, tal como lo entrega S_ChangeMusic.
bool MusSynth_Register(const void* data, int len);

void MusSynth_Play(bool looping);

void MusSynth_Stop();

void MusSynth_Pause();

void MusSynth_Resume();

bool MusSynth_IsPlaying();

// volume 0-127, tal como lo manda I_SetMusicVolume.
void MusSynth_SetVolume(int volume);

// Suma en acc (stereo intercalado, int32). Se llama desde la task del
// mixer, antes del EQ: la musica sale por la misma bocina.
void MusSynth_Render(int32_t* acc, size_t frames);

void MusSynth_Stats(int* peakVoices, uint32_t* steals);