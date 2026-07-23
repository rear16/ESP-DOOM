#include <DoomSound.h>

#include <Arduino.h>
#include <string.h>

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

// i_sound.c (el motor generico) declara estas dos como extern y las
// bindea en I_BindSoundVariables, pero quien las define en Doom
// original es i_sdlsound.c, que aqui no existe: sin esto no linkea.
// No resampleamos con libsamplerate (el mixer usa paso fijo 16.16),
// asi que solo necesitan existir con los defaults de chocolate-doom.
extern "C"
{
    int use_libsamplerate = 0;
    float libsamplerate_scale = 0.65f;
}

static constexpr uint32_t MIX_RATE = 11025;   // nativo de los lumps DMX
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

// Formato DMX: u16 format(3), u16 rate, u32 length (incluye 16+16 de
// pad), pad[16], samples[length-32] PCM 8bit unsigned, pad[16].
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

    while (mixRun)
    {
        memset(acc, 0, sizeof(acc));

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
        return true;   // DOOM_NO_AUDIO: seguimos "vivos" pero mudos

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

    if (DG_PCM) DG_PCM->end();

    if (mixLock) { vSemaphoreDelete(mixLock); mixLock = nullptr; }

    // Ver comentario en el ESP-DOOM completo (Ultravice): driver_data
    // apunta a la zone. Sin esto, reabrir Doom deja punteros colgando.
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

// --- Musica: stub en silencio. Nadie emula OPL ni sintetiza MUS aqui. ---
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
    // No-op: DG_SoundInit ya hace el trabajo, S_Init la llama.
    // Esta funcion existe para que el .ino tenga un punto de entrada
    // simetrico con DG_PCM, sin tener que saber de sound_module_t.
}