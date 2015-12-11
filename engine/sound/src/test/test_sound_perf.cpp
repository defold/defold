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
#include "../sound_decoder.h"

#include <dlib/sol.h>

#define DEF_EMBED(x) \
    extern unsigned char x[]; \
    extern uint32_t x##_SIZE;

DEF_EMBED(AMBIENCE_OGG)
DEF_EMBED(GLOCKENSPIEL_OGG)
DEF_EMBED(EXPLOSION_OGG)
DEF_EMBED(EXPLOSION_LOW_MONO_OGG)
DEF_EMBED(MUSIC_OGG)
DEF_EMBED(CYMBAL_OGG)
DEF_EMBED(MUSIC_LOW_OGG)

#undef DEF_EMBED

class dmSoundTest : public ::testing::Test
{
public:

    virtual void SetUp()
    {

    }

    virtual void TearDown()
    {

    }

    char m_UnCache[4*1024*1024];

    void DecodeAndTime(const dmSoundCodec::DecoderInfo *decoder, unsigned char *buf, uint32_t size, const char *decoder_name, bool skip, const char *test_name)
    {
        int junk = 0;
        for (unsigned int i=0;i<size;i+=4096)
            junk += buf[i];
        for (unsigned int i=0;i<sizeof(m_UnCache);i++)
            m_UnCache[i] += junk;

        char tmp[4096];
        dmSoundCodec::HDecodeStream stream;

        const uint64_t time_beg = dmTime::GetTime();
        ASSERT_EQ(decoder->m_OpenStream(buf, size, &stream), dmSoundCodec::RESULT_OK);
        const uint64_t time_open = dmTime::GetTime();

        uint64_t max_chunk_time = 0;
        uint64_t iterations = 0;
        uint64_t total_decoded = 0;

        while (true)
        {
            iterations++;

            uint32_t decoded;

            const uint64_t chunk_begin = dmTime::GetTime();

            if (skip)
                decoder->m_SkipInStream(stream, sizeof(tmp), &decoded);
            else
                decoder->m_DecodeStream(stream, tmp, sizeof(tmp), &decoded);

            total_decoded += decoded;

            if (decoded != sizeof(tmp))
                break;

            const uint64_t chunk_time = dmTime::GetTime() - chunk_begin;
            if (chunk_time > max_chunk_time)
                max_chunk_time = chunk_time;
        }

        const float t2s = 0.000001f;
        const float t2ms = 0.001f;
        const uint64_t time_done = dmTime::GetTime();

        dmSoundCodec::Info streamInfo;
        decoder->m_GetStreamInfo(stream, &streamInfo);

        const uint32_t bytes_per_second = streamInfo.m_Channels * streamInfo.m_Rate * streamInfo.m_BitsPerSample / 8;
        const float audio_length = (float)total_decoded / (float)bytes_per_second;

        printf("[%s - %s (%s)] Total: %.3f s", decoder_name, test_name, (skip ? "SKIPPING" : "DECODING"), t2s * (time_done - time_beg));
        printf(" | Chunks: %u, max: %.3f ms, avg: %.3f ms", (int)iterations, t2ms * max_chunk_time, (time_done - time_open) * t2ms / float(iterations));
        printf(" | Output: %d Kb, %.1f s\n", (int)(total_decoded / 1024), audio_length);
        printf("  * Per out second: %.3f ms", t2ms * (time_done - time_beg) / audio_length);
        printf(" | In %.1f kbps | Out: %.1f Kb/s\n", (float)size / (128.0f * audio_length), (float)bytes_per_second / 1024.0f);

        decoder->m_CloseStream(stream);
    }

    void RunSuite(const char *decoder_name, bool skip)
    {
        const dmSoundCodec::DecoderInfo *info = dmSoundCodec::FindDecoderByName(decoder_name);
        ASSERT_NE((void*) 0, info);
        DecodeAndTime(info, AMBIENCE_OGG,     AMBIENCE_OGG_SIZE, decoder_name, skip, "Cymbal");
        DecodeAndTime(info, GLOCKENSPIEL_OGG, GLOCKENSPIEL_OGG_SIZE, decoder_name, skip, "Glockenspiel");
        DecodeAndTime(info, EXPLOSION_OGG,    EXPLOSION_OGG_SIZE, decoder_name, skip, "Explosion");
        DecodeAndTime(info, EXPLOSION_LOW_MONO_OGG,EXPLOSION_LOW_MONO_OGG_SIZE, decoder_name, skip, "Explosion Low Mono");
        DecodeAndTime(info, MUSIC_OGG,        MUSIC_OGG_SIZE, decoder_name, skip, "Music");
        DecodeAndTime(info, MUSIC_LOW_OGG,    MUSIC_LOW_OGG_SIZE, decoder_name, skip, "Music Low");
        DecodeAndTime(info, CYMBAL_OGG,       CYMBAL_OGG_SIZE, decoder_name, skip, "Cymbal");

        if (dmSoundCodec::FindBestDecoder(dmSoundCodec::FORMAT_VORBIS) == info)
            printf("%s is used by default with current build settings on this platform\n", decoder_name);
        else
            printf("%s is NOT used by default with current build settings on this platform\n", decoder_name);
    }
};

TEST_F(dmSoundTest, MeasureStdb)
{
    RunSuite("VorbisDecoderStb", false);
}

TEST_F(dmSoundTest, MeasureStdbSkip)
{
    RunSuite("VorbisDecoderStb", true);
}

TEST_F(dmSoundTest, MeasureTremolo)
{
    RunSuite("VorbisDecoderTremolo", false);
}

TEST_F(dmSoundTest, MeasureTremoloSkip)
{
    RunSuite("VorbisDecoderTremolo", true);
}

int main(int argc, char **argv)
{
    dmSol::Initialize();
    testing::InitGoogleTest(&argc, argv);
    int r = RUN_ALL_TESTS();
    dmSol::FinalizeWithCheck();
    return r;
}
