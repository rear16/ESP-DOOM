#include <DoomSound.h>

#include <Arduino.h>
#include <string.h>

// hw_conf.h lives in the sketch, not the library, but the sketch
// folder is on the include path for every translation unit Arduino
// compiles -- including this one. __has_include keeps this file
// working standalone (no hw_conf.h at all) as well as with any
// example's hw_conf.h that opts into the full music synth.
#if __has_include("hw_conf.h")
#include "hw_conf.h"
#endif

#ifdef DOOM_MUSIC_SYNTH
#include <mus_synth.h>
#endif

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

extern "C"
{
#include <doomtype.h>
#include <i_sound.h>
#include <m_misc.h>
#include <deh_str.h>
#include <w_wad.h>
#include <z_zone.h>
#include <sounds.h>
}

IDoomPCMOutput* DG_PCM = nullptr;

// i_sound.c (the generic engine) declares these two as extern and
// binds them in I_BindSoundVariables, but in original Doom they're
// defined by i_sdlsound.c, which doesn't exist here: without this it
// won't link. We don't resample with libsamplerate (the mixer uses a
// fixed 16.16 step), so they only need to exist with chocolate-doom's
// defaults.
extern "C"
{
    int use_libsamplerate = 0;
    float libsamplerate_scale = 0.65f;
}

static constexpr uint32_t MIX_RATE = 11025;   // native rate of DMX lumps
static constexpr size_t MIX_FRAMES = 256;
static constexpr int MAX_CHANNELS = 8;        // s_sound.c: snd_channels

struct SfxData { const uint8_t* samples; uint32_t length; uint32_t rate; };

struct SfxChannel
{
    const uint8_t* data = nullptr;
    uint32_t length = 0;
    uint32_t pos = 0;    // fixed 16.16
    uint32_t step = 0;   // fixed 16.16
    int32_t vol = 0;     // 0-127
    bool active = false;
};

static SfxChannel channels[MAX_CHANNELS];
static SemaphoreHandle_t mixLock = nullptr;
static TaskHandle_t mixTask = nullptr;
static volatile bool mixRun = false;
static doom_boolean sfxPrefix = true;

// DMX format: u16 format(3), u16 rate, u32 length (includes 16+16
// bytes of padding), pad[16], samples[length-32] 8-bit unsigned PCM,
// pad[16].
static SfxData* LoadSfx(sfxinfo_t* sfx)
{
    if (sfx->driver_data)
        return (SfxData*)sfx->driver_data;

    if (sfx->lumpnum < 0)
        return nullptr;

    int lumplen = W_LumpLength(sfx->lumpnum);
    if (lumplen < 32)
        return nullptr;

    const uint8_t* lump = (const uint8_t*)W_CacheLumpNum(sfx->lumpnum, PU_STATIC);
    if (!lump)
        return nullptr;

    uint16_t format = lump[0] | (lump[1] << 8);
    uint16_t rate   = lump[2] | (lump[3] << 8);
    uint32_t length = lump[4] | (lump[5]<<8) | (lump[6]<<16) | ((uint32_t)lump[7]<<24);

    if (format != 3 || length < 32 || length > (uint32_t)(lumplen - 8))
        return nullptr;

    SfxData* data = (SfxData*)Z_Malloc(sizeof(SfxData), PU_STATIC, NULL);
    if (!data) return nullptr;

    data->samples = lump + 24;
    data->length  = length - 32;
    data->rate    = rate ? rate : MIX_RATE;

    sfx->driver_data = data;
    return data;
}

