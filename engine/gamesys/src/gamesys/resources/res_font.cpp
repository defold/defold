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
#include "res_glyph_bank.h"
#include "res_ttf.h"

#include <string.h>

#include <dlib/dstrings.h>
#include <dlib/log.h>

#include <render/font_renderer.h>
#include <render/render_ddf.h>

#include <dmsdk/gamesys/resources/res_material.h>

namespace dmGameSystem
{
    struct ImageDataHeader
    {
        uint8_t m_Compression; // FontGlyphCompression
    };

    struct DynamicGlyph
    {
        dmRender::FontGlyph     m_Glyph; // for faster access of the text renderer
        uint8_t*                m_Data;
        uint16_t                m_DataSize;
        uint16_t                m_DataImageWidth;
        uint16_t                m_DataImageHeight;
        uint8_t                 m_DataImageChannels;
        uint8_t                 m_Compression; //FontGlyphCompression
    };

    struct GlyphRange
    {
        TTFResource*           m_TTFResource;
        uint32_t               m_RangeStart;    // The code point range start (inclusive)
        uint32_t               m_RangeEnd;      // The code point range end (inclusive)
    };

    template<typename T>
    static void SwapVar(T& a, T& b)
    {
        T tmp = a;
        a = b;
        b = tmp;
    }

    struct FontResource
    {
        dmRenderDDF::FontMap*   m_DDF;
        dmRender::HFont         m_FontMap;
        HResourceDescriptor     m_Resource;             // For updating the resource size dynamically
        MaterialResource*       m_MaterialResource;
        GlyphBankResource*      m_GlyphBankResource;
        TTFResource*            m_TTFResource;          // the first ttf resource
        dmJobThread::HContext   m_Jobs;
        uint32_t                m_CacheCellPadding;
        bool                    m_IsDynamic;            // Are the glyphs populated at runtime?
        uint8_t                 m_Padding;              // Extra space for outline + shadow

        dmHashTable32<dmRenderDDF::GlyphBank::Glyph*>   m_Glyphs;
        dmHashTable32<DynamicGlyph*>                    m_DynamicGlyphs;
        dmArray<GlyphRange>                             m_Ranges;

        FontResource()
        {
            memset(this, 0, sizeof(*this));
        }

        // Used when recreating a font
        void Swap(FontResource* src)
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
    };

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

    static void AddFontRange(FontResource* resource, uint32_t range_start, uint32_t range_end, TTFResource* ttfresource)
    {
        if (resource->m_Ranges.Full())
            resource->m_Ranges.OffsetCapacity(4);

        GlyphRange range;
        range.m_RangeStart  = range_start;
        range.m_RangeEnd    = range_end;
        range.m_TTFResource = ttfresource;
        resource->m_Ranges.Push(range);
    }

    static TTFResource* GetFontFromCodePoint(FontResource* resource, uint32_t codepoint, uint32_t range_end)
    {
        uint32_t size = resource->m_Ranges.Size();
        GlyphRange* ranges = resource->m_Ranges.Begin();
        for (uint32_t i = size-1; i >= 0; --i)
        {
            GlyphRange* range = &ranges[i];
            if (range->m_RangeStart <= codepoint && codepoint <= range->m_RangeEnd)
                return range->m_TTFResource;
        }
        return 0;
    }

    // static void GenerateGlyphs(Context* ctx, FontInfo* info, const char* text, FGlyphCallback cbk, void* cbk_ctx)
    // {
    //     uint32_t len        = dmUtf8::StrLen(text);

    //     JobStatus* status      = new JobStatus;
    //     status->m_TimeGlyphGen = 0;
    //     status->m_Count        = len;
    //     status->m_Failures     = 0;
    //     status->m_Error        = 0;

    //     const char* cursor = text;
    //     uint32_t c = 0;
    //     uint32_t index  = 0;
    //     while ((c = dmUtf8::NextChar(&cursor)))
    //     {
    //         ++index;
    //         bool last_item = len == index;

