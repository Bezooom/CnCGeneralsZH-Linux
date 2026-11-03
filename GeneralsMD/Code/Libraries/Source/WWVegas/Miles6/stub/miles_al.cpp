/*
**  Miles Sound System 6.5 ABI, OpenAL backend for the Linux port.
**
**  Replaces the no-op stub (miles.c, __linux__ branch) as the source of the
**  `milesstub` target on Linux: libmss32.so backed by OpenAL Soft.  The engine
**  (MilesAudioManager.cpp) speaks the ABI declared in MSS/MSS.h; handles come
**  from fixed slot tables so the (UnsignedInt)handle callback ABI stays safe:
**  handle = (void*)(uintptr_t)(slot + 1), values <= 256, so the engine's
**  UnsignedInt casts round-trip through the table on LP64.
**
**  Threading model: every entry point takes g_mx (recursive).  One internal
**  thread polls every 20 ms for end-of-sample callbacks and pumps stream
**  buffer queues; the OpenAL context is current on that thread and on the
**  thread that ran AIL_quick_startup - matching how the real mss32.dll invoked
**  EOS callbacks from its own timer thread.
**
**  Decode: in-memory RIFF WAV (PCM 8/16 and IMA ADPCM) for samples; streams
**  sniff RIFF WAV or MP3 (minimp3, vendored under stub/minimp3/) and read
**  through the engine's AIL_set_file_callbacks, so music and speech come out
**  of the .big archives without the shim touching the filesystem itself.
*/

#if defined(__linux__)

#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"
#include "minimp3_ex.h"

#include <AL/al.h>
#include <AL/alc.h>

#include "MSS/MSS.h"

#ifndef WAVE_FORMAT_PCM
#define WAVE_FORMAT_PCM 1
#endif

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

namespace
{

/* ---- handle tables ------------------------------------------------------ */

const int MAX_SAMPLES = 128;
const int MAX_3D      = 128;
const int MAX_STREAMS = 8;

inline void *handleOf(int slot) { return (void *)(uintptr_t)(slot + 1); }
inline int   slotOf(void *h)    { return (int)((uintptr_t)h) - 1; }

/* 3D provider id: enumeration hands it out; 0 stays "no provider", so the
 * Miles convention AIL_open_3D_provider(0) keeps failing (nonzero). */
const HPROVIDER PROVIDER_ID = 0x5A520001u;
const char kProviderName[]  = "Miles Fast 2D Positional Audio";

/* the one listener the engine opens; a token distinct from sample slots */
const uintptr_t LISTENER_TOKEN = 0xF0000000u;

/* Generals world units are huge; OpenAL geometry wants meters */
const F32 UNIT_SCALE = 0.01f;

/* one 64 KiB block of PCM per stream AL buffer */
const size_t STREAM_CHUNK_BYTES = 65536;
/* bounded queue depth per stream */
const int STREAM_BUFFER_COUNT = 8;

/* ---- global state -------------------------------------------------------- */

std::recursive_mutex g_mx;

ALCdevice  *g_dev = NULL;
ALCcontext *g_ctx = NULL;
HDIGDRIVER  g_digToken = NULL;    /* stable non-NULL sentinel */

bool              g_threadRunning = false;
std::atomic<bool> g_stopThread(false);
std::thread       g_thread;

/* engine file callbacks (Miles ABI; the U32 handle ids belong to the engine) */
AILFILEOPENCB  g_openCb = NULL;
AILFILECLOSECB g_closeCb = NULL;
AILFILESEEKCB  g_seekCb = NULL;
AILFILEREADCB  g_readCb = NULL;

/* 3D provider / listener state */
int  g_providerOpen = 0;
bool g_listenerOpen = false;
F32  g_listenerPos[3] = {0, 0, 0};

/* ---- slots ---------------------------------------------------------------- */

struct SampleSlot
{
    bool        used = false;
    ALuint      src = 0;
    ALuint      buf = 0;          /* 0 = no sound attached */
    S32         fileRate = 0;     /* native rate of the loaded file, Hz */
    S32         rateHz = 0;       /* explicit playback rate; 0 = native */
    F32         volume = 1.0f;    /* 0..1 */
    F32         pan = 0.5f;       /* 0..1 */
    S32         user[4] = {0, 0, 0, 0};
    AILSAMPLECB cb = NULL;
    bool        started = false;  /* playing and not yet ended */
    bool        eosFired = true;
    U32         loopsRemaining = 0;   /* quick API only; engine leaves 0 */
};
SampleSlot g_samples[MAX_SAMPLES];

struct Sample3DSlot
{
    bool          used = false;
    ALuint        src = 0;
    ALuint        buf = 0;
    S32           fileRate = 0;
    S32           rateHz = 0;
    F32           volume = 1.0f;
    F32           nearDist = 1.0f;    /* full volume within */
    F32           farDist = 2.0f;     /* silent beyond */
    F32           occlusion = 0.0f;   /* 0..1, scales gain */
    F32           pos[3] = {0, 0, 0};
    S32           user[4] = {0, 0, 0, 0};
    AIL3DSAMPLECB cb = NULL;
    bool          started = false;
    bool          eosFired = true;
};
Sample3DSlot g_3d[MAX_3D];

struct StreamBuf
{
    ALuint id = 0;
    bool   queued = false;
};

struct StreamSlot
{
    bool     used = false;
    ALuint   src = 0;

    U32      file = 0;            /* engine file handle id, 0 = closed */

    int      kind = 0;            /* 1 = WAV, 2 = MP3 */
    uint64_t dataStart = 0;       /* WAV: byte offset of the data payload */
    uint64_t dataLen = 0;         /* WAV: payload bytes */
    S32      rate = 0;
    S32      channels = 0;
    S32      bits = 0;
    uint64_t readPos = 0;         /* WAV: next byte inside the data payload */

    mp3dec_ex_t mp3;
    mp3dec_io_t io;
    bool     mp3Open = false;

    std::vector<StreamBuf> buffers;   /* fixed pool, recycled in place */

    bool     playing = false;
    bool     paused = false;
    bool     passEof = false;     /* current pass decoded to end of file */
    S32      loopsRemaining = 0;  /* Miles: extra loops, -1 = infinite */
    bool     cbFired = true;
    AILSTREAMCB cb = NULL;

    F32      volume = 1.0f;
    F32      pan = 0.5f;

    uint64_t playedFrames = 0;    /* fully played sample frames */
    S32      totalMs = 0;
};
StreamSlot g_streams[MAX_STREAMS];

/* ---- helpers ---------------------------------------------------------------- */

F32 clampF(F32 v, F32 lo, F32 hi) { return v < lo ? lo : (v > hi ? hi : v); }

void toAL(F32 gx, F32 gy, F32 gz, F32 out[3])
{
    /* Generals: z-up.  OpenAL: y-up; keep handedness by (x, z, y). */
    out[0] = gx;
    out[1] = gz;
    out[2] = gy;
}

/* ---- RIFF WAV parsing --------------------------------------------------------- */

bool parseWavImage(const uint8_t *p, size_t maxBytes, AILSOUNDINFO &info)
{
    if (!p || maxBytes < 12 || memcmp(p, "RIFF", 4) != 0 || memcmp(p + 8, "WAVE", 4) != 0)
        return false;

    U32 riffSize = 0;
    memcpy(&riffSize, p + 4, 4);
    size_t n = maxBytes;
    if ((size_t)riffSize + 8u < n && riffSize >= 4u)
        n = (size_t)riffSize + 8u;

    const AILSOUNDINFO zero = {0};
    AILSOUNDINFO out = zero;
    out.initial_ptr = p;

    bool haveFmt = false, haveData = false;
    size_t pos = 12;
    while (pos + 8 <= n)
    {
        const char *id = (const char *)p + pos;
        U32 size;
        memcpy(&size, p + pos + 4, 4);
        size_t body = pos + 8;
        if (body + size > n)
            size = (U32)(n - body);

        if (memcmp(id, "fmt ", 4) == 0 && size >= 16)
        {
            U16 format, channels, bits, blockAlign;
            U32 rate;
            memcpy(&format, p + body + 0, 2);
            memcpy(&channels, p + body + 2, 2);
            memcpy(&rate, p + body + 4, 4);
            memcpy(&blockAlign, p + body + 12, 2);
            memcpy(&bits, p + body + 14, 2);
            if (format != WAVE_FORMAT_PCM && format != WAVE_FORMAT_IMA_ADPCM)
                return false;
            if (channels < 1 || bits == 0 || blockAlign == 0 || rate == 0)
                return false;
            out.format     = (S32)format;
            out.channels   = (S32)channels;
            out.rate       = rate;
            out.bits       = (S32)bits;
            out.block_size = (U32)blockAlign;
            haveFmt = true;
        }
        else if (memcmp(id, "data", 4) == 0)
        {
            out.data_ptr = p + body;
            out.data_len = size;
            haveData = true;
        }

        if (size == 0)
            break;
        pos = body + size + (size & 1);
    }

    if (!haveFmt || !haveData || out.data_len == 0)
        return false;

    if (out.format == WAVE_FORMAT_IMA_ADPCM)
    {
        const U32 blocks = out.data_len / out.block_size;
        if (out.block_size < (U32)out.channels * 4u)
            return false;
        const U32 perBlock = ((out.block_size / (U32)out.channels) - 4u) * 2u + 1u;
        out.samples = blocks * perBlock;
    }
    else
    {
        const U32 frameBytes = (U32)out.channels * ((U32)out.bits / 8u);
        out.samples = frameBytes ? out.data_len / frameBytes : 0;
    }

    info = out;
    return true;
}

void buildWavHeader(uint8_t *h, U32 format, U32 channels, U32 rate, U32 bits, U32 dataLen)
{
    const U32 byteRate = rate * channels * (bits / 8u);
    const U16 blockAlign = (U16)(channels * (bits / 8u));
    const U32 riffSize = 36 + dataLen;
    const U32 fmtSize = 16;
    memcpy(h + 0, "RIFF", 4);
    memcpy(h + 4, &riffSize, 4);
    memcpy(h + 8, "WAVE", 4);
    memcpy(h + 12, "fmt ", 4);
    memcpy(h + 16, &fmtSize, 4);
    memcpy(h + 20, &format, 2);
    memcpy(h + 22, &channels, 2);
    memcpy(h + 24, &rate, 4);
    memcpy(h + 28, &byteRate, 4);
    memcpy(h + 32, &blockAlign, 2);
    memcpy(h + 34, &bits, 2);
    memcpy(h + 36, "data", 4);
    memcpy(h + 40, &dataLen, 4);
}

/* ---- IMA ADPCM decode (CCITT G.726 / DVI, as WAV tags it) ---------------------- */

const int kStepIndex[16] = { -1, -1, -1, -1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096 };
const int kStep[] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73,
    80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 210, 231, 253, 279, 307, 337, 371, 408, 449, 494,
    544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499,
    2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487,
    12635, 13919, 15311, 16842, 18527, 20380, 22418, 24660, 27126, 29839, 32823, 36105, 39716, 43687, 48056
};