static void MixerTask(void*)
{
    static int32_t acc[MIX_FRAMES];
    static int16_t out[MIX_FRAMES];

    #ifdef DOOM_MUSIC_SYNTH
    // MusSynth_Render writes STEREO (2 int32 per frame): it was built
    // for the full Ultravice mixer, which outputs stereo. DoomSound's
    // pipeline is deliberately mono (see IDoomPCMOutput.h), so we
    // render into a scratch stereo buffer and downmix L+R here rather
    // than touching mus_synth.cpp's internals.
    static int32_t musicScratch[MIX_FRAMES * 2];
    #endif

    while (mixRun)
    {
        memset(acc, 0, sizeof(acc));

        #ifdef DOOM_MUSIC_SYNTH
        memset(musicScratch, 0, sizeof(musicScratch));
        MusSynth_Render(musicScratch, MIX_FRAMES);

        for (size_t i = 0; i < MIX_FRAMES; i++)
            acc[i] += (musicScratch[i * 2] + musicScratch[i * 2 + 1]) / 2;
        #endif

        xSemaphoreTake(mixLock, portMAX_DELAY);

        for (auto& ch : channels)
        {
            if (!ch.active) continue;

            for (size_t i = 0; i < MIX_FRAMES; i++)
            {
                uint32_t idx = ch.pos >> 16;
                if (idx >= ch.length) { ch.active = false; break; }

                int32_t s = ((int32_t)ch.data[idx] - 128) << 8;
                acc[i] += (s * ch.vol) / 127;

                ch.pos += ch.step;
            }
        }

        xSemaphoreGive(mixLock);

        for (size_t i = 0; i < MIX_FRAMES; i++)
        {
            int32_t v = acc[i];
            if (v > 32767) v = 32767;
            else if (v < -32768) v = -32768;
            out[i] = (int16_t)v;
        }

        DG_PCM->write(out, MIX_FRAMES);
    }

    mixTask = nullptr;
    vTaskDelete(NULL);
}

static doom_boolean DG_SoundInit(doom_boolean use_sfx_prefix)
{
    sfxPrefix = use_sfx_prefix;
    memset(channels, 0, sizeof(channels));

    if (!DG_PCM)
        return true;   // DOOM_NO_AUDIO: we stay "alive" but silent

    #ifdef DOOM_MUSIC_SYNTH
    MusSynth_Init(MIX_RATE);
    #endif

    mixLock = xSemaphoreCreateMutex();
    if (!mixLock) return false;

    if (!DG_PCM->begin(MIX_RATE))
        return false;

    mixRun = true;

    return xTaskCreatePinnedToCore(
        MixerTask, "doom_mix", 4096, NULL, 5, &mixTask, 0) == pdPASS;
}

static void DG_SoundShutdown(void)
{
    mixRun = false;
    for (int i = 0; i < 20 && mixTask; i++) vTaskDelay(pdMS_TO_TICKS(10));

    #ifdef DOOM_MUSIC_SYNTH
    MusSynth_Shutdown();
    #endif

    if (DG_PCM) DG_PCM->end();

    if (mixLock) { vSemaphoreDelete(mixLock); mixLock = nullptr; }

    // driver_data points into the zone. Without this, reopening Doom
    // leaves dangling pointers behind.
    for (int i = 0; i < NUMSFX; i++)
        S_sfx[i].driver_data = NULL;
}

static int DG_GetSfxLumpNum(sfxinfo_t* sfx)
{
    char namebuf[9];
    if (sfxPrefix) M_snprintf(namebuf, sizeof(namebuf), "ds%s", DEH_String(sfx->name));
    else           M_StringCopy(namebuf, DEH_String(sfx->name), sizeof(namebuf));
    return W_GetNumForName(namebuf);
}

static void DG_SoundUpdate(void) {}

static void DG_UpdateSoundParams(int channel, int vol, int)
{
    if (!mixRun || channel < 0 || channel >= MAX_CHANNELS) return;
    xSemaphoreTake(mixLock, portMAX_DELAY);
    channels[channel].vol = vol;
    xSemaphoreGive(mixLock);
}

static int DG_StartSound(sfxinfo_t* sfx, int channel, int vol, int)
{
    if (!mixRun || channel < 0 || channel >= MAX_CHANNELS) return -1;

    SfxData* data = LoadSfx(sfx);
    if (!data) return -1;

    xSemaphoreTake(mixLock, portMAX_DELAY);
    SfxChannel& ch = channels[channel];
    ch.data = data->samples;
    ch.length = data->length;
    ch.pos = 0;
    ch.step = (uint32_t)(((uint64_t)data->rate << 16) / MIX_RATE);
    ch.vol = vol;
    ch.active = true;
    xSemaphoreGive(mixLock);

    return channel;
}

