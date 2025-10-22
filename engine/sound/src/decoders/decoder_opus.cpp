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
#include <dlib/array.h>
#include <dlib/index_pool.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/profile.h>

#include "opus/opus.h"
#include "sound_codec.h"
#include "sound_decoder.h"

#include <string.h>

#define STREAM_BLOCK_SIZE (16 << 10)

namespace dmSoundCodec
{
    namespace
    {
        struct DecodestreamInfo
        {
            Info                m_Info;
            bool                m_NormalizedOutput;
            uint32_t            m_stream_serial;
            uint32_t            m_SamplePos;
            uint32_t            m_DecodeSamplePos;
            uint16_t            m_PreSkip;
            float               m_SampleScale;
            OpusDecoder*        m_Decoder;
            dmSound::HSoundData m_SoundData;
            uint32_t            m_StreamOffset;
            dmArray<uint8_t>    m_DataBuffer;
            uint32_t            m_LastOutputOffset;
            dmArray<uint8_t>    m_LastOutput;
            uint8_t             m_LacingTable[255];
            uint8_t             m_LacingTableSize;
            uint32_t            m_LacingTableIndex;
            uint32_t            m_SkipBytes;
            uint8_t             m_PacketBuffer[2048];
            uint32_t            m_PacketOffset;
            bool                m_bEOS : 1;
        };
    } // namespace

    static void CleanupBuffer(dmArray<uint8_t>& buffer, uint32_t consumed)
    {
        // Move sill un-consumed buffer contents to the front so we offer them in one contigous chunk to Vorbis at all times
        if (consumed == 0)
            return;

        assert(consumed <= buffer.Size());

        uint32_t n = buffer.Size();
        uint8_t* data = buffer.Begin();
        for (uint32_t i = consumed; i < n; ++i)
            data[i - consumed] = data[i];
        buffer.SetSize(n - consumed);
    }

    static uint32_t Read16Bit(const uint8_t* src)
    {
        return src[0] | ((uint32_t)src[1] << 8);
    }

    static uint32_t Read32Bit(const uint8_t* src)
    {
        return src[0] | ((uint32_t)src[1] << 8) | ((uint32_t)src[2] << 16) | ((uint32_t)src[3] << 24);
    }

    /*
        static uint64_t Read64Bit(const uint8_t* src)
        {
            return            src[0]        | ((uint64_t)src[1] <<  8) | ((uint64_t)src[2] << 16) | ((uint64_t)src[3] << 24) |
                   ((uint64_t)src[4] << 32) | ((uint64_t)src[5] << 40) | ((uint64_t)src[6] << 48) | ((uint64_t)src[7] << 56);
        }
    */

    static void EnsureDataRead(DecodestreamInfo* stream_info, dmSound::HSoundData sound_data, uint32_t min_bytes_needed)
    {
        if (stream_info->m_DataBuffer.Size() < min_bytes_needed)
        {
            uint32_t        read_size;
            dmSound::Result res = dmSound::SoundDataRead(sound_data, stream_info->m_StreamOffset, min_bytes_needed - stream_info->m_DataBuffer.Size(), stream_info->m_DataBuffer.End(), &read_size);
            if (res == dmSound::RESULT_OK || res == dmSound::RESULT_PARTIAL_DATA)
            {
                stream_info->m_DataBuffer.SetSize(stream_info->m_DataBuffer.Size() + read_size);
                stream_info->m_StreamOffset += read_size;
            }
            else
            {
                stream_info->m_bEOS = (res == dmSound::RESULT_END_OF_STREAM);
            }
        }
    }

