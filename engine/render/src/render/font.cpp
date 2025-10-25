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


#include "font.h"
#include "font_renderer_private.h"
#include "font_renderer_api.h"

#include <dlib/math.h>
#include <dlib/mutex.h>
#include <dlib/zlib.h>

#include <algorithm> // std::sort

namespace dmRender
{
    FontMapParams::FontMapParams()
    : m_GetGlyph(0)
    , m_GetGlyphData(0)
    , m_NameHash(0)
    , m_ShadowX(0.0f)
    , m_ShadowY(0.0f)
    , m_MaxAscent(0.0f)
    , m_MaxDescent(0.0f)
    , m_SdfSpread(1.0f)
    , m_SdfOutline(0)
    , m_SdfShadow(0)
    , m_CacheWidth(0)
    , m_CacheHeight(0)
    , m_CacheCellWidth(0)
    , m_CacheCellHeight(0)
    , m_GlyphChannels(1)
    , m_CacheCellPadding(0)
    , m_LayerMask(FACE)
    , m_IsMonospaced(false)
    , m_ImageFormat(dmRenderDDF::TYPE_BITMAP)
    {

    }

    // https://en.wikipedia.org/wiki/Delta_encoding
    static void delta_decode(uint8_t* buffer, int length)
    {
        uint8_t last = 0;
        for (int i = 0; i < length; i++)
        {
            uint8_t delta = buffer[i];
            buffer[i] = delta + last;
            last = buffer[i];
        }
    }

    // Font maps have no mips, so we need to make sure we use a supported min filter
    static dmGraphics::TextureFilter ConvertMinTextureFilter(dmGraphics::TextureFilter filter)
    {
        if (filter == dmGraphics::TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST)
        {
            filter = dmGraphics::TEXTURE_FILTER_NEAREST;
        }
        else if (filter == dmGraphics::TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST)
        {
            filter = dmGraphics::TEXTURE_FILTER_LINEAR;
        }

        return filter;
    }

    static void SetupCache(HFontMap font_map, uint32_t texture_width, uint32_t texture_height,
                                             uint32_t cell_width, uint32_t cell_height, uint32_t max_ascent)
    {
        if (font_map->m_Cache)
        {
            free(font_map->m_Cache);
            free(font_map->m_CellTempData);
            free(font_map->m_CacheIndices);
            font_map->m_GlyphCache.Clear();
        }

        font_map->m_CacheCellWidth = cell_width;
        font_map->m_CacheCellHeight = cell_height;
        font_map->m_CacheCellMaxAscent = max_ascent;

        font_map->m_CacheColumns = texture_width / cell_width;
        font_map->m_CacheRows = texture_height / cell_height;
        font_map->m_CacheCellCount = font_map->m_CacheColumns * font_map->m_CacheRows;

        font_map->m_CellTempData = (uint8_t*)malloc(font_map->m_CacheCellWidth*font_map->m_CacheCellHeight*4);

        font_map->m_CacheCursor = 0;
        font_map->m_CacheIndices = (uint16_t*)malloc(sizeof(uint16_t) * font_map->m_CacheCellCount);
        memset(font_map->m_CacheIndices, 0, sizeof(uint16_t) * font_map->m_CacheCellCount);

        font_map->m_Cache = (CacheGlyph*)malloc(sizeof(CacheGlyph) * font_map->m_CacheCellCount);
        memset(font_map->m_Cache, 0, sizeof(CacheGlyph*) * font_map->m_CacheCellCount);
        for (uint32_t i = 0; i < font_map->m_CacheCellCount; ++i)
        {
            font_map->m_CacheIndices[i] = i;

            CacheGlyph* glyph = &font_map->m_Cache[i];
            glyph->m_Glyph = 0;
            glyph->m_Frame = 0;

            // We calculate these only once
            uint32_t col = i % font_map->m_CacheColumns;
            uint32_t row = i / font_map->m_CacheColumns;
            glyph->m_X = col * font_map->m_CacheCellWidth;
            glyph->m_Y = row * font_map->m_CacheCellHeight;
        }

        uint32_t old_cap = font_map->m_GlyphCache.Capacity();
        int new_cap = font_map->m_CacheCellCount;
        if (new_cap > old_cap)
        {
            font_map->m_GlyphCache.SetCapacity((new_cap*3)/2, new_cap);
        }
    }

