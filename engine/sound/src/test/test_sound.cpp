// Copyright 2020-2024 The Defold Foundation
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
#include <map>
#include <set>
#include <vector>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include <dlib/array.h>
#include <dlib/hash.h>
#include <dlib/message.h>
#include <dlib/log.h>
#include <dlib/time.h>
#include <dlib/math.h>
#include "../sound.h"
#include "../sound_private.h"
#include "../sound_codec.h"
#include "../stb_vorbis/stb_vorbis.h"

#include "test/mono_tone_440_22050_44100.wav.embed.h"
#include "test/mono_tone_2000_22050_44100.wav.embed.h"
#include "test/mono_tone_440_32000_64000.wav.embed.h"
#include "test/mono_tone_2000_32000_64000.wav.embed.h"
#include "test/mono_tone_440_44000_88000.wav.embed.h"
#include "test/mono_tone_2000_44000_88000.wav.embed.h"
#include "test/mono_tone_440_44100_88200.wav.embed.h"
#include "test/mono_tone_2000_44100_88200.wav.embed.h"

#include "test/stereo_tone_440_22050_44100.wav.embed.h"
#include "test/stereo_tone_2000_22050_44100.wav.embed.h"
#include "test/stereo_tone_440_32000_64000.wav.embed.h"
#include "test/stereo_tone_2000_32000_64000.wav.embed.h"
#include "test/stereo_tone_440_44000_88000.wav.embed.h"
#include "test/stereo_tone_2000_44000_88000.wav.embed.h"
#include "test/stereo_tone_440_44100_88200.wav.embed.h"
#include "test/stereo_tone_2000_44100_88200.wav.embed.h"

#include "test/mono_toneramp_440_32000_64000.wav.embed.h"

#include "test/mono_resample_framecount_16000.ogg.embed.h"

#include "test/mono_dc_44100_88200.wav.embed.h"

#include "test/def2938.wav.embed.h"

extern unsigned char CLICK_TRACK_OGG[];
extern uint32_t CLICK_TRACK_OGG_SIZE;
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



struct TestParams
{
    typedef dmSound::SoundDataType SoundDataType;

    const char* m_DeviceName;
    void*       m_Sound;
    SoundDataType m_Type;
    uint32_t    m_SoundSize;
    uint32_t    m_FrameCount;
    uint32_t    m_ToneRate;
    uint32_t    m_MixRate;
    uint32_t    m_BufferFrameCount;
    float       m_Pan;
    float       m_Speed;
    uint8_t     m_Loopcount;

    TestParams(const char* device_name, void* sound, uint32_t sound_size, SoundDataType type, uint32_t tone_rate, uint32_t mix_rate, uint32_t frame_count, uint32_t buffer_frame_count)
    : m_Pan(0.0f)
    , m_Speed(1.0f)
    , m_Loopcount(0)
    {
        m_DeviceName = device_name;
        m_Sound = sound;
        m_SoundSize = sound_size;
        m_Type = type;
        m_FrameCount = frame_count;
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
        m_ToneRate = tone_rate;
        m_MixRate = mix_rate;
        m_BufferFrameCount = buffer_frame_count;
    }

};

struct TestParams2
{
    typedef dmSound::SoundDataType SoundDataType;

    const char* m_DeviceName;

    void*       m_Sound1;
    SoundDataType m_Type1;
    uint32_t    m_SoundSize1;
    uint32_t    m_FrameCount1;
    uint32_t    m_ToneRate1;
    uint32_t    m_MixRate1;
    float       m_Gain1;
    bool        m_Ramp1;

    void*       m_Sound2;
    SoundDataType m_Type2;
    uint32_t    m_SoundSize2;
    uint32_t    m_FrameCount2;
    uint32_t    m_ToneRate2;
    uint32_t    m_MixRate2;
    float       m_Gain2;
    bool        m_Ramp2;

    uint32_t    m_BufferFrameCount;
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

    virtual void SetUp()
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

