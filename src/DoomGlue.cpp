#include <DoomGlue.h>

#include <Arduino.h>

extern "C"
{
#include <doomgeneric.h>
#include <doomkeys.h>
#include <palette_esp32.h>
}

IDoomDisplay* DG_Display = nullptr;
IDoomInput*   DG_Input   = nullptr;

// ------------------------------------------------------------------
//  Cola de teclado
// ------------------------------------------------------------------
//
//  I_GetEvent() llama a DG_GetKey() en bucle hasta que devuelve 0. No
//  podemos llamar a DG_Input->update() en cada una de esas llamadas:
//  recalcularia pressed()/released() contra el estado ya consumido.
//  Un solo poll por tick (con rate limit), el resto solo drena.

struct KeyEvent { unsigned char key; unsigned char pressed; };

static constexpr uint8_t QUEUE_SIZE = 16;
static KeyEvent queue[QUEUE_SIZE];
static uint8_t qHead = 0, qTail = 0;

static void QueuePush(unsigned char key, unsigned char pressed)
{
    uint8_t next = (qTail + 1) % QUEUE_SIZE;
    if (next == qHead) return;
    queue[qTail] = {key, pressed};
    qTail = next;
}

static bool QueuePop(KeyEvent& e)
{
    if (qHead == qTail) return false;
    e = queue[qHead];
    qHead = (qHead + 1) % QUEUE_SIZE;
    return true;
}

struct ButtonMap { DoomButton button; unsigned char key; };

static const ButtonMap KEYMAP[] =
{
    { DoomButton::Up,     KEY_UPARROW    },
    { DoomButton::Down,   KEY_DOWNARROW  },
    { DoomButton::Left,   KEY_LEFTARROW  },
    { DoomButton::Right,  KEY_RIGHTARROW },
    { DoomButton::A,      KEY_FIRE       },
    { DoomButton::B,      KEY_USE        },
    { DoomButton::Start,  KEY_ENTER      },
    { DoomButton::Select, KEY_ESCAPE     },
};

static constexpr uint32_t POLL_INTERVAL_MS = 5;
static uint32_t lastPoll = 0;

static void PollInput()
{
    DG_Input->update();

    for (auto& m : KEYMAP)
    {
        if (DG_Input->pressed(m.button))       QueuePush(m.key, 1);
        else if (DG_Input->released(m.button)) QueuePush(m.key, 0);
    }
}

// ------------------------------------------------------------------
//  doomgeneric.h
// ------------------------------------------------------------------

void DG_Init()
{
    Serial.println("DG_Init()");
}

void DG_DrawFrame()
{
    if (!DG_Display) return;

    static uint16_t row[DOOMGENERIC_RESX];

    DG_Display->beginFrame();

    const uint8_t* src = (const uint8_t*)DG_ScreenBuffer;

    for (int y = 0; y < DOOMGENERIC_RESY; y++)
    {
        for (int x = 0; x < DOOMGENERIC_RESX; x++)
            row[x] = DG_PaletteColor565(src[x]);

        DG_Display->writeRow(y, row, DOOMGENERIC_RESX);

        src += DOOMGENERIC_RESX;
    }

    DG_Display->endFrame();
}

void DG_SleepMs(uint32_t ms) { delay(ms); }

uint32_t DG_GetTicksMs() { return millis(); }

int DG_GetKey(int* pressed, unsigned char* key)
{
    KeyEvent e;

    if (!QueuePop(e))
    {
        if (!DG_Input) return 0;

        uint32_t now = millis();
        if (now - lastPoll < POLL_INTERVAL_MS) return 0;
        lastPoll = now;

        PollInput();

        if (!QueuePop(e)) return 0;
    }

    *pressed = e.pressed;
    *key = e.key;
    return 1;
}

void DG_SetWindowTitle(const char*) {}

// ------------------------------------------------------------------
//  Ciclo de vida
// ------------------------------------------------------------------

void DoomGlue_Begin(const char* wadPath)
{
    static char argWad[] = "-iwad";
    static char argPath[128];
    strncpy(argPath, wadPath, sizeof(argPath) - 1);

    static char* argv[] = { (char*)"doom", argWad, argPath };

    doomgeneric_Create(3, argv);
}

void DoomGlue_Tick()
{
    doomgeneric_Tick();
}

void DoomGlue_Shutdown()
{
    doomgeneric_Shutdown();
}
