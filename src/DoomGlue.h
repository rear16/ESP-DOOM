#pragma once

#include <IDoomDisplay.h>
#include <IDoomInput.h>

// Punto de conexion entre doomgeneric.h y tu hardware. Ni CanvasDisplay
// ni DirectDisplay tocan este archivo: solo asignan estos punteros antes
// de doomgeneric_Create() y ya.
extern IDoomDisplay* DG_Display;
extern IDoomInput*   DG_Input;

// Rearma el ciclo enter/exit sin reiniciar el ESP32: llama
// doomgeneric_Create/doomgeneric_Tick/doomgeneric_Shutdown por ti.
// Ver el .ino de cada ejemplo para el patron completo de uso.
void DoomGlue_Begin(const char* wadPath);
void DoomGlue_Tick();
void DoomGlue_Shutdown();
