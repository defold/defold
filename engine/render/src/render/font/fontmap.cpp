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
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <vector>

namespace dmRender
{
    static const dmhash_t CURVE_TEXTURE_HASH = dmHashString64("curve_texture");
    static const dmhash_t BAND_TEXTURE_HASH = dmHashString64("band_texture");
    static const uint32_t VECTOR_CURVE_TEXTURE_WIDTH = 512;
    static const uint32_t VECTOR_CURVE_TEXTURE_HEIGHT = 64;
    static const uint32_t VECTOR_BAND_TEXTURE_WIDTH = 2048;
    static const uint32_t VECTOR_BAND_TEXTURE_HEIGHT = 128;
    static const uint32_t VECTOR_MAX_SHADER_CURVES = 256;
    static const uint32_t VECTOR_SCANLINE_STRIPE_COUNT = 8;
    static const uint32_t VECTOR_MAX_BANDS = 8;

    static void ResetVectorCache(HFontMap font_map);

    static uint16_t FloatToHalf(float value)
    {
        uint32_t bits;
        memcpy(&bits, &value, sizeof(bits));

        uint32_t sign = (bits >> 16) & 0x8000u;
        uint32_t mantissa = bits & 0x007fffffu;
        int32_t exponent = (int32_t)((bits >> 23) & 0xffu) - 127 + 15;

        if (exponent <= 0)
        {
            if (exponent < -10)
            {
                return (uint16_t)sign;
            }

            mantissa |= 0x00800000u;
            uint32_t shift = (uint32_t)(14 - exponent);
            uint32_t half_mantissa = mantissa >> shift;
            if ((mantissa >> (shift - 1)) & 1u)
            {
                half_mantissa += 1u;
            }
            return (uint16_t)(sign | half_mantissa);
        }

        if (exponent >= 31)
        {
            if ((bits & 0x7fffffffu) > 0x7f800000u)
            {
                return (uint16_t)(sign | 0x7e00u);
            }
            return (uint16_t)(sign | 0x7c00u);
        }

        uint32_t half = sign | ((uint32_t)exponent << 10) | (mantissa >> 13);
        if (mantissa & 0x00001000u)
        {
            half += 1u;
        }
        return (uint16_t)half;
    }

    static bool SelectVectorCurveTextureFormat(HFontMap font_map)
    {
        // Slug band entries contain absolute curve texel indices. RGBA16F cannot
        // represent every integer once the shared curve cache grows past 2048
        // texels, so using it would make later glyphs reference adjacent curves.
        if (font_map->m_VectorRenderer == FONT_RENDERER_SLUG)
        {
            if (dmGraphics::IsTextureFormatSupported(font_map->m_GraphicsContext, dmGraphics::TEXTURE_FORMAT_RGBA32F))
            {
                font_map->m_VectorCurveFormat = dmGraphics::TEXTURE_FORMAT_RGBA32F;
                font_map->m_VectorCurveComponentSize = sizeof(float);
                font_map->m_VectorCurveTexelsPerCurve = 2;
                return true;
            }

            dmLogError("Slug vector font %s requires RGBA32F texture support",
                       dmHashReverseSafe64(font_map->m_NameHash));
            font_map->m_VectorCurveFormat = dmGraphics::TEXTURE_FORMAT_RGBA;
            font_map->m_VectorCurveComponentSize = 0;
            font_map->m_VectorCurveTexelsPerCurve = 0;
            return false;
        }

        if (dmGraphics::IsTextureFormatSupported(font_map->m_GraphicsContext, dmGraphics::TEXTURE_FORMAT_RGBA16F))
        {
            font_map->m_VectorCurveFormat = dmGraphics::TEXTURE_FORMAT_RGBA16F;
            font_map->m_VectorCurveComponentSize = sizeof(uint16_t);
            font_map->m_VectorCurveTexelsPerCurve = 2;
            return true;
        }

        if (dmGraphics::IsTextureFormatSupported(font_map->m_GraphicsContext, dmGraphics::TEXTURE_FORMAT_RGBA32F))
        {
            dmLogWarning("RGBA16F is not supported for vector font %s; falling back to RGBA32F curve texture",
                         dmHashReverseSafe64(font_map->m_NameHash));
            font_map->m_VectorCurveFormat = dmGraphics::TEXTURE_FORMAT_RGBA32F;
            font_map->m_VectorCurveComponentSize = sizeof(float);
            font_map->m_VectorCurveTexelsPerCurve = 2;
            return true;
        }

        dmLogError("Vector font %s requires RGBA16F or RGBA32F texture support",
                   dmHashReverseSafe64(font_map->m_NameHash));
        font_map->m_VectorCurveFormat = dmGraphics::TEXTURE_FORMAT_RGBA;
        font_map->m_VectorCurveComponentSize = 0;
        font_map->m_VectorCurveTexelsPerCurve = 0;
        return false;
    }

    FontMapParams::FontMapParams()
    : m_FontCollection(0)
    , m_NameHash(0)
    , m_OnGlyphCacheMiss(0)
    , m_OnGlyphCacheMissContext(0)
    , m_Size(0.0f)
    , m_ShadowX(0.0f)
    , m_ShadowY(0.0f)
    , m_ShadowBlur(0.0f)
    , m_MaxAscent(0.0f)
    , m_MaxDescent(0.0f)
    , m_SdfSpread(1.0f)
    , m_SdfOutline(0)
    , m_SdfShadow(0)
    , m_Alpha(1.0f)
    , m_OutlineAlpha(0.0f)
    , m_OutlineWidth(0.0f)
    , m_ShadowAlpha(0.0f)
    , m_CacheWidth(0)
    , m_CacheHeight(0)
    , m_CacheCellWidth(0)
    , m_CacheCellHeight(0)
    , m_GlyphChannels(1)
    , m_CacheCellPadding(0)
    , m_LayerMask(FACE)
    , m_IsMonospaced(false)
    , m_ShadowSdf(false)
    , m_DebugGlyphBBoxes(false)
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
        memset(font_map->m_Cache, 0, sizeof(CacheGlyph) * font_map->m_CacheCellCount);
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

    static void RecreateTextureWithData(dmGraphics::HContext graphics_context,
                                        dmGraphics::HTexture* texture,
                                        uint32_t width,
                                        uint32_t height,
                                        dmGraphics::TextureFormat format,
                                        dmGraphics::TextureFilter min_filter,
                                        dmGraphics::TextureFilter mag_filter,
                                        const void* data,
                                        uint32_t data_size)
    {
        dmGraphics::TextureCreationParams tex_create_params;
        tex_create_params.m_Width = width;
        tex_create_params.m_Height = height;
        tex_create_params.m_OriginalWidth = width;
        tex_create_params.m_OriginalHeight = height;

        if (*texture)
        {
            dmGraphics::DeleteTexture(graphics_context, *texture);
        }

        *texture = dmGraphics::NewTexture(graphics_context, tex_create_params);

        dmGraphics::TextureParams tex_params;
        tex_params.m_Format = format;
        tex_params.m_Width = width;
        tex_params.m_Height = height;
        tex_params.m_Depth = 1;
        tex_params.m_MinFilter = min_filter;
        tex_params.m_MagFilter = mag_filter;
        tex_params.m_DataSize = data_size;
        tex_params.m_Data = data;

        dmGraphics::SetTexture(graphics_context, *texture, tex_params);
    }

    static bool UsesVectorSdfShadow(HFontMap font_map)
    {
        return font_map->m_ShadowSdf &&
               font_map->m_ShadowAlpha > 0.0f;
    }

    static void RecreateVectorSdfTexture(HFontMap font_map)
    {
        uint32_t width = UsesVectorSdfShadow(font_map) ? font_map->m_CacheWidth : 1;
        uint32_t height = UsesVectorSdfShadow(font_map) ? font_map->m_CacheHeight : 1;
        uint32_t data_size = width * height;
        uint8_t* data = (uint8_t*)calloc(data_size, 1);

        RecreateTextureWithData(font_map->m_GraphicsContext,
                                &font_map->m_VectorSdfTexture,
                                width,
                                height,
                                dmGraphics::TEXTURE_FORMAT_LUMINANCE,
                                dmGraphics::TEXTURE_FILTER_LINEAR,
                                dmGraphics::TEXTURE_FILTER_LINEAR,
                                data,
                                data_size);
        free(data);
    }