static void DG_StopSound(int channel)
{
    if (!mixRun || channel < 0 || channel >= MAX_CHANNELS) return;
    xSemaphoreTake(mixLock, portMAX_DELAY);
    channels[channel].active = false;
    xSemaphoreGive(mixLock);
}

static doom_boolean DG_SoundIsPlaying(int channel)
{
    if (!mixRun || channel < 0 || channel >= MAX_CHANNELS) return false;
    return channels[channel].active ? true : false;
}

static void DG_CacheSounds(sfxinfo_t*, int) {}

static snddevice_t sound_devices[] = { SNDDEVICE_SB };

// --- Music ---
//
// By default this is a silent stub: nothing emulates OPL or
// synthesizes MUS here, which keeps this file small and dependency-free
// for the "afternoon project" case.
//
// If you want real music, define DOOM_MUSIC_SYNTH in your hw_conf.h.
// That's a pulse/noise chiptune-style synthesizer (no OPL emulation
// either, but it actually plays the MUS score) already proven in the
// Ultravice project, plus a 4th-order high-pass filter and a limiter
// tuned for small speakers -- see mus_synth.cpp for the synth itself
// and its own comments for why the filter chain looks the way it does.
#ifdef DOOM_MUSIC_SYNTH

static doom_boolean DG_MusicInit(void) { return true; }
static void DG_MusicShutdown(void) {}
static void DG_SetMusicVolume(int volume) { MusSynth_SetVolume(volume); }
static void DG_PauseMusic(void) { MusSynth_Pause(); }
static void DG_ResumeMusic(void) { MusSynth_Resume(); }

// S_ChangeMusic caches the lump and hands us the raw MUS data; the
// pointer stays alive (PU_STATIC) for as long as it's playing, so we
// don't copy it: the handle IS the same pointer.
static void* DG_RegisterSong(void* data, int len)
{
    if (!MusSynth_Register(data, len))
        return NULL;
    return data;
}

static void DG_UnRegisterSong(void*) { MusSynth_Stop(); }

static void DG_PlaySong(void* handle, doom_boolean looping)
{
    if (!handle) return;
    MusSynth_Play(looping ? true : false);
}

static void DG_StopSong(void) { MusSynth_Stop(); }
static doom_boolean DG_MusicIsPlaying(void) { return MusSynth_IsPlaying() ? true : false; }
static void DG_PollMusic(void) {}

#else

static doom_boolean DG_MusicInit(void) { return true; }
static void DG_MusicShutdown(void) {}
static void DG_SetMusicVolume(int) {}
static void DG_PauseMusic(void) {}
static void DG_ResumeMusic(void) {}
static void* DG_RegisterSong(void*, int) { return NULL; }
static void DG_UnRegisterSong(void*) {}
static void DG_PlaySong(void*, doom_boolean) {}
static void DG_StopSong(void) {}
static doom_boolean DG_MusicIsPlaying(void) { return false; }
static void DG_PollMusic(void) {}

#endif // DOOM_MUSIC_SYNTH

static snddevice_t music_devices[] = { SNDDEVICE_SB };

extern "C"
{
sound_module_t DG_sound_module =
{
    sound_devices, 1,
    DG_SoundInit, DG_SoundShutdown, DG_GetSfxLumpNum, DG_SoundUpdate,
    DG_UpdateSoundParams, DG_StartSound, DG_StopSound, DG_SoundIsPlaying,
    DG_CacheSounds,
};

music_module_t DG_music_module =
{
    music_devices, 1,
    DG_MusicInit, DG_MusicShutdown, DG_SetMusicVolume, DG_PauseMusic,
    DG_ResumeMusic, DG_RegisterSong, DG_UnRegisterSong, DG_PlaySong,
    DG_StopSong, DG_MusicIsPlaying, DG_PollMusic,
};
}

void DoomSound_Init()
{
    // No-op: DG_SoundInit already does the work, S_Init calls it.
    // This function exists so the .ino has an entry point symmetric
    // with DG_PCM, without needing to know about sound_module_t.
}
