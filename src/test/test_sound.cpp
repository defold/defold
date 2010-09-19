#include <stdlib.h>
#include <map>
#include <vector>
#include <gtest/gtest.h>
#include <dlib/hash.h>
#include <dlib/message.h>
#include <dlib/log.h>
#include <dlib/time.h>
#include "../sound.h"

class dmSoundTest : public ::testing::Test
{
public:
    void*    m_DrumLoop;
    uint32_t m_DrumLoopSize;

    void*    m_OneFootStep;
    uint32_t m_OneFootStepSize;

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

        LoadFile("src/test/drumloop.wav", &m_DrumLoop, &m_DrumLoopSize);

        LoadFile("src/test/onefootstep.wav", &m_OneFootStep, &m_OneFootStepSize);
    }

    virtual void TearDown()
    {
        free(m_DrumLoop);
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

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
