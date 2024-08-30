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

#include "res_font_map.h"
#include "res_glyph_bank.h"

#include <string.h>

#include <dlib/dstrings.h>
#include <dlib/log.h>

#include <render/font_renderer.h>
#include <render/render_ddf.h>

#include <dmsdk/gamesys/resources/res_material.h>

namespace dmGameSystem
{
    struct FontResource
    {
        dmRenderDDF::FontMap*   m_DDF;
        dmRender::HFontMap      m_FontMap;
        HResourceDescriptor     m_Resource; // For updating the resource size dynamically
        MaterialResource*       m_MaterialResource;
        GlyphBankResource*      m_GlyphBankResource;

        dmHashTable32<dmRenderDDF::GlyphBank::Glyph*> m_Glyphs;

        FontResource()
            : m_FontMap(0)
            , m_Resource(0)
            , m_MaterialResource(0)
            , m_GlyphBankResource(0)
        {
        }
    };

    static void ReleaseResources(dmResource::HFactory factory, FontResource* resource)
    {
        dmResource::Release(factory, (void*) resource->m_MaterialResource);
        dmResource::Release(factory, (void*) resource->m_GlyphBankResource);

        if (resource->m_DDF)
            dmDDF::FreeMessage(resource->m_DDF);
    }

    static uint32_t GetResourceSize(FontResource* font_map)
    {
        uint32_t size = sizeof(FontResource);
        size += sizeof(dmRenderDDF::FontMap); // the ddf pointer
        size += font_map->m_Glyphs.Capacity() * sizeof(dmRenderDDF::GlyphBank::Glyph*);
        return size + dmRender::GetFontMapResourceSize(font_map->m_FontMap);
    }

    static void DeleteFontResource(dmResource::HFactory factory, FontResource* font_map)
    {
        ReleaseResources(factory, font_map);

        if (font_map->m_FontMap)
            dmRender::DeleteFontMap(font_map->m_FontMap);

        delete font_map;
    }

    static dmRender::FontGlyph* GetGlyph(uint32_t utf8, void* user_ctx)
    {
        FontResource* resource = (FontResource*)user_ctx;
        dmRender::FontGlyph** glyphp = resource->m_Glyphs.Get(utf8);
        //printf("Glyph: %u %p\n", utf8, glyphp ? *glyphp : 0);
        return glyphp ? *glyphp : 0;
    }

    static inline void* GetPointer(void* data, uint32_t offset)
    {
        return ((uint8_t*)data) + offset;
    }

    static void* GetGlyphData(uint32_t utf8, void* user_ctx, uint32_t* out_size)
    {
        FontResource* resource = (FontResource*)user_ctx;
        dmRender::FontGlyph** glyphp = resource->m_Glyphs.Get(utf8);
        if (!glyphp)
            return 0;
        dmRender::FontGlyph* glyph = *glyphp;

        dmRenderDDF::GlyphBank* glyph_bank = resource->m_GlyphBankResource->m_DDF;
        void* data = glyph_bank->m_GlyphData.m_Data;
        *out_size = glyph->m_GlyphDataSize;
        void* glyph_data = GetPointer(data, glyph->m_GlyphDataOffset);

        //printf("Glyph data: %u %p  dt: %p\n", utf8, glyph, glyph_data);
        return glyph_data;
    }

