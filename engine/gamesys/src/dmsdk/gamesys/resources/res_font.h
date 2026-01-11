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

#ifndef DMSDK_GAMESYS_RES_FONT_H
#define DMSDK_GAMESYS_RES_FONT_H

#include <render/font_ddf.h>
#include <dmsdk/resource/resource.h>
#include <dmsdk/font/font.h>
#include <dmsdk/font/fontcollection.h>
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
     * @name ResFontGetHandle
     * @param font [type: FontResource*] The font resource
     * @return result [type: dmRender::HFontMap] Handle to a font if successful. 0 otherwise.
     */
    dmRender::HFontMap ResFontGetHandle(FontResource* font);

    /*#
     * @name FPrewarmTextCallback
     * @typedef
     * @param ctx [type: void*] The callback context
     * @param result [type: int] The result of the prewarming. Non zero if successful
     * @param errmsg [type: const char*] An error message if not successful.
     */
    typedef void (*FPrewarmTextCallback)(void* ctx, int result, const char* errmsg);

    /*# Make sure each glyph in the text gets rasterized and put into the glyph cache
     * @name PrewarmText
     * @param font [type: FontResource*] The font resource
     * @param text [type: const char*] The text (utf8)
     * @param cbk [type: FPrewarmTextCallback] The callback is called when the last item is done
     * @param cbk_ctx [type: void*] The callback context
     * @return result [type: dmResource::Result] RESULT_OK if successful
     */
    dmResource::Result ResFontPrewarmText(FontResource* font, const char* text, FPrewarmTextCallback cbk, void* cbk_ctx);

    /*#
     * @name ResFontGetTTFResourceFromFont
     * @param resource [type: FontResource*] The font resource
     * @param font [type: HFont] The font
     * @return ttfresource [type: TTFResource*] The ttfresource if successful. 0 otherwise.
     */
    TTFResource* ResFontGetTTFResourceFromFont(FontResource* resource, HFont font);

    /*#
     * @name ResFontGetPathHashFromFont
     * @param resource [type: FontResource*] The font resource
     * @param font [type: HFont] The font
     * @return path_hash [type: dmhash_t] The path hash to the associated TTFresource*
     */
    dmhash_t ResFontGetPathHashFromFont(FontResource* resource, HFont font);

    /*#
     * @name ResFontGetFontCollection
     * @param resource [type: FontResource*] The font resource
     * @return font_collection [type: HFontCollection*] The font collection if successful. 0 otherwise.
     */
    HFontCollection ResFontGetFontCollection(FontResource* resource);

    // FontGen API

    /*#
     * @name ResFontGetInfo
     * @param font [type: FontResource*] The font resource to query
     * @param info [type: FontInfo*] The output info (out)
     * @return result [type: dmResource::Result] RESULT_OK if successful
     */
    dmResource::Result ResFontGetInfo(FontResource* font, FontInfo* info);

    /*#
     * @name ResFontAddGlyph
     * @param font [type: FontResource*] The font resource
     * @param hfont [type: HFont] The font the glyph was created from
     * @param glyph_index [type: uint32_t] The glyph index
     * @return result [type: bool] true if the glyph already has rasterized bitmap data
     */
    bool ResFontIsGlyphIndexCached(FontResource* font, HFont hfont, uint32_t glyph_index);

    /*#
     * @name ResFontAddGlyph
     * @param font [type: FontResource*] The font resource
     * @param hfont [type: HFont] The font the glyph was created from
     * @param glyph [type: FontGlyph*] The glyph
     * @return result [type: dmResource::Result] RESULT_OK if successful
     */
    dmResource::Result ResFontAddGlyph(FontResource* font, HFont hfont, FontGlyph* glyph);

    /*# add a ttf font to a font collection
     * @note Loads the resource if not already loaded
     * @name ResFontAddFontByPath
     * @param factory [type: dmResource::HFactory] The factory
     * @param font [type: FontResource*] The font collection (.fontc)
     * @param ttf_path [type: const char*] The .ttf path
     * @return result [type: dmResource::Result] RESULT_OK if successful
     */
    dmResource::Result ResFontAddFontByPath(dmResource::HFactory factory, FontResource* font, const char* ttf_path);

    /*# add a ttf font to a font collection
     * @note the ttf resource must already be loaded
     * @name ResFontAddFontByPathHash
     * @param factory [type: dmResource::HFactory] The factory
     * @param font [type: FontResource*] The font collection (.fontc)
     * @param ttf_hash [type: dmhash_t] The ttf path hash (.ttf)
     * @return result [type: dmResource::Result] RESULT_OK if successful
     */
    dmResource::Result ResFontAddFontByPathHash(dmResource::HFactory factory, FontResource* font, dmhash_t ttf_hash);

    /*# remove a ttf font from a font collection
     * @name ResFontRemoveFont
     * @param factory [type: dmResource::HFactory] The factory
     * @param font [type: FontResource*] The font collection (.fontc)
     * @param ttf_hash [type: dmhash_t] The ttf path hash (.ttf)
     * @return result [type: dmResource::Result] RESULT_OK if successful
     */
    dmResource::Result ResFontRemoveFont(dmResource::HFactory factory, FontResource* font, dmhash_t ttf_hash);
}

#endif // DMSDK_GAMESYS_RES_FONT_H
