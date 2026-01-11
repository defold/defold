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

#ifndef DMSDK_TEXT_LAYOUT_H
#define DMSDK_TEXT_LAYOUT_H

#include <stdint.h>

typedef struct Font* HFont;
typedef struct FontCollection* HFontCollection;

/*# SDK Text Layout API documentation
 *
 * API for laying out complex text into format ready for display
 *
 * @document
 * @name TextLayout
 * @path engine/font/src/dmsdk/font/text_layout.h
 * @language C++
 */

/*#
 * A handle representing a text layout
 * @typedef
 * @name HTextLayout
 */
typedef struct TextLayout* HTextLayout;

/*#
 * An enum representing text layout results
 * @enum
 * @name TextResult
 * @member TEXT_RESULT_OK
 * @member TEXT_RESULT_ERROR
 */
enum TextResult
{
    TEXT_RESULT_OK,
    TEXT_RESULT_ERROR,
};

/*#
 * An enum representing text layout directions
 * @enum
 * @name TextDirection
 * @member TEXT_DIRECTION_LTR   Left-to-right text direction
 * @member TEXT_DIRECTION_RTL   Right-to-left text direction
 */
enum TextDirection
{
    TEXT_DIRECTION_LTR = 0,
    TEXT_DIRECTION_RTL = 1,
};

/*#
 * An enum representing text layout features
 * Each font supports a layout type
 * The selected layout type it the minimum value of layout types
 * @enum
 * @name TextLayoutType
 * @member TEXT_LAYOUT_TYPE_LEGACY Legacy text shaping api
 * @member TEXT_LAYOUT_TYPE_FULL   Full text shaping api
 */
enum TextLayoutType
{
    TEXT_LAYOUT_TYPE_LEGACY     = 0,
    TEXT_LAYOUT_TYPE_FULL       = 1,
};

/*#
 * Glyph representing the final position within a layout
 * @struct
 * @name TextGlyph
 * @member m_Font [type: HFont] The font used for this glyph
 * @member m_X [type: float] the final x position, relative the top-left corner of the layout
 * @member m_Y [type: float] the final y position, relative the top-left corner of the layout
 * @member m_Width [type: float] the width of the glyph
 * @member m_Height [type: float] the height of the glyph
 * @member m_Codepoint [type: uint32_t] original copdepoint (if available)
 * @member m_GlyphIndex [type: uint16_t] the glyph index in the font
 * @member m_Cluster [type: uint16_t] the index in the original text, that this glyph corresponds to
 */
struct TextGlyph
{
    // The font is needed for actually using the glyph index (i.e. rasterizing the glyph bitmap)
    HFont       m_Font;

    float       m_X;            // X position inside the layout
    float       m_Y;            // Y position inside the layout
    // The bounding box is used to calculate the space required in the glyph cache texture
    float       m_Width;        // width of the glyph bounding box
    float       m_Height;       // height of the glyph bounding box

    uint32_t    m_Codepoint;    // Not always available if there was a substitution
    uint16_t    m_GlyphIndex;   // index into the font
    uint16_t    m_Cluster;      // index into the original text (i.e. into codepoints)

    // private
    float  m_Advance;     // LEGACY SHAPING ONLY! TODO: See if we can remove these
    float  m_LeftBearing; // LEGACY SHAPING ONLY!
};

/*#
 * Represents a line of glyphs
 * @struct
 * @name TextLine
 * @member m_Width [type: float] Width of the line
 * @member m_Index [type: uint16_t] Index into the list of glyphs
 * @member m_Length [type: uint16_t] Number of glyphs to render
 */
struct TextLine
{
    float    m_Width;
    uint16_t m_Index;
    uint16_t m_Length;
};

/*#
 * Describes how to do a text layout
 * @struct
 * @name TextLayoutSettings
 * @member m_Size [type: float] The desired size of the font (in pixels)
 * @member m_Width [type: float] Max layout width. Used only when m_LineBreak is non-zero
 * @member m_Leading [type: float] The extra space between each line. Set 1.0f as default.
 * @member m_Tracking [type: float] The extra tracking between glyphs. Set 0 as default.
 * @member m_Padding [type: uint32_t] Legacy: Padding for monospace, glyphbank fonts
 * @member m_LineBreak [type: uint8_t:1] Allow line breaks
 * @member m_Monospace [type: uint8_t:1] Legacy: Is the font a monospace font. Current: should be set on the font in the font collection!
 */
struct TextLayoutSettings
{
    float     m_Size;
    float     m_Width;
    float     m_Leading;
    float     m_Tracking;

    uint32_t    m_Padding;
    uint8_t     m_LineBreak:1;
    uint8_t     m_Monospace:1;
};


/*#
 * Create a text layout using a font collection
 * if successful, the caller must call TextLayoutFree() on the layout
 * @name TextLayoutCreate
 * @param collection [type: HFontCollection] the font collection
 * @param codepoints [type: uint32_t*] an array of codepoints
 * @param num_codepoints [type: uint32_t] number of codepoints in the array
 * @param settings [type: TextLayoutSettings*] the settings used for rendering
 * @param layout [type: HTextLayout*] (out) the output text layout
 * @return result [type: TextResult] the result. TEXT_RESULT_OK if successful
 */
TextResult TextLayoutCreate(HFontCollection collection,
                            uint32_t* codepoints, uint32_t num_codepoints,
                            TextLayoutSettings* settings, HTextLayout* layout);

/*#
 * Frees a previously created layout
 * @name TextLayoutFree
 * @param layout [type: HTextLayout] the text layout
 */
void TextLayoutFree(HTextLayout layout);

/*#
 * Get the glyph count in the layout
 * @name TextLayoutGetGlyphCount
 * @param layout [type: HTextLayout] the text layout
 * @return count [type: uint32_t] the number of glyphs in the layout
 */
uint32_t TextLayoutGetGlyphCount(HTextLayout layout);

/*#
 * Get the glyphs in the layout
 * @name TextLayoutGetGlyphs
 * @param layout [type: HTextLayout] the text layout
 * @return glyphs [type: TextGlyph*] the array of glyphs in the layout
 */
TextGlyph* TextLayoutGetGlyphs(HTextLayout layout);

/*#
 * Get the line count in the layout
 * @name TextLayoutGetLineCount
 * @param layout [type: HTextLayout] the text layout
 * @return count [type: uint32_t] the number of lines in the layout
 */
uint32_t TextLayoutGetLineCount(HTextLayout layout);

/*#
 * Get the lines in the layout
 * @name TextLayoutGetLines
 * @param layout [type: HTextLayout] the text layout
 * @return lines [type: TextLine*] the array of lines in the layout
 */
TextLine* TextLayoutGetLines(HTextLayout layout);

/*#
 * Get the lines in the layout
 * @name TextLayoutGetBounds
 * @param layout [type: HTextLayout] the text layout
 * @return width [type: float*] the total width of the layout (out)
 * @return height [type: float*] the total height of the layout (out)
 */
void TextLayoutGetBounds(HTextLayout layout, float* width, float* height);

#endif // DMSDK_TEXT_LAYOUT_H
