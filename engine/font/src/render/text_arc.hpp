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

#ifndef DM_FONT_TEXT_ARC_HPP
#define DM_FONT_TEXT_ARC_HPP

#include "../text_layout.h"
#include <float.h>
#include <math.h>
#include <dlib/math.h>
#include <dlib/vmath.h>

// Prepared circular mapping shared by font vertices, culling and label picking.
// The layout is borrowed only while preparing the mapping; no pointers are kept.
struct FontTextArc
{
    float m_Radius;
    float m_MinX;
    float m_Width;
    float m_CenterX;
    float m_CenterY;
    float m_FirstBaseline;
    float m_Span;
};

// Horizontal center of the cluster's layout bounds at the baseline. Marks
// share the same rigid frame. The optional end receives the next cluster index.
static inline float FontGetTextClusterPivot(const TextGlyph* glyphs, const TextLine& line, uint32_t index, float line_x, float first_x, uint32_t* end)
{
    uint32_t start = index;
    while (start > line.m_Index && glyphs[start - 1].m_Cluster == glyphs[index].m_Cluster)
        --start;
    uint32_t next = start + 1;
    float    minimum = glyphs[start].m_X;
    float    maximum = minimum + glyphs[start].m_Width;
    while (next < line.m_Index + line.m_Length && glyphs[next].m_Cluster == glyphs[index].m_Cluster)
    {
        minimum = dmMath::Min(minimum, glyphs[next].m_X);
        maximum = dmMath::Max(maximum, glyphs[next].m_X + glyphs[next].m_Width);
        ++next;
    }
    if (end)
        *end = next;
    return line_x + (minimum + maximum) * 0.5f - first_x;
}

// Prepare the aligned layout's circular baseline, or return false for straight or empty text.
static inline bool FontPrepareTextArc(HTextLayout layout, float width, float height, uint32_t align, uint32_t valign, float padding, float radius, float degrees, FontTextArc* arc)
{
    const uint32_t line_count = TextLayoutGetLineCount(layout);
    if (degrees == 0.0f || line_count == 0)
        return false;

    const TextLine*      lines = TextLayoutGetLines(layout);
    const TextParagraph* paragraphs = TextLayoutGetParagraphs(layout);
    float                min_x = FLT_MAX;
    float                max_x = -FLT_MAX;
    for (uint32_t i = 0; i < line_count; ++i)
    {
        uint32_t line_align = align;
        if (paragraphs[lines[i].m_ParagraphIndex].m_Direction == TEXT_DIRECTION_RTL && align != 1)
            line_align = 2 - align;
        const float factor = line_align * 0.5f;
        const float x = factor * (width - lines[i].m_Width) - padding * 0.5f;
        min_x = dmMath::Min(min_x, x);
        max_x = dmMath::Max(max_x, x + lines[i].m_Width);
    }
    if (max_x <= min_x)
        return false;

    float layout_width;
    float layout_height;
    TextLayoutGetBounds(layout, &layout_width, &layout_height);
    (void)layout_width;
    const float y = valign == 1 ? (height - layout_height) * 0.5f : valign == 2 ? 0.0f :
                                                                                  height - layout_height;
    const float radians = 0.017453292519943295f;
    arc->m_Radius = radius;
    arc->m_MinX = min_x;
    arc->m_Width = max_x - min_x;
    arc->m_CenterX = 0.0f;
    arc->m_CenterY = 0.0f;
    arc->m_FirstBaseline = y + lines[0].m_Baseline;
    arc->m_Span = degrees * radians;
    return true;
}

// Shared arc evaluation for glyph transforms and guide vertices.
static inline dmVMath::Matrix4 FontGetTextArcFrame(const FontTextArc& arc, float angle, float radius)
{
    const float cosine = cosf(angle);
    const float sine = sinf(angle);
    return dmVMath::Matrix4(dmVMath::Vector4(cosine, -sine, 0.0f, 0.0f),
                            dmVMath::Vector4(sine, cosine, 0.0f, 0.0f),
                            dmVMath::Vector4(0.0f, 0.0f, 1.0f, 0.0f),
                            dmVMath::Vector4(arc.m_CenterX + sine * radius, arc.m_CenterY + cosine * radius, 0.0f, 1.0f));
}

// One rigid transform per glyph cluster keeps marks and shadow/outline layers attached.
static inline dmVMath::Matrix4 FontGetTextArcTransform(const FontTextArc& arc, float x, float baseline)
{
    const float      angle = ((x - arc.m_MinX) / arc.m_Width - 0.5f) * arc.m_Span;
    const float      radius = arc.m_Radius + (baseline - arc.m_FirstBaseline);
    dmVMath::Matrix4 frame = FontGetTextArcFrame(arc, angle, radius);
    // Include the inverse pivot directly instead of multiplying three matrices.
    frame.setTranslation(frame.getTranslation() - frame.getCol0().getXYZ() * x - frame.getCol1().getXYZ() * baseline);
    return frame;
}

#endif // DM_FONT_TEXT_ARC_HPP