    static bool ReadPageHeader(DecodestreamInfo* stream_info, dmSound::HSoundData sound_data, uint8_t& flags, uint32_t& stream_serial, uint32_t& page_size, uint8_t& num_page_segments, uint8_t lacing_table[255])
    {
        EnsureDataRead(stream_info, sound_data, 27 + 255); // 27 bytes page header and max. 255 lacing table

        if (stream_info->m_DataBuffer.Size() >= 4)
        {
            const char* sync_mark = (const char*)stream_info->m_DataBuffer.Begin();

            // [0] == 'OggS'
            // [4] == 0x00
            // [5] == Header type flags (0x01 - continued packet; 0x02 first page of stream; 0x04 last page of stream)
            // [6..13] -> 64-bit absolute granule position
            // [14..17] -> 32-bit serial of this stream in the file
            // [18..21] -> 32-bit page sequence number
            // [22..25] -> 32-bit checksum
            // [26] -> bytes in lacing table
            // [...] lacing table

            if (sync_mark[0] == 'O' && sync_mark[1] == 'g' && sync_mark[2] == 'g' && sync_mark[3] == 'S')
            {
                if (stream_info->m_DataBuffer.Size() >= 27)
                {
                    flags = stream_info->m_DataBuffer[5];
                    stream_serial = Read32Bit(&stream_info->m_DataBuffer[14]);

                    num_page_segments = stream_info->m_DataBuffer[26];

                    if (stream_info->m_DataBuffer.Size() >= (27 + num_page_segments))
                    {
                        page_size = 0;
                        const uint8_t* lt = (const uint8_t*)&stream_info->m_DataBuffer[27];
                        for (uint32_t i = 0; i < num_page_segments; ++i)
                        {
                            page_size += (lacing_table[i] = lt[i]);
                        }

                        CleanupBuffer(stream_info->m_DataBuffer, 27 + num_page_segments);
                        return true;
                    }
                }
            }
            else
            {
                // NOTE: this code will ultimately seek through the file, but it will posibly take a LONG time to find the next sync marker
                // BUT: defacto it should never be triggered with any correctly formed stream! (as we do not randomly jump into the data)
                //  As we read from a reliable source we really should not need to resync, but if we would: this is a very slow way of moving the file position to find the next 'OggS' mark
                CleanupBuffer(stream_info->m_DataBuffer, 1);
            }
        }

        return false;
    }

    // OPT: to simplify the usage we do copy data for the next packet as needed, BUT: we could also only return a pointer to it if...
    //  a) it does not cross a page boundary
    //  b) the read buffer does not get reconfigured before the data is used in the decoder
    static bool ReadNextPacket(DecodestreamInfo* stream_info, dmSound::HSoundData sound_data, uint8_t* out_buffer, uint32_t& out_bytes)
    {
        uint32_t max_out_bytes = out_bytes;
        out_bytes = 0;
        bool is_ok = false;
        do
        {
            uint8_t  flags;
            uint32_t page_size;
            uint32_t stream_serial;

            if (stream_info->m_LacingTableIndex >= 256 && stream_info->m_SkipBytes == 0)
            {
                if (!ReadPageHeader(stream_info, sound_data, flags, stream_serial, page_size, stream_info->m_LacingTableSize, stream_info->m_LacingTable))
                {
                    break;
                }

                // This is the stream we are working with?
                bool is_data = false;
                if (stream_serial == stream_info->m_stream_serial)
                {
                    EnsureDataRead(stream_info, sound_data, 4);
                    const char* magic = (const char*)stream_info->m_DataBuffer.Begin();
                    // we need to avoid the Opushead and Opustags packets (the first one if we should restart the stream, the second one all the time as we do not explicitly parse it)
                    is_data = (magic[0] != 'O' || magic[1] != 'p' || magic[2] != 'u' || magic[3] != 's');
                }
                if (is_data)
                {
                    // Yes. Reset the lacing index...
                    stream_info->m_LacingTableIndex = 0;
                }
                else
                {
                    stream_info->m_SkipBytes = page_size;
                }
            }

            // Valid data accessible?
            if (stream_info->m_LacingTableIndex < 256)
            {
                uint8_t segment_size = stream_info->m_LacingTable[stream_info->m_LacingTableIndex];

                if (segment_size > 0)
                {
                    EnsureDataRead(stream_info, sound_data, STREAM_BLOCK_SIZE);

                    if (stream_info->m_DataBuffer.Size() < segment_size)
                    {
                        break;
                    }

                    if (max_out_bytes < (stream_info->m_PacketOffset + segment_size))
                    {
                        assert(!"Larger packet buffer needed!");
                        break;
                    }

                    memcpy(out_buffer + stream_info->m_PacketOffset, stream_info->m_DataBuffer.Begin(), segment_size);

                    CleanupBuffer(stream_info->m_DataBuffer, segment_size);
                    stream_info->m_PacketOffset += segment_size;
                }

                if (++stream_info->m_LacingTableIndex == stream_info->m_LacingTableSize)
                {
                    stream_info->m_LacingTableIndex = 256;
                }

                if (segment_size < 255)
                {
                    out_bytes = stream_info->m_PacketOffset;
                    stream_info->m_PacketOffset = 0;
                    is_ok = true;
                    break;
                }
            }
            else
            {
                // Skip however many bytes we need to skip...
                while (stream_info->m_SkipBytes)
                {
                    if (stream_info->m_DataBuffer.Empty())
                    {
                        uint32_t        read_size;
                        dmSound::Result res = dmSound::SoundDataRead(sound_data, stream_info->m_StreamOffset, STREAM_BLOCK_SIZE, stream_info->m_DataBuffer.Begin(), &read_size);
                        if (res == dmSound::RESULT_OK || res == dmSound::RESULT_PARTIAL_DATA)
                        {
                            stream_info->m_DataBuffer.SetSize(read_size);
                            stream_info->m_StreamOffset += read_size;
                        }
                        else
                        {
                            stream_info->m_bEOS = (res == dmSound::RESULT_END_OF_STREAM);
                            break;
                        }
                    }
                    uint32_t chunk = dmMath::Min(stream_info->m_DataBuffer.Size(), stream_info->m_SkipBytes);
                    CleanupBuffer(stream_info->m_DataBuffer, chunk);
                    stream_info->m_SkipBytes -= chunk;
                }
            }
        } while (!(stream_info->m_bEOS && stream_info->m_DataBuffer.Empty()));
        return is_ok;
    }

