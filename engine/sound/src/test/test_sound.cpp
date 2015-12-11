#include <stdlib.h>
#include <map>
#include <vector>
#include <gtest/gtest.h>
#include <dlib/hash.h>
#include <dlib/message.h>
#include <dlib/log.h>
#include <dlib/time.h>
#include "../sound.h"
#include "../sound_codec.h"
#include "../stb_vorbis/stb_vorbis.h"

#include <dlib/sol.h>

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
        params.m_OutputDevice = "default";
        params.m_MaxBuffers = MAX_BUFFERS;
        params.m_MaxSources = MAX_SOURCES;
        params.m_FrameCount = 2048;

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

TEST_F(dmSoundTest, Polyphony)
{
    dmSound::HSoundData sd = 0;
    dmSound::Result r = dmSound::NewSoundData(BOOSTER_ON_SFX_WAV, BOOSTER_ON_SFX_WAV_SIZE, dmSound::SOUND_DATA_TYPE_WAV, &sd);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    dmSound::HSoundInstance instance1 = 0;
    r = dmSound::NewSoundInstance(sd, &instance1);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    dmSound::HSoundInstance instance2 = 0;
    r = dmSound::NewSoundInstance(sd, &instance2);
    ASSERT_EQ(dmSound::RESULT_OK, r);

    uint64_t start = 0;
    uint64_t end = 0;

    start = dmTime::GetTime();
    r = dmSound::Play(instance1);
    end = dmTime::GetTime();

    ASSERT_EQ(dmSound::RESULT_OK, r);
    dmTime::Sleep(100 * 1000);

    start = dmTime::GetTime();
    r = dmSound::Play(instance2);
    end = dmTime::GetTime();
    ASSERT_EQ(dmSound::RESULT_OK, r);

    r = dmSound::Update();
    ASSERT_EQ(dmSound::RESULT_OK, r);
    while (dmSound::IsPlaying(instance1) || dmSound::IsPlaying(instance2))
    {
        r = dmSound::Update();
        ASSERT_EQ(dmSound::RESULT_OK, r);

        dmTime::Sleep(1000);
    }

    dmTime::Sleep(1000 * 1000);


    r = dmSound::DeleteSoundInstance(instance1);
    ASSERT_EQ(dmSound::RESULT_OK, r);
    r = dmSound::DeleteSoundInstance(instance2);
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

TEST_F(dmSoundTest, OggDecompressionRate)
{
    // Benchmark to test real performance on device
    dmSoundCodec::NewCodecContextParams params;
    dmSoundCodec::HCodecContext codec = dmSoundCodec::New(&params);
    ASSERT_NE((void*) 0, codec);

    dmSoundCodec::Result r;
    dmSoundCodec::Info info;
    dmSoundCodec::HDecoder decoder;

    r = dmSoundCodec::NewDecoder(codec, dmSoundCodec::FORMAT_VORBIS, (unsigned char*) LAYER_GUITAR_A_OGG, LAYER_GUITAR_A_OGG_SIZE, &decoder);
    ASSERT_EQ(dmSoundCodec::RESULT_OK, r);

    dmSoundCodec::GetInfo(codec, decoder, &info);

    const int num_rounds = 5*1024*1024 / LAYER_GUITAR_A_OGG_SIZE;
    char buf[2048];
    uint32_t decoded;
    uint32_t total;

    uint64_t start = dmTime::GetTime();
    total = 0;

    for (int i=0;i<num_rounds;i++)
    {
        do
        {
            r = dmSoundCodec::Decode(codec, decoder, buf, sizeof(buf), &decoded);
            total += decoded;
            ASSERT_EQ(dmSoundCodec::RESULT_OK, r);
        }
        while (decoded > 0);

        // This is (should) be a fairly cheap operation in comparison to decoding.
        dmSoundCodec::Reset(codec, decoder);
    }

    uint64_t end = dmTime::GetTime();

    ASSERT_EQ(dmSoundCodec::RESULT_OK, r);

    dmSoundCodec::DeleteDecoder(codec, decoder);
    dmSoundCodec::Delete(codec);

    float elapsed = (float) (end - start);

    printf("channels: %d\n", info.m_Channels);
    printf("sample rate: %d\n", info.m_Rate);
    float rate = (1000000.0f * total / elapsed);
    printf("rate: %.2f kb/s\n", rate / 1024.0f);
    float bytes_per_60_frame = info.m_Channels * 2 * info.m_Rate / 60.0f;
    printf("bytes required/60-frame: %.2f\n", bytes_per_60_frame);
    float time_per_60_frame = bytes_per_60_frame / rate;
    printf("time/60-frame: %.2f ms\n", 1000.0f * time_per_60_frame);
    printf("time for 8k buffer: %.2f ms\n", 1000.0f * 2 * 4096 / rate);
}

TEST_F(dmSoundTest, WavCodec)
{
    dmSoundCodec::NewCodecContextParams params;
    dmSoundCodec::HCodecContext codec = dmSoundCodec::New(&params);
    ASSERT_NE((void*) 0, codec);

    dmSoundCodec::Result r;
    dmSoundCodec::Info info;
    dmSoundCodec::HDecoder decoder;

    r = dmSoundCodec::NewDecoder(codec, dmSoundCodec::FORMAT_WAV, m_DrumLoop, m_DrumLoopSize, &decoder);
    ASSERT_EQ(dmSoundCodec::RESULT_OK, r);
    dmSoundCodec::GetInfo(codec, decoder, &info);
    ASSERT_EQ(44100U, info.m_Rate);
    ASSERT_EQ(1, (int) info.m_Channels);
    ASSERT_EQ(8, info.m_BitsPerSample);
    ASSERT_EQ(42158U, info.m_Size);

    char buf[2048];
    uint32_t decoded;
    uint32_t total;

    total = 0;
    do {
        r = dmSoundCodec::Decode(codec, decoder, buf, sizeof(buf), &decoded);
        total += decoded;
        ASSERT_EQ(dmSoundCodec::RESULT_OK, r);
    } while (decoded > 0);
    ASSERT_EQ(dmSoundCodec::RESULT_OK, r);
    ASSERT_EQ(info.m_Size, total);

    dmSoundCodec::DeleteDecoder(codec, decoder);

    r = dmSoundCodec::NewDecoder(codec, dmSoundCodec::FORMAT_WAV, m_OneFootStep, m_OneFootStepSize, &decoder);
    ASSERT_EQ(dmSoundCodec::RESULT_OK, r);
    dmSoundCodec::GetInfo(codec, decoder, &info);
    ASSERT_EQ(44100U, info.m_Rate);
    ASSERT_EQ(2, (int) info.m_Channels);
    ASSERT_EQ(16, info.m_BitsPerSample);
    ASSERT_EQ(27904U * 2 * 2, info.m_Size);

    total = 0;
    do {
        r = dmSoundCodec::Decode(codec, decoder, buf, sizeof(buf), &decoded);
        total += decoded;
        ASSERT_EQ(dmSoundCodec::RESULT_OK, r);
    } while (decoded > 0);
    ASSERT_EQ(dmSoundCodec::RESULT_OK, r);
    ASSERT_EQ(info.m_Size, total);

    dmSoundCodec::DeleteDecoder(codec, decoder);

    dmSoundCodec::Delete(codec);
}

TEST_F(dmSoundTest, VorbisCodec)
{
    dmSoundCodec::NewCodecContextParams params;
    dmSoundCodec::HCodecContext codec = dmSoundCodec::New(&params);
    ASSERT_NE((void*) 0, codec);

    dmSoundCodec::Result r;
    dmSoundCodec::Info info;
    dmSoundCodec::HDecoder decoder;

    r = dmSoundCodec::NewDecoder(codec, dmSoundCodec::FORMAT_VORBIS, TONE_MONO_22050_OGG, TONE_MONO_22050_OGG_SIZE, &decoder);
    ASSERT_EQ(dmSoundCodec::RESULT_OK, r);
    dmSoundCodec::GetInfo(codec, decoder, &info);
    ASSERT_EQ(22050U, info.m_Rate);
    ASSERT_EQ(1, (int) info.m_Channels);
    ASSERT_EQ(16, info.m_BitsPerSample);
    ASSERT_EQ(0U, info.m_Size);

    char buf[2048];
    uint32_t decoded;
    uint32_t total;

    total = 0;
    do {
        r = dmSoundCodec::Decode(codec, decoder, buf, sizeof(buf), &decoded);
        total += decoded;
        ASSERT_EQ(dmSoundCodec::RESULT_OK, r);
    } while (decoded > 0);
    ASSERT_EQ(dmSoundCodec::RESULT_OK, r);
    // NOTE: 220500 hard-coded. We don't know the total size from meta-data
    ASSERT_EQ(220500U, total);

    dmSoundCodec::DeleteDecoder(codec, decoder);
    dmSoundCodec::Delete(codec);
}

TEST_F(dmSoundTest, WavDecodeBug)
{
    dmSoundCodec::NewCodecContextParams params;
    dmSoundCodec::HCodecContext codec = dmSoundCodec::New(&params);
    ASSERT_NE((void*) 0, codec);

    dmSoundCodec::Result r;
    dmSoundCodec::HDecoder decoder;

    r = dmSoundCodec::NewDecoder(codec, dmSoundCodec::FORMAT_WAV, m_SineWave, m_SineWaveSize, &decoder);
    ASSERT_EQ(dmSoundCodec::RESULT_OK, r);

    const int buf_size = 1024 * 1024;
    char* buf = (char*) malloc(buf_size);
    uint32_t decoded;
    r = dmSoundCodec::Decode(codec, decoder, buf, buf_size, &decoded);
    ASSERT_EQ(dmSoundCodec::RESULT_OK, r);

    int16_t* shorts = (int16_t*) buf;
    // NOTE: Values by inspection of buffer when writing test
    ASSERT_EQ((int) 0, (int) shorts[0]);
    ASSERT_EQ((int) -2, (int) shorts[decoded/2-1]);

    dmSoundCodec::DeleteDecoder(codec, decoder);
    free(buf);

    dmSoundCodec::Delete(codec);
}


int main(int argc, char **argv)
{
    dmSol::Initialize();
    testing::InitGoogleTest(&argc, argv);
    int r = RUN_ALL_TESTS();
    dmSol::FinalizeWithCheck();
    return r;
}
