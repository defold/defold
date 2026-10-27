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

#ifndef DM_FONT_EFFECT_H
#define DM_FONT_EFFECT_H

#include <stdint.h>
#include <assert.h>
#include <string.h>

// CPU-side record packing also used by host tools without graphics dependencies.
// The record layout is shared with builtins/fonts/font-effect.glsl.
enum FontColorEffectType
{
    FONT_COLOR_EFFECT_COLOR,
    FONT_COLOR_EFFECT_LINEAR,
    FONT_COLOR_EFFECT_RADIAL,
    FONT_COLOR_EFFECT_FOUR_CORNER,
};

enum FontColorEffectFlags
{
    FONT_COLOR_EFFECT_MIRROR = 1,
};

struct FontColorEffect
{
    uint8_t m_Colors[4][4];
    // Nondecreasing positions in [0, 1]. Unused stops repeat the last stop.
    float   m_Positions[4];
    // Linear: projection x, y, offset. Radial: pivot x, y, then the inverse
    // ellipse transform's xx, yy, xy, yx coefficients applied to fit UV offsets.
    float   m_Parameters[6];
    uint8_t m_Type : 2;
    uint8_t m_StopCount : 3; // Stores 1-4 directly.
    uint8_t m_Flags : 1;
};

static const uint32_t FONT_EFFECT_TEXTURE_WIDTH = 16;
// Initial capacity grows only when all records are pinned. Row zero stores texture metadata.
static const uint32_t FONT_EFFECT_TEXTURE_HEIGHT = 256;
static const uint32_t FONT_EFFECT_MAX_TEXTURE_HEIGHT = 16384;
static const uint32_t FONT_EFFECT_RECORD_SIZE = FONT_EFFECT_TEXTURE_WIDTH * 4;

// GPU representation of one color effect: one row of 16 RGBA8 texels (64 bytes)
// in the font_effects texture. FontPackEffect encodes the typed FontColorEffect
// fields into this fixed byte layout, independent of C++ struct padding:
//   0-3:   effect type, stop count, flags, zero
//   4-19:  four RGBA8 colors
//   20-27: four normalized stop positions as big-endian uint16 values
//   28-51: six parameters as little-endian IEEE 754 float bits
//   52-63: zero padding
// Unused color/position slots repeat the final stop. All padding is zeroed so
// caches can hash and compare records byte-for-byte, then upload them directly.
// Vertices reference the cached texture row and carry their own effect UVs and
// animation phase, allowing different glyphs and animations to share a record.
struct FontEffectRecord
{
    uint8_t m_Data[FONT_EFFECT_RECORD_SIZE];
};

// The first texel of reserved row zero stores reciprocal height. Sampling at UV zero
// finds it even after growth, so existing vertex indices remain valid across batches.
static inline void FontSetEffectTextureHeight(FontEffectRecord* header, uint32_t height)
{
    memset(header, 0, sizeof(*header));
    const float reciprocal = 1.0f / height;
    uint32_t    bits;
    memcpy(&bits, &reciprocal, sizeof(bits));
    for (uint32_t i = 0; i < 4; ++i)
    {
        header->m_Data[i] = (uint8_t)(bits >> (i * 8));
    }
}

// Encodes up to four colors and positioned stops without allocating memory.
static inline void FontPackEffect(const FontColorEffect& effect, FontEffectRecord* record)
{
    assert(effect.m_StopCount > 0 && effect.m_StopCount <= 4);
    memset(record, 0, sizeof(*record));
    record->m_Data[0] = effect.m_Type;
    record->m_Data[1] = effect.m_StopCount;
    record->m_Data[2] = effect.m_Flags;
    for (uint32_t i = 0; i < 4; ++i)
    {
        const uint32_t stop = i < effect.m_StopCount ? i : effect.m_StopCount - 1;
        memcpy(record->m_Data + 4 + i * 4, effect.m_Colors[stop], 4);
        const uint32_t position = (uint32_t)(effect.m_Positions[stop] * 65535.0f + 0.5f);
        record->m_Data[20 + i * 2] = (uint8_t)(position >> 8);
        record->m_Data[21 + i * 2] = (uint8_t)position;
    }
    for (uint32_t i = 0; i < 6; ++i)
    {
        // Explicit byte order: the GPU decodes these bytes without integer bit operations.
        uint32_t parameter;
        memcpy(&parameter, &effect.m_Parameters[i], sizeof(parameter));
        for (uint32_t byte = 0; byte < 4; ++byte)
        {
            record->m_Data[28 + i * 4 + byte] = (uint8_t)(parameter >> (byte * 8));
        }
    }
}

#endif // DM_FONT_EFFECT_H
