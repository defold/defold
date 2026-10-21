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
float FontGetTextClusterPivot(const TextGlyph* glyphs, const TextLine& line, uint32_t index, float line_x, float first_x, uint32_t* end);

// Prepare the aligned layout's circular baseline, or return false for straight or empty text.
bool FontPrepareTextArc(HTextLayout layout, float width, float height, uint32_t align, uint32_t valign, float padding, float radius, float degrees, FontTextArc* arc);

// Shared arc evaluation for glyph transforms and guide vertices.
dmVMath::Matrix4 FontGetTextArcFrame(const FontTextArc& arc, float angle, float radius);

// One rigid transform per glyph cluster keeps marks and shadow/outline layers attached.
dmVMath::Matrix4 FontGetTextArcTransform(const FontTextArc& arc, float x, float baseline);

#endif // DM_FONT_TEXT_ARC_HPP
