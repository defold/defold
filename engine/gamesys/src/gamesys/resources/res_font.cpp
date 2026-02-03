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

#include "res_font.h"
#include "res_font_private.h"
#include "res_glyph_bank.h"
#include "res_ttf.h"
#include <gamesys/fontgen/fontgen.h>

#include <string.h>

#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <dlib/time.h>
#include <dlib/utf8.h>

#include <font/fontcollection.h>
#include <render/font/fontmap.h>
#include <render/font/font_renderer.h>
#include <render/render_ddf.h>

#include <dmsdk/gamesys/resources/res_material.h>

namespace dmGameSystem
{
    const static dmhash_t EXT_HASH_TTF = dmHashString64("ttf");
    const static dmhash_t EXT_HASH_FONTC = dmHashString64("fontc");

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

    static void DecRefJobResourceInfo(dmResource::HFactory factory, FontResource* resource, FontJobResourceInfo* job_info);

    FontResource::FontResource()
    {
        memset(this, 0, sizeof(*this));
    }

    // Used when recreating a font
    void FontResource::Swap(FontResource* src)
    {
        SwapVar(m_DDF, src->m_DDF);
        SwapVar(m_FontMap, src->m_FontMap);
        SwapVar(m_PathHash, src->m_PathHash);
        SwapVar(m_MaterialResource, src->m_MaterialResource);
        SwapVar(m_GlyphBankResource, src->m_GlyphBankResource);
        SwapVar(m_TTFResource, src->m_TTFResource);
        SwapVar(m_Jobs, src->m_Jobs);
        SwapVar(m_CacheCellPadding, src->m_CacheCellPadding);
        SwapVar(m_IsDynamic, src->m_IsDynamic);
        SwapVar(m_Padding, src->m_Padding);

        this->m_TTFResources.Swap(src->m_TTFResources);
        this->m_FontHashes.Swap(src->m_FontHashes);

        uint8_t dynamic = src->m_IsDynamic;
        src->m_IsDynamic = m_IsDynamic;
        m_IsDynamic = dynamic;
    }

    static void PushPendingJob(FontResource* font, FontJobResourceInfo* job_info)
    {
        if (font->m_PendingJobs.Full())
            font->m_PendingJobs.OffsetCapacity(2);
        font->m_PendingJobs.Push(job_info);

        dmJobThread::PushJob(font->m_Jobs, job_info->m_Job);
    }

    static void RemovePendingJob(FontResource* font, FontJobResourceInfo* job_info)
    {
        for (uint32_t i = 0; i < font->m_PendingJobs.Size(); ++i)
        {
            if (font->m_PendingJobs[i] == job_info)
            {
                font->m_PendingJobs.EraseSwap(i);
                return;
            }
        }
    }

    static void DeallocateJobResourceInfo(FontJobResourceInfo* job_info)
    {
        FontGenDestroyJobData(job_info->m_FontGenJobData);
        delete job_info;
    }

    static void CancelPendingJobs(FontResource* font)
    {
        for (uint32_t i = 0; i < font->m_PendingJobs.Size(); ++i)
        {
            FontJobResourceInfo* job_info = font->m_PendingJobs[i];
            dmJobThread::HJob hjob = job_info->m_Job;

            dmJobThread::JobResult jr = dmJobThread::CancelJob(font->m_Jobs, hjob);
            while (dmJobThread::JOB_RESULT_PENDING == jr)
            {
                dmTime::Sleep(1000);
                jr = dmJobThread::CancelJob(font->m_Jobs, hjob);
            }

            DecRefJobResourceInfo(font->m_Factory, font, job_info);
            DeallocateJobResourceInfo(job_info);
        }
        font->m_PendingJobs.SetSize(0);
    }

    static void ReleaseResourceIter(void* ctx, const uint64_t* hash, TTFResource** presource)
    {
        (void)hash;
        dmResource::HFactory factory = (dmResource::HFactory)ctx;
        dmResource::Release(factory, *presource);
    }

