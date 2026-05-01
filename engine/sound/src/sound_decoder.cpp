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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

#include <dlib/log.h>

#include "sound_codec.h"
#include "sound_decoder.h"

namespace dmSoundCodec
{
    namespace
    {
        DecoderInfo *g_FirstDecoder = 0;
    }

    void RegisterDecoder(DecoderInfo* info)
    {
        info->m_Next = g_FirstDecoder;
        g_FirstDecoder = info;
    }

    const DecoderInfo* FindDecoderByName(const char *name)
    {
        const DecoderInfo *decoder = g_FirstDecoder;
        while (decoder)
        {
            if (!strcmp(name, decoder->m_Name))
                return decoder;
            decoder = decoder->m_Next;
        }
        return 0;
    }

    const DecoderInfo* FindBestDecoder(Format format)
    {
        // All decoders contain a score, now pick the decoder with highest score
        // with matching format.
        int highest_score;
        const DecoderInfo *best = 0;
        const DecoderInfo *decoder = g_FirstDecoder;

        while (decoder)
        {
            if (decoder->m_Format != format)
            {
                decoder = decoder->m_Next;
                continue;
            }

            if (!best || decoder->m_Score > highest_score)
            {
                highest_score = decoder->m_Score;
                best = decoder;
            }

            decoder = decoder->m_Next;
        }
        return best;
    }

    const char* ResultToString(Result result)
    {
        switch(result)
        {
#define RESULT_CASE(_NAME) \
    case _NAME: return #_NAME

            RESULT_CASE(RESULT_OK);
            RESULT_CASE(RESULT_OUT_OF_RESOURCES);
            RESULT_CASE(RESULT_INVALID_FORMAT);
            RESULT_CASE(RESULT_DECODE_ERROR);
            RESULT_CASE(RESULT_UNSUPPORTED);
            RESULT_CASE(RESULT_END_OF_STREAM);
            RESULT_CASE(RESULT_UNKNOWN_ERROR);
            default: return "Unknown";
        }
#undef RESULT_CASE
    }

    const char* FormatToString(Format format)
    {
        switch(format)
        {
#define FORMAT_CASE(_NAME) \
    case _NAME: return #_NAME

            FORMAT_CASE(FORMAT_WAV);
            FORMAT_CASE(FORMAT_VORBIS);
            FORMAT_CASE(FORMAT_OPUS);
            default: return "Unknown";
        }
#undef FORMAT_CASE
    }
}
