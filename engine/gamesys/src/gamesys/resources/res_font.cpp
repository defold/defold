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

#include "res_font.h"
#include "res_font_private.h"
#include "res_glyph_bank.h"
#include "res_ttf.h"
#include <gamesys/fontgen/fontgen.h>

#include <string.h>

#include <dlib/dstrings.h>
#include <dlib/log.h>

#include <render/font.h>
#include <render/font_renderer.h>
#include <render/render_ddf.h>

#include <dmsdk/gamesys/resources/res_material.h>

namespace dmGameSystem
{
    struct ImageDataHeader
    {
        uint8_t m_Compression; // FontGlyphCompression
    };

    template<typename T>
    static void SwapVar(T& a, T& b)
    {
        T tmp = a;
        a = b;
        b = tmp;
    }

    FontResource::FontResource()
    {
        memset(this, 0, sizeof(*this));
    }

    // Used when recreating a font
    void FontResource::Swap(FontResource* src)
    {
        SwapVar(m_DDF, src->m_DDF);
        SwapVar(m_FontMap, src->m_FontMap);
        SwapVar(m_Resource, src->m_Resource);
        SwapVar(m_MaterialResource, src->m_MaterialResource);
        SwapVar(m_GlyphBankResource, src->m_GlyphBankResource);
        SwapVar(m_TTFResource, src->m_TTFResource);
        SwapVar(m_Jobs, src->m_Jobs);
        SwapVar(m_CacheCellPadding, src->m_CacheCellPadding);
        SwapVar(m_IsDynamic, src->m_IsDynamic);
        SwapVar(m_Padding, src->m_Padding);
    }

    static void PrintDynamicGlyph(uint32_t codepoint, DynamicGlyph* glyph, FontResource* font);
    static void PrintGlyph(uint32_t codepoint, dmRenderDDF::GlyphBank::Glyph* glyph, FontResource* font);

    static void ReleaseResources(dmResource::HFactory factory, FontResource* resource)
    {
        if (resource->m_MaterialResource)
            dmResource::Release(factory, (void*) resource->m_MaterialResource);
        resource->m_MaterialResource = 0;
        if (resource->m_GlyphBankResource)
            dmResource::Release(factory, (void*) resource->m_GlyphBankResource);
        resource->m_GlyphBankResource = 0;

        // We don't release resource->m_TTFResource directly, as it's already part of the ranges below
        for (uint32_t i = 0; i < resource->m_Ranges.Size(); ++i)
        {
            GlyphRange& range = resource->m_Ranges[i];
            dmResource::Release(factory, (void*)range.m_TTFResource);
        }
        resource->m_Ranges.SetSize(0);

        if (resource->m_DDF)
            dmDDF::FreeMessage(resource->m_DDF);
    }

    static void AddFontRange(FontResource* resource, TTFResource* ttfresource, uint32_t range_start, uint32_t range_end)
    {
        if (resource->m_Ranges.Full())
            resource->m_Ranges.OffsetCapacity(4);

        GlyphRange range;
        range.m_RangeStart  = range_start;
        range.m_RangeEnd    = range_end;
        range.m_TTFResource = ttfresource;
        resource->m_Ranges.Push(range);
    }

    static void RemoveFontRange(FontResource* font, TTFResource* ttf)
    {
        for (uint32_t i = 0; i < font->m_Ranges.Size();)
        {
            GlyphRange* range = &font->m_Ranges[i];
            ++i;
            if (range->m_TTFResource == ttf)
            {
                --i;
                dmResource::Release(factory, (void*)range->m_TTFResource);
                font->m_Ranges.EraseSwap(i);
            }
        }
    }

    static TTFResource* GetTTFFromCodePoint(FontResource* resource, uint32_t codepoint)
    {
        uint32_t size = resource->m_Ranges.Size();
        if (!size)
            return 0;
        GlyphRange* ranges = resource->m_Ranges.Begin();
        for (uint32_t i = size-1; i >= 0; --i)
        {
            GlyphRange* range = &ranges[i];
            if (range->m_RangeStart <= codepoint && codepoint <= range->m_RangeEnd)
                return range->m_TTFResource;
        }
        return 0;
    }

    static void PrewarmGlyphsCallback(void* ctx, int result, const char* errmsg)
    {
        FontResource* font = (FontResource*)ctx;
        font->m_Prewarming = 0;
        font->m_PrewarmDone = 1;
    }

    static dmResource::Result PrewarmGlyphCache(FontResource* resource, TTFResource* ttfresource, bool all_chars, const char* characters)
    {
        if (all_chars)
        {
            // It defeats the purpose of the dynamic glyph cache to include _all_ characters
            return dmResource::RESULT_OK;
        }

        bool result = dmGameSystem::FontGenAddGlyphs(resource, characters, true, PrewarmGlyphsCallback, resource);
        return result ? dmResource::RESULT_OK : dmResource::RESULT_INVALID_DATA;
    }

