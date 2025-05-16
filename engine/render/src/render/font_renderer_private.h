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

#ifndef DM_FONT_RENDERER_PRIVATE_H
#define DM_FONT_RENDERER_PRIVATE_H

#include "font_renderer.h"
#include "render_private.h"

#include <dlib/align.h>
#include <dlib/math.h>
#include <dlib/mutex.h>
#include <dlib/utf8.h>

#include <graphics/graphics.h>

namespace dmRender
{
    const uint32_t ZERO_WIDTH_SPACE_UNICODE = 0x200B;

    enum RenderLayerMask
    {
        FACE    = 0x1,
        OUTLINE = 0x2,
        SHADOW  = 0x4
    };

    struct FontMap
    {
        FontMap()
        : m_Mutex(0)
        , m_UserData(0)
        , m_Texture(0)
        , m_Material(0)
        , m_NameHash(0)
        , m_GetGlyph(0)
        , m_GetGlyphData(0)
        , m_GetFontMetrics(0)
        , m_ShadowX(0.0f)
        , m_ShadowY(0.0f)
        , m_MaxAscent(0.0f)
        , m_MaxDescent(0.0f)
        , m_CellTempData(0)
        , m_Cache(0)
        , m_CacheIndices(0)
        , m_CacheCursor(0)
        , m_CacheWidth(0)
        , m_CacheHeight(0)
        , m_CacheCellWidth(0)
        , m_CacheCellHeight(0)
        , m_CacheCellMaxAscent(0)
        , m_CacheColumns(0)
        , m_CacheRows(0)
        , m_CacheCellCount(0)
        , m_CacheCellPadding(0)
        , m_LayerMask(FACE)
        , m_IsMonospaced(0)
        , m_IsCacheSizeDirty(0)
        , m_Padding(0)
        {
        }

        ~FontMap()
        {
            free(m_CacheIndices);
            m_CacheIndices = 0;

            free(m_Cache);
            m_Cache = 0;

            free(m_CellTempData);
            m_CellTempData = 0;

            dmGraphics::DeleteTexture(m_Texture);
        }

        dmMutex::HMutex         m_Mutex;
        void*                   m_UserData; // The font map resources (see res_font.cpp)
        dmGraphics::HContext    m_GraphicsContext; // Used to recreate textures
        HFontRenderBackend      m_FontRenderBackend;
        dmGraphics::HTexture    m_Texture;
        HMaterial               m_Material;
        dmhash_t                m_NameHash;

        FGetGlyph               m_GetGlyph;
        FGetGlyphData           m_GetGlyphData;
        FGetFontMetrics         m_GetFontMetrics;

        float                   m_ShadowX;
        float                   m_ShadowY;
        float                   m_MaxAscent;
        float                   m_MaxDescent;
        float                   m_SdfSpread;
        float                   m_SdfOffset;
        float                   m_SdfOutline;
        float                   m_SdfShadow;
        float                   m_Alpha;
        float                   m_OutlineAlpha;
        float                   m_ShadowAlpha;

        uint8_t*                m_CellTempData; // a temporary unpack buffer for the compressed glyphs

        dmHashTable32<CacheGlyph*>  m_GlyphCache;   // Quick check what glyphs are in the cache
        CacheGlyph*                 m_Cache;        // The data (i.e. the pool)
        uint16_t*                   m_CacheIndices; // Indices into the cache array
        uint32_t                    m_CacheCursor;

        dmGraphics::TextureFormat m_CacheFormat;
        dmGraphics::TextureFilter m_MinFilter;
        dmGraphics::TextureFilter m_MagFilter;

        uint32_t                m_CacheWidth;           // In texels
        uint32_t                m_CacheHeight;          // In texels
        uint32_t                m_CacheCellWidth;       // In texels
        uint32_t                m_CacheCellHeight;      // In texels
        uint32_t                m_CacheCellMaxAscent;   // In texels
        uint32_t                m_CacheColumns;         // Number of cells in horizontal direction
        uint32_t                m_CacheRows;            // Number of cells in horizontal direction
        uint32_t                m_CacheCellCount;       // Number of cells in total
        uint8_t                 m_CacheChannels;        // Number of channels
        uint8_t                 m_CacheCellPadding;
        uint8_t                 m_LayerMask;
        uint8_t                 m_IsMonospaced:1;
        uint8_t                 m_IsCacheSizeDirty:1;   // if the glyph cell size has changed, or if the layout needs to be recalculated
        uint8_t                 m_Padding:6;            // The padding of the cell
    };

    ///////////////////////////////////////////////////////////////////////////////

    // Helper to calculate horizontal pivot point
    static inline float OffsetX(uint32_t align, float width)
    {
        switch (align)
        {
            case TEXT_ALIGN_LEFT:
                return 0.0f;
            case TEXT_ALIGN_CENTER:
                return width * 0.5f;
            case TEXT_ALIGN_RIGHT:
                return width;
            default:
                return 0.0f;
        }
    }

    // Helper to calculate vertical pivot point
    static inline float OffsetY(uint32_t valign, float height, float ascent, float descent, float leading, uint32_t line_count)
    {
        float line_height = ascent + descent;
        switch (valign)
        {
            case TEXT_VALIGN_TOP:
                return height - ascent;
            case TEXT_VALIGN_MIDDLE:
                return height * 0.5f + (line_count * (line_height * leading) - line_height * (leading - 1.0f)) * 0.5f - ascent;
            case TEXT_VALIGN_BOTTOM:
                return (line_height * leading * (line_count - 1)) + descent;
            default:
                return height - ascent;
        }
    }
}

#endif // #ifndef DM_FONT_RENDERER_PRIVATE_H
