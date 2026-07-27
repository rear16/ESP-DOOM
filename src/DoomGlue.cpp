#include <DoomGlue.h>

#include <Arduino.h>

extern "C"
{
#include <doomgeneric.h>
#include <doomkeys.h>
#include <palette_esp32.h>
}

// hw_conf.h lives in the sketch, not the library, but the sketch folder
// is on the include path for every translation unit Arduino compiles --
// including this one (same trick DoomSound.cpp uses for DOOM_MUSIC_SYNTH).
#if __has_include("hw_conf.h")
#include "hw_conf.h"
#endif

IDoomDisplay* DG_Display = nullptr;
IDoomInput*   DG_Input   = nullptr;

// ------------------------------------------------------------------
//  Rescale / fill / crop
// ------------------------------------------------------------------
//
//  Doom always renders at a fixed 320x200 (DOOMGENERIC_RESX/RESY). Real
//  panels don't: 240x240, 800x800, 80x160, whatever you're wiring up.
//  hw_conf.h can declare DOOM_VIEWPORT_W/H to control how Doom's image
//  is scaled BEFORE it's placed on the panel, independently of
//  DOOM_PANEL_W/H (the panel's own physical size). Three modes, picked
//  by which of DOOM_VIEWPORT_W/H you set:
//
//    not set / 0,0     NATIVE  -- no rescale. Content is 320x200.
//    W, 0              FILL    -- content is W wide; height follows
//                                 Doom's fixed 320:200 ratio
//                                 automatically (H = W * 200/320).
//                                 DOOM_VIEWPORT_W is the one number
//                                 you choose; it's always read as a
//                                 width, whichever physical axis of
//                                 your panel that ends up meaning.
//    W, H (both != 0)  CUSTOM  -- independent per-axis stretch/squash
//                                 to exactly W x H. No aspect ratio
//                                 preservation. examples/Minimal/ uses
//                                 this to fill a 240x240 panel.
//
//  Once content size is known, DOOM_VIEWPORT_X/Y (default 0,0) means
//  one of two different things PER AXIS, decided automatically:
//
//    content fits the panel  -> X/Y PLACE it (top-left corner inside
//                                the panel; same meaning as before).
//    content overflows it    -> X/Y instead pick which part of the
//                                content is visible: a fixed crop/pan
//                                origin, declared once here, not
//                                adjustable at runtime.
//
//  Getting any of this wrong used to be a silent out-of-bounds write.
//  The static_asserts below catch a mismatch at compile time instead.

#ifndef DOOM_VIEWPORT_W
#define DOOM_VIEWPORT_W 0

#endif

#ifndef DOOM_VIEWPORT_H
#define DOOM_VIEWPORT_H 0
#endif

#ifndef DOOM_VIEWPORT_X
#define DOOM_VIEWPORT_X 0
#endif

#ifndef DOOM_VIEWPORT_Y
#define DOOM_VIEWPORT_Y 0
#endif

#if DOOM_VIEWPORT_W == 0 && DOOM_VIEWPORT_H == 0
// NATIVE
#define DOOM_CONTENT_W DOOMGENERIC_RESX
#define DOOM_CONTENT_H DOOMGENERIC_RESY
#elif DOOM_VIEWPORT_H == 0
// FILL: width-driven, height derived from Doom's fixed 320:200 ratio.
#define DOOM_CONTENT_W DOOM_VIEWPORT_W
#define DOOM_CONTENT_H \
    ((DOOM_VIEWPORT_W * DOOMGENERIC_RESY + DOOMGENERIC_RESX / 2) / DOOMGENERIC_RESX)
#else
// CUSTOM: independent stretch.
#define DOOM_CONTENT_W DOOM_VIEWPORT_W
#define DOOM_CONTENT_H DOOM_VIEWPORT_H
#endif

static constexpr int kContentW = DOOM_CONTENT_W;
static constexpr int kContentH = DOOM_CONTENT_H;

#ifdef DOOM_PANEL_W
static constexpr int kPanelW = DOOM_PANEL_W;
#else
static constexpr int kPanelW = kContentW;   // no panel size declared: assume exact fit
#endif

#ifdef DOOM_PANEL_H
static constexpr int kPanelH = DOOM_PANEL_H;
#else
static constexpr int kPanelH = kContentH;
#endif

