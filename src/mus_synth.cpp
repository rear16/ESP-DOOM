#include "mus_synth.h"

#include <Arduino.h>
#include <math.h>
#include <string.h>

// ------------------------------------------------------------------
//  MUS format
// ------------------------------------------------------------------
//
//  Header:
//    char     id[4]        "MUS\x1a"
//    uint16   scoreLen
//    uint16   scoreStart
//    uint16   channels
//    uint16   secChannels
//    uint16   instrCnt
//    uint16   dummy
//    uint16   instruments[instrCnt]
//
//  Event: 1 byte  [last:1][type:3][channel:4]
//    0 release note      1 byte : note
//    1 play note         1 byte : [volflag:1][note:7] (+1 byte vol)
//    2 pitch bend        1 byte
//    3 system event      1 byte
//    4 controller        2 bytes: controller, value
//    5 end of measure    -
//    6 score end         -
//    7 unused            1 byte
//
//  If the 'last' bit is set, a variable-length delay follows (7 usable
//  bits per byte). The score runs at 140 ticks/second.

static constexpr uint32_t MUS_TICK_HZ = 140;

static constexpr int MUS_CHANNELS = 16;
static constexpr int MUS_PERCUSSION_CHANNEL = 15;

static constexpr int MAX_VOICES = 16;

// ------------------------------------------------------------------
//  Sequencer state
// ------------------------------------------------------------------

static const uint8_t* score = nullptr;
static uint32_t scoreLen = 0;
static uint32_t scorePos = 0;
static uint32_t scoreStart = 0;

static bool playing = false;
static bool paused = false;
static bool loopSong = false;

static uint32_t waitTicks = 0;

static uint32_t sampleRate = 11025;
static float samplesPerTick = 78.75f;
static float tickAccum = 0.0f;

static int musicVolume = 96;   // 0-127

// Per-channel volume and bend
static uint8_t chanVolume[MUS_CHANNELS];
static uint8_t chanLastVol[MUS_CHANNELS];
static float chanBend[MUS_CHANNELS];   // factor multiplicativo

// ------------------------------------------------------------------
//  Voices
// ------------------------------------------------------------------

struct Voice
{
    bool active;
    bool percussion;
    uint8_t channel;
    uint8_t note;

    uint32_t phase;      // 16.16
    uint32_t step;       // 16.16
    uint32_t duty;       // punto de corte del pulso, 16.16

    float env;           // 0..1
    float decay;
    float release;
    float sustain;
    bool released;

    uint32_t stepEnd;    // sweep target, 0 = no sweep
    uint8_t  noiseMix;
    uint8_t  noiseRate;
    uint8_t  noiseCount;
    float    noiseHold;

    uint32_t noise;      // LFSR
    uint32_t age;
};

static Voice voices[MAX_VOICES];
static uint32_t voiceClock = 0;

static volatile int voicesPeak = 0;
static volatile uint32_t voiceSteals = 0;
static volatile int32_t musPeak = 0;

// ------------------------------------------------------------------
//  GM percussion
// ------------------------------------------------------------------
//
//  In MUS, channel 15 uses the NOTE to pick the drum. It used to be
//  ignored and everything sounded like the same 1-second noise burst.
//
//  tone = 0 -> pure noise. tone > 0 -> pulse at that frequency.
//  Note: the kick (~90 Hz) won't be reproduced by a small speaker; it
//  ends up as a click. That's what you get from a 20 mm cone.

struct DrumDef
{
    uint16_t decayMs;
    uint16_t tone;      // 0 = no tonal component
    uint16_t toneEnd;   // barrido de altura; 0 = sin barrido
    uint8_t  noiseMix;  // 0-255: cuanto ruido contra tono
    uint8_t  noiseRate; // divisor del LFSR: 1 brillante, >1 oscuro
};

