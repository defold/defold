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

#include "text_layout.h"

static MarkupResult MarkupUnsupported(HMarkup* out_markup, MarkupError* out_error)
{
    if (out_markup)
    {
        *out_markup = 0;
    }

    if (out_error)
    {
        out_error->m_ByteOffset = 0;
        out_error->m_Type = MARKUP_ERROR_UNSUPPORTED;
    }

    return MARKUP_RESULT_UNSUPPORTED;
}

MarkupResult MarkupCreate(const char*, uint32_t, HMarkup* out_markup, MarkupError* out_error)
{
    return MarkupUnsupported(out_markup, out_error);
}

MarkupResult MarkupCreateRecovering(const char*, uint32_t, HMarkup* out_markup, MarkupError* out_error)
{
    return MarkupUnsupported(out_markup, out_error);
}

MarkupResult MarkupCreateStyleFragment(const char*, uint32_t, HMarkup* out_markup, MarkupError* out_error)
{
    return MarkupUnsupported(out_markup, out_error);
}

void MarkupDestroy(HMarkup)
{
}

const char* MarkupGetSource(HMarkup)
{
    return 0;
}

uint32_t MarkupGetSourceLength(HMarkup)
{
    return 0;
}

const uint32_t* MarkupGetText(HMarkup)
{
    return 0;
}

uint32_t MarkupGetTextLength(HMarkup)
{
    return 0;
}

const MarkupSpan* MarkupGetSpans(HMarkup)
{
    return 0;
}

uint32_t MarkupGetSpanCount(HMarkup)
{
    return 0;
}

const MarkupStyleNode* MarkupGetStyleNodes(HMarkup)
{
    return 0;
}

uint32_t MarkupGetStyleNodeCount(HMarkup)
{
    return 0;
}

const MarkupAttribute* MarkupGetAttributes(HMarkup)
{
    return 0;
}

uint32_t MarkupGetAttributeCount(HMarkup)
{
    return 0;
}

bool TextLayoutResolveMarkup(HMarkup, TextLayoutSettings*, ResolvedMarkup*)
{
    return false;
}

bool TextLayoutCompileStyleFragment(const char*, uint32_t, TextRenderStyle*, dmArray<TextEffect>*, MarkupError*)
{
    return false;
}

void TextLayoutGetGlyphRenderData(HTextLayout, const TextGlyph&, const float base_color[4], TextGlyphRenderData* data)
{
    for (uint32_t channel = 0; channel < 4; ++channel)
    {
        data->m_FaceColors.m_BottomLeft[channel] = base_color[channel];
        data->m_FaceColors.m_BottomRight[channel] = base_color[channel];
        data->m_FaceColors.m_TopLeft[channel] = base_color[channel];
        data->m_FaceColors.m_TopRight[channel] = base_color[channel];
        data->m_OutlineColor[channel] = 1.0f;
        data->m_ShadowColor[channel] = 1.0f;
    }

    data->m_OffsetX = 0.0f;
    data->m_OffsetY = 0.0f;
    data->m_OutlineWidth = 0.0f;
    data->m_ShadowX = 0.0f;
    data->m_ShadowY = 0.0f;
    data->m_ShadowBlur = 0.0f;
    data->m_StyleFlags = 0;
}

void TextLayoutGetGlyphFaceColors(HTextLayout layout, const TextGlyph& glyph, const float base_color[4], TextGlyphFaceColors* colors)
{
    TextGlyphRenderData data;
    TextLayoutGetGlyphRenderData(layout, glyph, base_color, &data);
    *colors = data.m_FaceColors;
}

bool TextLayoutHasMarkupOutline(HTextLayout)
{
    return false;
}

float TextLayoutGetMaxMarkupOutlineWidth(HTextLayout)
{
    return 0.0f;
}

bool TextLayoutHasMarkupShadow(HTextLayout)
{
    return false;
}
