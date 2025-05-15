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
#include <dmsdk/gamesys/resources/res_font.h>
#include <render/font_ddf.h>
#include <dmsdk/render/render.h>

namespace dmGameSystem
{
    /*#
     * Handle to font resource
     * @struct
     * @name FontResource
     */
    struct FontResource;

    /*#
     * Used to retrieve the information of a font.
     * @typedef
     * @name FontInfo
     * @member m_Size [type: uint32_t]
     * @member m_Antialias [type: uint32_t]
     * @member m_ShadowX [type: float]
     * @member m_ShadowY [type: float]
     * @member m_ShadowBlur [type: uint32_t]
     * @member m_ShadowAlpha [type: float]
     * @member m_Alpha [type: float]
     * @member m_OutlineAlpha [type: float]
     * @member m_OutlineWidth [type: float]
     */
    typedef dmRenderDDF::FontMap FontInfo;

    /*#
     * Represents a glyph.
     * If there's an associated image, it is of size width * height * channels.
     * @struct
     * @name FontGlyph
     * @member m_Width [type: int16_t] The glyph image width
     * @member m_Height [type: int16_t] The glyph image height
     * @member m_Channels [type: int16_t] The glyph image height
     * @member m_Advance [type: float] The advance step of the glyph (in pixels)
     * @member m_LeftBearing [type: float] The left bearing of the glyph (in pixels)
     * @member m_Ascent [type: float] The ascent of the glyph. (in pixels)
     * @member m_Descent [type: float] The descent of the glyph. Positive! (in pixels)
     */
    struct FontGlyph
    {
        int16_t m_Width;
        int16_t m_Height;
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
        FONT_GLYPH_COMPRESSION_NONE = 0,
        FONT_GLYPH_COMPRESSION_DEFLATE = 1,
    };

    /*#
     * @name ResFontGetHandle
     * @param font [type: FontResource*] The font resource to modify
     * @return result [type: dmRender::HFont] Handle to a font if successful. 0 otherwise.
     */
    dmRender::HFont ResFontGetHandle(FontResource* font);

    /*#
     * @name ResFontGetInfo
     * @param font [type: FontResource*] The font resource to modify
     * @param info [type: FontInfo*] The output info
     * @return result [type: dmResource::Result] RESULT_OK if successful
     */
    dmResource::Result ResFontGetInfo(FontResource* font, FontInfo* info);

    /*#
     * Set the font line height, by specifying the max ascent and descent
     * @name ResFontSetLineHeight
     * @param font [type: FontResource*] The font resource to modify
     * @param max_ascent [type: float] The max distance above the base line of any glyph
     * @param max_descent [type: float] The max distance below the base line of any glyph
     * @return result [type: dmResource::Result] RESULT_OK if successful
     */
    dmResource::Result ResFontSetLineHeight(FontResource* font, float max_ascent, float max_descent);

    /*#
     * Get the font line height (max_ascent + max_descent)
     * @name ResFontGetLineHeight
     * @param font [type: FontResource*] The font resource to modify
     * @param max_ascent [type: float*] The max distance above the base line of any glyph
     * @param max_descent [type: float*] The max distance below the base line of any glyph
     * @return result [type: dmResource::Result] RESULT_OK if successful
     */
    dmResource::Result ResFontGetLineHeight(FontResource* font, float* max_ascent, float* max_descent);

    /*#
     * Resets the glyph cache and sets the cell size.
     * @name ResFontSetCacheCellSize
     * @param font [type: FontResource*] The font resource to modify
     * @param cell_width [type: uint32_t] The width of a glyph cache cell
     * @param cell_height [type: uint32_t] The height of a glyph cache cell
     * @param max_ascent [type: uint32_t] The height of a glyph cache cell
     * @return result [type: dmResource::Result] RESULT_OK if successful
     */
    dmResource::Result ResFontSetCacheCellSize(FontResource* font, uint32_t cell_width, uint32_t cell_height, uint32_t max_ascent);

    /*#
     * @name ResFontGetCacheCellSize
     * @param font [type: FontResource*] The font resource to modify
     * @param width [type: uint32_t*] The cache cell width
     * @param height [type: uint32_t*] The cache cell height
     * @param max_ascent [type: uint32_t*] The distance from the top of the cell to the baseline.
     * @return result [type: dmResource::Result] RESULT_OK if successful
     */
    dmResource::Result ResFontGetCacheCellSize(FontResource* font, uint32_t* width, uint32_t* height, uint32_t* max_ascent);

    /*#
     * @name ResFontHasGlyph
     * @param font [type: FontResource*] The font resource
     * @param codepoint [type: uint32_t] The glyph codepoint
     * @return result [type: bool] true if the glyph already exists
     */
    bool ResFontHasGlyph(FontResource* font, uint32_t codepoint);

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

    void   ResFontDebugPrint(FontResource* font);
}

#endif // DMSDK_GAMESYS_RES_FONT_H