    static void ClearTexture(HFontMap font_map, uint32_t width, uint32_t height)
    {
        dmGraphics::TextureParams tex_params;
        tex_params.m_Format = font_map->m_CacheFormat;
        tex_params.m_Width = width;
        tex_params.m_Height = height;
        tex_params.m_Depth = 1;
        tex_params.m_MinFilter = dmGraphics::TEXTURE_FILTER_LINEAR;
        tex_params.m_MagFilter = dmGraphics::TEXTURE_FILTER_LINEAR;

        tex_params.m_DataSize = width * height * font_map->m_CacheChannels;
        tex_params.m_Data = malloc(tex_params.m_DataSize);
        memset((void*)tex_params.m_Data, 0, tex_params.m_DataSize);

        dmGraphics::SetTexture(font_map->m_GraphicsContext, font_map->m_Texture, tex_params);

        free((void*)tex_params.m_Data);
    }

    static void RecreateTexture(HFontMap font_map, dmGraphics::HContext graphics_context, uint32_t width, uint32_t height)
    {
        // create new texture to be used as a cache
        dmGraphics::TextureCreationParams tex_create_params;
        tex_create_params.m_Width = width;
        tex_create_params.m_Height = height;
        tex_create_params.m_OriginalWidth = width;
        tex_create_params.m_OriginalHeight = height;

        if (font_map->m_Texture)
        {
            dmGraphics::DeleteTexture(graphics_context, font_map->m_Texture);
        }
        font_map->m_Texture = dmGraphics::NewTexture(graphics_context, tex_create_params);

        ClearTexture(font_map, width, height);
    }

