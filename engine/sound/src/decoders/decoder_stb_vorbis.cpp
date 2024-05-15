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
            stb_vorbis* m_StbVorbis;
            uint32_t m_NumSamples;
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

            streamInfo->m_NumSamples = (uint32_t)stb_vorbis_stream_length_in_samples(vorbis);

            *stream = streamInfo;
            return RESULT_OK;
        } else {
            return RESULT_INVALID_FORMAT;
        }
    }

    static Result StbVorbisDecode(HDecodeStream stream, char* buffer, uint32_t buffer_size, uint32_t* decoded)
    {
        DecodeStreamInfo *streamInfo = (DecodeStreamInfo *) stream;

        DM_PROFILE(__FUNCTION__);

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

    static Result StbVorbisResetStream(HDecodeStream stream)
    {
        stb_vorbis_seek_start(((DecodeStreamInfo*)stream)->m_StbVorbis);
        return RESULT_OK;
    }

    static Result StbVorbisSkipInStream(HDecodeStream stream, uint32_t num_bytes, uint32_t* skipped)
    {
        // Decode with buffer = null corresponding number of bytes.
        // We've modified stb_vorbis to accept a null pointer, which allows us skipping a lot of decoding work.
        // NOTE: Although the stb-vorbis api has functions for seeking forward in the stream
        // there seem to be no clear cut way to get the exact position afterwards.
        // So, we revert to the "decode and discard" approach to keep the internal state of the stream intact.
        return StbVorbisDecode(stream, 0, num_bytes, skipped);
    }

    static void StbVorbisCloseStream(HDecodeStream stream)
    {
        DecodeStreamInfo *streamInfo = (DecodeStreamInfo*) stream;
        stb_vorbis_close(streamInfo->m_StbVorbis);
        delete streamInfo;
    }

    static void StbVorbisGetInfo(HDecodeStream stream, struct Info* out)
    {
        *out = ((DecodeStreamInfo *)stream)->m_Info;
    }

    static int64_t StbVorbisGetInternalPos(HDecodeStream stream)
    {
        DecodeStreamInfo *streamInfo = (DecodeStreamInfo *) stream;
        return stb_vorbis_get_sample_offset(streamInfo->m_StbVorbis);
    }

    DM_DECLARE_SOUND_DECODER(AudioDecoderStbVorbis, "VorbisDecoderStb", FORMAT_VORBIS,
                             5, // baseline score (1-10)
                             StbVorbisOpenStream, StbVorbisCloseStream, StbVorbisDecode,
                             StbVorbisResetStream, StbVorbisSkipInStream, StbVorbisGetInfo,
                             StbVorbisGetInternalPos);
}