    static uint32_t GetResourceSize(FontResource* font)
    {
        uint32_t size = sizeof(FontResource);
        size += sizeof(dmRenderDDF::FontMap); // the ddf pointer
        size += font->m_Glyphs.Capacity() * sizeof(dmRenderDDF::GlyphBank::Glyph*);
        size += font->m_DynamicGlyphs.Capacity() * sizeof(FontGlyph);
        size += font->m_ResourceSize;
        return size + dmRender::GetFontMapResourceSize(font->m_FontMap);
    }

    static void DeleteDynamicGlyphIter(FontResource* font_map, const uint32_t* hash, DynamicGlyph** glyphp)
    {
        (void)hash;
        DynamicGlyph* glyph = *glyphp;
        free((void*)glyph->m_Data);
        delete glyph;
    }

    static void DeleteFontResource(dmResource::HFactory factory, FontResource* font_map)
    {
        ReleaseResources(factory, font_map);

        if (font_map->m_FontMap)
            dmRender::DeleteFontMap(font_map->m_FontMap);

        font_map->m_DynamicGlyphs.Iterate(DeleteDynamicGlyphIter, font_map);
        font_map->m_DynamicGlyphs.Clear();

        delete font_map;
    }

    // Api for the font renderer
    static dmRender::FontGlyph* GetDynamicGlyph(uint32_t codepoint, FontResource* resource)
    {
        DynamicGlyph** dynglyphp = resource->m_DynamicGlyphs.Get(codepoint);
        if (dynglyphp)
            return &(*dynglyphp)->m_Glyph;
        return 0;
    }

    // Api for the font renderer
    static dmRender::FontGlyph* GetGlyph(uint32_t codepoint, FontResource* resource)
    {
        dmRender::FontGlyph** glyphp = resource->m_Glyphs.Get(codepoint);
        return glyphp ? *glyphp : 0;
    }

    static inline uint8_t* GetPointer(void* data, uint32_t offset)
    {
        return ((uint8_t*)data) + offset;
    }

    static void* GetDynamicGlyphData(uint32_t codepoint, void* user_ctx, uint32_t* out_size, uint32_t* out_compression, uint32_t* out_width, uint32_t* out_height, uint32_t* out_channels)
    {
        DM_STATIC_ASSERT(sizeof(ImageDataHeader) == 1, Invalid_struct_size);
        FontResource* resource = (FontResource*)user_ctx;
        DynamicGlyph** dynglyphp = resource->m_DynamicGlyphs.Get(codepoint);
        if (!dynglyphp)
            return 0;

        DynamicGlyph* dynglyph = *dynglyphp;
        *out_width = dynglyph->m_DataImageWidth;
        *out_height = dynglyph->m_DataImageHeight;
        *out_channels = dynglyph->m_DataImageChannels;
        *out_compression = (uint32_t)dynglyph->m_Compression;
        *out_size = dynglyph->m_DataSize - sizeof(ImageDataHeader);
        return dynglyph->m_Data + sizeof(ImageDataHeader); // we return only the image data here
    }

    static void* GetGlyphData(uint32_t codepoint, void* user_ctx, uint32_t* out_size, uint32_t* out_compression, uint32_t* out_width, uint32_t* out_height, uint32_t* out_channels)
    {
        FontResource* resource = (FontResource*)user_ctx;

        // Make sure to now mix character types, as their sizes don't match
        if (!resource->m_DynamicGlyphs.Empty())
        {
            DynamicGlyph** dynglyphp = resource->m_DynamicGlyphs.Get(codepoint);
            if (dynglyphp)
            {
                DM_STATIC_ASSERT(sizeof(ImageDataHeader) == 1, Invalid_struct_size);

                DynamicGlyph* dynglyph = *dynglyphp;
                *out_width = dynglyph->m_DataImageWidth;
                *out_height = dynglyph->m_DataImageHeight;
                *out_channels = dynglyph->m_DataImageChannels;
                *out_compression = (uint32_t)dynglyph->m_Compression;
                *out_size = dynglyph->m_DataSize - sizeof(ImageDataHeader);
                return dynglyph->m_Data + sizeof(ImageDataHeader); // we return only the image data here
            }
            return 0;
        }

        dmRender::FontGlyph** glyphp = resource->m_Glyphs.Get(codepoint);
        if (!glyphp)
            return 0;
        dmRender::FontGlyph* glyph = *glyphp;

        dmRenderDDF::GlyphBank* glyph_bank = resource->m_GlyphBankResource->m_DDF;
        uint8_t* data = (uint8_t*)glyph_bank->m_GlyphData.m_Data;
        uint8_t* glyph_data = GetPointer(data, glyph->m_GlyphDataOffset);

        // Currently the header is just a single byte
        uint8_t compression_type = glyph_data[0];
        uint32_t header_size = 1;

        *out_size = glyph->m_GlyphDataSize - header_size; // return the size of the payload
        *out_width = glyph->m_Width + resource->m_CacheCellPadding*2;
        *out_height = glyph->m_Ascent + glyph->m_Descent + resource->m_CacheCellPadding*2;
        *out_channels = glyph_bank->m_GlyphChannels;
        *out_compression = compression_type;
        return glyph_data + header_size;
    }