    static void ReleaseResources(dmResource::HFactory factory, FontResource* resource)
    {
        if (resource->m_Jobs)
        {
            CancelPendingJobs(resource);
        }

        if (resource->m_MaterialResource)
            dmResource::Release(factory, (void*) resource->m_MaterialResource);
        resource->m_MaterialResource = 0;
        if (resource->m_GlyphBankResource)
            dmResource::Release(factory, (void*) resource->m_GlyphBankResource);
        resource->m_GlyphBankResource = 0;

        resource->m_TTFResources.Iterate(ReleaseResourceIter, (void*)factory);
        resource->m_TTFResources.Clear();
        resource->m_FontHashes.Clear();
    }

    static FontJobResourceInfo* CreateJobResourceInfo(dmResource::HFactory factory, FontResource* resource, uint32_t glyph_count,
                                                        FPrewarmTextCallback cbk, void* cbk_ctx)
    {
        FontJobResourceInfo* job_info = new FontJobResourceInfo;
        job_info->m_Job = 0;
        job_info->m_Callback = cbk;
        job_info->m_CallbackContext = cbk_ctx;
        job_info->m_Resource = resource;

        HFontCollection fontcollection = ResFontGetFontCollection(resource);
        uint32_t num_fonts = FontCollectionGetFontCount(fontcollection);

        job_info->m_Resources.SetCapacity(num_fonts);
        job_info->m_Resources.SetSize(num_fonts);

        for (uint32_t i = 0; i < num_fonts; ++i)
        {
            HFont hfont = FontCollectionGetFont(fontcollection, i);
            TTFResource* ttfresource = ResFontGetTTFResourceFromFont(resource, hfont);

            dmResource::IncRef(factory, ttfresource);
            job_info->m_Resources[i] = ttfresource;
        }

        // Preallocate the scratch memoty for the job
        job_info->m_FontGenJobData = FontGenCreateJobData(resource, glyph_count);
        return job_info;
    }

    static void DecRefJobResourceInfo(dmResource::HFactory factory, FontResource* resource, FontJobResourceInfo* job_info)
    {
        for (uint32_t i = 0; i < job_info->m_Resources.Size(); ++i)
        {
            dmResource::Release(factory, job_info->m_Resources[i]);
        }
    }

    static void PrewarmGlyphsCallback(void* ctx, int result, const char* errmsg)
    {
        FontResource* font = (FontResource*)ctx;
        font->m_Prewarming = 0;
        font->m_PrewarmDone = 1;
    }

    static uint32_t TextToCodePoints(const char* text, dmArray<uint32_t>& codepoints)
    {
        uint32_t len = dmUtf8::StrLen(text);
        codepoints.SetCapacity(len);
        codepoints.SetSize(0);
        const char* cursor = text;
        while (uint32_t c = dmUtf8::NextChar(&cursor))
        {
            codepoints.Push(c);
        }
        return len;
    }

    static void DestroyJobInfo(FontJobResourceInfo* job_info)
    {
        FontResource* resource = job_info->m_Resource;
        DecRefJobResourceInfo(resource->m_Factory, resource, job_info);
        RemovePendingJob(resource, job_info);
        DeallocateJobResourceInfo(job_info);
    }

    static void TextCallbackJobInfo(void* cbk_ctx, int result, const char* errmsg)
    {
        FontJobResourceInfo* job_info = (FontJobResourceInfo*)cbk_ctx;
        if (job_info->m_Callback)
            job_info->m_Callback(job_info->m_CallbackContext, result, errmsg);
        DestroyJobInfo(job_info);
    }

    dmResource::Result ResFontPrewarmText(FontResource* resource, const char* text, FPrewarmTextCallback cbk, void* cbk_ctx)
    {
        if (!resource->m_IsDynamic)
        {
            return dmResource::RESULT_NOT_SUPPORTED;
        }

        dmArray<uint32_t> codepoints;
        TextToCodePoints(text, codepoints);

        dmRender::HFontMap font_map = resource->m_FontMap;

        TextLayoutSettings settings = {0};

        TextLayout* layout = 0;
        HFontCollection font_collection = dmRender::GetFontCollection(font_map);
        TextResult r = TextLayoutCreate(font_collection, codepoints.Begin(), codepoints.Size(), &settings, &layout);
        if (TEXT_RESULT_OK != r)
        {
            return dmResource::RESULT_UNKNOWN_ERROR;
        }

        uint32_t    glyph_count = TextLayoutGetGlyphCount(layout);
        TextGlyph*  glyphs      = TextLayoutGetGlyphs(layout);

        // Increment all resource before we send them to the thread
        FontJobResourceInfo* job_info = CreateJobResourceInfo(resource->m_Factory, resource, glyph_count, cbk, cbk_ctx);
        job_info->m_Job = dmGameSystem::FontGenAddGlyphs(job_info->m_FontGenJobData, glyphs, glyph_count, TextCallbackJobInfo, job_info);

        TextLayoutFree(layout);

        if (!job_info->m_Job)
        {
            DestroyJobInfo(job_info);
            return dmResource::RESULT_INVALID_DATA;
        }

        PushPendingJob(resource, job_info);
        return dmResource::RESULT_OK;
    }