    /**
     * Update the font map with the specified parameters. The parameters are consumed and should not be read after this call.
     * @param font_map Font map handle
     * @param params Parameters to update
     * @return result true if the font map was created correctly
     */
    static bool SetFontMap(HFontMap font_map, dmRender::HRenderContext render_context, dmGraphics::HContext graphics_context, FontMapParams& params)
    {
        assert(params.m_GetGlyph);
        assert(params.m_GetGlyphData);

        font_map->m_NameHash = params.m_NameHash;
        font_map->m_GetGlyph = params.m_GetGlyph;
        font_map->m_GetGlyphData = params.m_GetGlyphData;
        font_map->m_GetFontMetrics = params.m_GetFontMetrics;
        font_map->m_ShadowX = params.m_ShadowX;
        font_map->m_ShadowY = params.m_ShadowY;
        font_map->m_MaxAscent = params.m_MaxAscent;
        font_map->m_MaxDescent = params.m_MaxDescent;
        font_map->m_SdfSpread = params.m_SdfSpread;
        font_map->m_SdfOutline = params.m_SdfOutline;
        font_map->m_SdfShadow = params.m_SdfShadow;
        font_map->m_Alpha = params.m_Alpha;
        font_map->m_OutlineAlpha = params.m_OutlineAlpha;
        font_map->m_ShadowAlpha = params.m_ShadowAlpha;
        font_map->m_LayerMask = params.m_LayerMask;
        font_map->m_IsMonospaced = params.m_IsMonospaced;
        font_map->m_Padding = params.m_Padding;

        // Is the cache allowed to grow?
        font_map->m_DynamicCacheSize = params.m_CacheWidth == 0 && params.m_CacheHeight == 0;
        if (font_map->m_DynamicCacheSize)
        {
            // we mustn't have a 0x0 size texture
            font_map->m_CacheWidth  = 64;
            font_map->m_CacheHeight = 64;
        }
        else
        {
            font_map->m_CacheWidth = params.m_CacheWidth;
            font_map->m_CacheHeight = params.m_CacheHeight;
        }
        font_map->m_CacheMaxWidth = params.m_CacheMaxWidth;
        font_map->m_CacheMaxHeight = params.m_CacheMaxHeight;

        uint16_t cell_width = dmMath::Max(8U, params.m_CacheCellWidth);
        uint16_t cell_height = dmMath::Max(8U, params.m_CacheCellHeight);

        font_map->m_CacheCellPadding = params.m_CacheCellPadding;
        font_map->m_CacheChannels = params.m_GlyphChannels;

        SetupCache(font_map, font_map->m_CacheWidth, font_map->m_CacheHeight,
                                cell_width, cell_height, params.m_CacheCellMaxAscent);

        switch (font_map->m_CacheChannels)
        {
            case 1:
                font_map->m_CacheFormat = dmGraphics::TEXTURE_FORMAT_LUMINANCE;
            break;
            case 3:
                font_map->m_CacheFormat = dmGraphics::TEXTURE_FORMAT_RGB;
            break;
            case 4:
                font_map->m_CacheFormat = dmGraphics::TEXTURE_FORMAT_RGBA;
            break;
            default:
                dmLogError("Invalid channel count for glyph data: %u", params.m_GlyphChannels);
                return false;
        };

        if (params.m_ImageFormat == dmRenderDDF::TYPE_BITMAP)
        {
            dmGraphics::GetDefaultTextureFilters(graphics_context, font_map->m_MinFilter, font_map->m_MagFilter);
            // No mips for font cache
            font_map->m_MinFilter = ConvertMinTextureFilter(font_map->m_MinFilter);
        }
        else // Distance-field font
        {
            font_map->m_MinFilter = dmGraphics::TEXTURE_FILTER_LINEAR;
            font_map->m_MagFilter = dmGraphics::TEXTURE_FILTER_LINEAR;
        }

        font_map->m_GraphicsContext = graphics_context;
        RecreateTexture(font_map, font_map->m_GraphicsContext, font_map->m_CacheWidth, font_map->m_CacheHeight);
        return true;
    }

    HFontMap NewFontMap(dmRender::HRenderContext render_context, dmGraphics::HContext graphics_context, FontMapParams& params)
    {
        FontMap* font_map = new FontMap;
        font_map->m_Mutex = dmMutex::New();
        bool result = SetFontMap(font_map, render_context, graphics_context, params);
        if (!result)
        {
            DeleteFontMap(font_map);
            return 0;
        }
        return font_map;
    }

    void SetFontMapLineHeight(HFontMap font_map, float max_ascent, float max_descent)
    {
        DM_MUTEX_SCOPED_LOCK(font_map->m_Mutex);
        font_map->m_MaxAscent = max_ascent;
        font_map->m_MaxDescent = max_descent;
    }

    void GetFontMapLineHeight(HFontMap font_map, float* max_ascent, float* max_descent)
    {
        DM_MUTEX_SCOPED_LOCK(font_map->m_Mutex);
        *max_ascent = font_map->m_MaxAscent;
        *max_descent = font_map->m_MaxDescent;
    }

    void SetFontMapCacheSize(HFontMap font_map, uint32_t cell_width, uint32_t cell_height, uint32_t max_ascent)
    {
        DM_MUTEX_SCOPED_LOCK(font_map->m_Mutex);
        font_map->m_IsCacheSizeDirty = 1;

        SetupCache(font_map, font_map->m_CacheWidth, font_map->m_CacheHeight,
                            cell_width, cell_height, max_ascent);
    }

    void GetFontMapCacheSize(HFontMap font_map, uint32_t* cell_width, uint32_t* cell_height, uint32_t* max_ascent)
    {
        DM_MUTEX_SCOPED_LOCK(font_map->m_Mutex);
        *cell_width = font_map->m_CacheCellWidth;
        *cell_height = font_map->m_CacheCellHeight;
        *max_ascent = font_map->m_CacheCellMaxAscent;
    }