static DrumDef GetDrum(int note)
{
    switch (note)
    {
        //                 ms  tone  end  noise  color
        case 35: case 36: return {  70,  180,  60,   50, 4 };  // bombo
        case 37:          return {  40,  900,   0,  180, 2 };  // side stick
        case 38: case 40: return { 150,  240, 200,  200, 2 };  // caja
        case 39:          return { 120,    0,   0,  255, 2 };  // palmada
        case 42: case 44: return {  40,    0,   0,  255, 1 };  // hi-hat cerrado
        case 46:          return { 250,    0,   0,  255, 1 };  // hi-hat abierto
        case 49: case 57: return { 600,    0,   0,  255, 1 };  // crash
        case 51: case 59: return { 350, 3000,   0,  210, 1 };  // ride
        case 52:          return { 500,    0,   0,  255, 1 };  // china
        case 53:          return { 300, 2400,   0,   60, 1 };  // campana
        case 54:          return { 100,    0,   0,  255, 1 };  // pandereta
        case 55:          return { 300,    0,   0,  255, 1 };  // splash
        case 56:          return { 150,  840,   0,   30, 1 };  // cencerro
        case 41: case 43: return { 220,  180, 110,   70, 3 };  // tom grave
        case 45: case 47: return { 200,  260, 160,   70, 3 };  // tom medio
        case 48: case 50: return { 180,  340, 220,   70, 3 };  // tom agudo
        default:          return { 100,    0,   0,  255, 2 };
    }
}

// per-sample decay to reach 0.001 in ms milliseconds
static inline float DecayForMs(float ms)
{
    return expf(-6.908f / (ms * 0.001f * (float)sampleRate));
}

// ------------------------------------------------------------------

static inline float NoteFreq(int note, float bend)
{
    return 440.0f * powf(2.0f, (note - 69) / 12.0f) * bend;
}

static void VoiceOff(int channel, int note)
{
    for (int i = 0; i < MAX_VOICES; i++)
    {
        Voice& v = voices[i];

        if (v.active && !v.released &&
            v.channel == channel && v.note == note)
        {
            v.released = true;
            return;
        }
    }
}

static int AllocVoice()
{
    int oldest = 0;
    uint32_t oldestAge = 0xffffffff;

    for (int i = 0; i < MAX_VOICES; i++)
    {
        if (!voices[i].active)
            return i;


        // We prefer to steal one that's already been released.
        uint32_t age = voices[i].released ? voices[i].age : voices[i].age + 0x40000000;

        if (age < oldestAge)
        {
            oldestAge = age;
            oldest = i;
        }
    }

    voiceSteals++;   // no habia libre: robamos

    return oldest;
}

static void VoiceOn(int channel, int note, int vol)
{
    int i = AllocVoice();

    Voice& v = voices[i];

    v.active = true;
    v.channel = channel;
    v.note = note;
    v.percussion = (channel == MUS_PERCUSSION_CHANNEL);
    v.phase = 0;
    v.released = false;
    v.age = voiceClock++;
    v.noise = 0xACE1u + i * 7919u;

    if (v.percussion)
    {
        DrumDef d = GetDrum(note);

        v.step = d.tone
            ? (uint32_t)(((float)d.tone * 65536.0f) / (float)sampleRate)
            : 0;

        // The pitch sweep is what makes a drum sound like a drum
        // instead of a beep: the head detunes as it's struck.
        v.stepEnd = d.toneEnd
            ? (uint32_t)(((float)d.toneEnd * 65536.0f) / (float)sampleRate)
            : 0;

        v.noiseMix = d.noiseMix;
        v.noiseRate = d.noiseRate ? d.noiseRate : 1;
        v.noiseCount = 0;
        v.noiseHold = 0.0f;

        v.duty = 0x8000;
        v.env = 1.0f;
        v.decay = DecayForMs((float)d.decayMs);
        v.release = v.decay;
        v.sustain = 0.0f;
    }
    else
    {
        float f = NoteFreq(note, chanBend[channel]);

        v.step = (uint32_t)((f * 65536.0f) / (float)sampleRate);

        // Different duty per channel: gives some timbral separation
        // for free. 50%, 25% and 12.5%, like any 80s sound chip.
        static const uint32_t duties[3] =
        {
            0x8000 << 0, 0x4000 << 0, 0x2000 << 0
        };

        v.duty = duties[channel % 3] << 0;
        v.stepEnd = 0;
        v.noiseMix = 0;
        v.env = 1.0f;
        v.decay = 0.99997f;
        v.sustain = 0.75f;
        v.release = DecayForMs(120.0f);   // era 626 ms: comia voces
    }

    // note velocity x channel volume
    float amp = (vol / 127.0f) * (chanVolume[channel] / 127.0f);

    v.env = amp;
    v.sustain *= amp;
}