    static void GetGlyphMetric(dmRender::FontMetrics* metrics, const uint32_t* key, dmRenderDDF::GlyphBank::Glyph** pglyph)
    {
        dmRenderDDF::GlyphBank::Glyph* g = *pglyph;
        metrics->m_MaxAscent = dmMath::Max(metrics->m_MaxAscent, (float)g->m_Ascent);
        metrics->m_MaxDescent = dmMath::Max(metrics->m_MaxDescent, (float)g->m_Descent);

        float height = g->m_Ascent + g->m_Descent; // perhaps not the best, but should work for now
        assert(height < 1000.0f);
        metrics->m_MaxWidth = (uint32_t)dmMath::Max((float)metrics->m_MaxWidth, g->m_Width);
        metrics->m_MaxHeight = dmMath::Max(metrics->m_MaxHeight, height);
        // Our old font generator creates an image of the exact same size
        metrics->m_ImageMaxWidth = metrics->m_MaxWidth;
        metrics->m_ImageMaxHeight = metrics->m_MaxHeight;
    }

    static void GetDynamicGlyphMetric(dmRender::FontMetrics* metrics, const uint32_t* key, DynamicGlyph** pglyph)
    {
        if ((*key) == WHITESPACE_NEW_LINE || (*key) == WHITESPACE_CARRIAGE_RETURN) // new line doesn't have a size
            return;

        DynamicGlyph* g = *pglyph;
        assert(g->m_DataImageWidth < 1000);
        assert(g->m_DataImageHeight < 1000);
        float height = g->m_Glyph.m_Ascent + g->m_Glyph.m_Descent; // perhaps not the best, but should work for now
        assert(height < 1000.0f);
        metrics->m_MaxWidth = dmMath::Max(metrics->m_MaxWidth, g->m_Glyph.m_Width);
        metrics->m_MaxHeight = dmMath::Max(metrics->m_MaxHeight, height);
        metrics->m_ImageMaxWidth = dmMath::Max(metrics->m_ImageMaxWidth, g->m_DataImageWidth);
        metrics->m_ImageMaxHeight = dmMath::Max(metrics->m_ImageMaxHeight, g->m_DataImageHeight);
        metrics->m_MaxAscent = dmMath::Max(metrics->m_MaxAscent, (float)g->m_Glyph.m_Ascent);
        metrics->m_MaxDescent = dmMath::Max(metrics->m_MaxDescent, (float)g->m_Glyph.m_Descent);
    }

    static uint32_t GetDynamicFontMetrics(void* user_ctx, dmRender::FontMetrics* metrics)
    {
        FontResource* font = (FontResource*)user_ctx;
        if (!font->m_DynamicGlyphs.Empty())
        {
            font->m_DynamicGlyphs.Iterate(GetDynamicGlyphMetric, metrics);
            return font->m_DynamicGlyphs.Size();
        }

        font->m_Glyphs.Iterate(GetGlyphMetric, metrics);
        return font->m_Glyphs.Size();
    }

    static uint32_t GetFontMetrics(void* user_ctx, dmRender::FontMetrics* metrics)
    {
        FontResource* font = (FontResource*)user_ctx;
        font->m_Glyphs.Iterate(GetGlyphMetric, metrics);
        return font->m_Glyphs.Size() + font->m_DynamicGlyphs.Size();
    }

    static dmResource::Result AcquireResources(dmResource::HFactory factory, dmRenderDDF::FontMap* ddf,
                                                    FontResource* font_map, const char* filename)
    {
        dmResource::Result result = dmResource::Get(factory, ddf->m_Material, (void**) &font_map->m_MaterialResource);
        if (result != dmResource::RESULT_OK)
        {
            ReleaseResources(factory, font_map);
            return result;
        }

        if (ddf->m_Dynamic)
        {
            result = dmResource::Get(factory, ddf->m_Font, (void**) &font_map->m_TTFResource);
            if (result != dmResource::RESULT_OK)
            {
                dmLogError("Failed to find font '%s': %d\n", ddf->m_Font, result);
                ReleaseResources(factory, font_map);
                return result;
            }
        }
        else
        {
            result = dmResource::Get(factory, ddf->m_GlyphBank, (void**) &font_map->m_GlyphBankResource);
            if (result != dmResource::RESULT_OK)
            {
                ReleaseResources(factory, font_map);
                return result;
            }

        }
        return dmResource::RESULT_OK;
    }