    void DeleteFontMap(HFontMap font_map)
    {
        dmMutex::Delete(font_map->m_Mutex);
        delete font_map;
    }

    void SetFontMapUserData(HFontMap font_map, void* user_data)
    {
        DM_MUTEX_SCOPED_LOCK(font_map->m_Mutex);
        font_map->m_UserData = user_data;
    }

    void* GetFontMapUserData(HFontMap font_map)
    {
        DM_MUTEX_SCOPED_LOCK(font_map->m_Mutex);
        return font_map->m_UserData;
    }

    dmGraphics::HTexture GetFontMapTexture(HFontMap font_map)
    {
        DM_MUTEX_SCOPED_LOCK(font_map->m_Mutex);
        return font_map->m_Texture;
    }

    void SetFontMapMaterial(HFontMap font_map, HMaterial material)
    {
        DM_MUTEX_SCOPED_LOCK(font_map->m_Mutex);
        font_map->m_Material = material;
    }

    HMaterial GetFontMapMaterial(HFontMap font_map)
    {
        DM_MUTEX_SCOPED_LOCK(font_map->m_Mutex);
        return font_map->m_Material;
    }

    void GetTextMetrics(HFontMap font_map, const char* text, TextMetricsSettings* settings, TextMetrics* metrics)
    {
        DM_MUTEX_SCOPED_LOCK(font_map->m_Mutex);
        GetTextMetrics(font_map->m_FontRenderBackend, font_map, text, settings, metrics);
    }

    // also used for test
    dmRender::FontGlyph* GetGlyph(HFontMap font_map, uint32_t c)
    {
        dmRender::FontGlyph* glyph = font_map->m_GetGlyph(c, font_map->m_UserData);
        if (!glyph)
            glyph = font_map->m_GetGlyph(126U, font_map->m_UserData); // Fallback to ~
        return glyph;
    }

    struct FontGlyphInflaterContext {
        uint32_t m_Cursor;
        uint8_t* m_Output;
    };

    static bool FontGlyphInflater(void* context, const void* data, uint32_t data_len)
    {
        FontGlyphInflaterContext* ctx = (FontGlyphInflaterContext*)context;
        memcpy(ctx->m_Output + ctx->m_Cursor, data, data_len);
        ctx->m_Cursor += data_len;
        return true;
    }

    // static void DebugCache(HFontMap font_map)
    // {
    //     printf("Glyph cache:\n");
    //     for (uint32_t i = 0; i < font_map->m_CacheCursor; ++i)
    //     {
    //         uint16_t index = font_map->m_CacheIndices[i];
    //         CacheGlyph* g = &font_map->m_Cache[index];
    //         printf("%d: '%c'  t: %u  x/y: %u, %u  is_in_cache: %d\n", i, g->m_Glyph->m_Character, g->m_Frame, g->m_X, g->m_Y, IsInCache(font_map, g->m_Glyph->m_Character));
    //     }
    // }


    // From Box2D
    inline bool IsPowerOfTwo(uint32_t x)
    {
        return x > 0 && (x & (x - 1)) == 0;
    }

    // From Box2D
    inline uint32_t NextPowerOfTwo(uint32_t x)
    {
        x |= (x >> 1);
        x |= (x >> 2);
        x |= (x >> 4);
        x |= (x >> 8);
        x |= (x >> 16);
        return x + 1;
    }

    static bool GetNextCacheSize(HFontMap font_map, uint16_t* width, uint16_t* height)
    {
        const uint16_t max_width = font_map->m_CacheMaxWidth;
        const uint16_t max_height = font_map->m_CacheMaxHeight;
        if (*height <= *width)
            *height = NextPowerOfTwo(*height);
        else
            *width = NextPowerOfTwo(*width);

        return *width <= max_width && *height <= max_height;
    }