static void AllNotesOff()
{
    for (int i = 0; i < MAX_VOICES; i++)
        voices[i].active = false;
}

// ------------------------------------------------------------------
//  Sequencer
// ------------------------------------------------------------------

static void ScoreRewind()
{
    scorePos = scoreStart;
    waitTicks = 0;

    for (int i = 0; i < MUS_CHANNELS; i++)
    {
        chanVolume[i] = 100;
        chanLastVol[i] = 100;
        chanBend[i] = 1.0f;
    }
}

static void ScoreTick()
{
    if (waitTicks > 0)
    {
        waitTicks--;
        return;
    }

    // A tick can carry several events: they're processed until one
    // sets 'last' and hands back the delay.
    for (int guard = 0; guard < 64; guard++)
    {
        if (scorePos >= scoreLen)
        {
            playing = false;
            return;
        }

        uint8_t desc = score[scorePos++];

        bool last = (desc & 0x80) != 0;
        int type = (desc >> 4) & 7;
        int chan = desc & 15;

        switch (type)
        {
            case 0: // release note
            {
                if (scorePos >= scoreLen) { playing = false; return; }
                int note = score[scorePos++] & 0x7f;
                VoiceOff(chan, note);
                break;
            }

            case 1: // play note
            {
                if (scorePos >= scoreLen) { playing = false; return; }
                uint8_t b = score[scorePos++];
                int note = b & 0x7f;
                int vol = chanLastVol[chan];

                if (b & 0x80)
                {
                    if (scorePos >= scoreLen) { playing = false; return; }
                    vol = score[scorePos++] & 0x7f;
                    chanLastVol[chan] = vol;
                }

                VoiceOn(chan, note, vol);
                break;
            }

            case 2: // pitch bend: 0-255, 128 = centro, rango +-2 semitonos
            {
                if (scorePos >= scoreLen) { playing = false; return; }
                uint8_t b = score[scorePos++];
                chanBend[chan] = powf(2.0f, ((b - 128) / 64.0f) / 12.0f);

                // The bend used to only apply when the note was
                // triggered, i.e. it did nothing. Now it retunes
                // whatever's currently sounding.
                for (int i = 0; i < MAX_VOICES; i++)
                {
                    Voice& v = voices[i];

                    if (v.active && !v.percussion && v.channel == chan)
                    {
                        float f = NoteFreq(v.note, chanBend[chan]);
                        v.step = (uint32_t)((f * 65536.0f) / (float)sampleRate);
                    }
                }
                break;
            }

            case 3: // system event
            {
                if (scorePos >= scoreLen) { playing = false; return; }
                scorePos++;
                break;
            }

            case 4: // controller
            {
                if (scorePos + 1 >= scoreLen) { playing = false; return; }
                uint8_t ctrl = score[scorePos++] & 0x7f;
                uint8_t val = score[scorePos++] & 0x7f;

                if (ctrl == 3)          // channel volume
                    chanVolume[chan] = val;

                break;
            }

            case 5: // fin de compas
                break;

            case 6: // fin del score
            {
                if (loopSong)
                {
                    AllNotesOff();
                    ScoreRewind();
                }
                else
                {
                    playing = false;
                }
                return;
            }

            case 7: // sin uso
            {
                if (scorePos >= scoreLen) { playing = false; return; }
                scorePos++;
                break;
            }
        }

        if (last)
        {
            uint32_t delay = 0;
            uint8_t b;

            do
            {
                if (scorePos >= scoreLen) { playing = false; return; }
                b = score[scorePos++];
                delay = delay * 128 + (b & 0x7f);
            }
            while (b & 0x80);

            waitTicks = delay;
            return;
        }
    }
}

// ------------------------------------------------------------------
//  Render
// ------------------------------------------------------------------

