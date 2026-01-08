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


#include "fontmap.h"
#include "fontmap_private.h"
#include "font_renderer_private.h"
#include "font_renderer_api.h"

#include <dlib/math.h>
#include <dlib/mutex.h>
#include <dlib/zlib.h>

#include <algorithm> // std::sort

namespace dmRender
{
    FontMapParams::FontMapParams()
    : m_FontCollection(0)
    , m_NameHash(0)
    , m_OnGlyphCacheMiss(0)
    , m_OnGlyphCacheMissContext(0)
    , m_Size(0.0f)
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
        font_map->m_FontCollection = params.m_FontCollection;
        font_map->m_NameHash = params.m_NameHash;
        font_map->m_Size = params.m_Size;
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

        font_map->m_OnGlyphCacheMiss = params.m_OnGlyphCacheMiss;
        font_map->m_OnGlyphCacheMissContext = params.m_OnGlyphCacheMissContext;

        // Is the cache allowed to grow?
        font_map->m_DynamicCacheSize = params.m_CacheWidth < params.m_CacheMaxWidth || params.m_CacheHeight < params.m_CacheMaxHeight;
        font_map->m_CacheWidth = params.m_CacheWidth;
        font_map->m_CacheHeight = params.m_CacheHeight;
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

    static void SetFontMapCacheSize(HFontMap font_map, uint16_t cell_width, uint16_t cell_height, uint16_t max_ascent)
    {
        font_map->m_IsCacheSizeDirty = 1;

        SetupCache(font_map, font_map->m_CacheWidth, font_map->m_CacheHeight,
                            cell_width, cell_height, max_ascent);
    }
    static void GetFontMapCacheSize(HFontMap font_map, uint16_t* cell_width, uint16_t* cell_height, uint16_t* max_ascent)
    {
        *cell_width = font_map->m_CacheCellWidth;
        *cell_height = font_map->m_CacheCellHeight;
        *max_ascent = font_map->m_CacheCellMaxAscent;
    }

