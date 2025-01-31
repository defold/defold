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


#define STREAM_BLOCK_SIZE (4 << 10)


namespace dmSoundCodec
{
    namespace
    {
        struct DecodeStreamInfo {
            Info m_Info;
            uint32_t m_StreamSerial;
            uint32_t m_SamplePos;
            uint32_t m_DecodeSamplePos;
            uint16_t m_PreSkip;
            float m_SampleScale;
            OpusDecoder* m_Decoder;
            dmSound::HSoundData m_SoundData;
            uint32_t m_StreamOffset;
            dmArray<uint8_t> m_DataBuffer;
            uint32_t m_LastOutputOffset;
            dmArray<uint8_t> m_LastOutput;
            uint8_t m_LacingTable[255];
            uint8_t m_LacingTableSize;
            uint32_t m_LacingTableIndex;
            uint32_t m_SkipBytes;
            uint8_t m_PacketBuffer[2048];
            uint32_t m_PacketOffset;
            bool m_bEOS : 1;
        };
    }

    static void CleanupBuffer(dmArray<uint8_t>& buffer, uint32_t consumed)
    {
        // Move sill un-consumed buffer contents to the front so we offer them in one contigous chunk to Vorbis at all times
        if (consumed == 0)
            return;
        uint32_t n = buffer.Size();
        uint8_t* data = buffer.Begin();
        for(uint32_t i=consumed; i<n; ++i)
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

    static uint64_t Read64Bit(const uint8_t* src)
    {
        return            src[0]        | ((uint64_t)src[1] <<  8) | ((uint64_t)src[2] << 16) | ((uint64_t)src[3] << 24) |
               ((uint64_t)src[4] << 32) | ((uint64_t)src[5] << 40) | ((uint64_t)src[6] << 48) | ((uint64_t)src[7] << 56);
    }

    static bool ReadPageHeader(DecodeStreamInfo *streamInfo, dmSound::HSoundData sound_data, uint8_t& flags, uint32_t& streamSerial, uint32_t& pageSize, uint8_t& numPageSegments, uint8_t lacingTable[255])
    {
        const uint32_t minBytesNeeded = 27 + 255;    // 27 bytes page header and max. 255 lacing table

        // Make sure we got as much data as we could possibly need (if the stream can yield it)
        if (streamInfo->m_DataBuffer.Size() < minBytesNeeded) {
            uint32_t read_size;
            dmSound::Result res = dmSound::SoundDataRead(sound_data, streamInfo->m_StreamOffset, minBytesNeeded - streamInfo->m_DataBuffer.Size(), streamInfo->m_DataBuffer.End(), &read_size);
            if (res == dmSound::RESULT_OK || res == dmSound::RESULT_PARTIAL_DATA) {
                streamInfo->m_DataBuffer.SetSize(read_size);
                streamInfo->m_StreamOffset += read_size;
            }
            else {
                streamInfo->m_bEOS = (res == dmSound::RESULT_END_OF_STREAM);
            }
        }

        if (streamInfo->m_DataBuffer.Size() >= 4) {
            auto syncMark = (const char *)streamInfo->m_DataBuffer.Begin();

            // [0] == 'OggS'
            // [4] == 0x00
            // [5] == Header type flags (0x01 - continued packet; 0x02 first page of stream; 0x04 last page of stream)
            // [6..13] -> 64-bit absolute granule position
            // [14..17] -> 32-bit serial of this stream in the file
            // [18..21] -> 32-bit page sequence number
            // [22..25] -> 32-bit checksum
            // [26] -> bytes in lacing table
            // [...] lacing table

            if (syncMark[0] == 'O' && syncMark[1] == 'g' && syncMark[2] == 'g' && syncMark[3] == 'S') {
                if (streamInfo->m_DataBuffer.Size() >= 27) {

                    flags = streamInfo->m_DataBuffer[5];
                    streamSerial = Read32Bit(&streamInfo->m_DataBuffer[14]);

                    numPageSegments = streamInfo->m_DataBuffer[26];

                    if (streamInfo->m_DataBuffer.Size() >= (27 + numPageSegments)) {
                        pageSize = 0;
                        auto lt = (const uint8_t*)&streamInfo->m_DataBuffer[27];
                        for(uint32_t i=0; i<numPageSegments; ++i)
                        {
                            pageSize += (lacingTable[i] = lt[i]);
                        }

                        CleanupBuffer(streamInfo->m_DataBuffer, 27 + numPageSegments);
                        return true;
                    }
                }
            }
            else
            {
                // As we read from a reliable source we really should not need to resync, but if we would: this is a very slow way of moving the file position to find the next 'OggS' mark
                CleanupBuffer(streamInfo->m_DataBuffer, 1);
            }
        }

        return false;
    }

//OPT: to simplify the usage we do copy data for the next packet as needed, BUT: we could also only return a pointer to it if...
// a) it does not cross a page boundary
// b) the read buffer does not get reconfigured before the data is used in the decoder
    static bool ReadNextPacket(DecodeStreamInfo *streamInfo, dmSound::HSoundData sound_data, uint8_t* outBuffer, uint32_t& outBytes)
    {
        uint32_t maxOutBytes = outBytes;
        outBytes = 0;
        bool bOk = false;
        do {
            uint8_t flags;
            uint32_t pageSize;
            uint32_t streamSerial;

            if (streamInfo->m_LacingTableIndex >= 256 && streamInfo->m_SkipBytes == 0) {
                if (!ReadPageHeader(streamInfo, sound_data, flags, streamSerial, pageSize, streamInfo->m_LacingTableSize, streamInfo->m_LacingTable)) {
                    break;
                }

                // This is the stream we are working with?
                bool bIsData = false;
                if (streamSerial == streamInfo->m_StreamSerial) {
                    auto magic = (const char*)streamInfo->m_DataBuffer.Begin();
                    // we need to avoid the Opushead and Opustags packets (the first one if we should restart the stream, the second one all the time as we do not explicitly parse it)
                    bIsData = (magic[0] != 'O' || magic[1] != 'p' || magic[2] != 'u' || magic[3] != 's');
                }
                if (bIsData) {
                    // Yes. Reset the lacing index...
                    streamInfo->m_LacingTableIndex = 0;
                }
                else {
                    streamInfo->m_SkipBytes = pageSize;
                }
            }

            // Valid data accessible?
            if (streamInfo->m_LacingTableIndex < 256) {

                uint8_t segmentSize = streamInfo->m_LacingTable[streamInfo->m_LacingTableIndex];

                if (segmentSize > 0) {

                    if (!streamInfo->m_DataBuffer.Full()) {
                        uint32_t read_size;
                        dmSound::Result res = dmSound::SoundDataRead(sound_data, streamInfo->m_StreamOffset, STREAM_BLOCK_SIZE - streamInfo->m_DataBuffer.Size(), streamInfo->m_DataBuffer.End(), &read_size);
                        if (res == dmSound::RESULT_OK || res == dmSound::RESULT_PARTIAL_DATA) {
                            streamInfo->m_DataBuffer.SetSize(streamInfo->m_DataBuffer.Size() + read_size);
                            streamInfo->m_StreamOffset += read_size;
                        }
                        else {
                            streamInfo->m_bEOS = (res == dmSound::RESULT_END_OF_STREAM);
                        }
                    }

                    if (streamInfo->m_DataBuffer.Size() < segmentSize) {
                        break;
                    }

                    if (maxOutBytes < (streamInfo->m_PacketOffset + segmentSize))
                    {
                        assert(!"Larger packet buffer needed!");
                        break;
                    }

                    memcpy(outBuffer + streamInfo->m_PacketOffset, streamInfo->m_DataBuffer.Begin(), segmentSize);

                    CleanupBuffer(streamInfo->m_DataBuffer, segmentSize);
                    streamInfo->m_PacketOffset += segmentSize;
                }

                if (++streamInfo->m_LacingTableIndex == streamInfo->m_LacingTableSize) {
                    streamInfo->m_LacingTableIndex = 256;
                }

                if (segmentSize < 255) {
                    outBytes = streamInfo->m_PacketOffset;
                    streamInfo->m_PacketOffset = 0;
                    bOk = true;
                    break;
                }
            }
            else {
                // Skip however many bytes we need to skip...
                while(streamInfo->m_SkipBytes) {
                    if (streamInfo->m_DataBuffer.Empty()) {
                        uint32_t read_size;
                        dmSound::Result res = dmSound::SoundDataRead(sound_data, streamInfo->m_StreamOffset, STREAM_BLOCK_SIZE, streamInfo->m_DataBuffer.Begin(), &read_size);
                        if (res == dmSound::RESULT_OK || res == dmSound::RESULT_PARTIAL_DATA) {
                            streamInfo->m_DataBuffer.SetSize(read_size);
                            streamInfo->m_StreamOffset += read_size;
                        }
                        else {
                            streamInfo->m_bEOS = (res == dmSound::RESULT_END_OF_STREAM);
                            break;
                        }
                    }
                    uint32_t chunk = dmMath::Min(streamInfo->m_DataBuffer.Size(), streamInfo->m_SkipBytes);
                    CleanupBuffer(streamInfo->m_DataBuffer, chunk);
                    streamInfo->m_SkipBytes -= chunk;
                }
            }
        }
        while(!(streamInfo->m_bEOS && streamInfo->m_DataBuffer.Empty()));
        return bOk;
    }
    
    static Result OpusOpenStream(dmSound::HSoundData sound_data, HDecodeStream* stream)
    {
        DecodeStreamInfo *streamInfo = new DecodeStreamInfo;
        streamInfo->m_DataBuffer.SetCapacity(STREAM_BLOCK_SIZE);
        streamInfo->m_StreamOffset = 0;
        streamInfo->m_SoundData = sound_data;
        streamInfo->m_LacingTableIndex = 256;
        streamInfo->m_PacketOffset = 0;
        streamInfo->m_Decoder = NULL;

        uint32_t read_size;
        dmSound::Result res = dmSound::SoundDataRead(sound_data, 0, STREAM_BLOCK_SIZE, streamInfo->m_DataBuffer.Begin(), &read_size);

        if (res != dmSound::RESULT_OK && res != dmSound::RESULT_PARTIAL_DATA) {
            delete streamInfo;
            return RESULT_INVALID_FORMAT;
        }

        streamInfo->m_DataBuffer.SetSize(read_size);
        streamInfo->m_StreamOffset = read_size;
        streamInfo->m_bEOS = (res == dmSound::RESULT_END_OF_STREAM);
        streamInfo->m_SamplePos = 0;
        streamInfo->m_DecodeSamplePos = 0;

        bool bDone = false;
        do {
            uint8_t lacingTable[255];
            uint8_t flags;
            uint32_t pageSize;
            uint8_t numPageSegments;
            if (ReadPageHeader(streamInfo, sound_data, flags, streamInfo->m_StreamSerial, pageSize, numPageSegments, lacingTable)) {

                // Long enough for a OpusHeader & start of a new logical stream?
                if (pageSize >= sizeof(19) && flags == 0x02) {
                    auto opusHead = (const uint8_t *)streamInfo->m_DataBuffer.Begin();

                    // Magic ok?
                    if (opusHead[0] == 'O' && opusHead[1] == 'p' && opusHead[2] == 'u' && opusHead[3] == 's' && opusHead[4] == 'H' && opusHead[5] == 'e' && opusHead[6] == 'a' && opusHead[7] == 'd') {
                        // Yes. Version ok?
                        if (opusHead[8] == 0x01) {

                            streamInfo->m_Info.m_Channels = opusHead[9];
                            streamInfo->m_Info.m_BitsPerSample = 16;
                            streamInfo->m_Info.m_IsInterleaved  = true;

                            static const uint16_t rates[] = {8000, 12000, 16000, 24000, 48000, 0};

// COULD WE USE "MAX MIX RATE" TO AVOID ISSUES WITH US HAVING A SAMPLE ABOVE THE MIXER RATE (WHICH DEFOLD REJECTS - WHY PRECISELY?)
// ^^^ ISSUE: this will likely, quite often give us 24KHz, while the input data is encoded at 48KHz (and likely had 44.1 before)
// >>> actually we SHOULD use it (even by the spec) -> and use the netx HIGHER if not supported directly. (still triggering the Defold issue -- but working better with slower mixer rates should we ever encounter them!)
                            const uint32_t maxRefRate = 48000;

                            // Gather original sample rate (prior to any resampling during encode) if available. Otherwise assume: 48KHz per Opus recommendation.
                            uint32_t orgRate = Read32Bit(&opusHead[12]);
                            uint32_t targetRate = (orgRate > 0) ? dmMath::Min(orgRate, maxRefRate) : maxRefRate;

                            // Try finding a supported rate just above that rate
                            streamInfo->m_Info.m_Rate = maxRefRate;
                            for(uint32_t i=0; rates[i]; ++i) {
                                if (rates[i] >= targetRate) {
                                    streamInfo->m_Info.m_Rate = rates[i];
                                    break;
                                }
                            }

                            streamInfo->m_Info.m_Size = 0;  // unknown

                            streamInfo->m_PreSkip = Read16Bit(&opusHead[10]);

                            uint16_t gain = Read16Bit(&opusHead[16]);
                            streamInfo->m_SampleScale = (gain == 0) ? 1.0f : powf(10.0f, gain / (20.0f * 256.0f));

                            uint8_t channelMapFamily = opusHead[18];

                            if (channelMapFamily == 0) {
                                // For now we only support 1 or 2 channels - which corresponds to mapping family zero

                                streamInfo->m_LastOutput.SetCapacity(5720 * streamInfo->m_Info.m_Channels * sizeof(int16_t));

                                CleanupBuffer(streamInfo->m_DataBuffer, pageSize);
                                bDone = true;
                                break;
                            }
                        }

                        delete streamInfo;
                        return RESULT_INVALID_FORMAT;
                    }
                }

                CleanupBuffer(streamInfo->m_DataBuffer, pageSize);
            }
        }
        while(!bDone && !(streamInfo->m_bEOS && streamInfo->m_DataBuffer.Empty()));

        if (!bDone) {
            delete streamInfo;
            return RESULT_INVALID_FORMAT;
        }

        int error;
        streamInfo->m_Decoder = opus_decoder_create(streamInfo->m_Info.m_Rate, streamInfo->m_Info.m_Channels, &error);

        if (streamInfo->m_Decoder == NULL) {
            delete streamInfo;
            return RESULT_INVALID_FORMAT;
        }

        *stream = streamInfo;
        return RESULT_OK;
    }

    static Result OpusDecode(HDecodeStream stream, char* buffer[], uint32_t buffer_size, uint32_t* decoded)
    {
//SAMPLE SCALE! (DO WE REALLY ENCOUNTER != 1.0?)
//SIMD / PLATFORM SUPPORT (& profile)
        DecodeStreamInfo *streamInfo = (DecodeStreamInfo *) stream;

        DM_PROFILE(__FUNCTION__);

        uint32_t needed_frames = buffer_size / (streamInfo->m_Info.m_Channels * sizeof(int16_t));
        uint32_t done_frames = 0;

        uint32_t stride = streamInfo->m_Info.m_Channels * sizeof(int16_t);

        while (done_frames < needed_frames) {
            // Do we still have output from the last decode?
            if (streamInfo->m_LastOutput.Empty()) {

                // Read next packet data (if incomplete this will fail and needs to be called again later)
                uint32_t out_bytes = sizeof(streamInfo->m_PacketBuffer);
                if (ReadNextPacket(streamInfo, streamInfo->m_SoundData, streamInfo->m_PacketBuffer, out_bytes)) {

                    // Trigger a decode for the complete packet we now gathered...
                    int frame_size = opus_decode(streamInfo->m_Decoder, streamInfo->m_PacketBuffer, out_bytes, (opus_int16*)streamInfo->m_LastOutput.Begin(), streamInfo->m_LastOutput.Capacity() / stride, 0);
                    if (frame_size > 0) {
                        uint32_t newDecodePos = streamInfo->m_DecodeSamplePos + frame_size;
                        if (streamInfo->m_DecodeSamplePos < streamInfo->m_PreSkip) {
                            uint32_t skip = dmMath::Min(streamInfo->m_PreSkip - streamInfo->m_DecodeSamplePos, (uint32_t)frame_size);
                            streamInfo->m_LastOutputOffset = skip;
                        }
                        else {
                            streamInfo->m_LastOutputOffset = 0;
                        }
                        streamInfo->m_LastOutput.SetSize(frame_size * stride);
                        streamInfo->m_DecodeSamplePos = newDecodePos;
                    } else {
                        return RESULT_DECODE_ERROR;
                    }
                }
                else {
                    break;
                }
            }

            // Produce sample output from decoder results
            if (!streamInfo->m_LastOutput.Empty()) {
                uint32_t out_frames = dmMath::Min((streamInfo->m_LastOutput.Size() / stride) - streamInfo->m_LastOutputOffset, needed_frames - done_frames);

                // This might be called with a NULL buffer to avoid delivering data, so we need to check...
                if (buffer) {
                    memcpy((short*)buffer[0] + done_frames * streamInfo->m_Info.m_Channels, streamInfo->m_LastOutput.Begin() + stride * streamInfo->m_LastOutputOffset, out_frames * stride);
                }

                done_frames += out_frames;
                streamInfo->m_SamplePos += out_frames;

                streamInfo->m_LastOutputOffset += out_frames;
                if (streamInfo->m_LastOutputOffset * stride >= streamInfo->m_LastOutput.Size()) {
                    streamInfo->m_LastOutput.SetSize(0);
                }
            }
        }

        *decoded = done_frames * streamInfo->m_Info.m_Channels * sizeof(int16_t);

        return (streamInfo->m_bEOS && done_frames == 0) ? RESULT_END_OF_STREAM : RESULT_OK;
    }


    static Result OpusResetStream(HDecodeStream stream)
    {
        // Reset all to re-stream the data (the decoder will not be reiinitialized & header pages are not re-analyzed)
        DecodeStreamInfo *streamInfo = (DecodeStreamInfo *) stream;
        streamInfo->m_StreamOffset = 0;
        streamInfo->m_LacingTableIndex = 256;
        streamInfo->m_PacketOffset = 0;
        streamInfo->m_Decoder = NULL;
        streamInfo->m_PacketOffset = 0;
        streamInfo->m_bEOS = false;
        streamInfo->m_DecodeSamplePos = 0;
        streamInfo->m_SamplePos = 0;
        streamInfo->m_DataBuffer.SetSize(0);
        streamInfo->m_LastOutput.SetSize(0);
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
        DecodeStreamInfo *streamInfo = (DecodeStreamInfo*) stream;
        opus_decoder_destroy(streamInfo->m_Decoder);
        delete streamInfo;
    }

    static void OpusGetInfo(HDecodeStream stream, struct Info* out)
    {
        *out = ((DecodeStreamInfo *)stream)->m_Info;
    }

    static int64_t OpusGetInternalPos(HDecodeStream stream)
    {
        DecodeStreamInfo *streamInfo = (DecodeStreamInfo *) stream;
        return streamInfo->m_SamplePos;
    }

    DM_DECLARE_SOUND_DECODER(AudioDecoderOpus, "OpusDecoder", FORMAT_OPUS,
                             5, // baseline score (1-10)
                             OpusOpenStream, OpusCloseStream, OpusDecode,
                             OpusResetStream, OpusSkipInStream, OpusGetInfo,
                             OpusGetInternalPos);
}
