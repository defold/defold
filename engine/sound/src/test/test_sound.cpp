// Copyright 2020-2025 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include <stdlib.h>
#include <string.h>
#include <set>

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include <dlib/array.h>
#include <dlib/dstrings.h>
#include <dlib/hash.h>
#include <dlib/memory.h>
#include <dlib/message.h>
#include <dlib/log.h>
#include <dlib/time.h>
#include <dlib/math.h>
#include "../sound.h"
#include "../sound_private.h"
#include "../sound_codec.h"
#include "../sound_pfb.h"
#include "../stb_vorbis/stb_vorbis.h"

#include "test/mono_tone_2000_22050_5512.wav.embed.h"
#include "test/mono_tone_2000_22050_44100.wav.embed.h"
#include "test/mono_tone_2000_32000_8000.wav.embed.h"
#include "test/mono_tone_2000_32000_64000.wav.embed.h"
#include "test/mono_tone_2000_44000_11000.wav.embed.h"
#include "test/mono_tone_2000_44000_88000.wav.embed.h"
#include "test/mono_tone_2000_44100_11025.wav.embed.h"
#include "test/mono_tone_2000_44100_88200.wav.embed.h"
#include "test/mono_tone_440_22050_5512.wav.embed.h"
#include "test/mono_tone_440_32000_8000.wav.embed.h"
#include "test/mono_tone_440_44000_11000.wav.embed.h"
#include "test/mono_tone_440_44000_88000.wav.embed.h"
#include "test/mono_tone_440_44100_11025.wav.embed.h"
#include "test/mono_tone_440_48000_12000.wav.embed.h"
#include "test/mono_tone_440_44100_88200.wav.embed.h"
#include "test/mono_tone_2000_48000_12000.wav.embed.h"
#include "test/stereo_tone_2000_22050_5512.wav.embed.h"
#include "test/stereo_tone_2000_22050_44100.wav.embed.h"
#include "test/stereo_tone_2000_32000_8000.wav.embed.h"
#include "test/stereo_tone_2000_32000_64000.wav.embed.h"
#include "test/stereo_tone_2000_44000_11000.wav.embed.h"
#include "test/stereo_tone_2000_44000_88000.wav.embed.h"
#include "test/stereo_tone_2000_44100_11025.wav.embed.h"
#include "test/stereo_tone_2000_44100_88200.wav.embed.h"
#include "test/stereo_tone_440_22050_5512.wav.embed.h"
#include "test/stereo_tone_440_22050_44100.wav.embed.h"
#include "test/stereo_tone_440_32000_8000.wav.embed.h"
#include "test/stereo_tone_440_32000_64000.wav.embed.h"
#include "test/stereo_tone_440_44000_11000.wav.embed.h"
#include "test/stereo_tone_440_44000_88000.wav.embed.h"
#include "test/stereo_tone_440_44100_11025.wav.embed.h"
#include "test/stereo_tone_440_44100_88200.wav.embed.h"
#include "test/stereo_tone_440_48000_12000.wav.embed.h"
#include "test/stereo_tone_2000_48000_12000.wav.embed.h"

#include "test/mono_tone_440_22050_44100.wav.embed.h"
#include "test/mono_tone_440_32000_64000.wav.embed.h"
#include "test/mono_toneramp_440_32000_64000.wav.embed.h"

#include "test/mono_resample_framecount_16000.ogg.embed.h"
#include "test/mono_resample_framecount_16000.opus.embed.h"
#include "test/mono_resample_framecount_16000_adpcm.wav.embed.h"

#include "test/mono_dc_44100_88200.wav.embed.h"

#include "test/def2938.wav.embed.h"

extern unsigned char CLICK_TRACK_OGG[];
extern uint32_t CLICK_TRACK_OGG_SIZE;
extern unsigned char CLICK_TRACK_OPUS[];
extern uint32_t CLICK_TRACK_OPUS_SIZE;
extern unsigned char DRUMLOOP_WAV[];
extern uint32_t DRUMLOOP_WAV_SIZE;
extern unsigned char ONEFOOTSTEP_WAV[];
extern uint32_t ONEFOOTSTEP_WAV_SIZE;
extern unsigned char LAYER_GUITAR_A_OGG[];
extern uint32_t LAYER_GUITAR_A_OGG_SIZE;
extern unsigned char OSC2_SIN_440HZ_WAV[];
extern uint32_t OSC2_SIN_440HZ_WAV_SIZE;
extern unsigned char DOOR_OPENING_WAV[];
extern uint32_t DOOR_OPENING_WAV_SIZE;
extern unsigned char TONE_MONO_22050_OGG[];
extern uint32_t TONE_MONO_22050_OGG_SIZE;
extern unsigned char BOOSTER_ON_SFX_WAV[];
extern uint32_t BOOSTER_ON_SFX_WAV_SIZE;
extern unsigned char MONO_RESAMPLE_FRAMECOUNT_16000_OGG[];
extern uint32_t MONO_RESAMPLE_FRAMECOUNT_16000_OGG_SIZE;
extern unsigned char MONO_RESAMPLE_FRAMECOUNT_16000_OPUS[];
extern uint32_t MONO_RESAMPLE_FRAMECOUNT_16000_OPUS_SIZE;
extern unsigned char MUSIC_OPUS[];
extern uint32_t MUSIC_OPUS_SIZE;
extern unsigned char AMBIENCE_OPUS[];
extern uint32_t AMBIENCE_OPUS_SIZE;
extern unsigned char MONO_RESAMPLE_FRAMECOUNT_16000_ADPCM_WAV[];
extern uint32_t MONO_RESAMPLE_FRAMECOUNT_16000_ADPCM_WAV_SIZE;
extern unsigned char MUSIC_ADPCM_WAV[];
extern uint32_t MUSIC_ADPCM_WAV_SIZE;
extern unsigned char AMBIENCE_ADPCM_WAV[];
extern uint32_t AMBIENCE_ADPCM_WAV_SIZE;

struct TestParams
{
    typedef dmSound::SoundDataType SoundDataType;

    const char* m_DeviceName;
    void*       m_Sound;
    SoundDataType m_Type;
    uint32_t    m_SoundSize;
    uint32_t    m_FrameCount;
    uint32_t    m_Channels;
    uint32_t    m_ToneRate;
    uint32_t    m_MixRate;
    uint32_t    m_BufferFrameCount; // For the sound device
    float       m_Pan;
    float       m_Speed;
    uint8_t     m_Loopcount;

    TestParams(const char* device_name, void* sound, uint32_t sound_size, SoundDataType type,
                uint32_t tone_rate, uint32_t mix_rate, uint32_t frame_count, uint32_t buffer_frame_count, uint32_t channels)
    : m_Pan(0.0f)
    , m_Speed(1.0f)
    , m_Loopcount(0)
    {
        m_DeviceName = device_name;
        m_Sound = sound;
        m_SoundSize = sound_size;
        m_Type = type;
        m_FrameCount = frame_count;
        m_Channels = channels;
        m_ToneRate = tone_rate;
        m_MixRate = mix_rate;
        m_BufferFrameCount = buffer_frame_count;
    }

    TestParams(const char* device_name, void* sound, uint32_t sound_size, SoundDataType type, uint32_t tone_rate, uint32_t mix_rate,
                uint32_t frame_count, uint32_t buffer_frame_count, float pan, float speed)
    : m_Pan(pan)
    , m_Speed(speed)
    {
        m_DeviceName = device_name;
        m_Sound = sound;
        m_SoundSize = sound_size;
        m_Type = type;
        m_FrameCount = frame_count;
        m_Channels = 0;
        m_ToneRate = tone_rate;
        m_MixRate = mix_rate;
        m_BufferFrameCount = buffer_frame_count;
    }

    TestParams(const char* device_name, void* sound, uint32_t sound_size, SoundDataType type, uint32_t tone_rate, uint32_t mix_rate,
                uint32_t frame_count, uint32_t buffer_frame_count, float pan, float speed, uint32_t loopcount)
    : m_Pan(pan)
    , m_Speed(speed)
    , m_Loopcount(loopcount)
    {
        m_DeviceName = device_name;
        m_Sound = sound;
        m_SoundSize = sound_size;
        m_Type = type;
        m_FrameCount = frame_count;
        m_Channels = 0;
        m_ToneRate = tone_rate;
        m_MixRate = mix_rate;
        m_BufferFrameCount = buffer_frame_count;
    }

    float LengthInSeconds()
    {
        if (!m_MixRate)
            return 0.0f;

        return (m_FrameCount / (float)m_MixRate) * m_Speed;
    }
};

struct TestParams2
{
    typedef dmSound::SoundDataType SoundDataType;

    const char* m_DeviceName;

    void*       m_Sound1;
    uint32_t    m_SoundSize1;
    SoundDataType m_Type1;
    uint32_t    m_ToneRate1;
    uint32_t    m_MixRate1;
    uint32_t    m_FrameCount1;
    float       m_Gain1;
    bool        m_Ramp1;

    void*       m_Sound2;
    uint32_t    m_SoundSize2;
    SoundDataType m_Type2;
    uint32_t    m_ToneRate2;
    uint32_t    m_MixRate2;
    uint32_t    m_FrameCount2;
    float       m_Gain2;
    bool        m_Ramp2;

    uint32_t    m_BufferFrameCount; // For the sound device
    bool        m_UseThread;

    TestParams2(const char* device_name,
                void* sound1, uint32_t sound_size1, SoundDataType type1, uint32_t tone_rate1, uint32_t mix_rate1, uint32_t frame_count1, float gain1, bool ramp1,
                void* sound2, uint32_t sound_size2, SoundDataType type2, uint32_t tone_rate2, uint32_t mix_rate2, uint32_t frame_count2, float gain2, bool ramp2,
                uint32_t buffer_frame_count, bool use_thread)
    {
        m_DeviceName = device_name;

        m_Sound1 = sound1;
        m_SoundSize1 = sound_size1;
        m_Type1 = type1;
        m_FrameCount1 = frame_count1;
        m_ToneRate1 = tone_rate1;
        m_MixRate1 = mix_rate1;
        m_Gain1 = gain1;
        m_Ramp1 = ramp1;

        m_Sound2 = sound2;
        m_SoundSize2 = sound_size2;
        m_Type2 = type2;
        m_FrameCount2 = frame_count2;
        m_ToneRate2 = tone_rate2;
        m_MixRate2 = mix_rate2;
        m_Gain2 = gain2;
        m_Ramp2 = ramp2;

        m_BufferFrameCount = buffer_frame_count;
        m_UseThread = use_thread;
    }
};


#define MAX_BUFFERS 32
#define MAX_SOURCES 16

class dmSoundTest : public jc_test_params_class<TestParams>
{
public:
    const char* m_DeviceName;

    dmSoundTest() {
        m_DeviceName = GetParam().m_DeviceName;
    }

    void SetUp() override
    {
        dmSound::InitializeParams params;
        params.m_MaxBuffers = MAX_BUFFERS;
        params.m_MaxSources = MAX_SOURCES;
        params.m_OutputDevice = m_DeviceName;
        params.m_FrameCount = GetParam().m_BufferFrameCount;
        params.m_UseThread = false;

        dmSound::Result r = dmSound::Initialize(0, &params);
        ASSERT_EQ(dmSound::RESULT_OK, r);
    }

    void TearDown() override
    {
        dmTime::Sleep(10000); // waiting for sounds to finish playing (to make it less choppy)
        dmSound::Result r = dmSound::Finalize();
        ASSERT_EQ(dmSound::RESULT_OK, r);
    }
};

class dmSoundTest2 : public jc_test_params_class<TestParams2>
{
public:
    const char* m_DeviceName;

    dmSoundTest2() {
        m_DeviceName = GetParam().m_DeviceName;
    }

    void SetUp() override
    {
        dmSound::InitializeParams params;
        params.m_MaxBuffers = MAX_BUFFERS;
        params.m_MaxSources = MAX_SOURCES;
        params.m_OutputDevice = m_DeviceName;
        params.m_FrameCount = GetParam().m_BufferFrameCount;
        params.m_UseThread = GetParam().m_UseThread;

        dmSound::Result r = dmSound::Initialize(0, &params);
        ASSERT_EQ(dmSound::RESULT_OK, r);
    }

    void TearDown() override
    {
        dmSound::Result r = dmSound::Finalize();
        ASSERT_EQ(dmSound::RESULT_OK, r);
    }
};

class dmSoundVerifyTest : public dmSoundTest
{
};

class dmSoundVerifyWavTest : public dmSoundTest
{
};

class dmSoundVerifyOggTest : public dmSoundTest
{
};

class dmSoundVerifyOpusTest : public dmSoundTest
{
};

class dmSoundVerifyAdpcmTest : public dmSoundTest
{
};

class dmSoundTestPlayTest : public dmSoundTest
{
};

class dmSoundTestPlaySpeedTest : public dmSoundTest
{
};

class dmSoundTestSpeedTest : public dmSoundTest
{
};

class dmSoundTestGroupRampTest : public dmSoundTest
{
};

class dmSoundTestLoopingTest :  public dmSoundTest
{
};

class dmSoundMixerTest : public dmSoundTest2
{
};

// Some arbitrary process "time" for loopback-device buffers
#define LOOPBACK_DEVICE_PROCESS_TIME (4)

struct LoopbackBuffer
{
    dmArray<int16_t> m_Buffer;
    uint64_t m_Queued;

    LoopbackBuffer(uint32_t capacity, uint64_t queued) {
        m_Buffer.SetCapacity(capacity * 2);
        m_Queued = queued;
    }
};