    void DeleteFontMap(HFontMap font_map)
    {
        FontCollectionDestroy(font_map->m_FontCollection);
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

    float GetFontMapSize(dmRender::HFontMap font_map)
    {
        DM_MUTEX_SCOPED_LOCK(font_map->m_Mutex);
        return font_map->m_Size;
    }

    bool GetFontMapMonospaced(dmRender::HFontMap font_map)
    {
        return font_map->m_IsMonospaced;
    }

    uint32_t GetFontMapPadding(dmRender::HFontMap font_map)
    {
        return font_map->m_Padding;
    }

    void GetTextMetrics(HFontMap font_map, const char* text, TextLayoutSettings* settings, TextMetrics* metrics)
    {
        DM_MUTEX_SCOPED_LOCK(font_map->m_Mutex);
        GetTextMetrics(font_map->m_FontRenderBackend, font_map, text, settings, metrics);
    }

    uint64_t MakeGlyphIndexKey(HFont font, uint32_t glyph_index)
    {
        uint64_t path_hash = (uint64_t)FontGetPathHash(font);
        return path_hash<<32 | glyph_index;
    }

    static void AddGlyph(dmRender::HFontMap font_map, uint64_t key, FontGlyph* glyph)
    {
        if (font_map->m_Glyphs.Full())
            font_map->m_Glyphs.OffsetCapacity(16);
        font_map->m_Glyphs.Put(key, glyph);
    }

    static FontResult HandleCacheMiss(dmRender::HFontMap font_map, HFont font, uint32_t glyph_index, uint64_t key, FontGlyph** glyph)
    {
        if (!font_map->m_OnGlyphCacheMiss)
            return FONT_RESULT_ERROR;

        FontResult r = font_map->m_OnGlyphCacheMiss(font_map->m_OnGlyphCacheMissContext, font_map, font, glyph_index, glyph);
        if (FONT_RESULT_OK == r && glyph != 0)
        {
            return FONT_RESULT_OK;
        }

        return FONT_RESULT_ERROR;
    }

    FontResult GetOrCreateGlyphByIndex(dmRender::HFontMap font_map, HFont font, uint32_t glyph_index, FontGlyph** glyph)
    {
        uint64_t key = MakeGlyphIndexKey(font, glyph_index);
        FontGlyph** pglyph = font_map->m_Glyphs.Get(key);
        if (pglyph)
        {
            *glyph = *pglyph;
            return FONT_RESULT_OK;
        }

        FontResult r;
        FontType type = FontGetType(font);

        if (type == FONT_TYPE_STBTTF)
        {
            // Since generating the SDF takes a long time (several milliseconds)
            // we simply opt out of creating that data just-in-time
            r = HandleCacheMiss(font_map, font, glyph_index, key, glyph);
            if (FONT_RESULT_OK == r)
            {
                AddGlyph(font_map, key, *glyph);
                return r;
            }
            return FONT_RESULT_ERROR; // we don't yet have the glyph
        }

        // If we reached this point, it'a a .glyphbankc font

        FontGlyphOptions glyph_options;
        glyph_options.m_Scale = 1.0f;           // Glyph bank fonts are pre-rendered, so we use scale 1 for all its metrics
        glyph_options.m_GenerateImage = true;   // We want to get (or "generate" the glyphbank images)

        FontGlyph* out = new FontGlyph;
        r = FontGetGlyphByIndex(font, glyph_index, &glyph_options, out);
        if (FONT_RESULT_OK != r)
        {
            // Last chance to get the glyph just-in-time
            r = HandleCacheMiss(font_map, font, glyph_index, key, glyph);
            if (FONT_RESULT_OK == r)
            {
                AddGlyph(font_map, key, *glyph);
                return r;
            }

            delete out;
        }
        else
        {
            *glyph = out;
            AddGlyph(font_map, key, *glyph);
        }

        return r;
    }

    // Used for test
    FontResult GetOrCreateGlyph(dmRender::HFontMap font_map, HFont font, uint32_t codepoint, FontGlyph** glyph)
    {
        uint32_t glyph_index = FontGetGlyphIndex(font, codepoint);
        return GetOrCreateGlyphByIndex(font_map, font, glyph_index, glyph);
    }

    void AddGlyphByIndex(dmRender::HFontMap font_map, HFont font, uint32_t glyph_index, FontGlyph* glyph)
    {
        DM_MUTEX_SCOPED_LOCK(font_map->m_Mutex);

        uint64_t key = MakeGlyphIndexKey(font, glyph_index);
        FontGlyph** pglyph = font_map->m_Glyphs.Get(key);
        if (pglyph)
        {
            FontFreeGlyph(font, *pglyph);
            delete *pglyph;
        }

        if (font_map->m_Glyphs.Full())
            font_map->m_Glyphs.OffsetCapacity(16);

        font_map->m_Glyphs.Put(key, glyph);


        uint16_t prev_width, prev_height, prev_ascent;
        dmRender::GetFontMapCacheSize(font_map, &prev_width, &prev_height, &prev_ascent);

        uint16_t bitmap_width   = (uint16_t)glyph->m_Bitmap.m_Width;
        uint16_t bitmap_height  = (uint16_t)glyph->m_Bitmap.m_Height;
        uint16_t ascent         = (uint16_t)glyph->m_Ascent;
        bool dirty = bitmap_width > prev_width ||
                      bitmap_height > prev_height ||
                      ascent > prev_ascent;
        if (dirty)
        {
            font_map->m_CacheCellWidth      = dmMath::Max(bitmap_width, prev_width);
            font_map->m_CacheCellHeight     = dmMath::Max(bitmap_height, prev_height);
            font_map->m_CacheCellMaxAscent  = dmMath::Max(ascent, prev_ascent);
            dmRender::SetFontMapCacheSize(font_map, font_map->m_CacheCellWidth, font_map->m_CacheCellHeight, font_map->m_CacheCellMaxAscent);
        }
    }

    void RemoveGlyphByIndex(dmRender::HFontMap font_map, HFont font, uint32_t glyph_index)
    {
        uint64_t key = MakeGlyphIndexKey(font, glyph_index);
        FontGlyph** pglyph = font_map->m_Glyphs.Get(key);
        if (pglyph)
        {
            FontFreeGlyph(font, *pglyph);
            delete *pglyph;
            font_map->m_Glyphs.Erase(key);
        }
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
    //         printf("%d: %p  t: %u  x/y: %u, %u  in cache: %d\n", i, (void*)(uintptr_t)g->m_GlyphKey, g->m_Frame, g->m_X, g->m_Y, IsInCache(font_map, g->m_GlyphKey));
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

    static void ResetCache(HFontMap font_map, dmGraphics::HContext graphics_context, bool recreate_texture,
                            uint16_t cell_width, uint16_t cell_height, uint16_t max_ascent)
    {
        if (font_map->m_IsCacheSizeTooSmall)
        {
            GetNextCacheSize(font_map, &font_map->m_CacheWidth, &font_map->m_CacheHeight);
            font_map->m_IsCacheSizeTooSmall = 0;
        }
        else
        {
            font_map->m_CacheWidth = dmMath::Max(font_map->m_CacheWidth, (uint16_t)cell_width);
            font_map->m_CacheHeight = dmMath::Max(font_map->m_CacheHeight, (uint16_t)cell_height);
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

        SetFontMapCacheSize(font_map, cell_width, cell_height, max_ascent);
    }

    // Is the font cache too small for the largest glyph?
    static bool IsTextureTooSmall(HFontMap font_map)
    {
        // If the texture is actually too small
        return font_map->m_CacheWidth < font_map->m_CacheCellWidth ||
               font_map->m_CacheHeight < font_map->m_CacheCellHeight;
    }

    void UpdateCacheTexture(HFontMap font_map)
    {
        DM_MUTEX_SCOPED_LOCK(font_map->m_Mutex);
        bool texture_too_small = IsTextureTooSmall(font_map);
        bool update_cache = font_map->m_IsCacheSizeDirty || texture_too_small;

        if (update_cache)
        {
            ResetCache(font_map, font_map->m_GraphicsContext, texture_too_small,
                        font_map->m_CacheCellWidth, font_map->m_CacheCellHeight, font_map->m_CacheCellMaxAscent);
            font_map->m_IsCacheSizeDirty = 0;
        }
    }

    static void UpdateGlyphTexture(HFontMap font_map, FontGlyph* g, int32_t x, int32_t y, int offset_y)
    {
        uint32_t glyph_image_width      = g->m_Bitmap.m_Width;
        uint32_t glyph_image_height     = g->m_Bitmap.m_Height;
        uint32_t glyph_image_channels   = g->m_Bitmap.m_Channels;
        uint8_t* glyph_data             = g->m_Bitmap.m_Data;
        uint32_t glyph_data_flags       = g->m_Bitmap.m_Flags; // E.g. FONT_GLYPH_COMPRESSION_NONE;
        uint32_t glyph_data_size        = g->m_Bitmap.m_DataSize;

        void* data = 0;
        if ((FONT_GLYPH_COMPRESSION_DEFLATE & glyph_data_flags))
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
                dmLogError("Failed to decompress glyph (%c / %u) in font %s: %d", g->m_Codepoint, g->m_GlyphIndex, dmHashReverseSafe64(font_map->m_NameHash), zlib_result);
                return;
            }

            uint32_t uncompressed_size = deflate_context.m_Cursor;
            delta_decode(font_map->m_CellTempData, uncompressed_size);

            data = font_map->m_CellTempData;
        }
        else
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
    static CacheGlyph* AcquireFreeGlyphFromCache(HFontMap font_map)
    {
        uint32_t i;
        if (font_map->m_CacheCursor < font_map->m_CacheCellCount)
            i = font_map->m_CacheCursor++;      // Get the unused slot
        else
            i = font_map->m_CacheCellCount-1;   // Get the oldest slot

        uint32_t index = font_map->m_CacheIndices[i];
        return &font_map->m_Cache[index];
    }

    CacheGlyph* GetFromCache(HFontMap font_map, uint64_t glyph_key, uint32_t frame)
    {
        CacheGlyph** glyphp = font_map->m_GlyphCache.Get(glyph_key);
        if (glyphp)
        {
            (*glyphp)->m_Frame = frame;
            return *glyphp;
        }
        return 0;
    }

    bool IsInCache(HFontMap font_map, uint64_t glyph_key)
    {
        CacheGlyph** glyphp = font_map->m_GlyphCache.Get(glyph_key);
        return glyphp != 0;
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

    void AddGlyphToCache(HFontMap font_map, uint32_t frame, uint64_t glyph_key, FontGlyph* glyph, int32_t g_offset_y)
    {
        DM_MUTEX_SCOPED_LOCK(font_map->m_Mutex);

        // Since accessing glyphs will update their timestamps, we need to sort them when
        // we need to allocate a new glyph, so that we pick the oldest one
        SortCache(font_map);
        //DebugCache(font_map);

        // Locate a cache cell candidate
        CacheGlyph* cache_glyph = AcquireFreeGlyphFromCache(font_map);

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
            dmLogWarning("Entire font glyph cache (%u x %u) is filled in a single frame %u ('%c' %u / %u). Consider increasing the cache for %s", font_map->m_CacheWidth, font_map->m_CacheHeight, frame, glyph->m_Codepoint < 255 ? glyph->m_Codepoint : ' ', glyph->m_Codepoint, glyph->m_GlyphIndex  , dmHashReverseSafe64(font_map->m_NameHash));
            return;
        }

        if (cache_glyph->m_Glyph) // It already existed in the cache
        {
            // Clear the old data from the cache
            font_map->m_GlyphCache.Erase(cache_glyph->m_GlyphKey);
        }
        cache_glyph->m_Glyph = glyph;
        cache_glyph->m_GlyphKey = glyph_key;
        cache_glyph->m_Frame = frame;

        font_map->m_GlyphCache.Put(glyph_key, cache_glyph);


        UpdateGlyphTexture(font_map, glyph, cache_glyph->m_X, cache_glyph->m_Y, g_offset_y);
    }


    HFontCollection GetFontCollection(dmRender::HFontMap font_map)
    {
        return font_map->m_FontCollection;
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
