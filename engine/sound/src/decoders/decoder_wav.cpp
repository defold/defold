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
            uint32_t m_BufferOffset;
            dmSound::HSoundData m_SoundData;
        };
    }

    static Result WavOpenStream(dmSound::HSoundData sound_data, HDecodeStream* stream)
    {
        // note: the code below assumes enough data to be present to evaluate a WAV files structure
        // (this also assumes all format / header chunks to be situated BEFORE the data chunk! - at least if the data is streamed)

        RiffHeader header;
        DecodeStreamInfo streamTemp;

        streamTemp.m_SoundData = sound_data;

        bool fmt_found = false;
        bool data_found = false;

        uint32_t nread;
        dmSound::Result res = dmSound::SoundDataRead(sound_data, 0, sizeof(header), &header, &nread);
        if (res < dmSound::RESULT_OK)
        {
            dmLogError("Failed to read riff header: %d", res);
            return RESULT_INVALID_FORMAT;
        }

        if (nread < sizeof(header))
        {
            dmLogWarning("Available data size too small for riff header");
            return RESULT_INVALID_FORMAT;
        }

        if (header.m_ChunkID == FOUR_CC('R', 'I', 'F', 'F') &&
            header.m_Format == FOUR_CC('W', 'A', 'V', 'E')) {

            uint32_t current_offset = sizeof(header);
            do {
                CommonHeader header, org_header;

                res = dmSound::SoundDataRead(sound_data, current_offset, sizeof(org_header), &org_header, &nread);
                if (res < dmSound::RESULT_OK)
                {
                    dmLogError("Failed to read common header: %d", res);
                    break;
                }

                if (nread < sizeof(org_header))
                {
                    // not enough bytes left for a full header. just ignore this.
                    break;
                }

                memcpy(&header, &org_header, sizeof(header));
                header.SwapHeader();

                if (header.m_ChunkID == FOUR_CC('f', 'm', 't', ' ')) {
                    FmtChunk fmt;

                    memcpy(&fmt, &org_header, sizeof(CommonHeader));
                    res = dmSound::SoundDataRead(sound_data, current_offset + sizeof(CommonHeader), sizeof(fmt) - sizeof(CommonHeader), (void*)((uintptr_t)&fmt + sizeof(CommonHeader)), &nread);
                    if (res < dmSound::RESULT_OK)
                    {
                        dmLogError("WAV sound data seems corrupt or truncated: %d", res);
                        return RESULT_INVALID_FORMAT;
                    }
                    fmt.Swap();
                    fmt_found = true;

                    if( fmt.m_AudioFormat != 1 )
                    {
                        dmLogError("Only wav-files with 8 or 16 bit PCM format (format=1) supported, got format=%d and bitdepth=%d", fmt.m_AudioFormat, fmt.m_BitsPerSample);
                        return RESULT_INVALID_FORMAT;
                    }
                    streamTemp.m_Info.m_Rate = fmt.m_SampleRate;
                    streamTemp.m_Info.m_Channels = fmt.m_NumChannels;
                    streamTemp.m_Info.m_BitsPerSample = fmt.m_BitsPerSample;

                } else if (header.m_ChunkID == FOUR_CC('d', 'a', 't', 'a')) {
                    data_found = true;

                    streamTemp.m_BufferOffset = current_offset + sizeof(DataChunk);
                    streamTemp.m_Info.m_Size = header.m_ChunkSize;
                }
                // note: we assume the Riff header, format chunk and data chunk to roughly appear in this order and close proximity. Theoretically we might see WAV files that do NOT follow this pattern!
//TODO: ^^^ WE SHOULD CATCH THAT! FILES LIKE THAT WOULD NOT BE SUITABLE FOR STREAMING IN THIS MANNER! --> WE WOULD NEED READ DATA CONVERSION!!!
                current_offset += sizeof(CommonHeader) + header.m_ChunkSize;

            } while (!(fmt_found && data_found));

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
                dmLogWarning("Format (%d) or data (%d)  not found", fmt_found, data_found);
                return RESULT_INVALID_FORMAT;
            }
        } else {
            const char* chunk = (const char*)&header.m_ChunkID;
            dmLogWarning("Wav: Unknown header: chunk: %08x %c%c%c%c  format: %08x", header.m_ChunkID,
                        (char)chunk[0], (char)chunk[1], (char)chunk[2], (char)chunk[3], header.m_Format);
            return RESULT_INVALID_FORMAT;
        }
    }

    static void WavCloseStream(HDecodeStream stream)
    {
        assert(stream);
        DecodeStreamInfo *streamInfo = (DecodeStreamInfo *) stream;
        delete streamInfo;
    }

    static Result WavResetStream(HDecodeStream stream)
    {
        DecodeStreamInfo *streamInfo = (DecodeStreamInfo *) stream;
        streamInfo->m_Cursor = 0;
        return RESULT_OK;
    }

    static Result WavDecodeStream(HDecodeStream stream, char* buffer, uint32_t buffer_size, uint32_t* decoded)
    {
        DecodeStreamInfo *streamInfo = (DecodeStreamInfo *) stream;
        DM_PROFILE(__FUNCTION__);

        assert(streamInfo->m_Cursor <= streamInfo->m_Info.m_Size);
        uint32_t n = dmMath::Min(buffer_size, streamInfo->m_Info.m_Size - streamInfo->m_Cursor);

        // WAV files can contain data beyond the end of the data chunk. Hence the EOS of the reader is not the only thing tgat can trigger an EOS logically!
        if (n == 0) {
            *decoded = 0;
            return RESULT_END_OF_STREAM;
        }

        uint32_t read_size;
        dmSound::Result res = dmSound::SoundDataRead(streamInfo->m_SoundData, streamInfo->m_BufferOffset + streamInfo->m_Cursor, n, buffer, &read_size);
        if (res == dmSound::RESULT_OK || res == dmSound::RESULT_PARTIAL_DATA)
        {
            *decoded = read_size;
            streamInfo->m_Cursor += read_size;
        }
        else
        {
            *decoded = 0;
        }

        return (res == dmSound::RESULT_END_OF_STREAM) ? RESULT_END_OF_STREAM : RESULT_OK;
    }

    static Result WavSkipInStream(HDecodeStream stream, uint32_t bytes, uint32_t* skipped)
    {
        DecodeStreamInfo *streamInfo = (DecodeStreamInfo *) stream;
        
        if (streamInfo->m_Cursor >= streamInfo->m_Info.m_Size) {
            *skipped = 0;
            return RESULT_END_OF_STREAM;
        }

        uint32_t n = dmMath::Min(bytes, streamInfo->m_Info.m_Size - streamInfo->m_Cursor);
        *skipped = n;
        streamInfo->m_Cursor += n;
        return RESULT_OK;
    }

    static void WavGetInfo(HDecodeStream stream, struct Info* out)
    {
        *out = ((DecodeStreamInfo *)stream)->m_Info;
    }

    static int64_t WavGetInternalPos(HDecodeStream stream)
    {
        DecodeStreamInfo *streamInfo = (DecodeStreamInfo *) stream;
        return streamInfo->m_Cursor;
    }

    DM_DECLARE_SOUND_DECODER(AudioDecoderWav, "WavDecoder", FORMAT_WAV,
                             0,
                             WavOpenStream, WavCloseStream, WavDecodeStream,
                             WavResetStream, WavSkipInStream, WavGetInfo,
                             WavGetInternalPos);
}
