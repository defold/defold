// Copyright 2020-2024 The Defold Foundation
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
    struct FontResource;
    typedef dmRenderDDF::FontMap FontMapDesc;

    struct FontGlyph
    {
        uint32_t  m_Width;      // Bitmap width
        uint32_t  m_Height;     // Bitmap height
        float     m_Advance;
        float     m_LeftBearing;
        float     m_Ascent;     // pixels above the base line
        float     m_Descent;    // pixels below the base line
    };

    enum FontGlyphCompression
    {
        FONT_GLYPH_COMPRESSION_NONE = 0,
        FONT_GLYPH_COMPRESSION_DEFLATE = 1,
    };

    dmRender::HFont ResFontGetHandle(FontResource* font);

    /*#
     * @name ResFontGetInfo
     * @param font [type: FontResource*] The font resource to modify
     * @param desc [type: FontMapDesc*] The output info
     * @return result [type: dmResource::Result] RESULT_OK if successful
     */
    dmResource::Result ResFontGetInfo(FontResource* font, FontMapDesc* desc);

    /*#
     * @name ResFontGetCacheCellInfo
     * @param font [type: FontResource*] The font resource to modify
     * @param width [type: uint32_t*] The cache cell width
     * @param height [type: uint32_t*] The cache cell height
     * @param max_ascent [type: uint32_t*] The distance from the top of the cell to the baseline.
     * @return result [type: dmResource::Result] RESULT_OK if successful
     */
    dmResource::Result ResFontGetCacheCellInfo(FontResource* font, uint32_t* width, uint32_t* height, uint32_t* max_ascent);

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
     * @param imagedata [type: void*] The bitmap or sdf data
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