    //         FGlyphCallback last_callback = 0;
    //         void*          last_callback_ctx = 0;
    //         if (last_item)
    //         {
    //             last_callback = cbk;
    //             last_callback_ctx = cbk_ctx;
    //         }
    //         GenerateGlyph(ctx, info, c, last_item, status, last_callback, last_callback_ctx);
    //     }
    // }

    // static void GenerateGlyph(Context* ctx, FontInfo* info, uint32_t codepoint, bool last_item, JobStatus* status, FGlyphCallback cbk, void* cbk_ctx)
    // {
    //     JobItem* item = new JobItem;
    //     item->m_FontInfo = info;
    //     item->m_Codepoint = codepoint;
    //     item->m_Callback = cbk;
    //     item->m_CallbackCtx = cbk_ctx;
    //     item->m_Status = status;
    //     item->m_LastItem = last_item;
    //     dmJobThread::PushJob(ctx->m_Jobs, JobGenerateGlyph, JobPostProcessGlyph, ctx, item);
    // }

    static void PrewarmDynamicGlyphs(FontResource* resource, TTFResource* ttfresource, bool all_chars, const char* characters)
    {
        // uint32_t range_start = 0;
        // uint32_t range_end = characters ? (uint32_t)dmUtf8::StrLen(characters) : 0;
        if (all_chars)
        {
            //GetFontFromCodePoint
            // //range_end = 0x10FFFF; // From Fontc.java
            // for (uint32_t i = 0; i < 0x10FFFF; ++i)
            // {
            //     uint32_t c = i;
            // }

        }

        // for (uint32_t i = range_start; i < range_end; ++i)
        // {
        //     uint32_t c = all_chars ? i : characters[i];

        // }
    }

