// Copyright 2020 The Defold Foundation
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

        assert(best != 0);
        return best;
    }
}
