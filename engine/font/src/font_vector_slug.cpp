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

#include "font_vector_slug.h"
#include <math.h>
#include <string.h>

void FontVectorSlugBegin(FontVectorSlugData* data)
{
    dmSlug::Begin(data);
}

bool FontVectorSlugAddGlyph(FontVectorSlugData* data, const FontGlyphOutline& outline, float font_size, uint32_t bands, FontVectorSlugGlyph* glyph)
{
    data->m_Overflow = false;
    memset(glyph, 0, sizeof(*glyph));
    if (!isfinite(font_size) || font_size <= 0 || bands < 1 || bands > 32 ||
        outline.m_CommandCount > 4096 || (outline.m_CommandCount && !outline.m_Commands))
        return false;
    dmArray<dmSlug::CurveCommand> commands;
    commands.SetCapacity(outline.m_CommandCount);
    commands.SetSize(outline.m_CommandCount);
    for (uint32_t i = 0; i < outline.m_CommandCount; ++i)
    {
        commands[i].m_Type = outline.m_Commands[i].m_Type;
        uint32_t points = commands[i].m_Type == FONT_CURVE_QUADRATIC_TO ? 2 : (commands[i].m_Type == FONT_CURVE_CLOSE ? 0 : 1);
        for (uint32_t p = 0; p < points; ++p)
        {
            commands[i].m_Points[p].m_X = outline.m_Commands[i].m_Points[p].m_X;
            commands[i].m_Points[p].m_Y = outline.m_Commands[i].m_Points[p].m_Y;
        }
    }
    return dmSlug::AddPath(data, commands.Begin(), commands.Size(), font_size, bands, glyph);
}

bool FontVectorSlugAddFontGlyph(FontVectorSlugData* data, const FontGlyph& source, uint32_t bands, FontVectorSlugGlyph* glyph)
{
    data->m_Overflow = false;
    memset(glyph, 0, sizeof(*glyph));
    if (bands < 1 || bands > 32)
        return false;
    if (source.m_Vector.m_DataSize)
    {
        const uint32_t stride = 8 * sizeof(float);
        if (!source.m_Vector.m_Data || source.m_Vector.m_CurveCount > 1024 ||
            source.m_Vector.m_DataSize / stride != source.m_Vector.m_CurveCount || source.m_Vector.m_DataSize % stride)
            return false;
        return dmSlug::AddEncodedCurves(data, source.m_Vector.m_Data, source.m_Vector.m_CurveCount, bands, glyph);
    }
    const FontGlyphOutline& outline = source.m_Outline;
    const float             height = outline.m_Ascent + outline.m_Descent;
    if (!isfinite(outline.m_Width) || !isfinite(height) || outline.m_Width <= 0 || height <= 0 ||
        outline.m_CommandCount > 4096 || (outline.m_CommandCount && !outline.m_Commands))
        return false;
    dmArray<dmSlug::CurveCommand> commands;
    commands.SetCapacity(outline.m_CommandCount);
    commands.SetSize(outline.m_CommandCount);
    for (uint32_t i = 0; i < outline.m_CommandCount; ++i)
    {
        commands[i].m_Type = outline.m_Commands[i].m_Type;
        uint32_t points = commands[i].m_Type == FONT_CURVE_QUADRATIC_TO ? 2 : (commands[i].m_Type == FONT_CURVE_CLOSE ? 0 : 1);
        for (uint32_t j = 0; j < points; ++j)
        {
            commands[i].m_Points[j].m_X = (outline.m_Commands[i].m_Points[j].m_X - outline.m_LeftBearing) / outline.m_Width;
            commands[i].m_Points[j].m_Y = (outline.m_Commands[i].m_Points[j].m_Y + outline.m_Descent) / height;
        }
    }
    return dmSlug::AddPath(data, commands.Begin(), commands.Size(), 1.0f, bands, glyph);
}