    static uint32_t GetResourceSize(FontResource* font_map)
    {
        uint32_t size = sizeof(FontResource);
        size += sizeof(dmRenderDDF::FontMap); // the ddf pointer
        size += font_map->m_Glyphs.Capacity() * sizeof(dmRenderDDF::GlyphBank::Glyph*);
        size += font_map->m_DynamicGlyphs.Capacity() * sizeof(FontGlyph);
        return size + dmRender::GetFontMapResourceSize(font_map->m_FontMap);
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
        if (!resource->m_DynamicGlyphs.Empty())
        {
            DynamicGlyph** dynglyphp = resource->m_DynamicGlyphs.Get(codepoint);
            if (dynglyphp)
                return &(*dynglyphp)->m_Glyph;
            return 0;
        }

        dmRender::FontGlyph** glyphp = resource->m_Glyphs.Get(codepoint);
        return glyphp ? *glyphp : 0;
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

    static void SetupParamsForDynamiFont(dmRenderDDF::FontMap* ddf, const char* filename, dmRender::FontMapParams* params)
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

    static dmResource::Result CreateFont(dmRender::HRenderContext context, dmRenderDDF::FontMap* ddf, const char* path, FontResource* font_map)
    {
        dmRender::FontMapParams params;
        SetupParamsBase(ddf, path, &params);
        if (ddf->m_Dynamic)
            SetupParamsForDynamiFont(ddf, path, &params);
        else
            SetupParamsForGlyphBank(ddf, path, font_map->m_GlyphBankResource->m_DDF, &params);

        dmGraphics::HContext graphics_context = dmRender::GetGraphicsContext(context);
        // if (font_map->m_FontMap == 0)
        // {
        font_map->m_FontMap = dmRender::NewFontMap(context, graphics_context, params);
        if (!font_map->m_FontMap)
        {
            dmLogError("Failed creating resource '%s'", path);
            return dmResource::RESULT_INVALID_DATA;
        }
        // }
        // else
        // {
        //     dmRender::SetFontMap(font_map->m_FontMap, context, graphics_context, params);
        //     ReleaseResources(factory, font_map);
        // }

        font_map->m_DDF               = ddf;
        font_map->m_CacheCellPadding  = params.m_CacheCellPadding;
        font_map->m_IsDynamic         = ddf->m_Dynamic;
        font_map->m_Padding           = ddf->m_Padding;

        dmRender::SetFontMapMaterial(font_map->m_FontMap, font_map->m_MaterialResource->m_Material);
        dmRender::SetFontMapUserData(font_map->m_FontMap, (void*)font_map);
        return dmResource::RESULT_OK;
    }

    // static dmResource::Result CreateFontDynamic(dmRender::HRenderContext context,
    //                                 dmRenderDDF::FontMap* ddf, const char* path, FontResource* font_map)
    // {
    //     if (ddf->m_OutputFormat != dmRenderDDF::TYPE_DISTANCE_FIELD)
    //     {
    //         dmLogError("Currently only distance field fonts are supported: %s", path);
    //         return dmResource::RESULT_NOT_SUPPORTED;
    //     }

    //     dmResource::Result r = CreateFont(context, ddf, font_map, path);
    //     if (r != dmResource::RESULT_OK)
    //     {
    //         return r;
    //     }

    //     // // Prewarm cache
    //     // font_map->m_Jobs = dmResource::GetJobThread(factory);
    //     // // Use the default ttf resource for prewarming
    //     // //PrewarmDynamicGlyphs(resource, ttfresource, ddf->m_AllChars, ddf->m_Characters);

    //     // AddFontRange(font_map, 0, 0xFFFFFFFF, font_map->m_TTFResource); // Add the default font/range

    //     return dmResource::RESULT_OK;
    // }

    // static dmResource::Result CreateFontWithGlyphBank(dmRender::HRenderContext context,
    //                                 dmRenderDDF::FontMap* ddf, const char* path, FontResource* font_map)
    // {
    //     dmResource::Result r = CreateFont(context, ddf, font_map, path);
    //     if (r != dmResource::RESULT_OK)
    //     {
    //         return r;
    //     }

    //     // Add all glyphs into a lookup table
    //     dmRenderDDF::GlyphBank* glyph_bank = font_map->m_GlyphBankResource->m_DDF;
    //     uint32_t glyph_count = glyph_bank->m_Glyphs.m_Count;
    //     font_map->m_Glyphs.Clear();
    //     font_map->m_Glyphs.OffsetCapacity(dmMath::Max(1U, glyph_count));
    //     for (uint32_t i = 0; i < glyph_count; ++i)
    //     {
    //         dmRenderDDF::GlyphBank::Glyph* glyph = &glyph_bank->m_Glyphs[i];
    //         font_map->m_Glyphs.Put(glyph->m_Character, glyph);
    //     }

    //     return dmResource::RESULT_OK;
    // }

    static dmResource::Result PostCreateFont(dmResource::HFactory factory, const char* path, FontResource* font_map)
    {
        if (font_map->m_IsDynamic)
        {
            // Prewarm cache
            font_map->m_Jobs = dmResource::GetJobThread(factory);
            // Use the default ttf resource for prewarming
            //PrewarmDynamicGlyphs(resource, ttfresource, ddf->m_AllChars, ddf->m_Characters);

            AddFontRange(font_map, 0, 0xFFFFFFFF, font_map->m_TTFResource); // Add the default font/range
        }
        else
        {
            // Add all glyphs into a lookup table
            dmRenderDDF::GlyphBank* glyph_bank = font_map->m_GlyphBankResource->m_DDF;
            uint32_t glyph_count = glyph_bank->m_Glyphs.m_Count;
            font_map->m_Glyphs.Clear();
            font_map->m_Glyphs.OffsetCapacity(dmMath::Max(1U, glyph_count));
            for (uint32_t i = 0; i < glyph_count; ++i)
            {
                dmRenderDDF::GlyphBank::Glyph* glyph = &glyph_bank->m_Glyphs[i];
                font_map->m_Glyphs.Put(glyph->m_Character, glyph);
            }
        }

        return dmResource::RESULT_OK;
    }

    dmResource::Result ResFontPreload(const dmResource::ResourcePreloadParams* params)
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

    dmResource::Result ResFontCreate(const dmResource::ResourceCreateParams* params)
    {
        FontResource* font_map = new FontResource;
        font_map->m_Resource = params->m_Resource;

        const char* path = params->m_Filename;
        dmRenderDDF::FontMap* ddf = (dmRenderDDF::FontMap*) params->m_PreloadData;
        dmResource::Result r = AcquireResources(params->m_Factory, ddf, font_map, path);
        if (r != dmResource::RESULT_OK)
        {
            DeleteFontResource(params->m_Factory, font_map);
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

        r = CreateFont((dmRender::HRenderContext) params->m_Context, ddf, path, font_map);
        if (r != dmResource::RESULT_OK)
        {
            DeleteFontResource(params->m_Factory, font_map);
            dmResource::SetResource(params->m_Resource, 0);
            return r;
        }

        PostCreateFont(params->m_Factory, path, font_map);

        dmResource::SetResource(params->m_Resource, font_map);
        dmResource::SetResourceSize(params->m_Resource, GetResourceSize(font_map));
        return r;
    }

    dmResource::Result ResFontDestroy(const dmResource::ResourceDestroyParams* params)
    {
        FontResource* font_map = (FontResource*)dmResource::GetResource(params->m_Resource);
        DeleteFontResource(params->m_Factory, font_map);
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResFontRecreate(const dmResource::ResourceRecreateParams* params)
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
        ReleaseResources(params->m_Factory, resource_font_map);
        // Copy the resources
        resource_font_map->Swap(tmp_font_map);
        // Setup the helper structs again
        PostCreateFont(params->m_Factory, path, resource_font_map);
        DeleteFontResource(params->m_Factory, tmp_font_map);

        dmResource::SetResourceSize(params->m_Resource, GetResourceSize(resource_font_map));
        return dmResource::RESULT_OK;
    }

    dmRender::HFont ResFontGetHandle(FontResource* resource)
    {
        return resource->m_FontMap;
    }

    dmResource::Result ResFontGetInfo(FontResource* font, FontInfo* desc)
    {
        memcpy(desc, font->m_DDF, sizeof(FontInfo));
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResFontSetLineHeight(FontResource* font, float max_ascent, float max_descent)
    {
        dmRender::SetFontMapLineHeight(font->m_FontMap, max_ascent, max_descent);
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResFontGetLineHeight(FontResource* font, float* max_ascent, float* max_descent)
    {
        dmRender::GetFontMapLineHeight(font->m_FontMap, max_ascent, max_descent);
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResFontSetCacheCellSize(FontResource* font, uint32_t cell_width, uint32_t cell_height, uint32_t max_ascent)
    {
        dmRender::SetFontMapCacheSize(font->m_FontMap, cell_width, cell_height, max_ascent);
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResFontGetCacheCellSize(FontResource* font, uint32_t* cell_width, uint32_t* cell_height, uint32_t* max_ascent)
    {
        dmRender::GetFontMapCacheSize(font->m_FontMap, cell_width, cell_height, max_ascent);
        return dmResource::RESULT_OK;
    }

    bool ResFontHasGlyph(FontResource* font, uint32_t codepoint)
    {
        return 0 != font->m_DynamicGlyphs.Get(codepoint) || 0 != font->m_Glyphs.Get(codepoint);
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

        assert(glyph->m_DataImageWidth < 1000);
        assert(glyph->m_DataImageHeight < 1000);

        font->m_DynamicGlyphs.Put(codepoint, glyph);

        uint32_t prev_width, prev_height, prev_ascent;
        dmRender::GetFontMapCacheSize(font->m_FontMap, &prev_width, &prev_height, &prev_ascent);

        if (!font->m_IsDynamic)
        {
            font->m_IsDynamic = 1;

            // Discard any precalculated size
            prev_width = prev_height = prev_ascent = 0;
        }

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
        free((void*)glyph->m_Data);
        delete glyph;
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
}

//DM_DECLARE_RESOURCE_TYPE(ResourceTypeFont, "fontc", dmFont::RegisterResourceType_TTFFont, dmFont::DeregisterResourceType_TTFFont);
