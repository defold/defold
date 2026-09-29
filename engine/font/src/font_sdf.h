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

#ifndef DM_FONT_SDF_H
#define DM_FONT_SDF_H

#include "font_outline.h"

/*# signed-distance-field generation parameters
 * @struct
 * @name FontSDFParams
 * @member m_Scale [type: float] scale from font coordinates to bitmap pixels
 * @member m_Spread [type: float] distance range in pixels; bitmap padding is rounded up
 * @member m_OnEdgeValue [type: uint8_t] bitmap value assigned to the glyph edge
 * @member m_OutputBitmap [type: bool] output alpha coverage directly, without quantizing distance first
 * @member m_OutlineWidth [type: float] bitmap outline width; positive values add a second coverage channel
 * @member m_Antialias [type: bool] antialias bitmap coverage
 */
struct FontSDFParams
{
    float    m_Scale;
    float    m_Spread;
    uint8_t  m_OnEdgeValue;
    bool     m_OutputBitmap;
    float    m_OutlineWidth;
    bool     m_Antialias;
};

/*# generate a signed distance field from a glyph outline
 * Distances are calculated directly from the outline's line and Bezier
 * segments. The outline is filled using the non-zero winding rule. The
 * returned bitmap is top-down. SDF output has one channel; bitmap output has
 * face coverage and, when requested, a second outline coverage channel.
 * Its data is owned by the
 * bitmap and must be released with FontSDFFree.
 * @name FontSDFGenerate
 * @param outline [type: const FontOutline*] source outline in unscaled font coordinates
 * @param params [type: const FontSDFParams*] rasterization parameters
 * @param bitmap [type: FontGlyphBitmap*] (out) generated single-channel bitmap
 * @param offset_x [type: int32_t*] (out) bitmap left edge in scaled glyph coordinates
 * @param offset_y [type: int32_t*] (out) bitmap top edge in scaled, downward-positive glyph coordinates
 * @return result [type: FontResult] FONT_RESULT_OK on success
 */
FontResult FontSDFGenerate(const FontOutline* outline, const FontSDFParams* params,
                           FontGlyphBitmap* bitmap, int32_t* offset_x, int32_t* offset_y);

/*# release a generated signed distance field
 * Frees m_Data and clears the bitmap. The FontGlyphBitmap value itself remains
 * caller-owned.
 * @name FontSDFFree
 * @param bitmap [type: FontGlyphBitmap*] bitmap returned by FontSDFGenerate
 */
void FontSDFFree(FontGlyphBitmap* bitmap);

#endif // DM_FONT_SDF_H