    virtual void TearDown()
    {
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

    virtual void SetUp()
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

    virtual void TearDown()
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
    int              m_Time; // read cursor
    int              m_QueueTime; // write cursor
    int              m_NumWrites;
};


LoopbackDevice *g_LoopbackDevice = 0;

dmSound::Result DeviceLoopbackOpen(const dmSound::OpenDeviceParams* params, dmSound::HDevice* device)
{
    LoopbackDevice* d = new LoopbackDevice;

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
        d->m_Buffers.Push(new LoopbackBuffer(params->m_FrameCount, i));
    }

    d->m_TotalBuffersQueued = 0;
    d->m_Time = params->m_BufferCount;
    d->m_QueueTime = params->m_BufferCount;
    d->m_NumWrites = 0;

    *device = d;
    g_LoopbackDevice = d;
    return dmSound::RESULT_OK;
}

void DeviceLoopbackClose(dmSound::HDevice device)
{
    LoopbackDevice* d = (LoopbackDevice*) device;
    for (uint32_t i = 0; i < d->m_Buffers.Size(); ++i) {
        delete d->m_Buffers[i];
    }

    delete (LoopbackDevice*) device;
    g_LoopbackDevice = 0;
}

dmSound::Result DeviceLoopbackQueue(dmSound::HDevice device, const int16_t* samples, uint32_t sample_count)
{
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

uint32_t DeviceLoopbackFreeBufferSlots(dmSound::HDevice device)
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

void DeviceLoopbackDeviceInfo(dmSound::HDevice device, dmSound::DeviceInfo* info)
{
    info->m_MixRate = 44100;
}

void DeviceLoopbackRestart(dmSound::HDevice device)
{

}

void DeviceLoopbackStop(dmSound::HDevice device)
{

}

#if !defined(GITHUB_CI) || (defined(GITHUB_CI) && !(defined(WIN32) || defined(__MACH__)))
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

    // This is set up by observation: after first iteration, m_Time is 164.
    // For each loop done afterwards, another 172 is added.
    ASSERT_EQ(g_LoopbackDevice->m_Time, 164 + 172*loopcount);

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


#if !defined(GITHUB_CI) || (defined(GITHUB_CI) && !(defined(WIN32) || defined(__MACH__)))
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

    r = dmSound::Play(instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    printf("Playing: %d\n", dmSound::IsPlaying(instance));

    do {
        r = dmSound::Update();
    } while (dmSound::IsPlaying(instance));
    r = dmSound::DeleteSoundInstance(instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    const uint32_t frame_count = params.m_FrameCount;
    const float rate = params.m_ToneRate;
    const float mix_rate = params.m_MixRate;

    const int n = (frame_count * 44100) / (int) mix_rate;
    for (int32_t i = 0; i < n - 1; i++) {
        const double f = 44100.0;
        int index = i * mix_rate / f;
        double level = sin(M_PI_4);
        double a1 = 0.8 * 32768.0 * level * sin((index * 2.0 * M_PI * rate) / mix_rate);
        double a2 = 0.8 * 32768.0 * level * sin(((index + 1) * 2.0 * M_PI * rate) / mix_rate);
        double frac = fmod(i * mix_rate / 44100.0, 1.0);
        double a = a1 * (1.0 - frac) + a2 * frac;
        int16_t as = (int16_t) a;
        ASSERT_NEAR(g_LoopbackDevice->m_AllOutput[2 * i], as, 27);
        ASSERT_NEAR(g_LoopbackDevice->m_AllOutput[2 * i + 1], as, 27);
    }

    ASSERT_EQ(0u, g_LoopbackDevice->m_AllOutput.Size() % 2);
    for (uint32_t i = 2 * n; i < g_LoopbackDevice->m_AllOutput.Size() / 2; ++i) {
        ASSERT_EQ(0, g_LoopbackDevice->m_AllOutput[2 * i]);
        ASSERT_EQ(0, g_LoopbackDevice->m_AllOutput[2 * i + 1]);
    }

    float rms_left, rms_right;
    dmSound::GetGroupRMS(dmHashString64("master"), params.m_BufferFrameCount / 44100.0f, &rms_left, &rms_right);
    // Theoretical RMS for a sin-function with amplitude a is a / sqrt(2)
    ASSERT_NEAR(0.8f / sqrtf(2.0f) * 0.707107f, rms_left, 0.02f);
    ASSERT_NEAR(0.8f / sqrtf(2.0f) * 0.707107f, rms_right, 0.02f);

    float peak_left, peak_right;
    dmSound::GetGroupPeak(dmHashString64("master"), params.m_BufferFrameCount / 44100.0f, &peak_left, &peak_right);
    ASSERT_NEAR(0.8f* 0.707107f, peak_left, 0.01f);
    ASSERT_NEAR(0.8f* 0.707107f, peak_right, 0.01f);

    int expected_queued = (frame_count * 44100) / ((int) mix_rate * params.m_BufferFrameCount)
                            + dmMath::Min(1U, (frame_count * 44100) % ((int) mix_rate * params.m_BufferFrameCount));
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
            2048)};

/*
const TestParams params_verify_test[] = {
TestParams("loopback",
            MONO_TONE_440_22050_44100_WAV,
            MONO_TONE_440_22050_44100_WAV_SIZE,
            dmSound::SOUND_DATA_TYPE_WAV,
            440,
            22050,
            44100,
            2048),
TestParams("loopback",
            MONO_TONE_440_32000_64000_WAV,
            MONO_TONE_440_32000_64000_WAV_SIZE,
            dmSound::SOUND_DATA_TYPE_WAV,
            440,
            32000,
            64000,
            2048),
TestParams("loopback",
            MONO_TONE_440_44000_88000_WAV,
            MONO_TONE_440_44000_88000_WAV_SIZE,
            dmSound::SOUND_DATA_TYPE_WAV,
            440,
            44000,
            88000,
            2048),
TestParams("loopback",
            MONO_TONE_440_44100_88200_WAV,
            MONO_TONE_440_44100_88200_WAV_SIZE,
            dmSound::SOUND_DATA_TYPE_WAV,
            440,
            44100,
            88200,
            2048),
TestParams("loopback",
            MONO_TONE_2000_22050_44100_WAV,
            MONO_TONE_2000_22050_44100_WAV_SIZE,
            dmSound::SOUND_DATA_TYPE_WAV,
            2000,
            22050,
            44100,
            2048),
TestParams("loopback",
            MONO_TONE_2000_32000_64000_WAV,
            MONO_TONE_2000_32000_64000_WAV_SIZE,
            dmSound::SOUND_DATA_TYPE_WAV,
            2000,
            32000,
            64000,
            2048),
TestParams("loopback",
            MONO_TONE_2000_44000_88000_WAV,
            MONO_TONE_2000_44000_88000_WAV_SIZE,
            dmSound::SOUND_DATA_TYPE_WAV,
            2000,
            44000,
            88000,
            2048),
TestParams("loopback",
            MONO_TONE_2000_44100_88200_WAV,
            MONO_TONE_2000_44100_88200_WAV_SIZE,
            dmSound::SOUND_DATA_TYPE_WAV,
            2000,
            44100,
            88200,
            2048),
TestParams("loopback",
            STEREO_TONE_440_22050_44100_WAV,
            STEREO_TONE_440_22050_44100_WAV_SIZE,
            dmSound::SOUND_DATA_TYPE_WAV,
            440,
            22050,
            44100,
            2048),
TestParams("loopback",
            STEREO_TONE_440_32000_64000_WAV,
            STEREO_TONE_440_32000_64000_WAV_SIZE,
            dmSound::SOUND_DATA_TYPE_WAV,
            440,
            32000,
            64000,
            2048),
TestParams("loopback",
            STEREO_TONE_440_44000_88000_WAV,
            STEREO_TONE_440_44000_88000_WAV_SIZE,
            dmSound::SOUND_DATA_TYPE_WAV,
            440,
            44000,
            88000,
            2048),
TestParams("loopback",
            STEREO_TONE_440_44100_88200_WAV,
            STEREO_TONE_440_44100_88200_WAV_SIZE,
            dmSound::SOUND_DATA_TYPE_WAV,
            440,
            44100,
            88200,
            2048),
TestParams("loopback",
            STEREO_TONE_2000_22050_44100_WAV,
            STEREO_TONE_2000_22050_44100_WAV_SIZE,
            dmSound::SOUND_DATA_TYPE_WAV,
            2000,
            22050,
            44100,
            2048),
TestParams("loopback",
            STEREO_TONE_2000_32000_64000_WAV,
            STEREO_TONE_2000_32000_64000_WAV_SIZE,
            dmSound::SOUND_DATA_TYPE_WAV,
            2000,
            32000,
            64000,
            2048),
TestParams("loopback",
            STEREO_TONE_2000_44000_88000_WAV,
            STEREO_TONE_2000_44000_88000_WAV_SIZE,
            dmSound::SOUND_DATA_TYPE_WAV,
            2000,
            44000,
            88000,
            2048),
TestParams("loopback",
            STEREO_TONE_2000_44100_88200_WAV,
            STEREO_TONE_2000_44100_88200_WAV_SIZE,
            dmSound::SOUND_DATA_TYPE_WAV,
            2000,
            44100,
            88200,
            2048)
};*/

INSTANTIATE_TEST_CASE_P(dmSoundVerifyTest, dmSoundVerifyTest, jc_test_values_in(params_verify_test));


#if !defined(GITHUB_CI) || (defined(GITHUB_CI) && !(defined(WIN32) || defined(__MACH__)))
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

        frames = g_LoopbackDevice->m_AllOutput.Size() / 2;
        if (frames != prev_frames || frames == 0)
        {
            for (int i = prev_frames; i < dmMath::Min(expected_frames, frames); i++) {
                int16_t actual = g_LoopbackDevice->m_AllOutput[2 * i];
                float mix = (i - prev_frames) / (float) (frames - prev_frames);
                float expectedf = (32768.0f * 0.8f * ((1.0f - mix) * prev_g + g * mix));
                int16_t expected = (int16_t) expectedf ;
                ASSERT_NEAR(expected * 0.707107f, actual, 2U);
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
        2048)
};
INSTANTIATE_TEST_CASE_P(dmSoundTestGroupRampTest, dmSoundTestGroupRampTest, jc_test_values_in(params_group_ramp_test));
#endif

#if !defined(GITHUB_CI) || (defined(GITHUB_CI) && !(defined(WIN32) || defined(__MACH__)))
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
			MONO_TONE_440_44100_88200_WAV,		// sound
            MONO_TONE_440_44100_88200_WAV_SIZE,	// uint32_t sound_size
            dmSound::SOUND_DATA_TYPE_WAV,		// SoundDataType type
            440,								// uint32_t tone_rate
            44100,								// uint32_t mix_rate
            88200,								// uint32_t frame_count
            1999,								// uint32_t buffer_frame_count
            0.0f,								// float pan
            3.999909297f),						// float speed - this strange number will result in having remainder frames at the end of mixing
};
INSTANTIATE_TEST_CASE_P(dmSoundTestSpeedTest, dmSoundTestSpeedTest, jc_test_values_in(params_speed_test));
#endif

