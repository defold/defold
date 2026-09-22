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

#ifndef DM_FONT_TRUETYPE_H
#define DM_FONT_TRUETYPE_H

#include "font_outline.h"

/*# parsed TrueType/OpenType face
 * Opaque face data used to map characters, read metrics, and decode outlines.
 * The object borrows the complete source font data supplied to
 * FontTrueTypeCreate.
 * @typedef
 * @name FontTrueType
 */
struct FontTrueType;

/*# get the number of faces in TrueType/OpenType data
 * @name FontTrueTypeGetFaceCount
 * @param data [type: const void*] complete TTF, OTF, or TTC data
 * @param data_size [type: uint32_t] size of data in bytes
 * @return face_count [type: uint32_t] number of valid face records
 */
uint32_t FontTrueTypeGetFaceCount(const void* data, uint32_t data_size);

/*# create a TrueType/OpenType face decoder
 * Creates a decoder when the selected SFNT face contains supported outline
 * data. The source data is borrowed and must remain valid until destruction.
 * @name FontTrueTypeCreate
 * @param data [type: const void*] complete TTF, OTF, or TTC data
 * @param data_size [type: uint32_t] size of data in bytes
 * @param face_index [type: uint32_t] zero-based face index
 * @return font [type: FontTrueType*] decoder, or null when the face is unsupported or invalid
 */
FontTrueType* FontTrueTypeCreate(const void* data, uint32_t data_size, uint32_t face_index);

/*# destroy a TrueType/OpenType face decoder
 * Releases decoder state without releasing the borrowed source font data.
 * @name FontTrueTypeDestroy
 * @param font [type: FontTrueType*] decoder to destroy
 */
void FontTrueTypeDestroy(FontTrueType* font);

/*# decode a glyph outline
 * Decodes the glyph at the default normalized variation coordinates. The
 * returned outline must be released with FontFreeGlyphOutline.
 * @name FontTrueTypeGetGlyphOutline
 * @param font [type: FontTrueType*] source face
 * @param glyph_index [type: uint32_t] glyph index
 * @param outline [type: FontOutline*] (out) decoded outline
 * @return result [type: FontResult] FONT_RESULT_OK on success or FONT_RESULT_ERROR for invalid glyph data
 */
FontResult FontTrueTypeGetGlyphOutline(FontTrueType* font, uint32_t glyph_index, FontOutline* outline);

/*# map a code point to a glyph index
 * @name FontTrueTypeGetGlyphIndex
 * @param font [type: FontTrueType*] source face
 * @param codepoint [type: uint32_t] Unicode code point
 * @return glyph_index [type: uint32_t] glyph index, or zero if missing
 */
uint32_t FontTrueTypeGetGlyphIndex(FontTrueType* font, uint32_t codepoint);

/*# return the scale for a requested pixel size
 * @name FontTrueTypeGetScaleFromSize
 * @param font [type: FontTrueType*] source face
 * @param size [type: uint32_t] requested size in pixels
 * @return scale [type: float] scale from font coordinates to pixels
 */
float FontTrueTypeGetScaleFromSize(FontTrueType* font, uint32_t size);

/*# return unscaled vertical font metrics
 * @name FontTrueTypeGetVerticalMetrics
 * @param font [type: FontTrueType*] source face
 * @param ascent [type: int32_t*] (out) ascent
 * @param descent [type: int32_t*] (out) descent
 * @param line_gap [type: int32_t*] (out) line gap
 * @return success [type: bool] true when the metrics are available
 */
bool FontTrueTypeGetVerticalMetrics(FontTrueType* font, int32_t* ascent, int32_t* descent, int32_t* line_gap);

/*# return unscaled horizontal glyph metrics
 * @name FontTrueTypeGetGlyphHMetrics
 * @param font [type: FontTrueType*] source face
 * @param glyph_index [type: uint32_t] glyph index
 * @param advance [type: int32_t*] (out) advance width
 * @param left_bearing [type: int32_t*] (out) left side bearing
 */
void FontTrueTypeGetGlyphHMetrics(FontTrueType* font, uint32_t glyph_index, int32_t* advance, int32_t* left_bearing);

/*# return the selected outline storage type
 * @name FontTrueTypeGetOutlineType
 * @param font [type: FontTrueType*] source face
 * @return outline_type [type: FontOutlineType] glyf, CFF1, or CFF2
 */
FontOutlineType FontTrueTypeGetOutlineType(FontTrueType* font);

/*# return unscaled glyph bounds
 * @name FontTrueTypeGetGlyphBox
 * @param font [type: FontTrueType*] source face
 * @param glyph_index [type: uint32_t] glyph index
 * @param x0 [type: int32_t*] (out) minimum horizontal coordinate
 * @param y0 [type: int32_t*] (out) minimum vertical coordinate
 * @param x1 [type: int32_t*] (out) maximum horizontal coordinate
 * @param y1 [type: int32_t*] (out) maximum vertical coordinate
 * @return has_bounds [type: bool] false for an empty glyph
 */
bool FontTrueTypeGetGlyphBox(FontTrueType* font, uint32_t glyph_index, int32_t* x0, int32_t* y0, int32_t* x1, int32_t* y1);

#endif // DM_FONT_TRUETYPE_H