    static void SetupParamsBase(dmRenderDDF::FontMap* ddf, const char* filename, dmRender::FontMapParams* params)
    {
        params->m_NameHash           = dmHashString64(filename);
        params->m_ShadowX            = ddf->m_ShadowX;
        params->m_ShadowY            = ddf->m_ShadowY;
        params->m_OutlineAlpha       = ddf->m_OutlineAlpha;
        params->m_ShadowAlpha        = ddf->m_ShadowAlpha;
        params->m_Alpha              = ddf->m_Alpha;
        params->m_LayerMask          = ddf->m_LayerMask;
        params->m_Padding            = ddf->m_Padding;
        params->m_ImageFormat        = ddf->m_OutputFormat;

        params->m_SdfSpread          = ddf->m_SdfSpread;
        params->m_SdfOutline         = ddf->m_SdfOutline;
        params->m_SdfShadow          = ddf->m_SdfShadow;
    }

    static void SetupParamsForDynamicFont(dmRenderDDF::FontMap* ddf, const char* filename, dmRender::FontMapParams* params)
    {
        if (ddf->m_ShadowBlur > 0.0f && ddf->m_ShadowAlpha > 0.0f) {
            params->m_GlyphChannels = 3;
        }
        else {
            params->m_GlyphChannels = 1;
        }

        params->m_CacheWidth     = ddf->m_CacheWidth;
        params->m_CacheHeight    = ddf->m_CacheHeight;

        params->m_GetGlyph       = (dmRender::FGetGlyph)GetDynamicGlyph;
        params->m_GetGlyphData   = (dmRender::FGetGlyphData)GetDynamicGlyphData;
        params->m_GetFontMetrics = (dmRender::FGetFontMetrics)GetDynamicFontMetrics;
    }

    static void SetupParamsForGlyphBank(dmRenderDDF::FontMap* ddf, const char* filename, dmRenderDDF::GlyphBank* glyph_bank, dmRender::FontMapParams* params)
    {
        params->m_GlyphChannels      = glyph_bank->m_GlyphChannels;
        params->m_CacheWidth         = glyph_bank->m_CacheWidth;
        params->m_CacheHeight        = glyph_bank->m_CacheHeight;

        params->m_MaxAscent          = glyph_bank->m_MaxAscent;
        params->m_MaxDescent         = glyph_bank->m_MaxDescent;
        params->m_CacheCellWidth     = glyph_bank->m_CacheCellWidth;
        params->m_CacheCellHeight    = glyph_bank->m_CacheCellHeight;
        params->m_CacheCellMaxAscent = glyph_bank->m_CacheCellMaxAscent;
        params->m_CacheCellPadding   = glyph_bank->m_GlyphPadding;
        params->m_IsMonospaced       = glyph_bank->m_IsMonospaced;

        params->m_GetGlyph           = (dmRender::FGetGlyph)GetGlyph;
        params->m_GetGlyphData       = (dmRender::FGetGlyphData)GetGlyphData;
        params->m_GetFontMetrics     = (dmRender::FGetFontMetrics)GetFontMetrics;
    }

    static float CalcPadding(FontResource* resource, float* outline_padding, float* shadow_padding)
    {
        float base_padding = dmGameSystem::FontGenGetBasePadding();
        *outline_padding = resource->m_DDF->m_OutlineWidth;
        *shadow_padding = resource->m_DDF->m_ShadowBlur;
        return base_padding + *outline_padding + *shadow_padding;
    }

    static float GetPaddedSdfSpread(float padding)
    {
        // Make sure the output spread value is not zero. We distribute the distance values over
        // the spread when we generate the DF glyphs, so if this value is zero we won't be able to map
        // the distance values to a valid range..
        // We use sqrt(2) since it is the diagonal length of a pixel, but any small positive value would do.
        const float sqrt2 = 1.4142f;
        return sqrt2 + padding;
    }

    static float CalcSdfValue(float padding, float width)
    {
        float on_edge_value = dmGameSystem::FontGenGetEdgeValue(); // [0 .. 255] e.g. 191
        const float base_edge = SDF_EDGE_VALUE * 255.0f;

        // Described in the stb_truetype.h as "what value the SDF should increase by when moving one SDF "pixel" away from the edge"
        float pixel_dist_scale = (float)on_edge_value/padding;

        return (base_edge - (pixel_dist_scale * width)) / 255.0f;;
    }