struct LoopbackDevice
{
    dmArray<LoopbackBuffer*> m_Buffers;
    dmArray<int16_t> m_AllOutput;
    uint32_t         m_TotalBuffersQueued;
    int              m_Time; // read cursor (frames)
    int              m_QueueTime; // write cursor (frames)
    int              m_NumWrites;
    uint32_t         m_DeviceFrameCount;
};


LoopbackDevice *g_LoopbackDevice = 0;

static dmSound::Result DeviceLoopbackOpen(const dmSound::OpenDeviceParams* params, dmSound::HDevice* device)
{
    LoopbackDevice* d = new LoopbackDevice;

    d->m_DeviceFrameCount = params->m_FrameCount;

    // to avoid making big spammy requests and getting huge chunks all at the same time
    // set up the buffers as if we have been playing continuously already. first call
    // will only ask for one buffer and there will be no bursts
    //
    // 0 1 2 3 4 5
    //             ^
    //             +- time & queue time starts here
    //
    d->m_Buffers.SetCapacity(params->m_BufferCount);
    for (uint32_t i = 0; i < params->m_BufferCount; ++i) {
        d->m_Buffers.Push(new LoopbackBuffer(d->m_DeviceFrameCount, i));
    }

    d->m_TotalBuffersQueued = 0;
    d->m_Time = params->m_BufferCount;
    d->m_QueueTime = params->m_BufferCount;
    d->m_NumWrites = 0;

    *device = d;
    g_LoopbackDevice = d;
    return dmSound::RESULT_OK;
}

static void DeviceLoopbackClose(dmSound::HDevice device)
{
    LoopbackDevice* d = (LoopbackDevice*) device;
    for (uint32_t i = 0; i < d->m_Buffers.Size(); ++i) {
        delete d->m_Buffers[i];
    }

    delete (LoopbackDevice*) device;
    g_LoopbackDevice = 0;
}

static dmSound::Result DeviceLoopbackQueue(dmSound::HDevice device, const void* _samples, uint32_t sample_count)
{
    const int16_t* samples = (const int16_t*)_samples;

    LoopbackDevice* loopback = (LoopbackDevice*) device;
    loopback->m_NumWrites++;

    loopback->m_TotalBuffersQueued++;
    if (loopback->m_AllOutput.Remaining() < sample_count * 2) {
        loopback->m_AllOutput.OffsetCapacity(sample_count * 2);
    }
    loopback->m_AllOutput.PushArray(samples, sample_count * 2);

    LoopbackBuffer* b = 0;
    for (uint32_t i = 0; i < loopback->m_Buffers.Size(); ++i) {
        if ((uint64_t)loopback->m_Time < loopback->m_Buffers[i]->m_Queued)
            continue;
        if (loopback->m_Time - loopback->m_Buffers[i]->m_Queued > LOOPBACK_DEVICE_PROCESS_TIME) {
            b = loopback->m_Buffers[i];
            break;
        }
    }

    loopback->m_QueueTime += LOOPBACK_DEVICE_PROCESS_TIME;

    b->m_Queued = loopback->m_QueueTime;
    b->m_Buffer.SetSize(0);
    b->m_Buffer.PushArray(samples, sample_count * 2);

    return dmSound::RESULT_OK;
}

static uint32_t DeviceLoopbackFreeBufferSlots(dmSound::HDevice device)
{
    LoopbackDevice* loopback = (LoopbackDevice*) device;

    uint32_t n = 0;
    for (uint32_t i = 0; i < loopback->m_Buffers.Size(); ++i) {
        if ((uint64_t)loopback->m_Time < loopback->m_Buffers[i]->m_Queued)
            continue;
        uint64_t diff = loopback->m_Time - loopback->m_Buffers[i]->m_Queued;
        if (diff > LOOPBACK_DEVICE_PROCESS_TIME) {
            ++n;
        }
    }

    loopback->m_Time++;
    return n;
}

static void DeviceLoopbackDeviceInfo(dmSound::HDevice device, dmSound::DeviceInfo* info)
{
    LoopbackDevice* loopback = (LoopbackDevice*) device;
    info->m_MixRate = 44100;
    info->m_FrameCount = loopback->m_DeviceFrameCount;
}

static void DeviceLoopbackRestart(dmSound::HDevice device)
{

}

static void DeviceLoopbackStop(dmSound::HDevice device)
{

}

