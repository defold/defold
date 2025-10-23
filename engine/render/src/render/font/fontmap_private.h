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

#ifndef DM_FONTMAP_PRIVATE_H
#define DM_FONTMAP_PRIVATE_H

#include "font_renderer.h"
#include "../render_private.h"

namespace dmRender
{
    struct FontMap
    {
        FontMap()
        {
            memset(this, 0, sizeof(*this));
        }

        ~FontMap()
        {
            free(m_CacheIndices);
            m_CacheIndices = 0;

            free(m_Cache);
            m_Cache = 0;

            free(m_CellTempData);
            m_CellTempData = 0;

            if (m_Texture)
                dmGraphics::DeleteTexture(m_GraphicsContext, m_Texture);
        }

        dmMutex::HMutex         m_Mutex;
        HFontCollection         m_FontCollection;
        void*                   m_UserData; // The font map resources (see res_font.cpp)
        dmGraphics::HContext    m_GraphicsContext; // Used to recreate textures
        HFontRenderBackend      m_FontRenderBackend;
        dmGraphics::HTexture    m_Texture;
        HMaterial               m_Material;
        dmhash_t                m_NameHash;

        dmHashTable64<FontGlyph*>   m_Glyphs;       // Ache with generated glyphs (with bitmap data!)
        dmHashTable64<CacheGlyph*>  m_GlyphCache;   // Quick check what glyphs are in the cache texture
        CacheGlyph*                 m_Cache;        // The data (i.e. the pool)
        uint16_t*                   m_CacheIndices; // Indices into the cache array
        uint32_t                    m_CacheCursor;

        FGlyphCacheMiss         m_OnGlyphCacheMiss;
        void*                   m_OnGlyphCacheMissContext;

        float                   m_Size;         // Size of font (in pixels)
        float                   m_PixelScale;   // Scale factor from points to pixel scale
        float                   m_ShadowX;
        float                   m_ShadowY;
        float                   m_MaxAscent;
        float                   m_MaxDescent;
        float                   m_SdfSpread;
        float                   m_SdfOutline;
        float                   m_SdfShadow;
        float                   m_Alpha;
        float                   m_OutlineAlpha;
        float                   m_ShadowAlpha;

        uint8_t*                m_CellTempData; // a temporary unpack buffer for the compressed glyphs


        dmGraphics::TextureFormat m_CacheFormat;
        dmGraphics::TextureFilter m_MinFilter;
        dmGraphics::TextureFilter m_MagFilter;

        uint16_t                m_CacheMaxWidth;        // In texels
        uint16_t                m_CacheMaxHeight;       // In texels
        uint16_t                m_CacheWidth;           // In texels
        uint16_t                m_CacheHeight;          // In texels
        uint16_t                m_CacheCellWidth;       // In texels
        uint16_t                m_CacheCellHeight;      // In texels
        uint16_t                m_CacheCellMaxAscent;   // In texels
        uint16_t                m_CacheColumns;         // Number of cells in horizontal direction
        uint16_t                m_CacheRows;            // Number of cells in horizontal direction
        uint16_t                m_CacheCellCount;       // Number of cells in total
        uint16_t                m_MaxGlyphWidth;        // Maximum width of any of the used glyphs
        uint16_t                m_MaxGlyphHeight;       // Maximum height of any of the used glyphs
        uint8_t                 m_CacheCellPadding;
        uint8_t                 m_LayerMask;
        uint8_t                 m_Padding;              // The padding of the cell
        uint8_t                 m_IsMonospaced:1;
        uint8_t                 m_IsCacheSizeDirty:1;   // if the glyph cell size has changed, or if the layout needs to be recalculated
        uint8_t                 m_DynamicCacheSize:1;
        uint8_t                 m_IsCacheSizeTooSmall:1;
        uint8_t                 m_CacheChannels:3;      // Number of channels (1-4)
    };
}

#endif // #ifndef DM_FONTMAP_PRIVATE_H