    static uint32_t GetResourceSize(FontResource* font)
    {
        uint32_t size = sizeof(FontResource);
        size += sizeof(dmRenderDDF::FontMap); // the ddf pointer
        size += font->m_ResourceSize;
        return size + dmRender::GetFontMapResourceSize(font->m_FontMap);
    }

    static void DeleteFontResource(dmResource::HFactory factory, FontResource* font_map)
    {
        ReleaseResources(factory, font_map);

        if (font_map->m_FontMap)
            dmRender::DeleteFontMap(font_map->m_FontMap);

        if (font_map->m_DDF)
        {
            dmDDF::FreeMessage(font_map->m_DDF);
            font_map->m_DDF = 0;
        }

        delete font_map;
    }

    // Api for the font renderer

    static inline bool IsDynamic(dmRenderDDF::FontMap* ddf)
    {
        // If it's empty, we don't have a glyph bank
        return ddf->m_GlyphBank[0] == 0;

    }

    static dmResource::Result AcquireResources(dmResource::HFactory factory, dmRenderDDF::FontMap* ddf,
                                                    FontResource* font_map, const char* filename)
    {
        font_map->m_DDF = ddf;

        dmResource::Result result = dmResource::Get(factory, ddf->m_Material, (void**) &font_map->m_MaterialResource);
        if (result != dmResource::RESULT_OK)
        {
            return result;
        }

        if (IsDynamic(ddf))
        {
            result = dmResource::Get(factory, ddf->m_Font, (void**) &font_map->m_TTFResource);
            if (result != dmResource::RESULT_OK)
            {
                dmLogError("Failed to find font '%s': %d\n", ddf->m_Font, result);
                return result;
            }
        }
        else
        {
            result = dmResource::Get(factory, ddf->m_GlyphBank, (void**) &font_map->m_GlyphBankResource);
            if (result != dmResource::RESULT_OK)
            {
                return result;
            }

        }
        return dmResource::RESULT_OK;
    }

    static float CalcPadding(dmRenderDDF::FontMap* ddf, float* outline_padding, float* shadow_padding)
    {
        float base_padding = dmGameSystem::FontGenGetBasePadding();
        *outline_padding = ddf->m_OutlineWidth;
        *shadow_padding = ddf->m_ShadowBlur;
        return base_padding + *outline_padding + *shadow_padding;
    }

    static float CalcSdfValue(float padding, float width)
    {
        float on_edge_value = dmGameSystem::FontGenGetEdgeValue(); // [0 .. 255] e.g. 191
        const float base_edge = SDF_EDGE_VALUE * 255.0f;

        // Described in the stb_truetype.h as "what value the SDF should increase by when moving one SDF "pixel" away from the edge"
        float pixel_dist_scale = (float)on_edge_value/padding;

        return (base_edge - (pixel_dist_scale * width)) / 255.0f;;
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
        params->m_IsDynamic          = 0;
    }

    static void GetMaxCellSize(HFont hfont, float scale, const char* text, float* cell_width, float* cell_height)
    {
        *cell_width = 0;
        *cell_height = 0;

        FontGlyphOptions options;

        const char* cursor = text;
        uint32_t codepoint = 0;
        while ((codepoint = dmUtf8::NextChar(&cursor)))
        {
            if (dmUtf8::IsWhiteSpace(codepoint))
                continue;

            FontGlyph glyph;
            options.m_Scale = scale;
            FontResult r = FontGetGlyph(hfont, codepoint, &options, &glyph);
            if (r == FONT_RESULT_OK)
            {
                *cell_width = dmMath::Max(*cell_width, glyph.m_Width);
                *cell_height = dmMath::Max(*cell_height, glyph.m_Height);
            }
        }
    }

