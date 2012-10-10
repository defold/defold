#include <stdlib.h>
#include <map>
#include <vector>
#include <gtest/gtest.h>
#include <dlib/hash.h>
#include <dlib/message.h>
#include <dlib/log.h>
#include <dlib/time.h>
#include "../sound.h"

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

class dmSoundTest : public ::testing::Test
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

TEST_F(dmSoundTest, Initialize)
{
}

TEST_F(dmSoundTest, SoundData)
{
    const uint32_t n = 100;
    std::vector<dmSound::HSoundData> sounds;

    for (uint32_t i = 0; i < n; ++i)
    {
        dmSound::HSoundData sd = 0;
        dmSound::Result r = dmSound::NewSoundData(m_DrumLoop, m_DrumLoopSize, dmSound::SOUND_DATA_TYPE_WAV, &sd);
        ASSERT_EQ(dmSound::RESULT_OK, r);
        ASSERT_NE((dmSound::HSoundData) 0, sd);
        r = dmSound::SetSoundData(sd, m_OneFootStep, m_OneFootStepSize);
        ASSERT_EQ(dmSound::RESULT_OK, r);
        ASSERT_NE((dmSound::HSoundData) 0, sd);
        sounds.push_back(sd);
    }

    for (uint32_t i = 0; i < n; ++i)
    {
        dmSound::HSoundData sd = sounds[i];
        dmSound::Result r = dmSound::DeleteSoundData(sd);
        ASSERT_EQ(dmSound::RESULT_OK, r);
    }
}

TEST_F(dmSoundTest, SoundDataInstance)
{
    const uint32_t n = 100;
    dmSound::HSoundData sd = 0;
    dmSound::Result r = dmSound::NewSoundData(m_OneFootStep, m_OneFootStepSize, dmSound::SOUND_DATA_TYPE_WAV, &sd);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    std::vector<dmSound::HSoundInstance> instances;

    for (uint32_t i = 0; i < n; ++i)
    {
        dmSound::HSoundInstance instance = 0;
        r = dmSound::NewSoundInstance(sd, &instance);
        ASSERT_EQ(dmSound::RESULT_OK, r);
        ASSERT_NE((dmSound::HSoundInstance) 0, instance);
        instances.push_back(instance);
    }

    for (uint32_t i = 0; i < n; ++i)
    {
        dmSound::HSoundInstance instance = instances[i];
        dmSound::Result r = dmSound::DeleteSoundInstance(instance);
        ASSERT_EQ(dmSound::RESULT_OK, r);
    }

    r = dmSound::DeleteSoundData(sd);
    ASSERT_EQ(dmSound::RESULT_OK, r);
}

