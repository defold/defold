// Copyright 2020-2026 The Defold Foundation
// Licensed under the Defold License version 1.0.

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
    if (result != FONT_RESULT_OK || params->m_ShadowBlur <= 0.0f || glyph->m_Bitmap.m_Data == 0)
        return result;

    const uint32_t width = glyph->m_Bitmap.m_Width;
    const uint32_t height = glyph->m_Bitmap.m_Height;
    const uint32_t channels = 3;
    uint8_t* rgb = (uint8_t*)malloc(width * height * channels);
    if (rgb == 0)
    {
        FontFreeGlyph(font, glyph);
        memset(glyph, 0, sizeof(*glyph));
        return FONT_RESULT_ERROR;
    }

    const float outline_edge = CalcSdfValueU8(params->m_SdfPadding, params->m_OutlineWidth, params->m_SdfEdgeValue);
    for (uint32_t y = 0; y < height; ++y)
    {
        for (uint32_t x = 0; x < width; ++x)
        {
            const uint8_t value = glyph->m_Bitmap.m_Data[y * width + x];
            const uint32_t offset = (y * width + x) * channels;
            rgb[offset + 0] = value;
            rgb[offset + 1] = 0;
            rgb[offset + 2] = RemapSdfValue(value, outline_edge);
        }
    }

    free(glyph->m_Bitmap.m_Data);
    glyph->m_Bitmap.m_Data = rgb;
    glyph->m_Bitmap.m_DataSize = width * height * channels;
    glyph->m_Bitmap.m_Channels = channels;
    return FONT_RESULT_OK;
}