    static void ResetCache(HFontMap font_map, dmGraphics::HContext graphics_context, bool recreate_texture, dmRender::FontMetrics* metrics)
    {
        if (font_map->m_IsCacheSizeTooSmall)
        {
            GetNextCacheSize(font_map, &font_map->m_CacheWidth, &font_map->m_CacheHeight);
            font_map->m_IsCacheSizeTooSmall = 0;
        }
        else
        {
            font_map->m_CacheWidth = dmMath::Max(font_map->m_CacheWidth, (uint16_t)metrics->m_ImageMaxWidth);
            font_map->m_CacheHeight = dmMath::Max(font_map->m_CacheHeight, (uint16_t)metrics->m_ImageMaxHeight);
        }

        if (!IsPowerOfTwo(font_map->m_CacheWidth))
            font_map->m_CacheWidth = NextPowerOfTwo(font_map->m_CacheWidth);
        if (!IsPowerOfTwo(font_map->m_CacheHeight))
            font_map->m_CacheHeight = NextPowerOfTwo(font_map->m_CacheHeight);

#if defined(__EMSCRIPTEN__)
        // Currently the web gpu backend has a bug in the SetTexture mechanism.
        // So we recreate the texture for now.
        RecreateTexture(font_map, graphics_context, font_map->m_CacheWidth, font_map->m_CacheHeight);
#else
        if (recreate_texture)
            RecreateTexture(font_map, graphics_context, font_map->m_CacheWidth, font_map->m_CacheHeight);
        else
            ClearTexture(font_map, font_map->m_CacheWidth, font_map->m_CacheHeight);
#endif

        SetFontMapCacheSize(font_map, metrics->m_ImageMaxWidth, metrics->m_ImageMaxHeight, metrics->m_MaxAscent);
    }

    // Is the font cache too small for the largest glyph?
    static bool IsTextureTooSmall(HFontMap font_map, dmRender::FontMetrics& font_metrics)
    {
        if (!font_map->m_GetFontMetrics)
            return false;

        uint32_t num_glyphs = font_map->m_GetFontMetrics(font_map->m_UserData, &font_metrics);

        // If the texture is actually too small
        return num_glyphs > 0 && (font_map->m_CacheWidth < font_metrics.m_MaxWidth || font_map->m_CacheHeight < font_metrics.m_MaxHeight);
    }

    void UpdateCacheTexture(HFontMap font_map)
    {
        DM_MUTEX_SCOPED_LOCK(font_map->m_Mutex);
        dmRender::FontMetrics font_metrics = {0};
        bool texture_too_small = IsTextureTooSmall(font_map, font_metrics);
        bool update_cache = font_map->m_IsCacheSizeDirty || texture_too_small;

        if (update_cache)
        {
            ResetCache(font_map, font_map->m_GraphicsContext, texture_too_small, &font_metrics);
            font_map->m_IsCacheSizeDirty = 0;
        }
    }

