#include <stdint.h>
#include <dlib/index_pool.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/profile.h>

#include "sound.h"
#include "sound2.h"
#include "sound_decoder.h"

namespace dmSoundCodec
{
    /*
    namespace
    {
    
        static const uint32_t SIZE_POW2 = 0x10000;
        static const uint32_t SIZE_MASK = SIZE_POW2 - 1;
        
        struct DecodeStreamInfo {
            char m_Buffer[SIZE_POW2];
            uint32_t m_Begin, m_End;
        };
    }

    static Result RawOpenStream(const void* buffer, uint32_t buffer_size, HDecodeStream* stream)
    {
         DecodeStreamInfo *streamOut = new DecodeStreamInfo;
         streamOut->m_Begin = 0;
         streamOut->m_End = 0;
         *stream = streamOut;
         return RESULT_OK;
    }
    
    void RawCloseStream(HDecodeStream stream)
    {
        assert(stream);
        DecodeStreamInfo *streamInfo = (DecodeStreamInfo *) stream;
        delete streamInfo;
    }

    Result RawResetStream(HDecodeStream stream)
    {
        DecodeStreamInfo *streamInfo = (DecodeStreamInfo *) stream;
        streamInfo->m_Begin = 0;
        streamInfo->m_End = 0;
        return RESULT_OK;
    }

    Result RawDecodeStream(HDecodeStream stream, char* buffer, uint32_t buffer_size, uint32_t* decoded)
    {
        DecodeStreamInfo *streamInfo = (DecodeStreamInfo *) stream;
        DM_PROFILE(SoundCodec, "Raw")

        uint32_t to_copy = buffer_size;
        uint32_t in_buf = streamInfo->m_End - streamInfo->m_Begin;
        
        if (to_copy > in_buf)
        {
            to_copy = in_buf;
        }
        
        for (uint32_t i=0;i<to_copy;i++)
        {
            buffer[i] = streamInfo->m_Buffer[(streamInfo->m_Begin++) & SIZE_MASK];
        }
        
        assert(streamInfo->m_Begin <= streamInfo->m_End);
        *decoded = to_copy;
        return RESULT_OK;
    }
    
    Result RawFeed(HDecodeStream stream, char* buffer, uint32_t bytes, uint32_t* left)
    {
        DecodeStreamInfo *streamInfo = (DecodeStreamInfo *) stream;
        uint32_t room = SIZE_POW2 - (streamInfo->m_End - streamInfo->m_Begin);
        
        if (bytes > room)
        {
            *left = room;
            return RESULT_OUT_OF_RESOURCES;
        }

        for (uint32_t i=0;i<bytes;i++)
        {
            streamInfo->m_Buffer[(streamInfo->m_End++) & SIZE_MASK] = buffer[i];
        }
        
        *left = SIZE_POW2 - (streamInfo->m_End - streamInfo->m_Begin);
        return RESULT_OK;
    }
    

    Result RawSkipInStream(HDecodeStream stream, uint32_t bytes, uint32_t* skipped)
    {
        DecodeStreamInfo *streamInfo = (DecodeStreamInfo *) stream;
        
        uint32_t old = streamInfo->m_Begin;
        streamInfo->m_Begin += bytes;
        if (streamInfo->m_Begin > streamInfo->m_End)
        {
            streamInfo->m_Begin = streamInfo->m_End;
        }
        
        *skipped = streamInfo->m_Begin - old;
        return RESULT_OK;
    }

    void RawGetInfo(HDecodeStream stream, struct Info* out)
    {
        out->m_Rate = 44100;
        out->m_Size = 0;
        out->m_Channels = 2;
        out->m_BitsPerSample = 16;
    }

    DM_DECLARE_SOUND_DECODER(AudioDecoderRaw, "RawDecoder", FORMAT_RAW,
                             0,
                             RawOpenStream, RawCloseStream, RawDecodeStream, RawResetStream, RawSkipInStream, RawFeed, RawGetInfo);
                             */
}