    static void SetupParamsForDynamicFont(dmRenderDDF::FontMap* ddf, const char* filename, HFont hfont, dmRender::FontMapParams* params)
    {
        if (ddf->m_ShadowBlur > 0.0f && ddf->m_ShadowAlpha > 0.0f) {
            params->m_GlyphChannels = 3;
        }
        else {
            params->m_GlyphChannels = 1;
        }

        float outline_padding;
        float shadow_padding; // the extra padding for the shadow blur
        float padding           = CalcPadding(ddf, &outline_padding, &shadow_padding);

        params->m_SdfSpread     = padding;
        params->m_SdfOutline    = CalcSdfValue(padding, outline_padding);
        params->m_IsDynamic     = 1;

        float sdf_shadow = 1.0f;
        if (shadow_padding)
        {
            sdf_shadow = CalcSdfValue(padding, shadow_padding);
        }
        params->m_SdfShadow = sdf_shadow;

        float scale = FontGetScaleFromSize(hfont, ddf->m_Size);
        params->m_MaxAscent     = FontGetAscent(hfont, scale);
        params->m_MaxDescent    = -FontGetDescent(hfont, scale);

        bool dynamic_cache_size = ddf->m_CacheWidth == 0 || ddf->m_CacheHeight == 0;
        if (dynamic_cache_size)
        {
            // From Fontc.java
            params->m_CacheMaxWidth = 2048;
            params->m_CacheMaxHeight= 4096;
            // no real point in starting too small
            params->m_CacheWidth    = 64;
            params->m_CacheHeight   = 64;
        }
        else
        {
            // If we have a fixed size, let's stick to it
            params->m_CacheMaxWidth = ddf->m_CacheWidth;
            params->m_CacheMaxHeight= ddf->m_CacheHeight;
            params->m_CacheWidth    = ddf->m_CacheWidth;
            params->m_CacheHeight   = ddf->m_CacheHeight;
        }

        params->m_CacheCellWidth     = 0;
        params->m_CacheCellHeight    = 0;
        params->m_CacheCellMaxAscent = 0;

        bool all_chars = ddf->m_AllChars;
        bool has_chars = ddf->m_Characters != 0 && ddf->m_Characters[0] != 0;
        if (!all_chars && has_chars)
        {
            // We can make a guesstimate of the needed cache and cell sizes
            float cell_width, cell_height;
            GetMaxCellSize(hfont, scale, ddf->m_Characters, &cell_width, &cell_height);

            params->m_CacheCellWidth     = (uint32_t)ceilf(cell_width) + 2 * ceilf(padding);
            params->m_CacheCellHeight    = (uint32_t)ceilf(cell_height) + 2 * ceilf(padding);
            params->m_CacheCellMaxAscent = (uint32_t)ceilf(params->m_MaxAscent) + ceilf(padding);

            if (dynamic_cache_size)
            {
                // We want to grow dynamically, so let's make a good guesstimate to fit all prewarming glyphs
                params->m_CacheWidth = 1;
                params->m_CacheHeight = 1;
                uint32_t num_chars = dmUtf8::StrLen(ddf->m_Characters);

                uint32_t prewarm_area = params->m_CacheCellWidth * params->m_CacheCellHeight * num_chars;
                uint32_t max_area = params->m_CacheMaxWidth * params->m_CacheMaxHeight;

                uint32_t size = params->m_CacheWidth * params->m_CacheHeight;
                while (size < prewarm_area && size < max_area)
                {
                    if (params->m_CacheWidth < params->m_CacheHeight)
                        params->m_CacheWidth *= 2;
                    else
                        params->m_CacheHeight *= 2;
                    size = params->m_CacheWidth * params->m_CacheHeight;
                }
            }
        }
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
        params->m_Padding            = glyph_bank->m_Padding;
    }

    HFontCollection ResFontGetFontCollection(FontResource* resource)
    {
        return dmRender::GetFontCollection(resource->m_FontMap);
    }

    TTFResource* ResFontGetTTFResourceFromFont(FontResource* resource, HFont font)
    {
        dmhash_t* path_hash = resource->m_FontHashes.Get(FontGetPathHash(font));
        if (!path_hash)
            return 0;
        TTFResource** ttfresource = resource->m_TTFResources.Get(*path_hash);
        return ttfresource != 0 ? *ttfresource : 0;
    }