    static void UpdateGlyphTexture(HFontMap font_map, dmRender::FontGlyph* g, int32_t x, int32_t y, int offset_y)
    {
        uint32_t glyph_data_compression = 0; // E.g. FONT_GLYPH_COMPRESSION_NONE;
        uint32_t glyph_data_size = 0;
        uint32_t glyph_image_width = 0;
        uint32_t glyph_image_height = 0;
        uint32_t glyph_image_channels = 0;
        uint8_t* glyph_data = (uint8_t*)font_map->m_GetGlyphData(g->m_Character, font_map->m_UserData, &glyph_data_size, &glyph_data_compression, &glyph_image_width, &glyph_image_height, &glyph_image_channels);

        void* data = 0;
        if (FONT_GLYPH_COMPRESSION_DEFLATE == glyph_data_compression)
        {
            // When if came to choosing between the different algorithms, here are some speed/compression tests
            // Decoding 100 glyphs
            // lz4:     0.1060 ms  compression: 72%
            // deflate: 0.2190 ms  compression: 66%
            // png:     0.6930 ms  compression: 67%
            // webp:    1.5170 ms  compression: 55%
            // further improvements (different test, Android, 92 glyphs)
            // webp          2.9440 ms  compression: 55%
            // deflate       0.7110 ms  compression: 66%
            // deflate+delta 0.7680 ms  compression: 62%

            FontGlyphInflaterContext deflate_context;
            deflate_context.m_Output = font_map->m_CellTempData;
            deflate_context.m_Cursor = 0;
            dmZlib::Result zlib_result = dmZlib::InflateBuffer(glyph_data, glyph_data_size, &deflate_context, FontGlyphInflater);
            if (zlib_result != dmZlib::RESULT_OK)
            {
                dmLogError("Failed to decompress glyph (%c) in font %s: %d", g->m_Character, dmHashReverseSafe64(font_map->m_NameHash), zlib_result);
                return;
            }

            uint32_t uncompressed_size = deflate_context.m_Cursor;
            delta_decode(font_map->m_CellTempData, uncompressed_size);

            data = font_map->m_CellTempData;
        }
        else if (FONT_GLYPH_COMPRESSION_NONE == glyph_data_compression)
        {
            uint32_t num_out_channels;
            switch(font_map->m_CacheFormat)
            {
            case dmGraphics::TEXTURE_FORMAT_LUMINANCE:  num_out_channels = 1; break;
            case dmGraphics::TEXTURE_FORMAT_RGB:        num_out_channels = 3; break;
            case dmGraphics::TEXTURE_FORMAT_RGBA:       num_out_channels = 4; break;
            default:
                dmLogWarning("Unknown texture format: %d", font_map->m_CacheFormat);
                num_out_channels = glyph_image_channels;
            }

            data = glyph_data;
            if (glyph_image_channels != num_out_channels)
            {
                data = font_map->m_CellTempData;

                uint32_t cursor = 0;
                uint8_t* tmp = font_map->m_CellTempData;;
                for (uint32_t y = 0; y < glyph_image_height; ++y)
                {
                    for (uint32_t x = 0; x < glyph_image_width; ++x)
                    {
                        uint8_t v = glyph_data[y * glyph_image_width + x];
                        for (uint32_t c = 0; c < num_out_channels; ++c)
                            tmp[cursor++] = v;
                    }
                }
            }
        }
        else
        {
            dmLogOnceError("Unknown glyph compression: %u for glyph (%c) in font %s", glyph_data_compression, g->m_Character, dmHashReverseSafe64(font_map->m_NameHash));
            return;
        }

        dmGraphics::TextureParams tex_params;
        tex_params.m_SubUpdate = true;
        tex_params.m_MipMap = 0;
        tex_params.m_Format = font_map->m_CacheFormat;
        tex_params.m_MinFilter = font_map->m_MinFilter;
        tex_params.m_MagFilter = font_map->m_MagFilter;

        tex_params.m_Width = glyph_image_width;
        tex_params.m_Height = glyph_image_height;
        tex_params.m_Depth = 1;

        tex_params.m_X = x;
        tex_params.m_Y = y + offset_y;

        tex_params.m_Data = data;

        // Upload glyph data to GPU
        dmGraphics::SetTexture(font_map->m_GraphicsContext, font_map->m_Texture, tex_params);
    }

    struct CompareCacheGlyphPred
    {
        CacheGlyph* m_Glyphs;
        CompareCacheGlyphPred(CacheGlyph* glyphs)
        : m_Glyphs(glyphs) {}

        bool operator()(uint16_t ia, uint16_t ib) const
        {
            const CacheGlyph& a = m_Glyphs[ia];
            const CacheGlyph& b = m_Glyphs[ib];
            // Sort the oldest last (e.g. 0 is at the end)
            return a.m_Frame > b.m_Frame;
        }
    };

    static void SortCache(HFontMap font_map)
    {
        std::sort(font_map->m_CacheIndices, font_map->m_CacheIndices + font_map->m_CacheCursor, CompareCacheGlyphPred(font_map->m_Cache));
    }

    // Either get a free slot, or the oldest one
    CacheGlyph* AcquireFreeGlyphFromCache(HFontMap font_map, uint32_t c, uint32_t time)
    {
        uint32_t index;
        if (font_map->m_CacheCursor < font_map->m_CacheCellCount)
            index = font_map->m_CacheCursor++;      // Get the unused slot
        else
            index = font_map->m_CacheCellCount-1;   // Get the oldest slot

        return &font_map->m_Cache[index];
    }