TEST_F(dmSoundTest, Play)
{
    dmSound::HSoundData sd = 0;
    dmSound::Result r = dmSound::NewSoundData(m_DrumLoop, m_DrumLoopSize, dmSound::SOUND_DATA_TYPE_WAV, &sd);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    dmSound::HSoundInstance instance = 0;
    r = dmSound::NewSoundInstance(sd, &instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    ASSERT_NE((dmSound::HSoundInstance) 0, instance);

    r = dmSound::Play(instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    r = dmSound::Update();
    ASSERT_EQ(dmSound::RESULT_OK, r);
    while (dmSound::IsPlaying(instance))
    {
        r = dmSound::Update();
        ASSERT_EQ(dmSound::RESULT_OK, r);

        dmTime::Sleep(1000);
    }

    r = dmSound::DeleteSoundInstance(instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    r = dmSound::DeleteSoundData(sd);
    ASSERT_EQ(dmSound::RESULT_OK, r);
}

TEST_F(dmSoundTest, UnderflowBug)
{
    /* Test for a invalid buffer underflow bug fixed */

    dmSound::HSoundData sd = 0;
    dmSound::Result r = dmSound::NewSoundData(m_DoorOpening, m_DoorOpeningSize, dmSound::SOUND_DATA_TYPE_WAV, &sd);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    dmSound::HSoundInstance instance = 0;
    r = dmSound::NewSoundInstance(sd, &instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    ASSERT_NE((dmSound::HSoundInstance) 0, instance);

    r = dmSound::Play(instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    r = dmSound::Update();
    ASSERT_EQ(dmSound::RESULT_OK, r);
    while (dmSound::IsPlaying(instance))
    {
        r = dmSound::Update();
        ASSERT_EQ(dmSound::RESULT_OK, r);

        dmTime::Sleep(1000);
    }

    r = dmSound::DeleteSoundInstance(instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    r = dmSound::DeleteSoundData(sd);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    dmSound::Stats stats;
    dmSound::GetStats(&stats);

    ASSERT_EQ(0U, stats.m_BufferUnderflowCount);
}

TEST_F(dmSoundTest, PlayOggVorbis)
{
    dmSound::HSoundData sd = 0;
    dmSound::Result r = dmSound::NewSoundData(m_LayerGuitarA, m_LayerGuitarASize, dmSound::SOUND_DATA_TYPE_OGG_VORBIS, &sd);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    dmSound::HSoundInstance instance = 0;
    r = dmSound::NewSoundInstance(sd, &instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    ASSERT_NE((dmSound::HSoundInstance) 0, instance);

    r = dmSound::Play(instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    r = dmSound::Update();
    ASSERT_EQ(dmSound::RESULT_OK, r);
    while (dmSound::IsPlaying(instance))
    {
        r = dmSound::Update();
        ASSERT_EQ(dmSound::RESULT_OK, r);

        dmTime::Sleep(1000);
    }

    r = dmSound::DeleteSoundInstance(instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    r = dmSound::DeleteSoundData(sd);
    ASSERT_EQ(dmSound::RESULT_OK, r);
}

TEST_F(dmSoundTest, PlayOggVorbisLoop)
{
    dmSound::HSoundData sd = 0;
    dmSound::Result r = dmSound::NewSoundData(m_ClickTrack, m_ClickTrackSize, dmSound::SOUND_DATA_TYPE_OGG_VORBIS, &sd);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    dmSound::HSoundInstance instance = 0;
    r = dmSound::NewSoundInstance(sd, &instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    ASSERT_NE((dmSound::HSoundInstance) 0, instance);

    r = dmSound::SetLooping(instance, true);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    r = dmSound::Play(instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    r = dmSound::Update();
    ASSERT_EQ(dmSound::RESULT_OK, r);

    for (int i = 0; i < 2000; ++i)
    {
        dmTime::Sleep(5000);
        r = dmSound::Update();
        ASSERT_EQ(dmSound::RESULT_OK, r);
    }

    r = dmSound::Update();
    ASSERT_EQ(dmSound::RESULT_OK, r);
    ASSERT_TRUE(dmSound::IsPlaying(instance));

    // Stop sound
    r = dmSound::Stop(instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    r = dmSound::Update();
    ASSERT_EQ(dmSound::RESULT_OK, r);
    ASSERT_FALSE(dmSound::IsPlaying(instance));

    r = dmSound::DeleteSoundInstance(instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    r = dmSound::DeleteSoundData(sd);
    ASSERT_EQ(dmSound::RESULT_OK, r);
}

TEST_F(dmSoundTest, Looping)
{
    dmSound::HSoundData sd = 0;
    dmSound::Result r = dmSound::NewSoundData(m_OneFootStep, m_OneFootStepSize, dmSound::SOUND_DATA_TYPE_WAV, &sd);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    dmSound::HSoundInstance instance = 0;
    r = dmSound::NewSoundInstance(sd, &instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    ASSERT_NE((dmSound::HSoundInstance) 0, instance);

    r = dmSound::SetLooping(instance, true);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    // Play looping and sleep
    r = dmSound::Play(instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    r = dmSound::Update();
    ASSERT_EQ(dmSound::RESULT_OK, r);
    for (int i = 0; i < 300; ++i)
    {
        dmTime::Sleep(5000);
        r = dmSound::Update();
        ASSERT_EQ(dmSound::RESULT_OK, r);
    }

    r = dmSound::Update();
    ASSERT_EQ(dmSound::RESULT_OK, r);
    ASSERT_TRUE(dmSound::IsPlaying(instance));

    // Stop sound
    r = dmSound::Stop(instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    r = dmSound::Update();
    ASSERT_EQ(dmSound::RESULT_OK, r);
    ASSERT_FALSE(dmSound::IsPlaying(instance));


    r = dmSound::DeleteSoundInstance(instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    r = dmSound::DeleteSoundData(sd);
    ASSERT_EQ(dmSound::RESULT_OK, r);
}

TEST_F(dmSoundTest, Buffers)
{
    dmSound::HSoundData sd = 0;
    dmSound::Result r = dmSound::NewSoundData(m_DrumLoop, m_DrumLoopSize, dmSound::SOUND_DATA_TYPE_WAV, &sd);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    dmSound::HSoundInstance instance = 0;
    r = dmSound::NewSoundInstance(sd, &instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    ASSERT_NE((dmSound::HSoundInstance) 0, instance);

    for (int i = 0; i < MAX_BUFFERS * 2; ++i)
    {
        r = dmSound::Play(instance);
        ASSERT_EQ(dmSound::RESULT_OK, r);
        r = dmSound::Update();
        ASSERT_EQ(dmSound::RESULT_OK, r);
        while (dmSound::IsPlaying(instance))
        {
            r = dmSound::Update();
            ASSERT_EQ(dmSound::RESULT_OK, r);

            r = dmSound::Stop(instance);
            ASSERT_EQ(dmSound::RESULT_OK, r);

            dmTime::Sleep(1000);
        }
    }

    r = dmSound::DeleteSoundInstance(instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    r = dmSound::DeleteSoundData(sd);
    ASSERT_EQ(dmSound::RESULT_OK, r);
}

TEST_F(dmSoundTest, LoopingSine)
{
    dmSound::HSoundData sd = 0;
    dmSound::Result r = dmSound::NewSoundData(m_SineWave, m_SineWaveSize, dmSound::SOUND_DATA_TYPE_WAV, &sd);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    dmSound::HSoundInstance instance = 0;
    r = dmSound::NewSoundInstance(sd, &instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    ASSERT_NE((dmSound::HSoundInstance) 0, instance);

    r = dmSound::SetLooping(instance, true);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    // Play looping and sleep
    r = dmSound::Play(instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    r = dmSound::Update();
    ASSERT_EQ(dmSound::RESULT_OK, r);
    for (int i = 0; i < 60; ++i)
    {
        dmTime::Sleep(16000);
        r = dmSound::Update();
        ASSERT_EQ(dmSound::RESULT_OK, r);
    }

    ASSERT_TRUE(dmSound::IsPlaying(instance));

    // Stop sound
    r = dmSound::Stop(instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    r = dmSound::DeleteSoundInstance(instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    r = dmSound::DeleteSoundData(sd);
    ASSERT_EQ(dmSound::RESULT_OK, r);
}

// Crash when deleting playing sound and then continue to update sound system
TEST_F(dmSoundTest, DeletePlayingSound)
{
    dmSound::HSoundData sd = 0;
    dmSound::Result r = dmSound::NewSoundData(m_DrumLoop, m_DrumLoopSize, dmSound::SOUND_DATA_TYPE_WAV, &sd);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    dmSound::HSoundInstance instance = 0;
    r = dmSound::NewSoundInstance(sd, &instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    ASSERT_NE((dmSound::HSoundInstance) 0, instance);

    r = dmSound::Play(instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    r = dmSound::Update();
    ASSERT_EQ(dmSound::RESULT_OK, r);

    r = dmSound::DeleteSoundInstance(instance);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    r = dmSound::DeleteSoundData(sd);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    // Update again to make sure it's cleaned (there was a crash here)
    r = dmSound::Update();
    ASSERT_EQ(dmSound::RESULT_OK, r);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
