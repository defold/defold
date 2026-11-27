// Copyright 2020-2026 The Defold Foundation
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
#include <stdlib.h>
#include <string.h>

#include <dlib/math.h>

#include "sound_decoder.h"
#include <ogg/ogg.h>

extern "C"
{
#include "include/nogg.h"
#include "src/common.h"
#include "src/decode/common.h"
#include "src/decode/packet.h"
#include "src/util/float-to-int16.h"
}

namespace dmSoundCodec
{
    struct VorbisStream
    {
        dmSound::HSoundData m_Data;
        ogg_sync_state      m_Sync;
        ogg_stream_state    m_Ogg;
        stb_vorbis*         m_Decoder;
        uint8_t*            m_SetupPacket;
        uint32_t            m_SetupSize;
        Info                m_Info;
        float**             m_Frame;
        uint32_t            m_FrameOffset;
        uint32_t            m_FramesLeft;
        uint32_t            m_ReadOffset;
        int64_t             m_Position;
        int64_t             m_PageGranule;
        float               m_Scale;
        uint8_t             m_Identification[30];
        uint8_t             m_HeaderCount;
        bool                m_HaveStream;
        bool                m_FinalPage;
    };

    // Packet and frame pointers belong to their respective decoders until the next read.
    // A null packet with RESULT_OK means the resource is temporarily out of data.
    static Result NextPacket(VorbisStream* stream, ogg_packet* packet)
    {
        packet->packet = 0;
        for (;;)
        {
            if (stream->m_HaveStream)
            {
                int result = ogg_stream_packetout(&stream->m_Ogg, packet);
                if (result > 0)
                {
                    return RESULT_OK;
                }

                if (result < 0)
                {
                    return RESULT_DECODE_ERROR;
                }

                if (stream->m_FinalPage)
                {
                    return RESULT_END_OF_STREAM;
                }
            }

            ogg_page page;
            int result = ogg_sync_pageout(&stream->m_Sync, &page);
            if (result < 0)
            {
                return RESULT_DECODE_ERROR;
            }

            if (result > 0)
            {
                int serial = ogg_page_serialno(&page);
                if (!stream->m_HaveStream)
                {
                    if (!ogg_page_bos(&page))
                    {
                        return RESULT_INVALID_FORMAT;
                    }

                    if (ogg_stream_init(&stream->m_Ogg, serial) != 0)
                    {
                        return RESULT_OUT_OF_RESOURCES;
                    }

                    stream->m_HaveStream = true;
                }

                if (serial != stream->m_Ogg.serialno)
                {
                    continue;
                }

                if (ogg_stream_pagein(&stream->m_Ogg, &page) != 0)
                {
                    return RESULT_DECODE_ERROR;
                }

                stream->m_FinalPage = ogg_page_eos(&page) != 0;
                stream->m_PageGranule = ogg_page_granulepos(&page);
                continue;
            }

            const uint32_t block_size = 16 * 1024;
            char* data = ogg_sync_buffer(&stream->m_Sync, block_size);
            if (!data)
            {
                return RESULT_OUT_OF_RESOURCES;
            }

            uint32_t bytes = 0;
            dmSound::Result read = dmSound::SoundDataRead(stream->m_Data, stream->m_ReadOffset, block_size, data, &bytes);
            if (read == dmSound::RESULT_END_OF_STREAM)
            {
                return RESULT_END_OF_STREAM;
            }

            if (read == dmSound::RESULT_NO_DATA)
            {
                return RESULT_OK;
            }

            if (read != dmSound::RESULT_OK && read != dmSound::RESULT_PARTIAL_DATA)
            {
                return RESULT_DECODE_ERROR;
            }

            if (!bytes)
            {
                return RESULT_OK;
            }

            if (ogg_sync_wrote(&stream->m_Sync, bytes) != 0)
            {
                return RESULT_DECODE_ERROR;
            }

            stream->m_ReadOffset += bytes;
        }
    }