    static dmResource::Result CreateFont(dmRender::HRenderContext context, dmRenderDDF::FontMap* ddf, const char* path, FontResource* resource)
    {
        dmRender::FontMapParams params;
        SetupParamsBase(ddf, path, &params);

        if (ddf->m_Dynamic)
            SetupParamsForDynamicFont(ddf, path, &params);
        else
            SetupParamsForGlyphBank(ddf, path, resource->m_GlyphBankResource->m_DDF, &params);

        dmGraphics::HContext graphics_context = dmRender::GetGraphicsContext(context);
        resource->m_FontMap = dmRender::NewFontMap(context, graphics_context, params);
        if (!resource->m_FontMap)
        {
            dmLogError("Failed creating resource '%s'", path);
            return dmResource::RESULT_INVALID_DATA;
        }

        resource->m_DDF               = ddf;
        resource->m_CacheCellPadding  = params.m_CacheCellPadding;
        resource->m_IsDynamic         = ddf->m_Dynamic;
        resource->m_Padding           = ddf->m_Padding;

        dmRender::SetFontMapMaterial(resource->m_FontMap, resource->m_MaterialResource->m_Material);
        dmRender::SetFontMapUserData(resource->m_FontMap, (void*)resource);
        if (ddf->m_Dynamic)
        {
            float outline_padding;
            float shadow_padding; // the extra padding for the shadow blur
            float padding = CalcPadding(resource, &outline_padding, &shadow_padding);

            dmRender::SetFontMapSdfSpread(resource->m_FontMap, padding);
            dmRender::SetFontMapSdfOutlineWidth(resource->m_FontMap, CalcSdfValue(padding, outline_padding));

            float sdf_shadow = 1.0f;
            if (shadow_padding)
            {
                sdf_shadow = CalcSdfValue(padding, shadow_padding);
            }
            dmRender::SetFontMapSdfShadow(resource->m_FontMap, sdf_shadow);

            dmFont::HFont hfont = dmGameSystem::GetFont(resource->m_TTFResource);
            float scale = dmFont::GetPixelScaleFromSize(hfont, resource->m_DDF->m_Size);
            dmRender::SetFontMapMaxAscent(resource->m_FontMap, dmFont::GetAscent(hfont, scale));
            dmRender::SetFontMapMaxDescent(resource->m_FontMap, -dmFont::GetDescent(hfont, scale));
        }
        return dmResource::RESULT_OK;
    }

    static dmResource::Result PrewarmFont(dmResource::HFactory factory, const char* path, FontResource* font)
    {
        if (font->m_IsDynamic)
        {
            // Prewarm cache
            font->m_Jobs = dmResource::GetJobThread(factory);
            AddFontRange(font, font->m_TTFResource, 0, 0xFFFFFFFF); // Add the default font/range

            bool all_chars = font->m_DDF->m_AllChars;
            bool has_chars = font->m_DDF->m_Characters != 0 && font->m_DDF->m_Characters[0] != 0;
            if (all_chars || !has_chars)
            {
                font->m_PrewarmDone = 1;
                return dmResource::RESULT_OK;
            }

            // Use the default ttf resource for prewarming
            dmResource::Result r = PrewarmGlyphCache(font, font->m_TTFResource, all_chars, font->m_DDF->m_Characters);
            if (dmResource::RESULT_OK != r)
            {
                dmLogError("Failed to prewarm glyph cache for font '%s'", path);
                return dmResource::RESULT_OK;
            }

            font->m_Prewarming = 1;
        }
        else
        {
            // Add all glyphs into a lookup table
            dmRenderDDF::GlyphBank* glyph_bank = font->m_GlyphBankResource->m_DDF;
            uint32_t glyph_count = glyph_bank->m_Glyphs.m_Count;
            font->m_Glyphs.Clear();
            font->m_Glyphs.OffsetCapacity(dmMath::Max(1U, glyph_count));
            for (uint32_t i = 0; i < glyph_count; ++i)
            {
                dmRenderDDF::GlyphBank::Glyph* glyph = &glyph_bank->m_Glyphs[i];
                font->m_Glyphs.Put(glyph->m_Character, glyph);
            }

            font->m_PrewarmDone = 1;
        }

        return font->m_Prewarming ? dmResource::RESULT_PENDING : dmResource::RESULT_OK;
    }

    static dmResource::Result ResFontPreload(const dmResource::ResourcePreloadParams* params)
    {
        dmRenderDDF::FontMap* ddf;
        dmDDF::Result e = dmDDF::LoadMessage<dmRenderDDF::FontMap>(params->m_Buffer, params->m_BufferSize, &ddf);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }

        dmResource::PreloadHint(params->m_HintInfo, ddf->m_Material);
        if (ddf->m_Dynamic)
            dmResource::PreloadHint(params->m_HintInfo, ddf->m_Font);

