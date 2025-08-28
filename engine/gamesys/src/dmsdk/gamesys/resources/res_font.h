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

#ifndef DMSDK_GAMESYS_RES_FONT_H
#define DMSDK_GAMESYS_RES_FONT_H

#include <dmsdk/resource/resource.h>
#include <render/font_ddf.h>
#include <dmsdk/font/font.h>
#include <dmsdk/render/render.h>

namespace dmGameSystem
{
    /*# Font resource functions
     *
     * Font resource functions.
     *
     * @document
     * @namespace dmGameSystem
     * @name Font Resource
     * @language C++
     */

    /*#
     * The edge value of an sdf glyph bitmap
     * @constant
     * @name SDF_EDGE_VALUE
     */
    const float SDF_EDGE_VALUE = 0.75f; // This is a fixed constant in the current font renderer

    /*#
     * Handle to font resource
     * @struct
     * @name FontResource
     */
    struct FontResource;

    struct TTFResource;

    /*#
     * Used to retrieve the information of a font.
     * @struct
     * @name FontInfo
     * @member m_Size [type: uint32_t] The size of the font (in points)
     * @member m_ShadowX [type: float] The shadow distance in X-axis (in pixels)
     * @member m_ShadowY [type: float] The shadow distance in Y-axis (in pixels)
     * @member m_ShadowBlur [type: uint32_t] The shadow blur spread [0.255] (in pixels)
     * @member m_ShadowAlpha [type: float] The shadow alpha value [0..255]
     * @member m_Alpha [type: float] The alpha value [0..255]
     * @member m_OutlineAlpha [type: float] The outline alpha value [0..255]
     * @member m_OutlineWidth [type: float] The outline size (in pixels)
     * @member m_OutputFormat [type: dmRenderDDF::FontTextureFormat] The type of font (bitmap or distance field)
     * @member m_RenderMode [type: dmRenderDDF::FontRenderMode] Single or multi channel
     */
    struct FontInfo
    {
        uint32_t   m_Size;
        float      m_ShadowX;
        float      m_ShadowY;
        uint32_t   m_ShadowBlur;
        float      m_ShadowAlpha;
        float      m_Alpha;
        float      m_OutlineAlpha;
        float      m_OutlineWidth;
        dmRenderDDF::FontTextureFormat m_OutputFormat;
        dmRenderDDF::FontRenderMode m_RenderMode;
    };

    /*#
     * Represents a glyph.
     * If there's an associated image, it is of size width * height * channels.
     * @struct
     * @name FontGlyph
     * @member m_Width [type: float] The glyph bounding width
     * @member m_Height [type: float] The glyph bounding height
     * @member m_ImageWidth [type: int16_t] The glyph image width
     * @member m_ImageHeight [type: int16_t] The glyph image height
     * @member m_Channels [type: int16_t] The glyph image height
     * @member m_Advance [type: float] The advance step of the glyph (in pixels)
     * @member m_LeftBearing [type: float] The left bearing of the glyph (in pixels)
     * @member m_Ascent [type: float] The ascent of the glyph. (in pixels)
     * @member m_Descent [type: float] The descent of the glyph. Positive! (in pixels)
     */
    struct FontGlyph
    {
        float   m_Width;
        float   m_Height;
        int16_t m_ImageWidth;
        int16_t m_ImageHeight;
        int16_t m_Channels;
        float   m_Advance;
        float   m_LeftBearing;
        float   m_Ascent;
        float   m_Descent;
    };

    /*#
     * Describes what compression is used for the glyph image
     * @enum
     * @name FontGlyphCompression
     * @member FONT_GLYPH_COMPRESSION_NONE      No compression
     * @member FONT_GLYPH_COMPRESSION_DEFLATE   Data is compressed using the deflate() algorithm
     */
    enum FontGlyphCompression
    {
        FONT_GLYPH_COMPRESSION_NONE = dmFont::GLYPH_BM_FLAG_COMPRESSION_NONE,
        FONT_GLYPH_COMPRESSION_DEFLATE = dmFont::GLYPH_BM_FLAG_COMPRESSION_DEFLATE,
    };

    static const uint32_t WHITESPACE_TAB               = 0x09;      // '\t'
    static const uint32_t WHITESPACE_NEW_LINE          = 0x0A;      // '\n'
    static const uint32_t WHITESPACE_CARRIAGE_RETURN   = 0x0D;      // '\r'
    static const uint32_t WHITESPACE_SPACE             = 0x20;      // ' '
    static const uint32_t WHITESPACE_ZERO_WIDTH_SPACE  = 0x200b;
    static const uint32_t WHITESPACE_NO_BREAK_SPACE    = 0x00a0;
    static const uint32_t WHITESPACE_IDEOGRAPHIC_SPACE = 0x3000;