    static Result OpusOpenStream(dmSound::HSoundData sound_data, HDecodeStream* stream)
    {
        //
        // Note: the code below assumes all data needed for intialization of the stream is
        //       already available and does NOT need to be stalled for!
        //
        DecodestreamInfo* stream_info = new DecodestreamInfo;
        stream_info->m_DataBuffer.SetCapacity(STREAM_BLOCK_SIZE);
        stream_info->m_StreamOffset = 0;
        stream_info->m_SoundData = sound_data;
        stream_info->m_LacingTableIndex = 256;
        stream_info->m_PacketOffset = 0;
        stream_info->m_Decoder = NULL;
        stream_info->m_SkipBytes = 0;

        // Prefetch some data right away...
        uint32_t        read_size;
        dmSound::Result res = dmSound::SoundDataRead(sound_data, 0, STREAM_BLOCK_SIZE, stream_info->m_DataBuffer.Begin(), &read_size);

        if (res != dmSound::RESULT_OK && res != dmSound::RESULT_PARTIAL_DATA)
        {
            delete stream_info;
            return RESULT_INVALID_FORMAT;
        }

        stream_info->m_DataBuffer.SetSize(read_size);
        stream_info->m_StreamOffset = read_size;
        stream_info->m_bEOS = (res == dmSound::RESULT_END_OF_STREAM);
        stream_info->m_SamplePos = 0;
        stream_info->m_DecodeSamplePos = 0;

        bool is_done = false;
        do
        {
            uint8_t  lacing_table[255];
            uint8_t  flags;
            uint32_t page_size;
            uint8_t  num_page_segments;
            if (ReadPageHeader(stream_info, sound_data, flags, stream_info->m_stream_serial, page_size, num_page_segments, lacing_table))
            {
                assert(page_size <= STREAM_BLOCK_SIZE);

                // Refill buffer with whatever next page we found... (if it's not yet in there)
                EnsureDataRead(stream_info, sound_data, STREAM_BLOCK_SIZE);
                if (stream_info->m_DataBuffer.Size() < page_size)
                {
                    delete stream_info;
                    return RESULT_INVALID_FORMAT;
                }

                // Long enough for a OpusHeader & start of a new logical stream?
                if (page_size <= stream_info->m_DataBuffer.Size() && page_size >= 19 && flags == 0x02)
                {
                    const uint8_t* opusHead = (const uint8_t*)stream_info->m_DataBuffer.Begin();

                    // Magic ok?
                    if (opusHead[0] == 'O' && opusHead[1] == 'p' && opusHead[2] == 'u' && opusHead[3] == 's' && opusHead[4] == 'H' && opusHead[5] == 'e' && opusHead[6] == 'a' && opusHead[7] == 'd')
                    {
                        // Yes. Version ok?
                        if (opusHead[8] == 0x01)
                        {
                            dmSound::DecoderOutputSettings settings;
                            dmSound::GetDecoderOutputSettings(&settings);

                            stream_info->m_NormalizedOutput = settings.m_UseNormalizedFloatRange;

                            stream_info->m_Info.m_Channels = opusHead[9];
                            stream_info->m_Info.m_BitsPerSample = 32;
                            stream_info->m_Info.m_IsInterleaved = settings.m_UseInterleaved;

                            static const uint16_t rates[] = { 8000, 12000, 16000, 24000, 48000, 0 };

                            const uint32_t        maxRefRate = 48000;

                            // Gather original sample rate (prior to any resampling during encode) if available. Otherwise assume: 48KHz per Opus recommendation.
                            uint32_t orgRate = Read32Bit(&opusHead[12]);
                            uint32_t targetRate = (orgRate > 0) ? dmMath::Min(orgRate, maxRefRate) : maxRefRate;

                            // Try finding a supported rate just above that rate
                            stream_info->m_Info.m_Rate = maxRefRate;
                            for (uint32_t i = 0; rates[i]; ++i)
                            {
                                if (rates[i] >= targetRate)
                                {
                                    stream_info->m_Info.m_Rate = rates[i];
                                    break;
                                }
                            }

                            stream_info->m_Info.m_Size = 0; // unknown

                            stream_info->m_PreSkip = Read16Bit(&opusHead[10]);

                            uint16_t gain = Read16Bit(&opusHead[16]);
                            stream_info->m_SampleScale = (gain == 0) ? 1.0f : powf(10.0f, gain / (20.0f * 256.0f));

                            uint8_t channelMapFamily = opusHead[18];

                            if (channelMapFamily == 0)
                            {
                                // For now we only support 1 or 2 channels - which corresponds to mapping family zero

                                stream_info->m_LastOutput.SetCapacity(5720 * stream_info->m_Info.m_Channels * sizeof(float));

                                CleanupBuffer(stream_info->m_DataBuffer, page_size);
                                is_done = true;
                                break;
                            }
                        }

                        delete stream_info;
                        return RESULT_INVALID_FORMAT;
                    }
                }

                CleanupBuffer(stream_info->m_DataBuffer, dmMath::Min(page_size, stream_info->m_DataBuffer.Size()));
            }
        } while (!is_done && !(stream_info->m_bEOS && stream_info->m_DataBuffer.Empty()));

        if (!is_done)
        {
            delete stream_info;
            return RESULT_INVALID_FORMAT;
        }

        int error;
        stream_info->m_Decoder = opus_decoder_create(stream_info->m_Info.m_Rate, stream_info->m_Info.m_Channels, &error);

        if (stream_info->m_Decoder == NULL)
        {
            delete stream_info;
            return RESULT_INVALID_FORMAT;
        }

        *stream = stream_info;
        return RESULT_OK;
    }