static void RenderVoices(int32_t* acc, size_t frames, size_t offset)
{
    float master = musicVolume / 127.0f;

    for (int i = 0; i < MAX_VOICES; i++)
    {
        Voice& v = voices[i];

        if (!v.active)
            continue;

        for (size_t n = 0; n < frames; n++)
        {
            float s;

            if (v.percussion)
            {
                float tone = 0.0f;
                float noise = 0.0f;

                if (v.step)
                {
                    v.phase += v.step;
                    tone = ((v.phase & 0xffff) < v.duty) ? 1.0f : -1.0f;

                    // Sweep toward stepEnd
                    if (v.stepEnd)
                        v.step += ((int32_t)v.stepEnd - (int32_t)v.step) >> 9;
                }

                if (v.noiseMix)
                {
                    // Sample-and-hold on the LFSR: lowering the rate
                    // darkens the noise. Hi-hat rate 1, snare rate 2.
                    if (++v.noiseCount >= v.noiseRate)
                    {
                        v.noiseCount = 0;

                        uint32_t bit = ((v.noise >> 0) ^ (v.noise >> 2) ^
                                        (v.noise >> 3) ^ (v.noise >> 5)) & 1;
                        v.noise = (v.noise >> 1) | (bit << 15);

                        v.noiseHold = (v.noise & 1) ? 1.0f : -1.0f;
                    }

                    noise = v.noiseHold;
                }

                float m = v.noiseMix / 255.0f;
                s = tone * (1.0f - m) + noise * m;
            }
            else
            {
                v.phase += v.step;
                s = ((v.phase & 0xffff) < v.duty) ? 1.0f : -1.0f;
            }

            s *= v.env * master * 3500.0f;

            int32_t out = (int32_t)s;

            acc[(offset + n) * 2 + 0] += out;
            acc[(offset + n) * 2 + 1] += out;

            // Envelope
            if (v.released || v.percussion)
            {
                v.env *= v.percussion ? v.decay : v.release;

                if (v.env < 0.001f)
                {
                    v.active = false;
                    break;
                }
            }
            else if (v.env > v.sustain)
            {
                v.env *= v.decay;
            }
        }
    }
}

// NOTE: this measures the music bus BEFORE the EQ. The limiter runs
// AFTER the HPF, and since E1M1 lives entirely in the 82-165 Hz range,
// the filter removes ~85% of it before the limiter ever sees it. So
// this number does NOT tell you whether the music triggers the
// limiter; for that, look at "lim ... active %" in i_sound_esp32.cpp's
// report. It's only useful to see the melody/percussion balance
// before filtering.

static uint32_t lastPeakReport = 0;

static void ReportPeak()
{
    uint32_t now = millis();

    if (now - lastPeakReport < 2000)
        return;

    lastPeakReport = now;

    Serial.printf(
        "[musica] pico pre-EQ %5d\n", (int)musPeak);

    musPeak = 0;
}

// Dedicated music bus. Needed to measure its peak without SFX
// contaminating the reading, and it leaves the door open to limiting
// it separately.
static constexpr size_t MUS_MAX_FRAMES = 256;
static int32_t musBus[MUS_MAX_FRAMES * 2];

// Tempo diagnostic. Reports every ~2 seconds, from inside MusSynth_Render
// (i.e. from the mixer task, NOT the main loop -- printing from a task
// pinned to core 0 uses Serial's ISR-safe path in Arduino-ESP32, which
// is fine, but keep the format terse).
//
// At the correct tempo we should see ticks/sec ~= 140.0, and
// samples/sec ~= sampleRate (11025 by default). If ticks/sec is higher
// than 140, the score is running faster than the audio clock -- which
// is exactly the "music sounds sped up" symptom.
static uint32_t diagSamples = 0;
static uint32_t diagTicks   = 0;
static uint32_t diagLastReportMs = 0;