#if !defined(GITHUB_CI) || (defined(GITHUB_CI) && !(defined(WIN32) || defined(__MACH__)))
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

const TestParams params_verify_ogg_test[] = {TestParams("loopback",
                                            MONO_RESAMPLE_FRAMECOUNT_16000_OGG,
                                            MONO_RESAMPLE_FRAMECOUNT_16000_OGG_SIZE,
                                            dmSound::SOUND_DATA_TYPE_OGG_VORBIS,
                                            2000,
                                            44100,
                                            35200,
                                            2048)};
INSTANTIATE_TEST_CASE_P(dmSoundVerifyOggTest, dmSoundVerifyOggTest, jc_test_values_in(params_verify_ogg_test));
#endif

#if !defined(GITHUB_CI) || (defined(GITHUB_CI) && !(defined(WIN32) || defined(__MACH__)))
TEST_P(dmSoundTestPlayTest, Play)
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

    float a = 0.0f;

    r = dmSound::SetParameter(instance, dmSound::PARAMETER_PAN, dmVMath::Vector4(cosf(a),0,0,0));
    ASSERT_EQ(dmSound::RESULT_OK, r);

    ASSERT_EQ(dmSound::RESULT_OK, r);

    r = dmSound::Play(instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);

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

TEST_P(dmSoundTestPlaySpeedTest, Play)
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

    r = dmSound::SetParameter(instance, dmSound::PARAMETER_GAIN, dmVMath::Vector4(0.5f,0,0,0));
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

