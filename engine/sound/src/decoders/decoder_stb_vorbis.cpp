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
#include <dlib/array.h>
#include <dlib/index_pool.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/profile.h>

#include "stb_vorbis/stb_vorbis.h"
#include "sound_codec.h"
#include "sound_decoder.h"

#define STREAM_BLOCK_SIZE   (16 << 10)          // notes: - we assume this to be large enough to ALWAYS contain all data needed for stream initialization (and always be present on stream startup)
                                                //        - OggVorbis usually has blocks up to 8K-sh at maximum, but might go as high as 64K - this would FAIL currently (but practically we say WAY smaller sizes; hence the current value)

namespace dmSoundCodec
{
    namespace
    {
        struct DecodeStreamInfo {
            Info m_Info;
            bool m_NormalizedOutput;
            stb_vorbis* m_StbVorbis;
            dmSound::HSoundData m_SoundData;
            uint32_t m_StreamOffset;
            dmArray<uint8_t> m_DataBuffer;
            int m_LastOutputFrames;
            uint32_t m_LastOutputOffset;
            float** m_LastOutput;
        };
    }

    static void CleanupBuffer(dmArray<uint8_t>& buffer, uint32_t consumed)
    {
        // Move sill un-consumed buffer contents to the fronr so we offer them in one contigous chunk to Vorbis at all times
        uint32_t n = buffer.Size();
        uint8_t* data = buffer.Begin();
        for(uint32_t i=consumed; i<n; ++i)
            data[i - consumed] = data[i];
        buffer.SetSize(n - consumed);
    }

    static Result StbVorbisOpenStream(dmSound::HSoundData sound_data, HDecodeStream* stream)
    {
        DecodeStreamInfo *streamInfo = new DecodeStreamInfo;
        streamInfo->m_DataBuffer.SetCapacity(STREAM_BLOCK_SIZE);

        uint32_t read_size;
        dmSound::Result res = dmSound::SoundDataRead(sound_data, 0, STREAM_BLOCK_SIZE, streamInfo->m_DataBuffer.Begin(), &read_size);

        if (res != dmSound::RESULT_NO_DATA) {
            streamInfo->m_DataBuffer.SetSize(read_size);
            streamInfo->m_StreamOffset = read_size;

            int error, consumed;
            stb_vorbis *vorbis = stb_vorbis_open_pushdata(streamInfo->m_DataBuffer.Begin(), (int)streamInfo->m_DataBuffer.Size(), &consumed, &error, NULL);
            if (vorbis) {
                CleanupBuffer(streamInfo->m_DataBuffer, consumed);

                streamInfo->m_SoundData = sound_data;
                streamInfo->m_LastOutput = NULL;

                dmSound::DecoderOutputSettings settings;
                dmSound::GetDecoderOutputSettings(&settings);

                streamInfo->m_NormalizedOutput = settings.m_UseNormalizedFloatRange;
                
                stb_vorbis_info info = stb_vorbis_get_info(vorbis);

                streamInfo->m_Info.m_Rate = info.sample_rate;
                streamInfo->m_Info.m_Size = 0;
                streamInfo->m_Info.m_Channels = info.channels;
                streamInfo->m_Info.m_BitsPerSample = 32;
                streamInfo->m_Info.m_IsInterleaved = settings.m_UseInterleaved;
                streamInfo->m_StbVorbis = vorbis;

                *stream = streamInfo;
                return RESULT_OK;
            }

            if (error != VORBIS_need_more_data) {
                dmLogWarning("Vorbis data seems to be invalid!");
            } else {
                // Now would be the time to present even more data to the decoder to startup decoding - but we fail as we assume our initial block always being large enough
                dmLogWarning("Vorbis needs more data to be initialized than expected!");
            }
        }

        delete streamInfo;
        return RESULT_INVALID_FORMAT;
    }

    static void ConvertDecoderOutputNormalizedInterleaved(uint32_t channels, float *out, float **data, uint32_t offset, uint32_t frames)
    {
        assert(channels == 1 || channels == 2);
        uint32_t s = channels - 1;

        for(uint32_t c=0; c<channels; ++c) {
            for(uint32_t f=0; f<frames; ++f) {
                out[(f << s) + c] = data[c][f + offset];
            }
        }
    }

    static void ConvertDecoderOutputNonInterleaved(uint32_t channels, char *out[], uint32_t out_offset, float **data, uint32_t in_offset, uint32_t frames)
    {
        // Copy out data from Vorbis internal buffer & scale it up from normalized representation to fit the mixer's expectations
        for(uint32_t c=0; c<channels; ++c)
        {
            const float* src = data[c] + in_offset;
            float* dest = (float*)out[c] + out_offset;
            for(uint32_t i=0; i<frames; ++i)
            {
                *(dest++) = *(src++) * 32767.0f;
            }
        }
    }

