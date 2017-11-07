#include <stdint.h>
#include <dlib/index_pool.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/profile.h>

#include "sound.h"
#include "sound_decoder.h"

#if DM_ENDIAN == DM_ENDIAN_LITTLE
#define FOUR_CC(a,b,c,d) (((uint32_t)d << 24) | ((uint32_t)c << 16) | ((uint32_t)b << 8) | ((uint32_t)a))
#else
#define FOUR_CC(a,b,c,d) (((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)c << 8) | ((uint32_t)d))
#endif

namespace dmSoundCodec
{
    namespace
    {
    #if DM_ENDIAN == DM_ENDIAN_LITTLE
        static uint16_t Swap16(uint16_t x)
        {
          return x;
        }

        static uint32_t Swap32(uint32_t x)
        {
          return x;
        }
    #else
        static uint16_t Swap16(uint16_t x)
        {
          return ((x & 0xff00) >> 8) |
               ((x & 0x00ff) << 8);
        }

        static uint32_t Swap32(uint32_t x)
        {
          return ((x & 0xff000000) >> 24) |
               ((x & 0x00ff0000) >>  8) |
               ((x & 0x0000ff00) <<  8) |
               ((x & 0x000000ff) << 24);
        }
    #endif

        struct CommonHeader
        {
            uint32_t m_ChunkID;
            uint32_t m_ChunkSize;

            void SwapHeader() {
                m_ChunkID = Swap32(m_ChunkID);
                m_ChunkSize = Swap32(m_ChunkSize);
            }
        };

        struct RiffHeader : CommonHeader
        {
            uint32_t m_Format;
            void Swap()
            {
                SwapHeader();
                m_Format = Swap32(m_Format);
            }
        };

        struct FmtChunk : CommonHeader
        {
            uint16_t m_AudioFormat;
            uint16_t m_NumChannels;
            uint32_t m_SampleRate;
            uint32_t m_ByteRate;
            uint16_t m_BlockAlign;
            uint16_t m_BitsPerSample;

            void Swap()
            {
                SwapHeader();
                m_AudioFormat = Swap16(m_AudioFormat);
                m_NumChannels = Swap16(m_NumChannels);
                m_SampleRate = Swap32(m_SampleRate);
                m_ByteRate = Swap32(m_ByteRate);
                m_BlockAlign = Swap16(m_BlockAlign);
                m_BitsPerSample = Swap16(m_BitsPerSample);
            }

        };

        struct DataChunk : CommonHeader
        {
            char m_Data[0];
            void Swap() {
                SwapHeader();
            }
        };

        struct DecodeStreamInfo {
            Info m_Info;
            uint32_t m_Cursor;
            const void* m_Buffer;
        };
    }