    /*#
     * Checks if a codepoint is a whitespace
     * @name IsWhiteSpace
     * @param c [type: uint32_t] the codepoint
     * @return result [type: bool] true if it's a whitespace
     */
    inline bool IsWhiteSpace(uint32_t c)
    {
        return c == WHITESPACE_SPACE ||
               c == WHITESPACE_NEW_LINE ||
               c == WHITESPACE_ZERO_WIDTH_SPACE ||
               c == WHITESPACE_NO_BREAK_SPACE ||
               c == WHITESPACE_IDEOGRAPHIC_SPACE ||
               c == WHITESPACE_TAB ||
               c == WHITESPACE_CARRIAGE_RETURN;
    }

    /*#
     * @name ResFontGetHandle
     * @param font [type: FontResource*] The font resource
     * @return result [type: dmRender::HFont] Handle to a font if successful. 0 otherwise.
     */
    dmRender::HFont ResFontGetHandle(FontResource* font);

    /*#
     * @name ResFontGetTTFResourceFromCodepoint
     * @param font [type: FontResource*] The font resource
     * @param codepoint [type: uint32_t] The codepoint to query
     * @return ttfresource [type: TTFResource*] The ttfresource if successful. 0 otherwise.
     */
    TTFResource* ResFontGetTTFResourceFromCodepoint(FontResource* resource, uint32_t codepoint);

    /*#
     * @name ResFontGetInfo
     * @param font [type: FontResource*] The font resource to modify
     * @param info [type: FontInfo*] The output info
     * @return result [type: dmResource::Result] RESULT_OK if successful
     */
    dmResource::Result ResFontGetInfo(FontResource* font, FontInfo* info);

    // FontGen API

    /*#
     * @name ResFontAddGlyph
     * @param font [type: FontResource*] The font resource to modify
     * @param codepoint [type: uint32_t] The glyph codepoint
     * @param glyph [type: FontGlyph*] The glyph meta data
     * @param imagedata [type: void*] The bitmap or sdf data. May be null for e.g. white space characters. The font will now own this data.
     * @return result [type: dmResource::Result] RESULT_OK if successful
     */
    dmResource::Result ResFontAddGlyph(FontResource* font, uint32_t codepoint, FontGlyph* glyph, void* imagedata, uint32_t imagedatasize);

    /*#
     * @name ResFontRemoveGlyph
     * @param font [type: FontResource*] The font resource
     * @param codepoint [type: uint32_t] The glyph codepoint
     * @return result [type: dmResource::Result] RESULT_OK if successful
     */
    dmResource::Result ResFontRemoveGlyph(FontResource* font, uint32_t codepoint);

    /*# add a new glyph range
     * Add a new glyph range
     * @name ResFontAddGlyphSource
     * @param factory [type: dmResource::HFactory] The factory
     * @param fontc_hash [type: dmhash_t] The font path hash (.fontc)
     * @param ttf_hash [type: dmhash_t] The ttf  path hash (.ttf)
     * @param codepoint_min [type: uint32_t] The glyph minimum codepoint (inclusive)
     * @param codepoint_max [type: uint32_t] The glyph maximum codepoint (inclusive)
     * @return result [type: dmResource::Result] RESULT_OK if successful
     */
    dmResource::Result ResFontAddGlyphSource(dmResource::HFactory factory, dmhash_t fontc_hash, dmhash_t ttf_hash, uint32_t codepoint_min, uint32_t codepoint_max);


    /*# removes all glyph ranges associated with a ttfresource
     * Removes all glyph ranges associated with a ttfresource
     *
     * @name ResFontRemoveGlyphSource
     * @param factory [type: dmResource::HFactory] The factory
     * @param fontc_hash [type: dmhash_t] The font path hash (.fontc)
     * @param ttf_hash [type: dmhash_t] The ttf  path hash (.ttf)
     * @return result [type: dmResource::Result] RESULT_OK if successful
     */
    dmResource::Result ResFontRemoveGlyphSource(dmResource::HFactory factory, dmhash_t fontc_hash, dmhash_t ttf_hash);
}

#endif // DMSDK_GAMESYS_RES_FONT_H
