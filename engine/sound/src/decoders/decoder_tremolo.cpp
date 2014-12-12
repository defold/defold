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
            size_t m_Size, m_Cursor;
            const char *m_Buffer;
            ogg_int64_t m_SeekTo;
            ogg_int64_t m_PcmLength;
        };
    }

    // The functions below mimic the usual fopen/fread etc functions, reading from a buffer
    // in memory
    static size_t OggRead(void *ptr, size_t size, size_t nmemb, void *datasource)
    {
        DecodeStreamInfo *info = (DecodeStreamInfo*) datasource;

        size_t tot = nmemb * size;
        if (tot > (info->m_Size - info->m_Cursor)) {
            tot = info->m_Size - info->m_Cursor;
        }

        memcpy(ptr, &info->m_Buffer[info->m_Cursor], tot);
        info->m_Cursor += tot;
        return tot;
    }

    static int OggSeek(void *datasource, long long offset, int whence)
    {
        DecodeStreamInfo *info = (DecodeStreamInfo*) datasource;

        if (whence == SEEK_SET)
            info->m_Cursor = offset;
        else if (whence == SEEK_CUR)
            info->m_Cursor += offset;
        else if (whence == SEEK_END)
            info->m_Cursor = info->m_Size + offset;

        return 0;
    }

    static int OggClose(void *datasource)
    {
        (void)datasource;
        return 0;
    }

    static long OggTell(void *datasource)
    {
        DecodeStreamInfo *info = (DecodeStreamInfo*) datasource;
        return info->m_Cursor;
    }

    static Result TremoloOpenStream(const void* buffer, uint32_t buffer_size, HDecodeStream* stream)
    {
        DecodeStreamInfo *tmp = new DecodeStreamInfo();
        tmp->m_Buffer = (const char*) buffer;
        tmp->m_Size = buffer_size;
        tmp->m_Cursor = 0;

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
        tmp->m_Info.m_Size = 0;
        tmp->m_Info.m_Channels = info->channels;
        tmp->m_Info.m_BitsPerSample = 16;

        tmp->m_PcmLength = ov_pcm_total(&tmp->m_File, -1);
        tmp->m_SeekTo = -1;

        *stream = tmp;
        return RESULT_OK;
    }

    static Result TremoloDecode(HDecodeStream stream, char* buffer, uint32_t buffer_size, uint32_t* decoded)
    {
        DM_PROFILE(SoundCodec, "Tremolo")

        DecodeStreamInfo *streamInfo = (DecodeStreamInfo *) stream;
        uint32_t got_bytes = 0;

        // Seeks are deferred to decode time.
        if (streamInfo->m_SeekTo != -1)
        {
            ov_pcm_seek(&streamInfo->m_File, streamInfo->m_SeekTo);
            streamInfo->m_SeekTo = -1;
        }

        // The decoding API requires to fill up the whole buffer if possible,
        // but ov_read provides ogg frame by ogg frame, which might be significantly
        // shorter than the total buffer size passed in to this function. So loop and fetch.
        while (true)
        {
            const uint32_t remaining = buffer_size - got_bytes;
            if (!remaining)
            {
                break;
            }

            int bitstream;
            int bytes = ov_read(&streamInfo->m_File, &buffer[buffer_size - remaining], (int) remaining, &bitstream);

            if (!bytes)
            {
                // reached end of file
                break;
            }

            if (bytes < 0)
            {
                return RESULT_DECODE_ERROR;
            }

            got_bytes += bytes;
        }

        *decoded = got_bytes;
        return RESULT_OK;
    }

    static Result TremoloResetStream(HDecodeStream stream)
    {
        DecodeStreamInfo *streamInfo = (DecodeStreamInfo*) stream;
        ov_raw_seek(&streamInfo->m_File, 0);
        streamInfo->m_SeekTo = -1;
        return RESULT_OK;
    }

    static Result TremoloSkipInStream(HDecodeStream stream, uint32_t bytes, uint32_t* skipped)
    {
        DecodeStreamInfo *streamInfo = (DecodeStreamInfo*) stream;
        if (streamInfo->m_PcmLength > 0)
        {
            // if in skip mode already, use that position.
            ogg_int64_t pos = streamInfo->m_SeekTo;
            if (pos == -1)
                pos = ov_pcm_tell(&streamInfo->m_File);

            // clamp to end of stream
            const ogg_int64_t stride = streamInfo->m_Info.m_Channels * streamInfo->m_Info.m_BitsPerSample / 8;
            ogg_int64_t newpos = pos + bytes / stride;
            if (newpos > streamInfo->m_PcmLength)
                newpos = streamInfo->m_PcmLength;

            streamInfo->m_SeekTo = newpos;
            *skipped = (uint32_t)((newpos - pos) * stride);
            return RESULT_OK;
        }
        else
        {
            // unseekable stream.
            *skipped = 0;
            return RESULT_UNSUPPORTED;
        }
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

    // TREMOLO_SCORE is provided by wscript
    DM_DECLARE_SOUND_DECODER(AudioDecoderTremolo, "VorbisDecoderTremolo", FORMAT_VORBIS, TREMOLO_SCORE,
                             TremoloOpenStream, TremoloCloseStream, TremoloDecode, TremoloResetStream, TremoloSkipInStream, TremoloGetInfo);
}