    static Result OpenPacketDecoder(VorbisStream* stream, const uint8_t* setup, uint32_t size)
    {
        if (stream->m_SetupSize == size && memcmp(stream->m_SetupPacket, setup, size) == 0)
        {
            // The first audio packet establishes the granule position and starts a new overlap window.
            stream->m_Decoder->previous_length = 0;
            stream->m_Decoder->first_decode = true;
            stream->m_Decoder->eof = false;
            stream->m_Decoder->error = VORBIS__no_error;
            return RESULT_OK;
        }

        if (stream->m_Decoder)
        {
            stb_vorbis_close(stream->m_Decoder);
            stream->m_Decoder = 0;
        }

        free(stream->m_SetupPacket);
        stream->m_SetupPacket = 0;
        stream->m_SetupSize = 0;

        // The packet decoder uses only the allocation callbacks in this context.
        // Zero callbacks select libnogg's aligned allocator for every stream.
        static vorbis_t allocator = {};
        int error;
        stb_vorbis* decoder = stb_vorbis_open_packet(&allocator, stream->m_Identification, sizeof(stream->m_Identification),
                                                     setup, (int32_t)size, 0, &error);
        if (!decoder)
        {
            return error == VORBIS_outofmem ? RESULT_OUT_OF_RESOURCES : RESULT_INVALID_FORMAT;
        }

        stb_vorbis_info info = stb_vorbis_get_info(decoder);
        if ((info.channels != 1 && info.channels != 2) ||
            (stream->m_Info.m_Channels && (info.channels != stream->m_Info.m_Channels || info.sample_rate != stream->m_Info.m_Rate)))
        {
            stb_vorbis_close(decoder);
            return RESULT_UNSUPPORTED;
        }

        stream->m_Decoder = decoder;
        stream->m_Info.m_Rate = info.sample_rate;
        stream->m_Info.m_Channels = (uint8_t)info.channels;
        // libogg owns the packet. Keep a copy to recognize the same setup on rewind or across a chain.
        stream->m_SetupPacket = (uint8_t*)malloc(size);
        if (stream->m_SetupPacket)
        {
            memcpy(stream->m_SetupPacket, setup, size);
            stream->m_SetupSize = size;
        }

        return RESULT_OK;
    }

    static Result ReadHeaders(VorbisStream* stream)
    {
        while (stream->m_HeaderCount < 3)
        {
            ogg_packet packet;
            Result result = NextPacket(stream, &packet);
            if (result != RESULT_OK || !packet.packet)
            {
                return result;
            }

            if (packet.bytes < 7 || memcmp(packet.packet + 1, "vorbis", 6) != 0)
            {
                return RESULT_INVALID_FORMAT;
            }

            if (stream->m_HeaderCount == 0)
            {
                if (packet.packet[0] != 1 || packet.bytes != sizeof(stream->m_Identification))
                {
                    return RESULT_INVALID_FORMAT;
                }

                if (memcmp(stream->m_Identification, packet.packet, sizeof(stream->m_Identification)) != 0)
                {
                    stream->m_SetupSize = 0;
                    memcpy(stream->m_Identification, packet.packet, sizeof(stream->m_Identification));
                }
            }
            else if (stream->m_HeaderCount == 1)
            {
                if (packet.packet[0] != 3)
                {
                    return RESULT_INVALID_FORMAT;
                }
            }
            else
            {
                if (packet.packet[0] != 5 || packet.bytes > INT32_MAX)
                {
                    return RESULT_INVALID_FORMAT;
                }

                result = OpenPacketDecoder(stream, packet.packet, (uint32_t)packet.bytes);
                if (result != RESULT_OK)
                {
                    return result;
                }
            }

            ++stream->m_HeaderCount;
        }

        return RESULT_OK;
    }

    static void ClearOggStream(VorbisStream* stream)
    {
        if (stream->m_HaveStream)
        {
            ogg_stream_clear(&stream->m_Ogg);
            stream->m_HaveStream = false;
        }

        stream->m_FinalPage = false;
        stream->m_HeaderCount = 0;
    }

    static void VorbisCloseStream(HDecodeStream handle)
    {
        VorbisStream* stream = (VorbisStream*)handle;
        if (stream->m_Decoder)
        {
            stb_vorbis_close(stream->m_Decoder);
        }

        free(stream->m_SetupPacket);
        ClearOggStream(stream);
        ogg_sync_clear(&stream->m_Sync);
        delete stream;
    }

    static Result VorbisOpenStream(dmSound::HSoundData data, HDecodeStream* out)
    {
        VorbisStream* stream = new VorbisStream();
        stream->m_Data = data;
        ogg_sync_init(&stream->m_Sync);
        Result result = ReadHeaders(stream);
        if (result != RESULT_OK || !stream->m_Decoder)
        {
            VorbisCloseStream(stream);
            return result == RESULT_OK || result == RESULT_END_OF_STREAM ? RESULT_INVALID_FORMAT : result;
        }

        dmSound::DecoderOutputSettings settings;
        dmSound::GetDecoderOutputSettings(&settings);
        stream->m_Info.m_IsInterleaved = settings.m_UseInterleaved;
        // Core OpenAL supports interleaved 16-bit PCM on every device.
        stream->m_Info.m_BitsPerSample = settings.m_UseInterleaved ? 16 : 32;
        stream->m_Scale = settings.m_UseNormalizedFloatRange ? 1.0f : 32767.0f;
        *out = stream;
        return RESULT_OK;
    }