    static dmResource::Result AcquireResources(dmResource::HFactory factory, dmRender::HRenderContext context,
        dmRenderDDF::FontMap* ddf, FontResource* font_map, const char* filename)
    {
        MaterialResource* material_res;
        dmResource::Result result = dmResource::Get(factory, ddf->m_Material, (void**) &material_res);
        if (result != dmResource::RESULT_OK)
        {
            dmDDF::FreeMessage(ddf);
            return result;
        }

        GlyphBankResource* glyph_bank_res;
        result = dmResource::Get(factory, ddf->m_GlyphBank, (void**) &glyph_bank_res);
        if (result != dmResource::RESULT_OK)
        {
            dmResource::Release(factory, (void*)material_res);
            dmDDF::FreeMessage(ddf);
            return result;
        }

        dmRenderDDF::GlyphBank* glyph_bank = glyph_bank_res->m_DDF;
        assert(glyph_bank);

        dmRender::FontMapParams params;

        // Font map
        params.m_NameHash           = dmHashString64(filename);
        params.m_ShadowX            = ddf->m_ShadowX;
        params.m_ShadowY            = ddf->m_ShadowY;
        params.m_OutlineAlpha       = ddf->m_OutlineAlpha;
        params.m_ShadowAlpha        = ddf->m_ShadowAlpha;
        params.m_Alpha              = ddf->m_Alpha;
        params.m_LayerMask          = ddf->m_LayerMask;

        // Glyphbank
        params.m_MaxAscent          = glyph_bank->m_MaxAscent;
        params.m_MaxDescent         = glyph_bank->m_MaxDescent;
        params.m_SdfOffset          = glyph_bank->m_SdfOffset;
        params.m_SdfSpread          = glyph_bank->m_SdfSpread;
        params.m_SdfOutline         = glyph_bank->m_SdfOutline;
        params.m_SdfShadow          = glyph_bank->m_SdfShadow;
        params.m_CacheCellWidth     = glyph_bank->m_CacheCellWidth;
        params.m_CacheCellHeight    = glyph_bank->m_CacheCellHeight;
        params.m_CacheCellMaxAscent = glyph_bank->m_CacheCellMaxAscent;
        params.m_CacheCellPadding   = glyph_bank->m_GlyphPadding;
        params.m_CacheWidth         = glyph_bank->m_CacheWidth;
        params.m_CacheHeight        = glyph_bank->m_CacheHeight;
        params.m_ImageFormat        = glyph_bank->m_ImageFormat;
        params.m_GlyphChannels      = glyph_bank->m_GlyphChannels;
        params.m_IsMonospaced       = glyph_bank->m_IsMonospaced;
        params.m_Padding            = glyph_bank->m_Padding;

        // User data is set with SetFontMapUserData
        params.m_GetGlyph = (dmRender::FGetGlyph)GetGlyph;
        params.m_GetGlyphData = (dmRender::FGetGlyphData)GetGlyphData;

        dmGraphics::HContext graphics_context = dmRender::GetGraphicsContext(context);
        if (font_map->m_FontMap == 0)
        {
            font_map->m_FontMap = dmRender::NewFontMap(graphics_context, params);
        }
        else
        {
            dmRender::SetFontMap(font_map->m_FontMap, graphics_context, params);
            ReleaseResources(factory, font_map);
        }

        font_map->m_MaterialResource  = material_res;
        font_map->m_GlyphBankResource = glyph_bank_res;
        font_map->m_DDF               = ddf;

        uint32_t capacity = glyph_bank->m_Glyphs.m_Count;
        font_map->m_Glyphs.SetCapacity(dmMath::Max(1U, (capacity*2)/3), capacity);
        for (uint32_t i = 0; i < glyph_bank->m_Glyphs.m_Count; ++i)
        {
            dmRenderDDF::GlyphBank::Glyph* glyph = &glyph_bank->m_Glyphs[i];
            font_map->m_Glyphs.Put(glyph->m_Character, glyph);
        }

        dmRender::SetFontMapMaterial(font_map->m_FontMap, material_res->m_Material);
        dmRender::SetFontMapUserData(font_map->m_FontMap, (void*)font_map);

        return dmResource::RESULT_OK;
    }


    dmResource::Result ResFontMapPreload(const dmResource::ResourcePreloadParams* params)
    {
        dmRenderDDF::FontMap* ddf;
        dmDDF::Result e = dmDDF::LoadMessage<dmRenderDDF::FontMap>(params->m_Buffer, params->m_BufferSize, &ddf);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }

        dmResource::PreloadHint(params->m_HintInfo, ddf->m_Material);

        *params->m_PreloadData = ddf;
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResFontMapCreate(const dmResource::ResourceCreateParams* params)
    {
        FontResource* font_map = new FontResource;
        font_map->m_Resource = params->m_Resource;

        dmRenderDDF::FontMap* ddf = (dmRenderDDF::FontMap*) params->m_PreloadData;
        dmResource::Result r = AcquireResources(params->m_Factory, (dmRender::HRenderContext) params->m_Context, ddf, font_map, params->m_Filename);
        if (r == dmResource::RESULT_OK)
        {
            dmResource::SetResource(params->m_Resource, font_map);
            dmResource::SetResourceSize(params->m_Resource, GetResourceSize(font_map));
        }
        else
        {
            DeleteFontResource(params->m_Factory, font_map);
            dmResource::SetResource(params->m_Resource, 0);
        }
        return r;
    }

    dmResource::Result ResFontMapDestroy(const dmResource::ResourceDestroyParams* params)
    {
        FontResource* font_map = (FontResource*)dmResource::GetResource(params->m_Resource);
        DeleteFontResource(params->m_Factory, font_map);
        return dmResource::RESULT_OK;
    }

    dmResource::Result ResFontMapRecreate(const dmResource::ResourceRecreateParams* params)
    {
        FontResource* font_map = (FontResource*)dmResource::GetResource(params->m_Resource);

        dmRenderDDF::FontMap* ddf;
        dmDDF::Result e = dmDDF::LoadMessage<dmRenderDDF::FontMap>(params->m_Buffer, params->m_BufferSize, &ddf);
        if ( e != dmDDF::RESULT_OK )
        {
            return dmResource::RESULT_FORMAT_ERROR;
        }

        dmResource::Result r = AcquireResources(params->m_Factory, (dmRender::HRenderContext) params->m_Context, ddf, font_map, params->m_Filename);
        if(r != dmResource::RESULT_OK)
        {
            return r;
        }

        dmResource::SetResourceSize(params->m_Resource, GetResourceSize(font_map));
        return dmResource::RESULT_OK;
    }

    dmRender::HFontMap ResFontMapGetHandle(FontResource* resource)
    {
        return resource->m_FontMap;
    }

    // TODO: Update the resource size!
    //Result GetDescriptor(HFactory factory, const char* name, HResourceDescriptor* descriptor);
}