bool decompressIma(const AILSOUNDINFO &info, std::vector<int16_t> &pcm)
{
    if (info.format != WAVE_FORMAT_IMA_ADPCM || info.channels < 1 || info.block_size == 0 || !info.data_ptr)
        return false;
    const uint32_t ch = (uint32_t)info.channels;
    const uint32_t block = info.block_size;
    if (block < ch * 4u || (block % ch) != 0)
        return false;
    const uint32_t dataPerCh = block / ch - 4u;
    const uint32_t samplesPerBlock = dataPerCh * 2u + 1u;
    const uint32_t blocks = info.data_len / block;
    if (blocks == 0)
        return false;

    pcm.resize((size_t)blocks * samplesPerBlock * (size_t)ch);
    const uint8_t *src = (const uint8_t *)info.data_ptr;

    size_t w = 0;
    for (uint32_t b = 0; b < blocks; ++b)
    {
        std::vector<int> predictor(ch);
        std::vector<int> stepIdx(ch, 0);
        for (uint32_t c = 0; c < ch; ++c)
        {
            const uint8_t *blk = src + (size_t)b * block + (size_t)c * 4u;
            int16_t pred;
            memcpy(&pred, blk, 2);
            predictor[c] = pred;
            stepIdx[c] = blk[2] & 0x1F;
        }
        for (uint32_t s = 0; s < samplesPerBlock; ++s)
        {
            for (uint32_t c = 0; c < ch; ++c)
            {
                int out;
                if (s == 0)
                {
                    out = predictor[c];
                }
                else
                {
                    const uint32_t byteNo = (s - 1) >> 1;
                    const uint8_t *col = src + (size_t)b * block + (size_t)c * 4u + 4u;
                    uint8_t nib = col[byteNo];
                    if (s & 1)
                        nib &= 0x0F;
                    else
                        nib >>= 4;

                    const int step = kStep[stepIdx[c]];
                    int diff = 0;
                    if (nib & 4) diff += step;
                    if (nib & 2) diff += step >> 1;
                    if (nib & 1) diff += step >> 2;
                    diff += step >> 3;
                    if (nib & 8) diff = -diff;

                    int p = predictor[c] + diff;
                    if (p > 32767) p = 32767;
                    if (p < -32768) p = -32768;
                    predictor[c] = p;
                    out = p;

                    stepIdx[c] += kStepIndex[nib];
                    if (stepIdx[c] < 0) stepIdx[c] = 0;
                    if (stepIdx[c] > 88) stepIdx[c] = 88;
                }
                pcm[w++] = (int16_t)out;
            }
        }
    }
    return true;
}

/* ---- per-slot AL state application ---------------------------------------------- */

void applySamplePitch(SampleSlot &s)
{
    if (!s.src)
        return;
    F32 pitch = 1.0f;
    if (s.fileRate > 0 && s.rateHz > 0)
        pitch = (F32)s.rateHz / (F32)s.fileRate;
    alSourcef(s.src, AL_PITCH, clampF(pitch, 0.5f, 2.0f));
}

void applySampleGainPan(SampleSlot &s)
{
    if (!s.src)
        return;
    alSourcei(s.src, AL_SOURCE_RELATIVE, AL_TRUE);
    alSourcef(s.src, AL_ROLLOFF_FACTOR, 0.0f);
    alSourcef(s.src, AL_GAIN, clampF(s.volume, 0.0f, 1.0f));
    const F32 x = 2.0f * clampF(s.pan, 0.0f, 1.0f) - 1.0f;
    alSource3f(s.src, AL_POSITION, x, 0.0f, -1.0f);
}

F32 gain3D(const Sample3DSlot &s)
{
    const F32 dx = s.pos[0] - g_listenerPos[0];
    const F32 dy = s.pos[1] - g_listenerPos[1];
    const F32 dz = s.pos[2] - g_listenerPos[2];
    const F32 d = (F32)std::sqrt((double)(dx * dx + dy * dy + dz * dz));

    F32 rolloff;
    if (d <= s.nearDist)
        rolloff = 1.0f;
    else if (d >= s.farDist)
        rolloff = 0.0f;
    else
        rolloff = 1.0f - (d - s.nearDist) / (s.farDist - s.nearDist);

    const F32 occ = 1.0f - clampF(s.occlusion, 0.0f, 1.0f);
    return clampF(s.volume, 0.0f, 1.0f) * rolloff * occ;
}

void apply3D(Sample3DSlot &s)
{
    if (!s.src)
        return;
    F32 a[3];
    toAL(s.pos[0] - g_listenerPos[0], s.pos[1] - g_listenerPos[1], s.pos[2] - g_listenerPos[2], a);
    alSourcei(s.src, AL_SOURCE_RELATIVE, AL_TRUE);
    alSourcef(s.src, AL_ROLLOFF_FACTOR, 0.0f);
    alSource3f(s.src, AL_POSITION, a[0] * UNIT_SCALE, a[1] * UNIT_SCALE, a[2] * UNIT_SCALE);
    alSourcef(s.src, AL_GAIN, gain3D(s));
}

/* attach an already-decoded PCM image to a 2D sample slot */
bool slotAttachPcm(SampleSlot &s, S32 rate, S32 channels, S32 bits, const void *data, U32 len)
{
    ALenum fmt;
    if (channels == 1 && bits == 8) fmt = AL_FORMAT_MONO8;
    else if (channels == 2 && bits == 8) fmt = AL_FORMAT_STEREO8;
    else if (channels == 1 && bits == 16) fmt = AL_FORMAT_MONO16;
    else if (channels == 2 && bits == 16) fmt = AL_FORMAT_STEREO16;
    else return false;
    if (!data || len == 0 || rate <= 0)
        return false;

    ALuint b = 0;
    alGenBuffers(1, &b);
    alBufferData(b, fmt, data, (ALsizei)len, (ALsizei)rate);
    if (s.buf)
        alDeleteBuffers(1, &s.buf);
    s.buf = b;
    s.fileRate = rate;
    s.rateHz = 0;
    if (s.src)
    {
        alSourcei(s.src, AL_BUFFER, (ALint)b);
        applySamplePitch(s);
    }
    return true;
}

/* set an already-decoded PCM image on a 3D slot */
bool slot3DAttachPcm(Sample3DSlot &s, S32 rate, S32 channels, S32 bits, const void *data, U32 len)
{
    ALenum fmt;
    if (channels == 1 && bits == 8) fmt = AL_FORMAT_MONO8;
    else if (channels == 2 && bits == 8) fmt = AL_FORMAT_STEREO8;
    else if (channels == 1 && bits == 16) fmt = AL_FORMAT_MONO16;
    else if (channels == 2 && bits == 16) fmt = AL_FORMAT_STEREO16;
    else return false;
    if (!data || len == 0 || rate <= 0)
        return false;

    ALuint b = 0;
    alGenBuffers(1, &b);
    alBufferData(b, fmt, data, (ALsizei)len, (ALsizei)rate);
    if (s.buf)
        alDeleteBuffers(1, &s.buf);
    s.buf = b;
    s.fileRate = rate;
    s.rateHz = 0;
    if (s.src)
    {
        alSourcei(s.src, AL_BUFFER, (ALint)b);
        alSourcef(s.src, AL_PITCH, 1.0f);
    }
    return true;
}

