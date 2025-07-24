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

#ifndef DM_FONT_DEFAULT_PRIVATE_H
#define DM_FONT_DEFAULT_PRIVATE_H

#include <stdint.h>
#include <dmsdk/dlib/array.h>

typedef struct Font* HFont;

// Common api for the default renderers


struct TextShapeGlyph
{
    int32_t  m_X; // X position on the infinite line (in points)
    int32_t  m_Y; // Y position on the infinite line (in points)

    uint32_t m_Codepoint;  // Not always available if there was a substitution
    uint16_t m_GlyphIndex; // index into the font
    int16_t  :16;

    int16_t  m_Width;
    int16_t  m_Height;

    int16_t  m_Advance;
    int16_t  m_LeftBearing;
};

struct TextRun
{
    uint16_t m_Index;   /// index into list of glyphs
    uint16_t m_Length;  /// number of glyphs to render

    // if needed: script, direction, language
};

struct TextLine
{
    int32_t  m_Width;   /// Width of the line (in points)
    uint16_t m_Index;   /// Index into the list of glyphs
    uint16_t m_Length;  /// Number of glyphs to render
};

struct TextShapeInfo
{
    // TODO: Make these C arrays?
    dmArray<TextShapeGlyph> m_Glyphs;
    dmArray<TextRun>        m_Runs;
    uint32_t                m_NumValidGlyphs;   // non renderable glyphs
    HFont                   m_Font;
};

struct TextMetricsSettings
{
    int32_t     m_Width;        /// Max width. used only when line_break is true. (in points)
    float       m_Leading;      /// leading scale value (1.0f is default scale)
    int32_t     m_Tracking;     /// (in points)
    uint32_t    m_Padding;      /// Legacy: Padding for monospace, glyphbank fonts
    uint8_t     m_LineBreak:1;  /// Allow line breaks
    uint8_t     m_Monospace:1;  /// Legacy: Is the font a monospace font
};

struct TextMetrics
{
    int32_t  m_Width;       /// Total string width (in points)
    int32_t  m_Height;      /// Total string height (in points)
    uint32_t m_MaxAscent;   /// Max ascent of font (in points)
    uint32_t m_MaxDescent;  /// Max descent of font, positive value. (in points)
    uint32_t m_LineCount;   /// Number of lines of text
};


enum TextShapeResult
{
    TEXT_SHAPE_RESULT_OK,
    TEXT_SHAPE_RESULT_ERROR,
};

// Shapes text into final glyphs and positions
TextShapeResult TextShapeText(HFont font,
                                uint32_t* codepoints, uint32_t num_codepoints,
                                TextShapeInfo* info);

// Calculates the lines from a shaped text
TextShapeResult TextLayout(TextMetricsSettings* settings, TextShapeInfo* info,
                            TextLine* lines, uint32_t num_lines,
                            TextMetrics* metrics);

// Calculates the bounding box and other metrics of a text (calls TextShapeText and TextLayout)
TextShapeResult TextGetMetrics(HFont font,
                                uint32_t* codepoints, uint32_t num_codepoints,
                                TextMetricsSettings* settings, TextMetrics* metrics);



#endif // #ifndef DM_FONT_DEFAULT_PRIVATE_H