    static void CreateVectorTextures(HFontMap font_map)
    {
        font_map->m_VectorCurveCapacity = VECTOR_CURVE_TEXTURE_WIDTH * VECTOR_CURVE_TEXTURE_HEIGHT;
        font_map->m_VectorCurveCursor = 0;
        font_map->m_VectorBandCapacity = 0;
        font_map->m_VectorBandCursor = 0;

        uint32_t curve_component_count = font_map->m_VectorCurveCapacity * 4;

        free(font_map->m_VectorCurveData);
        font_map->m_VectorCurveData = 0;
        free(font_map->m_VectorBandData);
        font_map->m_VectorBandData = 0;
        if (font_map->m_VectorBandTexture && font_map->m_VectorRenderer != FONT_RENDERER_SLUG)
        {
            dmGraphics::DeleteTexture(font_map->m_GraphicsContext, font_map->m_VectorBandTexture);
            font_map->m_VectorBandTexture = 0;
        }

        if (!SelectVectorCurveTextureFormat(font_map))
        {
            font_map->m_VectorCurveCapacity = 0;
            font_map->m_VectorCurveCursor = 0;
            return;
        }

        font_map->m_VectorCurveData = malloc(font_map->m_VectorCurveComponentSize * curve_component_count);

        if (!font_map->m_VectorCurveData)
        {
            dmLogError("Failed to allocate vector font textures for %s", dmHashReverseSafe64(font_map->m_NameHash));
            free(font_map->m_VectorCurveData);
            font_map->m_VectorCurveData = 0;
            font_map->m_VectorCurveCapacity = 0;
            font_map->m_VectorCurveCursor = 0;
            font_map->m_VectorCurveComponentSize = 0;
            font_map->m_VectorCurveTexelsPerCurve = 0;
            return;
        }

        memset(font_map->m_VectorCurveData, 0, font_map->m_VectorCurveComponentSize * curve_component_count);

        RecreateTextureWithData(font_map->m_GraphicsContext,
                                &font_map->m_Texture,
                                VECTOR_CURVE_TEXTURE_WIDTH,
                                VECTOR_CURVE_TEXTURE_HEIGHT,
                                font_map->m_VectorCurveFormat,
                                dmGraphics::TEXTURE_FILTER_NEAREST,
                                dmGraphics::TEXTURE_FILTER_NEAREST,
                                font_map->m_VectorCurveData,
                                font_map->m_VectorCurveComponentSize * curve_component_count);

        if (font_map->m_VectorRenderer == FONT_RENDERER_SLUG)
        {
            font_map->m_VectorBandCapacity = VECTOR_BAND_TEXTURE_WIDTH * VECTOR_BAND_TEXTURE_HEIGHT;
            uint32_t band_float_count = font_map->m_VectorBandCapacity * 4;
            font_map->m_VectorBandData = (float*)calloc(band_float_count, sizeof(float));
            if (!font_map->m_VectorBandData)
            {
                dmLogError("Failed to allocate Slug band texture for %s", dmHashReverseSafe64(font_map->m_NameHash));
                font_map->m_VectorBandCapacity = 0;
                return;
            }
            RecreateTextureWithData(font_map->m_GraphicsContext,
                                    &font_map->m_VectorBandTexture,
                                    VECTOR_BAND_TEXTURE_WIDTH,
                                    VECTOR_BAND_TEXTURE_HEIGHT,
                                    dmGraphics::TEXTURE_FORMAT_RGBA32F,
                                    dmGraphics::TEXTURE_FILTER_NEAREST,
                                    dmGraphics::TEXTURE_FILTER_NEAREST,
                                    font_map->m_VectorBandData,
                                    sizeof(float) * band_float_count);
        }
        RecreateVectorSdfTexture(font_map);
    }

    static void RestoreLegacyTexture(HFontMap font_map)
    {
        free(font_map->m_VectorCurveData);
        font_map->m_VectorCurveData = 0;
        free(font_map->m_VectorBandData);
        font_map->m_VectorBandData = 0;

        font_map->m_VectorCurveCapacity = 0;
        font_map->m_VectorCurveCursor = 0;
        font_map->m_VectorBandCapacity = 0;
        font_map->m_VectorBandCursor = 0;
        font_map->m_VectorCurveComponentSize = 0;
        font_map->m_VectorCurveTexelsPerCurve = 0;

        if (font_map->m_VectorSdfTexture)
        {
            dmGraphics::DeleteTexture(font_map->m_GraphicsContext, font_map->m_VectorSdfTexture);
            font_map->m_VectorSdfTexture = 0;
        }

        if (font_map->m_VectorBandTexture)
        {
            dmGraphics::DeleteTexture(font_map->m_GraphicsContext, font_map->m_VectorBandTexture);
            font_map->m_VectorBandTexture = 0;
        }

        RecreateTexture(font_map, font_map->m_GraphicsContext, font_map->m_CacheWidth, font_map->m_CacheHeight);
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
        font_map->m_ShadowBlur = params.m_ShadowBlur;
        font_map->m_MaxAscent = params.m_MaxAscent;
        font_map->m_MaxDescent = params.m_MaxDescent;
        font_map->m_SdfSpread = params.m_SdfSpread;
        font_map->m_SdfOutline = params.m_SdfOutline;
        font_map->m_SdfShadow = params.m_SdfShadow;
        font_map->m_Alpha = params.m_Alpha;
        font_map->m_OutlineAlpha = params.m_OutlineAlpha;
        font_map->m_OutlineWidth = params.m_OutlineWidth;
        font_map->m_ShadowAlpha = params.m_ShadowAlpha;
        font_map->m_LayerMask = params.m_LayerMask;
        font_map->m_IsMonospaced = params.m_IsMonospaced;
        font_map->m_IsDynamic = params.m_IsDynamic;
        font_map->m_ShadowSdf = params.m_ShadowSdf;
        font_map->m_DebugGlyphBBoxes = params.m_DebugGlyphBBoxes;
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
            font_map->m_IsSdf = 0;
        }
        else // Distance-field font
        {
            font_map->m_MinFilter = dmGraphics::TEXTURE_FILTER_LINEAR;
            font_map->m_MagFilter = dmGraphics::TEXTURE_FILTER_LINEAR;
            font_map->m_IsSdf = 1;
        }

        font_map->m_GraphicsContext = graphics_context;
        font_map->m_IsVector = 0;
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

    dmGraphics::HTexture GetFontMapBandTexture(HFontMap font_map)
    {
        DM_MUTEX_SCOPED_LOCK(font_map->m_Mutex);
        return font_map->m_VectorBandTexture;
    }

    void SetFontMapMaterial(HFontMap font_map, HMaterial material)
    {
        DM_MUTEX_SCOPED_LOCK(font_map->m_Mutex);
        font_map->m_Material = material;

        FontRendererType renderer = FONT_RENDERER_SDF;
        if (material)
        {
            if (GetMaterialSamplerUnit(material, CURVE_TEXTURE_HASH) != INVALID_SAMPLER_UNIT)
            {
                renderer = GetMaterialSamplerUnit(material, BAND_TEXTURE_HASH) != INVALID_SAMPLER_UNIT
                    ? FONT_RENDERER_SLUG
                    : FONT_RENDERER_SWEEP;
            }
        }

        if (renderer != FONT_RENDERER_SDF)
        {
            font_map->m_VectorRenderer = renderer;
            CreateVectorTextures(font_map);
        }
        else if (font_map->m_IsVector)
        {
            RestoreLegacyTexture(font_map);
        }

        font_map->m_IsVector = renderer != FONT_RENDERER_SDF ? 1 : 0;
        font_map->m_VectorRenderer = renderer;
    }

    HMaterial GetFontMapMaterial(HFontMap font_map)
    {
        DM_MUTEX_SCOPED_LOCK(font_map->m_Mutex);
        return font_map->m_Material;
    }

    bool GetFontMapIsVector(HFontMap font_map)
    {
        DM_MUTEX_SCOPED_LOCK(font_map->m_Mutex);
        return font_map->m_IsVector != 0;
    }

