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

#ifndef DM_SOUND_DECODER_H
#define DM_SOUND_DECODER_H

#include "sound_codec.h"
#include "sound.h"

namespace dmSoundCodec
{
    typedef void* HDecodeStream;

    enum DecoderFlags
    {
        DECODER_FLOATING_POINT = 1,
        DECODER_FIXED_POINT    = 2
    };

    struct DecoderInfo
    {
        /**
         * Implementation name
         */
        const char* m_Name;

        /**
         * Format the decoder handles
         */
        Format m_Format;

        /**
         * Performance score. Recommended range 0-10
         */
        int m_Score;

        /**
         * Open a stream for decoding
         */
        Result (*m_OpenStream)(dmSound::HSoundData sound_data, HDecodeStream* out);

        /**
         * Close and free decoding resources
         */
        void (*m_CloseStream)(HDecodeStream);

        /**
         * Fetch a chunk of PCM data from the decoder. The buffer(s) will be filled as long as there is
         * enough data left in the compressed stream.
         */
        Result (*m_DecodeStream)(HDecodeStream decoder, char* buffer[], uint32_t buffer_size, uint32_t* decoded);

        /**
         * Seek to the beginning.
         */
        Result (*m_ResetStream)(HDecodeStream);

        /**
         * Skip in stream
         */
        Result (*m_SkipInStream)(HDecodeStream, uint32_t bytes, uint32_t* skipped);

        /**
         * Get samplerate and number of channels.
         */
        void (*m_GetStreamInfo)(HDecodeStream, struct Info* out);

        /**
         * Get internal cursor (for unit tests only)
         */
        int64_t (*m_GetInternalStreamPosition)(HDecodeStream);

        DecoderInfo *m_Next;
    };

    /**
     * Store a decoder in the internal registry. Will update m_Next in the supplied instance.
     */
    void RegisterDecoder(DecoderInfo* info);

    /**
     * Finds the best match for a stream among all registered decoders.
     */
    const DecoderInfo* FindBestDecoder(Format format);

    /**
     * Get by name of implementation
     */
    const DecoderInfo* FindDecoderByName(const char *name);

    #define DM_REGISTER_SOUND_DECODER(name, desc) extern "C" void name () { \
        dmSoundCodec::RegisterDecoder(&desc); \
    }

    #ifndef DM_SOUND_PASTE
    #define DM_SOUND_PASTE(x, y) x ## y
    #define DM_SOUND_PASTE2(x, y) DM_SOUND_PASTE(x, y)
    #endif

    /**
     * Declare a new stream decoder
     */
    #define DM_DECLARE_SOUND_DECODER(symbol, name, format, score, open, close, decode, reset, skip, getinfo, get_internal_pos) \
            dmSoundCodec::DecoderInfo DM_SOUND_PASTE2(symbol, __LINE__) = { \
                    name, \
                    format, \
                    score, \
                    open, \
                    close, \
                    decode, \
                    reset, \
                    skip, \
                    getinfo, \
                    get_internal_pos, \
            };\
        DM_REGISTER_SOUND_DECODER(symbol, DM_SOUND_PASTE2(symbol, __LINE__))
}

#endif