// Per axis: does the content fit, or does it need cropping?
static constexpr bool kCropX = kContentW > kPanelW;
static constexpr bool kCropY = kContentH > kPanelH;

// How many pixels/rows actually get sent to the display per frame.
static constexpr int kOutW = kCropX ? kPanelW : kContentW;
static constexpr int kOutH = kCropY ? kPanelH : kContentH;

// Where on the panel drawing starts (0 when cropping: the crop already
// fills the panel edge to edge on that axis).
static constexpr int kPanelX0 = kCropX ? 0 : DOOM_VIEWPORT_X;
static constexpr int kPanelY0 = kCropY ? 0 : DOOM_VIEWPORT_Y;

// Where in the (possibly rescaled) content reading starts (0 when
// placing: the whole content is shown, nothing to crop into).
static constexpr int kCropX0 = kCropX ? DOOM_VIEWPORT_X : 0;
static constexpr int kCropY0 = kCropY ? DOOM_VIEWPORT_Y : 0;

static_assert(!kCropX ? (DOOM_VIEWPORT_X + kContentW <= kPanelW)
                       : (DOOM_VIEWPORT_X >= 0 && DOOM_VIEWPORT_X + kPanelW <= kContentW),
    "DOOM_VIEWPORT_X doesn't fit: content doesn't fit at that placement, "
    "or the crop window falls outside the (rescaled) content");

static_assert(!kCropY ? (DOOM_VIEWPORT_Y + kContentH <= kPanelH)
                       : (DOOM_VIEWPORT_Y >= 0 && DOOM_VIEWPORT_Y + kPanelH <= kContentH),
    "DOOM_VIEWPORT_Y doesn't fit: content doesn't fit at that placement, "
    "or the crop window falls outside the (rescaled) content");

static uint16_t xLUT[kContentW];
static uint16_t yLUT[kContentH];
static bool lutReady = false;

// Nearest-neighbor with rounding (not truncation): srcIdx = round(dstIdx
// * srcSize / dstSize), clamped. Cheap -- built once, not per frame --
// and good enough for a resize that's mostly there to fit a panel, not
// to be a quality upscaler. Maps CONTENT-space (post-rescale) back to
// Doom's native 320x200 source space.
static void BuildScaleLUT()
{
    for (int x = 0; x < kContentW; x++)
    {
        int sx = (x * DOOMGENERIC_RESX + kContentW / 2) / kContentW;
        if (sx >= DOOMGENERIC_RESX) sx = DOOMGENERIC_RESX - 1;
        xLUT[x] = (uint16_t)sx;
    }

    for (int y = 0; y < kContentH; y++)
    {
        int sy = (y * DOOMGENERIC_RESY + kContentH / 2) / kContentH;
        if (sy >= DOOMGENERIC_RESY) sy = DOOMGENERIC_RESY - 1;
        yLUT[y] = (uint16_t)sy;
    }

    lutReady = true;
}

// ------------------------------------------------------------------
//  Key queue
// ------------------------------------------------------------------
//
//  I_GetEvent() calls DG_GetKey() in a loop until it returns 0. We
//  can't call DG_Input->update() on every one of those calls: it
//  would recompute pressed()/released() against state that's already
//  been consumed. A single poll per tick (rate-limited), the rest
//  just drains the queue.

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
}

void DG_DrawFrame()
{
    if (!DG_Display) return;

    if (!lutReady)
        BuildScaleLUT();

    static uint16_t row[kOutW];

    DG_Display->beginFrame();

    const uint8_t* src = (const uint8_t*)DG_ScreenBuffer;

    for (int panelRow = 0; panelRow < kOutH; panelRow++)
    {
        int contentRow = panelRow + kCropY0;
        const uint8_t* srcRow = src + (size_t)yLUT[contentRow] * DOOMGENERIC_RESX;

        for (int panelCol = 0; panelCol < kOutW; panelCol++)
        {
            int contentCol = panelCol + kCropX0;
            row[panelCol] = DG_PaletteColor565(srcRow[xLUT[contentCol]]);
        }

        DG_Display->writeRow(kPanelX0, kPanelY0 + panelRow, row, kOutW);
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
//  Lifecycle
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