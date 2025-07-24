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
     * @path engine/gamesys/src/dmsdk/gamesys/resource/res_font.h
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
     * @name ResFontGetHandle
     * @param font [type: FontResource*] The font resource
     * @return result [type: dmRender::HFontMap] Handle to a font if successful. 0 otherwise.
     */
    dmRender::HFontMap ResFontGetHandle(FontResource* font);

    /*#
     * @name ResFontGetTTFResourceFromCodepoint
     * @param font [type: FontResource*] The font resource
     * @param codepoint [type: uint32_t] The codepoint to query
     * @return ttfresource [type: TTFResource*] The ttfresource if successful. 0 otherwise.
     */
    TTFResource* ResFontGetTTFResourceFromCodepoint(FontResource* resource, uint32_t codepoint);

    /*#
     * @name ResFontGetTTFResourceFromCodepoint
     * @param font [type: FontResource*] The font resource
     * @param codepoint [type: uint32_t] The codepoint to query
     * @return ttfresource [type: TTFResource*] The ttfresource if successful. 0 otherwise.
     */
    TTFResource* ResFontGetTTFResourceFromGlyphIndex(FontResource* resource, uint32_t glyph_index);

    /*#
     * @name ResFontGetInfo
     * @param font [type: FontResource*] The font resource to query
     * @param info [type: FontInfo*] The output info
     * @return result [type: dmResource::Result] RESULT_OK if successful
     */
    dmResource::Result ResFontGetInfo(FontResource* font, FontInfo* info);

    // FontGen API

    /*#
     * @name ResFontAddGlyph
     * @param font [type: FontResource*] The font resource
     * @param hfont [type: HFont] The font the glyh was created from
     * @param glyph [type: FontGlyph*] The glyph
     * @return result [type: dmResource::Result] RESULT_OK if successful
     */
    dmResource::Result ResFontAddGlyph(FontResource* font, HFont hfont, FontGlyph* glyph);

    /*#
     * @name ResFontRemoveGlyph
     * @param font [type: FontResource*] The font resource
     * @param hfont [type: HFont] The font the glyh was created from
     * @param glyph_index [type: uint32_t] The glyph index
     * @return result [type: dmResource::Result] RESULT_OK if successful
     */
    dmResource::Result ResFontRemoveGlyph(FontResource* font, HFont hfont, uint32_t glyph_index);

    /*# add a new glyph range
     * Add a new glyph range
     * @note Does not check if
     * @name ResFontAddGlyphSource
     * @param factory [type: dmResource::HFactory] The factory
     * @param font_hash [type: dmhash_t] The font path hash (.fontc)
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
     * @param font_hash [type: dmhash_t] The font path hash (.fontc)
     * @param ttf_hash [type: dmhash_t] The ttf  path hash (.ttf)
     * @return result [type: dmResource::Result] RESULT_OK if successful
     */
    dmResource::Result ResFontRemoveGlyphSource(dmResource::HFactory factory, dmhash_t fontc_hash, dmhash_t ttf_hash);
}

#endif // DMSDK_GAMESYS_RES_FONT_H
