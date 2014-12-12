#include <stdint.h>
#include <dlib/index_pool.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/profile.h>

#include "stb_vorbis/stb_vorbis.h"
#include "sound_codec.h"
#include "sound_decoder.h"

namespace dmSoundCodec
{
    namespace
    {
        struct DecodeStreamInfo {
            Info m_Info;
            stb_vorbis *m_StbVorbis;
        };
    }

    static Result StbVorbisOpenStream(const void* buffer, uint32_t buffer_size, HDecodeStream* stream)
    {
        int error;
        stb_vorbis* vorbis = stb_vorbis_open_memory((unsigned char*) buffer, buffer_size, &error, NULL);

        if (vorbis) {
            stb_vorbis_info info = stb_vorbis_get_info(vorbis);

            DecodeStreamInfo *streamInfo = new DecodeStreamInfo;
            streamInfo->m_Info.m_Rate = info.sample_rate;
            streamInfo->m_Info.m_Size = 0;
            streamInfo->m_Info.m_Channels = info.channels;
            streamInfo->m_Info.m_BitsPerSample = 16;
            streamInfo->m_StbVorbis = vorbis;

            *stream = streamInfo;
            return RESULT_OK;
        } else {
            return RESULT_INVALID_FORMAT;
        }
    }

    static Result StbVorbisDecode(HDecodeStream stream, char* buffer, uint32_t buffer_size, uint32_t* decoded)
    {
        DecodeStreamInfo *streamInfo = (DecodeStreamInfo *) stream;

        DM_PROFILE(SoundCodec, "StbVorbis")

        int ret = 0;
        if (streamInfo->m_Info.m_Channels == 1) {
            ret = stb_vorbis_get_samples_short_interleaved(streamInfo->m_StbVorbis, 1, (short*) buffer, buffer_size / 2);
        } else if (streamInfo->m_Info.m_Channels == 2) {
            ret = stb_vorbis_get_samples_short_interleaved(streamInfo->m_StbVorbis, 2, (short*) buffer, buffer_size / 2);
        } else {
            assert(0);
        }

        if (ret < 0) {
            return RESULT_DECODE_ERROR;
        } else {
            if (streamInfo->m_Info.m_Channels == 1) {
                *decoded = ret * 2;
            } else if (streamInfo->m_Info.m_Channels == 2) {
                *decoded = ret * 4;
            } else {
                assert(0);
            }
        }

        return RESULT_OK;
    }

    Result StbVorbisResetStream(HDecodeStream stream)
    {
        stb_vorbis_seek_start(((DecodeStreamInfo*)stream)->m_StbVorbis);
        return RESULT_OK;
    }

    Result StbVorbisSkipInStream(HDecodeStream stream, uint32_t bytes, uint32_t* skipped)
    {
        // Decode with buffer = null corresponding number of bytes.
        // stb_vorbis has a special case for this skipping a lot of
        // decoding work.
        Result r = StbVorbisDecode(stream, 0, bytes, skipped);
        return r;
    }

    void StbVorbisCloseStream(HDecodeStream stream)
    {
        DecodeStreamInfo *streamInfo = (DecodeStreamInfo*) stream;
        stb_vorbis_close(streamInfo->m_StbVorbis);
        delete streamInfo;
    }

    void StbVorbisGetInfo(HDecodeStream stream, struct Info* out)
    {
        *out = ((DecodeStreamInfo *)stream)->m_Info;
    }

    DM_DECLARE_SOUND_DECODER(AudioDecoderStbVorbis, "VorbisDecoderStb", FORMAT_VORBIS,
                             5, // baseline score (1-10)
                             StbVorbisOpenStream, StbVorbisCloseStream, StbVorbisDecode, StbVorbisResetStream, StbVorbisSkipInStream, StbVorbisGetInfo);
}