const TestParams params_test_play_test[] = {
    TestParams("default",
                MONO_TONE_440_32000_64000_WAV,
                MONO_TONE_440_32000_64000_WAV_SIZE,
                dmSound::SOUND_DATA_TYPE_WAV,
                440,
                32000,
                64000,
                2048),
    TestParams("default",
                MONO_TONE_440_44000_88000_WAV,
                MONO_TONE_440_44000_88000_WAV_SIZE,
                dmSound::SOUND_DATA_TYPE_WAV,
                440,
                44000,
                88000,
                2048),
    TestParams("default",
                STEREO_TONE_440_32000_64000_WAV,
                STEREO_TONE_440_32000_64000_WAV_SIZE,
                dmSound::SOUND_DATA_TYPE_WAV,
                440,
                32000,
                64000,
                2048),
    TestParams("default",
                STEREO_TONE_440_44000_88000_WAV,
                STEREO_TONE_440_44000_88000_WAV_SIZE,
                dmSound::SOUND_DATA_TYPE_WAV,
                440,
                44000,
                88000,
                2048),
    TestParams("default",
                MONO_TONE_440_44100_88200_WAV,
                MONO_TONE_440_44100_88200_WAV_SIZE,
                dmSound::SOUND_DATA_TYPE_WAV,
                440,
                44100,
                44100,
                2048),
};
INSTANTIATE_TEST_CASE_P(dmSoundTestPlayTest, dmSoundTestPlayTest, jc_test_values_in(params_test_play_test));