int allocSlot(bool used[], int n)
{
    for (int i = 0; i < n; ++i)
        if (!used[i])
            return i;
    return -1;
}

/* ---- forward declarations for the audio thread ------------------------------------ */

void pollFinished();
void pumpStream(StreamSlot &st);
void resetPass(StreamSlot &st);

/* ---- OpenAL device ------------------------------------------------------------------ */

bool openAlDevice()
{
    if (g_ctx)
        return true;
    g_dev = alcOpenDevice(NULL);
    if (!g_dev)
        return false;
    const ALCint attrs[] = {
        ALC_FREQUENCY, 44100,
        0
    };
    g_ctx = alcCreateContext(g_dev, attrs);
    if (!g_ctx)
    {
        alcCloseDevice(g_dev);
        g_dev = NULL;
        return false;
    }
    alcMakeContextCurrent(g_ctx);
    alDistanceModel(AL_NONE);
    alListener3f(AL_POSITION, 0, 0, 0);
    alListener3f(AL_VELOCITY, 0, 0, 0);

    const char *devName = alcGetString(g_dev, ALC_DEVICE_SPECIFIER);
    fprintf(stderr, "[mss32-al] OpenAL device: %s\n", devName ? devName : "?");

    g_stopThread = false;
    g_threadRunning = true;
    g_thread = std::thread([]() {
        alcMakeContextCurrent(g_ctx);
        while (!g_stopThread.load())
        {
            {
                std::lock_guard<std::recursive_mutex> lk(g_mx);
                pollFinished();
                for (int i = 0; i < MAX_STREAMS; ++i)
                    pumpStream(g_streams[i]);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    });
    return true;
}

/* ---- end-of-sample poll --------------------------------------------------------------- */

void pollFinished()
{
    for (int i = 0; i < MAX_SAMPLES; ++i)
    {
        SampleSlot &s = g_samples[i];
        if (!s.used || !s.src || !s.started)
            continue;
        /* quick API slots have no EOS callback but still own loop counts, so
         * the skip test has to allow them through until the loops run out */
        if (!s.cb && s.eosFired && s.loopsRemaining == 0)
            continue;
        ALint st = 0;
        alGetSourcei(s.src, AL_SOURCE_STATE, &st);
        if (st != AL_STOPPED)
            continue;
        if (s.loopsRemaining > 0)
        {
            --s.loopsRemaining;
            alSourceRewind(s.src);
            alSourcePlay(s.src);
            continue;
        }
        s.eosFired = true;
        s.started = false;
        if (s.cb)
            s.cb((HSAMPLE)handleOf(i));
    }
    for (int i = 0; i < MAX_3D; ++i)
    {
        Sample3DSlot &s = g_3d[i];
        if (!s.used || !s.src || !s.started || s.eosFired)
            continue;
        ALint st = 0;
        alGetSourcei(s.src, AL_SOURCE_STATE, &st);
        if (st != AL_STOPPED)
            continue;
        s.eosFired = true;
        s.started = false;
        if (s.cb)
            s.cb((H3DSAMPLE)handleOf(i));
    }
}

/* ---- streams: chunked buffer-queue pump --------------------------------------- */

/* mp3 io through the engine's file callbacks; g_mx is already held */
size_t mp3IoRead(void *buf, size_t size, void *ud)
{
    StreamSlot *st = (StreamSlot *)ud;
    if (!g_readCb || st->file == 0)
        return 0;
    return g_readCb(st->file, buf, (U32)size);
}

int mp3IoSeek(uint64_t position, void *ud)
{
    StreamSlot *st = (StreamSlot *)ud;
    if (!g_seekCb || st->file == 0)
        return -1;
    return (g_seekCb(st->file, (S32)position, 0) < 0) ? -1 : 0;
}

S32 streamFrameBytes(const StreamSlot &st)
{
    return st.channels * (st.bits / 8);
}

ALenum streamFormat(const StreamSlot &st)
{
    if (st.channels == 1)
        return st.bits == 8 ? AL_FORMAT_MONO8 : AL_FORMAT_MONO16;
    return st.bits == 8 ? AL_FORMAT_STEREO8 : AL_FORMAT_STEREO16;
}

/* decode the next chunk of the pass into pcmBytes; returns sample frames (0 = pass end) */
uint64_t decodeNextChunk(StreamSlot &st, std::vector<uint8_t> &pcmBytes)
{
    pcmBytes.clear();
    const S32 frameBytes = streamFrameBytes(st);
    if (frameBytes <= 0)
        return 0;

    if (st.kind == 1)   /* WAV: raw PCM straight through */
    {
        if (st.readPos >= st.dataLen)
            return 0;
        size_t want = STREAM_CHUNK_BYTES - (STREAM_CHUNK_BYTES % (size_t)frameBytes);
        if (want > st.dataLen - st.readPos)
            want = (size_t)(st.dataLen - st.readPos);
        if (g_seekCb)
            g_seekCb(st.file, (S32)(st.dataStart + st.readPos), 0);
        pcmBytes.resize(want);
        const U32 got = g_readCb ? g_readCb(st.file, pcmBytes.data(), (U32)want) : 0;
        if (got == 0)
            return 0;
        st.readPos += got;
        pcmBytes.resize(got);
        return got / (uint64_t)frameBytes;
    }

    /* MP3 */
    if (!st.mp3Open)
        return 0;
    const size_t wantFrames = STREAM_CHUNK_BYTES / (size_t)frameBytes;
    pcmBytes.resize(wantFrames * (size_t)frameBytes);
    const size_t gotSamples = mp3dec_ex_read(&st.mp3, (mp3d_sample_t *)pcmBytes.data(),
                                             wantFrames * (size_t)st.channels);
    if ((ssize_t)gotSamples <= 0)
        return 0;
    pcmBytes.resize(gotSamples * sizeof(mp3d_sample_t));
    return (uint64_t)gotSamples / (uint64_t)st.channels;
}

void resetPass(StreamSlot &st)
{
    st.passEof = false;
    st.playedFrames = 0;
    if (st.kind == 1)
    {
        st.readPos = 0;
        if (g_seekCb && st.file)
            g_seekCb(st.file, (S32)st.dataStart, 0);
    }
    else if (st.kind == 2 && st.mp3Open)
    {
        mp3dec_ex_seek(&st.mp3, 0);
    }
}

void unqueueAll(StreamSlot &st)
{
    ALint q = 0;
    alGetSourcei(st.src, AL_BUFFERS_QUEUED, &q);
    while (q-- > 0)
    {
        ALuint b = 0;
        alSourceUnqueueBuffers(st.src, 1, &b);
        for (StreamBuf &sb : st.buffers)
            if (sb.id == b)
                sb.queued = false;
    }
}

void pumpStream(StreamSlot &st)
{
    if (!st.used || !st.src || !st.playing || st.paused)
        return;

    /* recycle fully played buffers, counting played frames */
    ALint processed = 0;
    alGetSourcei(st.src, AL_BUFFERS_PROCESSED, &processed);
    while (processed-- > 0)
    {
        ALuint b = 0;
        alSourceUnqueueBuffers(st.src, 1, &b);
        for (StreamBuf &sb : st.buffers)
        {
            if (sb.id == b)
            {
                sb.queued = false;
                ALint size = 0;
                alGetBufferi(b, AL_SIZE, &size);
                st.playedFrames += (uint64_t)size / (uint64_t)streamFrameBytes(st);
                break;
            }
        }
    }

    int queued = 0;
    for (const StreamBuf &sb : st.buffers)
        if (sb.queued)
            ++queued;

    /* top up the queue */
    while (queued < STREAM_BUFFER_COUNT && !st.passEof)
    {
        int slot = -1;
        for (int i = 0; i < (int)st.buffers.size(); ++i)
            if (!st.buffers[i].queued && st.buffers[i].id != 0)
            {
                slot = i;
                break;
            }
        if (slot < 0)
        {
            if ((int)st.buffers.size() >= STREAM_BUFFER_COUNT)
            {
                /* all buffers are queued: wait for playback to catch up */
                break;
            }
            StreamBuf nb;
            alGenBuffers(1, &nb.id);
            st.buffers.push_back(nb);
            slot = (int)st.buffers.size() - 1;
        }

        std::vector<uint8_t> pcm;
        const uint64_t frames = decodeNextChunk(st, pcm);
        if (frames == 0)
        {
            st.passEof = true;
            break;
        }
        alBufferData(st.buffers[slot].id, streamFormat(st), pcm.data(),
                     (ALsizei)(frames * (uint64_t)streamFrameBytes(st)), (ALsizei)st.rate);
        alSourceQueueBuffers(st.src, 1, &st.buffers[slot].id);
        st.buffers[slot].queued = true;
        ++queued;
    }

    /* pass finished? */
    if (st.passEof && queued == 0)
    {
        if (st.loopsRemaining != 0)
        {
            if (st.loopsRemaining > 0)
                --st.loopsRemaining;
            resetPass(st);
        }
        else
        {
            st.playing = false;
            if (!st.cbFired)
            {
                st.cbFired = true;
                if (st.cb)
                    st.cb((HSTREAM)handleOf((int)(&st - g_streams)));
            }
        }
    }

    /* restart a source that starved while the stream stays live */
    if (st.playing && !st.paused)
    {
        ALint state = 0;
        alGetSourcei(st.src, AL_SOURCE_STATE, &state);
        ALint q2 = 0;
        alGetSourcei(st.src, AL_BUFFERS_QUEUED, &q2);
        if (state != AL_PLAYING && q2 > 0)
            alSourcePlay(st.src);
    }
}

/* ---- sample image from memory (shared by samples and quick API) ---------------------- */

bool setSampleImage(SampleSlot &s, const uint8_t *image, size_t size)
{
    AILSOUNDINFO info;
    const AILSOUNDINFO zero = {0};
    info = zero;

    /* RIFF first: raw PCM payload bytes can contain MP3-looking sync words,
     * so MP3 detection is only consulted once the WAV parse has failed */
    if (parseWavImage(image, size, info))
    {
        if (info.format == WAVE_FORMAT_IMA_ADPCM)
        {
            std::vector<int16_t> pcm;
            if (!decompressIma(info, pcm))
                return false;
            return slotAttachPcm(s, (S32)info.rate, info.channels, 16, pcm.data(),
                                 (U32)(pcm.size() * sizeof(int16_t)));
        }
        return slotAttachPcm(s, (S32)info.rate, info.channels, info.bits,
                             info.data_ptr, info.data_len);
    }

    if (!mp3dec_detect_buf(image, size < 65536 ? size : 65536))
        return false;
    mp3dec_t dec;
    mp3dec_init(&dec);
    mp3dec_file_info_t fi;
    memset(&fi, 0, sizeof(fi));
    if (mp3dec_load_buf(&dec, image, size, &fi, NULL, NULL) != 0 || !fi.samples)
        return false;
    const bool ok = slotAttachPcm(s, fi.hz, fi.channels, 16, fi.buffer,
                                  (U32)(fi.samples * (uint32_t)sizeof(mp3d_sample_t)));
    free(fi.buffer);
    return ok;
}

/* stream source state: gain + pan (streams never use AL distance geometry) */
void applyStreamGain(StreamSlot &st)
{
    if (!st.src)
        return;
    alSourcei(st.src, AL_SOURCE_RELATIVE, AL_TRUE);
    alSourcef(st.src, AL_ROLLOFF_FACTOR, 0.0f);
    alSourcef(st.src, AL_GAIN, clampF(st.volume, 0.0f, 1.0f));
    const F32 x = 2.0f * clampF(st.pan, 0.0f, 1.0f) - 1.0f;
    alSource3f(st.src, AL_POSITION, x, 0.0f, -1.0f);
}


} // anonymous namespace

/* ============================================================================
 *  Public ABI
 * ==========================================================================*/

extern "C" {

/* ---- startup / global ----------------------------------------------------- */

S32 AILCALL AIL_startup(void)
{
    std::lock_guard<std::recursive_mutex> lk(g_mx);
    return openAlDevice() ? 1 : 0;
}

void AILCALL AIL_shutdown(void)
{
    std::lock_guard<std::recursive_mutex> lk(g_mx);
    if (g_threadRunning)
    {
        g_stopThread = true;
        if (g_thread.joinable())
            g_thread.join();
        g_threadRunning = false;
    }

    for (int i = 0; i < MAX_STREAMS; ++i)
    {
        StreamSlot &st = g_streams[i];
        if (!st.used)
            continue;
        if (st.src)
        {
            alSourceStop(st.src);
            unqueueAll(st);
            for (StreamBuf &sb : st.buffers)
                if (sb.id)
                    alDeleteBuffers(1, &sb.id);
            alDeleteSources(1, &st.src);
        }
        if (st.mp3Open)
        {
            mp3dec_ex_close(&st.mp3);
            st.mp3Open = false;
        }
        if (st.file && g_closeCb)
            g_closeCb(st.file);
        st = StreamSlot();
    }
    for (int i = 0; i < MAX_SAMPLES; ++i)
    {
        SampleSlot &s = g_samples[i];
        if (!s.used)
            continue;
        if (s.src) { alSourceStop(s.src); alDeleteSources(1, &s.src); }
        if (s.buf) alDeleteBuffers(1, &s.buf);
        s = SampleSlot();
    }
    for (int i = 0; i < MAX_3D; ++i)
    {
        Sample3DSlot &s = g_3d[i];
        if (!s.used)
            continue;
        if (s.src) { alSourceStop(s.src); alDeleteSources(1, &s.src); }
        if (s.buf) alDeleteBuffers(1, &s.buf);
        s = Sample3DSlot();
    }

    if (g_ctx) { alcMakeContextCurrent(NULL); alcDestroyContext(g_ctx); g_ctx = NULL; }
    if (g_dev) { alcCloseDevice(g_dev); g_dev = NULL; }
    g_digToken = NULL;
    g_providerOpen = 0;
    g_listenerOpen = false;
}

S32 AILCALL AIL_quick_startup(S32 use_digital, S32 use_MIDI, U32 output_rate, S32 output_bits, S32 output_channels)
{
    (void)use_MIDI;
    (void)output_bits;
    (void)output_channels;
    std::lock_guard<std::recursive_mutex> lk(g_mx);
    if (use_digital <= 0)
        return 0;
    (void)output_rate;    /* every buffer carries its own rate; the device is 44100 */
    if (!openAlDevice())
        return 0;
    if (!g_digToken)
        g_digToken = (HDIGDRIVER)(uintptr_t)0x1u;    /* stable non-NULL sentinel */
    return 1;
}

void AILCALL AIL_quick_handles(HDIGDRIVER *pdig, HMDIDRIVER *pmdi, HDLSDEVICE *pdls)
{
    std::lock_guard<std::recursive_mutex> lk(g_mx);
    if (pdig) *pdig = g_ctx ? g_digToken : NULL;
    if (pmdi) *pmdi = NULL;
    if (pdls) *pdls = NULL;
}

char *AILCALL AIL_set_redist_directory(const char *dir)
{
    (void)dir;
    return NULL;
}

void AILCALL AIL_lock(void)   { g_mx.lock(); }
void AILCALL AIL_unlock(void) { g_mx.unlock(); }

S32 AILCALL AIL_get_timer_highest_delay(void)
{
    return 0;   /* no reported callback latency on this backend */
}

void AILCALL AIL_set_file_callbacks(AILFILEOPENCB opencb, AILFILECLOSECB closecb,
                                    AILFILESEEKCB seekcb, AILFILEREADCB readcb)
{
    std::lock_guard<std::recursive_mutex> lk(g_mx);
    g_openCb = opencb;
    g_closeCb = closecb;
    g_seekCb = seekcb;
    g_readCb = readcb;
}

/* ---- providers / filters ---------------------------------------------------- */

S32 AILCALL AIL_enumerate_3D_providers(HPROENUM *next, HPROVIDER *dest, char **name)
{
    std::lock_guard<std::recursive_mutex> lk(g_mx);
    if (!next || *next != HPROENUM_FIRST || !g_ctx)
    {
        if (next) *next = HPROENUM_FIRST;
        if (dest) *dest = 0;
        if (name) *name = NULL;
        return 0;
    }
    if (dest) *dest = PROVIDER_ID;
    if (name) *name = (char *)kProviderName;
    *next = 1;
    return 1;
}

S32 AILCALL AIL_open_3D_provider(HPROVIDER lib)
{
    std::lock_guard<std::recursive_mutex> lk(g_mx);
    if (lib != PROVIDER_ID || !g_ctx)
        return -1;  /* M3D_NOERR is 0: nonzero means failed */
    ++g_providerOpen;
    return 0;
}

void AILCALL AIL_close_3D_provider(HPROVIDER lib)
{
    std::lock_guard<std::recursive_mutex> lk(g_mx);
    if (lib == PROVIDER_ID && g_providerOpen > 0)
        --g_providerOpen;
}

S32 AILCALL AIL_enumerate_filters(HPROENUM *next, HPROVIDER *dest, char **name)
{
    /* no pipeline filters: initDelayFilter() walks an empty list */
    if (next) *next = HPROENUM_FIRST;
    if (dest) *dest = 0;
    if (name) *name = NULL;
    return 0;
}

void AILCALL AIL_set_3D_speaker_type(HPROVIDER lib, S32 speaker_type)
{
    (void)lib;
    (void)speaker_type;
}

HPROVIDER AILCALL AIL_set_sample_processor(HSAMPLE sample, S32 pipeline_stage, HPROVIDER provider)
{
    (void)sample;
    (void)pipeline_stage;
    (void)provider;
    return 0;
}

void AILCALL AIL_set_filter_sample_preference(HSAMPLE sample, const char *name, const void *val)
{
    (void)sample;
    (void)name;
    (void)val;
}

/* ---- 2D samples -------------------------------------------------------------- */

HSAMPLE AILCALL AIL_allocate_sample_handle(HDIGDRIVER dig)
{
    std::lock_guard<std::recursive_mutex> lk(g_mx);
    if (!dig || !g_ctx)
        return NULL;
    bool used[MAX_SAMPLES];
    for (int k = 0; k < MAX_SAMPLES; ++k)
        used[k] = g_samples[k].used;
    const int i = allocSlot(used, MAX_SAMPLES);
    if (i < 0)
        return NULL;
    SampleSlot &s = g_samples[i];
    s = SampleSlot();
    s.used = true;
    alGenSources(1, &s.src);
    applySampleGainPan(s);
    return (HSAMPLE)handleOf(i);
}

void AILCALL AIL_release_sample_handle(HSAMPLE sample)
{
    std::lock_guard<std::recursive_mutex> lk(g_mx);
    const int i = slotOf(sample);
    if (i < 0 || i >= MAX_SAMPLES || !g_samples[i].used)
        return;
    SampleSlot &s = g_samples[i];
    if (s.src) { alSourceStop(s.src); alDeleteSources(1, &s.src); }
    if (s.buf) alDeleteBuffers(1, &s.buf);
    s = SampleSlot();
}

void AILCALL AIL_init_sample(HSAMPLE sample)
{
    std::lock_guard<std::recursive_mutex> lk(g_mx);
    const int i = slotOf(sample);
    if (i < 0 || i >= MAX_SAMPLES || !g_samples[i].used)
        return;
    SampleSlot &s = g_samples[i];
    s.rateHz = 0;
    s.volume = 1.0f;
    s.pan = 0.5f;
    s.started = false;
    s.eosFired = true;
    s.loopsRemaining = 0;
    if (s.src)
    {
        alSourceStop(s.src);
        alSourceRewind(s.src);
        applySampleGainPan(s);
        applySamplePitch(s);
    }
}

S32 AILCALL AIL_set_sample_file(HSAMPLE sample, const void *file_image, S32 block)
{
    (void)block;    /* the image is fully decoded here: nothing to wait for */
    std::lock_guard<std::recursive_mutex> lk(g_mx);
    const int i = slotOf(sample);
    if (i < 0 || i >= MAX_SAMPLES || !g_samples[i].used || !file_image)
        return 0;
    return setSampleImage(g_samples[i], (const uint8_t *)file_image, 0x7FFFFFFF) ? 1 : 0;
}

void AILCALL AIL_start_sample(HSAMPLE sample)
{
    std::lock_guard<std::recursive_mutex> lk(g_mx);
    const int i = slotOf(sample);
    if (i < 0 || i >= MAX_SAMPLES || !g_samples[i].used || !g_samples[i].src)
        return;
    SampleSlot &s = g_samples[i];
    alSourceRewind(s.src);
    alSourcePlay(s.src);
    s.started = true;
    s.eosFired = false;
}

void AILCALL AIL_stop_sample(HSAMPLE sample)
{
    std::lock_guard<std::recursive_mutex> lk(g_mx);
    const int i = slotOf(sample);
    if (i < 0 || i >= MAX_SAMPLES || !g_samples[i].used || !g_samples[i].src)
        return;
    alSourcePause(g_samples[i].src);   /* Miles stop keeps the position for resume */
}

void AILCALL AIL_resume_sample(HSAMPLE sample)
{
    std::lock_guard<std::recursive_mutex> lk(g_mx);
    const int i = slotOf(sample);
    if (i < 0 || i >= MAX_SAMPLES || !g_samples[i].used || !g_samples[i].src)
        return;
    SampleSlot &s = g_samples[i];
    ALint st = 0;
    alGetSourcei(s.src, AL_SOURCE_STATE, &st);
    if (st == AL_PAUSED)
        alSourcePlay(s.src);
}

void AILCALL AIL_end_sample(HSAMPLE sample)
{
    std::lock_guard<std::recursive_mutex> lk(g_mx);
    const int i = slotOf(sample);
    if (i < 0 || i >= MAX_SAMPLES || !g_samples[i].used || !g_samples[i].src)
        return;
    SampleSlot &s = g_samples[i];
    alSourceStop(s.src);
    alSourceRewind(s.src);
    s.started = false;
    s.eosFired = true;
}

void AILCALL AIL_set_sample_volume_pan(HSAMPLE sample, F32 volume, F32 pan)
{
    std::lock_guard<std::recursive_mutex> lk(g_mx);
    const int i = slotOf(sample);
    if (i < 0 || i >= MAX_SAMPLES || !g_samples[i].used)
        return;
    SampleSlot &s = g_samples[i];
    s.volume = volume;
    s.pan = pan;
    applySampleGainPan(s);
}

void AILCALL AIL_sample_volume_pan(HSAMPLE sample, F32 *volume, F32 *pan)
{
    std::lock_guard<std::recursive_mutex> lk(g_mx);
    const int i = slotOf(sample);
    if (volume) *volume = 0;
    if (pan) *pan = 0;
    if (i < 0 || i >= MAX_SAMPLES || !g_samples[i].used)
        return;
    if (volume) *volume = g_samples[i].volume;
    if (pan) *pan = g_samples[i].pan;
}

void AILCALL AIL_set_sample_playback_rate(HSAMPLE sample, S32 playback_rate)
{
    std::lock_guard<std::recursive_mutex> lk(g_mx);
    const int i = slotOf(sample);
    if (i < 0 || i >= MAX_SAMPLES || !g_samples[i].used)
        return;
    g_samples[i].rateHz = (playback_rate > 0) ? playback_rate : 0;  /* 0 = native */
    applySamplePitch(g_samples[i]);
}

S32 AILCALL AIL_sample_playback_rate(HSAMPLE sample)
{
    std::lock_guard<std::recursive_mutex> lk(g_mx);
    const int i = slotOf(sample);
    if (i < 0 || i >= MAX_SAMPLES || !g_samples[i].used)
        return 0;
    SampleSlot &s = g_samples[i];
    if (s.rateHz > 0)
        return s.rateHz;
    return s.fileRate;   /* 0 when no file is loaded, as Miles reports it */
}

void AILCALL AIL_set_sample_user_data(HSAMPLE sample, U32 index, S32 value)
{
    std::lock_guard<std::recursive_mutex> lk(g_mx);
    const int i = slotOf(sample);
    if (i < 0 || i >= MAX_SAMPLES || !g_samples[i].used || index >= 4)
        return;
    g_samples[i].user[index] = value;
}

S32 AILCALL AIL_sample_user_data(HSAMPLE sample, U32 index)
{
    std::lock_guard<std::recursive_mutex> lk(g_mx);
    const int i = slotOf(sample);
    if (i < 0 || i >= MAX_SAMPLES || !g_samples[i].used || index >= 4)
        return 0;
    return g_samples[i].user[index];
}

AILSAMPLECB AILCALL AIL_register_EOS_callback(HSAMPLE sample, AILSAMPLECB EOS)
{
    std::lock_guard<std::recursive_mutex> lk(g_mx);
    const int i = slotOf(sample);
    if (i < 0 || i >= MAX_SAMPLES || !g_samples[i].used)
        return NULL;
    AILSAMPLECB old = g_samples[i].cb;
    g_samples[i].cb = EOS;
    return old;
}

void AILCALL AIL_get_DirectSound_info(HSAMPLE sample, AILLPDIRECTSOUND *lplpDS,
                                      AILLPDIRECTSOUNDBUFFER *lplpDSB)
{
    (void)sample;
    if (lplpDS) *lplpDS = NULL;
    if (lplpDSB) *lplpDSB = NULL;
}

/* ---- 3D samples --------------------------------------------------------------- */

H3DSAMPLE AILCALL AIL_allocate_3D_sample_handle(HPROVIDER lib)
{
    std::lock_guard<std::recursive_mutex> lk(g_mx);
    if (lib != PROVIDER_ID || g_providerOpen <= 0 || !g_ctx)
        return NULL;
    bool used[MAX_3D];
    for (int k = 0; k < MAX_3D; ++k)
        used[k] = g_3d[k].used;
    const int i = allocSlot(used, MAX_3D);
    if (i < 0)
        return NULL;
    Sample3DSlot &s = g_3d[i];
    s = Sample3DSlot();
    s.used = true;
    alGenSources(1, &s.src);
    apply3D(s);
    return (H3DSAMPLE)handleOf(i);
}

void AILCALL AIL_release_3D_sample_handle(H3DSAMPLE sample)
{
    std::lock_guard<std::recursive_mutex> lk(g_mx);
    const int i = slotOf(sample);
    if (i < 0 || i >= MAX_3D || !g_3d[i].used)
        return;
    Sample3DSlot &s = g_3d[i];
    if (s.src) { alSourceStop(s.src); alDeleteSources(1, &s.src); }
    if (s.buf) alDeleteBuffers(1, &s.buf);
    s = Sample3DSlot();
}

S32 AILCALL AIL_set_3D_sample_file(H3DSAMPLE sample, const void *file_image)
{
    std::lock_guard<std::recursive_mutex> lk(g_mx);
    const int i = slotOf(sample);
    if (i < 0 || i >= MAX_3D || !g_3d[i].used || !file_image)
        return 0;

    AILSOUNDINFO info;
    const AILSOUNDINFO zero = {0};
    info = zero;
    if (!parseWavImage((const uint8_t *)file_image, 0x7FFFFFFF, info))
        return 0;

    if (info.format == WAVE_FORMAT_IMA_ADPCM)
    {
        std::vector<int16_t> pcm;
        if (!decompressIma(info, pcm))
            return 0;
        return slot3DAttachPcm(g_3d[i], (S32)info.rate, info.channels, 16, pcm.data(),
                               (U32)(pcm.size() * sizeof(int16_t))) ? 1 : 0;
    }
    return slot3DAttachPcm(g_3d[i], (S32)info.rate, info.channels, info.bits,
                           info.data_ptr, info.data_len) ? 1 : 0;
}

void AILCALL AIL_start_3D_sample(H3DSAMPLE sample)
{
    std::lock_guard<std::recursive_mutex> lk(g_mx);
    const int i = slotOf(sample);
    if (i < 0 || i >= MAX_3D || !g_3d[i].used || !g_3d[i].src)
        return;
    Sample3DSlot &s = g_3d[i];
    apply3D(s);
    alSourceRewind(s.src);
    alSourcePlay(s.src);
    s.started = true;
    s.eosFired = false;
}

void AILCALL AIL_stop_3D_sample(H3DSAMPLE sample)
{
    std::lock_guard<std::recursive_mutex> lk(g_mx);
    const int i = slotOf(sample);
    if (i < 0 || i >= MAX_3D || !g_3d[i].used || !g_3d[i].src)
        return;
    alSourcePause(g_3d[i].src);
}

void AILCALL AIL_resume_3D_sample(H3DSAMPLE sample)
{
    std::lock_guard<std::recursive_mutex> lk(g_mx);
    const int i = slotOf(sample);
    if (i < 0 || i >= MAX_3D || !g_3d[i].used || !g_3d[i].src)
        return;
    Sample3DSlot &s = g_3d[i];
    ALint st = 0;
    alGetSourcei(s.src, AL_SOURCE_STATE, &st);
    if (st == AL_PAUSED)
        alSourcePlay(s.src);
}

void AILCALL AIL_end_3D_sample(H3DSAMPLE sample)
{
    std::lock_guard<std::recursive_mutex> lk(g_mx);
    const int i = slotOf(sample);
    if (i < 0 || i >= MAX_3D || !g_3d[i].used || !g_3d[i].src)
        return;
    Sample3DSlot &s = g_3d[i];
    alSourceStop(s.src);
    alSourceRewind(s.src);
    s.started = false;
    s.eosFired = true;
}

void AILCALL AIL_set_3D_sample_volume(H3DSAMPLE sample, F32 volume)
{
    std::lock_guard<std::recursive_mutex> lk(g_mx);
    const int i = slotOf(sample);
    if (i < 0 || i >= MAX_3D || !g_3d[i].used)
        return;
    g_3d[i].volume = volume;
    apply3D(g_3d[i]);
}

void AILCALL AIL_set_3D_sample_playback_rate(H3DSAMPLE sample, S32 playback_rate)
{
    std::lock_guard<std::recursive_mutex> lk(g_mx);
    const int i = slotOf(sample);
    if (i < 0 || i >= MAX_3D || !g_3d[i].used || !g_3d[i].src)
        return;
    Sample3DSlot &s = g_3d[i];
    s.rateHz = (playback_rate > 0) ? playback_rate : 0;
    F32 pitch = 1.0f;
    if (s.fileRate > 0 && s.rateHz > 0)
        pitch = (F32)s.rateHz / (F32)s.fileRate;
    alSourcef(s.src, AL_PITCH, clampF(pitch, 0.5f, 2.0f));
}

S32 AILCALL AIL_3D_sample_playback_rate(H3DSAMPLE sample)
{
    std::lock_guard<std::recursive_mutex> lk(g_mx);
    const int i = slotOf(sample);
    if (i < 0 || i >= MAX_3D || !g_3d[i].used)
        return 0;
    Sample3DSlot &s = g_3d[i];
    if (s.rateHz > 0)
        return s.rateHz;
    return s.fileRate;
}

void AILCALL AIL_set_3D_sample_distances(H3DSAMPLE sample, F32 max_dist, F32 min_dist)
{
    std::lock_guard<std::recursive_mutex> lk(g_mx);
    const int i = slotOf(sample);
    if (i < 0 || i >= MAX_3D || !g_3d[i].used)
        return;
    Sample3DSlot &s = g_3d[i];
    /* the engine always calls this (m_minDistance, m_maxDistance) - see
     * playSample3D and getEffectiveVolume: first argument is the inner
     * radius, the second the outer one, whatever the header names them */
    s.nearDist = (max_dist < 0.0f) ? 0.0f : max_dist;
    s.farDist = min_dist;
    if (s.farDist <= s.nearDist)
        s.farDist = s.nearDist + 1.0f;
    apply3D(s);
}

void AILCALL AIL_set_3D_sample_occlusion(H3DSAMPLE sample, F32 occlusion)
{
    std::lock_guard<std::recursive_mutex> lk(g_mx);
    const int i = slotOf(sample);
    if (i < 0 || i >= MAX_3D || !g_3d[i].used)
        return;
    g_3d[i].occlusion = occlusion;
    apply3D(g_3d[i]);
}

void AILCALL AIL_set_3D_user_data(H3DPOBJECT obj, U32 index, S32 value)
{
    std::lock_guard<std::recursive_mutex> lk(g_mx);
    const int i = slotOf(obj);
    if (i < 0 || i >= MAX_3D || !g_3d[i].used || index >= 4)
        return;
    g_3d[i].user[index] = value;
}

S32 AILCALL AIL_3D_user_data(H3DPOBJECT obj, U32 index)
{
    std::lock_guard<std::recursive_mutex> lk(g_mx);
    const int i = slotOf(obj);
    if (i < 0 || i >= MAX_3D || !g_3d[i].used || index >= 4)
        return 0;
    return g_3d[i].user[index];
}

AIL3DSAMPLECB AILCALL AIL_register_3D_EOS_callback(H3DSAMPLE sample, AIL3DSAMPLECB EOS)
{
    std::lock_guard<std::recursive_mutex> lk(g_mx);
    const int i = slotOf(sample);
    if (i < 0 || i >= MAX_3D || !g_3d[i].used)
        return NULL;
    AIL3DSAMPLECB old = g_3d[i].cb;
    g_3d[i].cb = EOS;
    return old;
}

/* ---- 3D positioning ----------------------------------------------------------- */

H3DPOBJECT AILCALL AIL_open_3D_listener(HPROVIDER lib)
{
    std::lock_guard<std::recursive_mutex> lk(g_mx);
    if (lib != PROVIDER_ID || g_providerOpen <= 0 || !g_ctx || g_listenerOpen)
        return NULL;
    g_listenerOpen = true;
    g_listenerPos[0] = g_listenerPos[1] = g_listenerPos[2] = 0.0f;
    const F32 orient[] = {0, 0, -1, 0, 1, 0};   // forward, then up
    alListenerfv(AL_ORIENTATION, orient);
    alListener3f(AL_POSITION, 0, 0, 0);
    alListener3f(AL_VELOCITY, 0, 0, 0);
    return (H3DPOBJECT)(uintptr_t)LISTENER_TOKEN;
}

void AILCALL AIL_close_3D_listener(H3DPOBJECT listener)
{
    (void)listener;
    std::lock_guard<std::recursive_mutex> lk(g_mx);
    g_listenerOpen = false;
}

void AILCALL AIL_set_3D_position(H3DPOBJECT obj, F32 X, F32 Y, F32 Z)
{
    std::lock_guard<std::recursive_mutex> lk(g_mx);
    if ((uintptr_t)obj == LISTENER_TOKEN)
    {
        g_listenerPos[0] = X;
        g_listenerPos[1] = Y;
        g_listenerPos[2] = Z;
        /* sources are listener-relative; the AL listener stays at the origin */
        for (int i = 0; i < MAX_3D; ++i)
            if (g_3d[i].used)
                apply3D(g_3d[i]);
        return;
    }
    const int i = slotOf(obj);
    if (i < 0 || i >= MAX_3D || !g_3d[i].used)
        return;
    g_3d[i].pos[0] = X;
    g_3d[i].pos[1] = Y;
    g_3d[i].pos[2] = Z;
    apply3D(g_3d[i]);
}

void AILCALL AIL_set_3D_orientation(H3DPOBJECT obj, F32 X_face, F32 Y_face, F32 Z_face,
                                    F32 X_up, F32 Y_up, F32 Z_up)
{
    std::lock_guard<std::recursive_mutex> lk(g_mx);
    if ((uintptr_t)obj != LISTENER_TOKEN)
        return;     /* source orientation only affects directivity: unused */
    F32 at[3], up[3];
    toAL(X_face, Y_face, Z_face, at);
    toAL(X_up, Y_up, Z_up, up);
    const float alen = (float)std::sqrt((double)(at[0] * at[0] + at[1] * at[1] + at[2] * at[2]));
    if (alen > 1e-6f)
    {
        at[0] /= alen; at[1] /= alen; at[2] /= alen;
    }
    else
    {
        at[0] = 0; at[1] = 0; at[2] = -1;
    }
    const float ulen = (float)std::sqrt((double)(up[0] * up[0] + up[1] * up[1] + up[2] * up[2]));
    if (ulen > 1e-6f)
    {
        up[0] /= ulen; up[1] /= ulen; up[2] /= ulen;
    }
    else
    {
        up[0] = 0; up[1] = 1; up[2] = 0;
    }
    const float orient[6] = { at[0], at[1], at[2], up[0], up[1], up[2] };
    alListenerfv(AL_ORIENTATION, orient);
}

void AILCALL AIL_set_3D_velocity_vector(H3DPOBJECT obj, F32 dX, F32 dY, F32 dZ)
{
    std::lock_guard<std::recursive_mutex> lk(g_mx);
    F32 v[3];
    toAL(dX, dY, dZ, v);
    if ((uintptr_t)obj == LISTENER_TOKEN)
    {
        alListener3f(AL_VELOCITY, v[0] * UNIT_SCALE, v[1] * UNIT_SCALE, v[2] * UNIT_SCALE);
        return;
    }
    const int i = slotOf(obj);
    if (i < 0 || i >= MAX_3D || !g_3d[i].used || !g_3d[i].src)
        return;
    alSource3f(g_3d[i].src, AL_VELOCITY,
               v[0] * UNIT_SCALE, v[1] * UNIT_SCALE, v[2] * UNIT_SCALE);
}

/* ---- streams -------------------------------------------------------------------- */

HSTREAM AILCALL AIL_open_stream(HDIGDRIVER dig, const char *filename, S32 stream_mem)
{
    (void)stream_mem;
    std::lock_guard<std::recursive_mutex> lk(g_mx);
    if (!dig || !g_ctx || !filename || !*filename || !g_openCb || !g_readCb)
        return NULL;

    int i = -1;
    for (int k = 0; k < MAX_STREAMS; ++k)
        if (!g_streams[k].used)
        {
            i = k;
            break;
        }
    if (i < 0)
        return NULL;

    U32 fh = 0;
    if (!g_openCb(filename, &fh) || fh == 0)
        return NULL;

    StreamSlot &st = g_streams[i];
    st = StreamSlot();
    st.file = fh;

    /* sniff the first bytes, then rewind */
    std::vector<uint8_t> peek(4096);
    if (g_seekCb)
        g_seekCb(fh, 0, 0);
    const U32 got = g_readCb(fh, peek.data(), (U32)peek.size());
    peek.resize(got);
    if (g_seekCb)
        g_seekCb(fh, 0, 0);

    if (got >= 12 && memcmp(peek.data(), "RIFF", 4) == 0 && memcmp(peek.data() + 8, "WAVE", 4) == 0)
        st.kind = 1;
    else if (mp3dec_detect_buf(peek.data(), peek.size()))
        st.kind = 2;

    if (st.kind == 1)
    {
        /* sequential header walk: fmt + data */
        uint8_t hdr[16];
        if (g_readCb(fh, hdr, 12) != 12)
            st.kind = 0;
        uint64_t fpos = 12;
        bool haveFmt = false, haveData = false;
        S32 rate = 0, channels = 0, bits = 0;
        while (st.kind == 1 && fpos + 8 <= 0x7FFFFFFFu)
        {
            uint8_t ch8[8];
            if (g_readCb(fh, ch8, 8) != 8)
                break;
            U32 size;
            memcpy(&size, ch8 + 4, 4);
            const uint64_t body = fpos + 8;
            if (size == 0)
            {
                fpos = body;    /* keeps advancing: body = fpos + 8 */
                continue;
            }
            if (memcmp(ch8, "fmt ", 4) == 0 && size >= 16)
            {
                if (g_readCb(fh, hdr, 16) != 16)
                    break;
                U16 formatTag, c, bl, ba;
                memcpy(&formatTag, hdr + 0, 2);
                memcpy(&c, hdr + 2, 2);
                memcpy(&rate, hdr + 4, 4);
                memcpy(&ba, hdr + 12, 2);
                memcpy(&bl, hdr + 14, 2);
                (void)ba;
                /* streamed WAV is PCM here; ADPCM files go through the
                 * sample path (AudioFileCache decompresses before Miles) */
                if (formatTag != WAVE_FORMAT_PCM)
                    break;
                channels = c;
                bits = bl;
                haveFmt = true;
            }
            else if (memcmp(ch8, "data", 4) == 0)
            {
                st.dataStart = body;
                st.dataLen = size;
                haveData = true;
                break;
            }
            if (g_seekCb)
                g_seekCb(fh, (S32)(body + size), 0);
            fpos = body + size;
        }

        if (!haveFmt || !haveData || rate <= 0 || channels <= 0 || (bits != 8 && bits != 16))
        {
            if (g_closeCb)
                g_closeCb(fh);
            return NULL;
        }
        st.rate = rate;
        st.channels = channels;
        st.bits = bits;
        st.readPos = 0;
        const uint64_t frames = st.dataLen / (uint64_t)(channels * (bits / 8));
        st.totalMs = (S32)(frames * 1000ull / (uint64_t)rate);
    }
    else if (st.kind == 2)
    {
        st.io.read = mp3IoRead;
        st.io.read_data = &st;
        st.io.seek = mp3IoSeek;
        st.io.seek_data = &st;
        memset(&st.mp3, 0, sizeof(st.mp3));
        if (mp3dec_ex_open_cb(&st.mp3, &st.io, MP3D_SEEK_TO_SAMPLE) != 0)
        {
            if (g_closeCb)
                g_closeCb(fh);
            return NULL;
        }
        st.mp3Open = true;
        st.rate = st.mp3.info.hz > 0 ? st.mp3.info.hz : 44100;
        st.channels = st.mp3.info.channels > 0 ? st.mp3.info.channels : 2;
        st.bits = 16;
        if (st.mp3.detected_samples > 0 && st.mp3.info.hz > 0)
            st.totalMs = (S32)(st.mp3.detected_samples / (uint64_t)st.channels * 1000ull /
                               (uint64_t)st.mp3.info.hz);
        mp3dec_ex_seek(&st.mp3, 0);
    }
    else
    {
        if (g_closeCb)
            g_closeCb(fh);
        return NULL;
    }

    st.used = true;
    alGenSources(1, &st.src);
    st.buffers.reserve(STREAM_BUFFER_COUNT);
    alSourcei(st.src, AL_SOURCE_RELATIVE, AL_TRUE);
    alSourcef(st.src, AL_ROLLOFF_FACTOR, 0.0f);
    return (HSTREAM)handleOf(i);
}

void AILCALL AIL_close_stream(HSTREAM stream)
{
    std::lock_guard<std::recursive_mutex> lk(g_mx);
    const int i = slotOf(stream);
    if (i < 0 || i >= MAX_STREAMS || !g_streams[i].used)
        return;
    StreamSlot &st = g_streams[i];
    if (st.src)
    {
        alSourceStop(st.src);
        unqueueAll(st);
        for (StreamBuf &sb : st.buffers)
            if (sb.id)
                alDeleteBuffers(1, &sb.id);
        alDeleteSources(1, &st.src);
    }
    if (st.mp3Open)
    {
        mp3dec_ex_close(&st.mp3);
        st.mp3Open = false;
    }
    if (st.file && g_closeCb)
        g_closeCb(st.file);
    st = StreamSlot();
}

void AILCALL AIL_start_stream(HSTREAM stream)
{
    std::lock_guard<std::recursive_mutex> lk(g_mx);
    const int i = slotOf(stream);
    if (i < 0 || i >= MAX_STREAMS || !g_streams[i].used || !g_streams[i].src)
        return;
    StreamSlot &st = g_streams[i];
    alSourceStop(st.src);
    unqueueAll(st);
    resetPass(st);
    st.playing = true;
    st.paused = false;
    st.cbFired = false;
    applyStreamGain(st);
    pumpStream(st);
}

void AILCALL AIL_pause_stream(HSTREAM stream, S32 onoff)
{
    std::lock_guard<std::recursive_mutex> lk(g_mx);
    const int i = slotOf(stream);
    if (i < 0 || i >= MAX_STREAMS || !g_streams[i].used || !g_streams[i].src)
        return;
    StreamSlot &st = g_streams[i];
    if (onoff)
    {
        alSourcePause(st.src);
        st.paused = true;
    }
    else if (st.playing)
    {
        st.paused = false;
        ALint q = 0;
        alGetSourcei(st.src, AL_BUFFERS_QUEUED, &q);
        if (q > 0)
            alSourcePlay(st.src);
    }
}

void AILCALL AIL_set_stream_loop_count(HSTREAM stream, S32 count)
{
    std::lock_guard<std::recursive_mutex> lk(g_mx);
    const int i = slotOf(stream);
    if (i < 0 || i >= MAX_STREAMS || !g_streams[i].used)
        return;
    g_streams[i].loopsRemaining = count;
}

S32 AILCALL AIL_stream_loop_count(HSTREAM stream)
{
    std::lock_guard<std::recursive_mutex> lk(g_mx);
    const int i = slotOf(stream);
    if (i < 0 || i >= MAX_STREAMS || !g_streams[i].used)
        return 0;
    return g_streams[i].loopsRemaining;
}

void AILCALL AIL_set_stream_volume_pan(HSTREAM stream, F32 volume, F32 pan)
{
    std::lock_guard<std::recursive_mutex> lk(g_mx);
    const int i = slotOf(stream);
    if (i < 0 || i >= MAX_STREAMS || !g_streams[i].used)
        return;
    g_streams[i].volume = volume;
    g_streams[i].pan = pan;
    applyStreamGain(g_streams[i]);
}

void AILCALL AIL_stream_volume_pan(HSTREAM stream, F32 *volume, F32 *pan)
{
    std::lock_guard<std::recursive_mutex> lk(g_mx);
    const int i = slotOf(stream);
    if (volume) *volume = 0;
    if (pan) *pan = 0;
    if (i < 0 || i >= MAX_STREAMS || !g_streams[i].used)
        return;
    if (volume) *volume = g_streams[i].volume;
    if (pan) *pan = g_streams[i].pan;
}

void AILCALL AIL_stream_ms_position(HSTREAM stream, S32 *total_milliseconds, S32 *current_milliseconds)
{
    std::lock_guard<std::recursive_mutex> lk(g_mx);
    const int i = slotOf(stream);
    if (total_milliseconds) *total_milliseconds = 0;
    if (current_milliseconds) *current_milliseconds = 0;
    if (i < 0 || i >= MAX_STREAMS || !g_streams[i].used)
        return;
    const StreamSlot &st = g_streams[i];
    if (total_milliseconds) *total_milliseconds = st.totalMs;
    if (current_milliseconds && st.rate > 0)
        *current_milliseconds = (S32)(st.playedFrames * 1000ull / (uint64_t)st.rate);
}

AILSTREAMCB AILCALL AIL_register_stream_callback(HSTREAM stream, AILSTREAMCB callback)
{
    std::lock_guard<std::recursive_mutex> lk(g_mx);
    const int i = slotOf(stream);
    if (i < 0 || i >= MAX_STREAMS || !g_streams[i].used)
        return NULL;
    AILSTREAMCB old = g_streams[i].cb;
    g_streams[i].cb = callback;
    return old;
}

/* ---- quick API (mission briefing audio) ------------------------------------------ */

HAUDIO AILCALL AIL_quick_load_and_play(const char *filename, U32 loop_count, S32 wait_request)
{
    (void)wait_request;
    std::lock_guard<std::recursive_mutex> lk(g_mx);
    if (!g_ctx || !filename || !*filename)
        return NULL;

    std::vector<uint8_t> image;
    if (g_openCb && g_readCb)
    {
        U32 fh = 0;
        if (!g_openCb(filename, &fh) || fh == 0)
            return NULL;
        uint8_t chunk[65536];
        for (;;)
        {
            const U32 got = g_readCb(fh, chunk, sizeof(chunk));
            if (got == 0)
                break;
            image.insert(image.end(), chunk, chunk + got);
            if (got < sizeof(chunk))
                break;
        }
        if (g_closeCb)
            g_closeCb(fh);
    }
    else
    {
        FILE *fp = fopen(filename, "rb");
        if (!fp)
            return NULL;
        uint8_t chunk[65536];
        size_t got;
        while ((got = fread(chunk, 1, sizeof(chunk), fp)) > 0)
            image.insert(image.end(), chunk, chunk + got);
        fclose(fp);
    }
    if (image.empty())
        return NULL;

    bool used[MAX_SAMPLES];
    for (int k = 0; k < MAX_SAMPLES; ++k)
        used[k] = g_samples[k].used;
    const int si = allocSlot(used, MAX_SAMPLES);
    if (si < 0)
        return NULL;
    SampleSlot &s = g_samples[si];
    s = SampleSlot();
    s.used = true;
    alGenSources(1, &s.src);

    if (!setSampleImage(s, image.data(), image.size()) || !s.buf)
    {
        if (s.src)
            alDeleteSources(1, &s.src);
        s = SampleSlot();
        return NULL;
    }

    /* Miles quick semantics: loop_count extra plays, 0xFFFFFFFF = forever */
    s.loopsRemaining = (loop_count == 0xFFFFFFFFu) ? 0x7FFFFFFFu : loop_count;
    applySampleGainPan(s);
    alSourceRewind(s.src);
    alSourcePlay(s.src);
    s.started = true;
    s.eosFired = true;   /* no EOS callback for HAUDIO handles */
    return (HAUDIO)handleOf(si);
}

void AILCALL AIL_quick_unload(HAUDIO audio)
{
    AIL_release_sample_handle((HSAMPLE)audio);
}

void AILCALL AIL_quick_set_volume(HAUDIO audio, F32 volume, F32 extravol)
{
    /* the engine documents extravol as the pan position ("0.5 is the middle") */
    AIL_set_sample_volume_pan((HSAMPLE)audio, volume, extravol);
}

/* ---- file format helpers --------------------------------------------------------- */

S32 AILCALL AIL_WAV_info(const void *data, AILSOUNDINFO *info)
{
    std::lock_guard<std::recursive_mutex> lk(g_mx);
    const AILSOUNDINFO zero = {0};
    if (info)
        *info = zero;
    if (!data)
        return 0;
    AILSOUNDINFO parsed = zero;
    if (!parseWavImage((const uint8_t *)data, 0x7FFFFFFF, parsed))
        return 0;
    if (info)
        *info = parsed;
    return 1;
}

S32 AILCALL AIL_decompress_ADPCM(const AILSOUNDINFO *info, void **outdata, U32 *outsize)
{
    std::lock_guard<std::recursive_mutex> lk(g_mx);
    if (outdata)
        *outdata = NULL;
    if (outsize)
        *outsize = 0;
    if (!info)
        return 0;
    if (info->format == WAVE_FORMAT_PCM)
    {
        /* nothing to decompress: hand back a copy wrapped as a WAV file */
        const U32 total = 44 + info->data_len;
        uint8_t *buf = (uint8_t *)malloc(total);
        if (!buf)
            return 0;
        buildWavHeader(buf, WAVE_FORMAT_PCM, (U32)info->channels, info->rate,
                       (U32)info->bits, info->data_len);
        memcpy(buf + 44, info->data_ptr, info->data_len);
        if (outdata)
            *outdata = buf;
        else
            free(buf);
        if (outsize)
            *outsize = total;
        return 1;
    }
    if (info->format != WAVE_FORMAT_IMA_ADPCM)
        return 0;

    std::vector<int16_t> pcm;
    if (!decompressIma(*info, pcm))
        return 0;
    const U32 pcmBytes = (U32)(pcm.size() * sizeof(int16_t));
    const U32 total = 44 + pcmBytes;
    uint8_t *buf = (uint8_t *)malloc(total);
    if (!buf)
        return 0;
    buildWavHeader(buf, WAVE_FORMAT_PCM, (U32)info->channels, info->rate, 16, pcmBytes);
    memcpy(buf + 44, pcm.data(), pcmBytes);
    if (outdata)
        *outdata = buf;
    else
        free(buf);
    if (outsize)
        *outsize = total;
    return 1;
}

void AILCALL AIL_mem_free_lock(void *ptr)
{
    free(ptr);
}

} // extern "C"

#endif /* __linux__ */
