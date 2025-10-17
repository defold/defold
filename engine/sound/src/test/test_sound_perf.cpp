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
#include <map>
#include <vector>
#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>
#include <dlib/hash.h>
#include <dlib/message.h>
#include <dlib/log.h>
#include <dlib/time.h>
#include "../sound.h"
#include "../sound_codec.h"
#include "../sound_decoder.h"

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

DEF_EMBED(AMBIENCE_OPUS)
DEF_EMBED(GLOCKENSPIEL_OPUS)
DEF_EMBED(EXPLOSION_OPUS)
DEF_EMBED(EXPLOSION_LOW_MONO_OPUS)
DEF_EMBED(MUSIC_OPUS)
DEF_EMBED(CYMBAL_OPUS)
DEF_EMBED(MUSIC_LOW_OPUS)

#undef DEF_EMBED

#define MAX_BUFFERS 32
#define MAX_SOURCES 16

class dmSoundTest : public jc_test_base_class
{
public:

    void SetUp() override
    {

    }

    void TearDown() override
    {

    }

    char m_UnCache[4*1024*1024];

    void DecodeAndTime(const dmSoundCodec::DecoderInfo *decoder, unsigned char *buf, uint32_t size, const char *decoder_name, dmSound::SoundDataType sound_datatype,  bool skip, const char *test_name)
    {
        int junk = 0;
        for (unsigned int i=0;i<size;i+=4096)
            junk += buf[i];
        for (unsigned int i=0;i<sizeof(m_UnCache);i++)
            m_UnCache[i] += junk;

        // Setup two output buffers (dummy) so we can support interleaved and non-interleaved data output
        char tmp_l[4096*2];
        char tmp_r[4096*2];
        char* tmp_ptr[] = {tmp_l, tmp_r};
        dmSoundCodec::HDecodeStream stream;

        dmSound::HSoundData sd = 0;
        dmSound::NewSoundData(buf, size, sound_datatype, &sd, dmHashString64(test_name));

        const uint64_t time_beg = dmTime::GetMonotonicTime();
        ASSERT_EQ(decoder->m_OpenStream(sd, &stream), dmSoundCodec::RESULT_OK);
        const uint64_t time_open = dmTime::GetMonotonicTime();

        // select output buffer size depending if we have int16_t or float data being produced (we are not interested in PCM8 here) to make sure we 'chunk' the decode the same for both

        dmSoundCodec::Info info;
        decoder->m_GetStreamInfo(stream, &info);

        // Make sure we get half the buffers only if we decode to int16 not float (to keep chunking the same)
        uint32_t out_size = sizeof(tmp_l) / (32 / info.m_BitsPerSample);
        if (!info.m_IsInterleaved)
        {
            // Half the buffer size if we have non-interleaved data (as we use per channel buffers)
            out_size /= info.m_Channels;
            assert(info.m_Channels <= 2);
        }

        uint64_t max_chunk_time = 0;
        uint64_t iterations = 0;
        uint64_t total_decoded = 0;

        while (true)
        {
            iterations++;

            uint32_t decoded;

            const uint64_t chunk_begin = dmTime::GetMonotonicTime();

            if (skip)
                decoder->m_SkipInStream(stream, out_size, &decoded);
            else
                decoder->m_DecodeStream(stream, tmp_ptr, out_size, &decoded);

            total_decoded += decoded * (info.m_IsInterleaved ? 1 : info.m_Channels);

            if (decoded != out_size)
                break;

            const uint64_t chunk_time = dmTime::GetMonotonicTime() - chunk_begin;
            if (chunk_time > max_chunk_time)
                max_chunk_time = chunk_time;
        }

        const float t2s = 0.000001f;
        const float t2ms = 0.001f;
        const uint64_t time_done = dmTime::GetMonotonicTime();

        dmSoundCodec::Info streamInfo;
        decoder->m_GetStreamInfo(stream, &streamInfo);

        const uint32_t bytes_per_second = streamInfo.m_Channels * streamInfo.m_Rate * streamInfo.m_BitsPerSample / 8;
        const float audio_length = (float)total_decoded / (float)bytes_per_second;

        printf("[%s - %s (%s)] Total: %.3f s", decoder_name, test_name, (skip ? "SKIPPING" : "DECODING"), t2s * (time_done - time_beg));
        printf(" | Chunks: %u, max: %.3f ms, avg: %.3f ms", (int)iterations, t2ms * max_chunk_time, (time_done - time_open) * t2ms / float(iterations));
        printf(" | Output: %d Kb, %.1f s\n", (int)(total_decoded / 1024), audio_length);
        printf("  * Per out second: %.3f ms", t2ms * (time_done - time_open) / audio_length);
        printf(" | In %.1f kbps | Out: %.1f Kb/s\n", (float)size / (128.0f * audio_length), (float)bytes_per_second / 1024.0f);

        decoder->m_CloseStream(stream);

        dmSound::DeleteSoundData(sd);
    }

