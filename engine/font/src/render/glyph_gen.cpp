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

#include "glyph_gen.h"

#include <stdlib.h>
#include <string.h>

#include <dmsdk/dlib/math.h>

static const float SDF_EDGE = 0.75f;

FontGlyphGenParams::FontGlyphGenParams()
    : m_Scale(1.0f)
    , m_SdfPadding(3.0f)
    , m_OutlineWidth(0.0f)
    , m_ShadowBlur(0.0f)
    , m_SdfEdgeValue(191)
    , m_OutputBitmap(false)
    , m_Antialias(true)
    , m_HasOutline(false)
    , m_HasShadow(false)
{
}

static float CalcSdfValueU8(float padding, float width, uint8_t edge_value)
{
    const float base_edge = SDF_EDGE * 255.0f;
    const float pixel_dist_scale = (float)edge_value / padding;
    return base_edge - pixel_dist_scale * width;
}

static uint8_t RemapSdfValue(uint8_t value, float outline_edge)
{
    float unit = value / outline_edge;
    return (uint8_t)(dmMath::Clamp(unit, 0.0f, 1.0f) * SDF_EDGE * 255.0f);
}

static uint8_t SdfCoverage(uint8_t value, float edge, float pixel_dist_scale, bool antialias)
{
    if (!antialias)
        return value >= edge ? 255 : 0;
    const float coverage = 0.5f + ((float)value - edge) / pixel_dist_scale;
    return (uint8_t)(dmMath::Clamp(coverage, 0.0f, 1.0f) * 255.0f);
}

static bool BlurBitmapChannel(uint8_t* pixels, uint32_t width, uint32_t height, uint32_t channels, uint32_t channel, uint32_t passes)
{
    if (passes == 0 || width == 0 || height == 0)
        return true;

    const uint64_t pixel_count = (uint64_t)width * height;
    if (pixel_count > UINT32_MAX)
        return false;

    uint8_t* source = (uint8_t*)malloc((size_t)pixel_count);
    uint8_t* target = (uint8_t*)malloc((size_t)pixel_count);
    if (!source || !target)
    {
        free(source);
        free(target);
        return false;
    }

    for (uint32_t i = 0; i < pixel_count; ++i)
        source[i] = pixels[i * channels + channel];

    // This is the same separable 3x3 Gaussian kernel used by the former Java
    // bitmap compiler: [1 2 1; 2 4 2; 1 2 1] / 16. Border pixels retain
    // their source value, matching ConvolveOp.EDGE_NO_OP.
    for (uint32_t pass = 0; pass < passes; ++pass)
    {
        for (uint32_t y = 0; y < height; ++y)
        {
            for (uint32_t x = 0; x < width; ++x)
            {
                const uint32_t offset = y * width + x;
                if (x == 0 || y == 0 || x + 1 == width || y + 1 == height)
                {
                    target[offset] = source[offset];
                    continue;
                }

                const uint32_t sum =
                    source[offset - width - 1] + 2 * source[offset - width] + source[offset - width + 1] +
                    2 * source[offset - 1] + 4 * source[offset] + 2 * source[offset + 1] +
                    source[offset + width - 1] + 2 * source[offset + width] + source[offset + width + 1];
                target[offset] = (uint8_t)(sum / 16);
            }
        }

        uint8_t* swap = source;
        source = target;
        target = swap;
    }

    for (uint32_t i = 0; i < pixel_count; ++i)
        pixels[i * channels + channel] = source[i];

    free(source);
    free(target);
    return true;
}

uint32_t FontGetGlyphChannelCount(bool output_bitmap, bool has_outline, bool has_shadow, float shadow_blur)
{
    if (output_bitmap)
        return has_outline || has_shadow ? 3 : 1;
    return shadow_blur > 0.0f ? 3 : 1;
}

FontResult FontGenerateGlyph(HFont font, uint32_t glyph_index, const FontGlyphGenParams* params, FontGlyph* glyph)
{
    if (font == 0 || params == 0 || glyph == 0 || params->m_Scale <= 0.0f || params->m_SdfPadding <= 0.0f)
        return FONT_RESULT_ERROR;
    memset(glyph, 0, sizeof(*glyph));

    FontGlyphOptions options;
    options.m_Scale = params->m_Scale;
    options.m_GenerateImage = true;
    options.m_StbttSDFPadding = params->m_SdfPadding;
    options.m_StbttSDFOnEdgeValue = params->m_SdfEdgeValue;

    FontResult result = FontGetGlyphByIndex(font, glyph_index, &options, glyph);
    if (result != FONT_RESULT_OK || glyph->m_Bitmap.m_Data == 0)
        return result;

    const uint32_t width = glyph->m_Bitmap.m_Width;
    const uint32_t height = glyph->m_Bitmap.m_Height;
    const uint32_t channels = FontGetGlyphChannelCount(params->m_OutputBitmap, params->m_HasOutline, params->m_HasShadow, params->m_ShadowBlur);
    if (channels == 1 && !params->m_OutputBitmap)
        return result;
    const uint64_t pixel_count = (uint64_t)width * height * channels;
    if (pixel_count > UINT32_MAX)
    {
        FontFreeGlyph(font, glyph);
        memset(glyph, 0, sizeof(*glyph));
        return FONT_RESULT_ERROR;
    }
    uint8_t* rgb = (uint8_t*)malloc((size_t)pixel_count);
    if (rgb == 0)
    {
        FontFreeGlyph(font, glyph);
        memset(glyph, 0, sizeof(*glyph));
        return FONT_RESULT_ERROR;
    }

    const float pixel_dist_scale = (float)params->m_SdfEdgeValue / params->m_SdfPadding;
    const float outline_edge = CalcSdfValueU8(params->m_SdfPadding, params->m_OutlineWidth, params->m_SdfEdgeValue);
    for (uint32_t y = 0; y < height; ++y)
    {
        for (uint32_t x = 0; x < width; ++x)
        {
            const uint8_t  value = glyph->m_Bitmap.m_Data[y * width + x];
            const uint32_t offset = (y * width + x) * channels;
            if (params->m_OutputBitmap)
            {
                const uint8_t face_coverage = SdfCoverage(value, params->m_SdfEdgeValue, pixel_dist_scale, params->m_Antialias);
                rgb[offset + 0] = face_coverage;
                if (channels == 3)
                {
                    const uint8_t outline_coverage = params->m_HasOutline ? SdfCoverage(value, outline_edge, pixel_dist_scale, params->m_Antialias) : 0;
                    rgb[offset + 1] = outline_coverage;
                    rgb[offset + 2] = params->m_HasShadow ? (params->m_HasOutline ? outline_coverage : face_coverage) : 0;
                }
            }
            else
            {
                rgb[offset + 0] = value;
                rgb[offset + 1] = 0;
                rgb[offset + 2] = RemapSdfValue(value, outline_edge);
            }
        }
    }

    if (params->m_OutputBitmap && params->m_HasShadow && params->m_ShadowBlur > 0.0f &&
        !BlurBitmapChannel(rgb, width, height, channels, 2, (uint32_t)params->m_ShadowBlur))
    {
        free(rgb);
        FontFreeGlyph(font, glyph);
        memset(glyph, 0, sizeof(*glyph));
        return FONT_RESULT_ERROR;
    }

    free(glyph->m_Bitmap.m_Data);
    glyph->m_Bitmap.m_Data = rgb;
    glyph->m_Bitmap.m_DataSize = (uint32_t)pixel_count;
    glyph->m_Bitmap.m_Channels = channels;
    return FONT_RESULT_OK;
}
