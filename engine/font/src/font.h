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

#if !defined(DM_FONT_H)
#define DM_FONT_H

#include <stdint.h>

namespace dmFont
{
    enum FontResult
    {
        RESULT_OK,
        RESULT_ERROR,
    };

    enum FontType
    {
        FONT_TYPE_UNKNOWN = -1,
        FONT_TYPE_STBTTF,
        FONT_TYPE_MAX
    };

    enum GlyphBitmapFlags
    {
        GLYPH_BM_FLAG_NONE = 0,
        GLYPH_BM_FLAG_RLE_COMRESSION = 1,
    };

    /*#
     * Holds the bitmap data of a glyph.
     * If there's an associated image, it is of size width * height * channels.
     * @struct
     * @name GlyphBitmap
     * @member m_Width [type: uint16_t] The glyph image width
     * @member m_Height [type: uint16_t] The glyph image height
     * @member m_Channels [type: uint16_t] The glyph image height
     * @member m_Flags [type: uint8_t] Flags describing the data. 0 == no compression, 1 == RLE compression
     * @member m_Data [type: uint8_t*] The bitmap data, or null if no data available.
     */
    struct GlyphBitmap
    {
        uint16_t    m_Width;
        uint16_t    m_Height;
        uint8_t     m_Channels;
        uint8_t     m_Flags;
        uint8_t*    m_Data;
    };

    /*#
     * Represents a glyph.
     * If there's an associated image, it is of size width * height * channels.
     * @struct
     * @name Glyph
     * @member m_Width [type: float] The glyph bounding width
     * @member m_Height [type: float] The glyph bounding height
     * @member m_Advance [type: float] The advance step of the glyph (in pixels)
     * @member m_LeftBearing [type: float] The left bearing of the glyph (in pixels)
     * @member m_Ascent [type: float] The ascent of the glyph. (in pixels)
     * @member m_Descent [type: float] The descent of the glyph. Positive! (in pixels)
     * @member m_Bitmap [type: GlyphBitmap] The bitmap data of the glyph.
     */
    struct Glyph
    {
        GlyphBitmap m_Bitmap;
        uint32_t    m_Codepoint; // Unicode code point
        float       m_Width;
        float       m_Height;
        float       m_Advance;
        float       m_LeftBearing;
        float       m_Ascent;
        float       m_Descent;
    };

    typedef struct Font* HFont;

    // ****************************************************************
    // Fonts
    HFont LoadFontFromPath(const char* path);
    HFont LoadFontFromMemory(const char* name, void* data, uint32_t data_size);

    void  DestroyFont(HFont font);

    FontType GetType(HFont font);
    const char* GetPath(HFont font);

    float GetPixelScaleFromSize(HFont font, float size);
    float GetAscent(HFont font, float scale);
    float GetDescent(HFont font, float scale);
    float GetLineGap(HFont font, float scale);

    // For reporting at runtime. It returns the size of the font (in bytes)
    uint32_t   GetResourceSize(HFont font);

    // ****************************************************************
    // Glyphs

    struct GlyphOptions
    {
        float m_Scale; // Point to Size scale
        bool  m_GenerateImage;

        // stbtt options (see stbtt_GetGlyphSDF)
        int   m_StbttPadding;
        int   m_StbttOnEdgeValue;
    };

    FontResult GetGlyph(HFont font, uint32_t codepoint, GlyphOptions* options, Glyph* glyph);
    // In case a bit map was allocated, and you wish to free the memory
    FontResult FreeGlyph(HFont font, Glyph* glyph);

    void DebugFont(HFont font, float scale, const char* text);

} // namespace

#endif // DM_FONT_H