    static Result ReadFrame(VorbisStream* stream)
    {
        while (stream->m_FramesLeft == 0)
        {
            if (stream->m_HeaderCount < 3)
            {
                Result result = ReadHeaders(stream);
                if (result != RESULT_OK || stream->m_HeaderCount < 3)
                {
                    return result;
                }
            }

            ogg_packet packet;
            Result result = NextPacket(stream, &packet);
            if (result == RESULT_END_OF_STREAM && stream->m_FinalPage)
            {
                // The next logical stream may already be buffered in m_Sync.
                ClearOggStream(stream);
                continue;
            }

            if (result != RESULT_OK || !packet.packet)
            {
                return result;
            }

            if (packet.bytes == 0 || (packet.packet[0] & 1))
            {
                continue;
            }

            if (packet.bytes > INT32_MAX)
            {
                return RESULT_DECODE_ERROR;
            }

            // Supply the same granule information as libnogg's synchronous Ogg reader.
            // The final page can trim audio as early as its penultimate packet.
            stb_vorbis* decoder = stream->m_Decoder;
            decoder->end_seg_with_known_loc = packet.granulepos >= 0 ? decoder->last_seg_index : -2;
            decoder->known_loc_for_packet = (uint64_t)stream->m_PageGranule;
            decoder->page_flag = stream->m_FinalPage ? PAGEFLAG_last_page : 0;
            int frames;
            if (!stb_vorbis_decode_packet_float(decoder, packet.packet, (int32_t)packet.bytes, &stream->m_Frame, &frames))
            {
                return RESULT_DECODE_ERROR;
            }

            stream->m_FrameOffset = 0;
            stream->m_FramesLeft = (uint32_t)frames;
        }

        return RESULT_OK;
    }

    static Result VorbisDecodeStream(HDecodeStream handle, char* buffers[], uint32_t buffer_size, uint32_t* decoded)
    {
        VorbisStream* stream = (VorbisStream*)handle;
        *decoded = 0;
        uint32_t channels = stream->m_Info.m_Channels;
        uint32_t stride = stream->m_Info.m_IsInterleaved ? sizeof(int16_t) * channels : sizeof(float);
        uint32_t frames = buffer_size / stride;
        uint32_t done = 0;
        while (done < frames)
        {
            Result result = ReadFrame(stream);
            if (result != RESULT_OK || !stream->m_FramesLeft)
            {
                return result == RESULT_END_OF_STREAM && done ? RESULT_OK : result;
            }

            uint32_t count = dmMath::Min(frames - done, stream->m_FramesLeft);
            if (buffers && stream->m_Info.m_IsInterleaved)
            {
                float* src[2];
                for (uint32_t c = 0; c < channels; ++c)
                {
                    src[c] = stream->m_Frame[c] + stream->m_FrameOffset;
                }

                int16_t* dst = (int16_t*)buffers[0] + done * channels;
                // libnogg's SSE2 stereo kernel uses aligned loads and stores.
                if (channels == 2 && (((uintptr_t)dst | (uintptr_t)src[0] | (uintptr_t)src[1]) & 15) == 0)
                {
                    float_to_int16_interleave_2(dst, src[0], src[1], count);
                }
                else
                {
                    float_to_int16_interleave(dst, src, channels, count);
                }
            }
            else if (buffers)
            {
                for (uint32_t c = 0; c < channels; ++c)
                {
                    const float* src = stream->m_Frame[c] + stream->m_FrameOffset;
                    float* dst = (float*)buffers[c] + done;
                    for (uint32_t i = 0; i < count; ++i)
                    {
                        dst[i] = src[i] * stream->m_Scale;
                    }
                }
            }

            done += count;
            stream->m_FrameOffset += count;
            stream->m_FramesLeft -= count;
            stream->m_Position += count;
            *decoded = done * stride;
        }

        return RESULT_OK;
    }

    static Result VorbisResetStream(HDecodeStream handle)
    {
        VorbisStream* stream = (VorbisStream*)handle;
        ClearOggStream(stream);
        ogg_sync_reset(&stream->m_Sync);
        stream->m_FramesLeft = 0;
        stream->m_ReadOffset = 0;
        stream->m_Position = 0;
        return RESULT_OK;
    }

    static Result VorbisSkipInStream(HDecodeStream stream, uint32_t bytes, uint32_t* skipped)
    {
        return VorbisDecodeStream(stream, 0, bytes, skipped);
    }

    static void VorbisGetInfo(HDecodeStream handle, Info* info)
    {
        *info = ((VorbisStream*)handle)->m_Info;
    }

    static int64_t VorbisGetInternalPos(HDecodeStream handle)
    {
        return ((VorbisStream*)handle)->m_Position;
    }

    DM_DECLARE_SOUND_DECODER(AudioDecoderVorbis, "VorbisDecoder", FORMAT_VORBIS, 5,
                             VorbisOpenStream, VorbisCloseStream, VorbisDecodeStream, VorbisResetStream, VorbisSkipInStream, VorbisGetInfo, VorbisGetInternalPos);
}