    dmhash_t ResFontGetPathHashFromFont(FontResource* resource, HFont font)
    {
        dmhash_t* path_hash = resource->m_FontHashes.Get(FontGetPathHash(font));
        if (!path_hash)
            return 0;
        return *path_hash;
    }

    static TTFResource* ResFontGetTTFResourceFromPathHash(FontResource* resource, dmhash_t path_hash)
    {
        TTFResource** ttfresource = resource->m_TTFResources.Get(path_hash);
        return ttfresource != 0 ? *ttfresource : 0;
    }

    // Called by the fontmap, if the glyph didn't exist when it's time to render
    static FontResult OnGlyphCacheMiss(void* user_ctx, dmRender::HFontMap font_map, HFont font, uint32_t glyph_index, FontGlyph** out)
    {
        FontResource* resource = (FontResource*)user_ctx;

        // Increment all child resources (i.e. .ttf) before we send them to the thread
        FontJobResourceInfo* job_info = CreateJobResourceInfo(resource->m_Factory, resource, 1, 0, 0);
        job_info->m_Job = dmGameSystem::FontGenAddGlyphByIndex(job_info->m_FontGenJobData, font, glyph_index, TextCallbackJobInfo, (void*)job_info);
        if (!job_info->m_Job)
        {
            DestroyJobInfo(job_info);
            return FONT_RESULT_ERROR;
        }

        PushPendingJob(resource, job_info);

        // Instead of keeping track of the async creation process here, we create a null dummy glyph
        // and instead rely on the font generator to overwrite the dummy glyph once it's fully generated.
        // This will prevent from further calls to this cache miss function in the meantime.
        FontGlyph* glyph = new FontGlyph;
        memset(glyph, 0, sizeof(*glyph));
        glyph->m_GlyphIndex = (uint16_t)glyph_index;
        *out = glyph;
        return FONT_RESULT_OK;
    }

    static dmResource::Result CreateFont(dmRender::HRenderContext context, dmRenderDDF::FontMap* ddf, const char* path, FontResource* resource)
    {
        dmRender::FontMapParams params;
        SetupParamsBase(ddf, path, &params);

        HFont hfont;

        resource->m_IsDynamic = IsDynamic(ddf);
        if (resource->m_IsDynamic)
        {
            hfont = dmGameSystem::GetFont(resource->m_TTFResource);
            SetupParamsForDynamicFont(ddf, path, hfont, &params);
        }
        else
        {
            hfont = dmGameSystem::GetFont(resource->m_GlyphBankResource);
            dmRenderDDF::GlyphBank* glyph_bank = GetGlyphBank(resource->m_GlyphBankResource);
            SetupParamsForGlyphBank(ddf, path, glyph_bank, &params);
        }

        HFontCollection font_collection = FontCollectionCreate();
        FontCollectionAddFont(font_collection, hfont);

        params.m_FontCollection = font_collection;
        params.m_Size = ddf->m_Size;

        // If glyphs aren't already present in the .glyph_bankc font, we can't resolve it at runtime either
        if (resource->m_IsDynamic)
        {
            params.m_OnGlyphCacheMiss        = OnGlyphCacheMiss;
            params.m_OnGlyphCacheMissContext = resource;
        }

        dmGraphics::HContext graphics_context = dmRender::GetGraphicsContext(context);
        resource->m_FontMap = dmRender::NewFontMap(context, graphics_context, params);
        if (!resource->m_FontMap)
        {
            dmLogError("Failed creating resource '%s'", path);
            return dmResource::RESULT_INVALID_DATA;
        }

        resource->m_CacheCellPadding  = params.m_CacheCellPadding;
        resource->m_Padding           = ddf->m_Padding;

        dmRender::SetFontMapMaterial(resource->m_FontMap, resource->m_MaterialResource->m_Material);
        dmRender::SetFontMapUserData(resource->m_FontMap, (void*)resource);
        return dmResource::RESULT_OK;
    }

