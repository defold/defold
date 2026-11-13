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

#include "text_arc.h"
#include "../text_layout.h"
#include <float.h>
#include <math.h>
#include <string.h>
#include <dlib/math.h>

using namespace dmVMath;

float FontGetTextClusterPivot(const TextGlyph* glyphs, const TextLine* line, uint32_t index, float line_x, float first_x, uint32_t* end)
{
    uint32_t start = index;
    while (start > line->m_Index && glyphs[start - 1].m_Cluster == glyphs[index].m_Cluster)
    {
        --start;
    }
    uint32_t next = start + 1;
    float    minimum = glyphs[start].m_X;
    float    maximum = minimum + glyphs[start].m_Width;
    while (next < line->m_Index + line->m_Length && glyphs[next].m_Cluster == glyphs[index].m_Cluster)
    {
        minimum = dmMath::Min(minimum, glyphs[next].m_X);
        maximum = dmMath::Max(maximum, glyphs[next].m_X + glyphs[next].m_Width);
        ++next;
    }
    if (end)
    {
        *end = next;
    }
    return line_x + (minimum + maximum) * 0.5f - first_x;
}

bool FontPrepareTextArc(HTextLayout layout, float width, float height, uint32_t align, uint32_t valign, float padding, float radius, float degrees, FontTextArc* arc)
{
    const uint32_t line_count = TextLayoutGetLineCount(layout);
    if (degrees == 0.0f || line_count == 0)
    {
        return false;
    }

    const TextLine*      lines = TextLayoutGetLines(layout);
    const TextParagraph* paragraphs = TextLayoutGetParagraphs(layout);
    float                min_x = FLT_MAX;
    float                max_x = -FLT_MAX;
    for (uint32_t i = 0; i < line_count; ++i)
    {
        uint32_t line_align = align;
        if (paragraphs[lines[i].m_ParagraphIndex].m_Direction == TEXT_DIRECTION_RTL && align != 1)
        {
            line_align = 2 - align;
        }
        const float align_factor = line_align * 0.5f;
        const float line_x = align_factor * (width - lines[i].m_Width) - padding * 0.5f;
        min_x = dmMath::Min(min_x, line_x);
        max_x = dmMath::Max(max_x, line_x + lines[i].m_Width);
    }
    if (max_x <= min_x)
    {
        return false;
    }

    float layout_width;
    float layout_height;
    TextLayoutGetBounds(layout, &layout_width, &layout_height);
    (void)layout_width;
    float layout_y = height - layout_height;
    if (valign == 1)
    {
        layout_y = (height - layout_height) * 0.5f;
    }
    else if (valign == 2)
    {
        layout_y = 0.0f;
    }

    arc->m_Radius = radius;
    arc->m_MinX = min_x;
    arc->m_Width = max_x - min_x;
    arc->m_FirstBaseline = layout_y + lines[0].m_Baseline;
    arc->m_Span = dmMath::DegToRad(degrees);
    return true;
}

static Matrix4 GetTextArcFrame(float angle, float radius)
{
    Matrix4 frame = Matrix4::rotationZ(-angle);
    frame.setTranslation(frame.getCol1().getXYZ() * radius);
    return frame;
}

void FontGetTextArcFrame(float angle, float radius, float transform[16])
{
    const Matrix4 frame = GetTextArcFrame(angle, radius);
    memcpy(transform, &frame, sizeof(frame));
}

void FontGetTextArcTransform(const FontTextArc* arc, float x, float baseline, float transform[16])
{
    const float distance = x - (arc->m_MinX + arc->m_Width * 0.5f);
    const float radius = arc->m_Radius + (baseline - arc->m_FirstBaseline);
    // Finalized baselines and vertical alignment can round an exact zero into a tiny circle.
    const float precision = FLT_EPSILON * (fabsf(arc->m_Radius) + fabsf(baseline) + fabsf(arc->m_FirstBaseline));
    Matrix4 frame;
    if (radius == 0.0f || (baseline != arc->m_FirstBaseline && fabsf(radius) <= precision))
    {
        // A zero-length circular baseline uses its straight tangent.
        frame = Matrix4::translation(Vector3(distance, 0.0f, 0.0f));
    }
    else
    {
        frame = GetTextArcFrame(distance / radius, radius);
    }

    // Apply the inverse cluster pivot in this local frame.
    frame.setTranslation(frame.getTranslation()
                         - frame.getCol0().getXYZ() * x
                         - frame.getCol1().getXYZ() * baseline);
    memcpy(transform, &frame, sizeof(frame));
}
