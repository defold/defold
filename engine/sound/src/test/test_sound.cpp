#include <stdlib.h>
#include <map>
#include <set>
#include <vector>
#include <gtest/gtest.h>
#include <dlib/hash.h>
#include <dlib/message.h>
#include <dlib/log.h>
#include <dlib/time.h>
#include <dlib/math.h>
#include "../sound.h"
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

    TestParams(const char* device_name, void* sound, uint32_t sound_size, SoundDataType type, uint32_t tone_rate, uint32_t mix_rate, uint32_t frame_count, uint32_t buffer_frame_count)
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

    TestParams2(const char* device_name,
                void* sound1, uint32_t sound_size1, SoundDataType type1, uint32_t tone_rate1, uint32_t mix_rate1, uint32_t frame_count1, float gain1, bool ramp1,
                void* sound2, uint32_t sound_size2, SoundDataType type2, uint32_t tone_rate2, uint32_t mix_rate2, uint32_t frame_count2, float gain2, bool ramp2,
                uint32_t buffer_frame_count)
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
    }
};


#define MAX_BUFFERS 32
#define MAX_SOURCES 16

class dmSoundTest : public ::testing::TestWithParam<TestParams>
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

        dmSound::Result r = dmSound::Initialize(0, &params);
        ASSERT_EQ(dmSound::RESULT_OK, r);
    }

    virtual void TearDown()
    {
        dmSound::Result r = dmSound::Finalize();
        ASSERT_EQ(dmSound::RESULT_OK, r);
    }
};

class dmSoundTest2 : public ::testing::TestWithParam<TestParams2>
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

class dmSoundVerifyOggTest : public dmSoundTest
{
};

class dmSoundTestPlayTest : public dmSoundTest
{
};

class dmSoundTestGroupRampTest : public dmSoundTest
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
    do {
        r = dmSound::Update();
        ASSERT_EQ(dmSound::RESULT_OK, r);
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
        double a1 = 0.8 * 32768.0 * sin((index * 2.0 * 3.14159265 * rate) / mix_rate);
        double a2 = 0.8 * 32768.0 * sin(((index + 1) * 2.0 * 3.14159265 * rate) / mix_rate);
        double frac = fmod(i * mix_rate / 44100.0, 1.0);
        double a = a1 * (1.0 - frac) + a2 * frac;
        int16_t as = (int16_t) a;
        ASSERT_NEAR(g_LoopbackDevice->m_AllOutput[2 * i], as, 24);
        ASSERT_NEAR(g_LoopbackDevice->m_AllOutput[2 * i + 1], as, 24);
    }

    ASSERT_EQ(0, g_LoopbackDevice->m_AllOutput.Size() % 2);
    for (uint32_t i = 2 * n; i < g_LoopbackDevice->m_AllOutput.Size() / 2; ++i) {
        ASSERT_EQ(0, g_LoopbackDevice->m_AllOutput[2 * i]);
        ASSERT_EQ(0, g_LoopbackDevice->m_AllOutput[2 * i + 1]);
    }

    float rms_left, rms_right;
    dmSound::GetGroupRMS(dmHashString64("master"), params.m_BufferFrameCount / 44100.0f, &rms_left, &rms_right);
    // Theoretical RMS for a sin-function with amplitude a is a / sqrt(2)
    ASSERT_NEAR(rms_left, 0.8f / sqrtf(2.0f), 0.02f);
    ASSERT_NEAR(rms_right, 0.8f / sqrtf(2.0f), 0.02f);

    float peak_left, peak_right;
    dmSound::GetGroupPeak(dmHashString64("master"), params.m_BufferFrameCount / 44100.0f, &peak_left, &peak_right);
    ASSERT_NEAR(peak_left, 0.8f, 0.01f);
    ASSERT_NEAR(peak_right, 0.8f, 0.01f);

    int expected_queued = (frame_count * 44100) / ((int) mix_rate * params.m_BufferFrameCount)
                            + dmMath::Min(1U, (frame_count * 44100) % ((int) mix_rate * params.m_BufferFrameCount));
    ASSERT_EQ(g_LoopbackDevice->m_TotalBuffersQueued, expected_queued);

    r = dmSound::DeleteSoundData(sd);
    ASSERT_EQ(dmSound::RESULT_OK, r);
}

INSTANTIATE_TEST_CASE_P(dmSoundVerifyTest,
                        dmSoundVerifyTest,
                        ::testing::Values(
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
                                ));


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
                ASSERT_NEAR(expected, actual, 2U);
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

INSTANTIATE_TEST_CASE_P(dmSoundTestGroupRampTest,
        dmSoundTestGroupRampTest,
                        ::testing::Values(
                                TestParams("loopback",
                                            MONO_DC_44100_88200_WAV,
                                            MONO_DC_44100_88200_WAV_SIZE,
                                            dmSound::SOUND_DATA_TYPE_WAV,
                                            0,
                                            44100,
                                            88200,
                                            2048)
                                ));

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

INSTANTIATE_TEST_CASE_P(dmSoundVerifyOggTest,
                        dmSoundVerifyOggTest,
                        ::testing::Values(
                                TestParams("loopback",
                                            MONO_RESAMPLE_FRAMECOUNT_16000_OGG,
                                            MONO_RESAMPLE_FRAMECOUNT_16000_OGG_SIZE,
                                            dmSound::SOUND_DATA_TYPE_OGG_VORBIS,
                                            2000,
                                            44100,
                                            35200,
                                            2048)
                                ));

INSTANTIATE_TEST_CASE_P(dmSoundTestPlayTest,
                        dmSoundTestPlayTest,
                        ::testing::Values(
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
                                            2048)
                                ));

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

    ASSERT_EQ(3, dmSound::GetGroupCount());
    std::set<dmhash_t> groups;
    for (uint32_t i = 0; i < dmSound::GetGroupCount(); i++) {
        dmhash_t group = 0;
        dmSound::GetGroupHash(i, &group);
        groups.insert(group);
    }

    ASSERT_TRUE(groups.find(dmHashString64("master")) != groups.end());
    ASSERT_TRUE(groups.find(dmHashString64("g1")) != groups.end());
    ASSERT_TRUE(groups.find(dmHashString64("g2")) != groups.end());

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
        ASSERT_EQ(dmSound::RESULT_OK, r);

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
            ASSERT_NEAR(g_LoopbackDevice->m_AllOutput[2 * i], as * master_gain, abs_error);
            ASSERT_NEAR(g_LoopbackDevice->m_AllOutput[2 * i + 1], as * master_gain, abs_error);
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

INSTANTIATE_TEST_CASE_P(dmSoundMixerTest,
                        dmSoundMixerTest,
                        ::testing::Values(
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

                                            2048),

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

                                            2048),

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

                                            2048)

                        ));


DM_DECLARE_SOUND_DEVICE(LoopBackDevice, "loopback", DeviceLoopbackOpen, DeviceLoopbackClose, DeviceLoopbackQueue, DeviceLoopbackFreeBufferSlots, DeviceLoopbackDeviceInfo, DeviceLoopbackRestart, DeviceLoopbackStop);

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