        *params->m_PreloadData = ddf;
        return dmResource::RESULT_OK;
    }

    static dmResource::Result ResFontCreate(const dmResource::ResourceCreateParams* params)
    {
        FontResource* font = new FontResource;
        font->m_Resource = params->m_Resource;

        const char* path = params->m_Filename;
        dmRenderDDF::FontMap* ddf = (dmRenderDDF::FontMap*) params->m_PreloadData;
        dmResource::Result r = AcquireResources(params->m_Factory, ddf, font, path);
        if (r != dmResource::RESULT_OK)
        {
            DeleteFontResource(params->m_Factory, font);
            dmResource::SetResource(params->m_Resource, 0);
            return r;
        }

        if (ddf->m_Dynamic)
        {
            if (ddf->m_OutputFormat != dmRenderDDF::TYPE_DISTANCE_FIELD)
            {
                dmLogError("Currently only distance field fonts are supported: %s", path);
                return dmResource::RESULT_NOT_SUPPORTED;
            }
        }

        r = CreateFont((dmRender::HRenderContext) params->m_Context, ddf, path, font);
        if (r != dmResource::RESULT_OK)
        {
            DeleteFontResource(params->m_Factory, font);
            dmResource::SetResource(params->m_Resource, 0);
            return r;
        }

        dmResource::SetResource(params->m_Resource, font);
        dmResource::SetResourceSize(params->m_Resource, GetResourceSize(font));
        return r;
    }

    static dmResource::Result ResFontPostCreate(const dmResource::ResourcePostCreateParams* params)
    {
        FontResource* font = (FontResource*)dmResource::GetResource(params->m_Resource);

        if (font->m_Prewarming)
        {
            dmGameSystem::FontGenFlushFinishedJobs(8000);
            return font->m_PrewarmDone ? dmResource::RESULT_OK : dmResource::RESULT_PENDING;
        }
        return PrewarmFont(params->m_Factory, params->m_Filename, font);
    }

    static dmResource::Result ResFontDestroy(const dmResource::ResourceDestroyParams* params)
    {
        FontResource* font = (FontResource*)dmResource::GetResource(params->m_Resource);
        DeleteFontResource(params->m_Factory, font);
        return dmResource::RESULT_OK;
    }

    static dmResource::Result ResFontRecreate(const dmResource::ResourceRecreateParams* params)
    {
        dmRenderDDF::FontMap* ddf;
        dmDDF::Result e = dmDDF::LoadMessage<dmRenderDDF::FontMap>(params->m_Buffer, params->m_BufferSize, &ddf);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }

        const char* path = params->m_Filename;
        FontResource* tmp_font_map = new FontResource;
        tmp_font_map->m_Resource = params->m_Resource;

        dmResource::Result r = AcquireResources(params->m_Factory, ddf, tmp_font_map, path);
        if(r != dmResource::RESULT_OK)
        {
            DeleteFontResource(params->m_Factory, tmp_font_map);
            return r;
        }

        r = CreateFont((dmRender::HRenderContext) params->m_Context, ddf, path, tmp_font_map);
        if (r != dmResource::RESULT_OK)
        {
            DeleteFontResource(params->m_Factory, tmp_font_map);
            dmResource::SetResource(params->m_Resource, 0);
            return r;
        }

        // It's easiest to do the swap here
        FontResource* resource_font_map = (FontResource*)dmResource::GetResource(params->m_Resource);
        // Copy the resources
        resource_font_map->Swap(tmp_font_map);
        // Setup the helper structs again
        PrewarmFont(params->m_Factory, path, resource_font_map);
        DeleteFontResource(params->m_Factory, tmp_font_map);

        dmResource::SetResourceSize(params->m_Resource, GetResourceSize(resource_font_map));
        return dmResource::RESULT_OK;
    }

    // Api

    dmRender::HFont ResFontGetHandle(FontResource* resource)
    {
        return resource->m_FontMap;
    }

    TTFResource* ResFontGetTTFResourceFromCodepoint(FontResource* resource, uint32_t codepoint)
    {
        TTFResource* ttf = GetTTFFromCodePoint(resource, codepoint);
        return ttf ? ttf : resource->m_TTFResource;
    }

    dmResource::Result ResFontGetInfo(FontResource* resource, FontInfo* desc)
    {
        memcpy(desc, resource->m_DDF, sizeof(FontInfo));
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResFontAddGlyph(FontResource* font, uint32_t codepoint, FontGlyph* inglyph, void* imagedata, uint32_t imagedatasize)
    {
        DynamicGlyph** glyphp = font->m_DynamicGlyphs.Get(codepoint);
        if (glyphp != 0)
        {
            return dmResource::RESULT_OK;
        }

        if (font->m_DynamicGlyphs.Full())
        {
            uint32_t cap = font->m_DynamicGlyphs.Capacity() + 64;
            font->m_DynamicGlyphs.SetCapacity((cap*3)/2, cap);
        }

        if (!font->m_Glyphs.Empty())
        {
            font->m_Glyphs.Clear();
        }

        DynamicGlyph* glyph = new DynamicGlyph;
        // dmRender::Glyph is currently a dmRenderDDF::GlyphBank::Glyph
        dmRenderDDF::GlyphBank::Glyph& g = glyph->m_Glyph;
        g.m_Character  = codepoint;
        g.m_Width      = inglyph->m_Width;
        g.m_ImageWidth = inglyph->m_ImageWidth;
        g.m_Advance    = inglyph->m_Advance;
        g.m_LeftBearing= inglyph->m_LeftBearing;
        g.m_Ascent     = inglyph->m_Ascent;
        g.m_Descent    = fabs(inglyph->m_Descent);

        // The extra padding is stored in the glyph bank. See Fontc.java: getPadding():
        //      return fontDesc.getShadowBlur() + (int)(fontDesc.getOutlineWidth()) + 1;
        g.m_Width += font->m_Padding * 2;
        g.m_Width = dmMath::Min(g.m_Width, (float)g.m_ImageWidth);

        // Redundant in this setup
        // g.m_X;
        // g.m_Y;
        // g.m_GlyphDataOffset;
        // g.m_GlyphDataSize;

        ImageDataHeader* header = (ImageDataHeader*)imagedata;
        glyph->m_Compression = header ? header->m_Compression : FONT_GLYPH_COMPRESSION_NONE;
        glyph->m_Data = (uint8_t*)imagedata;
        glyph->m_DataSize = imagedatasize;
        glyph->m_DataImageWidth = inglyph->m_ImageWidth;
        glyph->m_DataImageHeight = inglyph->m_ImageHeight;
        glyph->m_DataImageChannels = inglyph->m_Channels;

        font->m_ResourceSize += glyph->m_DataSize;

        assert(glyph->m_DataImageWidth < 1000);
        assert(glyph->m_DataImageHeight < 1000);

        font->m_DynamicGlyphs.Put(codepoint, glyph);

        uint32_t prev_width, prev_height, prev_ascent;
        dmRender::GetFontMapCacheSize(font->m_FontMap, &prev_width, &prev_height, &prev_ascent);

        bool dirty = inglyph->m_ImageWidth > prev_width ||
                      inglyph->m_ImageHeight > prev_height ||
                      inglyph->m_Ascent > prev_ascent;
        if (dirty)
        {
            uint32_t cell_width = dmMath::Max((uint32_t)inglyph->m_ImageWidth, prev_width);
            uint32_t cell_height = dmMath::Max((uint32_t)inglyph->m_ImageHeight, prev_height);
            uint32_t cell_ascent = dmMath::Max((uint32_t)inglyph->m_Ascent, prev_ascent);
            dmRender::SetFontMapCacheSize(font->m_FontMap, cell_width, cell_height, cell_ascent);
        }

        // TODO: Calculate the current size (including the bitmaps!)
        dmResource::SetResourceSize(font->m_Resource, GetResourceSize(font));

        return dmResource::RESULT_OK;
    }

    dmResource::Result ResFontRemoveGlyph(FontResource* font, uint32_t codepoint)
    {
        DynamicGlyph** glyphp = font->m_DynamicGlyphs.Get(codepoint);
        if (!glyphp)
            return dmResource::RESULT_RESOURCE_NOT_FOUND;

        font->m_DynamicGlyphs.Erase(codepoint);

        DynamicGlyph* glyph = *glyphp;
        font->m_ResourceSize -= glyph->m_DataSize;

        free((void*)glyph->m_Data);
        delete glyph;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResFontAddGlyphSource(dmResource::HFactory factory, dmhash_t fontc_hash, dmhash_t ttf_hash, uint32_t codepoint_min, uint32_t codepoint_max)
    {
        dmGameSystem::FontResource* font;
        dmResource::Result r = dmResource::Get(factory, fontc_hash, (void**)&font);
        if (dmResource::RESULT_OK != r)
        {
            dmLogError("Failed to get font '%s': %d", dmHashReverseSafe64(fontc_hash), r);
            return dmResource::RESULT_RESOURCE_NOT_FOUND;
        }

        dmGameSystem::TTFResource* ttf;
        r = dmResource::Get(factory, ttf_hash, (void**)&ttf);
        if (dmResource::RESULT_OK != r)
        {
            dmResource::Release(factory, font);
            dmLogError("Failed to get ttf '%s': %d", dmHashReverseSafe64(ttf_hash), r);
            return dmResource::RESULT_RESOURCE_NOT_FOUND;
        }

        // We leave the IncRef'd ttf resource as we store it in our internal structure
        AddFontRange(font, ttf, codepoint_min, codepoint_max);

        dmResource::Release(factory, font);
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResFontRemoveGlyphSource(dmResource::HFactory factory, dmhash_t fontc_hash, dmhash_t ttf_hash)
    {
        dmGameSystem::FontResource* font;
        dmResource::Result r = dmResource::Get(factory, fontc_hash, (void**)&font);
        if (dmResource::RESULT_OK != r)
        {
            dmLogError("Failed to get font '%s': %d", dmHashReverseSafe64(fontc_hash), r);
            return dmResource::RESULT_RESOURCE_NOT_FOUND;
        }

        dmGameSystem::TTFResource* ttf;
        r = dmResource::Get(factory, ttf_hash, (void**)&ttf);
        if (dmResource::RESULT_OK != r)
        {
            dmResource::Release(factory, font);
            dmLogError("Failed to get ttf '%s': %d", dmHashReverseSafe64(ttf_hash), r);
            return dmResource::RESULT_RESOURCE_NOT_FOUND;
        }

        RemoveFontRange(font, ttf);

        dmResource::Release(factory, ttf);
        dmResource::Release(factory, font);
        return dmResource::RESULT_OK;
    }

    static void PrintGlyph(uint32_t codepoint, dmRenderDDF::GlyphBank::Glyph* glyph, FontResource* font)
    {
        dmRenderDDF::GlyphBank* glyph_bank = font->m_GlyphBankResource->m_DDF;

        printf("    ");
        printf("c: '%c' 0x%0X w: %.2f    ", codepoint, codepoint, glyph->m_Width);
        printf("adv: %.2f  l: %.2f ", glyph->m_Advance, glyph->m_LeftBearing);
        printf("asc/dsc: %d, %d ", glyph->m_Ascent, glyph->m_Descent);

        printf("img w/h: %2d, %2d  masc: %2d", glyph_bank->m_CacheCellWidth, glyph_bank->m_CacheCellHeight, glyph_bank->m_CacheCellMaxAscent);
        printf("\n");
    }

    static void PrintDynamicGlyph(uint32_t codepoint, DynamicGlyph* glyph, FontResource* font)
    {
        printf("    ");
        printf("c: '%c' 0x%0X  w: %.2f  imgw: %.u  ", codepoint, codepoint, glyph->m_Glyph.m_Width, glyph->m_Glyph.m_ImageWidth);
        printf("adv: %.2f  l: %.2f ", glyph->m_Glyph.m_Advance, glyph->m_Glyph.m_LeftBearing);
        printf("asc/dsc: %d, %d ", glyph->m_Glyph.m_Ascent, glyph->m_Glyph.m_Descent);

        printf("img w/h: %2d, %2d ", glyph->m_DataImageWidth, glyph->m_DataImageHeight);
        printf("\n");
    }

    static void PrintGlyphs(FontResource* font, const uint32_t* key, dmRenderDDF::GlyphBank::Glyph** pglyph)
    {
        PrintGlyph(*key, *pglyph, font);
    }

    static void PrintDynamicGlyphs(FontResource* font, const uint32_t* key, DynamicGlyph** pglyph)
    {
        PrintDynamicGlyph(*key, *pglyph, font);
    }

    void ResFontDebugPrint(FontResource* font)
    {
        printf("FONT:\n");
        printf("  cache cell padding: %u\n", font->m_CacheCellPadding);
        printf("  glyphs:\n");
        font->m_Glyphs.Iterate(PrintGlyphs, font);
        printf("  dyn glyphs:\n");
        font->m_DynamicGlyphs.Iterate(PrintDynamicGlyphs, font);

        printf("\n");
    }

    static ResourceResult RegisterResourceType_Font(HResourceTypeContext ctx, HResourceType type)
    {
        // The engine.cpp creates the contexts for some of our our built in types (i.e. same context for some types)
        void* render_context = ResourceTypeContextGetContextByHash(ctx, ResourceTypeGetNameHash(type));
        assert(render_context);
        return (ResourceResult)dmResource::SetupType(ctx,
                                           type,
                                           render_context,
                                           ResFontPreload,
                                           ResFontCreate,
                                           ResFontPostCreate,
                                           ResFontDestroy,
                                           ResFontRecreate);
    }

    static ResourceResult DeregisterResourceType_Font(HResourceTypeContext ctx, HResourceType type)
    {
        return RESOURCE_RESULT_OK;
    }
}

DM_DECLARE_RESOURCE_TYPE(ResourceTypeFont, "fontc", dmGameSystem::RegisterResourceType_Font, dmGameSystem::DeregisterResourceType_Font);