    static Result OpusDecode(HDecodeStream stream, char* buffer[], uint32_t buffer_size, uint32_t* decoded)
    {
        DecodestreamInfo* stream_info = (DecodestreamInfo*)stream;

        DM_PROFILE(__FUNCTION__);

        uint32_t stride_inout = stream_info->m_Info.m_IsInterleaved ? (stream_info->m_Info.m_Channels * sizeof(float)) : sizeof(float);

        uint32_t needed_frames = buffer_size / stride_inout;
        uint32_t done_frames = 0;

        uint32_t stride = stream_info->m_Info.m_Channels * sizeof(float);

        while (done_frames < needed_frames)
        {
            // Do we still have output from the last decode?
            if (stream_info->m_LastOutput.Empty())
            {
                // Read next packet data (if incomplete this will fail and needs to be called again later)
                uint32_t out_bytes = sizeof(stream_info->m_PacketBuffer);
                if (ReadNextPacket(stream_info, stream_info->m_SoundData, stream_info->m_PacketBuffer, out_bytes))
                {
                    // Trigger a decode for the complete packet we now gathered...
                    int frame_size = opus_decode_float(stream_info->m_Decoder, stream_info->m_PacketBuffer, out_bytes, (float*)stream_info->m_LastOutput.Begin(), stream_info->m_LastOutput.Capacity() / stride, 0);
                    if (frame_size > 0)
                    {
                        uint32_t newDecodePos = stream_info->m_DecodeSamplePos + frame_size;
                        if (stream_info->m_DecodeSamplePos < stream_info->m_PreSkip)
                        {
                            uint32_t skip = dmMath::Min(stream_info->m_PreSkip - stream_info->m_DecodeSamplePos, (uint32_t)frame_size);
                            stream_info->m_LastOutputOffset = skip;
                        }
                        else
                        {
                            stream_info->m_LastOutputOffset = 0;
                        }
                        stream_info->m_LastOutput.SetSize(frame_size * stride);
                        stream_info->m_DecodeSamplePos = newDecodePos;
                    }
                    else
                    {
                        return RESULT_DECODE_ERROR;
                    }
                }
                else
                {
                    break;
                }
            }

            // Produce sample output from decoder results
            if (!stream_info->m_LastOutput.Empty())
            {
                uint32_t out_frames = dmMath::Min((stream_info->m_LastOutput.Size() / stride) - stream_info->m_LastOutputOffset, needed_frames - done_frames);

                // This might be called with a NULL buffer to avoid delivering data, so we need to check...
                if (buffer)
                {
                    if (stream_info->m_NormalizedOutput && stream_info->m_Info.m_IsInterleaved)
                    {
                        float    scale = stream_info->m_SampleScale;
                        uint32_t nc = stream_info->m_Info.m_Channels;

                        float* dest = (float*)buffer[0] + done_frames * nc;
                        const float* out = (float*)(stream_info->m_LastOutput.Begin() + stride * stream_info->m_LastOutputOffset);
                        uint32_t ns = nc * out_frames;
                        for (uint32_t s = 0; s < ns; ++s)
                        {
                            *(dest++) = *(out++) * scale;
                        }
                    }
                    else
                    {
                        assert(!stream_info->m_Info.m_IsInterleaved);

                        // SIMD? Any noticable cost? Simpler code for single channel...
                        //  note: Opus float format is normalized, hence we need to scale things up (not only due to the scale in the data)
                        float    scale = stream_info->m_SampleScale * 32767.0f;
                        uint32_t nc = stream_info->m_Info.m_Channels;

                        for (uint32_t c = 0; c < nc; ++c)
                        {
                            float*       dest = (float*)buffer[c] + done_frames;
                            const float* out = (float*)(stream_info->m_LastOutput.Begin() + stride * stream_info->m_LastOutputOffset) + c;
                            for (uint32_t i = 0; i < out_frames; ++i)
                            {
                                *(dest++) = *out * scale;
                                out += nc;
                            }
                        }
                    }
                }

                done_frames += out_frames;
                stream_info->m_SamplePos += out_frames;

                stream_info->m_LastOutputOffset += out_frames;
                if (stream_info->m_LastOutputOffset * stride >= stream_info->m_LastOutput.Size())
                {
                    stream_info->m_LastOutput.SetSize(0);
                }
            }
        }

        *decoded = done_frames * stride_inout;
        return (stream_info->m_bEOS && done_frames == 0) ? RESULT_END_OF_STREAM : RESULT_OK;
    }

