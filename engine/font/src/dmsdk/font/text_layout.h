// Copyright 2020-2025 The Defold Foundation
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
#include <dmsdk/font/fontcollection.h>

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
 */
enum TextDirection
{
    TEXT_DIRECTION_LTR = 0,
    TEXT_DIRECTION_RTL = 1,
};

/*#
 * Glyph representing the final position within a layout
 * @struct
 * @name TextGlyph
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


    // TODO: See if we can remove these
    float  m_Advance;     // LEGACY SHAPING ONLY!
    float  m_LeftBearing; // LEGACY SHAPING ONLY!
};

/*#
 * Represents a line of glyphs
 * @struct
 * @name TextLine
 * @member m_Width
 * @member m_Index
 * @member m_Length
 */
struct TextLine
{
    float    m_Width;   /// Width of the line
    uint16_t m_Index;   /// Index into the list of glyphs
    uint16_t m_Length;  /// Number of glyphs to render
};

/*#
 * Represents a line of glyphs
 * @struct
 * @name TextLine
 * @member m_Width
 * @member m_Index
 * @member m_Length
 */
struct TextLayoutSettings
{
    float     m_Size;           /// The desired size of the font
    float     m_Width;          /// Max layout width. used only when line_break is true
    float     m_Leading;        ///
    float     m_Tracking;       ///

    uint32_t    m_Padding;      /// Legacy: Padding for monospace, glyphbank fonts
    uint8_t     m_LineBreak:1;  /// Allow line breaks
    uint8_t     m_Monospace:1;  /// Legacy: Is the font a monospace font. Current: should be set on the font in the font collection!
};


/*#
 *
 */
TextResult TextLayoutCreate(HFontCollection collection,
                            uint32_t* codepoints, uint32_t num_codepoints,
                            TextLayoutSettings* settings, HTextLayout* layout);

void TextLayoutFree(HTextLayout layout);


uint32_t    TextLayoutGetGlyphCount(HTextLayout layout);
TextGlyph*  TextLayoutGetGlyphs(HTextLayout layout);
uint32_t    TextLayoutGetLineCount(HTextLayout layout);
TextLine*   TextLayoutGetLines(HTextLayout layout);
void        TextLayoutGetBounds(HTextLayout layout, float* width, float* height);

#endif // DMSDK_TEXT_LAYOUT_H
