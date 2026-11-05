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

/** Prepared circular mapping. Contains values only; no layout pointers are kept. */
typedef struct FontTextArc
{
    float m_Radius;
    float m_MinX;
    float m_Width;
    float m_FirstBaseline;
    float m_Span; // Radians.
} FontTextArc;

/**
 * Return the aligned horizontal center of a glyph cluster's layout bounds.
 * Combining marks share the cluster's pivot and rigid transform.
 * @param glyphs Layout glyph array, borrowed for this call
 * @param line Line containing the glyph
 * @param index Index into glyphs
 * @param line_x Aligned line start in local units
 * @param first_x Shaped line origin to subtract from glyph coordinates
 * @param end Optional output index immediately after the cluster
 * @return Cluster pivot x coordinate in local units
 */
float FontGetTextClusterPivot(const struct TextGlyph* glyphs, const struct TextLine* line, uint32_t index, float line_x, float first_x, uint32_t* end);

/**
 * Prepare an origin-centered arc from the aligned text block.
 * @param layout Borrowed text layout
 * @param width Layout box width in local units
 * @param height Layout box height in local units
 * @param align Horizontal alignment: 0 left, 1 center, 2 right
 * @param valign Vertical alignment: 0 top, 1 center, 2 bottom
 * @param padding Monospace padding in local units
 * @param radius Signed first-baseline radius in local units
 * @param degrees Arc span in degrees; zero disables the arc
 * @param arc Output mapping, valid when true is returned
 * @return True for nonempty curved text, false otherwise
 */
bool FontPrepareTextArc(struct TextLayout* layout, float width, float height, uint32_t align, uint32_t valign, float padding, float radius, float degrees, FontTextArc* arc);

/**
 * Evaluate the shared circular frame used by glyphs and editor guides.
 * @param angle Angle in radians from the positive y axis
 * @param radius Signed baseline radius in local units
 * @param transform Output 16 column-major floats; no alignment requirement
 */
void FontGetTextArcFrame(float angle, float radius, float transform[16]);

/**
 * Map a glyph cluster's baseline pivot onto the prepared arc.
 * Each line preserves layout advances along its own radius. A zero-radius
 * baseline follows a straight tangent through the origin.
 * @param arc Prepared mapping, borrowed for this call
 * @param x Aligned cluster pivot x coordinate
 * @param baseline Layout baseline y coordinate
 * @param transform Output 16 column-major floats; no alignment requirement
 */
void FontGetTextArcTransform(const FontTextArc* arc, float x, float baseline, float transform[16]);

#ifdef __cplusplus
}
#endif

#endif // DM_FONT_TEXT_ARC_H