void MusSynth_Render(int32_t* acc, size_t frames)
{
    if (!playing || paused)
        return;

    if (frames > MUS_MAX_FRAMES)
        frames = MUS_MAX_FRAMES;

    memset(musBus, 0, frames * 2 * sizeof(int32_t));

    size_t done = 0;

    diagSamples += frames;

    while (done < frames)
    {
        // When tickAccum crosses zero, fire the next MUS event and
        // credit the sample budget for the tick that just started.
        while (tickAccum <= 0.0f)
        {
            ScoreTick();
            diagTicks++;
            tickAccum += samplesPerTick;

            if (!playing)
                break;
        }

        if (!playing)
            break;

        // Render up to the smaller of: (a) how many samples the current
        // tick has left, or (b) how many the caller still needs. NOTE
        // the +0.999f: without it, (size_t)78.75 truncates to 78 and we
        // silently drop 0.75 samples per tick against the audio clock.
        // Sustained over a song that's a ~1% tempo drift -- exactly the
        // "music sounds sped up" symptom.
        size_t chunk = (size_t)(tickAccum + 0.999f);

        if (chunk > frames - done)
            chunk = frames - done;

        if (chunk == 0)
            chunk = 1;

        RenderVoices(musBus, chunk, done);

        done += chunk;
        // Debit the EXACT number of samples rendered, keeping the
        // fractional part alive across iterations. Losing it here was
        // the bug.
        tickAccum -= (float)chunk;
    }

    // Measure the music's own peak and add it into the main bus.
    for (size_t i = 0; i < frames * 2; i++)
    {
        int32_t v = musBus[i];
        int32_t m = v < 0 ? -v : v;

        if (m > musPeak)
            musPeak = m;

        acc[i] += v;
    }

    ReportPeak();

    // Tempo diagnostic. See comment near diagSamples/diagTicks above.
    uint32_t nowMs = millis();
    if (nowMs - diagLastReportMs >= 2000)
    {
        uint32_t deltaMs = nowMs - diagLastReportMs;
        if (diagLastReportMs != 0 && deltaMs > 0)
        {
            float ticksPerSec   = (float)diagTicks   * 1000.0f / (float)deltaMs;
            float samplesPerSec = (float)diagSamples * 1000.0f / (float)deltaMs;
            Serial.printf(
                "[musica] tempo: %.1f ticks/s (esperado 140.0) | %.0f samples/s (esperado %u)\n",
                ticksPerSec, samplesPerSec, (unsigned)sampleRate);
        }
        diagLastReportMs = nowMs;
        diagSamples = 0;
        diagTicks = 0;
    }
}

// ------------------------------------------------------------------
//  API
// ------------------------------------------------------------------

bool MusSynth_Init(uint32_t rate)
{
    sampleRate = rate;
    samplesPerTick = (float)rate / (float)MUS_TICK_HZ;
    tickAccum = 0.0f;

    memset(voices, 0, sizeof(voices));

    playing = false;
    paused = false;
    score = nullptr;

    return true;
}

void MusSynth_Shutdown()
{
    playing = false;
    score = nullptr;
    AllNotesOff();
}

bool MusSynth_Register(const void* data, int len)
{
    const uint8_t* d = (const uint8_t*)data;

    if (!d || len < 16)
        return false;

    if (memcmp(d, "MUS\x1a", 4) != 0)
    {
        Serial.println("[musica] lump no es MUS");
        return false;
    }

    uint16_t sLen   = d[4] | (d[5] << 8);
    uint16_t sStart = d[6] | (d[7] << 8);

    if (sStart >= len)
        return false;

    score = d;
    scoreStart = sStart;

    scoreLen = sStart + sLen;

    if (scoreLen > (uint32_t)len)
        scoreLen = len;

    return true;
}

void MusSynth_Play(bool looping)
{
    if (!score)
        return;

    loopSong = looping;

    AllNotesOff();
    ScoreRewind();

    tickAccum = 0.0f;
    paused = false;
    playing = true;
}

void MusSynth_Stop()
{
    playing = false;
    AllNotesOff();
}

void MusSynth_Pause()
{
    paused = true;
}

void MusSynth_Resume()
{
    paused = false;
}

bool MusSynth_IsPlaying()
{
    return playing;
}

void MusSynth_Stats(int* peakVoices, uint32_t* steals)
{
    int n = 0;

    for (int i = 0; i < MAX_VOICES; i++)
        if (voices[i].active)
            n++;

    if (n > voicesPeak)
        voicesPeak = n;

    if (peakVoices) *peakVoices = voicesPeak;
    if (steals) *steals = voiceSteals;

    voicesPeak = 0;
    voiceSteals = 0;
}

void MusSynth_SetVolume(int volume)
{
    if (volume < 0) volume = 0;
    else if (volume > 127) volume = 127;

    musicVolume = volume;
}