const TestParams params_test_play_speed_test[] = {
    TestParams("default",
            MONO_TONE_440_44100_88200_WAV,
            MONO_TONE_440_44100_88200_WAV_SIZE,
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
            MONO_TONE_440_32000_64000_WAV,
            MONO_TONE_440_32000_64000_WAV_SIZE,
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
            STEREO_TONE_440_32000_64000_WAV,
            STEREO_TONE_440_32000_64000_WAV_SIZE,
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
            MONO_TONE_440_44100_88200_WAV,
            MONO_TONE_440_44100_88200_WAV_SIZE,
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
            STEREO_TONE_440_32000_64000_WAV,
            STEREO_TONE_440_32000_64000_WAV_SIZE,
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
            MONO_TONE_440_44100_88200_WAV,
            MONO_TONE_440_44100_88200_WAV_SIZE,
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

#if !defined(GITHUB_CI) || (defined(GITHUB_CI) && !(defined(WIN32) || defined(__MACH__)))
TEST_P(dmSoundVerifyWavTest, Mix)
{
    TestParams params = GetParam();
    dmSound::Result r;
    dmSound::HSoundData sd = 0;

    dmSound::NewSoundData(params.m_Sound, params.m_SoundSize, params.m_Type, &sd, 1234);

    uint64_t tstart = dmTime::GetTime();

    dmSound::HSoundInstance instance = 0;
    r = dmSound::NewSoundInstance(sd, &instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    ASSERT_NE((dmSound::HSoundInstance) 0, instance);

    uint64_t tend = dmTime::GetTime();
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
                                            2048)};
INSTANTIATE_TEST_CASE_P(dmSoundVerifyWavTest, dmSoundVerifyWavTest, jc_test_values_in(params_verify_wav_test));
#endif

#if !defined(GITHUB_CI) || (defined(GITHUB_CI) && !(defined(WIN32) || defined(__MACH__)))
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


    float master_gain = 0.75f;

    r = dmSound::SetGroupGain(dmHashString64("master"), master_gain);
    ASSERT_EQ(dmSound::RESULT_OK, r);

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

    dmSound::HSoundInstance instance1 = 0;
    dmSound::HSoundInstance instance2 = 0;
    r = dmSound::NewSoundInstance(sd1, &instance1);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    ASSERT_NE((dmSound::HSoundInstance) 0, instance1);
    r = dmSound::SetInstanceGroup(instance1, "g1");
    ASSERT_EQ(dmSound::RESULT_OK, r);

    r = dmSound::NewSoundInstance(sd1, &instance2);
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
    const float rate1 = params.m_ToneRate1;
    const uint32_t frame_count2 = params.m_FrameCount2;
    const float rate2 = params.m_ToneRate2;
    const float mix_rate1 = params.m_MixRate1;
    const float mix_rate2 = params.m_MixRate2;

    const int n1 = (frame_count1 * 44100) / (int) mix_rate1;
    const int n2 = (frame_count2 * 44100) / (int) mix_rate2;
    const int n = dmMath::Max(n1, n2);
    float sum_square = 0.0f;
    float last_rms = 0.0f;
    for (int32_t i = 0; i < n - 1; i++) {
        const double f = 44100.0;

        double ax = 0;
        double ay = 0;

        int index1 = i * mix_rate1 / f;
        float ramp1 = params.m_Ramp1 ? ((n - 1) - i) / (float) n : 1.0f;
        double a1x = ramp1 * params.m_Gain1 * 0.8 * 32768.0 * sin((index1 * 2.0 * 3.14159265 * rate1) / mix_rate1);
        double a2x = ramp1 * params.m_Gain1 * 0.8 * 32768.0 * sin(((index1 + 1) * 2.0 * 3.14159265 * rate1) / mix_rate1);
        double frac1 = fmod(i * mix_rate1 / 44100.0, 1.0);
        ax = a1x * (1.0 - frac1) + a2x * frac1;

        int index2 = i * mix_rate2 / f;
        float ramp2 = params.m_Ramp2 ? ((n - 1) - i) / (float) n : 1.0f;
        double a1y = ramp2 * params.m_Gain2 * 0.8 * 32768.0 * sin((index2 * 2.0 * 3.14159265 * rate2) / mix_rate2);
        double a2y = ramp2 * params.m_Gain2 * 0.8 * 32768.0 * sin(((index2 + 1) * 2.0 * 3.14159265 * rate2) / mix_rate2);
        double frac2 = fmod(i * mix_rate2 / 44100.0, 1.0);
        ay = a1y * (1.0 - frac2) + a2y * frac2;

        double a = ax + ay;
        a = dmMath::Min(32767.0, a);
        a = dmMath::Max(-32768.0, a);

        sum_square += ax * ax;
        if (i % params.m_BufferFrameCount == 0 && i > 0) {
            last_rms = sqrtf(sum_square / (float) params.m_BufferFrameCount);
            sum_square =0 ;
        }

        int16_t as = (int16_t) a;

        const int abs_error = 36;
        if ((uint32_t)i > params.m_BufferFrameCount * 2) {
            ASSERT_NEAR(g_LoopbackDevice->m_AllOutput[2 * i], as * master_gain * 0.707107f, abs_error);
            ASSERT_NEAR(g_LoopbackDevice->m_AllOutput[2 * i + 1], as * master_gain * 0.707107f, abs_error);
        }
    }
    last_rms /= 32767.0f;

    float rms_left, rms_right;
    dmSound::GetGroupRMS(dmHashString64("g1"), params.m_BufferFrameCount / 44100.0f, &rms_left, &rms_right);
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
                true),
};
INSTANTIATE_TEST_CASE_P(dmSoundMixerTest, dmSoundMixerTest, jc_test_values_in(params_mixer_test));
#endif

DM_DECLARE_SOUND_DEVICE(LoopBackDevice, "loopback", DeviceLoopbackOpen, DeviceLoopbackClose, DeviceLoopbackQueue, DeviceLoopbackFreeBufferSlots, DeviceLoopbackDeviceInfo, DeviceLoopbackRestart, DeviceLoopbackStop);

extern "C" void dmExportedSymbols();

int main(int argc, char **argv)
{
    dmExportedSymbols();
    LoopBackDevice();

    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