    static Result WavOpenStream(const void* buffer, uint32_t buffer_size, HDecodeStream* stream)
    {
        RiffHeader* header = (RiffHeader*) buffer;
        DecodeStreamInfo streamTemp;

        bool fmt_found = false;
        bool data_found = false;

        if (buffer_size < sizeof(RiffHeader)) {
            return RESULT_INVALID_FORMAT;
        }

        if (header->m_ChunkID == FOUR_CC('R', 'I', 'F', 'F') &&
            header->m_Format == FOUR_CC('W', 'A', 'V', 'E')) {

            const char* begin = (const char*) buffer;
            const char* current = (const char*) buffer;
            const char* end = (const char*) buffer + buffer_size;
            current += sizeof(RiffHeader);
            do {
                CommonHeader header;
                if (current + sizeof(header) > end) {
                    // not enough bytes left for a full header. just ignore this.
                    break;
                }

                memcpy(&header, current, sizeof(header));
                header.SwapHeader();
                if (header.m_ChunkID == FOUR_CC('f', 'm', 't', ' ')) {
                    FmtChunk fmt;
                    if (current + sizeof(fmt) > end) {
                        dmLogWarning("WAV sound data seems corrupt or truncated at position %d out of %d", (int)(current - begin), buffer_size);
                        return RESULT_INVALID_FORMAT;
                    }

                    memcpy(&fmt, current, sizeof(fmt));
                    fmt.Swap();
                    fmt_found = true;

                    if( fmt.m_AudioFormat != 1 )
                    {
                        dmLogWarning("Only wav-files with 8 or 16 bit PCM format (format=1) supported, got format=%d and bitdepth=%d", fmt.m_AudioFormat, fmt.m_BitsPerSample);
                        return RESULT_INVALID_FORMAT;
                    }
                    streamTemp.m_Info.m_Rate = fmt.m_SampleRate;
                    streamTemp.m_Info.m_Channels = fmt.m_NumChannels;
                    streamTemp.m_Info.m_BitsPerSample = fmt.m_BitsPerSample;

                } else if (header.m_ChunkID == FOUR_CC('d', 'a', 't', 'a')) {
                    // NOTE: We don't byte-swap PCM-data and a potential problem on big-endian architectures
                    DataChunk data;
                    if (current + sizeof(data) > end) {
                        dmLogWarning("WAV sound data seems corrupt or truncated at position %d out of %d", (int)(current - begin), buffer_size);
                        return RESULT_INVALID_FORMAT;
                    }

                    memcpy(&data, current, sizeof(data));
                    data.Swap();
                    streamTemp.m_Buffer = (void*) (current + sizeof(DataChunk));
                    streamTemp.m_Info.m_Size = data.m_ChunkSize;
                    data_found = true;
                }
                current += header.m_ChunkSize + sizeof(CommonHeader);
            } while (current < end && !(fmt_found && data_found));

            if (fmt_found && data_found) {
                // Allocate stream output and copy temporary data over there.
                // Doing this last-minute avoids having to worry about deallocating
                // on failure. NOTE: Maybe pool allocate here.
                streamTemp.m_Cursor = 0;
                DecodeStreamInfo *streamOut = new DecodeStreamInfo;
                *streamOut = streamTemp;
                *stream = streamOut;
                return RESULT_OK;
            } else {
                return RESULT_INVALID_FORMAT;
            }
        } else {
            return RESULT_INVALID_FORMAT;
        }
    }

    void WavCloseStream(HDecodeStream stream)
    {
        assert(stream);
        DecodeStreamInfo *streamInfo = (DecodeStreamInfo *) stream;
        delete streamInfo;
    }

    Result WavResetStream(HDecodeStream stream)
    {
        DecodeStreamInfo *streamInfo = (DecodeStreamInfo *) stream;
        streamInfo->m_Cursor = 0;
        return RESULT_OK;
    }

    Result WavDecodeStream(HDecodeStream stream, char* buffer, uint32_t buffer_size, uint32_t* decoded)
    {
        DecodeStreamInfo *streamInfo = (DecodeStreamInfo *) stream;

        DM_PROFILE(SoundCodec, "Wav")

        assert(streamInfo->m_Cursor <= streamInfo->m_Info.m_Size);
        uint32_t n = dmMath::Min(buffer_size, streamInfo->m_Info.m_Size - streamInfo->m_Cursor);
        *decoded = n;
        memcpy(buffer, (const char*) streamInfo->m_Buffer + streamInfo->m_Cursor, n);
        streamInfo->m_Cursor += n;
        return RESULT_OK;
    }

    Result WavSkipInStream(HDecodeStream stream, uint32_t bytes, uint32_t* skipped)
    {
        DecodeStreamInfo *streamInfo = (DecodeStreamInfo *) stream;
        assert(streamInfo->m_Cursor <= streamInfo->m_Info.m_Size);
        uint32_t n = dmMath::Min(bytes, streamInfo->m_Info.m_Size - streamInfo->m_Cursor);
        *skipped = n;
        streamInfo->m_Cursor += n;
        return RESULT_OK;
    }

    void WavGetInfo(HDecodeStream stream, struct Info* out)
    {
        *out = ((DecodeStreamInfo *)stream)->m_Info;
    }

    DM_DECLARE_SOUND_DECODER(AudioDecoderWav, "WavDecoder", FORMAT_WAV,
                             0,
                             WavOpenStream, WavCloseStream, WavDecodeStream, WavResetStream, WavSkipInStream, WavGetInfo);
}
