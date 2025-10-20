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
#include <dlib/array.h>

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
            virtual ~DecodeStreamInfo() {}

            Info m_Info;
            bool m_IsADPCM : 1;
            uint8_t : 7;
            uint32_t m_Cursor;
            const void* m_Buffer;
            uint32_t m_BufferOffset;
            dmSound::HSoundData m_SoundData;
        };

        struct DecodeStreamInfoADPCM : public DecodeStreamInfo {
            ~DecodeStreamInfoADPCM() override = default;

            dmArray<int16_t> m_OutBuffer;
            dmArray<int8_t> m_InBuffer;
            uint32_t m_InBufferOffset;
            int32_t m_Pred[2];
            int32_t m_StepIndex[2];
            int32_t m_Step[2];
            uint16_t m_BlockAlign;
            uint16_t m_BlockFrames;
            uint16_t m_OutFramesOffset;
        };
    }

    static Result WavOpenStream(dmSound::HSoundData sound_data, HDecodeStream* stream)
    {
        // note: the code below assumes enough data to be present to evaluate a WAV files structure
        // (this also assumes all format / header chunks to be situated BEFORE the data chunk! - at least if the data is streamed)

        RiffHeader header;

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

            DecodeStreamInfo *streamOut = nullptr;

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
                        delete streamOut;
                        dmLogError("WAV sound data seems corrupt or truncated: %d", res);
                        return RESULT_INVALID_FORMAT;
                    }
                    fmt.Swap();
                    fmt_found = true;

                    if( (fmt.m_AudioFormat != 0x01 && fmt.m_AudioFormat != 0x11) || (fmt.m_AudioFormat == 0x11 && fmt.m_BitsPerSample != 4) )
                    {
                        delete streamOut;
                        dmLogError("Only wav-files with 8 or 16 bit PCM or IMA ADPCM format (format=0x01 or 0x11) supported, got format=%02x and bitdepth=%d", fmt.m_AudioFormat, fmt.m_BitsPerSample);
                        return RESULT_INVALID_FORMAT;
                    }

                    if (fmt.m_AudioFormat == 0x11) {
                        streamOut = new DecodeStreamInfoADPCM();
                        streamOut->m_IsADPCM = true;
                    }
                    else {
                        streamOut = new DecodeStreamInfo();
                        streamOut->m_IsADPCM = false;
                    }

                    streamOut->m_SoundData = sound_data;

                    if (streamOut->m_IsADPCM) {
                        DecodeStreamInfoADPCM* streamOutADPCM = static_cast<DecodeStreamInfoADPCM*>(streamOut);
                        streamOutADPCM->m_BlockAlign = fmt.m_BlockAlign;
                        streamOutADPCM->m_BlockFrames = (fmt.m_NumChannels == 1) ? ((fmt.m_BlockAlign - 4) * 2) : (fmt.m_BlockAlign - 8);
                        streamOutADPCM->m_OutFramesOffset = streamOutADPCM->m_BlockFrames;
                        streamOutADPCM->m_OutBuffer.SetCapacity(fmt.m_NumChannels * 8);
                        streamOutADPCM->m_OutBuffer.SetSize(0);
                        streamOutADPCM->m_InBuffer.SetCapacity(streamOutADPCM->m_BlockAlign);
                        streamOutADPCM->m_InBufferOffset = 0;
                    }

                    streamOut->m_Info.m_Rate = fmt.m_SampleRate;
                    streamOut->m_Info.m_Channels = fmt.m_NumChannels;
                    streamOut->m_Info.m_BitsPerSample = streamOut->m_IsADPCM ? 16 : fmt.m_BitsPerSample;    // IMA ADPCM has 4-bits per sample in data, but we output 16-bit
                    streamOut->m_Info.m_IsInterleaved = true;

                } else if (header.m_ChunkID == FOUR_CC('d', 'a', 't', 'a')) {
                    if (streamOut != nullptr) {
                        data_found = true;

                        streamOut->m_BufferOffset = current_offset + sizeof(DataChunk);
                        streamOut->m_Info.m_Size = header.m_ChunkSize;
                    }
                }
                // note: we assume the Riff header, format chunk and data chunk to roughly appear in this order and close proximity. Theoretically we might see WAV files that do NOT follow this pattern!