    CacheGlyph* GetFromCache(HFontMap font_map, uint32_t c)
    {
        CacheGlyph** glyphp = font_map->m_GlyphCache.Get(c);
        return glyphp ? *glyphp : 0;
    }

    bool IsInCache(HFontMap font_map, uint32_t c)
    {
        return GetFromCache(font_map, c) != 0;
    }

    static bool CanCacheTextureGrow(HFontMap font_map)
    {
        if (!font_map->m_DynamicCacheSize)
        {
            return false;
        }

        uint16_t width  = font_map->m_CacheWidth;
        uint16_t height = font_map->m_CacheHeight;
        bool result = GetNextCacheSize(font_map, &width, &height);
        return result;
    }

    void AddGlyphToCache(HFontMap font_map, uint32_t frame, dmRender::FontGlyph* g, int32_t g_offset_y)
    {
        DM_MUTEX_SCOPED_LOCK(font_map->m_Mutex);

        // Locate a cache cell candidate
        CacheGlyph* cache_glyph = AcquireFreeGlyphFromCache(font_map, g->m_Character, frame);

        if (cache_glyph->m_Glyph && cache_glyph->m_Frame == frame)
        {
            bool can_resize = CanCacheTextureGrow(font_map);
            if (can_resize)
            {
                font_map->m_IsCacheSizeDirty = 1;
                font_map->m_IsCacheSizeTooSmall = 1;
                return;
            }

            // It means we've filled the entire cache with upload requests
            // We might then just as well skip the next uploads until the next frame
            dmLogWarning("Entire font glyph cache (%u x %u) is filled in a single frame %u ('%c' %u). Consider increasing the cache for %s", font_map->m_CacheWidth, font_map->m_CacheHeight, frame, g->m_Character < 255 ? g->m_Character : ' ', g->m_Character, dmHashReverseSafe64(font_map->m_NameHash));
            return;
        }

        if (cache_glyph->m_Glyph) // It already existed in the cache
        {
            // Clear the old data from the cache
            font_map->m_GlyphCache.Erase(cache_glyph->m_Glyph->m_Character);
        }
        cache_glyph->m_Glyph = g;

        font_map->m_GlyphCache.Put(g->m_Character, cache_glyph);

        // Sort the glyphs, so that if we need to add another one, the oldest is at the end, for fast access
        cache_glyph->m_Frame = frame;
        SortCache(font_map);

        //DebugCache(font_map);

        UpdateGlyphTexture(font_map, g, cache_glyph->m_X, cache_glyph->m_Y, g_offset_y);
    }

    const uint8_t* GetGlyphData(dmRender::HFontMap font_map, uint32_t codepoint, uint32_t* out_size, uint32_t* out_compression, uint32_t* out_width, uint32_t* out_height, uint32_t* out_channels)
    {
        return (uint8_t*)font_map->m_GetGlyphData(codepoint, font_map->m_UserData, out_size, out_compression, out_width, out_height, out_channels);
    }

    uint32_t GetFontMapResourceSize(HFontMap font_map)
    {
        uint32_t size = sizeof(FontMap);
        // The cache size
        size += font_map->m_CacheCellCount*( (sizeof(CacheGlyph) * sizeof(uint32_t)) );
        // The texture size
        size += dmGraphics::GetTextureResourceSize(font_map->m_GraphicsContext, font_map->m_Texture);
        return size;
    }

    // Test functions begin
    bool VerifyFontMapMinFilter(dmRender::HFontMap font_map, dmGraphics::TextureFilter filter)
    {
        return font_map->m_MinFilter == filter;
    }

    bool VerifyFontMapMagFilter(dmRender::HFontMap font_map, dmGraphics::TextureFilter filter)
    {
        return font_map->m_MagFilter == filter;
    }
}