    void RunSuite(const char *decoder_name, dmSound::SoundDataType sound_datatype, bool skip)
    {
        dmSound::InitializeParams params;
        params.m_MaxBuffers = MAX_BUFFERS;
        params.m_MaxSources = MAX_SOURCES;
        params.m_OutputDevice = "default";
        params.m_FrameCount = 2048;//512;//GetParam().m_BufferFrameCount;
        params.m_UseThread = true; // all our desktop platforms support this

        dmSound::Result r = dmSound::Initialize(0, &params);
        assert(r == dmSound::RESULT_OK);

        const dmSoundCodec::DecoderInfo *info = dmSoundCodec::FindDecoderByName(decoder_name);
        ASSERT_NE((void*) 0, info);

        if (sound_datatype == dmSound::SOUND_DATA_TYPE_OGG_VORBIS) {
            DecodeAndTime(info, AMBIENCE_OGG,     AMBIENCE_OGG_SIZE, decoder_name, sound_datatype, skip, "Ambience");
            DecodeAndTime(info, GLOCKENSPIEL_OGG, GLOCKENSPIEL_OGG_SIZE, decoder_name, sound_datatype, skip, "Glockenspiel");
            DecodeAndTime(info, EXPLOSION_OGG,    EXPLOSION_OGG_SIZE, decoder_name, sound_datatype, skip, "Explosion");
            DecodeAndTime(info, EXPLOSION_LOW_MONO_OGG,EXPLOSION_LOW_MONO_OGG_SIZE, decoder_name, sound_datatype, skip, "Explosion Low Mono");
            DecodeAndTime(info, MUSIC_OGG,        MUSIC_OGG_SIZE, decoder_name, sound_datatype, skip, "Music");
            DecodeAndTime(info, MUSIC_LOW_OGG,    MUSIC_LOW_OGG_SIZE, decoder_name, sound_datatype, skip, "Music Low");
            DecodeAndTime(info, CYMBAL_OGG,       CYMBAL_OGG_SIZE, decoder_name, sound_datatype, skip, "Cymbal");
        }
        else {
            DecodeAndTime(info, AMBIENCE_OPUS,    AMBIENCE_OPUS_SIZE, decoder_name, sound_datatype, skip, "Ambience");
            DecodeAndTime(info, GLOCKENSPIEL_OPUS,GLOCKENSPIEL_OPUS_SIZE, decoder_name, sound_datatype, skip, "Glockenspiel");
            DecodeAndTime(info, EXPLOSION_OPUS,   EXPLOSION_OPUS_SIZE, decoder_name, sound_datatype, skip, "Explosion");
            DecodeAndTime(info, EXPLOSION_LOW_MONO_OPUS,EXPLOSION_LOW_MONO_OPUS_SIZE, decoder_name, sound_datatype, skip, "Explosion Low Mono");
            DecodeAndTime(info, MUSIC_OPUS,       MUSIC_OPUS_SIZE, decoder_name, sound_datatype, skip, "Music");
            DecodeAndTime(info, MUSIC_LOW_OPUS,   MUSIC_LOW_OPUS_SIZE, decoder_name, sound_datatype, skip, "Music Low");
            DecodeAndTime(info, CYMBAL_OPUS,      CYMBAL_OPUS_SIZE, decoder_name, sound_datatype, skip, "Cymbal");
        }

        if (dmSoundCodec::FindBestDecoder(dmSoundCodec::FORMAT_VORBIS) == info)
            printf("%s is used by default with current build settings on this platform\n", decoder_name);
        else
            printf("%s is NOT used by default with current build settings on this platform\n", decoder_name);

        r = dmSound::Finalize();
        assert(r == dmSound::RESULT_OK);
    }
};

#if !defined(GITHUB_CI) || (defined(GITHUB_CI) && !defined(__MACH__))
TEST_F(dmSoundTest, MeasureStdb)
{
    RunSuite("VorbisDecoderStb", dmSound::SOUND_DATA_TYPE_OGG_VORBIS, false);
}

TEST_F(dmSoundTest, MeasureStdbSkip)
{
    RunSuite("VorbisDecoderStb", dmSound::SOUND_DATA_TYPE_OGG_VORBIS, true);
}

TEST_F(dmSoundTest, MeasureTremolo)
{
    RunSuite("VorbisDecoderTremolo", dmSound::SOUND_DATA_TYPE_OGG_VORBIS, false);
}

TEST_F(dmSoundTest, MeasureTremoloSkip)
{
    RunSuite("VorbisDecoderTremolo", dmSound::SOUND_DATA_TYPE_OGG_VORBIS, true);
}

TEST_F(dmSoundTest, MeasureOpus)
{
    RunSuite("OpusDecoder", dmSound::SOUND_DATA_TYPE_OPUS, false);
}

TEST_F(dmSoundTest, MeasureOpusSkip)
{
    RunSuite("OpusDecoder", dmSound::SOUND_DATA_TYPE_OPUS, true);
}

#endif

extern "C" void dmExportedSymbols();

int main(int argc, char **argv)
{
    dmExportedSymbols();
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