    static Result OpusResetStream(HDecodeStream stream)
    {
        // Reset all to re-stream the data (the decoder will not be reiinitialized & header pages are not re-analyzed)
        DecodestreamInfo* stream_info = (DecodestreamInfo*)stream;
        stream_info->m_StreamOffset = 0;
        stream_info->m_LacingTableIndex = 256;
        stream_info->m_PacketOffset = 0;
        stream_info->m_SkipBytes = 0;
        stream_info->m_PacketOffset = 0;
        stream_info->m_bEOS = false;
        stream_info->m_DecodeSamplePos = 0;
        stream_info->m_SamplePos = 0;
        stream_info->m_DataBuffer.SetSize(0);
        stream_info->m_LastOutput.SetSize(0);

        opus_decoder_init(stream_info->m_Decoder, stream_info->m_Info.m_Rate, stream_info->m_Info.m_Channels);

        return RESULT_OK;
    }

    static Result OpusSkipInStream(HDecodeStream stream, uint32_t num_bytes, uint32_t* skipped)
    {
        // Note: passing in NULL will not safe a lot of cycles, but we are able to avoid receiving any data
        // (we currently do tno really seek (forward), while the overall structure of Ogg and Opus would allow to add support eventually)
        return OpusDecode(stream, NULL, num_bytes, skipped);
    }

    static void OpusCloseStream(HDecodeStream stream)
    {
        DecodestreamInfo* stream_info = (DecodestreamInfo*)stream;
        opus_decoder_destroy(stream_info->m_Decoder);
        delete stream_info;
    }

    static void OpusGetInfo(HDecodeStream stream, struct Info* out)
    {
        *out = ((DecodestreamInfo*)stream)->m_Info;
    }

    static int64_t OpusGetInternalPos(HDecodeStream stream)
    {
        DecodestreamInfo* stream_info = (DecodestreamInfo*)stream;
        return stream_info->m_SamplePos;
    }

    DM_DECLARE_SOUND_DECODER(AudioDecoderOpus, "OpusDecoder", FORMAT_OPUS,
                             5, // baseline score (1-10)
                             OpusOpenStream,
                             OpusCloseStream,
                             OpusDecode,
                             OpusResetStream,
                             OpusSkipInStream,
                             OpusGetInfo,
                             OpusGetInternalPos);
} // namespace dmSoundCodec