#if !defined(GITHUB_CI) || (defined(GITHUB_CI) && !defined(WIN32))
TEST_P(dmSoundTestLoopingTest, Loopcount)
{
    TestParams params = GetParam();
    dmSound::Result r;
    dmSound::HSoundData sd = 0;
    dmSound::NewSoundData(params.m_Sound, params.m_SoundSize, params.m_Type, &sd, 1234);

    printf("tone: %d, rate: %d, frames: %d\n", params.m_ToneRate, params.m_MixRate, params.m_FrameCount);

    dmSound::HSoundInstance instance = 0;
    r = dmSound::NewSoundInstance(sd, &instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    ASSERT_NE((dmSound::HSoundInstance) 0, instance);

    int8_t loopcount = 5;
    r = dmSound::SetLooping(instance, 1, loopcount);
    ASSERT_EQ(dmSound::RESULT_OK, r);


    r = dmSound::Play(instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    printf("Playing: %d\n", dmSound::IsPlaying(instance));

    do {
        r = dmSound::Update();
    } while (dmSound::IsPlaying(instance));
    r = dmSound::DeleteSoundInstance(instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    uint32_t sound_frames = params.m_FrameCount * (loopcount + 1);      // +1 -> note that the count specifies how often we execute the loop point. The last loop will result in one more playback!
    uint32_t blkend_frame = g_LoopbackDevice->m_DeviceFrameCount * g_LoopbackDevice->m_NumWrites;
    uint32_t blkbeg_frame = blkend_frame - g_LoopbackDevice->m_DeviceFrameCount;
    ASSERT_TRUE(blkbeg_frame <= sound_frames && sound_frames <= blkend_frame);

    r = dmSound::DeleteSoundData(sd);
    ASSERT_EQ(dmSound::RESULT_OK, r);
}

const TestParams params_looping_test[] = {
    TestParams("loopback",
            MONO_TONE_440_44100_88200_WAV,
            MONO_TONE_440_44100_88200_WAV_SIZE,
            dmSound::SOUND_DATA_TYPE_WAV,
            440,
            44100,
            88200,
            2048,
            0.0f,
            2.0f)
};
INSTANTIATE_TEST_CASE_P(dmSoundTestLoopingTest, dmSoundTestLoopingTest, jc_test_values_in(params_looping_test));
#endif

// gain to scale conversion
static float GainToScale(float gain)
{
    float scale;
    dmSound::GetScaleFromGain(gain, &scale);
    return scale;
}

// Generate sine wave per given parameters and resample it as needed to mimic runtimes signal path for generated test waves
static double GenAndMixTone(uint64_t pos, float tone_frq, float sample_rate, float mix_rate, int num_frames, bool ramp_active, float scale)
{
    static_assert(_pfb_num_phases == 1 << 11, "PFB header does not readily offer this constant - update needed (see below)!");

    int index = (int)(pos >> dmSound::RESAMPLE_FRACTION_BITS);

    double a;
    // only generate data inside samples range (assumes no looping)
    if (index < num_frames) {
        if (mix_rate != sample_rate) {
            int frac = (int)(pos >> (dmSound::RESAMPLE_FRACTION_BITS - 11)) & (_pfb_num_phases - 1);
            int32_t ti = _pfb_num_taps * frac;
            const int32_t js = -(_pfb_num_taps / 2 - 1);
            const int32_t je = _pfb_num_taps / 2;
            // Polyphase FIR
            a = 0.0;
            for (int32_t j = js; j <= je; j++, ti++) {
                // add only if we are past the start (assumes zeroes in history buffer)
                int32_t idx = index + j;
                if (idx >= 0) {
                    // Clamp index at end of sample data (assumes samples at the end get repeated as needed for resampling)
                    idx = dmMath::Min(idx, num_frames - 1);
                    float ramp = ramp_active ? ((num_frames - 1) - idx) / (float) num_frames : 1.0f;
                    double t = ramp * 0.8 * 32768.0 * sin((idx * 2.0 * M_PI * tone_frq) / sample_rate);
                    a += t * _pfb[ti];
                }
            }
            a *= scale;
        }
        else {
            // 1:1
            float ramp = ramp_active ? ((num_frames - 1) - index) / (float) num_frames : 1.0f;
            a = scale * ramp * 0.8 * 32768.0 * sin((index * 2.0 * M_PI * tone_frq) / sample_rate);
        }
    }
    else {
        a = 0.0;
    }
    return a;
}

#if !defined(GITHUB_CI) || (defined(GITHUB_CI) && !defined(WIN32))
TEST_P(dmSoundVerifyTest, Mix)
{
    TestParams params = GetParam();
    dmSound::Result r;
    dmSound::HSoundData sd = 0;
    dmSound::NewSoundData(params.m_Sound, params.m_SoundSize, params.m_Type, &sd, 1234);

    printf("tone: %d, rate: %d, frames: %d\n", params.m_ToneRate, params.m_MixRate, params.m_FrameCount);

    dmSound::HSoundInstance instance = 0;
    r = dmSound::NewSoundInstance(sd, &instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    ASSERT_NE((dmSound::HSoundInstance) 0, instance);

    r = dmSound::SetParameter(instance, dmSound::PARAMETER_PAN, dmVMath::Vector4(params.m_Pan,0,0,0));
    ASSERT_EQ(dmSound::RESULT_OK, r);

    r = dmSound::Play(instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    printf("Playing: %d\n", dmSound::IsPlaying(instance));

    do {
        r = dmSound::Update();
    } while (dmSound::IsPlaying(instance));
    r = dmSound::DeleteSoundInstance(instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    const uint32_t frame_count = params.m_FrameCount;
    const float mix_rate = params.m_MixRate;

    const double f = 44100.0;
    const int n = (int)(frame_count * (f / mix_rate));
    const double level = 1.0f;

    ASSERT_GE(g_LoopbackDevice->m_AllOutput.Size(), n * 2U);



    // We need to check that the panning works as intended.
    // Instead of checking the panning cintinuously, we check the -1 (left), 1 (right) and 0 (center)
    // Note: currently center panning introduces ~3dB loss of signal (see dmSound::GetPanScale())

    // map [-1,1] to [0,1] for use with
    float pan_01 = dmMath::Max(-1.0f, dmMath::Min(1.0f, params.m_Pan));
    pan_01 = (pan_01 + 1.0f) * 0.5f;

    float ls, rs;
    dmSound::GetPanScale(pan_01, &ls, &rs);
    uint64_t delta = (uint64_t)(mix_rate * (1UL << dmSound::RESAMPLE_FRACTION_BITS) / f);
    uint64_t pos = 0;
    for (int32_t i = 0; i < n - 1; i++) {

        double a = GenAndMixTone(pos, params.m_ToneRate, mix_rate, f, frame_count, false, level);
        pos += delta;

        int16_t as = (int16_t)dmMath::Clamp(a, -32768.0, 32767.0);
        int16_t aleft = as * ls;
        int16_t aright = as * rs;

        ASSERT_NEAR(g_LoopbackDevice->m_AllOutput[2 * i], aleft, 27);
        ASSERT_NEAR(g_LoopbackDevice->m_AllOutput[2 * i + 1], aright, 27);
    }

    ASSERT_EQ(0u, g_LoopbackDevice->m_AllOutput.Size() % 2);
    for (uint32_t i = 2 * n; i < g_LoopbackDevice->m_AllOutput.Size() / 2; ++i) {
        ASSERT_EQ(0, g_LoopbackDevice->m_AllOutput[2 * i]);
        ASSERT_EQ(0, g_LoopbackDevice->m_AllOutput[2 * i + 1]);
    }

    float rms_left, rms_right;
    dmSound::GetGroupRMS(dmHashString64("master"), params.m_BufferFrameCount / f, &rms_left, &rms_right);
    dmSound::GetGroupRMS(dmHashString64("master"), params.m_BufferFrameCount / f, &rms_left, &rms_right);
    // Theoretical RMS for a sin-function with amplitude a is a / sqrt(2)
    ASSERT_NEAR(0.8f / sqrtf(2.0f) * level * ls, rms_left, 0.02f);
    ASSERT_NEAR(0.8f / sqrtf(2.0f) * level * rs, rms_right, 0.02f);

    float peak_left, peak_right;
    dmSound::GetGroupPeak(dmHashString64("master"), params.m_BufferFrameCount / f, &peak_left, &peak_right);
    dmSound::GetGroupPeak(dmHashString64("master"), params.m_BufferFrameCount / f, &peak_left, &peak_right);
    ASSERT_NEAR(0.8f* level * ls, peak_left, 0.01f);
    ASSERT_NEAR(0.8f* level * rs, peak_right, 0.01f);

    int expected_queued = (frame_count * (int)f) / ((int) mix_rate * params.m_BufferFrameCount)
                            + dmMath::Min(1U, (frame_count * (int)f) % ((int) mix_rate * params.m_BufferFrameCount));
    ASSERT_EQ(g_LoopbackDevice->m_TotalBuffersQueued, (uint32_t)expected_queued);

    r = dmSound::DeleteSoundData(sd);
    ASSERT_EQ(dmSound::RESULT_OK, r);
}
#endif

TEST_P(dmSoundVerifyTest, EarlyBailOnNoSoundInstances)
{
    ASSERT_EQ(dmSound::RESULT_NOTHING_TO_PLAY, dmSound::Update());
}

TEST_P(dmSoundVerifyTest, NoEarlyBailOnSoundInstances)
{
    TestParams params = GetParam();
    dmSound::Result r;
    dmSound::HSoundData sd = 0;
    dmSound::NewSoundData(params.m_Sound, params.m_SoundSize, params.m_Type, &sd, 1234);
    dmSound::HSoundInstance instance = 0;
    r = dmSound::NewSoundInstance(sd, &instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    ASSERT_NE((dmSound::HSoundInstance) 0, instance);

    ASSERT_EQ(dmSound::RESULT_OK, dmSound::Update());

    r = dmSound::DeleteSoundInstance(instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    r = dmSound::DeleteSoundData(sd);
    ASSERT_EQ(dmSound::RESULT_OK, r);
}

const TestParams params_verify_test[] = {
TestParams("loopback",
            MONO_TONE_440_22050_44100_WAV,
            MONO_TONE_440_22050_44100_WAV_SIZE,
            dmSound::SOUND_DATA_TYPE_WAV,
            440,
            22050,
            44100,
            1056,
            1),
TestParams("loopback",
            MONO_TONE_440_32000_64000_WAV,
            MONO_TONE_440_32000_64000_WAV_SIZE,
            dmSound::SOUND_DATA_TYPE_WAV,
            440,
            32000,
            64000,
            1056,
            1),
TestParams("loopback",
            MONO_TONE_440_44000_88000_WAV,
            MONO_TONE_440_44000_88000_WAV_SIZE,
            dmSound::SOUND_DATA_TYPE_WAV,
            440,
            44000,
            88000,
            1056,
            1),
TestParams("loopback",
            MONO_TONE_440_44100_88200_WAV,
            MONO_TONE_440_44100_88200_WAV_SIZE,
            dmSound::SOUND_DATA_TYPE_WAV,
            440,
            44100,
            88200,
            1056,
            1),
TestParams("loopback",
            MONO_TONE_2000_22050_44100_WAV,
            MONO_TONE_2000_22050_44100_WAV_SIZE,
            dmSound::SOUND_DATA_TYPE_WAV,
            2000,
            22050,
            44100,
            1056,
            1),
TestParams("loopback",
            MONO_TONE_2000_32000_64000_WAV,
            MONO_TONE_2000_32000_64000_WAV_SIZE,
            dmSound::SOUND_DATA_TYPE_WAV,
            2000,
            32000,
            64000,
            1056,
            1),
TestParams("loopback",
            MONO_TONE_2000_44000_88000_WAV,
            MONO_TONE_2000_44000_88000_WAV_SIZE,
            dmSound::SOUND_DATA_TYPE_WAV,
            2000,
            44000,
            88000,
            1056,
            1),
TestParams("loopback",
            MONO_TONE_2000_44100_88200_WAV,
            MONO_TONE_2000_44100_88200_WAV_SIZE,
            dmSound::SOUND_DATA_TYPE_WAV,
            2000,
            44100,
            88200,
            1056,
            1),
TestParams("loopback",
            STEREO_TONE_440_22050_44100_WAV,
            STEREO_TONE_440_22050_44100_WAV_SIZE,
            dmSound::SOUND_DATA_TYPE_WAV,
            440,
            22050,
            44100,
            1056,
            2),
TestParams("loopback",
            STEREO_TONE_440_32000_64000_WAV,
            STEREO_TONE_440_32000_64000_WAV_SIZE,
            dmSound::SOUND_DATA_TYPE_WAV,
            440,
            32000,
            64000,
            1056,
            2),
TestParams("loopback",
            STEREO_TONE_440_44000_88000_WAV,
            STEREO_TONE_440_44000_88000_WAV_SIZE,
            dmSound::SOUND_DATA_TYPE_WAV,
            440,
            44000,
            88000,
            1056,
            2),
TestParams("loopback",
            STEREO_TONE_440_44100_88200_WAV,
            STEREO_TONE_440_44100_88200_WAV_SIZE,
            dmSound::SOUND_DATA_TYPE_WAV,
            440,
            44100,
            88200,
            1056,
            2),
TestParams("loopback",
            STEREO_TONE_2000_22050_44100_WAV,
            STEREO_TONE_2000_22050_44100_WAV_SIZE,
            dmSound::SOUND_DATA_TYPE_WAV,
            2000,
            22050,
            44100,
            1056,
            2),
TestParams("loopback",
            STEREO_TONE_2000_32000_64000_WAV,
            STEREO_TONE_2000_32000_64000_WAV_SIZE,
            dmSound::SOUND_DATA_TYPE_WAV,
            2000,
            32000,
            64000,
            1056,
            2),
TestParams("loopback",
            STEREO_TONE_2000_44000_88000_WAV,
            STEREO_TONE_2000_44000_88000_WAV_SIZE,
            dmSound::SOUND_DATA_TYPE_WAV,
            2000,
            44000,
            88000,
            1056,
            2),
TestParams("loopback",
            STEREO_TONE_2000_44100_88200_WAV,
            STEREO_TONE_2000_44100_88200_WAV_SIZE,
            dmSound::SOUND_DATA_TYPE_WAV,
            2000,
            44100,
            88200,
            1056,
            2)
};

INSTANTIATE_TEST_CASE_P(dmSoundVerifyTest, dmSoundVerifyTest, jc_test_values_in(params_verify_test));

// We also want to test the panning outputs correctly
const TestParams params_verify_test_pan[] = {
    TestParams("loopback",
            MONO_TONE_440_44100_88200_WAV,
            MONO_TONE_440_44100_88200_WAV_SIZE,
            dmSound::SOUND_DATA_TYPE_WAV,
            440,
            44100,
            88200,
            1056,
            -1.0f,
            1.0f),
    TestParams("loopback",
            STEREO_TONE_440_44100_88200_WAV,
            STEREO_TONE_440_44100_88200_WAV_SIZE,
            dmSound::SOUND_DATA_TYPE_WAV,
            440,
            44100,
            88200,
            1056,
            1.0f,
            1.0f)
};

INSTANTIATE_TEST_CASE_P(dmSoundVerifyTestPan, dmSoundVerifyTest, jc_test_values_in(params_verify_test_pan));

#if !defined(GITHUB_CI) || (defined(GITHUB_CI) && !defined(WIN32))
TEST_P(dmSoundTestGroupRampTest, GroupRamp)
{
    TestParams params = GetParam();
    dmSound::Result r;
    dmSound::HSoundData sd = 0;
    dmSound::NewSoundData(params.m_Sound, params.m_SoundSize, params.m_Type, &sd, 1234);

    printf("tone: %d, rate: %d, frames: %d\n", params.m_ToneRate, params.m_MixRate, params.m_FrameCount);

    dmSound::HSoundInstance instance = 0;
    r = dmSound::NewSoundInstance(sd, &instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    ASSERT_NE((dmSound::HSoundInstance) 0, instance);

    r = dmSound::Play(instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    const uint32_t frame_count = params.m_FrameCount;
    const float mix_rate = params.m_MixRate;
    const int expected_frames = (frame_count * 44100) / (int) mix_rate;

    int prev_frames = g_LoopbackDevice->m_AllOutput.Size() / 2;
    float prev_g = 1.0f;
    do {
        int frames = g_LoopbackDevice->m_AllOutput.Size() / 2;
        float t = (float) (frames) / (float) (params.m_FrameCount);
        float g = 1.0f - t;
        dmSound::SetGroupGain(dmHashString64("master"), g);
        r = dmSound::Update();
        ASSERT_EQ(dmSound::RESULT_OK, r);

        g = GainToScale(g);

        frames = g_LoopbackDevice->m_AllOutput.Size() / 2;
        if (frames != prev_frames || frames == 0)
        {
            for (int i = prev_frames; i < dmMath::Min(expected_frames, frames); i++) {
                int16_t actual = g_LoopbackDevice->m_AllOutput[2 * i];
                float mix = (i - prev_frames) / (float) (frames - prev_frames);
                float expectedf = (32768.0f * 0.8f * ((1.0f - mix) * prev_g + g * mix));
                int16_t expected = (int16_t) expectedf ;
                ASSERT_NEAR(expected * 0.707107f, actual, 2U);      // 0.707... due to mono sample being panned to center
            }
            prev_g = g;
        }

        prev_frames = frames;
    } while (dmSound::IsPlaying(instance));

    r = dmSound::DeleteSoundInstance(instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    r = dmSound::DeleteSoundData(sd);
    ASSERT_EQ(dmSound::RESULT_OK, r);
}

const TestParams params_group_ramp_test[] = {
    TestParams("loopback",
        MONO_DC_44100_88200_WAV,
        MONO_DC_44100_88200_WAV_SIZE,
        dmSound::SOUND_DATA_TYPE_WAV,
        0,
        44100,
        88200,
        2048,
        1)
};
INSTANTIATE_TEST_CASE_P(dmSoundTestGroupRampTest, dmSoundTestGroupRampTest, jc_test_values_in(params_group_ramp_test));
#endif

#if !defined(GITHUB_CI) || (defined(GITHUB_CI) && !defined(WIN32))
TEST_P(dmSoundTestSpeedTest, Speed)
{
    TestParams params = GetParam();
    dmSound::Result r;
    dmSound::HSoundData sd = 0;
    dmSound::NewSoundData(params.m_Sound, params.m_SoundSize, params.m_Type, &sd, 1234);

    // printf("tone: %d, rate: %d, frames: %d, speed: %f\n", params.m_ToneRate, params.m_MixRate, params.m_FrameCount, params.m_Speed);

    dmSound::HSoundInstance instance = 0;
    r = dmSound::NewSoundInstance(sd, &instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    ASSERT_NE((dmSound::HSoundInstance) 0, instance);

    r = dmSound::SetParameter(instance, dmSound::PARAMETER_GAIN, dmVMath::Vector4(0.8,0,0,0));
    ASSERT_EQ(dmSound::RESULT_OK, r);
    r = dmSound::SetParameter(instance, dmSound::PARAMETER_SPEED, dmVMath::Vector4(params.m_Speed,0,0,0));
    ASSERT_EQ(dmSound::RESULT_OK, r);

    r = dmSound::Play(instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    // We create a muted instance, to make sure that they end at the same time
    dmSound::HSoundInstance muted_instance = 0;
    r = dmSound::NewSoundInstance(sd, &muted_instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    ASSERT_NE((dmSound::HSoundInstance) 0, muted_instance);

    r = dmSound::SetParameter(muted_instance, dmSound::PARAMETER_GAIN, dmVMath::Vector4(0,0,0,0)); // gain 0
    ASSERT_EQ(dmSound::RESULT_OK, r);
    r = dmSound::SetParameter(muted_instance, dmSound::PARAMETER_SPEED, dmVMath::Vector4(params.m_Speed,0,0,0));
    ASSERT_EQ(dmSound::RESULT_OK, r);

    r = dmSound::Play(muted_instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);


    float mix_rate = params.m_MixRate / 44100.0f;
    uint32_t buffer_count = params.m_BufferFrameCount * params.m_Speed;
    int expected_count = params.m_FrameCount / buffer_count;
    expected_count /= mix_rate;

    if ((expected_count * buffer_count) < params.m_FrameCount)
        expected_count++;

    do {
        r = dmSound::Update();
        ASSERT_EQ(dmSound::RESULT_OK, r);
        ASSERT_LT(g_LoopbackDevice->m_NumWrites, 500); // probably will never end

        int64_t pos_1 = dmSound::GetInternalPos(instance);
        int64_t pos_2 = dmSound::GetInternalPos(muted_instance);
        (void)pos_1;
        (void)pos_2;
    } while (dmSound::IsPlaying(instance));

    // The loop back device will have time to write out another output buffer while the
    // sound is finishing up
    expected_count++;

    ASSERT_NEAR((float)expected_count, (float)g_LoopbackDevice->m_NumWrites, 1.0f);

    r = dmSound::DeleteSoundInstance(instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    r = dmSound::DeleteSoundInstance(muted_instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    r = dmSound::DeleteSoundData(sd);
    ASSERT_EQ(dmSound::RESULT_OK, r);
}

const TestParams params_speed_test[] = {
    TestParams("loopback",
            MONO_TONE_440_44100_88200_WAV,
            MONO_TONE_440_44100_88200_WAV_SIZE,
            dmSound::SOUND_DATA_TYPE_WAV,
            440,
            44100,
            88200,
            2048,
            0.0f,
            2.0f),
    TestParams("loopback",
            MONO_TONE_440_32000_64000_WAV,
            MONO_TONE_440_32000_64000_WAV_SIZE,
            dmSound::SOUND_DATA_TYPE_WAV,
            440,
            32000,
            64000,
            2048,
            0.0f,
            2.0f),
    TestParams("loopback",
            STEREO_TONE_440_32000_64000_WAV,
            STEREO_TONE_440_32000_64000_WAV_SIZE,
            dmSound::SOUND_DATA_TYPE_WAV,
            440,
            32000,
            64000,
            2048,
            0.0f,
            2.0f),
    TestParams("loopback",
            MONO_TONE_440_44100_88200_WAV,
            MONO_TONE_440_44100_88200_WAV_SIZE,
            dmSound::SOUND_DATA_TYPE_WAV,
            440,
            44100,
            88200,
            2048,
            0.0f,
            0.5f),
    TestParams("loopback",
            STEREO_TONE_440_32000_64000_WAV,
            STEREO_TONE_440_32000_64000_WAV_SIZE,
            dmSound::SOUND_DATA_TYPE_WAV,
            440,
            32000,
            64000,
            2048,
            0.0f,
            0.5f),
    TestParams("loopback",
            MONO_TONE_440_44100_88200_WAV,      // sound
            MONO_TONE_440_44100_88200_WAV_SIZE, // uint32_t sound_size
            dmSound::SOUND_DATA_TYPE_WAV,       // SoundDataType type
            440,                                // uint32_t tone_rate
            44100,                              // uint32_t mix_rate
            88200,                              // uint32_t frame_count
            1999,                               // uint32_t buffer_frame_count
            0.0f,                               // float pan
            3.999909297f),                      // float speed - this strange number will result in having remainder frames at the end of mixing
};
INSTANTIATE_TEST_CASE_P(dmSoundTestSpeedTest, dmSoundTestSpeedTest, jc_test_values_in(params_speed_test));
#endif

#if !defined(GITHUB_CI) || (defined(GITHUB_CI) && !defined(WIN32))
TEST_P(dmSoundVerifyOggTest, Mix)
{
    TestParams params = GetParam();
    dmSound::Result r;
    dmSound::HSoundData sd = 0;
    dmSound::NewSoundData(params.m_Sound, params.m_SoundSize, params.m_Type, &sd, 1234);

    printf("verifying ogg mix: frames: %d\n", params.m_FrameCount);

    dmSound::HSoundInstance instance = 0;
    r = dmSound::NewSoundInstance(sd, &instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    ASSERT_NE((dmSound::HSoundInstance) 0, instance);

    r = dmSound::Play(instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    do {
        r = dmSound::Update();
        ASSERT_EQ(dmSound::RESULT_OK, r);
    } while (dmSound::IsPlaying(instance));

    r = dmSound::DeleteSoundInstance(instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    r = dmSound::DeleteSoundData(sd);
    ASSERT_EQ(dmSound::RESULT_OK, r);
}

TEST_P(dmSoundVerifyOggTest, Kill)
{
    TestParams params = GetParam();
    dmSound::Result r;
    dmSound::HSoundData sd = 0;
    dmSound::NewSoundData(params.m_Sound, params.m_SoundSize, params.m_Type, &sd, 1234);

    int tick = 0;
    int killTick = 16;
    int killed = 0;
    printf("verifying ogg kill: frames: %d killing on: %d\n", params.m_FrameCount, killTick);

    dmSound::HSoundInstance instanceA = 0;
    dmSound::HSoundInstance instanceB = 0;
    r = dmSound::NewSoundInstance(sd, &instanceA);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    ASSERT_NE((dmSound::HSoundInstance) 0, instanceA);

    r = dmSound::NewSoundInstance(sd, &instanceB);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    ASSERT_NE((dmSound::HSoundInstance) 0, instanceB);

    r = dmSound::Play(instanceA);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    r = dmSound::Play(instanceB);
    ASSERT_EQ(dmSound::RESULT_OK, r);


    do {
        r = dmSound::Update();
        ASSERT_EQ(dmSound::RESULT_OK, r);
        if (0 == killed && ++tick == killTick) {
            r =  dmSound::Stop(instanceB);
            ASSERT_EQ(dmSound::RESULT_OK, r);
            r = dmSound::Update();
            ASSERT_EQ(dmSound::RESULT_OK, r);
            r = dmSound::DeleteSoundInstance(instanceB);
            killed = 1;
            ASSERT_EQ(dmSound::RESULT_OK, r);
            r = dmSound::Update();
            ASSERT_EQ(dmSound::RESULT_OK, r);
        }
    } while (dmSound::IsPlaying(instanceA));

    if (0 == killed) {
        r = dmSound::DeleteSoundInstance(instanceB);
    }
    ASSERT_EQ(dmSound::RESULT_OK, r);

    r = dmSound::DeleteSoundInstance(instanceA);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    r = dmSound::DeleteSoundData(sd);
    ASSERT_EQ(dmSound::RESULT_OK, r);
}

// Verifies that the Skip function skips the correct number of samples
TEST_P(dmSoundVerifyOggTest, SkipSync)
{
    TestParams params = GetParam();
    dmSound::Result r;
    dmSound::HSoundData sd = 0;
    dmSound::NewSoundData(params.m_Sound, params.m_SoundSize, params.m_Type, &sd, 1234);

    dmSound::HSoundInstance instance = 0;
    r = dmSound::NewSoundInstance(sd, &instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    ASSERT_NE((dmSound::HSoundInstance) 0, instance);

    r = dmSound::SetParameter(instance, dmSound::PARAMETER_GAIN, dmVMath::Vector4(0.8f,0,0,0));
    ASSERT_EQ(dmSound::RESULT_OK, r);
    r = dmSound::SetParameter(instance, dmSound::PARAMETER_SPEED, dmVMath::Vector4(params.m_Speed,0,0,0));
    ASSERT_EQ(dmSound::RESULT_OK, r);

    r = dmSound::Play(instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    // We create a muted instance, to make sure that they end at the same time
    dmSound::HSoundInstance muted_instance = 0;
    r = dmSound::NewSoundInstance(sd, &muted_instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    ASSERT_NE((dmSound::HSoundInstance) 0, muted_instance);

    r = dmSound::SetParameter(muted_instance, dmSound::PARAMETER_GAIN, dmVMath::Vector4(0,0,0,0)); // gain 0
    ASSERT_EQ(dmSound::RESULT_OK, r);
    r = dmSound::SetParameter(muted_instance, dmSound::PARAMETER_SPEED, dmVMath::Vector4(params.m_Speed,0,0,0));
    ASSERT_EQ(dmSound::RESULT_OK, r);

    r = dmSound::Play(muted_instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    do {
        r = dmSound::Update();
        ASSERT_EQ(dmSound::RESULT_OK, r);

        int64_t pos_1 = dmSound::GetInternalPos(instance);
        int64_t pos_2 = dmSound::GetInternalPos(muted_instance);
        ASSERT_EQ(pos_1, pos_2); // If these numbers differ, then the Skip function isn't working correctly
    } while (dmSound::IsPlaying(instance));

    r = dmSound::DeleteSoundInstance(instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    r = dmSound::DeleteSoundInstance(muted_instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    r = dmSound::DeleteSoundData(sd);
    ASSERT_EQ(dmSound::RESULT_OK, r);
}

// Verifies that the ref counted for HSoundData works correct
TEST_P(dmSoundVerifyOggTest, SoundDataRefCount)
{
    TestParams params = GetParam();
    dmSound::Result r;
    dmSound::HSoundData sd = 0;
    dmSound::NewSoundData(params.m_Sound, params.m_SoundSize, params.m_Type, &sd, 1234);

    dmSound::HSoundInstance instance = 0;
    r = dmSound::NewSoundInstance(sd, &instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    ASSERT_NE((dmSound::HSoundInstance) 0, instance);

    r = dmSound::SetParameter(instance, dmSound::PARAMETER_GAIN, dmVMath::Vector4(0.8f,0,0,0));
    ASSERT_EQ(dmSound::RESULT_OK, r);
    r = dmSound::SetParameter(instance, dmSound::PARAMETER_SPEED, dmVMath::Vector4(params.m_Speed,0,0,0));
    ASSERT_EQ(dmSound::RESULT_OK, r);

    r = dmSound::Play(instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    // We create a muted instance, to make sure that they end at the same time
    dmSound::HSoundInstance muted_instance = 0;
    r = dmSound::NewSoundInstance(sd, &muted_instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    ASSERT_NE((dmSound::HSoundInstance) 0, muted_instance);

    r = dmSound::SetParameter(muted_instance, dmSound::PARAMETER_GAIN, dmVMath::Vector4(0,0,0,0)); // gain 0
    ASSERT_EQ(dmSound::RESULT_OK, r);
    r = dmSound::SetParameter(muted_instance, dmSound::PARAMETER_SPEED, dmVMath::Vector4(params.m_Speed,0,0,0));
    ASSERT_EQ(dmSound::RESULT_OK, r);

    r = dmSound::Play(muted_instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    ASSERT_EQ(dmSound::GetRefCount(sd), 3);
    dmSound::DeleteSoundData(sd);
    ASSERT_EQ(dmSound::GetRefCount(sd), 2);

    r = dmSound::DeleteSoundInstance(instance);
    ASSERT_EQ(dmSound::GetRefCount(sd), 1);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    r = dmSound::DeleteSoundInstance(muted_instance);
    ASSERT_EQ(dmSound::GetRefCount(sd), 0);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    r = dmSound::DeleteSoundData(sd);
    ASSERT_EQ(dmSound::RESULT_OK, r);
}

const TestParams params_verify_ogg_test[] = {TestParams("loopback",
                                            MONO_RESAMPLE_FRAMECOUNT_16000_OGG,
                                            MONO_RESAMPLE_FRAMECOUNT_16000_OGG_SIZE,
                                            dmSound::SOUND_DATA_TYPE_OGG_VORBIS,
                                            2000,
                                            44100,
                                            35200,
                                            2048,
                                            1)};
INSTANTIATE_TEST_CASE_P(dmSoundVerifyOggTest, dmSoundVerifyOggTest, jc_test_values_in(params_verify_ogg_test));
#endif

#if !defined(GITHUB_CI) || (defined(GITHUB_CI) && !defined(WIN32))
TEST_P(dmSoundVerifyOpusTest, Mix)
{
    TestParams params = GetParam();
    dmSound::Result r;
    dmSound::HSoundData sd = 0;
    dmSound::NewSoundData(params.m_Sound, params.m_SoundSize, params.m_Type, &sd, 1234);

    printf("verifying opus mix: frames: %d\n", params.m_FrameCount);

    dmSound::HSoundInstance instance = 0;
    r = dmSound::NewSoundInstance(sd, &instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    ASSERT_NE((dmSound::HSoundInstance) 0, instance);

    r = dmSound::Play(instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    do {
        r = dmSound::Update();
        ASSERT_EQ(dmSound::RESULT_OK, r);
    } while (dmSound::IsPlaying(instance));

    r = dmSound::DeleteSoundInstance(instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    r = dmSound::DeleteSoundData(sd);
    ASSERT_EQ(dmSound::RESULT_OK, r);
}

TEST_P(dmSoundVerifyOpusTest, Kill)
{
    TestParams params = GetParam();
    dmSound::Result r;
    dmSound::HSoundData sd = 0;
    dmSound::NewSoundData(params.m_Sound, params.m_SoundSize, params.m_Type, &sd, 1234);

    int tick = 0;
    int killTick = 16;
    int killed = 0;
    printf("verifying opus kill: frames: %d killing on: %d\n", params.m_FrameCount, killTick);

    dmSound::HSoundInstance instanceA = 0;
    dmSound::HSoundInstance instanceB = 0;
    r = dmSound::NewSoundInstance(sd, &instanceA);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    ASSERT_NE((dmSound::HSoundInstance) 0, instanceA);

    r = dmSound::NewSoundInstance(sd, &instanceB);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    ASSERT_NE((dmSound::HSoundInstance) 0, instanceB);

    r = dmSound::Play(instanceA);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    r = dmSound::Play(instanceB);
    ASSERT_EQ(dmSound::RESULT_OK, r);


    do {
        r = dmSound::Update();
        ASSERT_EQ(dmSound::RESULT_OK, r);
        if (0 == killed && ++tick == killTick) {
            r =  dmSound::Stop(instanceB);
            ASSERT_EQ(dmSound::RESULT_OK, r);
            r = dmSound::Update();
            ASSERT_EQ(dmSound::RESULT_OK, r);
            r = dmSound::DeleteSoundInstance(instanceB);
            killed = 1;
            ASSERT_EQ(dmSound::RESULT_OK, r);
            r = dmSound::Update();
            ASSERT_EQ(dmSound::RESULT_OK, r);
        }
    } while (dmSound::IsPlaying(instanceA));

    if (0 == killed) {
        r = dmSound::DeleteSoundInstance(instanceB);
    }
    ASSERT_EQ(dmSound::RESULT_OK, r);

    r = dmSound::DeleteSoundInstance(instanceA);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    r = dmSound::DeleteSoundData(sd);
    ASSERT_EQ(dmSound::RESULT_OK, r);
}

// Verifies that the Skip function skips the correct number of samples
TEST_P(dmSoundVerifyOpusTest, SkipSync)
{
    TestParams params = GetParam();
    dmSound::Result r;
    dmSound::HSoundData sd = 0;
    dmSound::NewSoundData(params.m_Sound, params.m_SoundSize, params.m_Type, &sd, 1234);

    dmSound::HSoundInstance instance = 0;
    r = dmSound::NewSoundInstance(sd, &instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    ASSERT_NE((dmSound::HSoundInstance) 0, instance);

    r = dmSound::SetParameter(instance, dmSound::PARAMETER_GAIN, dmVMath::Vector4(0.5f,0,0,0));
    ASSERT_EQ(dmSound::RESULT_OK, r);
    r = dmSound::SetParameter(instance, dmSound::PARAMETER_SPEED, dmVMath::Vector4(params.m_Speed,0,0,0));
    ASSERT_EQ(dmSound::RESULT_OK, r);

    r = dmSound::Play(instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    // We create a muted instance, to make sure that they end at the same time
    dmSound::HSoundInstance muted_instance = 0;
    r = dmSound::NewSoundInstance(sd, &muted_instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    ASSERT_NE((dmSound::HSoundInstance) 0, muted_instance);

    r = dmSound::SetParameter(muted_instance, dmSound::PARAMETER_GAIN, dmVMath::Vector4(0,0,0,0)); // gain 0
    ASSERT_EQ(dmSound::RESULT_OK, r);
    r = dmSound::SetParameter(muted_instance, dmSound::PARAMETER_SPEED, dmVMath::Vector4(params.m_Speed,0,0,0));
    ASSERT_EQ(dmSound::RESULT_OK, r);

    r = dmSound::Play(muted_instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    do {
        r = dmSound::Update();
        ASSERT_EQ(dmSound::RESULT_OK, r);

        int64_t pos_1 = dmSound::GetInternalPos(instance);
        int64_t pos_2 = dmSound::GetInternalPos(muted_instance);
        ASSERT_EQ(pos_1, pos_2); // If these numbers differ, then the Skip function isn't working correctly
    } while (dmSound::IsPlaying(instance));

    r = dmSound::DeleteSoundInstance(instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    r = dmSound::DeleteSoundInstance(muted_instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    r = dmSound::DeleteSoundData(sd);
    ASSERT_EQ(dmSound::RESULT_OK, r);
}

// Verifies that the ref counted for HSoundData works correct
TEST_P(dmSoundVerifyOpusTest, SoundDataRefCount)
{
    TestParams params = GetParam();
    dmSound::Result r;
    dmSound::HSoundData sd = 0;
    dmSound::NewSoundData(params.m_Sound, params.m_SoundSize, params.m_Type, &sd, 1234);

    dmSound::HSoundInstance instance = 0;
    r = dmSound::NewSoundInstance(sd, &instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    ASSERT_NE((dmSound::HSoundInstance) 0, instance);

    r = dmSound::SetParameter(instance, dmSound::PARAMETER_GAIN, dmVMath::Vector4(0.5f,0,0,0));
    ASSERT_EQ(dmSound::RESULT_OK, r);
    r = dmSound::SetParameter(instance, dmSound::PARAMETER_SPEED, dmVMath::Vector4(params.m_Speed,0,0,0));
    ASSERT_EQ(dmSound::RESULT_OK, r);

    r = dmSound::Play(instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    // We create a muted instance, to make sure that they end at the same time
    dmSound::HSoundInstance muted_instance = 0;
    r = dmSound::NewSoundInstance(sd, &muted_instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    ASSERT_NE((dmSound::HSoundInstance) 0, muted_instance);

    r = dmSound::SetParameter(muted_instance, dmSound::PARAMETER_GAIN, dmVMath::Vector4(0,0,0,0)); // gain 0
    ASSERT_EQ(dmSound::RESULT_OK, r);
    r = dmSound::SetParameter(muted_instance, dmSound::PARAMETER_SPEED, dmVMath::Vector4(params.m_Speed,0,0,0));
    ASSERT_EQ(dmSound::RESULT_OK, r);

    r = dmSound::Play(muted_instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    ASSERT_EQ(dmSound::GetRefCount(sd), 3);
    dmSound::DeleteSoundData(sd);
    ASSERT_EQ(dmSound::GetRefCount(sd), 2);

    r = dmSound::DeleteSoundInstance(instance);
    ASSERT_EQ(dmSound::GetRefCount(sd), 1);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    r = dmSound::DeleteSoundInstance(muted_instance);
    ASSERT_EQ(dmSound::GetRefCount(sd), 0);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    r = dmSound::DeleteSoundData(sd);
    ASSERT_EQ(dmSound::RESULT_OK, r);
}

const TestParams params_verify_opus_test[] = {TestParams("loopback",
                                            MONO_RESAMPLE_FRAMECOUNT_16000_OPUS,
                                            MONO_RESAMPLE_FRAMECOUNT_16000_OPUS_SIZE,
                                            dmSound::SOUND_DATA_TYPE_OPUS,
                                            2000,
                                            44100,
                                            35200,
                                            2048,
                                            1)};
INSTANTIATE_TEST_CASE_P(dmSoundVerifyOpusTest, dmSoundVerifyOpusTest, jc_test_values_in(params_verify_opus_test));
#endif

#if !defined(GITHUB_CI) || (defined(GITHUB_CI) && !defined(WIN32))
TEST_P(dmSoundVerifyAdpcmTest, Mix)
{
    TestParams params = GetParam();
    dmSound::Result r;
    dmSound::HSoundData sd = 0;
    dmSound::NewSoundData(params.m_Sound, params.m_SoundSize, params.m_Type, &sd, 1234);

    printf("verifying adpcm mix: frames: %d\n", params.m_FrameCount);

    dmSound::HSoundInstance instance = 0;
    r = dmSound::NewSoundInstance(sd, &instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    ASSERT_NE((dmSound::HSoundInstance) 0, instance);

    r = dmSound::Play(instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    do {
        r = dmSound::Update();
        ASSERT_EQ(dmSound::RESULT_OK, r);
    } while (dmSound::IsPlaying(instance));

    r = dmSound::DeleteSoundInstance(instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    r = dmSound::DeleteSoundData(sd);
    ASSERT_EQ(dmSound::RESULT_OK, r);
}

TEST_P(dmSoundVerifyAdpcmTest, Kill)
{
    TestParams params = GetParam();
    dmSound::Result r;
    dmSound::HSoundData sd = 0;
    dmSound::NewSoundData(params.m_Sound, params.m_SoundSize, params.m_Type, &sd, 1234);

    int tick = 0;
    int killTick = 16;
    int killed = 0;
    printf("verifying adpcm kill: frames: %d killing on: %d\n", params.m_FrameCount, killTick);

    dmSound::HSoundInstance instanceA = 0;
    dmSound::HSoundInstance instanceB = 0;
    r = dmSound::NewSoundInstance(sd, &instanceA);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    ASSERT_NE((dmSound::HSoundInstance) 0, instanceA);

    r = dmSound::NewSoundInstance(sd, &instanceB);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    ASSERT_NE((dmSound::HSoundInstance) 0, instanceB);

    r = dmSound::Play(instanceA);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    r = dmSound::Play(instanceB);
    ASSERT_EQ(dmSound::RESULT_OK, r);


    do {
        r = dmSound::Update();
        ASSERT_EQ(dmSound::RESULT_OK, r);
        if (0 == killed && ++tick == killTick) {
            r =  dmSound::Stop(instanceB);
            ASSERT_EQ(dmSound::RESULT_OK, r);
            r = dmSound::Update();
            ASSERT_EQ(dmSound::RESULT_OK, r);
            r = dmSound::DeleteSoundInstance(instanceB);
            killed = 1;
            ASSERT_EQ(dmSound::RESULT_OK, r);
            r = dmSound::Update();
            ASSERT_EQ(dmSound::RESULT_OK, r);
        }
    } while (dmSound::IsPlaying(instanceA));

    if (0 == killed) {
        r = dmSound::DeleteSoundInstance(instanceB);
    }
    ASSERT_EQ(dmSound::RESULT_OK, r);

    r = dmSound::DeleteSoundInstance(instanceA);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    r = dmSound::DeleteSoundData(sd);
    ASSERT_EQ(dmSound::RESULT_OK, r);
}

// Verifies that the Skip function skips the correct number of samples
TEST_P(dmSoundVerifyAdpcmTest, SkipSync)
{
    TestParams params = GetParam();
    dmSound::Result r;
    dmSound::HSoundData sd = 0;
    dmSound::NewSoundData(params.m_Sound, params.m_SoundSize, params.m_Type, &sd, 1234);

    dmSound::HSoundInstance instance = 0;
    r = dmSound::NewSoundInstance(sd, &instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    ASSERT_NE((dmSound::HSoundInstance) 0, instance);

    r = dmSound::SetParameter(instance, dmSound::PARAMETER_GAIN, dmVMath::Vector4(0.8f,0,0,0));
    ASSERT_EQ(dmSound::RESULT_OK, r);
    r = dmSound::SetParameter(instance, dmSound::PARAMETER_SPEED, dmVMath::Vector4(params.m_Speed,0,0,0));
    ASSERT_EQ(dmSound::RESULT_OK, r);

    r = dmSound::Play(instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    // We create a muted instance, to make sure that they end at the same time
    dmSound::HSoundInstance muted_instance = 0;
    r = dmSound::NewSoundInstance(sd, &muted_instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    ASSERT_NE((dmSound::HSoundInstance) 0, muted_instance);

    r = dmSound::SetParameter(muted_instance, dmSound::PARAMETER_GAIN, dmVMath::Vector4(0,0,0,0)); // gain 0
    ASSERT_EQ(dmSound::RESULT_OK, r);
    r = dmSound::SetParameter(muted_instance, dmSound::PARAMETER_SPEED, dmVMath::Vector4(params.m_Speed,0,0,0));
    ASSERT_EQ(dmSound::RESULT_OK, r);

    r = dmSound::Play(muted_instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    do {
        r = dmSound::Update();
        ASSERT_EQ(dmSound::RESULT_OK, r);

        int64_t pos_1 = dmSound::GetInternalPos(instance);
        int64_t pos_2 = dmSound::GetInternalPos(muted_instance);
        ASSERT_EQ(pos_1, pos_2); // If these numbers differ, then the Skip function isn't working correctly
    } while (dmSound::IsPlaying(instance));

    r = dmSound::DeleteSoundInstance(instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    r = dmSound::DeleteSoundInstance(muted_instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    r = dmSound::DeleteSoundData(sd);
    ASSERT_EQ(dmSound::RESULT_OK, r);
}

// Verifies that the ref counted for HSoundData works correct
TEST_P(dmSoundVerifyAdpcmTest, SoundDataRefCount)
{
    TestParams params = GetParam();
    dmSound::Result r;
    dmSound::HSoundData sd = 0;
    dmSound::NewSoundData(params.m_Sound, params.m_SoundSize, params.m_Type, &sd, 1234);

    dmSound::HSoundInstance instance = 0;
    r = dmSound::NewSoundInstance(sd, &instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    ASSERT_NE((dmSound::HSoundInstance) 0, instance);

    r = dmSound::SetParameter(instance, dmSound::PARAMETER_GAIN, dmVMath::Vector4(0.5f,0,0,0));
    ASSERT_EQ(dmSound::RESULT_OK, r);
    r = dmSound::SetParameter(instance, dmSound::PARAMETER_SPEED, dmVMath::Vector4(params.m_Speed,0,0,0));
    ASSERT_EQ(dmSound::RESULT_OK, r);

    r = dmSound::Play(instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    // We create a muted instance, to make sure that they end at the same time
    dmSound::HSoundInstance muted_instance = 0;
    r = dmSound::NewSoundInstance(sd, &muted_instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    ASSERT_NE((dmSound::HSoundInstance) 0, muted_instance);

    r = dmSound::SetParameter(muted_instance, dmSound::PARAMETER_GAIN, dmVMath::Vector4(0,0,0,0)); // gain 0
    ASSERT_EQ(dmSound::RESULT_OK, r);
    r = dmSound::SetParameter(muted_instance, dmSound::PARAMETER_SPEED, dmVMath::Vector4(params.m_Speed,0,0,0));
    ASSERT_EQ(dmSound::RESULT_OK, r);

    r = dmSound::Play(muted_instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    ASSERT_EQ(dmSound::GetRefCount(sd), 3);
    dmSound::DeleteSoundData(sd);
    ASSERT_EQ(dmSound::GetRefCount(sd), 2);

    r = dmSound::DeleteSoundInstance(instance);
    ASSERT_EQ(dmSound::GetRefCount(sd), 1);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    r = dmSound::DeleteSoundInstance(muted_instance);
    ASSERT_EQ(dmSound::GetRefCount(sd), 0);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    r = dmSound::DeleteSoundData(sd);
    ASSERT_EQ(dmSound::RESULT_OK, r);
}

const TestParams params_verify_adpcm_test[] = {TestParams("loopback",
                                            MONO_RESAMPLE_FRAMECOUNT_16000_ADPCM_WAV,
                                            MONO_RESAMPLE_FRAMECOUNT_16000_ADPCM_WAV_SIZE,
                                            dmSound::SOUND_DATA_TYPE_WAV,
                                            2000,
                                            44100,
                                            35200,
                                            2048,
                                            1)};
INSTANTIATE_TEST_CASE_P(dmSoundVerifyAdpcmTest, dmSoundVerifyAdpcmTest, jc_test_values_in(params_verify_adpcm_test));
#endif

#if !defined(GITHUB_CI) || (defined(GITHUB_CI) && !defined(WIN32))
TEST_P(dmSoundTestPlayTest, Panning)
{

    TestParams params = GetParam();
    dmSound::Result r;
    dmSound::HSoundData sd = 0;
    dmSound::NewSoundData(params.m_Sound, params.m_SoundSize, params.m_Type, &sd, 1234);

    dmSound::HSoundInstance instance = 0;
    r = dmSound::NewSoundInstance(sd, &instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    ASSERT_NE((dmSound::HSoundInstance) 0, instance);

    r = dmSound::SetParameter(instance, dmSound::PARAMETER_GAIN, dmVMath::Vector4(0.8f,0,0,0));
    ASSERT_EQ(dmSound::RESULT_OK, r);

    float a = 0.0f;

    r = dmSound::SetParameter(instance, dmSound::PARAMETER_PAN, dmVMath::Vector4(cosf(a),0,0,0));
    ASSERT_EQ(dmSound::RESULT_OK, r);

    ASSERT_EQ(dmSound::RESULT_OK, r);

    r = dmSound::Play(instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    bool playing = false;
    float length = params.LengthInSeconds();
    uint64_t tstart = dmTime::GetMonotonicTime();
    do {
        r = dmSound::Update();
        ASSERT_EQ(dmSound::RESULT_OK, r);
        a += M_PI / 20000000.0f;
        if (a > M_PI*2) {
            a-= M_PI*2;
        }
        float pan = cosf(a);
        r = dmSound::SetParameter(instance, dmSound::PARAMETER_PAN, dmVMath::Vector4(pan,0,0,0));
        ASSERT_EQ(dmSound::RESULT_OK, r);

        uint64_t tend = dmTime::GetMonotonicTime();
        float elapsed = (tend - tstart) / 1000000.0f;

        if (length > 0.0f)
            playing = elapsed <= length;
        else
            playing = dmSound::IsPlaying(instance);

    } while (playing);

    r = dmSound::DeleteSoundInstance(instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    r = dmSound::DeleteSoundData(sd);
    ASSERT_EQ(dmSound::RESULT_OK, r);
}

TEST_P(dmSoundTestPlaySpeedTest, PlaySpeed)
{
    TestParams params = GetParam();
    dmSound::Result r;
    dmSound::HSoundData sd = 0;
    dmSound::NewSoundData(params.m_Sound, params.m_SoundSize, params.m_Type, &sd, 1234);

    dmSound::HSoundInstance instance = 0;
    r = dmSound::NewSoundInstance(sd, &instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    ASSERT_NE((dmSound::HSoundInstance) 0, instance);

    r = dmSound::SetLooping(instance, 1, params.m_Loopcount);

    r = dmSound::SetParameter(instance, dmSound::PARAMETER_GAIN, dmVMath::Vector4(0.8f,0,0,0));
    ASSERT_EQ(dmSound::RESULT_OK, r);
    r = dmSound::SetParameter(instance, dmSound::PARAMETER_SPEED, dmVMath::Vector4(params.m_Speed,0,0,0));
    ASSERT_EQ(dmSound::RESULT_OK, r);

    r = dmSound::Play(instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    r = dmSound::Play(instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    float a = 0.0f;
    do {
        r = dmSound::Update();
        ASSERT_EQ(dmSound::RESULT_OK, r);
        a += M_PI / 20000000.0f;
        if (a > M_PI*2) {
            a-= M_PI*2;
        }
        r = dmSound::SetParameter(instance, dmSound::PARAMETER_PAN, dmVMath::Vector4(cosf(a),0,0,0));
        ASSERT_EQ(dmSound::RESULT_OK, r);

    } while (dmSound::IsPlaying(instance));

    r = dmSound::DeleteSoundInstance(instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    r = dmSound::DeleteSoundData(sd);
    ASSERT_EQ(dmSound::RESULT_OK, r);
}

#define SOUND_TEST(DEVICE, CHANNELS, TONE, SAMPLE_RATE, NUM_FRAMES, BUFFERSIZE, NUM_CHANNELS) \
    TestParams(DEVICE, \
                CHANNELS ## _TONE_ ## TONE ## _ ## SAMPLE_RATE ## _ ## NUM_FRAMES ## _WAV, \
                CHANNELS ## _TONE_ ## TONE ## _ ## SAMPLE_RATE ## _ ## NUM_FRAMES ## _WAV_SIZE, \
                dmSound::SOUND_DATA_TYPE_WAV, \
                TONE, \
                SAMPLE_RATE, \
                NUM_FRAMES, \
                BUFFERSIZE, \
                NUM_CHANNELS)

const TestParams params_test_play_test[] = {

    SOUND_TEST("default", MONO, 2000, 22050, 5512, 2048, 1),
    SOUND_TEST("default", MONO, 2000, 32000, 8000, 2048, 1),
    SOUND_TEST("default", MONO, 2000, 44000, 11000, 2048, 1),
    SOUND_TEST("default", MONO, 2000, 44100, 11025, 2048, 1),
    SOUND_TEST("default", MONO, 2000, 48000, 12000, 2048, 1),
    SOUND_TEST("default", MONO, 440, 22050, 5512, 2048, 1),
    SOUND_TEST("default", MONO, 440, 32000, 8000, 2048, 1),
    SOUND_TEST("default", MONO, 440, 44000, 11000, 2048, 1),
    SOUND_TEST("default", MONO, 440, 44100, 11025, 2048, 1),
    SOUND_TEST("default", MONO, 440, 48000, 12000, 2048, 1),

    SOUND_TEST("default", STEREO, 440, 22050, 5512, 2048, 2),
    SOUND_TEST("default", STEREO, 440, 32000, 8000, 2048, 2),
    SOUND_TEST("default", STEREO, 440, 44000, 11000, 2048, 2),
    SOUND_TEST("default", STEREO, 440, 44100, 11025, 2048, 2),
    SOUND_TEST("default", STEREO, 440, 48000, 12000, 2048, 2),
    SOUND_TEST("default", STEREO, 2000, 22050, 5512, 2048, 2),
    SOUND_TEST("default", STEREO, 2000, 32000, 8000, 2048, 2),
    SOUND_TEST("default", STEREO, 2000, 44000, 11000, 2048, 2),
    SOUND_TEST("default", STEREO, 2000, 44100, 11025, 2048, 2),
    SOUND_TEST("default", STEREO, 2000, 48000, 12000, 2048, 2),

    TestParams("default", AMBIENCE_OPUS, AMBIENCE_OPUS_SIZE, dmSound::SOUND_DATA_TYPE_OPUS, 0, 0, 0, 2048, 2),
//  TestParams("default", MUSIC_OPUS, MUSIC_OPUS_SIZE, dmSound::SOUND_DATA_TYPE_OPUS, 0, 0, 0, 2048, 2),
    TestParams("default", MONO_RESAMPLE_FRAMECOUNT_16000_OPUS, MONO_RESAMPLE_FRAMECOUNT_16000_OPUS_SIZE, dmSound::SOUND_DATA_TYPE_OPUS, 0, 0, 0, 2048, 1),

    TestParams("default", AMBIENCE_ADPCM_WAV, AMBIENCE_ADPCM_WAV_SIZE, dmSound::SOUND_DATA_TYPE_WAV, 0, 0, 0, 2048, 2),
//  TestParams("default", MUSIC_ADPCM_WAV, MUSIC_ADPCM_WAV_SIZE, dmSound::SOUND_DATA_TYPE_WAV, 0, 0, 0, 2048, 2),
    TestParams("default", MONO_RESAMPLE_FRAMECOUNT_16000_ADPCM_WAV, MONO_RESAMPLE_FRAMECOUNT_16000_ADPCM_WAV_SIZE, dmSound::SOUND_DATA_TYPE_WAV, 0, 0, 0, 2048, 1),
};
INSTANTIATE_TEST_CASE_P(dmSoundTestPlayTest, dmSoundTestPlayTest, jc_test_values_in(params_test_play_test));

const TestParams params_test_play_speed_test[] = {
    TestParams("default",
            MONO_TONE_440_44100_11025_WAV,
            MONO_TONE_440_44100_11025_WAV_SIZE,
            dmSound::SOUND_DATA_TYPE_WAV,
            440,
            44100,
            88200,
            2048,
            0.0f,
            2.0f,
            0       // loopcount. Increase it to stress the mix algorithm when testing by listening.
    ),
    TestParams("default",
            MONO_TONE_440_32000_8000_WAV,
            MONO_TONE_440_32000_8000_WAV_SIZE,
            dmSound::SOUND_DATA_TYPE_WAV,
            440,
            32000,
            64000,
            2048,
            0.0f,
            2.0f,
            0
    ),
    TestParams("default",
            STEREO_TONE_440_32000_8000_WAV,
            STEREO_TONE_440_32000_8000_WAV_SIZE,
            dmSound::SOUND_DATA_TYPE_WAV,
            440,
            32000,
            64000,
            2048,
            0.0f,
            2.0f,
            0
    ),
    TestParams("default",
            MONO_TONE_440_44100_11025_WAV,
            MONO_TONE_440_44100_11025_WAV_SIZE,
            dmSound::SOUND_DATA_TYPE_WAV,
            440,
            44100,
            88200,
            2048,
            0.0f,
            0.5f,
            0
    ),
    TestParams("default",
            STEREO_TONE_440_32000_8000_WAV,
            STEREO_TONE_440_32000_8000_WAV_SIZE,
            dmSound::SOUND_DATA_TYPE_WAV,
            440,
            32000,
            64000,
            2048,
            0.0f,
            0.5f,
            0
    ),
    TestParams("default",
            MONO_TONE_440_44100_11025_WAV,
            MONO_TONE_440_44100_11025_WAV_SIZE,
            dmSound::SOUND_DATA_TYPE_WAV,
            440,
            44100,
            88200,
            2048,
            0.0f,
            2.159f,     // float speed - this would result in crackling sounds in #5613
            5           // loop 5 times
    ),
};
INSTANTIATE_TEST_CASE_P(dmSoundTestPlaySpeedTest, dmSoundTestPlaySpeedTest, jc_test_values_in(params_test_play_speed_test));
#endif

#if !defined(GITHUB_CI) || (defined(GITHUB_CI) && !defined(WIN32))
TEST_P(dmSoundVerifyWavTest, Mix)
{
    TestParams params = GetParam();
    dmSound::Result r;
    dmSound::HSoundData sd = 0;

    dmSound::NewSoundData(params.m_Sound, params.m_SoundSize, params.m_Type, &sd, 1234);

    uint64_t tstart = dmTime::GetMonotonicTime();

    dmSound::HSoundInstance instance = 0;
    r = dmSound::NewSoundInstance(sd, &instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    ASSERT_NE((dmSound::HSoundInstance) 0, instance);

    uint64_t tend = dmTime::GetMonotonicTime();
    ASSERT_GT(250u, tend-tstart);

    r = dmSound::Play(instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    do {
        r = dmSound::Update();
        ASSERT_EQ(dmSound::RESULT_OK, r);
    } while (dmSound::IsPlaying(instance));

    r = dmSound::DeleteSoundInstance(instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    r = dmSound::DeleteSoundData(sd);
    ASSERT_EQ(dmSound::RESULT_OK, r);
}

const TestParams params_verify_wav_test[] = {TestParams("loopback",
                                            DEF2938_WAV,
                                            DEF2938_WAV_SIZE,
                                            dmSound::SOUND_DATA_TYPE_WAV,
                                            2000,
                                            22050,
                                            35200,
                                            2048,
                                            0)};
INSTANTIATE_TEST_CASE_P(dmSoundVerifyWavTest, dmSoundVerifyWavTest, jc_test_values_in(params_verify_wav_test));
#endif

#if !defined(GITHUB_CI) || (defined(GITHUB_CI) && !defined(WIN32))
TEST_P(dmSoundMixerTest, Mixer)
{
    TestParams2 params = GetParam();
    dmSound::Result r;
    dmSound::HSoundData sd1 = 0;
    dmSound::HSoundData sd2 = 0;
    dmSound::NewSoundData(params.m_Sound1, params.m_SoundSize1, params.m_Type1, &sd1, 1234);
    dmSound::NewSoundData(params.m_Sound2, params.m_SoundSize2, params.m_Type2, &sd2, 1234);

    printf("tone1: %d, rate1: %d, frames1: %d, tone2: %d, rate2: %d, frames2: %d\n",
            params.m_ToneRate1, params.m_MixRate1, params.m_FrameCount1,
            params.m_ToneRate2, params.m_MixRate2, params.m_FrameCount2);


    float master_gain = 0.9f;

    r = dmSound::SetGroupGain(dmHashString64("master"), master_gain);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    master_gain = GainToScale(master_gain);

    r = dmSound::AddGroup("g1");
    ASSERT_EQ(dmSound::RESULT_OK, r);

    r = dmSound::AddGroup("g2");
    ASSERT_EQ(dmSound::RESULT_OK, r);


    dmhash_t groups[dmSound::MAX_GROUPS];
    uint32_t group_count = dmSound::MAX_GROUPS;
    dmSound::GetGroupHashes(&group_count, groups);

    ASSERT_EQ(3, group_count);
    std::set<dmhash_t> testgroups;
    for (uint32_t i = 0; i < group_count; i++) {
        testgroups.insert(groups[i]);
    }

    ASSERT_TRUE(testgroups.find(dmHashString64("master")) != testgroups.end());
    ASSERT_TRUE(testgroups.find(dmHashString64("g1")) != testgroups.end());
    ASSERT_TRUE(testgroups.find(dmHashString64("g2")) != testgroups.end());

    r = dmSound::SetGroupGain(dmHashString64("g1"), params.m_Gain1);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    r = dmSound::SetGroupGain(dmHashString64("g2"), params.m_Gain2);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    float gain1 = GainToScale(params.m_Gain1);
    float gain2 = GainToScale(params.m_Gain2);

    dmSound::HSoundInstance instance1 = 0;
    dmSound::HSoundInstance instance2 = 0;
    r = dmSound::NewSoundInstance(sd1, &instance1);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    ASSERT_NE((dmSound::HSoundInstance) 0, instance1);
    r = dmSound::SetInstanceGroup(instance1, "g1");
    ASSERT_EQ(dmSound::RESULT_OK, r);

    r = dmSound::NewSoundInstance(sd2, &instance2);
    r = dmSound::NewSoundInstance(sd2, &instance2);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    ASSERT_NE((dmSound::HSoundInstance) 0, instance2);
    r = dmSound::SetInstanceGroup(instance2, "g2");
    ASSERT_EQ(dmSound::RESULT_OK, r);

    r = dmSound::Play(instance1);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    r = dmSound::Play(instance2);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    do {
        r = dmSound::Update();
        if (!GetParam().m_UseThread)
            ASSERT_EQ(dmSound::RESULT_OK, r);
        dmTime::Sleep(1);
    } while (dmSound::IsPlaying(instance1) || dmSound::IsPlaying(instance2));

    r = dmSound::DeleteSoundInstance(instance1);
    r = dmSound::DeleteSoundInstance(instance2);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    const uint32_t frame_count1 = params.m_FrameCount1;
    const uint32_t frame_count2 = params.m_FrameCount2;
    const float mix_rate1 = params.m_MixRate1;
    const float mix_rate2 = params.m_MixRate2;

    const double f = 44100.0;
    const double level = sin(M_PI_4);                       // center panning introduces this

    const int n1 = (int)(frame_count1 * (f / mix_rate1));
    const int n2 = (int)(frame_count2 * (f / mix_rate2));
    const int n = dmMath::Max(n1, n2);


    float sum_square = 0.0f;
    float last_rms = 0.0f;

    uint64_t delta1 = (uint64_t)(mix_rate1 * (1UL << dmSound::RESAMPLE_FRACTION_BITS) / f);
    uint64_t pos1 = 0;
    uint64_t delta2 = (uint64_t)(mix_rate2 * (1UL << dmSound::RESAMPLE_FRACTION_BITS) / f);
    uint64_t pos2 = 0;
    for (int32_t i = 0; i < n - 1; i++) {

        // Generate comparison data for G1 output
        double ax = GenAndMixTone(pos1, params.m_ToneRate1, params.m_MixRate1, f, frame_count1, params.m_Ramp1, level * gain1);
        // Generate comparison data for G2 output
        double ay = GenAndMixTone(pos2, params.m_ToneRate2, params.m_MixRate2, f, frame_count2, params.m_Ramp2, level * gain2);

        pos1 += delta1;
        pos2 += delta2;

        // Mix groups and apply master gain (& clamp)
        double a = dmMath::Clamp((ax + ay) * master_gain, -32768.0, 32767.0);

        // Note: yes, we do this only for the output from G1
        sum_square += ax * ax;
        if (i % params.m_BufferFrameCount == 0 && i > 0) {
            last_rms = sqrtf(sum_square / (float) params.m_BufferFrameCount);
            sum_square = 0.0f;
            sum_square = 0.0f;
        }

        // Check with actual output...
        // Check with actual output...
        int16_t as = (int16_t) a;

        const int abs_error = 36;
        if ((uint32_t)i > params.m_BufferFrameCount * 2) {
            ASSERT_NEAR(g_LoopbackDevice->m_AllOutput[2 * i], as, abs_error);
            ASSERT_NEAR(g_LoopbackDevice->m_AllOutput[2 * i + 1], as, abs_error);
            ASSERT_NEAR(g_LoopbackDevice->m_AllOutput[2 * i], as, abs_error);
            ASSERT_NEAR(g_LoopbackDevice->m_AllOutput[2 * i + 1], as, abs_error);
        }
    }
    last_rms /= 32767.0f;

    float rms_left, rms_right;
    dmSound::GetGroupRMS(dmHashString64("g1"), params.m_BufferFrameCount / f, &rms_left, &rms_right);
    dmSound::GetGroupRMS(dmHashString64("g1"), params.m_BufferFrameCount / f, &rms_left, &rms_right);
    const float rms_tol = 3.0f;
    ASSERT_NEAR(rms_left, last_rms, rms_tol);
    ASSERT_NEAR(rms_right, last_rms, rms_tol);

    ASSERT_EQ(0, g_LoopbackDevice->m_AllOutput.Size() % 2);
    for (uint32_t i = 2 * n; i < g_LoopbackDevice->m_AllOutput.Size() / 2; ++i) {
        ASSERT_EQ(0, g_LoopbackDevice->m_AllOutput[2 * i]);
        ASSERT_EQ(0, g_LoopbackDevice->m_AllOutput[2 * i + 1]);
    }

    r = dmSound::DeleteSoundData(sd1);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    r = dmSound::DeleteSoundData(sd2);
    ASSERT_EQ(dmSound::RESULT_OK, r);
}

const TestParams2 params_mixer_test[] = {
    TestParams2("loopback",
                MONO_TONE_440_22050_44100_WAV,
                MONO_TONE_440_22050_44100_WAV_SIZE,
                dmSound::SOUND_DATA_TYPE_WAV,
                440,
                22050,
                44100,
                0.8f,
                false,

                MONO_TONE_440_22050_44100_WAV,
                MONO_TONE_440_22050_44100_WAV_SIZE,
                dmSound::SOUND_DATA_TYPE_WAV,
                440,
                22050,
                44100,
                0.3f,
                false,

                2048,
                false),

    TestParams2("loopback",
                MONO_TONE_440_22050_44100_WAV,
                MONO_TONE_440_22050_44100_WAV_SIZE,
                dmSound::SOUND_DATA_TYPE_WAV,
                440,
                22050,
                44100,
                0.6f,
                false,

                MONO_TONE_440_32000_64000_WAV,
                MONO_TONE_440_32000_64000_WAV_SIZE,
                dmSound::SOUND_DATA_TYPE_WAV,
                440,
                32000,
                64000,
                0.4f,
                false,

                2048,
                false),

    TestParams2("loopback",
                MONO_TONERAMP_440_32000_64000_WAV,
                MONO_TONERAMP_440_32000_64000_WAV_SIZE,
                dmSound::SOUND_DATA_TYPE_WAV,
                440,
                32000,
                64000,
                0.6f,
                true,

                MONO_TONERAMP_440_32000_64000_WAV,
                MONO_TONERAMP_440_32000_64000_WAV_SIZE,
                dmSound::SOUND_DATA_TYPE_WAV,
                440,
                32000,
                64000,
                0.6f,
                true,

                2048,
                false),

    // Threaded
    TestParams2("loopback",
                MONO_TONE_440_22050_44100_WAV,
                MONO_TONE_440_22050_44100_WAV_SIZE,
                dmSound::SOUND_DATA_TYPE_WAV,
                440,
                22050,
                44100,
                0.8f,
                false,

                MONO_TONE_440_22050_44100_WAV,
                MONO_TONE_440_22050_44100_WAV_SIZE,
                dmSound::SOUND_DATA_TYPE_WAV,
                440,
                22050,
                44100,
                0.3f,
                false,

                2048,
                true)
};
INSTANTIATE_TEST_CASE_P(dmSoundMixerTest, dmSoundMixerTest, jc_test_values_in(params_mixer_test));
#endif

DM_DECLARE_SOUND_DEVICE(LoopBackDevice, "loopback", DeviceLoopbackOpen, DeviceLoopbackClose, DeviceLoopbackQueue,
                        DeviceLoopbackFreeBufferSlots, 0, DeviceLoopbackDeviceInfo, DeviceLoopbackRestart, DeviceLoopbackStop);

extern "C" void dmExportedSymbols();

static uint8_t* ReadFile(const char* path, uint32_t* file_size)
{
    FILE* f = fopen(path, "rb");
    if (!f)
    {
        dmLogError("Failed to load file %s", path);
        return 0;
    }

    if (fseek(f, 0, SEEK_END) != 0)
    {
        fclose(f);
        dmLogError("Failed to seek to end of file %s", path);
        return 0;
    }

    size_t size = (size_t)ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0)
    {
        fclose(f);
        dmLogError("Failed to seek to start of file %s", path);
        return 0;
    }

    uint8_t* buffer;
    dmMemory::AlignedMalloc((void**)&buffer, 16, size);

    size_t nread = fread(buffer, 1, size, f);
    fclose(f);

    if (nread != size)
    {
        dmMemory::AlignedFree((void*)buffer);
        dmLogError("Failed to read %u bytes from file %s", (uint32_t)size, path);
        return 0;
    }

    *file_size = size;
    return buffer;
}

static const char* FindSoundFile(int argc, char** argv, dmSound::SoundDataType* type)
{
    for (int i = 0; i < argc; ++i)
    {
        const char* arg = argv[i];
        const char* suffix = strrchr(arg, '.');
        if (!suffix)
            continue;
        if (dmStrCaseCmp(suffix, ".wav")==0)
        {
            *type = dmSound::SOUND_DATA_TYPE_WAV;
            return arg;
        }

        if (dmStrCaseCmp(suffix, ".ogg")==0)
        {
            *type = dmSound::SOUND_DATA_TYPE_OGG_VORBIS;
            return arg;
        }

        if (dmStrCaseCmp(suffix, ".opus")==0)
        {
            *type = dmSound::SOUND_DATA_TYPE_OPUS;
            return arg;
        }
    }
    return 0;
}

#define CHECK_ERROR(RESULT) \
    if (dmSound::RESULT_OK != RESULT) { \
        fprintf(stderr, "Error: %d on line %d\n", RESULT, __LINE__); \
    }

static int PlaySound(const char* path, dmSound::SoundDataType type)
{
    uint32_t sound_size;
    uint8_t* sound_data = ReadFile(path, &sound_size);
    if (!sound_data)
    {
        fprintf(stderr, "Failed to read file '%s'\n", path);
        return 1;
    }
    printf("Found '%s' of type %d\n", path, type);

    dmSound::InitializeParams params;
    params.m_MaxBuffers = MAX_BUFFERS;
    params.m_MaxSources = MAX_SOURCES;
    params.m_OutputDevice = "default";
    params.m_FrameCount = 2048;//512;//GetParam().m_BufferFrameCount;
    params.m_UseThread = true; // all our desktop platforms support this

    dmSound::Result r = dmSound::Initialize(0, &params);
    CHECK_ERROR(r);

    dmSound::HSoundData sd = 0;
    dmSound::NewSoundData(sound_data, sound_size, type, &sd, 1234);

    dmSound::HSoundInstance instance = 0;
    r = dmSound::NewSoundInstance(sd, &instance);
    CHECK_ERROR(r);

    r = dmSound::Play(instance);
    CHECK_ERROR(r);

    printf("Playing sound %s: %d\n", path, (int)dmSound::IsPlaying(instance));

    do {
        r = dmSound::Update();
    } while (dmSound::IsPlaying(instance));

    dmTime::Sleep(1000000);

    r = dmSound::DeleteSoundInstance(instance);
    CHECK_ERROR(r);

    r = dmSound::Finalize();
    CHECK_ERROR(r);

    dmMemory::AlignedFree(sound_data);
    return 0;
}

int main(int argc, char **argv)
{
    dmExportedSymbols();

    dmSound::SoundDataType sound_type;
    const char* sound_file = FindSoundFile(argc, argv, &sound_type);
    if (sound_file)
    {
        return PlaySound(sound_file, sound_type);
    }

    jc_test_init(&argc, argv);
    return jc_test_run_all();
}

// New tests for start_time/start_frame offset support

TEST(SoundStartOffset, FrameIndependentOfSpeed)
{
    dmSound::InitializeParams params;
    params.m_MaxBuffers = MAX_BUFFERS;
    params.m_MaxSources = MAX_SOURCES;
    params.m_OutputDevice = "loopback";
    params.m_FrameCount = 2048;
    params.m_UseThread = false;

    ASSERT_EQ(dmSound::RESULT_OK, dmSound::Initialize(0, &params));

    dmSound::HSoundData sd = 0;
    ASSERT_EQ(dmSound::RESULT_OK, dmSound::NewSoundData(MONO_TONE_440_44100_88200_WAV, MONO_TONE_440_44100_88200_WAV_SIZE, dmSound::SOUND_DATA_TYPE_WAV, &sd, dmHashString64("startoffset_wav")));

    dmSound::HSoundInstance a = 0, b = 0;
    ASSERT_EQ(dmSound::RESULT_OK, dmSound::NewSoundInstance(sd, &a));
    ASSERT_EQ(dmSound::RESULT_OK, dmSound::NewSoundInstance(sd, &b));

    // Different speeds should not affect starting offset
    ASSERT_EQ(dmSound::RESULT_OK, dmSound::SetParameter(a, dmSound::PARAMETER_SPEED, dmVMath::Vector4(0.5f,0,0,0)));
    ASSERT_EQ(dmSound::RESULT_OK, dmSound::SetParameter(b, dmSound::PARAMETER_SPEED, dmVMath::Vector4(2.0f,0,0,0)));

    const uint32_t start_frame = 22050; // 0.5 seconds @ 44100
    ASSERT_EQ(dmSound::RESULT_OK, dmSound::SetStartFrame(a, start_frame));
    ASSERT_EQ(dmSound::RESULT_OK, dmSound::SetStartFrame(b, start_frame));

    int64_t posa = dmSound::GetInternalPos(a);
    int64_t posb = dmSound::GetInternalPos(b);

    ASSERT_EQ((int64_t)start_frame, posa);
    ASSERT_EQ((int64_t)start_frame, posb);

    ASSERT_EQ(dmSound::RESULT_OK, dmSound::DeleteSoundInstance(a));
    ASSERT_EQ(dmSound::RESULT_OK, dmSound::DeleteSoundInstance(b));
    ASSERT_EQ(dmSound::RESULT_OK, dmSound::DeleteSoundData(sd));
    ASSERT_EQ(dmSound::RESULT_OK, dmSound::Finalize());
}

TEST(SoundStartOffset, TimeIndependentOfSpeed)
{
    dmSound::InitializeParams params;
    params.m_MaxBuffers = MAX_BUFFERS;
    params.m_MaxSources = MAX_SOURCES;
    params.m_OutputDevice = "loopback";
    params.m_FrameCount = 2048;
    params.m_UseThread = false;

    ASSERT_EQ(dmSound::RESULT_OK, dmSound::Initialize(0, &params));

    dmSound::HSoundData sd = 0;
    ASSERT_EQ(dmSound::RESULT_OK, dmSound::NewSoundData(MONO_TONE_440_44100_88200_WAV, MONO_TONE_440_44100_88200_WAV_SIZE, dmSound::SOUND_DATA_TYPE_WAV, &sd, dmHashString64("startoffset_time_wav")));

    dmSound::HSoundInstance a = 0, b = 0;
    ASSERT_EQ(dmSound::RESULT_OK, dmSound::NewSoundInstance(sd, &a));
    ASSERT_EQ(dmSound::RESULT_OK, dmSound::NewSoundInstance(sd, &b));

    ASSERT_EQ(dmSound::RESULT_OK, dmSound::SetParameter(a, dmSound::PARAMETER_SPEED, dmVMath::Vector4(0.5f,0,0,0)));
    ASSERT_EQ(dmSound::RESULT_OK, dmSound::SetParameter(b, dmSound::PARAMETER_SPEED, dmVMath::Vector4(2.0f,0,0,0)));

    const float start_time = 0.5f; // 0.5 seconds @ 44100 -> 22050 frames
    ASSERT_EQ(dmSound::RESULT_OK, dmSound::SetStartTime(a, start_time));
    ASSERT_EQ(dmSound::RESULT_OK, dmSound::SetStartTime(b, start_time));

    int64_t posa = dmSound::GetInternalPos(a);
    int64_t posb = dmSound::GetInternalPos(b);

    ASSERT_EQ(22050, posa);
    ASSERT_EQ(22050, posb);

    ASSERT_EQ(dmSound::RESULT_OK, dmSound::DeleteSoundInstance(a));
    ASSERT_EQ(dmSound::RESULT_OK, dmSound::DeleteSoundInstance(b));
    ASSERT_EQ(dmSound::RESULT_OK, dmSound::DeleteSoundData(sd));
    ASSERT_EQ(dmSound::RESULT_OK, dmSound::Finalize());
}

TEST(SoundStartOffset, BeyondLengthCompletes)
{
    dmSound::InitializeParams params;
    params.m_MaxBuffers = MAX_BUFFERS;
    params.m_MaxSources = MAX_SOURCES;
    params.m_OutputDevice = "loopback";
    params.m_FrameCount = 2048;
    params.m_UseThread = false;

    ASSERT_EQ(dmSound::RESULT_OK, dmSound::Initialize(0, &params));

    dmSound::HSoundData sd = 0;
    ASSERT_EQ(dmSound::RESULT_OK, dmSound::NewSoundData(MONO_TONE_440_44100_11025_WAV, MONO_TONE_440_44100_11025_WAV_SIZE, dmSound::SOUND_DATA_TYPE_WAV, &sd, dmHashString64("startoffset_beyond_wav")));

    dmSound::HSoundInstance inst = 0;
    ASSERT_EQ(dmSound::RESULT_OK, dmSound::NewSoundInstance(sd, &inst));

    // Set start offset well beyond the clip length
    ASSERT_EQ(dmSound::RESULT_OK, dmSound::SetStartFrame(inst, 20000));

    ASSERT_EQ(dmSound::RESULT_OK, dmSound::Play(inst));

    // Run a few updates; instance should quickly end
    for (int i = 0; i < 8; ++i)
    {
        dmSound::Update();
    }
    ASSERT_FALSE(dmSound::IsPlaying(inst));

    // Ensure no error log on delete
    dmSound::Stop(inst);
    // Ensure no error log on delete
    dmSound::Stop(inst);
    ASSERT_EQ(dmSound::RESULT_OK, dmSound::DeleteSoundInstance(inst));
    ASSERT_EQ(dmSound::RESULT_OK, dmSound::DeleteSoundData(sd));
    ASSERT_EQ(dmSound::RESULT_OK, dmSound::Finalize());
}

#if !defined(GITHUB_CI) || (defined(GITHUB_CI) && !defined(WIN32))
class dmSoundTestStartTimePlayTest : public dmSoundTest
{
};

TEST_P(dmSoundTestStartTimePlayTest, StartTime)
{
    TestParams params = GetParam();
    dmSound::Result r;
    dmSound::HSoundData sd = 0;
    dmSound::NewSoundData(params.m_Sound, params.m_SoundSize, params.m_Type, &sd, 1234);

    dmSound::HSoundInstance instance = 0;
    r = dmSound::NewSoundInstance(sd, &instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    ASSERT_NE((dmSound::HSoundInstance) 0, instance);

    // Start at 1.585 seconds
    r = dmSound::SetStartTime(instance, 1.618f);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    r = dmSound::SetParameter(instance, dmSound::PARAMETER_GAIN, dmVMath::Vector4(0.8f,0,0,0));
    ASSERT_EQ(dmSound::RESULT_OK, r);

    r = dmSound::Play(instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    do {
        r = dmSound::Update();
        ASSERT_EQ(dmSound::RESULT_OK, r);
    } while (dmSound::IsPlaying(instance));

    r = dmSound::DeleteSoundInstance(instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    r = dmSound::DeleteSoundData(sd);
    ASSERT_EQ(dmSound::RESULT_OK, r);
}

const TestParams params_starttime_play_test[] = {
    TestParams("default", MONO_RESAMPLE_FRAMECOUNT_16000_OGG, MONO_RESAMPLE_FRAMECOUNT_16000_OGG_SIZE, dmSound::SOUND_DATA_TYPE_OGG_VORBIS, 0, 0, 0, 2048, 1),
    TestParams("default", MONO_RESAMPLE_FRAMECOUNT_16000_ADPCM_WAV, MONO_RESAMPLE_FRAMECOUNT_16000_ADPCM_WAV_SIZE, dmSound::SOUND_DATA_TYPE_WAV, 0, 0, 0, 2048, 1),
};
INSTANTIATE_TEST_CASE_P(dmSoundTestStartTimePlayTest, dmSoundTestStartTimePlayTest, jc_test_values_in(params_starttime_play_test));
#endif

TEST(SoundSdk, MasterMuteSilencesWithoutChangingGain)
{
    dmSound::InitializeParams params;
    dmSound::SetDefaultInitializeParams(&params);
    params.m_FrameCount   = 2048;
    params.m_UseThread    = false;
    params.m_OutputDevice = "loopback";

    ASSERT_EQ(dmSound::RESULT_OK, dmSound::Initialize(nullptr, &params));

    const dmhash_t master_hash = dmHashString64("master");
    float master_gain = 0.0f;
    ASSERT_EQ(dmSound::RESULT_OK, dmSound::GetGroupGain(master_hash, &master_gain));
    EXPECT_NEAR(1.0f, master_gain, 1e-6f);
    EXPECT_FALSE(dmSound::IsGroupMuted(master_hash));

    EXPECT_EQ(dmSound::RESULT_OK, dmSound::SetGroupMute(master_hash, true));
    EXPECT_TRUE(dmSound::IsGroupMuted(master_hash));
    ASSERT_EQ(dmSound::RESULT_OK, dmSound::GetGroupGain(master_hash, &master_gain));
    EXPECT_NEAR(1.0f, master_gain, 1e-6f);

    EXPECT_EQ(dmSound::RESULT_OK, dmSound::ToggleGroupMute(master_hash));
    EXPECT_FALSE(dmSound::IsGroupMuted(master_hash));
    ASSERT_EQ(dmSound::RESULT_OK, dmSound::GetGroupGain(master_hash, &master_gain));
    EXPECT_NEAR(1.0f, master_gain, 1e-6f);

    ASSERT_EQ(dmSound::RESULT_OK, dmSound::Finalize());
}

TEST(SoundSdk, MasterMuteRestoresPreviousGain)
{
    dmSound::InitializeParams params;
    dmSound::SetDefaultInitializeParams(&params);
    params.m_FrameCount   = 2048;
    params.m_UseThread    = false;
    params.m_OutputDevice = "loopback";

    ASSERT_EQ(dmSound::RESULT_OK, dmSound::Initialize(nullptr, &params));

    const dmhash_t master_hash = dmHashString64("master");
    ASSERT_EQ(dmSound::RESULT_OK, dmSound::SetGroupGain(master_hash, 0.35f));

    EXPECT_EQ(dmSound::RESULT_OK, dmSound::SetGroupMute(master_hash, true));
    EXPECT_TRUE(dmSound::IsGroupMuted(master_hash));
    EXPECT_EQ(dmSound::RESULT_OK, dmSound::SetGroupMute(master_hash, false));
    EXPECT_FALSE(dmSound::IsGroupMuted(master_hash));

    float master_gain = 0.0f;
    ASSERT_EQ(dmSound::RESULT_OK, dmSound::GetGroupGain(master_hash, &master_gain));
    EXPECT_NEAR(0.35f, master_gain, 1e-6f);

    ASSERT_EQ(dmSound::RESULT_OK, dmSound::Finalize());
}

TEST(SoundSdk, GroupMuteSilencesWithoutChangingGain)
{
    dmSound::InitializeParams params;
    dmSound::SetDefaultInitializeParams(&params);
    params.m_FrameCount   = 2048;
    params.m_UseThread    = false;
    params.m_OutputDevice = "loopback";

    ASSERT_EQ(dmSound::RESULT_OK, dmSound::Initialize(nullptr, &params));

    ASSERT_EQ(dmSound::RESULT_OK, dmSound::AddGroup("ads"));
    const dmhash_t group_hash = dmHashString64("ads");
    ASSERT_EQ(dmSound::RESULT_OK, dmSound::SetGroupGain(group_hash, 0.25f));

    EXPECT_EQ(dmSound::RESULT_OK, dmSound::SetGroupMute(group_hash, true));
    float gain = 1.0f;
    ASSERT_EQ(dmSound::RESULT_OK, dmSound::GetGroupGain(group_hash, &gain));
    EXPECT_NEAR(0.25f, gain, 1e-6f);
    EXPECT_TRUE(dmSound::IsGroupMuted(group_hash));

    EXPECT_EQ(dmSound::RESULT_OK, dmSound::SetGroupMute(group_hash, false));
    ASSERT_EQ(dmSound::RESULT_OK, dmSound::GetGroupGain(group_hash, &gain));
    EXPECT_NEAR(0.25f, gain, 1e-6f);
    EXPECT_FALSE(dmSound::IsGroupMuted(group_hash));

    ASSERT_EQ(dmSound::RESULT_OK, dmSound::Finalize());
}

TEST(SoundSdk, GroupMuteDefaultsToUnityGainWhenMuted)
{
    dmSound::InitializeParams params;
    dmSound::SetDefaultInitializeParams(&params);
    params.m_FrameCount   = 2048;
    params.m_UseThread    = false;
    params.m_OutputDevice = "loopback";

    ASSERT_EQ(dmSound::RESULT_OK, dmSound::Initialize(nullptr, &params));

    ASSERT_EQ(dmSound::RESULT_OK, dmSound::AddGroup("ui"));
    const dmhash_t group_hash = dmHashString64("ui");
    ASSERT_EQ(dmSound::RESULT_OK, dmSound::SetGroupGain(group_hash, 0.0f));

    EXPECT_EQ(dmSound::RESULT_OK, dmSound::SetGroupMute(group_hash, false));
    float gain = 0.0f;
    ASSERT_EQ(dmSound::RESULT_OK, dmSound::GetGroupGain(group_hash, &gain));
    EXPECT_NEAR(0.0f, gain, 1e-6f);
    EXPECT_FALSE(dmSound::IsGroupMuted(group_hash));

    ASSERT_EQ(dmSound::RESULT_OK, dmSound::Finalize());
}

TEST(SoundSdk, GroupToggleMute)
{
    dmSound::InitializeParams params;
    dmSound::SetDefaultInitializeParams(&params);
    params.m_FrameCount   = 2048;
    params.m_UseThread    = false;
    params.m_OutputDevice = "loopback";

    ASSERT_EQ(dmSound::RESULT_OK, dmSound::Initialize(nullptr, &params));

    ASSERT_EQ(dmSound::RESULT_OK, dmSound::AddGroup("amb"));
    const dmhash_t group_hash = dmHashString64("amb");

    EXPECT_FALSE(dmSound::IsGroupMuted(group_hash));

    EXPECT_EQ(dmSound::RESULT_OK, dmSound::ToggleGroupMute(group_hash));
    EXPECT_TRUE(dmSound::IsGroupMuted(group_hash));

    EXPECT_EQ(dmSound::RESULT_OK, dmSound::ToggleGroupMute(group_hash));
    EXPECT_FALSE(dmSound::IsGroupMuted(group_hash));

    const dmhash_t missing_hash = dmHashString64("missing-group");
    EXPECT_EQ(dmSound::RESULT_NO_SUCH_GROUP, dmSound::ToggleGroupMute(missing_hash));
    EXPECT_FALSE(dmSound::IsGroupMuted(missing_hash));

    ASSERT_EQ(dmSound::RESULT_OK, dmSound::Finalize());
}
