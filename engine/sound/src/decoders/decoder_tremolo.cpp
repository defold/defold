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

#include "tremolo/ivorbisfile.h"

#include "sound_codec.h"
#include "sound_decoder.h"

namespace dmSoundCodec
{
    namespace
    {
        struct DecodeStreamInfo
        {
            Info m_Info;
            OggVorbis_File m_File;
            size_t m_Cursor;
            dmSound::HSoundData m_SoundData;
            uint8_t m_bEOS : 1;
        };
    }

    // The functions below mimic the usual fopen/fread etc functions, reading from a buffer
    // in memory
    static size_t OggRead(void *ptr, size_t size, size_t nmemb, void *datasource)
    {
        DecodeStreamInfo *info = (DecodeStreamInfo*) datasource;

        if (info->m_bEOS) {
            return 0;
        }

        size_t tot = nmemb * size;

        // note: this does NOT 100% emulate a fread as we are at times unable to deliver all data we are asked to deliver
        uint32_t read = 0;
        dmSound::Result res = dmSound::SoundDataRead(info->m_SoundData, (uint32_t)info->m_Cursor, (uint32_t)tot, ptr, &read);

        info->m_bEOS = (res == dmSound::RESULT_END_OF_STREAM);
        info->m_Cursor += read;

        // Return the count of objects vs. bytes (just like C's stdio)
        return read / size;
    }

    static int OggSeek(void *datasource, long long offset, int whence)
    {
        (void)datasource;
        return -1;  // we cannot seek
    }

    static int OggClose(void *datasource)
    {
        (void)datasource;
        return 0;
    }

    static long OggTell(void *datasource)
    {
        (void)datasource;
        return -1;  // flag this data as non-seekable (this will deactivate any and all seeking / length-query etc. support in Tremolo)
    }

    static Result TremoloOpenStream(dmSound::HSoundData sound_data, HDecodeStream* stream)
    {
        DecodeStreamInfo *tmp = new DecodeStreamInfo();
        tmp->m_SoundData = sound_data;
        tmp->m_Cursor = 0;
        tmp->m_bEOS = false;

        ov_callbacks cb;
        cb.read_func = OggRead;
        cb.close_func = OggClose;
        cb.seek_func = OggSeek;
        cb.tell_func = OggTell;

        int res = ov_open_callbacks(tmp, &tmp->m_File, 0, 0, cb);
        if (res)
        {
            delete tmp;
            return RESULT_INVALID_FORMAT;
        }

        vorbis_info *info = ov_info(&tmp->m_File, -1);

        tmp->m_Info.m_Rate = info->rate;
        tmp->m_Info.m_Channels = info->channels;
        tmp->m_Info.m_BitsPerSample = 16;

        *stream = tmp;
        return RESULT_OK;
    }

    static Result TremoloDecode(HDecodeStream stream, char* buffer, uint32_t buffer_size, uint32_t* decoded)
    {
        // note: EOS detection is solely based on data consumption and hence not sample precise (the last decoded block may contain silence not part of the original material)

        DM_PROFILE(__FUNCTION__);

        DecodeStreamInfo *streamInfo = (DecodeStreamInfo *) stream;
        uint32_t got_bytes = 0;

        // The decoding API requires to fill up the whole buffer if possible,
        // but ov_read provides ogg frame by ogg frame, which might be significantly
        // shorter than the total buffer size passed in to this function. So loop and fetch.
        while (true)
        {
            const uint32_t remaining = buffer_size - got_bytes;
            if (remaining == 0)
            {
                break;
            }

            int bitstream;
            int bytes = ov_read(&streamInfo->m_File, buffer ? &buffer[buffer_size - remaining] : NULL, (int) remaining, &bitstream);

            if (bytes == 0)
            {
                // no more data available for now
                break;
            }

            if (bytes < 0)
            {
                return RESULT_DECODE_ERROR;
            }

            got_bytes += bytes;
        }

        *decoded = got_bytes;
        return (got_bytes == 0 && streamInfo->m_bEOS) ? RESULT_END_OF_STREAM : RESULT_OK;
    }

    static Result TremoloResetStream(HDecodeStream stream)
    {
        DecodeStreamInfo *streamInfo = (DecodeStreamInfo*) stream;

        // shutdown & restart the decoder as we cannot seek anywhere if we do not hand it a seek call on the file IO level
        ov_clear(&streamInfo->m_File);

        ov_callbacks cb;
        cb.read_func = OggRead;
        cb.close_func = OggClose;
        cb.seek_func = OggSeek;
        cb.tell_func = OggTell;

        streamInfo->m_bEOS = false;
        streamInfo->m_Cursor = 0;

        int res = ov_open_callbacks(streamInfo, &streamInfo->m_File, 0, 0, cb);
        if (res)
        {
            delete streamInfo;
            return RESULT_INVALID_FORMAT;
        }

        return RESULT_OK;
    }

    static Result TremoloSkipInStream(HDecodeStream stream, uint32_t bytes, uint32_t* skipped)
    {
        // Decode to 'NIL' (unfortunately with zero cycle savings vs. a real decode)
        char buffer[4096];
        Result ret = RESULT_OK;
        while(bytes && ret == RESULT_OK)
        {
            uint32_t chunk = dmMath::Min(bytes, (uint32_t)sizeof(buffer));
            uint32_t decoded = 0;
            ret = TremoloDecode(stream, buffer, chunk, &decoded);
            (*skipped) += decoded;
            bytes -= decoded;
        }
        return ret;
    }

    static void TremoloCloseStream(HDecodeStream stream)
    {
        DecodeStreamInfo *streamInfo = (DecodeStreamInfo*) stream;
        ov_clear(&streamInfo->m_File);
        delete streamInfo;
    }

    static void TremoloGetInfo(HDecodeStream stream, struct Info* out)
    {
        *out = ((DecodeStreamInfo *) stream)->m_Info;
    }

    static int64_t TremoloGetInternalPos(HDecodeStream stream)
    {
        DecodeStreamInfo *streamInfo = (DecodeStreamInfo *) stream;
        return (int64_t)ov_pcm_tell(&streamInfo->m_File);
    }

    DM_DECLARE_SOUND_DECODER(AudioDecoderTremolo, "VorbisDecoderTremolo", FORMAT_VORBIS, 8,
                             TremoloOpenStream, TremoloCloseStream, TremoloDecode,
                             TremoloResetStream, TremoloSkipInStream, TremoloGetInfo,
                             TremoloGetInternalPos);
}