    static dmResource::Result PrewarmFont(dmResource::HFactory factory, const char* path, FontResource* font)
    {
        if (font->m_IsDynamic)
        {
            // Prewarm cache
            bool all_chars = font->m_DDF->m_AllChars;
            bool has_chars = font->m_DDF->m_Characters != 0 && font->m_DDF->m_Characters[0] != 0;
            if (all_chars || !has_chars)
            {
                font->m_PrewarmDone = 1;
                return dmResource::RESULT_OK;
            }

            font->m_Prewarming = 1;
            font->m_PrewarmDone = 0;

            dmResource::Result r = ResFontPrewarmText(font, font->m_DDF->m_Characters, PrewarmGlyphsCallback, font);
            if (dmResource::RESULT_OK != r)
            {
                font->m_Prewarming = 0;
                dmLogError("Failed to prewarm glyph cache for font '%s'", path);
                return dmResource::RESULT_OK;
            }
        }
        else
        {
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
        if (IsDynamic(ddf))
            dmResource::PreloadHint(params->m_HintInfo, ddf->m_Font);

        *params->m_PreloadData = ddf;
        return dmResource::RESULT_OK;
    }

    static dmResource::Result ResFontCreate(const dmResource::ResourceCreateParams* params)
    {
        FontResource* font = new FontResource;
        font->m_Factory = params->m_Factory;

        const char* path = params->m_Filename;
        dmRenderDDF::FontMap* ddf = (dmRenderDDF::FontMap*) params->m_PreloadData;
        dmResource::Result r = AcquireResources(params->m_Factory, ddf, font, path);
        if (r != dmResource::RESULT_OK)
        {
            DeleteFontResource(params->m_Factory, font);
            dmResource::SetResource(params->m_Resource, 0);
            return r;
        }

        if (IsDynamic(ddf))
        {
            if (ddf->m_OutputFormat != dmRenderDDF::TYPE_DISTANCE_FIELD)
            {
                dmLogError("Currently only distance field fonts are supported: %s", path);
                return dmResource::RESULT_NOT_SUPPORTED;
            }

            if (font->m_TTFResources.Full())
            {
                font->m_TTFResources.OffsetCapacity(4);
                font->m_FontHashes.OffsetCapacity(4);
            }

            HFont hfont = dmGameSystem::GetFont(font->m_TTFResource);
            uint32_t font_hash = FontGetPathHash(hfont);
            dmhash_t ttf_hash;
            dmResource::GetPath(params->m_Factory, font->m_TTFResource, &ttf_hash);

            font->m_TTFResources.Put(ttf_hash, font->m_TTFResource);
            font->m_FontHashes.Put(font_hash, ttf_hash);

            font->m_Jobs = dmResource::GetJobThread(params->m_Factory);

            font->m_PendingJobs.SetCapacity(1); // each font will need at least one job
        }

        r = CreateFont((dmRender::HRenderContext) params->m_Context, ddf, path, font);
        if (r != dmResource::RESULT_OK)
        {
            DeleteFontResource(params->m_Factory, font);
            dmResource::SetResource(params->m_Resource, 0);
            return r;
        }

        font->m_PathHash = ResourceDescriptorGetNameHash(params->m_Resource);
        dmResource::SetResource(params->m_Resource, font);
        dmResource::SetResourceSize(params->m_Resource, GetResourceSize(font));
        return r;
    }

    static dmResource::Result ResFontPostCreate(const dmResource::ResourcePostCreateParams* params)
    {
        FontResource* font = (FontResource*)dmResource::GetResource(params->m_Resource);
        if (font->m_PrewarmDone)
        {
            return dmResource::RESULT_OK;
        }

        if (font->m_Prewarming)
        {
            // This is force updating the global job thread, so that we don't end up in a dead lock
            // waiting for the font jobs to complete
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

    dmRender::HFontMap ResFontGetHandle(FontResource* resource)
    {
        return resource->m_FontMap;
    }

    dmResource::Result ResFontGetInfo(FontResource* resource, FontInfo* desc)
    {
        dmRenderDDF::FontMap* ddf = resource->m_DDF;
        desc->m_Size         = ddf->m_Size;
        desc->m_ShadowX      = ddf->m_ShadowX;
        desc->m_ShadowY      = ddf->m_ShadowY;
        desc->m_ShadowBlur   = ddf->m_ShadowBlur;
        desc->m_ShadowAlpha  = ddf->m_ShadowAlpha;
        desc->m_Alpha        = ddf->m_Alpha;
        desc->m_OutlineAlpha = ddf->m_OutlineAlpha;
        desc->m_OutlineWidth = ddf->m_OutlineWidth;
        desc->m_OutputFormat = ddf->m_OutputFormat;
        desc->m_RenderMode   = ddf->m_RenderMode;
        return dmResource::RESULT_OK;
    }

    bool ResFontIsGlyphIndexCached(FontResource* font, HFont hfont, uint32_t glyph_index)
    {
        uint64_t key = dmRender::MakeGlyphIndexKey(hfont, glyph_index);
        return dmRender::IsInCache(font->m_FontMap, key);
    }

    dmResource::Result ResFontAddGlyph(FontResource* font, HFont hfont, FontGlyph* glyph)
    {
        if (hfont == 0)
        {
            hfont = dmGameSystem::GetFont(font->m_TTFResource);
        }
        dmRender::AddGlyphByIndex(font->m_FontMap, hfont, glyph->m_GlyphIndex, glyph);
        ResourceDescriptor* rd = dmResource::FindByHash(font->m_Factory, font->m_PathHash);
        if (rd) // may be 0 when actually loading the font
            dmResource::SetResourceSize(rd, GetResourceSize(font));
        return dmResource::RESULT_OK;
    }

    static dmResource::Result AddFontInternal(dmResource::HFactory factory, FontResource* resource, dmGameSystem::TTFResource* ttfresource, dmhash_t ttf_hash)
    {
        if (resource->m_TTFResource == ttfresource)
        {
            dmLogError("The default font is already added to the font collection: '%s'", dmHashReverseSafe64(ttf_hash));
            dmResource::Release(factory, ttfresource);
            return dmResource::RESULT_INVALID_DATA;
        }

        HFont hfont = dmGameSystem::GetFont(ttfresource);
        HFontCollection font_collection = dmRender::GetFontCollection(resource->m_FontMap);

        uint32_t num_fonts = FontCollectionGetFontCount(font_collection);
        for (uint32_t i = 0; i < num_fonts; ++i)
        {
            HFont font_i = FontCollectionGetFont(font_collection, i);
            if (hfont == font_i)
            {
                dmLogError("The font is already added to the font collection: '%s'", dmHashReverseSafe64(ttf_hash));
                dmResource::Release(factory, ttfresource);
                return dmResource::RESULT_INVALID_DATA;
            }
        }

        FontResult fr = FontCollectionAddFont(font_collection, hfont);
        if (FONT_RESULT_OK != fr)
        {
            dmResource::Release(factory, ttfresource);
            return dmResource::RESULT_INVALID_DATA;
        }

        if (resource->m_TTFResources.Full())
        {
            resource->m_TTFResources.OffsetCapacity(4);
            resource->m_FontHashes.OffsetCapacity(4);
        }
        resource->m_TTFResources.Put(ttf_hash, ttfresource);
        resource->m_FontHashes.Put(FontGetPathHash(hfont), ttf_hash);

        return dmResource::RESULT_OK;
    }

    dmResource::Result ResFontAddFontByPathHash(dmResource::HFactory factory, FontResource* resource, dmhash_t ttf_hash)
    {
        if (!resource->m_TTFResource) // Only dynamic fonts are supported
        {
            dmLogError("Cannot only add font to a dynamic font collection!");
            return dmResource::RESULT_NOT_SUPPORTED;
        }

        dmGameSystem::TTFResource* ttfresource;
        dmResource::Result r = dmResource::GetWithExt(factory, ttf_hash, EXT_HASH_TTF, (void**)&ttfresource);
        if (dmResource::RESULT_OK != r)
        {
            dmLogError("Failed to get ttf '%s': %d", dmHashReverseSafe64(ttf_hash), r);
            return dmResource::RESULT_RESOURCE_NOT_FOUND;
        }

        return AddFontInternal(factory, resource, ttfresource, ttf_hash);
    }

    dmResource::Result ResFontAddFontByPath(dmResource::HFactory factory, FontResource* resource, const char* ttf_path)
    {
        if (!resource->m_TTFResource) // Only dynamic fonts are supported
        {
            dmLogError("Cannot only add font to a dynamic font collection!");
            return dmResource::RESULT_NOT_SUPPORTED;
        }

        dmGameSystem::TTFResource* ttfresource;
        dmResource::Result r = dmResource::GetWithExt(factory, ttf_path, "ttf", (void**)&ttfresource);
        if (dmResource::RESULT_OK != r)
        {
            dmLogError("Failed to get ttf '%s': %d", ttf_path, r);
            return dmResource::RESULT_RESOURCE_NOT_FOUND;
        }

        dmhash_t ttf_hash;
        dmResource::GetPath(factory, ttfresource, &ttf_hash); // We get it like this in case the path != canonical path

        return AddFontInternal(factory, resource, ttfresource, ttf_hash);
    }

    dmResource::Result ResFontRemoveFont(dmResource::HFactory factory, FontResource* font, dmhash_t ttf_hash)
    {
        TTFResource* ttfresource = ResFontGetTTFResourceFromPathHash(font, ttf_hash);
        if (!ttfresource)
        {
            return dmResource::RESULT_RESOURCE_NOT_FOUND;
        }

        if (font->m_TTFResource == ttfresource)
        {
            dmLogError("You cannot remove the default font from the font collection: '%s'", dmHashReverseSafe64(ttf_hash));
            return dmResource::RESULT_INVALID_DATA;
        }

        HFont hfont = dmGameSystem::GetFont(ttfresource);
        uint32_t font_hash = FontGetPathHash(hfont);
        uint64_t* path_hash = font->m_FontHashes.Get(font_hash);
        if (path_hash)
        {
            font->m_TTFResources.Erase(*path_hash);
            font->m_FontHashes.Erase(font_hash);
        }

        HFontCollection font_collection = dmRender::GetFontCollection(font->m_FontMap);
        FontResult fr = FontCollectionRemoveFont(font_collection, hfont);

        dmResource::Release(factory, ttfresource);

        if (FONT_RESULT_OK != fr)
        {
            return dmResource::RESULT_INVALID_DATA;
        }

        return dmResource::RESULT_OK;
    }

    // static void PrintGlyph(uint32_t codepoint, dmRenderDDF::GlyphBank::Glyph* glyph, FontResource* font)
    // {
    //     dmRenderDDF::GlyphBank* glyph_bank = font->m_GlyphBankResource->m_DDF;

    //     printf("    ");
    //     printf("c: '%c' 0x%0X w: %.2f    ", codepoint, codepoint, glyph->m_Width);
    //     printf("adv: %.2f  l: %.2f ", glyph->m_Advance, glyph->m_LeftBearing);
    //     printf("asc/dsc: %d, %d ", glyph->m_Ascent, glyph->m_Descent);

    //     printf("img w/h: %2d, %2d  masc: %2d", glyph_bank->m_CacheCellWidth, glyph_bank->m_CacheCellHeight, glyph_bank->m_CacheCellMaxAscent);
    //     printf("\n");
    // }

    // static void PrintDynamicGlyph(uint32_t codepoint, DynamicGlyph* glyph, FontResource* font)
    // {
    //     printf("    ");
    //     printf("c: '%c' 0x%0X  w: %.2f  imgw: %.u  ", codepoint, codepoint, glyph->m_Glyph.m_Width, glyph->m_Glyph.m_ImageWidth);
    //     printf("adv: %.2f  l: %.2f ", glyph->m_Glyph.m_Advance, glyph->m_Glyph.m_LeftBearing);
    //     printf("asc/dsc: %d, %d ", glyph->m_Glyph.m_Ascent, glyph->m_Glyph.m_Descent);

    //     printf("img w/h: %2d, %2d ", glyph->m_DataImageWidth, glyph->m_DataImageHeight);
    //     printf("\n");
    // }

    // static void PrintGlyphs(FontResource* font, const uint32_t* key, dmRenderDDF::GlyphBank::Glyph** pglyph)
    // {
    //     PrintGlyph(*key, *pglyph, font);
    // }

    // static void PrintDynamicGlyphs(FontResource* font, const uint32_t* key, DynamicGlyph** pglyph)
    // {
    //     PrintDynamicGlyph(*key, *pglyph, font);
    // }

    // void ResFontDebugPrint(FontResource* font)
    // {
    //     printf("FONT:\n");
    //     printf("  cache cell padding: %u\n", font->m_CacheCellPadding);
    //     printf("  glyphs:\n");
    //     font->m_Glyphs.Iterate(PrintGlyphs, font);
    //     printf("  dyn glyphs:\n");
    //     font->m_DynamicGlyphs.Iterate(PrintDynamicGlyphs, font);

    //     printf("\n");
    // }

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