//TODO: ^^^ WE SHOULD CATCH THAT! FILES LIKE THAT WOULD NOT BE SUITABLE FOR STREAMING IN THIS MANNER! --> WE WOULD NEED READ DATA CONVERSION!!!
                current_offset += sizeof(CommonHeader) + header.m_ChunkSize;

            } while (!(fmt_found && data_found));

            if (fmt_found && data_found) {
                streamOut->m_Cursor = 0;
                *stream = streamOut;
                return RESULT_OK;
            } else {
                delete streamOut;
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
        if (streamInfo->m_IsADPCM) {
            DecodeStreamInfoADPCM* streamInfoADPCM = static_cast<DecodeStreamInfoADPCM*>(streamInfo);
            streamInfoADPCM->m_OutFramesOffset = 0;
            streamInfoADPCM->m_InBuffer.SetSize(0);
            streamInfoADPCM->m_InBufferOffset = 0;
            streamInfoADPCM->m_OutBuffer.SetSize(0);
        }
        return RESULT_OK;
    }

    static const int8_t ima_index_table[16] = {
        -1, -1, -1, -1, 2, 4, 6, 8,
        -1, -1, -1, -1, 2, 4, 6, 8
    };

    static const int16_t ima_step_table[89] = {
        7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
        19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
        50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
        130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
        337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
        876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
        2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
        5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
        15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
    };

    static inline void DecodeNibble(uint32_t n, int32_t& pred, int32_t& step_index, int32_t& step)
    {
            step_index = dmMath::Clamp(step_index + ima_index_table[n & 15], 0, 88);
            int32_t diff = step >> 3;
            diff += (n & 1) ? (step >> 2) : 0;
            diff += (n & 2) ? (step >> 1) : 0;
            diff += (n & 4) ?  step       : 0;
            pred = (n & 8) ? dmMath::Max(pred - diff, -32768) : dmMath::Min(pred + diff, 32767);
            step = ima_step_table[step_index];
    }

    static Result WavDecodeStream(HDecodeStream stream, char* buffer[], uint32_t buffer_size, uint32_t* decoded)
    {
        DM_PROFILE(__FUNCTION__);

        if (!((DecodeStreamInfo *)stream)->m_IsADPCM) {
            //
            // PCM8/16 data delivery
            //
            // (assumes little endian system!)
            //
            DecodeStreamInfo *streamInfo = (DecodeStreamInfo *) stream;

            assert(streamInfo->m_Cursor <= streamInfo->m_Info.m_Size);

            uint32_t n = dmMath::Min(buffer_size, streamInfo->m_Info.m_Size - streamInfo->m_Cursor);

            // WAV files can contain data beyond the end of the data chunk. Hence the EOS of the reader is not the only thing tgat can trigger an EOS logically!
            if (n == 0) {
                *decoded = 0;
                return RESULT_END_OF_STREAM;
            }

            uint32_t read_size;
            dmSound::Result res = dmSound::SoundDataRead(streamInfo->m_SoundData, streamInfo->m_BufferOffset + streamInfo->m_Cursor, n, buffer[0], &read_size);
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

        //
        // ADPCM read / decode
        //
        DecodeStreamInfoADPCM* streamInfo = (DecodeStreamInfoADPCM*) stream;

        assert(streamInfo->m_Cursor <= streamInfo->m_Info.m_Size);

        dmSound::Result res = dmSound::RESULT_OK;
        char *outbuf = buffer[0];
        uint32_t stride = sizeof(int16_t) * streamInfo->m_Info.m_Channels;
        int32_t needed_frames = buffer_size / stride;
        uint32_t num_channels = streamInfo->m_Info.m_Channels;
        uint32_t min_frames = (num_channels == 1) ? 2 : 8;

        // Do we have some decoded data left over from last time?
        if (streamInfo->m_OutBuffer.Size() != 0) {
            // Yes. See what we can use...
            uint32_t frames_in_buffer = streamInfo->m_OutBuffer.Size() / num_channels;
            uint32_t num = dmMath::Min(frames_in_buffer - streamInfo->m_OutFramesOffset, (uint32_t)needed_frames);

            memcpy(outbuf, streamInfo->m_OutBuffer.Begin() + streamInfo->m_OutFramesOffset * num_channels, num * stride);

            needed_frames -= num;
            outbuf += stride * num;
            streamInfo->m_OutFramesOffset += num;
            if (streamInfo->m_OutFramesOffset >= frames_in_buffer) {
                streamInfo->m_OutBuffer.SetSize(0);
            }
        }

        // Decode from scratch...
        while(needed_frames > 0) {

            // Try to get a full block into the input buffer... (this may only fill it partly)
            if (streamInfo->m_InBuffer.Size() < streamInfo->m_BlockAlign) {

                // Check if we are at the end of the chunk in the WAV file (this does not need to be the end of the file)
                if (streamInfo->m_Cursor >= streamInfo->m_Info.m_Size) {
                    // EOS. We have no new data...
                    res = dmSound::RESULT_END_OF_STREAM;
                    break;
                }

                uint32_t read_size;
                res = dmSound::SoundDataRead(streamInfo->m_SoundData, streamInfo->m_BufferOffset + streamInfo->m_Cursor, streamInfo->m_BlockAlign - streamInfo->m_InBuffer.Size(), streamInfo->m_InBuffer.End(), &read_size);
                if (res != dmSound::RESULT_OK && res != dmSound::RESULT_PARTIAL_DATA)
                {
                    // Error case / EOS
                    break;
                }

                streamInfo->m_Cursor += read_size;
                streamInfo->m_InBuffer.SetSize(streamInfo->m_InBuffer.Size() + read_size);
            }

            // Do we need to decode a block header?
            if (streamInfo->m_InBufferOffset == 0) {
                // Yes...

                // Enough data read?
                if (streamInfo->m_InBuffer.Size() < num_channels * 4) {
                    break;  // No...
                }

                int8_t* in = streamInfo->m_InBuffer.Begin();
                for(uint32_t c=0; c<num_channels; ++c) {
                    streamInfo->m_Pred[c] = *(int16_t*)in;
                    streamInfo->m_StepIndex[c] = in[2];
                    streamInfo->m_Step[c] = ima_step_table[streamInfo->m_StepIndex[c]];
                    in += 4;
                }
                streamInfo->m_InBufferOffset += num_channels * 4;
            }

            // Given the blocking in the format, how many do we need to decode to actually get the number we need?
            uint32_t needed_frames_aligned;
            if (needed_frames < min_frames) {
                needed_frames_aligned = min_frames;
            } else {
                needed_frames_aligned = needed_frames & ~(min_frames - 1);
            }

            // Limit decoding further with the number of input bytes we have
            uint32_t num;
            if (num_channels == 1) {
                // note: we decode only on byte mulotiples (2 frames)
                num = dmMath::Min(needed_frames_aligned, (streamInfo->m_InBuffer.Size() - streamInfo->m_InBufferOffset) * 2);
            }
            else {
                assert(num_channels == 2);
                // note: 8 byte sub-blocks are needed so we can decode 8 frames (data is interleaved in 4-byte per channel chunks)
                num = dmMath::Min(needed_frames_aligned, (streamInfo->m_InBuffer.Size() & ~7) - streamInfo->m_InBufferOffset);
            }
            if (num == 0) {
                break;
            }

            // Do we produce more samples than we have been asked for?
            int16_t* out;
            bool uses_buffer;
            if (needed_frames < needed_frames_aligned) {
                // Yes, decode into temp buffer first, so we can save some frames to be delivered later...
                streamInfo->m_OutBuffer.SetSize(needed_frames_aligned * num_channels);
                streamInfo->m_OutFramesOffset = 0;
                out = streamInfo->m_OutBuffer.Begin();
                uses_buffer = true;
            }
            else {
                // No, direct decode to output buffer is best choice...
                out = (int16_t*)outbuf;
                uses_buffer = false;
            }

            // Decode data as needed / possible with data available...
            int8_t* in = streamInfo->m_InBuffer.Begin() + streamInfo->m_InBufferOffset;
            if (num_channels == 1) {
                int32_t pred = streamInfo->m_Pred[0];
                int32_t step_index = streamInfo->m_StepIndex[0];
                int32_t step = streamInfo->m_Step[0];

                uint32_t bytes = num >> 1;
                for(uint32_t i=0; i<bytes; ++i) {
                    uint32_t b = *(in++);
                    DecodeNibble(b, pred, step_index, step);
                    *(out++) = (int16_t)pred;
                    DecodeNibble(b >> 4, pred, step_index, step);
                    *(out++) = (int16_t)pred;
                    }

                streamInfo->m_Pred[0] = pred;
                streamInfo->m_StepIndex[0] = step_index;
                streamInfo->m_Step[0] = step;
            }
            else {
                int32_t pred0 = streamInfo->m_Pred[0];
                int32_t pred1 = streamInfo->m_Pred[1];
                int32_t step_index0 = streamInfo->m_StepIndex[0];
                int32_t step_index1 = streamInfo->m_StepIndex[1];
                int32_t step0 = streamInfo->m_Step[0];
                int32_t step1 = streamInfo->m_Step[1];

                int32_t bytes = num;
                for(int32_t i=bytes; i>7; i-=8) {

                    uint32_t blk0 = *(uint32_t*)&in[0];
                    uint32_t blk1 = *(uint32_t*)&in[4];
                    in += 8;

                    for(uint32_t b=0; b<8; ++b) {
                        DecodeNibble(blk0, pred0, step_index0, step0);
                        *(out++) = (int16_t)pred0;
                        DecodeNibble(blk1, pred1, step_index1, step1);
                        *(out++) = (int16_t)pred1;
                        blk0 >>= 4;
                        blk1 >>= 4;
                    }
                }

                streamInfo->m_Pred[0] = pred0;
                streamInfo->m_Pred[1] = pred1;
                streamInfo->m_StepIndex[0] = step_index0;
                streamInfo->m_StepIndex[1] = step_index1;
                streamInfo->m_Step[0] = step0;
                streamInfo->m_Step[1] = step1;

            }

            // Update input buffer offset & needed frames
            streamInfo->m_InBufferOffset = (uint32_t)(in - streamInfo->m_InBuffer.Begin());
            needed_frames -= num;

            // Input buffer exhausted?
            if (streamInfo->m_InBufferOffset >= streamInfo->m_BlockAlign) {
                // Mark as empty...
                streamInfo->m_InBufferOffset = 0;
                streamInfo->m_InBuffer.SetSize(0);
            }

            // Update output buffer pointer if we did not use a temp buffer (we will do more updates)
            if (!uses_buffer) {
                outbuf = (char*)out;
            }
        }

        // If we overshot the need, we know we need to still copy some data into the output buffer...
        if (needed_frames < 0) {
            needed_frames += min_frames;
            memcpy(outbuf, streamInfo->m_OutBuffer.Begin(), needed_frames * stride);
            streamInfo->m_OutFramesOffset = needed_frames;
            outbuf += needed_frames * stride;
        }

        *decoded = (uint32_t)((uintptr_t)outbuf - (uintptr_t)buffer[0]);

        if (res == dmSound::RESULT_END_OF_STREAM) {
            return (*decoded == 0) ? RESULT_END_OF_STREAM : RESULT_OK;
        }

        return (res == dmSound::RESULT_OK || res == dmSound::RESULT_PARTIAL_DATA) ? RESULT_OK : RESULT_DECODE_ERROR;
    }

    static Result WavSkipInStream(HDecodeStream stream, uint32_t bytes, uint32_t* skipped)
    {
        if (!((DecodeStreamInfo *)stream)->m_IsADPCM) {
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

        Result res = RESULT_OK;
        *skipped = 0;
        while(bytes != 0)
        {
            char buffer[512];
            uint32_t n = dmMath::Min((uint32_t)sizeof(buffer), bytes);
            uint32_t decoded = 0;
            char* bufs[2] = {buffer, nullptr};
            res = WavDecodeStream(stream, bufs, n, &decoded);
            if (res != RESULT_OK) {
                break;
            }
            bytes -= decoded;
            *skipped += decoded;
        }

        if ((*skipped != 0) && (res == RESULT_END_OF_STREAM)) {
            return RESULT_OK;
        }
        return res;
    }

    static void WavGetInfo(HDecodeStream stream, struct Info* out)
    {
        *out = ((DecodeStreamInfo *)stream)->m_Info;
    }

    static int64_t WavGetInternalPos(HDecodeStream stream)
    {
        if (!((DecodeStreamInfo *)stream)->m_IsADPCM) {
            DecodeStreamInfo *streamInfo = (DecodeStreamInfo *) stream;
            uint64_t stride = (streamInfo->m_Info.m_Channels * streamInfo->m_Info.m_BitsPerSample) >> 3;
            return streamInfo->m_Cursor / stride;
        }

        DecodeStreamInfoADPCM *streamInfo = (DecodeStreamInfoADPCM *) stream;
        uint64_t pos = streamInfo->m_Cursor - streamInfo->m_InBuffer.Size() + streamInfo->m_InBufferOffset;
        uint64_t block_frames = (streamInfo->m_Info.m_Channels == 1) ? ((streamInfo->m_BlockAlign - 4) * 2) : (streamInfo->m_BlockAlign - 8);
        uint64_t block = pos / streamInfo->m_BlockAlign;
        int64_t block_off = pos - block * streamInfo->m_BlockAlign;
        return block * block_frames + ((streamInfo->m_Info.m_Channels == 1) ? (dmMath::Max(block_off - 4, (int64_t)0) * 2) : (dmMath::Max(block_off - 8, (int64_t)0)));
    }

    DM_DECLARE_SOUND_DECODER(AudioDecoderWav, "WavDecoder", FORMAT_WAV,
                             0,
                             WavOpenStream, WavCloseStream, WavDecodeStream,
                             WavResetStream, WavSkipInStream, WavGetInfo,
                             WavGetInternalPos);
}