    float GetFontMapSdfSpread(HFontMap font_map)
    {
        DM_MUTEX_SCOPED_LOCK(font_map->m_Mutex);
        return font_map->m_SdfSpread;
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

    void GetTextMetrics(HFontMap font_map, HTextLayout layout, TextMetrics* metrics)
    {
        metrics->m_Width = 0.0f;
        metrics->m_Height = 0.0f;
        metrics->m_LineCount = 0;
        {
            DM_MUTEX_SCOPED_LOCK(font_map->m_Mutex);
            metrics->m_MaxAscent = font_map->m_MaxAscent;
            metrics->m_MaxDescent = font_map->m_MaxDescent;
        }

        if (layout)
        {
            TextLayoutGetBounds(layout, &metrics->m_Width, &metrics->m_Height);
            metrics->m_LineCount = TextLayoutGetLineCount(layout);
        }
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
            if (font_map->m_IsVector)
            {
                FontGlyphOptions glyph_options;
                glyph_options.m_Scale = FontGetScaleFromSize(font, font_map->m_Size);
                glyph_options.m_GenerateImage = UsesVectorSdfShadow(font_map);
                glyph_options.m_GenerateOutline = true;
                glyph_options.m_StbttSDFPadding = font_map->m_SdfSpread;

                FontGlyph temp;
                r = FontGetGlyphByIndex(font, glyph_index, &glyph_options, &temp);
                if (FONT_RESULT_OK != r)
                {
                    return r;
                }

                FontGlyph* out = new FontGlyph;
                *out = temp;

                temp.m_Outline.m_Commands = 0;
                temp.m_Outline.m_CommandCount = 0;
                temp.m_Outline.m_Flags = 0;
                if (UsesVectorSdfShadow(font_map))
                {
                    temp.m_Bitmap.m_Data = 0;
                    temp.m_Bitmap.m_DataSize = 0;
                    temp.m_Bitmap.m_Channels = 0;
                    temp.m_Bitmap.m_Flags = 0;
                }
                FontFreeGlyph(font, &temp);

                if (!UsesVectorSdfShadow(font_map))
                {
                    out->m_Bitmap.m_Data = 0;
                    out->m_Bitmap.m_DataSize = 0;
                    out->m_Bitmap.m_Channels = 0;
                    out->m_Bitmap.m_Flags = 0;
                }

                *glyph = out;
                AddGlyph(font_map, key, *glyph);
                return r;
            }

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
            if (font_map->m_IsVector)
            {
                if (font_map->m_IsCacheSizeTooSmall)
                {
                    GetNextCacheSize(font_map, &font_map->m_CacheWidth, &font_map->m_CacheHeight);
                    font_map->m_IsCacheSizeTooSmall = 0;
                }

                font_map->m_CacheWidth = dmMath::Max(font_map->m_CacheWidth, font_map->m_CacheCellWidth);
                font_map->m_CacheHeight = dmMath::Max(font_map->m_CacheHeight, font_map->m_CacheCellHeight);
                if (!IsPowerOfTwo(font_map->m_CacheWidth))
                    font_map->m_CacheWidth = NextPowerOfTwo(font_map->m_CacheWidth);
                if (!IsPowerOfTwo(font_map->m_CacheHeight))
                    font_map->m_CacheHeight = NextPowerOfTwo(font_map->m_CacheHeight);

                SetupCache(font_map,
                           font_map->m_CacheWidth,
                           font_map->m_CacheHeight,
                           font_map->m_CacheCellWidth,
                           font_map->m_CacheCellHeight,
                           font_map->m_CacheCellMaxAscent);
                ResetVectorCache(font_map);
                font_map->m_IsCacheSizeDirty = 0;
                return;
            }

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

    static bool UpdateVectorSdfGlyphTexture(HFontMap font_map, FontGlyph* glyph, int32_t x, int32_t y)
    {
        if (!font_map->m_VectorSdfTexture || !glyph->m_Bitmap.m_Data)
        {
            return false;
        }

        uint32_t width = glyph->m_Bitmap.m_Width;
        uint32_t height = glyph->m_Bitmap.m_Height;
        uint32_t channels = glyph->m_Bitmap.m_Channels;
        const uint8_t* source = glyph->m_Bitmap.m_Data;
        uint8_t* unpacked = font_map->m_CellTempData;

        if ((glyph->m_Bitmap.m_Flags & FONT_GLYPH_COMPRESSION_DEFLATE) != 0)
        {
            FontGlyphInflaterContext inflate_context;
            inflate_context.m_Output = unpacked;
            inflate_context.m_Cursor = 0;
            dmZlib::Result result = dmZlib::InflateBuffer(glyph->m_Bitmap.m_Data,
                                                          glyph->m_Bitmap.m_DataSize,
                                                          &inflate_context,
                                                          FontGlyphInflater);
            if (result != dmZlib::RESULT_OK)
            {
                dmLogError("Failed to decompress vector SDF glyph %u in font %s: %d",
                           glyph->m_GlyphIndex,
                           dmHashReverseSafe64(font_map->m_NameHash),
                           result);
                return false;
            }
            delta_decode(unpacked, inflate_context.m_Cursor);
            source = unpacked;
        }

        if (channels != 1)
        {
            for (uint32_t i = 0; i < width * height; ++i)
            {
                unpacked[i] = source[i * channels];
            }
            source = unpacked;
        }

        dmGraphics::TextureParams tex_params;
        memset(&tex_params, 0, sizeof(tex_params));
        tex_params.m_SubUpdate = true;
        tex_params.m_MipMap = 0;
        tex_params.m_Format = dmGraphics::TEXTURE_FORMAT_LUMINANCE;
        tex_params.m_MinFilter = dmGraphics::TEXTURE_FILTER_LINEAR;
        tex_params.m_MagFilter = dmGraphics::TEXTURE_FILTER_LINEAR;
        tex_params.m_Width = width;
        tex_params.m_Height = height;
        tex_params.m_Depth = 1;
        tex_params.m_X = x;
        tex_params.m_Y = y;
        tex_params.m_Data = source;
        tex_params.m_DataSize = width * height;
        dmGraphics::SetTexture(font_map->m_GraphicsContext, font_map->m_VectorSdfTexture, tex_params);
        return true;
    }

    static void UpdateVectorTexture(HFontMap font_map,
                                    dmGraphics::HTexture texture,
                                    dmGraphics::TextureFormat format,
                                    uint32_t width,
                                    uint32_t height,
                                    const void* data,
                                    uint32_t data_size)
    {
        dmGraphics::TextureParams tex_params;
        memset(&tex_params, 0, sizeof(tex_params));
        tex_params.m_Format = format;
        tex_params.m_Width = width;
        tex_params.m_Height = height;
        tex_params.m_Depth = 1;
        tex_params.m_MinFilter = dmGraphics::TEXTURE_FILTER_NEAREST;
        tex_params.m_MagFilter = dmGraphics::TEXTURE_FILTER_NEAREST;
        tex_params.m_Data = data;
        tex_params.m_DataSize = data_size;
        dmGraphics::SetTexture(font_map->m_GraphicsContext, texture, tex_params);
    }

    static void UploadVectorTextures(HFontMap font_map)
    {
        if (!font_map->m_Texture)
        {
            return;
        }

        UpdateVectorTexture(font_map,
                            font_map->m_Texture,
                            font_map->m_VectorCurveFormat,
                            VECTOR_CURVE_TEXTURE_WIDTH,
                            VECTOR_CURVE_TEXTURE_HEIGHT,
                            font_map->m_VectorCurveData,
                            font_map->m_VectorCurveComponentSize * font_map->m_VectorCurveCapacity * 4);

        if (font_map->m_VectorBandTexture && font_map->m_VectorBandData)
        {
            UpdateVectorTexture(font_map,
                                font_map->m_VectorBandTexture,
                                dmGraphics::TEXTURE_FORMAT_RGBA32F,
                                VECTOR_BAND_TEXTURE_WIDTH,
                                VECTOR_BAND_TEXTURE_HEIGHT,
                                font_map->m_VectorBandData,
                                sizeof(float) * font_map->m_VectorBandCapacity * 4);
        }
    }

    static void ResetVectorCache(HFontMap font_map)
    {
        font_map->m_GlyphCache.Clear();
        font_map->m_CacheCursor = 0;
        font_map->m_VectorCurveCursor = 0;
        font_map->m_VectorBandCursor = 0;

        for (uint32_t i = 0; i < font_map->m_CacheCellCount; ++i)
        {
            CacheGlyph* glyph = &font_map->m_Cache[i];
            glyph->m_Glyph = 0;
            glyph->m_Frame = 0;
            glyph->m_GlyphKey = 0;
            glyph->m_VectorCurveTexel = 0;
            glyph->m_VectorCurveTexelCount = 0;
            glyph->m_VectorCurveCount = 0;
            glyph->m_VectorStripeTexel = 0;
            glyph->m_VectorStripeCount = 0;
            glyph->m_VectorBandIndex = 0;
            glyph->m_VectorBandMaxX = 0;
            glyph->m_VectorBandMaxY = 0;
            glyph->m_VectorBandScaleX = 0.0f;
            glyph->m_VectorBandScaleY = 0.0f;
            glyph->m_VectorBandOffsetX = 0.0f;
            glyph->m_VectorBandOffsetY = 0.0f;
            glyph->m_VectorSdfCached = 0;
        }

        if (font_map->m_VectorCurveData)
            memset(font_map->m_VectorCurveData, 0, font_map->m_VectorCurveComponentSize * font_map->m_VectorCurveCapacity * 4);
        if (font_map->m_VectorBandData)
            memset(font_map->m_VectorBandData, 0, sizeof(float) * font_map->m_VectorBandCapacity * 4);

        if (font_map->m_VectorCurveData || font_map->m_VectorBandData)
            UploadVectorTextures(font_map);
        RecreateVectorSdfTexture(font_map);
    }

    static bool IsSameOutlinePoint(const FontCurvePoint& a, const FontCurvePoint& b)
    {
        return fabsf(a.m_X - b.m_X) < 0.0001f && fabsf(a.m_Y - b.m_Y) < 0.0001f;
    }

    static FontCurvePoint MakeMidpoint(const FontCurvePoint& a, const FontCurvePoint& b)
    {
        FontCurvePoint point;
        point.m_X = (a.m_X + b.m_X) * 0.5f;
        point.m_Y = (a.m_Y + b.m_Y) * 0.5f;
        return point;
    }

    static FontCurvePoint NormalizeOutlinePoint(const FontGlyph* glyph, const FontCurvePoint& point)
    {
        FontCurvePoint out = point;

        float width = dmMath::Max(0.0001f, glyph->m_Width);
        float height = dmMath::Max(0.0001f, glyph->m_Ascent + glyph->m_Descent);

        out.m_X = (point.m_X - glyph->m_LeftBearing) / width;
        out.m_Y = (point.m_Y + glyph->m_Descent) / height;
        return out;
    }

    struct EncodedVectorCurve
    {
        FontCurvePoint m_P0;
        FontCurvePoint m_P1;
        FontCurvePoint m_P2;
        float          m_MinX;
        float          m_MinY;
        float          m_MaxX;
        float          m_MaxY;
        float          m_StartTangentAngle;
        float          m_EndTangentAngle;
        uint16_t       m_CurveTexel;
    };

    static void StoreEncodedQuadratic(HFontMap font_map,
                                      uint32_t curve_texel,
                                      const EncodedVectorCurve& curve)
    {
        uint32_t texel0 = curve_texel * 4;
        uint32_t texel1 = (curve_texel + 1) * 4;

        if (font_map->m_VectorCurveFormat == dmGraphics::TEXTURE_FORMAT_RGBA16F)
        {
            uint16_t* curve_data = (uint16_t*) font_map->m_VectorCurveData;
            curve_data[texel0 + 0] = FloatToHalf(curve.m_P0.m_X);
            curve_data[texel0 + 1] = FloatToHalf(curve.m_P0.m_Y);
            curve_data[texel0 + 2] = FloatToHalf(curve.m_P1.m_X);
            curve_data[texel0 + 3] = FloatToHalf(curve.m_P1.m_Y);

            curve_data[texel1 + 0] = FloatToHalf(curve.m_P2.m_X);
            curve_data[texel1 + 1] = FloatToHalf(curve.m_P2.m_Y);
            curve_data[texel1 + 2] = FloatToHalf(curve.m_StartTangentAngle);
            curve_data[texel1 + 3] = FloatToHalf(curve.m_EndTangentAngle);
        }
        else if (font_map->m_VectorCurveFormat == dmGraphics::TEXTURE_FORMAT_RGBA32F)
        {
            float* curve_data = (float*) font_map->m_VectorCurveData;
            curve_data[texel0 + 0] = curve.m_P0.m_X;
            curve_data[texel0 + 1] = curve.m_P0.m_Y;
            curve_data[texel0 + 2] = curve.m_P1.m_X;
            curve_data[texel0 + 3] = curve.m_P1.m_Y;

            curve_data[texel1 + 0] = curve.m_P2.m_X;
            curve_data[texel1 + 1] = curve.m_P2.m_Y;
            curve_data[texel1 + 2] = curve.m_StartTangentAngle;
            curve_data[texel1 + 3] = curve.m_EndTangentAngle;
        }
        else
        {
            dmLogError("Unsupported vector curve texture format for vector font %s",
                       dmHashReverseSafe64(font_map->m_NameHash));
        }
    }

    static void StoreVectorStripeMetadata(HFontMap font_map,
                                          uint32_t metadata_texel,
                                          uint32_t relative_curve_texel,
                                          uint32_t curve_count)
    {
        uint32_t texel = metadata_texel * 4;
        if (font_map->m_VectorCurveFormat == dmGraphics::TEXTURE_FORMAT_RGBA16F)
        {
            uint16_t* curve_data = (uint16_t*) font_map->m_VectorCurveData;
            curve_data[texel + 0] = FloatToHalf((float)relative_curve_texel);
            curve_data[texel + 1] = FloatToHalf((float)curve_count);
            curve_data[texel + 2] = 0;
            curve_data[texel + 3] = 0;
        }
        else if (font_map->m_VectorCurveFormat == dmGraphics::TEXTURE_FORMAT_RGBA32F)
        {
            float* curve_data = (float*) font_map->m_VectorCurveData;
            curve_data[texel + 0] = (float)relative_curve_texel;
            curve_data[texel + 1] = (float)curve_count;
            curve_data[texel + 2] = 0.0f;
            curve_data[texel + 3] = 0.0f;
        }
    }

    static bool BuildVectorScanlineStripes(const std::vector<EncodedVectorCurve>& curves,
                                           std::vector<EncodedVectorCurve>* stripes,
                                           uint32_t* stripe_curve_count)
    {
        const float stripe_height = 1.0f / (float)VECTOR_SCANLINE_STRIPE_COUNT;
        uint32_t total_curve_references = 0;
        uint32_t max_stripe_curves = 0;

        for (uint32_t stripe = 0; stripe < VECTOR_SCANLINE_STRIPE_COUNT; ++stripe)
        {
            float stripe_min_y = (float)stripe * stripe_height;
            float stripe_max_y = (float)(stripe + 1) * stripe_height;
            for (uint32_t curve_index = 0; curve_index < curves.size(); ++curve_index)
            {
                const EncodedVectorCurve& curve = curves[curve_index];
                if (curve.m_MaxY >= stripe_min_y && curve.m_MinY <= stripe_max_y)
                {
                    stripes[stripe].push_back(curve);
                }
            }
            total_curve_references += stripes[stripe].size();
            max_stripe_curves = dmMath::Max(max_stripe_curves, (uint32_t)stripes[stripe].size());
        }

        *stripe_curve_count = total_curve_references;
        uint32_t curve_count = curves.size();
        return curve_count >= 12 &&
               total_curve_references <= curve_count * 2 &&
               max_stripe_curves * 2 <= curve_count;
    }

    static void PushEncodedQuadratic(std::vector<EncodedVectorCurve>& curves,
                                     const FontGlyph* glyph,
                                     const FontCurvePoint& p0,
                                     const FontCurvePoint& p1,
                                     const FontCurvePoint& p2)
    {
        EncodedVectorCurve curve;
        curve.m_P0 = NormalizeOutlinePoint(glyph, p0);
        curve.m_P1 = NormalizeOutlinePoint(glyph, p1);
        curve.m_P2 = NormalizeOutlinePoint(glyph, p2);
        curve.m_MinX = dmMath::Min(curve.m_P0.m_X, dmMath::Min(curve.m_P1.m_X, curve.m_P2.m_X));
        curve.m_MinY = dmMath::Min(curve.m_P0.m_Y, dmMath::Min(curve.m_P1.m_Y, curve.m_P2.m_Y));
        curve.m_MaxX = dmMath::Max(curve.m_P0.m_X, dmMath::Max(curve.m_P1.m_X, curve.m_P2.m_X));
        curve.m_MaxY = dmMath::Max(curve.m_P0.m_Y, dmMath::Max(curve.m_P1.m_Y, curve.m_P2.m_Y));
        curve.m_StartTangentAngle = 0.0f;
        curve.m_EndTangentAngle = 0.0f;
        curve.m_CurveTexel = 0;
        curves.push_back(curve);
    }

    static bool FindInteriorAxisExtremum(float p0, float p1, float p2, float* t)
    {
        const float epsilon = 0.0001f;
        float denominator = p0 - 2.0f * p1 + p2;
        if (fabsf(denominator) < 0.000001f)
        {
            return false;
        }

        float extremum = (p0 - p1) / denominator;
        if (extremum <= epsilon || extremum >= 1.0f - epsilon)
        {
            return false;
        }

        *t = extremum;
        return true;
    }

    static bool IsQuadraticMonotonic(const FontCurvePoint& p0,
                                     const FontCurvePoint& p1,
                                     const FontCurvePoint& p2)
    {
        float t = 0.0f;
        return !FindInteriorAxisExtremum(p0.m_X, p1.m_X, p2.m_X, &t) &&
               !FindInteriorAxisExtremum(p0.m_Y, p1.m_Y, p2.m_Y, &t);
    }

    static void AddUniqueSplit(float* splits, uint32_t* split_count, float t)
    {
        const float epsilon = 0.0001f;
        if (t <= epsilon || t >= 1.0f - epsilon)
        {
            return;
        }

        for (uint32_t i = 0; i < *split_count; ++i)
        {
            if (fabsf(splits[i] - t) < epsilon)
            {
                return;
            }
        }

        splits[(*split_count)++] = t;
    }

    static void AddAxisExtremumSplit(float* splits, uint32_t* split_count, float p0, float p1, float p2)
    {
        float t = 0.0f;
        if (FindInteriorAxisExtremum(p0, p1, p2, &t))
        {
            AddUniqueSplit(splits, split_count, t);
        }
    }

    static void SortSplits(float* splits, uint32_t split_count)
    {
        for (uint32_t i = 1; i < split_count; ++i)
        {
            float value = splits[i];
            uint32_t j = i;
            while (j > 0 && splits[j - 1] > value)
            {
                splits[j] = splits[j - 1];
                --j;
            }
            splits[j] = value;
        }
    }

    static FontCurvePoint LerpOutlinePoint(const FontCurvePoint& a, const FontCurvePoint& b, float t)
    {
        FontCurvePoint point;
        point.m_X = a.m_X + (b.m_X - a.m_X) * t;
        point.m_Y = a.m_Y + (b.m_Y - a.m_Y) * t;
        return point;
    }

    static void SplitQuadraticDeCasteljau(const FontCurvePoint& p0,
                                          const FontCurvePoint& p1,
                                          const FontCurvePoint& p2,
                                          float t,
                                          FontCurvePoint* left_p0,
                                          FontCurvePoint* left_p1,
                                          FontCurvePoint* left_p2,
                                          FontCurvePoint* right_p0,
                                          FontCurvePoint* right_p1,
                                          FontCurvePoint* right_p2)
    {
        FontCurvePoint p01 = LerpOutlinePoint(p0, p1, t);
        FontCurvePoint p12 = LerpOutlinePoint(p1, p2, t);
        FontCurvePoint p012 = LerpOutlinePoint(p01, p12, t);

        *left_p0 = p0;
        *left_p1 = p01;
        *left_p2 = p012;

        *right_p0 = p012;
        *right_p1 = p12;
        *right_p2 = p2;
    }

    static void PushMonotonicEncodedQuadratic(std::vector<EncodedVectorCurve>& curves,
                                              const FontGlyph* glyph,
                                              const FontCurvePoint& p0,
                                              const FontCurvePoint& p1,
                                              const FontCurvePoint& p2)
    {
        if (IsQuadraticMonotonic(p0, p1, p2))
        {
            PushEncodedQuadratic(curves, glyph, p0, p1, p2);
            return;
        }

        float splits[4] = { 0.0f, 1.0f, 0.0f, 0.0f };
        uint32_t split_count = 2;
        AddAxisExtremumSplit(splits, &split_count, p0.m_X, p1.m_X, p2.m_X);
        AddAxisExtremumSplit(splits, &split_count, p0.m_Y, p1.m_Y, p2.m_Y);
        SortSplits(splits, split_count);

        FontCurvePoint remaining_p0 = p0;
        FontCurvePoint remaining_p1 = p1;
        FontCurvePoint remaining_p2 = p2;
        float previous_t = 0.0f;

        for (uint32_t i = 1; i + 1 < split_count; ++i)
        {
            float global_t = splits[i];
            float remaining_interval = 1.0f - previous_t;
            if (remaining_interval <= 0.0001f)
            {
                continue;
            }

            float local_t = (global_t - previous_t) / remaining_interval;
            local_t = dmMath::Clamp(local_t, 0.0f, 1.0f);

            FontCurvePoint left_p0;
            FontCurvePoint left_p1;
            FontCurvePoint left_p2;
            FontCurvePoint right_p0;
            FontCurvePoint right_p1;
            FontCurvePoint right_p2;
            SplitQuadraticDeCasteljau(remaining_p0,
                                      remaining_p1,
                                      remaining_p2,
                                      local_t,
                                      &left_p0,
                                      &left_p1,
                                      &left_p2,
                                      &right_p0,
                                      &right_p1,
                                      &right_p2);

            PushEncodedQuadratic(curves, glyph, left_p0, left_p1, left_p2);

            remaining_p0 = right_p0;
            remaining_p1 = right_p1;
            remaining_p2 = right_p2;
            previous_t = global_t;
        }

        PushEncodedQuadratic(curves, glyph, remaining_p0, remaining_p1, remaining_p2);
    }

    static bool ValidateEncodedQuadratics(HFontMap font_map, const FontGlyph* glyph, const std::vector<EncodedVectorCurve>& curves)
    {
        uint32_t violation_count = 0;
        const uint32_t max_logged_violations = 8;

        for (uint32_t i = 0; i < curves.size(); ++i)
        {
            const EncodedVectorCurve& curve = curves[i];
            float tx = 0.0f;
            float ty = 0.0f;
            bool non_monotonic_x = FindInteriorAxisExtremum(curve.m_P0.m_X, curve.m_P1.m_X, curve.m_P2.m_X, &tx);
            bool non_monotonic_y = FindInteriorAxisExtremum(curve.m_P0.m_Y, curve.m_P1.m_Y, curve.m_P2.m_Y, &ty);

            if (!non_monotonic_x && !non_monotonic_y)
            {
                continue;
            }

            if (violation_count < max_logged_violations)
            {
                dmLogWarning("Vector glyph %u in %s has non-monotonic encoded curve %u: x=%d tx=%.6f y=%d ty=%.6f p0=(%.6f, %.6f) p1=(%.6f, %.6f) p2=(%.6f, %.6f)",
                             glyph->m_GlyphIndex,
                             dmHashReverseSafe64(font_map->m_NameHash),
                             i,
                             non_monotonic_x ? 1 : 0,
                             tx,
                             non_monotonic_y ? 1 : 0,
                             ty,
                             curve.m_P0.m_X,
                             curve.m_P0.m_Y,
                             curve.m_P1.m_X,
                             curve.m_P1.m_Y,
                             curve.m_P2.m_X,
                             curve.m_P2.m_Y);
            }

            ++violation_count;
        }

        if (violation_count > 0)
        {
            dmLogWarning("Vector glyph %u in %s failed monotonic curve validation: %u of %u encoded curves are invalid. Skipping vector texture upload for this glyph.",
                         glyph->m_GlyphIndex,
                         dmHashReverseSafe64(font_map->m_NameHash),
                         violation_count,
                         (uint32_t)curves.size());
            return false;
        }

        if (font_map->m_DebugGlyphBBoxes)
        {
            printf("FONT_VECTOR_MONOTONIC font=%s glyph_index=%u curves=%u result=ok\n",
                   dmHashReverseSafe64(font_map->m_NameHash),
                   glyph->m_GlyphIndex,
                   (uint32_t)curves.size());
        }

        return true;
    }

    static void CollectEncodedQuadratics(const FontGlyph* glyph, std::vector<EncodedVectorCurve>& curves)
    {
        FontCurvePoint current = {0.0f, 0.0f};
        FontCurvePoint contour_start = {0.0f, 0.0f};
        bool has_current = false;
        bool has_contour = false;

        for (uint32_t i = 0; i < glyph->m_Outline.m_CommandCount; ++i)
        {
            const FontCurveCommand& command = glyph->m_Outline.m_Commands[i];
            switch (command.m_Type)
            {
                case FONT_CURVE_MOVE_TO:
                    current = command.m_Points[0];
                    contour_start = current;
                    has_current = true;
                    has_contour = true;
                    break;
                case FONT_CURVE_LINE_TO:
                    if (has_current)
                    {
                        FontCurvePoint next = command.m_Points[0];
                        PushMonotonicEncodedQuadratic(curves, glyph, current, MakeMidpoint(current, next), next);
                        current = next;
                    }
                    break;
                case FONT_CURVE_QUADRATIC_TO:
                    if (has_current)
                    {
                        PushMonotonicEncodedQuadratic(curves, glyph, current, command.m_Points[0], command.m_Points[1]);
                        current = command.m_Points[1];
                    }
                    break;
                case FONT_CURVE_CLOSE:
                    if (has_current && has_contour && !IsSameOutlinePoint(current, contour_start))
                    {
                        PushMonotonicEncodedQuadratic(curves, glyph, current, MakeMidpoint(current, contour_start), contour_start);
                    }
                    has_current = false;
                    has_contour = false;
                    break;
                default:
                    break;
            }
        }

    }

    static void CollectSlugEncodedQuadratics(const FontGlyph* glyph, std::vector<EncodedVectorCurve>& curves)
    {
        FontCurvePoint current = {0.0f, 0.0f};
        FontCurvePoint contour_start = {0.0f, 0.0f};
        bool has_current = false;
        bool has_contour = false;

        for (uint32_t i = 0; i < glyph->m_Outline.m_CommandCount; ++i)
        {
            const FontCurveCommand& command = glyph->m_Outline.m_Commands[i];
            switch (command.m_Type)
            {
                case FONT_CURVE_MOVE_TO:
                    current = command.m_Points[0];
                    contour_start = current;
                    has_current = true;
                    has_contour = true;
                    break;
                case FONT_CURVE_LINE_TO:
                    if (has_current)
                    {
                        FontCurvePoint next = command.m_Points[0];
                        PushEncodedQuadratic(curves, glyph, current, MakeMidpoint(current, next), next);
                        current = next;
                    }
                    break;
                case FONT_CURVE_QUADRATIC_TO:
                    if (has_current)
                    {
                        PushEncodedQuadratic(curves, glyph, current, command.m_Points[0], command.m_Points[1]);
                        current = command.m_Points[1];
                    }
                    break;
                case FONT_CURVE_CLOSE:
                    if (has_current && has_contour && !IsSameOutlinePoint(current, contour_start))
                    {
                        PushEncodedQuadratic(curves, glyph, current, MakeMidpoint(current, contour_start), contour_start);
                    }
                    has_current = false;
                    has_contour = false;
                    break;
                default:
                    break;
            }
        }
    }

    static FontCurvePoint NormalizeCurveDirection(const FontCurvePoint& direction)
    {
        float length_sq = direction.m_X * direction.m_X + direction.m_Y * direction.m_Y;
        if (length_sq <= 0.00000001f)
        {
            FontCurvePoint fallback = { 1.0f, 0.0f };
            return fallback;
        }

        float inv_length = 1.0f / sqrtf(length_sq);
        FontCurvePoint normalized = { direction.m_X * inv_length, direction.m_Y * inv_length };
        return normalized;
    }

    static FontCurvePoint CurveStartDirection(const EncodedVectorCurve& curve)
    {
        FontCurvePoint direction = { curve.m_P1.m_X - curve.m_P0.m_X,
                                     curve.m_P1.m_Y - curve.m_P0.m_Y };
        if (direction.m_X * direction.m_X + direction.m_Y * direction.m_Y <= 0.00000001f)
        {
            direction.m_X = curve.m_P2.m_X - curve.m_P0.m_X;
            direction.m_Y = curve.m_P2.m_Y - curve.m_P0.m_Y;
        }
        return NormalizeCurveDirection(direction);
    }

    static FontCurvePoint CurveEndDirection(const EncodedVectorCurve& curve)
    {
        FontCurvePoint direction = { curve.m_P2.m_X - curve.m_P1.m_X,
                                     curve.m_P2.m_Y - curve.m_P1.m_Y };
        if (direction.m_X * direction.m_X + direction.m_Y * direction.m_Y <= 0.00000001f)
        {
            direction.m_X = curve.m_P2.m_X - curve.m_P0.m_X;
            direction.m_Y = curve.m_P2.m_Y - curve.m_P0.m_Y;
        }
        return NormalizeCurveDirection(direction);
    }

    static bool IsSameEncodedPoint(const FontCurvePoint& a, const FontCurvePoint& b)
    {
        const float epsilon = 0.00001f;
        return fabsf(a.m_X - b.m_X) <= epsilon && fabsf(a.m_Y - b.m_Y) <= epsilon;
    }

    static void CalculateEncodedCurveJoinAngles(std::vector<EncodedVectorCurve>& curves)
    {
        for (uint32_t i = 0; i < curves.size(); ++i)
        {
            FontCurvePoint outgoing = CurveStartDirection(curves[i]);
            FontCurvePoint join = outgoing;
            for (uint32_t previous = 0; previous < curves.size(); ++previous)
            {
                if (previous == i || !IsSameEncodedPoint(curves[previous].m_P2, curves[i].m_P0))
                {
                    continue;
                }

                FontCurvePoint incoming = CurveEndDirection(curves[previous]);
                FontCurvePoint sum = { incoming.m_X + outgoing.m_X, incoming.m_Y + outgoing.m_Y };
                if (sum.m_X * sum.m_X + sum.m_Y * sum.m_Y > 0.00000001f)
                {
                    join = NormalizeCurveDirection(sum);
                }
                break;
            }
            curves[i].m_StartTangentAngle = atan2f(join.m_Y, join.m_X);
        }

        for (uint32_t i = 0; i < curves.size(); ++i)
        {
            FontCurvePoint end_direction = CurveEndDirection(curves[i]);
            curves[i].m_EndTangentAngle = atan2f(end_direction.m_Y, end_direction.m_X);
            for (uint32_t next = 0; next < curves.size(); ++next)
            {
                if (next != i && IsSameEncodedPoint(curves[i].m_P2, curves[next].m_P0))
                {
                    curves[i].m_EndTangentAngle = curves[next].m_StartTangentAngle;
                    break;
                }
            }
        }
    }

    struct CurveBandRef
    {
        uint32_t m_CurveIndex;
        float    m_SortKey;
    };

    static void CollectHorizontalBand(const std::vector<EncodedVectorCurve>& curves,
                                      float band_min_y,
                                      float band_max_y,
                                      std::vector<uint32_t>& out_curve_indices)
    {
        std::vector<CurveBandRef> refs;
        refs.reserve(curves.size());
        for (uint32_t i = 0; i < curves.size(); ++i)
        {
            const EncodedVectorCurve& curve = curves[i];
            if (curve.m_MaxY >= band_min_y && curve.m_MinY <= band_max_y)
            {
                CurveBandRef ref = { i, curve.m_MaxX };
                refs.push_back(ref);
            }
        }
        std::sort(refs.begin(), refs.end(), [](const CurveBandRef& a, const CurveBandRef& b) {
            return a.m_SortKey > b.m_SortKey;
        });
        for (uint32_t i = 0; i < refs.size(); ++i)
        {
            out_curve_indices.push_back(refs[i].m_CurveIndex);
        }
    }

    static void CollectVerticalBand(const std::vector<EncodedVectorCurve>& curves,
                                    float band_min_x,
                                    float band_max_x,
                                    std::vector<uint32_t>& out_curve_indices)
    {
        std::vector<CurveBandRef> refs;
        refs.reserve(curves.size());
        for (uint32_t i = 0; i < curves.size(); ++i)
        {
            const EncodedVectorCurve& curve = curves[i];
            if (curve.m_MaxX >= band_min_x && curve.m_MinX <= band_max_x)
            {
                CurveBandRef ref = { i, curve.m_MaxY };
                refs.push_back(ref);
            }
        }
        std::sort(refs.begin(), refs.end(), [](const CurveBandRef& a, const CurveBandRef& b) {
            return a.m_SortKey > b.m_SortKey;
        });
        for (uint32_t i = 0; i < refs.size(); ++i)
        {
            out_curve_indices.push_back(refs[i].m_CurveIndex);
        }
    }

    struct SlugBandData
    {
        std::vector< std::vector<uint32_t> > m_Horizontal;
        std::vector< std::vector<uint32_t> > m_Vertical;
        float m_MinX;
        float m_MinY;
        float m_Width;
        float m_Height;
        uint32_t m_RowWidth;
    };

    static bool BuildSlugBands(const std::vector<EncodedVectorCurve>& curves,
                               float min_x,
                               float min_y,
                               float max_x,
                               float max_y,
                               SlugBandData* bands)
    {
        uint8_t num_hbands = dmMath::Clamp((uint8_t)(curves.size() / 2), (uint8_t)1, (uint8_t)VECTOR_MAX_BANDS);
        uint8_t num_vbands = num_hbands;
        bands->m_Horizontal.resize(num_hbands);
        bands->m_Vertical.resize(num_vbands);
        bands->m_MinX = min_x;
        bands->m_MinY = min_y;
        bands->m_Width = dmMath::Max(0.0001f, max_x - min_x);
        bands->m_Height = dmMath::Max(0.0001f, max_y - min_y);

        uint32_t location_count = 0;
        float hband_height = bands->m_Height / (float)num_hbands;
        float vband_width = bands->m_Width / (float)num_vbands;
        for (uint32_t i = 0; i < num_hbands; ++i)
        {
            CollectHorizontalBand(curves,
                                  min_y + hband_height * i,
                                  min_y + hband_height * (i + 1),
                                  bands->m_Horizontal[i]);
            location_count += bands->m_Horizontal[i].size();
        }
        for (uint32_t i = 0; i < num_vbands; ++i)
        {
            CollectVerticalBand(curves,
                                min_x + vband_width * i,
                                min_x + vband_width * (i + 1),
                                bands->m_Vertical[i]);
            location_count += bands->m_Vertical[i].size();
        }
        bands->m_RowWidth = num_hbands + num_vbands + location_count;
        return bands->m_RowWidth <= VECTOR_BAND_TEXTURE_WIDTH;
    }

    static inline uint32_t GetVectorBandTexelOffset(uint32_t row, uint32_t column)
    {
        return (row * VECTOR_BAND_TEXTURE_WIDTH + column) * 4;
    }

    static void StoreSlugBands(HFontMap font_map,
                               CacheGlyph* cache_glyph,
                               const std::vector<EncodedVectorCurve>& curves,
                               const SlugBandData& bands)
    {
        uint16_t band_index = font_map->m_VectorBandCursor++;
        uint32_t horizontal_count = bands.m_Horizontal.size();
        uint32_t vertical_count = bands.m_Vertical.size();
        uint32_t location_cursor = horizontal_count + vertical_count;
        float* band_data = font_map->m_VectorBandData;

        for (uint32_t i = 0; i < horizontal_count; ++i)
        {
            uint32_t texel = GetVectorBandTexelOffset(band_index, i);
            band_data[texel] = (float)bands.m_Horizontal[i].size();
            band_data[texel + 1] = (float)location_cursor;
            for (uint32_t j = 0; j < bands.m_Horizontal[i].size(); ++j)
            {
                uint32_t location_texel = GetVectorBandTexelOffset(band_index, location_cursor++);
                band_data[location_texel] = (float)curves[bands.m_Horizontal[i][j]].m_CurveTexel;
            }
        }
        for (uint32_t i = 0; i < vertical_count; ++i)
        {
            uint32_t texel = GetVectorBandTexelOffset(band_index, horizontal_count + i);
            band_data[texel] = (float)bands.m_Vertical[i].size();
            band_data[texel + 1] = (float)location_cursor;
            for (uint32_t j = 0; j < bands.m_Vertical[i].size(); ++j)
            {
                uint32_t location_texel = GetVectorBandTexelOffset(band_index, location_cursor++);
                band_data[location_texel] = (float)curves[bands.m_Vertical[i][j]].m_CurveTexel;
            }
        }

        cache_glyph->m_VectorBandIndex = band_index;
        cache_glyph->m_VectorBandMaxX = vertical_count - 1;
        cache_glyph->m_VectorBandMaxY = horizontal_count - 1;
        cache_glyph->m_VectorBandScaleX = (float)vertical_count / bands.m_Width;
        cache_glyph->m_VectorBandScaleY = (float)horizontal_count / bands.m_Height;
        cache_glyph->m_VectorBandOffsetX = -bands.m_MinX * cache_glyph->m_VectorBandScaleX;
        cache_glyph->m_VectorBandOffsetY = -bands.m_MinY * cache_glyph->m_VectorBandScaleY;
    }

    static bool EncodeGlyphOutlineToVectorCache(HFontMap font_map, CacheGlyph* cache_glyph, FontGlyph* glyph)
    {
        if (!glyph->m_Outline.m_Commands || glyph->m_Outline.m_CommandCount == 0)
        {
            return false;
        }

        std::vector<EncodedVectorCurve> encoded_curves;
        if (font_map->m_VectorRenderer == FONT_RENDERER_SLUG)
        {
            CollectSlugEncodedQuadratics(glyph, encoded_curves);
        }
        else
        {
            CollectEncodedQuadratics(glyph, encoded_curves);
        }
        CalculateEncodedCurveJoinAngles(encoded_curves);
        uint32_t curve_count = encoded_curves.size();
        if (curve_count == 0)
        {
            return false;
        }
        if (font_map->m_VectorRenderer == FONT_RENDERER_SWEEP &&
            !ValidateEncodedQuadratics(font_map, glyph, encoded_curves))
        {
            return false;
        }
        if (curve_count > VECTOR_MAX_SHADER_CURVES)
        {
            dmLogWarning("The vector glyph %u in %s has %u monotonic curves, exceeding shader limit %u", glyph->m_GlyphIndex, dmHashReverseSafe64(font_map->m_NameHash), curve_count, VECTOR_MAX_SHADER_CURVES);
            return false;
        }

        uint32_t required_curve_texels = curve_count * font_map->m_VectorCurveTexelsPerCurve;
        std::vector<EncodedVectorCurve> scanline_stripes[VECTOR_SCANLINE_STRIPE_COUNT];
        uint32_t stripe_curve_count = 0;
        bool use_scanline_stripes = font_map->m_VectorRenderer == FONT_RENDERER_SWEEP &&
                                    BuildVectorScanlineStripes(encoded_curves,
                                                               scanline_stripes,
                                                               &stripe_curve_count);
        uint32_t stripe_texel_count = use_scanline_stripes
            ? VECTOR_SCANLINE_STRIPE_COUNT + stripe_curve_count * font_map->m_VectorCurveTexelsPerCurve
            : 0;
        required_curve_texels += stripe_texel_count;
        float min_x = encoded_curves[0].m_MinX;
        float min_y = encoded_curves[0].m_MinY;
        float max_x = encoded_curves[0].m_MaxX;
        float max_y = encoded_curves[0].m_MaxY;
        for (uint32_t i = 1; i < curve_count; ++i)
        {
            min_x = dmMath::Min(min_x, encoded_curves[i].m_MinX);
            min_y = dmMath::Min(min_y, encoded_curves[i].m_MinY);
            max_x = dmMath::Max(max_x, encoded_curves[i].m_MaxX);
            max_y = dmMath::Max(max_y, encoded_curves[i].m_MaxY);
        }

        SlugBandData slug_bands;
        bool use_slug_bands = font_map->m_VectorRenderer == FONT_RENDERER_SLUG;
        if (use_slug_bands && !BuildSlugBands(encoded_curves, min_x, min_y, max_x, max_y, &slug_bands))
        {
            dmLogWarning("The Slug band row is too wide to fit glyph %u in %s",
                         glyph->m_GlyphIndex,
                         dmHashReverseSafe64(font_map->m_NameHash));
            return false;
        }

        if (font_map->m_DebugGlyphBBoxes)
        {
            printf("FONT_VECTOR_OUTLINE_BOUNDS font=%s glyph_index=%u curves=%u width=%.3f height=%.3f ascent=%.3f descent=%.3f left_bearing=%.3f nx0=%.6f ny0=%.6f nx1=%.6f ny1=%.6f\n",
                   dmHashReverseSafe64(font_map->m_NameHash),
                   glyph->m_GlyphIndex,
                   curve_count,
                   glyph->m_Width,
                   glyph->m_Ascent + glyph->m_Descent,
                   glyph->m_Ascent,
                   glyph->m_Descent,
                   glyph->m_LeftBearing,
                   min_x,
                   min_y,
                   max_x,
                   max_y);
        }

        if (font_map->m_VectorCurveCursor + required_curve_texels > font_map->m_VectorCurveCapacity ||
            (use_slug_bands && font_map->m_VectorBandCursor >= VECTOR_BAND_TEXTURE_HEIGHT))
        {
            ResetVectorCache(font_map);
        }

        if (font_map->m_VectorCurveCursor + required_curve_texels > font_map->m_VectorCurveCapacity ||
            (use_slug_bands && font_map->m_VectorBandCursor >= VECTOR_BAND_TEXTURE_HEIGHT))
        {
            dmLogWarning("The vector font cache is too small to fit glyph %u in %s", glyph->m_GlyphIndex, dmHashReverseSafe64(font_map->m_NameHash));
            return false;
        }

        uint16_t curve_texel = font_map->m_VectorCurveCursor;

        for (uint32_t i = 0; i < encoded_curves.size(); ++i)
        {
            encoded_curves[i].m_CurveTexel = curve_texel + i * font_map->m_VectorCurveTexelsPerCurve;
            StoreEncodedQuadratic(font_map,
                                  encoded_curves[i].m_CurveTexel,
                                  encoded_curves[i]);
            if (font_map->m_DebugGlyphBBoxes)
            {
                printf("FONT_VECTOR_CURVE font=%s glyph_index=%u curve=%u p0=(%.6f,%.6f) p1=(%.6f,%.6f) p2=(%.6f,%.6f)\n",
                       dmHashReverseSafe64(font_map->m_NameHash),
                       glyph->m_GlyphIndex,
                       i,
                       encoded_curves[i].m_P0.m_X,
                       encoded_curves[i].m_P0.m_Y,
                       encoded_curves[i].m_P1.m_X,
                       encoded_curves[i].m_P1.m_Y,
                       encoded_curves[i].m_P2.m_X,
                       encoded_curves[i].m_P2.m_Y);
            }
        }

        uint16_t stripe_texel = 0;
        if (use_scanline_stripes)
        {
            stripe_texel = curve_texel + curve_count * font_map->m_VectorCurveTexelsPerCurve;
            uint32_t stripe_curve_texel = stripe_texel + VECTOR_SCANLINE_STRIPE_COUNT;
            for (uint32_t stripe = 0; stripe < VECTOR_SCANLINE_STRIPE_COUNT; ++stripe)
            {
                uint32_t relative_curve_texel = stripe_curve_texel - stripe_texel;
                StoreVectorStripeMetadata(font_map,
                                          stripe_texel + stripe,
                                          relative_curve_texel,
                                          scanline_stripes[stripe].size());
                for (uint32_t i = 0; i < scanline_stripes[stripe].size(); ++i)
                {
                    StoreEncodedQuadratic(font_map, stripe_curve_texel, scanline_stripes[stripe][i]);
                    stripe_curve_texel += font_map->m_VectorCurveTexelsPerCurve;
                }
            }
        }

        font_map->m_VectorCurveCursor += required_curve_texels;

        cache_glyph->m_VectorCurveTexel = curve_texel;
        cache_glyph->m_VectorCurveTexelCount = required_curve_texels;
        cache_glyph->m_VectorCurveCount = curve_count;
        cache_glyph->m_VectorStripeTexel = stripe_texel;
        cache_glyph->m_VectorStripeCount = use_scanline_stripes ? VECTOR_SCANLINE_STRIPE_COUNT : 0;
        if (use_slug_bands)
        {
            StoreSlugBands(font_map, cache_glyph, encoded_curves, slug_bands);
        }

        if (font_map->m_DebugGlyphBBoxes)
        {
            printf("FONT_VECTOR_STRIPES font=%s glyph_index=%u enabled=%u stripes=%u source_curves=%u stripe_curve_references=%u total_texels=%u\n",
                   dmHashReverseSafe64(font_map->m_NameHash),
                   glyph->m_GlyphIndex,
                   use_scanline_stripes ? 1 : 0,
                   use_scanline_stripes ? VECTOR_SCANLINE_STRIPE_COUNT : 0,
                   curve_count,
                   stripe_curve_count,
                   required_curve_texels);
        }

        UploadVectorTextures(font_map);
        return true;
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

    CacheGlyph* AddGlyphToCache(HFontMap font_map, uint32_t frame, uint64_t glyph_key, FontGlyph* glyph, int32_t g_offset_y)
    {
        DM_MUTEX_SCOPED_LOCK(font_map->m_Mutex);

        // Since accessing glyphs will update their timestamps, we need to sort them when
        // we need to allocate a new glyph, so that we pick the oldest one
        SortCache(font_map);
        //DebugCache(font_map);

        // Locate a cache cell candidate
        CacheGlyph* cache_glyph = AcquireFreeGlyphFromCache(font_map);

        if (cache_glyph->m_Glyph)
        {
            if (cache_glyph->m_Frame == frame)
            {
                bool can_resize = CanCacheTextureGrow(font_map);
                if (can_resize)
                {
                    font_map->m_IsCacheSizeDirty = 1;
                    font_map->m_IsCacheSizeTooSmall = 1;
                    return 0;
                }

                // It means we've filled the entire cache with upload requests
                // We might then just as well skip the next uploads until the next frame
                dmLogWarning("Entire font glyph cache (%u x %u) is filled in a single frame %u ('%c' %u / %u). Consider increasing the cache for %s", font_map->m_CacheWidth, font_map->m_CacheHeight, frame, glyph->m_Codepoint < 255 ? glyph->m_Codepoint : ' ', glyph->m_Codepoint, glyph->m_GlyphIndex  , dmHashReverseSafe64(font_map->m_NameHash));
                return 0;
            }
        }

        if (cache_glyph->m_Glyph) // It already existed in the cache
        {
            // Clear the old data from the cache
            font_map->m_GlyphCache.Erase(cache_glyph->m_GlyphKey);
        }

        if (font_map->m_IsVector)
        {
            uint32_t glyph_image_width = glyph->m_Bitmap.m_Width;
            uint32_t glyph_image_height = glyph->m_Bitmap.m_Height;
            if (UsesVectorSdfShadow(font_map) &&
                (glyph_image_width > font_map->m_CacheCellWidth ||
                 glyph_image_height > font_map->m_CacheCellHeight ||
                 glyph->m_Ascent > font_map->m_CacheCellMaxAscent))
            {
                font_map->m_CacheCellWidth = dmMath::Max(font_map->m_CacheCellWidth, (uint16_t)glyph_image_width);
                font_map->m_CacheCellHeight = dmMath::Max(font_map->m_CacheCellHeight, (uint16_t)glyph_image_height);
                font_map->m_CacheCellMaxAscent = dmMath::Max(font_map->m_CacheCellMaxAscent, (uint16_t)glyph->m_Ascent);
                font_map->m_IsCacheSizeDirty = 1;
                return 0;
            }

            cache_glyph->m_Glyph = glyph;
            cache_glyph->m_GlyphKey = glyph_key;
            cache_glyph->m_Frame = frame;
            cache_glyph->m_VectorSdfCached = 0;

            if (!EncodeGlyphOutlineToVectorCache(font_map, cache_glyph, glyph))
            {
                cache_glyph->m_Glyph = 0;
                cache_glyph->m_GlyphKey = 0;
                cache_glyph->m_Frame = 0;
                return 0;
            }

            if (UsesVectorSdfShadow(font_map))
            {
                if ((cache_glyph->m_X + glyph_image_width) > font_map->m_CacheWidth ||
                    (cache_glyph->m_Y + glyph_image_height) > font_map->m_CacheHeight)
                {
                    cache_glyph->m_Glyph = 0;
                    cache_glyph->m_GlyphKey = 0;
                    cache_glyph->m_Frame = 0;
                    font_map->m_IsCacheSizeDirty = 1;
                    font_map->m_IsCacheSizeTooSmall = 1;
                    return 0;
                }
                cache_glyph->m_VectorSdfCached =
                    UpdateVectorSdfGlyphTexture(font_map, glyph, cache_glyph->m_X, cache_glyph->m_Y) ? 1 : 0;
            }

            font_map->m_GlyphCache.Put(glyph_key, cache_glyph);
            return cache_glyph;
        }

        // If the blit would write outside of the texture, then we try to resize it
        uint32_t glyph_image_width  = glyph->m_Bitmap.m_Width;
        uint32_t glyph_image_height = glyph->m_Bitmap.m_Height;
        if ( (cache_glyph->m_X + glyph_image_width) > font_map->m_CacheWidth ||
             (cache_glyph->m_Y + glyph_image_height + g_offset_y) > font_map->m_CacheHeight)
        {
            bool can_resize = CanCacheTextureGrow(font_map);
            if (can_resize)
            {
                font_map->m_IsCacheSizeDirty = 1;
                font_map->m_IsCacheSizeTooSmall = 1;
                return 0;
            }

            // It means we've filled the entire cache with upload requests
            // We might then just as well skip the next uploads until the next frame
            dmLogWarning("The font glyph cache (%u x %u) needed resizing to fit glyph bitmap.", font_map->m_CacheWidth, font_map->m_CacheHeight);
            return 0;
        }

        cache_glyph->m_Glyph = glyph;
        cache_glyph->m_GlyphKey = glyph_key;
        cache_glyph->m_Frame = frame;

        font_map->m_GlyphCache.Put(glyph_key, cache_glyph);

        UpdateGlyphTexture(font_map, glyph, cache_glyph->m_X, cache_glyph->m_Y, g_offset_y);
        return cache_glyph;
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
        if (font_map->m_Texture)
            size += dmGraphics::GetTextureResourceSize(font_map->m_GraphicsContext, font_map->m_Texture);
        if (font_map->m_VectorSdfTexture)
            size += dmGraphics::GetTextureResourceSize(font_map->m_GraphicsContext, font_map->m_VectorSdfTexture);
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