    static Result StbVorbisDecode(HDecodeStream stream, char* buffer[], uint32_t buffer_size, uint32_t* decoded)
    {
        // note: EOS detection is solely based on data consumption and hence not sample precise (the last decoded block may contain silence not part of the original material)
        
        DecodeStreamInfo *streamInfo = (DecodeStreamInfo *) stream;

        DM_PROFILE(__FUNCTION__);

        uint32_t stride = streamInfo->m_Info.m_IsInterleaved ? (streamInfo->m_Info.m_Channels * sizeof(float)) : sizeof(float);

        int needed_frames = buffer_size / stride;
        int done_frames = 0;

        bool bEOS = false;

        while (done_frames < needed_frames) {

            // Do we still have output from the last decode?
            if (streamInfo->m_LastOutput == NULL) {

                bool bInputDry = false;

                // No, keep input buffers as full as possible...
                if (!streamInfo->m_DataBuffer.Full()) {
                    uint32_t read_bytes;
                    dmSound::Result res = dmSound::SoundDataRead(streamInfo->m_SoundData, streamInfo->m_StreamOffset, streamInfo->m_DataBuffer.Remaining(), streamInfo->m_DataBuffer.End(), &read_bytes);
                    if (res == dmSound::RESULT_OK || res == dmSound::RESULT_PARTIAL_DATA) {
                        streamInfo->m_StreamOffset += read_bytes;
                        streamInfo->m_DataBuffer.SetSize(streamInfo->m_DataBuffer.Size() + read_bytes);
                    } else if (res == dmSound::RESULT_NO_DATA) {
                        bInputDry = true;
                    } else if (res == dmSound::RESULT_END_OF_STREAM) {
                        bInputDry = true;
                        bEOS = true;
                    } else {
                        return RESULT_DECODE_ERROR;
                    }
                }

                // Decode if we can...
                int channels;
                int consumed = stb_vorbis_decode_frame_pushdata(streamInfo->m_StbVorbis, streamInfo->m_DataBuffer.Begin(), streamInfo->m_DataBuffer.Size(), &channels, &streamInfo->m_LastOutput, &streamInfo->m_LastOutputFrames);
                if (consumed < 0) {
                    dmLogWarning("Vorbis decoder returned error code %d.", consumed);
                    return RESULT_DECODE_ERROR;
                }

                if (consumed == 0) {
                    // Sanity check: we should never have less data than needed if our input buffer is full!
                    if (streamInfo->m_DataBuffer.Full()) {
                        dmLogWarning("Vorbis needs more data to produce new samples than expected!");
                        return RESULT_DECODE_ERROR;
                    }
                    if (bInputDry) {
                        // We can't decode with the input we got and we can't have more data right now -> exit decode loop
                        break;
                    }
                }

                // Ready input buffer for more data
                if (consumed > 0) {
                    CleanupBuffer(streamInfo->m_DataBuffer, consumed);
                    // Reset output offset (in case we got new output data, too)
                    streamInfo->m_LastOutputOffset = 0;
                }
            }

            // Produce sample output from decoder results
            if (streamInfo->m_LastOutput) {
                uint32_t out_frames = dmMath::Min(streamInfo->m_LastOutputFrames, needed_frames - done_frames);
                // This might be called with a NULL buffer to avoid delivering data, so we need to check...
                if (buffer) {
                    if (streamInfo->m_NormalizedOutput && streamInfo->m_Info.m_IsInterleaved)
                        ConvertDecoderOutputNormalizedInterleaved(streamInfo->m_Info.m_Channels, (float*)*buffer + done_frames * streamInfo->m_Info.m_Channels, streamInfo->m_LastOutput, streamInfo->m_LastOutputOffset, out_frames);
                    else
                        ConvertDecoderOutputNonInterleaved(streamInfo->m_Info.m_Channels, buffer, done_frames, streamInfo->m_LastOutput, streamInfo->m_LastOutputOffset, out_frames);
                }
                
                done_frames += out_frames;

                streamInfo->m_LastOutputOffset += out_frames;
                streamInfo->m_LastOutputFrames -= out_frames;
                if (streamInfo->m_LastOutputFrames == 0) {
                    streamInfo->m_LastOutput = NULL;
                }
            }

        }

        *decoded = done_frames * stride;

        return (done_frames == 0 && bEOS) ? RESULT_END_OF_STREAM : RESULT_OK;
    }

    static Result StbVorbisResetStream(HDecodeStream stream)
    {
        DecodeStreamInfo *streamInfo = (DecodeStreamInfo *) stream;

        stb_vorbis_flush_pushdata(streamInfo->m_StbVorbis);

        streamInfo->m_LastOutput = NULL;
        streamInfo->m_StreamOffset = 0;
        streamInfo->m_DataBuffer.SetSize(0);
        return RESULT_OK;
    }

    static Result StbVorbisSkipInStream(HDecodeStream stream, uint32_t num_bytes, uint32_t* skipped)
    {
        // Decode with buffer = null corresponding number of bytes.
        // We've modified stb_vorbis to accept a null pointer, which allows us skipping a lot of decoding work.
        // NOTE: Although the stb-vorbis api has functions for seeking forward in the stream
        // there seem to be no clear cut way to get the exact position afterwards.
        // So, we revert to the "decode and discard" approach to keep the internal state of the stream intact.
        //
        // NOTE POST STREAMING-REFACTOR:
        //
        // - modifications in Vorbis are no longer needed
        // - NULL buffer really (old & new version) just skips the final float to short conversion - decode of the actual data still happens (unless decoder is in seek mode)
        // - we still need to decode to be able to monitor data consumption & keep decode state to reengage output later on / detect EOS
        //
        return StbVorbisDecode(stream, NULL, num_bytes, skipped);
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
