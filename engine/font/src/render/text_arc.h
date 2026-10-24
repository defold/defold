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

#ifndef DM_FONT_TEXT_ARC_H
#define DM_FONT_TEXT_ARC_H

#include <stdbool.h>
#include <stdint.h>

struct TextLayout;
struct TextGlyph;
struct TextLine;

#ifdef __cplusplus
extern "C"
{
#endif

    // Prepared circular mapping shared by font vertices, culling and label picking.
    // The layout is borrowed only while preparing the mapping; no pointers are kept.
    typedef struct FontTextArc
    {
        float m_Radius;
        float m_MinX;
        float m_Width;
        float m_FirstBaseline;
        float m_Span;
    } FontTextArc;

    // Horizontal center of the cluster's layout bounds at the baseline. Marks
    // share the same rigid frame. The optional end receives the next cluster index.
    float FontGetTextClusterPivot(const struct TextGlyph* glyphs, const struct TextLine* line, uint32_t index, float line_x, float first_x, uint32_t* end);

    // Prepare the aligned layout's circular baseline, or return false for straight or empty text.
    bool FontPrepareTextArc(struct TextLayout* layout, float width, float height, uint32_t align, uint32_t valign, float padding, float radius, float degrees, FontTextArc* arc);

    // Shared origin-centered arc evaluation for glyph transforms and guide vertices.
    // Writes 16 column-major floats to transform. Angle is in radians.
    void FontGetTextArcFrame(float angle, float radius, float transform[16]);

    // One rigid transform per glyph cluster keeps marks and shadow/outline layers attached.
    // Writes 16 column-major floats to transform.
    void FontGetTextArcTransform(const FontTextArc* arc, float x, float baseline, float transform[16]);

#ifdef __cplusplus
}
#endif

#endif // DM_FONT_TEXT_ARC_H
