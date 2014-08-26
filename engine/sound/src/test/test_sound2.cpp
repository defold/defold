#include <stdlib.h>
#include <map>
#include <vector>
#include <gtest/gtest.h>
#include <dlib/hash.h>
#include <dlib/message.h>
#include <dlib/log.h>
#include <dlib/time.h>
#include <dlib/math.h>
#include "../sound.h"
#include "../sound2.h"
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

#include "test/mono_resample_framecount_16000.ogg.embed.h"

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

class dmSoundTest : public ::testing::TestWithParam<TestParams>
{
public:
    void*    m_DrumLoop;
    uint32_t m_DrumLoopSize;

    void*    m_OneFootStep;
    uint32_t m_OneFootStepSize;

    void*    m_LayerGuitarA;
    uint32_t m_LayerGuitarASize;

    void*    m_ClickTrack;
    uint32_t m_ClickTrackSize;

    void*    m_SineWave;
    uint32_t m_SineWaveSize;

    void*    m_DoorOpening;
    uint32_t m_DoorOpeningSize;

    const char* m_DeviceName;

    dmSoundTest() {
        m_DrumLoop = 0;
        m_DrumLoopSize = 0;
        m_OneFootStep = 0;
        m_OneFootStepSize = 0;
        m_LayerGuitarA = 0;
        m_LayerGuitarASize = 0;
        m_ClickTrack = 0;
        m_ClickTrackSize = 0;
        m_SineWave = 0;
        m_SineWaveSize = 0;
        m_DoorOpening = 0;
        m_DoorOpeningSize = 0;

        m_DeviceName = GetParam().m_DeviceName;
    }

    void LoadFile(const char* file_name, void** buffer, uint32_t* size)
    {
        FILE* f = fopen(file_name, "rb");
        if (!f)
        {
            dmLogError("Unable to load: %s", file_name);
            exit(5);
        }

        fseek(f, 0, SEEK_END);
        *size = (uint32_t) ftell(f);
        fseek(f, 0, SEEK_SET);

        *buffer = malloc(*size);
        fread(*buffer, 1, *size, f);

        fclose(f);
    }

#define MAX_BUFFERS 32
#define MAX_SOURCES 16

    virtual void SetUp()
    {
        dmSound::InitializeParams params;
        params.m_MaxBuffers = MAX_BUFFERS;
        params.m_MaxSources = MAX_SOURCES;
        params.m_OutputDevice = m_DeviceName;
        params.m_FrameCount = GetParam().m_BufferFrameCount;

        dmSound::Result r = dmSound::Initialize(0, &params);
        ASSERT_EQ(dmSound::RESULT_OK, r);

        m_DrumLoop = (void*) DRUMLOOP_WAV;
        m_DrumLoopSize = DRUMLOOP_WAV_SIZE;

        m_OneFootStep = (void*) ONEFOOTSTEP_WAV;
        m_OneFootStepSize = ONEFOOTSTEP_WAV_SIZE;

        m_LayerGuitarA = (void*) LAYER_GUITAR_A_OGG;
        m_LayerGuitarASize = LAYER_GUITAR_A_OGG_SIZE;

        m_ClickTrack = (void*) CLICK_TRACK_OGG;
        m_ClickTrackSize = CLICK_TRACK_OGG_SIZE;

        m_SineWave = (void*) OSC2_SIN_440HZ_WAV;
        m_SineWaveSize = OSC2_SIN_440HZ_WAV_SIZE;

        m_DoorOpening = (void*) DOOR_OPENING_WAV;
        m_DoorOpeningSize = DOOR_OPENING_WAV_SIZE;
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

// Some arbitrary process time for loopback-device buffers
#define LOOPBACK_DEVICE_PROCESS_TIME (10 * 1000)

struct LoopbackBuffer
{
    dmArray<int16_t> m_Buffer;
    uint64_t m_Queued;

    LoopbackBuffer(uint32_t capacity) {
        m_Buffer.SetCapacity(capacity * 2);
        m_Queued = 0;
    }
};

struct LoopbackDevice
{
    dmArray<LoopbackBuffer*> m_Buffers;
    dmArray<int16_t> m_AllOutput;
    uint32_t         m_TotalBuffersQueued;
};


LoopbackDevice *g_LoopbackDevice = 0;

dmSound::Result DeviceLoopbackOpen(const dmSound::OpenDeviceParams* params, dmSound::HDevice* device)
{
    LoopbackDevice* d = new LoopbackDevice;

    d->m_Buffers.SetCapacity(params->m_BufferCount);
    for (uint32_t i = 0; i < params->m_BufferCount; ++i) {
        d->m_Buffers.Push(new LoopbackBuffer(params->m_FrameCount));
    }
    d->m_TotalBuffersQueued = 0;

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
        if (dmTime::GetTime() - loopback->m_Buffers[i]->m_Queued > LOOPBACK_DEVICE_PROCESS_TIME) {
            b = loopback->m_Buffers[i];
            break;
        }
    }

    b->m_Queued = dmTime::GetTime();
    b->m_Buffer.SetSize(0);
    b->m_Buffer.PushArray(samples, sample_count * 2);

    return dmSound::RESULT_OK;
}

uint32_t DeviceLoopbackFreeBufferSlots(dmSound::HDevice device)
{
    LoopbackDevice* loopback = (LoopbackDevice*) device;

    uint32_t n = 0;
    for (uint32_t i = 0; i < loopback->m_Buffers.Size(); ++i) {
        uint64_t diff = dmTime::GetTime() - loopback->m_Buffers[i]->m_Queued;
        if (diff > LOOPBACK_DEVICE_PROCESS_TIME) {
            ++n;
        }
    }

    return n;
}

void DeviceLoopbackDeviceInfo(dmSound::HDevice device, dmSound::DeviceInfo* info)
{
    info->m_MixRate = 44100;
}

TEST_P(dmSoundVerifyTest, Mix)
{
    TestParams params = GetParam();
    dmSound::Result r;
    dmSound::HSoundData sd = 0;
    dmSound::NewSoundData(params.m_Sound, params.m_SoundSize, params.m_Type, &sd);

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
    const int channels = 2;

    const int n = (frame_count * 44100) / (int) mix_rate;
    for (uint32_t i = 0; i < n - 1; i++) {
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


TEST_P(dmSoundVerifyOggTest, Mix)
{
    TestParams params = GetParam();
    dmSound::Result r;
    dmSound::HSoundData sd = 0;
    dmSound::NewSoundData(params.m_Sound, params.m_SoundSize, params.m_Type, &sd);

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
}

TEST_P(dmSoundTestPlayTest, Play)
{
    TestParams params = GetParam();
    dmSound::Result r;
    dmSound::HSoundData sd = 0;
    dmSound::NewSoundData(params.m_Sound, params.m_SoundSize, params.m_Type, &sd);

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

DM_DECLARE_SOUND_DEVICE(LoopBackDevice, "loopback", DeviceLoopbackOpen, DeviceLoopbackClose, DeviceLoopbackQueue, DeviceLoopbackFreeBufferSlots, DeviceLoopbackDeviceInfo);

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
