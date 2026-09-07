// Copyright 2026 The Defold Foundation
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

#ifndef DM_FONT_HARFBUZZ_H
#define DM_FONT_HARFBUZZ_H

#include "font_outline.h"

typedef struct hb_font_t hb_font_t;

/*# HarfBuzz-backed TrueType/OpenType face
 * Opaque face data used to map characters, read metrics, and decode outlines.
 * The object borrows the complete source font data supplied to
 * FontHarfbuzzCreate.
 * @typedef
 * @name FontHarfbuzz
 */
struct FontHarfbuzz;

/*# create a HarfBuzz-backed TrueType/OpenType face
 * The source data is borrowed and must remain valid until destruction.
 * @name FontHarfbuzzCreate
 * @param data [type: const void*] complete TTF, OTF, or TTC data
 * @param data_size [type: uint32_t] size of data in bytes
 * @param face_index [type: uint32_t] zero-based face index
 * @return font [type: FontHarfbuzz*] face, or null when allocation fails
 */
FontHarfbuzz* FontHarfbuzzCreate(const void* data, uint32_t data_size, uint32_t face_index);

/*# destroy a HarfBuzz-backed face
 * Releases face state without releasing the borrowed source font data.
 * @name FontHarfbuzzDestroy
 * @param font [type: FontHarfbuzz*] face to destroy
 */
void FontHarfbuzzDestroy(FontHarfbuzz* font);

/*# decode a glyph outline
 * The returned outline must be released with FontFreeGlyphOutline.
 * @name FontHarfbuzzGetGlyphOutline
 * @param font [type: FontHarfbuzz*] source face
 * @param glyph_index [type: uint32_t] glyph index
 * @param outline [type: FontOutline*] (out) decoded outline
 * @return result [type: FontResult] FONT_RESULT_OK on success
 */
FontResult FontHarfbuzzGetGlyphOutline(FontHarfbuzz* font, uint32_t glyph_index, FontOutline* outline);

/*# map a code point to a glyph index
 * @name FontHarfbuzzGetGlyphIndex
 * @param font [type: FontHarfbuzz*] source face
 * @param codepoint [type: uint32_t] Unicode code point
 * @return glyph_index [type: uint32_t] glyph index, or zero if missing
 */
uint32_t FontHarfbuzzGetGlyphIndex(FontHarfbuzz* font, uint32_t codepoint);

/*# return the scale for a requested pixel size
 * @name FontHarfbuzzGetScaleFromSize
 * @param font [type: FontHarfbuzz*] source face
 * @param size [type: uint32_t] requested size in pixels
 * @return scale [type: float] scale from font coordinates to pixels
 */
float FontHarfbuzzGetScaleFromSize(FontHarfbuzz* font, uint32_t size);

/*# return unscaled vertical font metrics
 * @name FontHarfbuzzGetVerticalMetrics
 * @param font [type: FontHarfbuzz*] source face
 * @param ascent [type: int32_t*] (out) ascent
 * @param descent [type: int32_t*] (out) descent
 * @param line_gap [type: int32_t*] (out) line gap
 * @return success [type: bool] true when horizontal font extents are available
 */
bool FontHarfbuzzGetVerticalMetrics(FontHarfbuzz* font, int32_t* ascent, int32_t* descent, int32_t* line_gap);

/*# return unscaled horizontal glyph metrics
 * @name FontHarfbuzzGetGlyphHMetrics
 * @param font [type: FontHarfbuzz*] source face
 * @param glyph_index [type: uint32_t] glyph index
 * @param advance [type: int32_t*] (out) advance width
 * @param left_bearing [type: int32_t*] (out) left side bearing
 */
void FontHarfbuzzGetGlyphHMetrics(FontHarfbuzz* font, uint32_t glyph_index, int32_t* advance, int32_t* left_bearing);

/*# return the selected outline storage type
 * @name FontHarfbuzzGetOutlineType
 * @param font [type: FontHarfbuzz*] source face
 * @return outline_type [type: FontOutlineType] glyf, CFF1, CFF2, or unknown
 */
FontOutlineType FontHarfbuzzGetOutlineType(FontHarfbuzz* font);

/*# return unscaled glyph bounds
 * @name FontHarfbuzzGetGlyphBox
 * @param font [type: FontHarfbuzz*] source face
 * @param glyph_index [type: uint32_t] glyph index
 * @param x0 [type: int32_t*] (out) minimum horizontal coordinate
 * @param y0 [type: int32_t*] (out) minimum vertical coordinate
 * @param x1 [type: int32_t*] (out) maximum horizontal coordinate
 * @param y1 [type: int32_t*] (out) maximum vertical coordinate
 * @return has_bounds [type: bool] false for an empty glyph
 */
bool FontHarfbuzzGetGlyphBox(FontHarfbuzz* font, uint32_t glyph_index, int32_t* x0, int32_t* y0, int32_t* x1, int32_t* y1);

/*# return the wrapped HarfBuzz font
 * The returned pointer is borrowed and remains valid until FontHarfbuzzDestroy.
 * @name FontHarfbuzzGetFont
 * @param font [type: FontHarfbuzz*] source face
 * @return hb_font [type: hb_font_t*] borrowed HarfBuzz font
 */
hb_font_t* FontHarfbuzzGetFont(FontHarfbuzz* font);

/*# return the HarfBuzz font associated with a Defold font
 * The returned pointer is borrowed and remains valid until the source font is
 * destroyed.
 * @name FontGetHarfbuzzFontFromTTF
 * @param font [type: HFont] Defold TrueType/OpenType font
 * @return hb_font [type: hb_font_t*] borrowed HarfBuzz font
 */
hb_font_t* FontGetHarfbuzzFontFromTTF(HFont font);

#endif // DM_FONT_HARFBUZZ_H
